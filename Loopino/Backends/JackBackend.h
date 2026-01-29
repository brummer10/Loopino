/*
 * jack.cc
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#pragma once

#include <jack/jack.h>
#include <jack/thread.h>
#include <jack/midiport.h>
#include <cstdio>
#include <cstring>
#include <atomic>

#include "engine.h"

/****************************************************************
        jack.cc   native jackd support for Loopino
        
        this file is meant to be included in main.
****************************************************************/

struct JackBackend
{
    Engine engine;
    Loopino*  ui = nullptr;

    jack_client_t* client   = nullptr;
    jack_port_t*   midi_port = nullptr;
    jack_port_t*   in_port   = nullptr;
    jack_port_t*   out_port  = nullptr;
    jack_port_t*   out1_port = nullptr;

    bool runProcess = false;
    unsigned int split = 0;
    std::atomic<float> latency {0.0f};
    std::atomic<uint32_t> splitPercent { 0 };

    JackBackend(Loopino* ui_) : engine() {   
        ui = ui_;
        ui->setLatencyCallback([this]() {
            return latency.load(std::memory_order_relaxed); });
    }

    bool start();
    void stop();

    static int  process(jack_nframes_t nframes, void* arg);
    static void shutdown(void* arg);
    static int  xrun(void* arg);
    static int  srate(jack_nframes_t samplerate, void* arg);
    static int  buffersize(jack_nframes_t nframes, void* arg);

    void getSplitPercent() noexcept;
    void setSplitPercent(uint32_t percent) noexcept;
    inline jack_nframes_t percentToSplit(uint32_t percent,
                                     jack_nframes_t nframes) noexcept;
    void processMidi(void* midi_input_port_buf);
    void collectMidi(void* midi_in);
};

void JackBackend::shutdown(void* arg) {
    auto* jb = static_cast<JackBackend*>(arg);
    jb->runProcess = false;
    fprintf(stderr, "jack shutdown, exit now\n");
    jb->ui->onExit();
}

int JackBackend::xrun(void* arg) {
    auto* jb = static_cast<JackBackend*>(arg);
    jb->ui->getXrun();
    fprintf(stderr, "Xrun\r");
    return 0;
}

int JackBackend::srate(jack_nframes_t samplerate, void* arg) {
    auto* jb = static_cast<JackBackend*>(arg);
    int prio = jack_client_real_time_priority(jb->client);
    if (prio < 0) prio = 25;

    fprintf(stderr, "Samplerate %iHz\n", samplerate);
    jb->ui->setJackSampleRate(samplerate);
    jb->engine.init(jb->ui, samplerate, prio, 1); //SCHED_FIFO
    return 0;
}

int JackBackend::buffersize(jack_nframes_t nframes, void* arg) {
    fprintf(stderr, "Buffersize is %i samples\n", nframes);
    return 0;
}

inline void JackBackend::setSplitPercent(uint32_t percent) noexcept {
    if (percent > 100) percent = 100;
    splitPercent.store(percent, std::memory_order_relaxed);
}

inline void JackBackend::getSplitPercent() noexcept {
    splitPercent.store(100 - ui->latency, std::memory_order_relaxed);
}

inline jack_nframes_t JackBackend::percentToSplit(uint32_t percent,
                                     jack_nframes_t nframes) noexcept {
    latency.store(nframes - ((nframes * percent) / 100), std::memory_order_relaxed);
    if (percent == 0) return 0;
    if (percent >= 100) return nframes;
    return (nframes * percent) / 100;
}

void JackBackend::processMidi(void* midi_input_port_buf) {
    jack_midi_event_t in_event;
    jack_nframes_t event_count = jack_midi_get_event_count(midi_input_port_buf);
    MidiKeyboard* keys = (MidiKeyboard*)ui->keyboard->private_struct;

    for (unsigned int i = 0; i < event_count; i++) {
        jack_midi_event_get(&in_event, midi_input_port_buf, i);
        if (in_event.time > split) break;
        if ((in_event.buffer[0] & 0xf0) == 0xc0) {  // program change on any midi channel
            //fprintf(stderr,"program changed %i", (int)in_event.buffer[1]);
            ui->loadPresetNum((int)in_event.buffer[1]);
        } else if ((in_event.buffer[0] & 0xf0) == 0xb0) {   // controller
            if (in_event.buffer[1]== 120) { // engine mute by All Sound Off on any midi channel
                //fprintf(stderr,"mute %i", (int)in_event.buffer[2]);
            } else if ((in_event.buffer[1]== 32 ||
                        in_event.buffer[1]== 0)) { // bank change (LSB/MSB) on any midi channel
                //fprintf(stderr,"bank changed %i", (int)in_event.buffer[2]);
            } else if (in_event.buffer[1]== 71) {
                ui->synth.setResoLP((int)in_event.buffer[2]);
            } else if (in_event.buffer[1]== 74) {
                ui->synth.setCutoffLP((int)in_event.buffer[2]);
            } else if (in_event.buffer[1] == 7) {    // CC7 Volume
                constexpr float min_dB = -20.0f;
                constexpr float max_dB =  12.0f;
                ui->volume = min_dB + ((float)in_event.buffer[2] / 127.0f) * (max_dB - min_dB);
            } else {
               // fprintf(stderr,"controller changed %i value %i", (int)in_event.buffer[1], (int)in_event.buffer[2]);
            }
        } else if ((in_event.buffer[0] & 0xf0) == 0xE0) {   // PitchBend
            int lsb = in_event.buffer[1];
            int msb = in_event.buffer[2];
            int value14 = lsb | (msb << 7);  // 0...16383
            float pitchwheel = (value14 - 8192) * 0.00012207; // 1/8192.0f;
            ui->synth.setPitchWheel(pitchwheel);
            wheel_set_value(ui->PitchWheel, pitchwheel);
        } else if ((in_event.buffer[0] & 0xf0) == 0x90) {   // Note On
            int velocity = in_event.buffer[2];
            if (velocity < 1) {
                ui->synth.noteOff((int)(in_event.buffer[1]));
                set_key_in_matrix(keys->in_key_matrix[0], (int)in_event.buffer[1], false);
            } else {
                ui->synth.noteOn((int)(in_event.buffer[1]), (float)((float)velocity/127.0f));
                set_key_in_matrix(keys->in_key_matrix[0], (int)in_event.buffer[1], true);
            }
            //fprintf(stderr,"Note On %i", (int)in_event.buffer[1]);
        }else if ((in_event.buffer[0] & 0xf0) == 0x80) {   // Note Off
            //fprintf(stderr,"Note Off %i", (int)in_event.buffer[1]);
            ui->synth.noteOff((int)(in_event.buffer[1]));
            set_key_in_matrix(keys->in_key_matrix[0], (int)in_event.buffer[1], false);
        }
    }
}

void JackBackend::collectMidi(void* midi_in) {
    uint32_t w = engine.midiWriteIdx.load(std::memory_order_relaxed);
    auto& buf = engine.midiBuf[w];
    buf.clear();

    jack_nframes_t ev_count = jack_midi_get_event_count(midi_in);
    jack_midi_event_t ev;

    for (uint32_t i = 0; i < ev_count; ++i) {
        jack_midi_event_get(&ev, midi_in, i);
        if (ev.time < split) continue;

        if (ev.size >= 3) {
            buf.push(ev.time - split,
                     ev.buffer[0],
                     ev.buffer[1],
                     ev.buffer[2]);
        }
    }
}

int JackBackend::process(jack_nframes_t nframes, void* arg) {
    auto* jb = static_cast<JackBackend*>(arg);
    if (!jb->runProcess) return 0;

    void* midi_in = jack_port_get_buffer(jb->midi_port, nframes);
    float* input  = (float*)jack_port_get_buffer(jb->in_port, nframes);
    float* output = (float*)jack_port_get_buffer(jb->out_port, nframes);
    float* output1= (float*)jack_port_get_buffer(jb->out1_port, nframes);

    jb->getSplitPercent();
    uint32_t percent = jb->splitPercent.load(std::memory_order_relaxed);
    jb->split = jb->percentToSplit(percent, nframes);
    uint32_t pframes = nframes - jb->split;

    static constexpr float THRESHOLD = 0.25f; // -12db
    static uint32_t r = 0;
    static bool rec = false;

    jb->processMidi(midi_in);
    jb->collectMidi(midi_in);

    float peak = 0.0f;
    if (jb->ui->record) {
        for (uint32_t i = 0; i < (uint32_t)nframes; i++) {
            float v = fabsf(input[i]);
            if (v > peak) peak = v;
        }
        
        if (peak > THRESHOLD) rec = true;
    }

    if (jb->ui->record && rec) {
        jb->ui->timer = 0;
        for (uint32_t i = 0; i<(uint32_t)nframes; i++) {
            jb->ui->af.samples[r] = input[i];
            r++;
            jb->ui->position++;
            if (r > jb->ui->af.samplesize) {
                r = 0;
                jb->ui->record = false;
                rec = false;
                break;
            }
        }
    }

    memset(output, 0.0, (uint32_t)nframes * sizeof(float));
    memset(output1, 0.0, (uint32_t)nframes * sizeof(float));
    if (( jb->ui->af.samplesize && jb->ui->af.samples != nullptr) && jb->ui->play && jb->ui->ready) {
        float fSlow0 = 0.0010000000000000009 * jb->ui->gain;
        for (uint32_t i = 0; i<jb->split; i++) {
            if (jb->ui->position > jb->ui->loopPoint_r) {
                for (; i < (uint32_t)nframes; i++) {
                    output[i+pframes] = 0.0f;
                    output1[i+pframes] = 0.0f;
                }
                jb->ui->play = false;
                break;
            }
            jb->ui->fRec0[0] = fSlow0 + 0.999 * jb->ui->fRec0[1];
            output[i+pframes] = jb->ui->af.samples[jb->ui->position*jb->ui->af.channels] * jb->ui->fRec0[0];
            output1[i+pframes] = jb->ui->af.samples[jb->ui->position*jb->ui->af.channels] * jb->ui->fRec0[0];
            jb->ui->fRec0[1] = jb->ui->fRec0[0];
            // track play-head position
            jb->ui->position++;
        }
    } else {
        jb->ui->fRec0[1] = jb->ui->fRec0[0] = 0.0f;
        jb->ui->position = jb->ui->loopPoint_l;
    }

    for (uint32_t i = 0; i<jb->split; i++) {
        float out = jb->ui->synth.process();
        output[i+pframes] += out;
        output1[i+pframes] += out;
    }

    jb->engine.process(pframes, output, output1);
    jb->engine.midiWriteIdx.store(
        jb->engine.midiWriteIdx.load() ^ 1,
        std::memory_order_release
    );
    return 0;
}

bool JackBackend::start() {
    char buffer[1024];
    auto fp = fmemopen(buffer, 1024, "w");
    if ( !fp ) { std::printf("error"); }
    auto old = stderr;
    stderr = fp;

    client = jack_client_open("loopino", JackNoStartServer, nullptr);

    std::fclose(fp);
    stderr = old;
   
    if (!client) {
        std::cout << "jack server not running trying ALSA " << std::endl;
        return false;
    }

    midi_port = jack_port_register(client, "in",
        JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    in_port   = jack_port_register(client, "in_0",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    out_port  = jack_port_register(client, "out_0",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out1_port = jack_port_register(client, "out_1",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    jack_set_process_callback(client, process, this);
    jack_set_xrun_callback(client, xrun, this);
    jack_set_sample_rate_callback(client, srate, this);
    jack_set_buffer_size_callback(client, buffersize, this);
    jack_on_shutdown(client, shutdown, this);

    if (jack_activate (client)) {
        fprintf (stderr, "cannot activate client");
        return false;
    }

    if (!jack_is_realtime(client)) {
        fprintf (stderr, "jack isn't running with realtime priority\n");
    } else {
        fprintf (stderr, "jack running with realtime priority\n");
    }

    runProcess = true;
    return true;
}

void JackBackend::stop() {
    runProcess = false;
    if (client) {
        if (jack_port_connected(midi_port)) {
            jack_port_disconnect(client,midi_port);
        }
        jack_port_unregister(client,midi_port);
        if (jack_port_connected(in_port)) {
            jack_port_disconnect(client,in_port);
        }
        jack_port_unregister(client,in_port);
        if (jack_port_connected(out_port)) {
            jack_port_disconnect(client,out_port);
        }
        jack_port_unregister(client,out_port);
        if (jack_port_connected(out1_port)) {
            jack_port_disconnect(client,out1_port);
        }
        jack_port_unregister(client,out1_port);
        jack_client_close (client);
    }    
}

