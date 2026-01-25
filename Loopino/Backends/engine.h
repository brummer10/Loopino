/*
 * engine.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include "ParallelThread.h"


#ifndef ENGINE_H_
#define ENGINE_H_


struct MidiEvent {
    uint32_t sampleOffset; // 0 .. n_samples-1
    uint8_t  status;
    uint8_t  data1;
    uint8_t  data2;
};

constexpr uint32_t MAX_MIDI_EVENTS = 4096;

struct MidiFrameBuffer {
    MidiEvent events[MAX_MIDI_EVENTS];
    uint32_t  count = 0;

    inline void clear() noexcept {
        count = 0;
    }

    inline void push(uint32_t ofs, uint8_t s, uint8_t d1, uint8_t d2) noexcept {
        if (count < MAX_MIDI_EVENTS) {
            events[count++] = { ofs, s, d1, d2 };
        }
    }
};


class Engine
{
public:
    ParallelThread               xrworker;
    Loopino*                     ui = nullptr;
    float                        latency;
    float                        XrunCounter;
    uint32_t                     bufsize;
    uint32_t                     buffersize;
    uint32_t                     s_rate;
    std::atomic<bool>            _execute;
    std::atomic<bool>            bufferIsInit;

    MidiFrameBuffer midiBuf[2];
    std::atomic<uint32_t> midiWriteIdx { 0 };


    inline Engine();
    inline ~Engine();

    inline void init(Loopino* ui, uint32_t rate, int32_t rt_prio_, int32_t rt_policy_);
    inline void do_work_mono();
    inline void process(uint32_t n_samples, float* output, float* output1);

private:
    ParallelThread               par;

    float*                       bufferoutput0;
    float*                       bufferinput0;
    void handleMidi(MidiEvent ev);
    inline void processBuffer();
    inline void processDsp(uint32_t n_samples, float* output);
};

inline Engine::Engine() :
    xrworker(),
    par(),
    bufferoutput0(NULL),
    bufferinput0(NULL) {
        bufsize = 128;
        buffersize = 0;
        XrunCounter = 0.0;
        latency = 0.0;

        xrworker.start();
        par.start();
};

inline Engine::~Engine(){
    xrworker.stop();
    par.stop();

    delete[] bufferoutput0;
    delete[] bufferinput0;
};

inline void Engine::init(Loopino* ui_, uint32_t rate, int32_t rt_prio_, int32_t rt_policy_) {
    ui = ui_;
    s_rate = rate;

    _execute.store(false, std::memory_order_release);
    bufferIsInit.store(false, std::memory_order_release);

    xrworker.setThreadName("Worker");
    xrworker.set<Engine, &Engine::do_work_mono>(this);
    xrworker.runProcess();

    par.setThreadName("RT-BUF");
    par.setPriority(rt_prio_, rt_policy_);
    par.set<Engine, &Engine::processBuffer>(this);
};

void Engine::do_work_mono() {

    // init buffer for background processing
    if (buffersize < bufsize) {
        buffersize = bufsize * 2;
        delete[] bufferoutput0;
        bufferoutput0 = NULL;
        bufferoutput0 = new float[buffersize];
        memset(bufferoutput0, 0, buffersize*sizeof(float));
        delete[] bufferinput0;
        bufferinput0 = NULL;
        bufferinput0 = new float[buffersize];
        memset(bufferinput0, 0, buffersize*sizeof(float));
        par.setTimeOut(std::max<int>(100,static_cast<int>((bufsize/(s_rate*0.000001))*0.1)));
        bufferIsInit.store(true, std::memory_order_release);
    }
    // set flag that work is done ready
    _execute.store(false, std::memory_order_release);
}

// process dsp in buffered in a background thread
inline void Engine::processBuffer() {
    processDsp(bufsize, bufferoutput0);
}


void Engine::handleMidi(MidiEvent ev) {
    MidiKeyboard* keys = (MidiKeyboard*)ui->keyboard->private_struct;
    if ((ev.status & 0xf0) == 0xc0) { 
        ui->loadPresetNum((int)ev.data1);
    } else if ((ev.status & 0xf0) == 0xb0) {
        if (ev.data1== 120) {
        } else if ((ev.data1== 32 ||
                    ev.data1== 0)) {
        } else if (ev.data1== 71) {
            ui->synth.setResoLP((int)ev.data2);
        } else if (ev.data1== 74) {
            ui->synth.setCutoffLP((int)ev.data2);
        } else if (ev.data1 == 7) {    // CC7 Volume
            constexpr float min_dB = -20.0f;
            constexpr float max_dB =  12.0f;
            ui->volume = min_dB + ((float)ev.data2 / 127.0f) * (max_dB - min_dB);
        } else {
           // fprintf(stderr,"controller changed %i value %i", (int)ev.data1, (int)ev.data2);
        }
    } else if ((ev.status & 0xf0) == 0xE0) {   // PitchBend
        int lsb = ev.data1;
        int msb = ev.data2;
        int value14 = lsb | (msb << 7);  // 0...16383
        float pitchwheel = (value14 - 8192) * 0.00012207; // 1/8192.0f;
        ui->synth.setPitchWheel(pitchwheel);
        wheel_set_value(ui->PitchWheel, pitchwheel);
    } else if ((ev.status & 0xf0) == 0x90) {   // Note On
        int velocity = ev.data2;
        if (velocity < 1) {
            ui->synth.noteOff((int)(ev.data1));
            set_key_in_matrix(keys->in_key_matrix[0], (int)ev.data1, false);
        } else {
            ui->synth.noteOn((int)(ev.data1), (float)((float)velocity/127.0f));
            set_key_in_matrix(keys->in_key_matrix[0], (int)ev.data1, true);
        }
    }else if ((ev.status & 0xf0) == 0x80) {   // Note Off
        ui->synth.noteOff((int)(ev.data1));
        set_key_in_matrix(keys->in_key_matrix[0], (int)ev.data1, false);
    }

}


inline void Engine::processDsp(uint32_t n_samples, float* output) {
    uint32_t readIdx = midiWriteIdx.load(std::memory_order_acquire) ^ 1;
    auto& midi = midiBuf[readIdx];

    uint32_t m = 0;
    if (( ui->af.samplesize && ui->af.samples != nullptr) && ui->play && ui->ready) {
        float fSlow0 = 0.0010000000000000009 * ui->gain;
        for (uint32_t i = 0; i<n_samples; i++) {
            if (ui->position > ui->loopPoint_r) {
                for (; i < n_samples; i++) {
                    output[i] = 0.0f;
                }
                ui->play = false;
                break;
            }
            ui->fRec0[0] = fSlow0 + 0.999 * ui->fRec0[1];
            output[i] = ui->af.samples[ui->position*ui->af.channels] * ui->fRec0[0];
            ui->fRec0[1] = ui->fRec0[0];
            // track play-head position
            ui->position++;
        }
    }
    for (uint32_t i = 0; i < n_samples; ++i) {
        while (m < midi.count && midi.events[m].sampleOffset == i) {
            auto& ev = midi.events[m];
            handleMidi(ev);
            ++m;
        }

       output[i] += ui->synth.process();
   }
}

inline void Engine::process(uint32_t n_samples, float* output, float* output1) {
    if(n_samples<1) return;
    // process in buffered mode
    if (bufferIsInit.load(std::memory_order_acquire)) {
        // avoid buffer overflow on frame size change
        if (buffersize < n_samples) {
            bufsize = n_samples;
            bufferIsInit.store(false, std::memory_order_release);
            _execute.store(true, std::memory_order_release);
            xrworker.runProcess();
            return;
        }
        // get the buffer from previous process
        if (!par.processWait()) {
            XrunCounter += 1;
        }
        // copy incoming data to internal input buffer
        memcpy(bufferinput0, output, n_samples*sizeof(float));

        bufsize = n_samples;
        // copy processed data from last circle to output
        memcpy(output, bufferoutput0, bufsize*sizeof(float));
        memcpy(output1, bufferoutput0, bufsize*sizeof(float));
        // copy internal input buffer to process buffer for next circle
        memcpy(bufferoutput0, bufferinput0, bufsize*sizeof(float));

        // process data in background thread
        if (par.getProcess()) par.runProcess();
        else XrunCounter += 1;
        latency = n_samples;
    } else {
        // process latency free
        processDsp(n_samples, output);
        latency = 0.0;
    }
}

#endif
