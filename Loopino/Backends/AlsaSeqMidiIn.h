
/*
 * AlsaSeqMidiIn.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


/****************************************************************
        AlsaSeqMidiIn.h open a ALSA Sequencer MIDI Input port
        
****************************************************************/


#pragma once
#include <alsa/asoundlib.h>
#include <atomic>
#include <thread>
#include <vector>
#include <poll.h>
#include <unistd.h>
#include <sys/eventfd.h>

class AlsaSeqMidiIn {
public:
    AlsaSeqMidiIn() = default;
    ~AlsaSeqMidiIn() { close(); }

    bool open(decltype(ui)* u, const char* name="Loopino") {
        uiPtr = u;

        if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0)
            return false;

        snd_seq_set_client_name(seq, name);

        port = snd_seq_create_simple_port(seq, "Input",
            SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_APPLICATION|SND_SEQ_PORT_TYPE_MIDI_GENERIC);

        if (port < 0) return false;

        snd_seq_nonblock(seq, 1);

        wakeFd = eventfd(0, EFD_NONBLOCK);
        return wakeFd >= 0;
    }

    void start() {
        if (running.exchange(true)) return;
        thread = std::thread(&AlsaSeqMidiIn::run, this);
    }

    void stop() {
        if (!running.exchange(false)) return;
        uint64_t one = 1;
        write(wakeFd, &one, sizeof(one));   // wake poll
        if (thread.joinable()) thread.join();
    }

    void close() {
        stop();
        if (wakeFd >= 0) ::close(wakeFd);
        if (seq) {
            snd_seq_delete_simple_port(seq, port);
            snd_seq_close(seq);
        }
        seq = nullptr;
        wakeFd = -1;
    }

private:
    snd_seq_t* seq = nullptr;
    int port = -1;
    int wakeFd = -1;
    std::atomic<bool> running{false};
    std::thread thread;
    decltype(ui)* uiPtr = nullptr;

    void run() {
        std::vector<pollfd> pfds(snd_seq_poll_descriptors_count(seq, POLLIN));
        snd_seq_poll_descriptors(seq, pfds.data(), pfds.size(), POLLIN);

        pfds.push_back({wakeFd, POLLIN, 0});

        while (running.load()) {
            if (poll(pfds.data(), pfds.size(), -1) <= 0) continue;

            if (pfds.back().revents & POLLIN) break;

            snd_seq_event_t* ev;
            while (snd_seq_event_input(seq, &ev) > 0)
                dispatch(ev);
        }
    }

    void dispatch(const snd_seq_event_t* ev) {
        auto& s = uiPtr->synth;
        auto* keys = (MidiKeyboard*)uiPtr->keyboard->private_struct;

        switch (ev->type) {
            case SND_SEQ_EVENT_NOTEON:
                if (ev->data.note.velocity) {
                    s.noteOn(ev->data.note.note, ev->data.note.velocity/127.f),
                    set_key_in_matrix(keys->in_key_matrix[0], ev->data.note.note, true);
                } else { s.noteOff(ev->data.note.note); }
                break;

            case SND_SEQ_EVENT_NOTEOFF:
                s.noteOff(ev->data.note.note);
                set_key_in_matrix(keys->in_key_matrix[0], ev->data.note.note, false);
                break;

            case SND_SEQ_EVENT_PITCHBEND:
                s.setPitchWheel(ev->data.control.value / 8192.f);
                break;

            case SND_SEQ_EVENT_PGMCHANGE:
                uiPtr->loadPresetNum(ev->data.control.value);
                break;

            case SND_SEQ_EVENT_CONTROLLER:
                handleCC(ev->data.control.param, ev->data.control.value);
                break;
        }
    }

    void handleCC(uint8_t cc, uint8_t v) {
        if (cc==71) uiPtr->synth.setResoLP(v);
        else if (cc==74) uiPtr->synth.setCutoffLP(v);
        else if (cc==7)  uiPtr->volume = -20.f + (v/127.f)*32.f;
    }
};
