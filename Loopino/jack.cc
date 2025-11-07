/*
 * jack.cc
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#include <jack/jack.h>
#include <jack/thread.h>
#include <jack/midiport.h>
#include <cstdio>
#include <cstring>

/****************************************************************
        jack.cc   native jackd support for Loopino
        
        this file is meant to be included in main.
****************************************************************/

jack_client_t *client;
jack_port_t *midi_port;
jack_port_t *out_port;
jack_port_t *out1_port;
bool runProcess = false;

void jack_shutdown (void *arg) {
    runProcess = false;
    fprintf (stderr, "jack shutdown, exit now \n");
    ui.onExit();
}

int jack_xrun_callback(void *arg) {
    fprintf (stderr, "Xrun \r");
    return 0;
}

int jack_srate_callback(jack_nframes_t samplerate, void* arg) {
    int prio = jack_client_real_time_priority(client);
    if (prio < 0) prio = 25;
    fprintf (stderr, "Samplerate %iHz \n", samplerate);
    ui.setJackSampleRate(samplerate);
    return 0;
}

int jack_buffersize_callback(jack_nframes_t nframes, void* arg) {
    fprintf (stderr, "Buffersize is %i samples \n", nframes);
    return 0;
}

void process_midi(void* midi_input_port_buf) {
    jack_midi_event_t in_event;
    jack_nframes_t event_count = jack_midi_get_event_count(midi_input_port_buf);
    MidiKeyboard* keys = (MidiKeyboard*)ui.keyboard->private_struct;
    unsigned int i;
    for (i = 0; i < event_count; i++) {
        jack_midi_event_get(&in_event, midi_input_port_buf, i);
        if ((in_event.buffer[0] & 0xf0) == 0xc0) {  // program change on any midi channel
            //fprintf(stderr,"program changed %i", (int)in_event.buffer[1]);
            ui.loadPresetNum((int)in_event.buffer[1]);
        } else if ((in_event.buffer[0] & 0xf0) == 0xb0) {   // controller
            if (in_event.buffer[1]== 120) { // engine mute by All Sound Off on any midi channel
                //fprintf(stderr,"mute %i", (int)in_event.buffer[2]);
            } else if ((in_event.buffer[1]== 32 ||
                        in_event.buffer[1]== 0)) { // bank change (LSB/MSB) on any midi channel
                //fprintf(stderr,"bank changed %i", (int)in_event.buffer[2]);
            } else {
               // fprintf(stderr,"controller changed %i value %i", (int)in_event.buffer[1], (int)in_event.buffer[2]);
            }
        } else if ((in_event.buffer[0] & 0xf0) == 0x90) {   // Note On
            int velocity = in_event.buffer[2];
            if (velocity < 1) {
                ui.synth.noteOff((int)(in_event.buffer[1]));
                set_key_in_matrix(keys->in_key_matrix[0], (int)in_event.buffer[1], false);
            } else {
                ui.synth.noteOn((int)(in_event.buffer[1]), (float)((float)velocity/127.0f));
                set_key_in_matrix(keys->in_key_matrix[0], (int)in_event.buffer[1], true);
            }
            //fprintf(stderr,"Note On %i", (int)in_event.buffer[1]);
        }else if ((in_event.buffer[0] & 0xf0) == 0x80) {   // Note Off
            //fprintf(stderr,"Note Off %i", (int)in_event.buffer[1]);
            ui.synth.noteOff((int)(in_event.buffer[1]));
            set_key_in_matrix(keys->in_key_matrix[0], (int)in_event.buffer[1], false);
        }
    }

}

int jack_process(jack_nframes_t nframes, void *arg) {
    if (!runProcess) return 0;
    void *midi_in = jack_port_get_buffer (midi_port, nframes);
    float *output = static_cast<float *>(jack_port_get_buffer (out_port, nframes));
    float *output1 = static_cast<float *>(jack_port_get_buffer (out1_port, nframes));

    static float fRec0[2] = {0};
    static float fRec1[2] = {0};

    process_midi(midi_in);

    if (( ui.af.samplesize && ui.af.samples != nullptr) && ui.play && ui.ready) {
        float fSlow0 = 0.0010000000000000009 * ui.gain;
        for (uint32_t i = 0; i<(uint32_t)nframes; i++) {
            fRec0[0] = fSlow0 + 0.999 * fRec0[1];
            for (uint32_t c = 0; c < ui.af.channels; c++) {
                if (!c) {
                    output[i] = ui.af.samples[ui.position*ui.af.channels] * fRec0[0];
                    if (ui.af.channels ==1) output1[i] = ui.af.samples[ui.position*ui.af.channels] * fRec0[0];
                } else output1[i] = ui.af.samples[ui.position*ui.af.channels+c] * fRec0[0];
            }
            fRec0[1] = fRec0[0];
            // track play-head position
            ui.position++;
            if (ui.position > ui.loopPoint_r) {
                ui.position = ui.loopPoint_l;
                ui.play = false;
            } else if (ui.position <= ui.loopPoint_l) {
                ui.position = ui.loopPoint_r;
            }
        }
    } else {
        memset(output, 0.0, (uint32_t)nframes * sizeof(float));
        memset(output1, 0.0, (uint32_t)nframes * sizeof(float));
    }
    float fSlow0 = 0.0010000000000000009 * ui.gain;
    for (uint32_t i = 0; i<(uint32_t)nframes; i++) {
        fRec1[0] = fSlow0 + 0.999 * fRec1[1];
        output[i] += ui.synth.process() * fRec1[0];
        output1[i] += ui.synth.process() * fRec1[0];
        fRec1[1] = fRec1[0];
    }

    return 0;
}

void startJack() {

    if ((client = jack_client_open ("loopino", JackNoStartServer, NULL)) == 0) {
        fprintf (stderr, "jack server not running?\n");
        ui.onExit();
    }

    if (client) {
        midi_port = jack_port_register(
                       client, "in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
        out_port = jack_port_register(
                       client, "out_0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        out1_port = jack_port_register(
                       client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        jack_set_xrun_callback(client, jack_xrun_callback, 0);
        jack_set_sample_rate_callback(client, jack_srate_callback, 0);
        jack_set_buffer_size_callback(client, jack_buffersize_callback, 0);
        jack_set_process_callback(client, jack_process, 0);
        jack_on_shutdown (client, jack_shutdown, 0);

        if (jack_activate (client)) {
            fprintf (stderr, "cannot activate client");
            ui.onExit();
        }

        if (!jack_is_realtime(client)) {
            fprintf (stderr, "jack isn't running with realtime priority\n");
        } else {
            fprintf (stderr, "jack running with realtime priority\n");
        }
        runProcess = true;
    }
}

void quitJack() {
    runProcess = false;
    if (client) {
        if (jack_port_connected(midi_port)) {
            jack_port_disconnect(client,midi_port);
        }
        jack_port_unregister(client,midi_port);
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
