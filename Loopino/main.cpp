/*
 * main.c
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#include <cmath>
#include <vector>
#include <signal.h>
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <string>
#include <condition_variable>

#include "ParallelThread.h"
#include "Loopino_ui.h"

Loopino ui;

#include "CmdParser.h"
#include "JackBackend.h"
#include "AlsaAudioOut.h"
#include "AlsaMidiIn.h"
//#include "AlsaSeqMidiIn.h"

AlsaRawMidiIn rawmidi;
std::vector<AlsaMidiDevice> devices;
std::string mididevice; // = "hw:2,0,0";

// catch signals and exit clean
void
signal_handler (int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGHUP:
        case SIGTERM:
        case SIGQUIT:
            std::cerr << "\nsignal "<< sig <<" received, exiting ...\n"  <<std::endl;
            XLockDisplay(ui.w_top->app->dpy);
            ui.onExit();
            XFlush(ui.w_top->app->dpy);
            XUnlockDisplay(ui.w_top->app->dpy);
        break;
        default:
        break;
    }
}

static void device_select(void *w_, void* user_data) {
    if(user_data !=NULL) {
        int response = *(int*)user_data;
        if (rawmidi.open(devices[response-1].id.data(), &ui)) rawmidi.start();
    }
}

void showMidiDeviceSelect() {
    if (!devices.size()) {
        Widget_t *dia = open_message_dialog(ui.w_top, INFO_BOX,
        "Select MIDI Device:",  "NO MIDI Devices found, MIDI support skipped", " ");
        XSetTransientForHint(ui.w_top->app->dpy, dia->widget, ui.w_top->widget);
        return;
    }
    if (devices.size() == 1) {
        if (rawmidi.open(devices[0].id.data(), &ui)) rawmidi.start();
        return;
    }

    std::string d;
    d +=  devices[0].label;
    for (size_t i = 1; i < devices.size(); ++i) {
        d += " | " + devices[i].label;
    }

    Widget_t *dia = open_message_dialog(ui.w_top, SELECTION_BOX,
        "Select MIDI Device:",  "Devices:", d.data());
    XSetTransientForHint(ui.w_top->app->dpy, dia->widget, ui.w_top->widget);
    ui.w_top->func.dialog_callback = device_select;
}

int main(int argc, char *argv[]){
    //auto t1 = std::chrono::high_resolution_clock::now();

    if(0 == XInitThreads()) 
        std::cerr << "Warning: XInitThreads() failed\n" << std::endl;

    CmdParser cmd;

    if (!cmd.parseCmdLine(argc, argv)) {
        cmd.printUsage(argv[0]);
        return 1;
    }
    // cmd options
    mididevice = cmd.opts.midiDevice.value_or("");
    float scaling = cmd.opts.scaling.value_or(1.0f);
    int bufferSize = cmd.opts.bufferSize.value_or(256);
    int sampleRate = cmd.opts.sampleRate.value_or(48000);

    Xputty app;
    JackBackend jb(&ui);
    AlsaAudioOut out;
    //AlsaSeqMidiIn aseq;
    std::condition_variable Sync;

    main_init(&app);
    if (scaling != 1.0f) app.hdpi = scaling;

    ui.createGUI(&app);
    //auto t2 = std::chrono::high_resolution_clock::now();
    //auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
    //std::cout << duration/1e+6 << std::endl;

    signal (SIGQUIT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGHUP, signal_handler);
    signal (SIGINT, signal_handler);

    if (!jb.start()) {
        if (out.init(&ui, sampleRate, bufferSize)) out.start();
        if (!mididevice.empty()) {
            if (rawmidi.open(mididevice.data(), &ui)) {
                rawmidi.start();
            } else {
                Widget_t *dia = open_message_dialog(ui.w_top, INFO_BOX,
                    "MIDI Device:",  "MIDI Devices not found, MIDI support skipped", " ");
                    XSetTransientForHint(ui.w_top->app->dpy, dia->widget, ui.w_top->widget);
            }
        } else {
            devices.reserve(4);
            devices = rawmidi.listAlsaRawMidiInputs();
            showMidiDeviceSelect();
        }
    }/* else {
        if (aseq.open(&ui)) aseq.start();
    }*/

    main_run(&app);

    ui.pa.stop();

    jb.stop();
    rawmidi.stop();
    //aseq.stop();
    out.stop();

    main_quit(&app);

    printf("bye bye\n");
    return 0;
}

