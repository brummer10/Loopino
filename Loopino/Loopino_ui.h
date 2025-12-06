
/*
 * SoundEdit.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <set>
#include <vector>
#include <string>
#include <sndfile.hh>
#include <fstream>
#include <limits>
#include <cstdint>

#include "ParallelThread.h"
#include "SupportedFormats.h"
#include "AudioFile.h"
#include "PitchTracker.h"
#include "LoopGenerator.h"
#include "SamplePlayer.h"
#include "Parameter.h"

#include "TextEntry.h"
#include "xwidgets.h"
#include "xfile-dialog.h"
#include "xmessage-dialog.h"


#ifndef LOOPINO_H
#define LOOPINO_H

struct StreamOut {
    virtual void write(const void* data, size_t size) = 0;
    virtual ~StreamOut() = default;
};

struct StreamIn {
    virtual void read(void* data, size_t size) = 0;
    virtual ~StreamIn() = default;
};

/****************************************************************
    class Loopino - create the GUI for loopino
****************************************************************/

class Loopino : public TextEntry
{
public:
    Xputty app;
    Widget_t *w_top;
    Widget_t *w;
    Widget_t *lw;
    Widget_t *keyboard;
    ParallelThread pa;
    ParallelThread fetch;
    AudioFile af;
    PitchTracker pt;
    LoopGenerator lg;
    PolySynth synth;
    Params param;
    
    std::vector<float> loopBuffer;
    std::vector<float> loopBufferSave;
    std::vector<float> sampleBuffer;
    std::vector<float> sampleBufferSave;

    SampleBank sbank;
    std::shared_ptr<SampleInfo> sampleData { nullptr };;
    SampleBank lbank;
    std::shared_ptr<SampleInfo> loopData { nullptr };;

    uint32_t jack_sr;
    uint32_t position;
    uint32_t loopPoint_l;
    uint32_t loopPoint_r;
    uint32_t loopPoint_l_auto;
    uint32_t loopPoint_r_auto;
    uint32_t frameSize;

    uint8_t rootkey;
    uint8_t loopRootkey;
    uint8_t saveRootkey;

    int16_t pitchCorrection;
    int16_t loopPitchCorrection;
    int16_t matches;
    int16_t currentLoop;
    int16_t loopPeriods;
    int16_t timer;

    float freq;
    float loopFreq;
    float gain;

    std::string filename;
    std::string lname;

    bool loadNew;
    bool loadLoopNew;
    bool play;
    bool play_loop;
    bool ready;
    bool havePresetToLoad;
    bool haveDefault;
    bool record;

    Loopino() : af() {
        sampleData = std::make_shared<SampleInfo>();
        loopData = std::make_shared<SampleInfo>();
        jack_sr = 0;
        position = 0;
        loopPoint_l = 0;
        loopPoint_r = 1000;
        loopPoint_l_auto = 0;
        loopPoint_r_auto = 0;
        frameSize = 0;
        gain = std::pow(1e+01, 0.05 * 0.0);
        is_loaded = false;
        loadNew = false;
        loadLoopNew = false;
        play = false;
        play_loop = false;
        ready = true;
        havePresetToLoad = false;
        haveDefault = true;
        record = false;
        matches = 0;
        currentLoop = 0;
        loopPeriods = 1;
        freq = 0.0;
        pitchCorrection = 0;
        rootkey = 60;
        saveRootkey = 69;
        loopFreq = 0.0;
        loopPitchCorrection = 0;
        loopRootkey = 69;
        loadPresetMIDI = -1;
        p = 0;
        firstLoop = true;
        attack = 0.01f;
        decay = 0.1f;
        release = 0.8f;
        sustain = 0.3f;
        frequency = 440.0f;
        resonance = 0.0;
        cutoff = 127.0;
        volume = 0.0f;
        sharp = 0.0f;
        saw = 0.0f;
        fadeout = 0.0f;
        pmfreq = 0.1f;
        pmdepth = 0.0f;
        vibdepth = 0.0f;
        vibrate = 5.0f;
        tremdepth = 0.0f;
        tremrate = 5.0f;
        pmmode = 0;
        useLoop = 0;
        timer = 30;
        generateKeys();
        guiIsCreated = false;
        analyseBuffer = new float[40960];
        #if defined (RUN_AS_PLUGIN)
        registerParameters();
        #endif
    };

    ~Loopino() {
        pa.stop();
        delete[] analyseBuffer;
    };

/****************************************************************
                      public function calls
****************************************************************/

    // stop background threads and quit main window
    void onExit() {
        pa.stop();
        //savePreset(presetFile);
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        quit(w_top);
        #else
        main_quit(w->app);
        #endif
    }
    
    // receive Sample Rate from audio back-end
    void setJackSampleRate(uint32_t sr) {
        jack_sr = sr;        
        synth.init((double)jack_sr, 48);
        if (!havePresetToLoad) generateSine();
        //loadPreset(presetFile);
    }

    // receive a file name from the File Browser or the command-line
    static void dialog_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if(user_data !=NULL) {
            self->filename = *(char**)user_data;
            self->loadFile();
        } else {
            std::cerr << "no file selected" <<std::endl;
        }
    }

    // load a audio file in background process
    void loadFile() {
        read_soundfile(filename.c_str());
    }

    void loadPresetNum(int v) {
        if (v < 0 || v > (int)presetFiles.size()) return;
        loadPresetMIDI = v;
    }

    void loadPresetToSynth() {
        //std::cout << "loadPresetToSynth" << std::endl;
        af.channels = 1;
        loopPoint_l = 0;
        loopPoint_r = af.samplesize;
        setOneShootToBank();
        if (createLoop()) {
            setLoopToBank();
        } 
        #if defined (RUN_AS_PLUGIN)
        setValuesFromHost();
        #endif
    }

/****************************************************************
                 Clap wrapper
****************************************************************/

    void markDirty(int num) {
        #if defined (RUN_AS_PLUGIN)
        param.setParamDirty(num , true);
        param.controllerChanged.store(true, std::memory_order_release);
        #endif
    }

#if defined (RUN_AS_PLUGIN)
#include "Clap/LoopinoClapWrapper.cc"
#endif

/****************************************************************
                      main window
****************************************************************/

    // create the main GUI
    void createGUI(Xputty *app) {
        #ifndef RUN_AS_PLUGIN
        set_custom_theme(app);
        w_top = create_window(app, os_get_root_window(app, IS_WINDOW), 0, 0, 880, 390);
        widget_set_title(w_top, "loopino");
        widget_set_icon_from_png(w_top,LDVAR(loopino_png));
        #endif
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        widget_set_dnd_aware(w_top);
        #endif
        os_set_input_mask(w_top);
        w_top->func.dnd_notify_callback = dnd_load_response;
        w_top->func.resize_notify_callback = resize_callback;
        commonWidgetSettings(w_top);
        os_set_window_min_size(w_top, 798, 290, 880, 390);

        w = create_widget(app, w_top, 0, 0, 440, 310);
        w->parent = w_top;
        w->scale.gravity = NORTCENTER;
        w->func.expose_callback = draw_window;
        commonWidgetSettings(w);

        loopMark_L = add_hslider(w, "",15, 2, 18, 18);
        loopMark_L->scale.gravity = NONE;
        loopMark_L->parent_struct = (void*)this;
        loopMark_L->adj_x = add_adjustment(loopMark_L,0.0, 0.0, 0.0, 1000.0,1.0, CL_METER);
        loopMark_L->adj = loopMark_L->adj_x;
        add_tooltip(loopMark_L, "Set left clip point ");
        loopMark_L->func.expose_callback = draw_slider;
        loopMark_L->func.button_release_callback = slider_l_released;
        loopMark_L->func.motion_callback = move_loopMark_L;
        loopMark_L->func.value_changed_callback = slider_l_changed_callback;

        loopMark_R = add_hslider(w, "",415, 2, 18, 18);
        loopMark_R->scale.gravity = NONE;
        loopMark_R->parent_struct = (void*)this;
        loopMark_R->adj_x = add_adjustment(loopMark_R,0.0, 0.0, -1000.0, 0.0,1.0, CL_METER);
        loopMark_R->adj = loopMark_R->adj_x;
        add_tooltip(loopMark_R, "Set right clip point ");
        loopMark_R->func.expose_callback = draw_slider;
        loopMark_R->func.button_release_callback = slider_r_released;
        loopMark_R->func.motion_callback = move_loopMark_R;
        loopMark_R->func.value_changed_callback = slider_r_changed_callback;

        wview = add_waveview(w, "", 20, 20, 400, 120);
        wview->scale.gravity = NORTHWEST;
        wview->adj_x = add_adjustment(wview,0.0, 0.0, 0.0, 1000.0,1.0, CL_METER);
        wview->adj = wview->adj_x;
        wview->func.expose_callback = draw_wview;
        wview->func.button_release_callback = set_playhead;
        commonWidgetSettings(wview);

        lw = create_widget(app, w_top, 440, 0, 440, 310);
        lw->parent = w_top;
        lw->scale.gravity = NORTCENTER;
        lw->func.expose_callback = draw_window;
        commonWidgetSettings(lw);

        loopview = add_waveview(lw, "", 20, 20, 400, 120);
        loopview->scale.gravity = NORTHWEST;
        loopview->adj_x = add_adjustment(loopview,0.0, 0.0, 0.0, 1000.0,1.0, CL_METER);
        loopview->adj = loopview->adj_x;
        loopview->func.expose_callback = draw_lwview;
        loopview->func.button_release_callback = set_playhead;
        commonWidgetSettings(loopview);

        Widget_t* frame = add_frame(w, "Sample Buffer", 10, 145, 425, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        filebutton = add_file_button(frame, 20, 20, 35, 35, getenv("HOME") ? getenv("HOME") : PATH_SEPARATOR, "audio");
        filebutton->scale.gravity = SOUTHEAST;
        widget_get_png(filebutton, LDVAR(load__png));
        filebutton->flags |= HAS_TOOLTIP;
        add_tooltip(filebutton, "Load audio file");
        filebutton->func.user_callback = dialog_response;
        commonWidgetSettings(filebutton);

        Presets = add_button(frame, "", 60, 20, 35, 35);
        Presets->scale.gravity = SOUTHWEST;
        widget_get_png(Presets, LDVAR(presets_png));
        Presets->flags |= HAS_TOOLTIP;
        add_tooltip(Presets, "Load/Save Presets");
        Presets->func.value_changed_callback = presets_callback;
        commonWidgetSettings(Presets);

        FadeOut = add_knob(frame, "FadeOut",230,18,38,38);
        FadeOut->scale.gravity = SOUTHWEST;
        FadeOut->flags |= HAS_TOOLTIP;
        add_tooltip(FadeOut, "Fade Out Samplebuffer");
        set_adjustment(FadeOut->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(FadeOut, (Color_state)1, (Color_mod)2, 0.15, 0.52, 0.55, 1.0);
        FadeOut->func.expose_callback = draw_knob;
        FadeOut->func.value_changed_callback = fade_callback;
        commonWidgetSettings(FadeOut);

        clip = add_button(frame, "", 290, 20, 35, 35);
        clip->scale.gravity = SOUTHWEST;
        widget_get_png(clip, LDVAR(clip__png));
        clip->flags |= HAS_TOOLTIP;
        add_tooltip(clip, "Clip Sample to clip marks");
        clip->func.value_changed_callback = button_clip_callback;
        commonWidgetSettings(clip);

        playbutton = add_image_toggle_button(frame, "", 330, 20, 35, 35);
        playbutton->scale.gravity = SOUTHWEST;
        widget_get_png(playbutton, LDVAR(play_png));
        playbutton->flags |= HAS_TOOLTIP;
        add_tooltip(playbutton, "Play Sample");
        playbutton->func.value_changed_callback = button_playbutton_callback;
        commonWidgetSettings(playbutton);

        #ifndef RUN_AS_PLUGIN
        Record = add_image_toggle_button(frame, "", 370, 20, 35, 35);
        Record->scale.gravity = SOUTHWEST;
        widget_get_png(Record, LDVAR(record_png));
        Record->flags |= HAS_TOOLTIP;
        add_tooltip(Record, "Record Sample");
        Record->func.value_changed_callback = button_record_callback;
        commonWidgetSettings(Record);
        #endif

        frame = add_frame(lw, "Loop Buffer", 2, 145, 180, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        setLoop = add_image_toggle_button(frame, "", 15, 20, 35, 35);
        setLoop->scale.gravity = SOUTHWEST;
        widget_get_png(setLoop, LDVAR(loop_png));
        setLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setLoop, "Use Loop Sample");
        setLoop->func.value_changed_callback = button_set_callback;
        commonWidgetSettings(setLoop);

        setLoopSize = add_knob(frame, "S",53,18,38,38);
        setLoopSize->scale.gravity = SOUTHWEST;
        setLoopSize->flags |= HAS_TOOLTIP;
        add_tooltip(setLoopSize, "Loop Periods");
        set_adjustment(setLoopSize->adj, 1.0, 1.0, 1.0, 12.0, 1.0, CL_CONTINUOS);
        setLoopSize->func.expose_callback = draw_knob;
        setLoopSize->func.value_changed_callback = setLoopSize_callback;
        commonWidgetSettings(setLoopSize);

        setPrevLoop = add_button(frame, "<", 95, 20, 35, 35);
        setPrevLoop->scale.gravity = SOUTHWEST;
        setPrevLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setPrevLoop, "Load previous loop");
        setPrevLoop->func.value_changed_callback = setPrevLoop_callback;
        commonWidgetSettings(setPrevLoop);

        setNextLoop = add_button(frame, ">", 130, 20, 35, 35);
        setNextLoop->scale.gravity = SOUTHWEST;
        setNextLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setNextLoop, "Load next loop");
        setNextLoop->func.value_changed_callback = setNextLoop_callback;
        commonWidgetSettings(setNextLoop);

        frame = add_frame(lw, "Sharp", 187, 145, 105, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        Sharp = add_knob(frame, "Square",15,20,38,38);
        Sharp->scale.gravity = SOUTHWEST;
        Sharp->flags |= HAS_TOOLTIP;
        add_tooltip(Sharp, "Square");
        set_adjustment(Sharp->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Sharp, (Color_state)1, (Color_mod)2, 0.55, 0.42, 0.15, 1.0);
        Sharp->func.expose_callback = draw_knob;
        Sharp->func.value_changed_callback = sharp_callback;
        commonWidgetSettings(Sharp);

        Saw = add_knob(frame, "Saw",55,20,38,38);
        Saw->scale.gravity = SOUTHWEST;
        Saw->flags |= HAS_TOOLTIP;
        add_tooltip(Saw, "Saw Tooth");
        set_adjustment(Saw->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Saw, (Color_state)1, (Color_mod)2, 0.55, 0.52, 0.15, 1.0);
        Saw->func.expose_callback = draw_knob;
        Saw->func.value_changed_callback = saw_callback;
        commonWidgetSettings(Saw);

        frame = add_frame(lw, "Gain", 297, 145, 65, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        Volume = add_knob(frame, "dB",14,20,38,38);
        Volume->scale.gravity = SOUTHWEST;
        Volume->flags |= HAS_TOOLTIP;
        add_tooltip(Volume, "Volume (dB)");
        set_adjustment(Volume->adj, 0.0, 0.0, -20.0, 12.0, 0.1, CL_CONTINUOS);
        set_widget_color(Volume, (Color_state)1, (Color_mod)2, 0.38, 0.62, 0.94, 1.0);
        Volume->func.expose_callback = draw_knob;
        Volume->func.value_changed_callback = volume_callback;
        commonWidgetSettings(Volume);

        #ifndef RUN_AS_PLUGIN
        frame = add_frame(lw, "Exit", 367, 145, 62, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        w_quit = add_button(frame, "", 15, 20, 35, 35);
        widget_get_png(w_quit, LDVAR(exit__png));
        w_quit->scale.gravity = SOUTHWEST;
        w_quit->flags |= HAS_TOOLTIP;
        add_tooltip(w_quit, "Exit");
        w_quit->func.value_changed_callback = button_quit_callback;
        commonWidgetSettings(w_quit);
        #endif

        frame = add_frame(w, "ADSR", 10, 230, 190, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        Attack = add_knob(frame, "Attack",15,20,38,38);
        Attack->scale.gravity = SOUTHWEST;
        Attack->flags |= HAS_TOOLTIP;
        add_tooltip(Attack, "Attack");
        set_adjustment(Attack->adj, 0.01, 0.01, 0.001, 5.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Attack, (Color_state)1, (Color_mod)2, 0.894, 0.106, 0.623, 1.0);
        Attack->func.expose_callback = draw_knob;
        Attack->func.value_changed_callback = attack_callback;
        commonWidgetSettings(Attack);

        Decay = add_knob(frame, "Decay",55,20,38,38);
        Decay->scale.gravity = SOUTHWEST;
        Decay->flags |= HAS_TOOLTIP;
        add_tooltip(Decay, "Decay");
        set_adjustment(Decay->adj, 0.1, 0.1, 0.005, 5.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Decay, (Color_state)1, (Color_mod)2, 0.902, 0.098, 0.117, 1.0);
        Decay->func.expose_callback = draw_knob;
        Decay->func.value_changed_callback = decay_callback;
        commonWidgetSettings(Decay);

        Sustain = add_knob(frame, "Sustain",95,20,38,38);
        Sustain->scale.gravity = SOUTHWEST;
        Sustain->flags |= HAS_TOOLTIP;
        add_tooltip(Sustain, "Sustain");
        set_adjustment(Sustain->adj, 0.8, 0.8, 0.001, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Sustain, (Color_state)1, (Color_mod)2, 0.377, 0.898, 0.109, 1.0);
        Sustain->func.expose_callback = draw_knob;
        Sustain->func.value_changed_callback = sustain_callback;
        commonWidgetSettings(Sustain);

        Release = add_knob(frame, "Release",135,20,38,38);
        Release->scale.gravity = SOUTHWEST;
        Release->flags |= HAS_TOOLTIP;
        add_tooltip(Release, "Release");
        set_adjustment(Release->adj, 0.3, 0.3, 0.005, 10.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Release, (Color_state)1, (Color_mod)2, 0.486, 0.106, 0.894, 1.0);
        Release->func.expose_callback = draw_knob;
        Release->func.value_changed_callback = release_callback;
        commonWidgetSettings(Release);

        frame = add_frame(w, "Filter", 205, 230, 110, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        Resonance = add_knob(frame, "Resonance",15,20,38,38);
        Resonance->scale.gravity = SOUTHWEST;
        Resonance->flags |= HAS_TOOLTIP;
        add_tooltip(Resonance, "Resonance");
        set_adjustment(Resonance->adj, 0.0, 0.0, 0.0, 127.0, 1.0, CL_CONTINUOS);
        set_widget_color(Resonance, (Color_state)1, (Color_mod)2, 0.95, 0.42, 0.15, 1.0);
        Resonance->func.expose_callback = draw_knob;
        Resonance->func.value_changed_callback = resonance_callback;
        commonWidgetSettings(Resonance);

        CutOff = add_knob(frame, "CutOff",55,20,38,38);
        CutOff->scale.gravity = SOUTHWEST;
        CutOff->flags |= HAS_TOOLTIP;
        add_tooltip(CutOff, "CutOff");
        set_adjustment(CutOff->adj, 127.0, 127.0, 0.0, 127.0, 1.0, CL_CONTINUOS);
        set_widget_color(CutOff, (Color_state)1, (Color_mod)2, 0.20, 0.60, 0.95, 1.0);
        CutOff->func.expose_callback = draw_knob;
        CutOff->func.value_changed_callback = cutoff_callback;
        commonWidgetSettings(CutOff);

        frame = add_frame(w, "Synth Freq", 320, 230, 115, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        Frequency = add_valuedisplay(frame, _(" Hz"), 22, 25, 70, 30);
        set_adjustment(Frequency->adj, 440.0, 440.0, 220.0, 880.0, 0.1, CL_CONTINUOS);
        Frequency->scale.gravity = SOUTHWEST;
        Frequency->flags |= HAS_TOOLTIP;
        add_tooltip(Frequency, "Synth Root Frequency");
        Frequency->func.value_changed_callback = frequency_callback;
        commonWidgetSettings(Frequency);

        frame = add_frame(lw, "Phase Modulator", 2, 230, 180, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        PmMode[0] = add_check_box(frame,"Sine" , 12, 12, 15, 15);
        PmMode[0]->flags |= IS_RADIO;
        set_widget_color(PmMode[0], (Color_state)0, (Color_mod)3, 0.55, 0.65, 0.55, 1.0);
        commonWidgetSettings(PmMode[0]);
        PmMode[0]->func.value_changed_callback = radio_box_button_pressed;

        PmMode[1] = add_check_box(frame,"Triangle" , 12, 27, 15, 15);
        PmMode[1]->flags |= IS_RADIO;
        set_widget_color(PmMode[1], (Color_state)0, (Color_mod)3, 0.55, 0.65, 0.55, 1.0);
        commonWidgetSettings(PmMode[1]);
        PmMode[1]->func.value_changed_callback = radio_box_button_pressed;

        PmMode[2] = add_check_box(frame,"Noise" , 12, 42, 15, 15);
        PmMode[2]->flags |= IS_RADIO;
        commonWidgetSettings(PmMode[2]);
        set_widget_color(PmMode[2], (Color_state)0, (Color_mod)3, 0.55, 0.65, 0.55, 1.0);
        PmMode[2]->func.value_changed_callback = radio_box_button_pressed;
        radio_box_set_active(PmMode[pmmode]);

        PmMode[3] = add_check_box(frame,"Juno" , 12, 57, 15, 15);
        PmMode[3]->flags |= IS_RADIO;
        commonWidgetSettings(PmMode[3]);
        set_widget_color(PmMode[3], (Color_state)0, (Color_mod)3, 0.55, 0.65, 0.55, 1.0);
        PmMode[3]->func.value_changed_callback = radio_box_button_pressed;
        radio_box_set_active(PmMode[pmmode]);

        PmDepth = add_knob(frame, "Depth",88,20,38,38);
        PmDepth->scale.gravity = SOUTHWEST;
        PmDepth->flags |= HAS_TOOLTIP;
        add_tooltip(PmDepth, "PM Depth");
        set_adjustment(PmDepth->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(PmDepth, (Color_state)1, (Color_mod)2, 0.55, 0.95, 0.80, 1.0);
        PmDepth->func.expose_callback = draw_knob;
        PmDepth->func.value_changed_callback = pmdepth_callback;
        commonWidgetSettings(PmDepth);

        PmFreq = add_knob(frame, "Freq",128,20,38,38);
        PmFreq->scale.gravity = SOUTHWEST;
        PmFreq->flags |= HAS_TOOLTIP;
        add_tooltip(PmFreq, "PM Freq");
        set_adjustment(PmFreq->adj, 0.01, 0.01, 0.01, 30.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(PmFreq, (Color_state)1, (Color_mod)2, 0.60, 0.80, 1.00, 1.0);
        PmFreq->func.expose_callback = draw_knob;
        PmFreq->func.value_changed_callback = pmfreq_callback;
        commonWidgetSettings(PmFreq);

        frame = add_frame(lw, "Vibrato", 187, 230, 105, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        VibDepth = add_knob(frame, "VibDepth",15,20,38,38);
        VibDepth->scale.gravity = SOUTHWEST;
        VibDepth->flags |= HAS_TOOLTIP;
        add_tooltip(VibDepth, "Vibrato Depth");
        set_adjustment(VibDepth->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(VibDepth, (Color_state)1, (Color_mod)2, 0.00, 0.78, 1.00, 1.0);
        VibDepth->func.expose_callback = draw_knob;
        VibDepth->func.value_changed_callback = vibdepth_callback;
        commonWidgetSettings(VibDepth);

        VibRate = add_knob(frame, "VibRate",55,20,38,38);
        VibRate->scale.gravity = SOUTHWEST;
        VibRate->flags |= HAS_TOOLTIP;
        add_tooltip(VibRate, "Vibrato Rate");
        set_adjustment(VibRate->adj, 5.0, 5.0, 0.1, 12.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(VibRate, (Color_state)1, (Color_mod)2, 0.00, 1.00, 0.78, 1.0);
        VibRate->func.expose_callback = draw_knob;
        VibRate->func.value_changed_callback = vibrate_callback;
        commonWidgetSettings(VibRate);

        frame = add_frame(lw, "Tremolo", 297, 230, 105, 75);
        frame->scale.gravity = SOUTHWEST;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        TremDepth = add_knob(frame, "TremDepth",15,20,38,38);
        TremDepth->scale.gravity = SOUTHWEST;
        TremDepth->flags |= HAS_TOOLTIP;
        add_tooltip(TremDepth, "Tremolo Depth");
        set_adjustment(TremDepth->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(TremDepth, (Color_state)1, (Color_mod)2, 1.00, 0.67, 0.47, 1.0);
        TremDepth->func.expose_callback = draw_knob;
        TremDepth->func.value_changed_callback = tremdepth_callback;
        commonWidgetSettings(TremDepth);

        TremRate = add_knob(frame, "TremRate",55,20,38,38);
        TremRate->scale.gravity = SOUTHWEST;
        TremRate->flags |= HAS_TOOLTIP;
        add_tooltip(TremRate, "Tremolo Rate");
        set_adjustment(TremRate->adj, 5.0, 5.0, 0.1, 15.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(TremRate, (Color_state)1, (Color_mod)2, 1.00, 0.78, 0.59, 1.0);
        TremRate->func.expose_callback = draw_knob;
        TremRate->func.value_changed_callback = tremrate_callback;
        commonWidgetSettings(TremRate);

        keyboard = add_midi_keyboard(w_top, "Organ", 0, 310, 880, 80);
        keyboard->flags |= HIDE_ON_DELETE;
        //keyboard->scale.gravity = SOUTHCENTER;
        keyboard->parent_struct = (void*)this;
        MidiKeyboard* keys = (MidiKeyboard*)keyboard->private_struct;
        Widget_t *view_port = keys->context_menu->childlist->childs[0];
        Widget_t *octavemap = view_port->childlist->childs[1];
        keys->octave = 12*3;
        keys->velocity = 100;
        adj_set_value(keys->vel->adj, keys->velocity);
        set_active_radio_entry_num(octavemap, keys->octave/12);
        keys->mk_send_note = get_note;
        keys->mk_send_all_sound_off = all_notes_off;

        #ifndef RUN_AS_PLUGIN
        widget_show_all(w_top);
        #endif
        pa.startTimeout(60);
        pa.set<Loopino, &Loopino::updateUI>(this);
        getConfigFilePath();
        createPrestList();
        guiIsCreated = true;
    }

private:
    Widget_t *w_quit;
    Widget_t *filebutton;
    Widget_t *wview;
    Widget_t *loopview;
    Widget_t *loopMark_L;
    Widget_t *loopMark_R;
    Widget_t *playbutton;
    Widget_t *Volume;
    Widget_t *saveLoop;
    Widget_t *clip;
    Widget_t *setLoop;
    Widget_t *setLoopSize;
    Widget_t *setNextLoop;
    Widget_t *setPrevLoop;
    Widget_t *Presets;
    Widget_t *Record;

    Widget_t *Attack;
    Widget_t *Decay;
    Widget_t *Sustain;
    Widget_t *Release;
    Widget_t *Frequency;
    Widget_t *Resonance;
    Widget_t *CutOff;
    Widget_t *Sharp;
    Widget_t *Saw;
    Widget_t *FadeOut;
    Widget_t *PmFreq;
    Widget_t *PmDepth;
    Widget_t *PmMode[4];
    Widget_t *VibDepth;
    Widget_t *VibRate;
    Widget_t *TremDepth;
    Widget_t *TremRate;

    Window    p;

    SupportedFormats supportedFormats;

    bool is_loaded;
    bool firstLoop;
    bool guiIsCreated;
    std::string newLabel;
    std::vector<std::string> keys;
    std::vector<std::string> presetFiles;

    std::string configFile;
    std::string presetFile;
    std::string presetDir;
    std::string presetName;
    int loadPresetMIDI;
    
    float attack;
    float decay;
    float sustain;
    float release;
    float frequency;
    float resonance;
    float cutoff;
    float volume;
    float sharp;
    float saw;
    float fadeout;
    float pmfreq;
    float pmdepth;
    float vibdepth;
    float vibrate;
    float tremdepth;
    float tremrate;
    int pmmode;
    int useLoop;
    
    float *analyseBuffer;

/****************************************************************
                    Create loop samples
****************************************************************/

    void normalize(std::vector<float>& Buffer, const float range){
        // get max abs amplitude for normalisation
        float maxAbs = 0.0f;
        for (size_t i = 0; i < Buffer.size(); ++i) {
            float a = std::fabs(Buffer[i]);
            if (a > maxAbs) maxAbs = a;
        }
        // normalise buffer
        float gain = range / maxAbs;
        for (size_t i = 0; i < Buffer.size(); ++i) {
            Buffer[i] *=gain;
        }
    }

    bool getNextLoop(int num) {
        if (num < 0 || num >= matches) return false;
        LoopGenerator::LoopInfo loopinfo;
        loopBuffer.clear();
        if (lg.getNextMatch(af.samples, af.samplesize , af.channels, 
            freq, loopBuffer, loopinfo, num)) {
                loopPoint_l_auto = loopinfo.start;
                loopPoint_r_auto = loopinfo.end;
                normalize(loopBuffer, 0.6f);
                loopBufferSave.clear();
                loopBufferSave.resize(loopBuffer.size());
                for (size_t i = 0; i < loopBuffer.size(); ++i) {
                    loopBufferSave[i] = loopBuffer[i];
                }
                process_sharp();
                currentLoop = num;
                return true;
            }
        return false;
    }

    void getPitch() {
        freq = 0.0;
        pitchCorrection = 0;
        rootkey = 0;
        if (af.samples) rootkey = pt.getPitch(af.samples, af.samplesize , af.channels, (float)jack_sr, &pitchCorrection, &freq);
        if (haveDefault) {
            freq = 440.0f;
            rootkey = 69;
        }
    }

    bool createLoop() {
        getPitch();
        if (freq > 0.0) {
            LoopGenerator::LoopInfo loopinfo;
            loopBuffer.clear();
            if (lg.generateLoop(af.samples, loopPoint_l, loopPoint_r, af.samplesize ,
                                af.channels, jack_sr, freq, loopBuffer, loopinfo, loopPeriods)) {
                loopPoint_l_auto = loopinfo.start;
                loopPoint_r_auto = loopinfo.end;
                matches = loopinfo.matches;
                currentLoop = matches - 1;
                normalize(loopBuffer, 0.6f);
                loopBufferSave.clear();
                loopBufferSave.resize(loopBuffer.size());
                for (size_t i = 0; i < loopBuffer.size(); ++i) {
                    loopBufferSave[i] = loopBuffer[i];
                }
                process_sharp();
            } else {
                loopPoint_l_auto = 0;
                loopPoint_r_auto = 0;
                if (guiIsCreated) {
                    Widget_t *dia = open_message_dialog(w, ERROR_BOX, "loopino",
                                                        _("Fail to create loop"),NULL);
                    os_set_transient_for_hint(w, dia);
                }
                return false;
            }
            return true;
        } else {
            if (jack_sr && af.samples && guiIsCreated) {
                Widget_t *dia = open_message_dialog(w, ERROR_BOX, "loopino",
                                                    _("Fail to get root Frequency"),NULL);
                os_set_transient_for_hint(w, dia);
                return false;
            }
        }
        return false;
    }

/****************************************************************
        offline processor (sharp (square) and saw tooth)
****************************************************************/

    void process_fadeout(std::vector<float>& buffer) {
        if (buffer.empty() || fadeout <= 0.0f) return;

        const size_t N = buffer.size();
        const float maxFraction = 5.0f / 6.0f;
        size_t fadeSamples = size_t(maxFraction * fadeout * N);
        if (fadeSamples < 1) return;
        size_t start = N - fadeSamples;

        for (size_t i = start; i < N; ++i)
        {
            float t = float(i - start) / float(fadeSamples);
            float gain = std::exp(-3.0f * t);
            buffer[i] *= gain;
        }
    }


    void process_saw(std::vector<float>& buffer) {
        if (buffer.empty()) return;
        if (saw <= 0.0001f) return;

        const size_t N = buffer.size();
        float* out = buffer.data();
        const float snapAmount = saw;
        const float snapTime = 0.003f * snapAmount;
        size_t start = 0;

        while (start < N - 1) {
            while (start < N - 1 && out[start] == 0.0f)
                start++;
            if (start >= N - 1) break;
            float sgn = (out[start] >= 0.0f ? 1.0f : -1.0f);
            size_t end = start + 1;

            while (end < N && (out[end] * sgn) >= 0.0f)
                end++;

            size_t len = end - start;
            if (len < 3) { start = end; continue; }
            float mn = out[start];
            float mx = out[start];

            for (size_t i = start; i < end; ++i) {
                mn = min(mn, out[i]);
                mx = max(mx, out[i]);
            }

            for (size_t i = 0; i < len; ++i)
            {
                float t = float(i) / float(len - 1);
                float linear;
                if (sgn > 0.0f) linear = mn + t * (mx - mn);
                else linear = mx + t * (mn - mx);

                out[start + i] = (1.0f - saw) * out[start + i] + saw * linear;
            }

            size_t snapSamples = size_t(snapTime * float(len));
            if (snapSamples < 1) snapSamples = 1;
            if (snapSamples > len / 3) snapSamples = len / 3;
            if (snapSamples == 1) {
                size_t idx = end - 1;
                float snapTarget = (sgn > 0.0f ? mn : mx);
                out[idx] = (out[idx] * 0.0f) + snapTarget * 1.0f;
            } else {
                float alpha = 0.25f + saw * 0.35f;
                float beta  = 1.20f + saw * 0.50f;

                for (size_t i = 0; i < snapSamples; ++i) {
                    float t = float(i) / float(snapSamples - 1);
                    float snapEnv = std::pow(t, alpha) * std::pow(1.0f - t, beta);
                    size_t idx = end - 1 - i;
                    float snapTarget = (sgn > 0.0f ? mn : mx);

                    out[idx] = out[idx] * (1.0f - snapEnv) + snapTarget * snapEnv;
                }
            }
            start = end;
        }
    }

    void process_sharp(){
        if (!loopBuffer.size()) return;

        for (size_t i = 0; i < loopBuffer.size(); ++i) {
            loopBuffer[i] = loopBufferSave[i];
        }

        float drive = 1.0f + sharp * 25.0f;
        float compDB = sharp * 6.0f;
        float compensation = std::pow(10.0f, compDB / 20.0f);

        for (size_t i = 0; i < loopBuffer.size(); ++i) {
            float x = loopBuffer.data()[i];
            float shaped = std::tanh(x * drive);

            loopBuffer.data()[i] = (x + sharp * (shaped - x)) * compensation;
        }
        process_saw(loopBuffer);
    }

    void process_sample_sharp() {
        if (!sampleBuffer.size()) return;

        for (size_t i = 0; i < sampleBuffer.size(); ++i)
            sampleBuffer[i] = sampleBufferSave[i];

        float drive = 1.0f + sharp * 25.0f;
        float compDB = sharp * 6.0f;
        float compensation = std::pow(10.0f, compDB / 20.0f);

        for (size_t i = 0; i < sampleBuffer.size(); ++i) {
            float x = sampleBuffer[i];
            float shaped = std::tanh(x * drive);

            sampleBuffer[i] =  (x + sharp * (shaped - x)) * compensation;
        }

        process_saw(sampleBuffer);
        process_fadeout(sampleBuffer);
        normalize(sampleBuffer, 0.6f);

        if (guiIsCreated) {
            loadNew = true;
            update_waveview(wview, sampleBuffer.data(), sampleBuffer.size());
        }
    }

/****************************************************************
                    Load samples into synth
****************************************************************/

    void setOneShootBank() {
        if (!sampleBuffer.size()) return;
        if (!sampleData) sampleData = std::make_shared<SampleInfo>();
        getPitch();
        //sbank.clear();
        sampleData->data = sampleBuffer;
        sampleData->sourceRate = (double)jack_sr;
        sampleData->rootFreq = (double) freq;
        sbank.addSample(std::const_pointer_cast<const SampleInfo>(sampleData));
        synth.setBank(&sbank);
    }

    void setOneShootToBank() {
        if (!af.samples) return;
        sampleBuffer.clear();
        sampleBuffer.resize(af.samplesize);
        float maxAbs = 0.0f;
        for (size_t i = 0; i < af.samplesize; i++) {
            float a = std::fabs(af.samples[i * af.channels]);
            if (a > maxAbs) maxAbs = a;
        }
        float gain = 1.0f/maxAbs;
        for (size_t i = 0; i < af.samplesize; i++) {
            sampleBuffer[i] = af.samples[i * af.channels] * gain;
        }
        sampleBufferSave.clear();
        sampleBufferSave.resize(sampleBuffer.size());
        for (size_t i = 0; i < sampleBuffer.size(); ++i) {
            sampleBufferSave[i] = sampleBuffer[i];
        }
        process_sample_sharp();
        setOneShootBank();
    }

    void setLoopBank() {
        if (!loopBuffer.size()) return;
        if (!loopData) loopData = std::make_shared<SampleInfo>();
        loopData->data = loopBufferSave;
        loopData->sourceRate = (double)jack_sr;
        loopData->rootFreq = (double)freq;
        lbank.addSample(std::const_pointer_cast<const SampleInfo>(loopData));
        synth.setLoopBank(&lbank);
        memset(analyseBuffer, 0, 40960 * sizeof(float));
        synth.getAnalyseBuffer(analyseBuffer, 40960);
        loopRootkey = pt.getPitch(analyseBuffer,
                        40960, 1, (float)jack_sr, &loopPitchCorrection, &loopFreq);
        if (haveDefault) {
            loopFreq = 440.0f;
            loopRootkey = 69;
        }
        double cor = loopFreq / 440.0f;
        //lbank.clear();
        loopData->data = loopBuffer;
        loopData->sourceRate = (double)jack_sr;
        loopData->rootFreq = (double)(freq * cor);
       // int set = max(1,jack_sr/loopBuffer.size());
       // for (int i =0; i<set;i++)
        lbank.addSample(std::const_pointer_cast<const SampleInfo>(loopData));
        synth.setLoopBank(&lbank);
        if (guiIsCreated) {
            uint32_t length = loopPoint_r_auto - loopPoint_l_auto;
            std::string tittle = "loopino: loop size " + std::to_string(length)
                                + " Samples | Key Note " + keys[loopRootkey]
                                + " | loop " + std::to_string(currentLoop)
                                + " from " + std::to_string(matches - 1);
            widget_set_title(w_top, tittle.data());
        }
    }

    void setBank() {
        setOneShootBank();
        setLoopBank();
        synth.setLoop(true);
    }

    void setLoopToBank() {
        if (!loopBuffer.size()) return;
        normalize(loopBuffer, 0.6f);
        loadLoopNew = true;
        play_loop = true;
        if (guiIsCreated) {
            update_waveview(loopview, loopBuffer.data(), loopBuffer.size());
        }
        setLoopBank();
    }

/****************************************************************
                    Sound File clipping
****************************************************************/

    // clip the audio buffer to match the loop marks
    void clipToLoopMarks() {
        if (!af.samples) return;
        play = false;
        ready = false;
        haveDefault = false;
        uint32_t new_size = (loopPoint_r-loopPoint_l) * af.channels;
        delete[] af.saveBuffer;
        af.saveBuffer = nullptr;
        af.saveBuffer = new float[new_size];
        std::memset(af.saveBuffer, 0, new_size * sizeof(float));
        for (uint32_t i = 0; i<new_size; i++) {
            af.saveBuffer[i] = af.samples[i+loopPoint_l];
        }
        matches = 0;
        delete[] af.samples;
        af.samples = nullptr;
        af.samples =  new float[new_size];
        std::memset(af.samples, 0, new_size * sizeof(float));
        memcpy(af.samples, af.saveBuffer, new_size * sizeof(float));

        af.samplesize = new_size / af.channels;
        position = 0;
        adj_set_max_value(wview->adj, (float)af.samplesize);
        adj_set_state(loopMark_L->adj, 0.0);
        loopPoint_l = 0;
        adj_set_state(loopMark_R->adj,1.0);
        loopPoint_r = af.samplesize;

        delete[] af.saveBuffer;
        af.saveBuffer = nullptr;
        //loadNew = true;
        //update_waveview(wview, af.samples, af.samplesize);
        if (adj_get_value(playbutton->adj))
             play = true;
        ready = true;
        setOneShootToBank();
        button_setLoop_callback(setLoop, NULL);
    }

/****************************************************************
                    Sound File loading
****************************************************************/

    // when Sound File loading fail, clear wave view and reset tittle
    void failToLoad() {
        if (guiIsCreated) {
            loadNew = true;
            update_waveview(wview, af.samples, af.samplesize);
            widget_set_title(w_top, "loopino");
        }
    }

    // load a Sound File when pre-load is the wrong file
    void load_soundfile(const char* file) {
        af.channels = 0;
        af.samplesize = 0;
        af.samplerate = 0;
        position = 0;

        ready = false;
        play_loop = false;
        matches = 0;
        adj_set_value(setLoop->adj, 0.0);
        is_loaded = af.getAudioFile(file, jack_sr);
        if (!is_loaded) failToLoad();
    }

    // load Sound File data into memory
    void read_soundfile(const char* file, bool haveLoopPoints = false) {
        load_soundfile(file);
        is_loaded = false;
        loadNew = true;
        haveDefault = false;
        if (af.samples) {
            std::filesystem::path p = file;
            if (guiIsCreated) {
                adj_set_max_value(wview->adj, (float)af.samplesize);
                adj_set_state(loopMark_L->adj, 0.0);
                adj_set_state(loopMark_R->adj,1.0);
            }
            loopPoint_l = 0;
            loopPoint_r = af.samplesize;
            //loadLoopNew = true;
            //update_waveview(wview, af.samples, af.samplesize);
            setOneShootToBank();
            button_setLoop_callback(setLoop, NULL);
        } else {
            af.samplesize = 0;
            std::cerr << "Error: could not resample file" << std::endl;
            failToLoad();
        }
        ready = true;
    }

    void generateSine() {
        haveDefault = true;
        int new_size = static_cast<int>(4.0 * jack_sr);
        delete[] af.samples;
        af.samples = nullptr;
        af.samples =  new float[new_size];
        af.samplesize = new_size;
        af.channels = 1;
        std::memset(af.samples, 0, new_size * sizeof(float));
        const float duration = new_size / jack_sr / 2;
        for (int i = 0; i < new_size; ++i) {
            float t = (float)i / jack_sr;
            float s = sinf(2.0f * M_PI * 440.0f * t);
            //float phase = fmodf(t * 440.f, 1.0f);  // 0..1
            //float s = 2.0f * phase - 1.0f;    
            float fade = 1.0f;
            float fadeStart = duration - 2.0f;
            if (t > fadeStart) {
                float x = (t - fadeStart) / 2.0f;
                fade = expf(-3.0f * x);
            }
            af.samples[i] = s * fade;
        }
        loopPoint_l = 0;
        if (guiIsCreated) {
            adj_set_max_value(wview->adj, (float)af.samplesize);
            adj_set_state(loopMark_L->adj, 0.0);
            adj_set_state(loopMark_R->adj,1.0);
        }
        loopPoint_l = 0;
        loopPoint_r = af.samplesize;
        setOneShootToBank();
        if (guiIsCreated) button_setLoop_callback(setLoop, NULL);
        else {
            createLoop();
            setLoopToBank();
        }
    }

    void record_sample() {
        int new_size = static_cast<int>(4.0 * jack_sr);
        delete[] af.samples;
        af.samples = nullptr;
        af.samples =  new float[new_size];
        af.samplesize = new_size;
        af.channels = 1;
        std::memset(af.samples, 0, new_size * sizeof(float));
        timer = 30;
        loopPoint_l = 0;
        loopPoint_r = af.samplesize;
        position = 0;
        play = false;
        haveDefault = false;
        if (guiIsCreated) {
            loadNew = true;
            update_waveview(wview, af.samples, af.samplesize);
        }
    }

    void set_record() {
        haveDefault = false;
        timer = 30;
        position = 0;
        if (guiIsCreated) {
            adj_set_max_value(wview->adj, (float)af.samplesize);
            adj_set_state(loopMark_L->adj, 0.0);
            adj_set_state(loopMark_R->adj,1.0);
        }
        setOneShootToBank();
        if (guiIsCreated) button_setLoop_callback(setLoop, NULL);
        else {
            createLoop();
            setLoopToBank();
        }
        
    }

/****************************************************************
            drag and drop handling for the main window
****************************************************************/

    std::string url_decode(const std::string& encoded) {
        std::ostringstream decoded;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%') {
                if (i + 2 < encoded.length()) {
                    std::istringstream iss(encoded.substr(i + 1, 2));
                    int hex;
                    if (iss >> std::hex >> hex)
                        decoded << static_cast<char>(hex);
                    i += 2;
                }
            } else {
                decoded << encoded[i];
            }
        }
        return decoded.str();
    }

    static void dnd_load_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (user_data != NULL) {
            char* dndfile = NULL;
            dndfile = strtok(*(char**)user_data, "\r\n");
            while (dndfile != NULL) {
                if (self->supportedFormats.isSupported(dndfile) ) {
                    self->filename = self->url_decode(dndfile);
                    self->loadFile();
                    break;
                } else {
                    std::cerr << "Unrecognized file extension: " << self->filename << std::endl;
                }
                dndfile = strtok(NULL, "\r\n");
            }
        }
    }

/****************************************************************
            generate Note Key table 
****************************************************************/

    void generateKeys() {
        std::vector<std::string> note_sharp = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        std::vector<std::string> octave = {"-1", "0","1","2","3","4","5","6", "7", "8", "9"};

        size_t o = 0;
        int j = 0;
        int k = 0;
        for (int i = 0; i < 128; i++) {
            std::string key = *(note_sharp.begin() + o) + *(octave.begin() + j);
            keys.push_back(key);
            if (i > k+10) {
                k = i+1;
                j++;
            }
            o++;
            if (o>=note_sharp.size()) o=0;
        }
    }

/****************************************************************
            Play head (called from timeout thread) 
****************************************************************/

    static void dummy_callback(void *w_, void* user_data) {}

    // frequently (60ms) update the wave view widget for playhead position
    // and the MIDI keyboard to visualising the MIDI input 
    // triggered from the timeout background thread 
    void updateUI() {
        static int waitOne = 0;
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        XLockDisplay(w->app->dpy);
        #endif
        if (loadPresetMIDI > -1) {
            std::string name = presetFiles[loadPresetMIDI];
            std::string path = getPathFor(name);
            loadPreset(path);
            loadPresetMIDI = -1;
        }

        wview->func.adj_callback = dummy_callback;
        playbutton->func.adj_callback = dummy_callback;
        if (ready) adj_set_value(wview->adj, (float) position);
        else {
            waitOne++;
            if (waitOne > 2) {
                transparent_draw(wview, nullptr);
                transparent_draw(loopview, nullptr);
                waitOne = 0;
            }
        }
        if (!play) {
            adj_set_value(playbutton->adj, 0.0);
            expose_widget(playbutton);
        }
        if (!record && timer == 0) {
            set_record();
            adj_set_value(Record->adj, 0.0);
            expose_widget(Record);
        }
        expose_widget(keyboard);
        expose_widget(wview);
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        XFlush(w->app->dpy);
        XUnlockDisplay(w->app->dpy);
        #endif
        wview->func.adj_callback = transparent_draw;
        playbutton->func.adj_callback = transparent_draw;
    }

/****************************************************************
                      Button callbacks 
****************************************************************/

    void commonWidgetSettings(Widget_t *wi) {
        wi->parent_struct = (void*)this;
        wi->flags |= NO_AUTOREPEAT;
        wi->func.key_press_callback = forward_key_press;
        wi->func.key_release_callback = forward_key_release;
    }

    // forward all keyboard press to the MIDI keyboard
    static void forward_key_press(void *w_, void *key, void *user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->keyboard->func.key_press_callback(self->keyboard, key, user_data);
    }

    // forward all keyboard release to the MIDI keyboard
    static void forward_key_release(void *w_, void *key, void *user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->keyboard->func.key_release_callback(self->keyboard, key, user_data);
    }

    // send MIDI keyboard input to the synth
    static void get_note(Widget_t *w, const int *key, int on_off) {
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        MidiKeyboard* keys = (MidiKeyboard*)self->keyboard->private_struct;
        if (on_off == 0x90) {
            self->synth.noteOn((int)(*key), (float(keys->velocity/127.0f)));              
        } else {
            self->synth.noteOff((int)(*key));
        }
    }

    // send Panic to the synth
    static void all_notes_off(Widget_t *w, const int *value) {
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->synth.allNoteOff();
    }

    // select if we play OneShoot or Loop
    static void button_set_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->useLoop = (int)adj_get_value(w->adj);
        self->markDirty(6);
        if (self->useLoop){
            self->synth.setLoop(true);
        } else {
            self->synth.setLoop(false);
        }
    }

    // loop size control (Periods to use)
    static void setLoopSize_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->loopPeriods = (int)(adj_get_value(w->adj));
        self->markDirty(7);
        self->synth.allNoteOff();
        if (self->af.samples) button_setLoop_callback(self->setLoop, NULL);
        self->synth.allNoteOff();
    }

    // set next Loop
    static void setNextLoop_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (w->flags & HAS_POINTER && !*(int*)user_data){
            if (self->getNextLoop(self->currentLoop + 1 )) {
                self->setLoopToBank();
            }
        }
    }

    // set previous Loop
    static void setPrevLoop_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (w->flags & HAS_POINTER && !*(int*)user_data){
            if (self->getNextLoop(self->currentLoop - 1 )) {
                self->setLoopToBank();
            }
        }
    }

    // set best Loop
    static void button_setLoop_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (!self->createLoop()) {
                adj_set_value(w->adj, 0.0);
                return;
            } 
            self->setLoopToBank();
    }

    // Root Key
    static void set_root_key(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->saveRootkey = static_cast<uint8_t>(adj_get_value(w->adj));
    }


    // quit
    static void button_quit_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (w->flags & HAS_POINTER && !*(int*)user_data){
            self->onExit();
        }
    }

    // clip
    static void button_clip_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (w->flags & HAS_POINTER && !*(int*)user_data){
            self->clipToLoopMarks();
        }
    }

    // playbutton
    static void button_playbutton_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (adj_get_value(w->adj)){
            self->play = true;
        } else self->play = false;
    }

    // playbutton
    static void button_record_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (adj_get_value(w->adj)){
            self->record_sample();
            self->record = true;
        } else self->record = false;
    }

    // set left loop point by value change
    static void slider_l_changed_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        float st = adj_get_state(w->adj);
        uint32_t lp = (self->af.samplesize) * st;
        if (lp > self->position) {
            lp = self->position;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        st = std::clamp(st, 0.0f, 0.99f);
        adj_set_state(w->adj, st);
        if (adj_get_state(self->loopMark_R->adj) < st+0.01)adj_set_state(self->loopMark_R->adj, st+0.01);
        int width = self->w->width-40;
        os_move_window(self->w->app->dpy, w, 15+ (width * st), 2);
        self->loopPoint_l = lp;
        //if(self->af.samples) fprintf(stderr, "left %f\n", self->af.samples[self->loopPoint_l]);
    }

    // set left loop point by mouse wheel
    static void slider_l_released(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if(xbutton->button == Button4) {
                adj_set_value(w->adj, adj_get_value(w->adj) + 1.0);
            } else if(xbutton->button == Button5) {
                adj_set_value(w->adj, adj_get_value(w->adj) - 1.0);
            }
        }
        expose_widget(w);
    }

    // move left loop point following the mouse pointer
    static void move_loopMark_L(void *w_, void *xmotion_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Widget_t *p = (Widget_t*)w->parent;
        int x1, y1;
        os_translate_coords(w, w->widget, p->widget, xmotion->x, 0, &x1, &y1);
        int width = self->w->width-40;
        int pos = max(15, min (width+15,x1-5));
        float st =  (float)( (float)(pos-15.0)/(float)width);
        uint32_t lp = (self->af.samplesize) * st;
        if (lp > self->position) {
            self->position = lp;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        st = std::clamp(st, 0.0f, 0.99f);
        adj_set_state(w->adj, st);
    }

    // set right loop point by value changes
    static void slider_r_changed_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        float st = adj_get_state(w->adj);
        uint32_t lp = (self->af.samplesize * st);
        if (lp < self->position) {
            self->position = lp;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        st = std::clamp(st, 0.01f, 1.0f);
        adj_set_state(w->adj, st);
        if (adj_get_state(self->loopMark_L->adj) > st-0.01)adj_set_state(self->loopMark_L->adj, st-0.01);
        int width = self->w->width-40;
        os_move_window(self->w->app->dpy, w, 15 + (width * st), 2);
        self->loopPoint_r = lp;
        //if(self->af.samples) fprintf(stderr, "right %f\n", self->af.samples[self->loopPoint_r]);
    }

    // set right loop point by mouse wheel
    static void slider_r_released(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if(xbutton->button == Button4) {
                adj_set_value(w->adj, adj_get_value(w->adj) - 1.0);
            } else if(xbutton->button == Button5) {
                adj_set_value(w->adj, adj_get_value(w->adj) + 1.0);
            }
        }
        expose_widget(w);
    }

    // move right loop point following the mouse pointer
    static void move_loopMark_R(void *w_, void *xmotion_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Widget_t *p = (Widget_t*)w->parent;
        int x1, y1;
        os_translate_coords(w, w->widget, p->widget, xmotion->x, 0, &x1, &y1);
        int width = self->w->width-40;
        int pos = max(15, min (width+15,x1-5));
        float st =  (float)( (float)(pos-15.0)/(float)width);
         uint32_t lp = (self->af.samplesize * st);
        if (lp < self->position) {
            self->position = lp;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        st = std::clamp(st, 0.01f, 1.0f);
        adj_set_state(w->adj, st);
    }

    // set loop mark positions on window resize
    static void resize_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        float st = adj_get_state(self->loopMark_L->adj);
        int width = self->w->width-40;
        os_move_window(w->app->dpy, self->loopMark_L, 15+ (width * st), 2);
        st = adj_get_state(self->loopMark_R->adj);
        os_move_window(w->app->dpy, self->loopMark_R, 15+ (width * st), 2);
    }

    // set playhead position to mouse pointer
    static void set_playhead(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if(xbutton->state & Button1Mask) {
                Metrics_t metrics;
                os_get_window_metrics(w, &metrics);
                int width = metrics.width;
                int x = xbutton->x;
                float st = max(0.0, min(1.0, static_cast<float>((float)x/(float)width)));
                uint32_t lp = adj_get_max_value(w->adj) * st;
                if (lp > self->loopPoint_r) lp = self->loopPoint_r;
                if (lp < self->loopPoint_l) lp = self->loopPoint_l;
                self->position = lp;
            }
        }
    }

    // Attack control
    static void attack_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->attack = adj_get_value(w->adj);
        self->markDirty(0);
        self->synth.setAttack(self->attack);
    }

    // Decay control
    static void decay_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->decay = adj_get_value(w->adj);
        self->markDirty(1);
        self->synth.setDecay(self->decay);
    }

    // Sustain control
    static void sustain_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->sustain = adj_get_value(w->adj);
        self->markDirty(2);
        self->synth.setSustain(self->sustain);
    }

    // Release control
    static void release_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->release = adj_get_value(w->adj);
        self->markDirty(3);
        self->synth.setRelease(self->release);
    }

    // Frequency control
    static void frequency_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->frequency = adj_get_value(w->adj);
        self->markDirty(4);
        self->synth.setRootFreq(self->frequency);
    }

    // Resonance control
    static void resonance_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->resonance = adj_get_value(w->adj);
        self->markDirty(8);
        self->synth.setReso((int)self->resonance);
    }

    // Cutoff control
    static void cutoff_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->cutoff = adj_get_value(w->adj);
        self->markDirty(9);
        self->synth.setCutoff((int)self->cutoff);
    }

    // pmfreq control
    static void pmfreq_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->pmfreq = adj_get_value(w->adj);
        self->markDirty(13);
        self->synth.setPmFreq(self->pmfreq);
    }

    // pmdepth control
    static void pmdepth_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->pmdepth = adj_get_value(w->adj);
        self->markDirty(14);
        self->synth.setPmDepth(self->pmdepth);
    }


    void radio_box_set_active(Widget_t *w) {
        Widget_t * p = (Widget_t*)w->parent;
        int response = 0;
        int i = 0;
        for(;i<p->childlist->elem;i++) {
            Widget_t *wid = p->childlist->childs[i];
            if (wid->adj && wid->flags & IS_RADIO) {
                if (wid != w) adj_set_value(wid->adj_y, 0.0);
                else if (wid == w) {
                    pmmode = response;
                    markDirty(15);
                    if (adj_get_value(wid->adj) != 1.0)
                        adj_set_value(wid->adj, 1.0);
                    synth.setPmMode(pmmode);
                }
                ++response;
            }
        }
    }

    // pmmode control
    static void radio_box_button_pressed(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (w->flags & HAS_FOCUS) {
            self->radio_box_set_active(w);
        }
    }

    // VibDepth control
    static void vibdepth_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->vibdepth = adj_get_value(w->adj);
        self->markDirty(15);
        self->synth.setvibDepth(self->vibdepth);
    }

    // VibRate control
    static void vibrate_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->vibrate = adj_get_value(w->adj);
        self->markDirty(16);
        self->synth.setvibRate(self->vibrate);
    }

    // Tremolo Depth control
    static void tremdepth_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->tremdepth = adj_get_value(w->adj);
        self->markDirty(17);
        self->synth.settremDepth(self->tremdepth);
    }

    // Tremolo Rate control
    static void tremrate_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->tremrate = adj_get_value(w->adj);
        self->markDirty(18);
        self->synth.settremRate(self->tremrate);
    }

    // volume control
    static void volume_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->volume = adj_get_value(w->adj);
        self->markDirty(5);
        self->gain = std::pow(1e+01, 0.05 * self->volume);
    }

    // sharp control
    static void sharp_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->sharp = adj_get_value(w->adj);
        self->markDirty(10);
        self->process_sharp();
        self->process_sample_sharp();
        self->setOneShootBank();
        self->setLoopToBank();
    }

    // saw control
    static void saw_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->saw = adj_get_value(w->adj);
        self->markDirty(11);
        self->process_sharp();
        self->process_sample_sharp();
        self->setOneShootBank();
        self->setLoopToBank();
    }

    // fade control
    static void fade_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->fadeout = adj_get_value(w->adj);
        self->markDirty(12);
        self->process_sample_sharp();
        self->setOneShootBank();
    }

/****************************************************************
                      Theme 
****************************************************************/

    void set_custom_theme(Xputty *app) {
        app->color_scheme->normal = (Colors) {
            /* cairo / r / g / b / a */
            .fg     = { 0.878, 0.878, 0.878, 1.000 }, // heller Text
            .bg     = { 0.094, 0.094, 0.094, 1.000 }, // Hintergrund
            .base   = { 0.125, 0.125, 0.125, 1.000 }, // Panel / Widget
            .text   = { 0.878, 0.878, 0.878, 1.000 },
            .shadow = { 0.000, 0.000, 0.000, 0.300 },
            .frame  = { 0.188, 0.188, 0.188, 1.000 },
            .light  = { 0.150, 0.150, 0.150, 1.000 }
        };

        app->color_scheme->prelight = (Colors) {
            .fg     = { 0.900, 0.900, 0.900, 1.000 }, // Text bei Hover
            .bg     = { 0.250, 0.250, 0.250, 1.000 }, // leicht angehoben
            .base   = { 0.302, 0.714, 0.675, 1.000 }, // Akzentfarbe (Türkisgrün)
            .text   = { 1.000, 1.000, 1.000, 1.000 },
            .shadow = { 0.302, 0.714, 0.675, 0.300 },
            .frame  = { 0.400, 0.820, 0.765, 1.000 },
            .light  = { 0.400, 0.820, 0.765, 1.000 }
        };

        app->color_scheme->selected = (Colors) {
            .fg     = { 0.950, 0.950, 0.950, 1.000 },
            .bg     = { 0.094, 0.094, 0.094, 1.000 },
            .base   = { 0.506, 0.780, 0.518, 1.000 }, // sanftes Grün für Selektion
            .text   = { 1.000, 1.000, 1.000, 1.000 },
            .shadow = { 0.506, 0.780, 0.518, 0.300 },
            .frame  = { 0.506, 0.780, 0.518, 1.000 },
            .light  = { 0.600, 0.850, 0.600, 1.000 }
        };

        app->color_scheme->active = (Colors) {
            .fg     = { 0.000, 0.737, 0.831, 1.000 }, // Cyan-Aktivfarbe
            .bg     = { 0.000, 0.000, 0.000, 1.000 },
            .base   = { 0.180, 0.380, 0.380, 1.000 },
            .text   = { 0.800, 0.800, 0.800, 1.000 },
            .shadow = { 0.000, 0.737, 0.831, 0.400 },
            .frame  = { 0.000, 0.737, 0.831, 1.000 },
            .light  = { 0.000, 0.737, 0.831, 1.000 }
        };

        app->color_scheme->insensitive = (Colors) {
            .fg     = { 0.600, 0.600, 0.600, 0.400 },
            .bg     = { 0.100, 0.100, 0.100, 0.400 },
            .base   = { 0.000, 0.000, 0.000, 0.400 },
            .text   = { 0.600, 0.600, 0.600, 0.400 },
            .shadow = { 0.000, 0.000, 0.000, 0.200 },
            .frame  = { 0.250, 0.250, 0.250, 0.600 },
            .light  = { 0.150, 0.150, 0.150, 0.400 }
        };
    }

/****************************************************************
                      drawings 
****************************************************************/

    static void setFrameColour(Widget_t* w, int x, int y, int wi, int h) {
        Colors *c = get_color_scheme(w, NORMAL_);
        cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x, y + h);
        cairo_pattern_add_color_stop_rgba
            (pat, 0, c->bg[0]*1.9, c->bg[1]*1.9, c->bg[2]*1.9,1.0);
        cairo_pattern_add_color_stop_rgba 
            (pat, 1, c->bg[0]*0.1, c->bg[1]*0.1, c->bg[2]*0.1,1.0);
        cairo_set_source(w->crb, pat);
        cairo_pattern_destroy (pat);
    }

    static void rounded_frame(cairo_t *cr,float x, float y, float w, float h, float lsize) {
        cairo_new_path (cr);
        float r = 12.0;
        cairo_move_to(cr, x+lsize+r,y);
        cairo_line_to(cr, x+w-r,y);
        cairo_curve_to(cr, x+w,y,x+w,y,x+w,y+r);
        cairo_line_to(cr, x+w,y+h-r);
        cairo_curve_to(cr, x+w,y+h,x+w,y+h,x+w-r,y+h);
        cairo_line_to(cr, x+r,y+h);
        cairo_curve_to(cr, x,y+h,x,y+h,x,y+h-r);
        cairo_line_to(cr, x,y+r);
        cairo_curve_to(cr, x,y,x,y,x+r,y);
    }

    static void draw_frame(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width_t = metrics.width;
        int height_t = metrics.height;

        cairo_text_extents_t extents;
        //use_text_color_scheme(w, get_color_state(w));
        cairo_set_source_rgba(w->crb, 0.55, 0.65, 0.55, 1);
        cairo_set_font_size (w->crb, w->app->normal_font/w->scale.ascale);
        cairo_text_extents(w->crb,"Abc" , &extents);
        cairo_move_to (w->crb, 20, extents.height);
        cairo_show_text(w->crb, w->label);
        cairo_new_path (w->crb);

        cairo_text_extents(w->crb,w->label , &extents);
        cairo_set_line_width(w->crb,2);
        setFrameColour(w, 5, 5, width_t-10, height_t-10);
        //cairo_set_source_rgba(w->crb, 0.55, 0.65, 0.55, 1);
        rounded_frame(w->crb, 5, 5, width_t-10, height_t-8, extents.width+10);
        cairo_stroke(w->crb);
    }

    static void draw_slider(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int height = metrics.height;
        if (!metrics.visible) return;
        float center = (float)height/2;
        float upcenter = (float)height;

        use_fg_color_scheme(w, get_color_state(w));
        float point = 5.0;
        cairo_move_to (w->crb, point - 5.0, center);
        cairo_line_to(w->crb, point + 5.0, center);
        cairo_line_to(w->crb, point , upcenter);
        cairo_line_to(w->crb, point - 5.0 , center);
        cairo_fill(w->crb);
    }

    void roundrec(cairo_t *cr, float x, float y, float width, float height, float r) {
        cairo_arc(cr, x+r, y+r, r, M_PI, 3*M_PI/2);
        cairo_arc(cr, x+width-r, y+r, r, 3*M_PI/2, 0);
        cairo_arc(cr, x+width-r, y+height-r, r, 0, M_PI/2);
        cairo_arc(cr, x+r, y+height-r, r, M_PI/2, M_PI);
        cairo_close_path(cr);
    }

    static void draw_knob(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width = metrics.width;
        int height = metrics.height;
        if (!metrics.visible) return;

        const double scale_zero = 20 * (M_PI/180); // defines "dead zone" for knobs
        int arc_offset = 0;
        int knob_x = 0;
        int knob_y = 0;

        int grow = (width > height) ? height:width;
        knob_x = grow-1;
        knob_y = grow-1;
        /** get values for the knob **/

        const int knobx1 = width* 0.5;

        const int knoby1 = height * 0.5;

        const double knobstate = adj_get_state(w->adj_y);
        const double angle = scale_zero + knobstate * 2 * (M_PI - scale_zero);

        const double pointer_off =knob_x/6;
        const double radius = min(knob_x-pointer_off, knob_y-pointer_off) / 2;

        const double add_angle = 90 * (M_PI / 180.);
        // base frame
        setFrameColour(w, 0, 0, width, height);
        cairo_set_line_width(w->crb,  2.0/w->scale.ascale);
        cairo_arc (w->crb, knobx1+arc_offset, knoby1+arc_offset, radius+3,
              add_angle, add_angle + 360 * (M_PI/180));
        cairo_stroke(w->crb);
        // base
        use_base_color_scheme(w, INSENSITIVE_);
        cairo_set_line_width(w->crb,  5.0/w->scale.ascale);
        cairo_arc (w->crb, knobx1+arc_offset, knoby1+arc_offset, radius,
              add_angle + scale_zero, add_angle + scale_zero + 320 * (M_PI/180));
        cairo_stroke(w->crb);

        // indicator
        cairo_set_line_width(w->crb,  3.0/w->scale.ascale);
        cairo_new_sub_path(w->crb);
        //cairo_set_source_rgba(w->crb, 0.75, 0.75, 0.75, 1);
        use_base_color_scheme(w, PRELIGHT_);
        cairo_arc (w->crb,knobx1+arc_offset, knoby1+arc_offset, radius,
              add_angle + scale_zero, add_angle + angle);
        cairo_stroke(w->crb);

        use_text_color_scheme(w, get_color_state(w));
        cairo_text_extents_t extents;
        /** show value on the kob**/
        char s[64];
        float value = adj_get_value(w->adj);
        if (fabs(w->adj->step)>0.09)
            snprintf(s, 63, "%.1f", value);
        else
            snprintf(s, 63, "%.2f", value);
        cairo_set_font_size (w->crb, (w->app->small_font-2)/w->scale.ascale);
        cairo_text_extents(w->crb, s, &extents);
        cairo_move_to (w->crb, knobx1-extents.width/2, knoby1+extents.height/2);
        cairo_show_text(w->crb, s);
        cairo_new_path (w->crb);
    }

    void create_waveview_image(Widget_t *w, int width, int height) {
        cairo_surface_destroy(w->image);
        w->image = NULL;
        w->image = cairo_surface_create_similar (w->surface,
                            CAIRO_CONTENT_COLOR_ALPHA, width, height);
        cairo_t *cri = cairo_create (w->image);

        WaveView_t *wave_view = (WaveView_t*)w->private_struct;
        int half_height_t = height/2;
        int draw_width = width-4;

        cairo_set_line_width(cri,2);
        cairo_set_source_rgba(cri, 0.05, 0.05, 0.05, 1);
        roundrec(cri, 0, 0, width, height, 5);
        cairo_fill_preserve(cri);
        cairo_set_source_rgba(cri, 0.33, 0.33, 0.33, 1);
        cairo_stroke(cri);
        cairo_move_to(cri,2,half_height_t);
        cairo_line_to(cri, width, half_height_t);
        cairo_stroke(cri);

        if (wave_view->size<1 || !ready) {
            cairo_set_source_rgba(cri, 0.55, 0.65, 0.55, 0.4);
            cairo_set_font_size (cri, (w->app->big_font+14)/w->scale.ascale);
            cairo_move_to (cri, width*0.25, half_height_t);
            cairo_show_text(cri, "Load a Sample");
            return;
        }
        int channels = play_loop ? 1 : af.channels;
        float step = (float)((float)(wave_view->size/(float)draw_width)/(float)channels);
        float lstep = (float)(half_height_t)/channels;
        cairo_set_line_width(cri,2);
        cairo_set_source_rgba(cri, 0.55, 0.65, 0.55, 1);

        int pos = half_height_t/channels;
        for (int c = 0; c < (int)channels; c++) {
            cairo_pattern_t *pat = cairo_pattern_create_linear (0, pos, 0, height);
            cairo_pattern_add_color_stop_rgba
                (pat, 0,1.53,0.33,0.33, 1.0);
            cairo_pattern_add_color_stop_rgba
                (pat, 0.7,0.53,0.33,0.33, 1.0);
            cairo_pattern_add_color_stop_rgba
                (pat, 0.3,0.33,0.53,0.33, 1.0);
            cairo_pattern_add_color_stop_rgba
                (pat, 0, 0.55, 0.55, 0.55, 1.0);
            cairo_pattern_set_extend(pat, CAIRO_EXTEND_REFLECT);
            cairo_set_source(cri, pat);
            for (int i=0;i<draw_width;i++) {
                cairo_move_to(cri,i+2,pos);
                float w = wave_view->wave[int(c+(i*channels)*step)];
                cairo_line_to(cri, i+2,(float)(pos)+ (-w * lstep));
                //cairo_line_to(cri, i+2,(float)(pos)+ (w * lstep));
            }
            pos += half_height_t;
            cairo_pattern_destroy (pat);
            pat = nullptr;
        }
        cairo_stroke(cri);
        cairo_destroy(cri);
    }

    static void draw_wview(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width_t = metrics.width;
        int height_t = metrics.height;
        if (!metrics.visible) return;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        int width, height;
        static bool clearImage = false;
        static bool clearImageDone = false;
        if (!self->ready && !clearImageDone) clearImage = true;
        if (w->image) {
            os_get_surface_size(w->image, &width, &height);
            if (((width != width_t || height != height_t) || self->loadNew) && self->ready) {
                clearImageDone = false;
                self->create_waveview_image(w, width_t, height_t);
                os_get_surface_size(w->image, &width, &height);
                self->loadNew = false;
            }
        } else {
            self->create_waveview_image(w, width_t, height_t);
            os_get_surface_size(w->image, &width, &height);
        }
        if (clearImage) {
            clearImage = false;
            clearImageDone = true;
            self->create_waveview_image(w, width_t, height_t);
            os_get_surface_size(w->image, &width, &height);
        }
        cairo_set_source_surface (w->crb, w->image, 0, 0);
        cairo_rectangle(w->crb,0, 0, width, height);
        cairo_fill(w->crb);

        double state = adj_get_state(w->adj);
        cairo_set_source_rgba(w->crb, 0.55, 0.05, 0.05, 1);
        cairo_rectangle(w->crb, (width * state) - 1.5,2,3, height-4);
        cairo_fill(w->crb);

        //int halfWidth = width*0.5;

        double state_l = adj_get_state(self->loopMark_L->adj);
        cairo_set_source_rgba(w->crb, 0.25, 0.25, 0.05, 0.666);
        cairo_rectangle(w->crb, 0, 2, (width*state_l), height-4);
        cairo_fill(w->crb);

        double state_r = adj_get_state(self->loopMark_R->adj);
        cairo_set_source_rgba(w->crb, 0.25, 0.25, 0.05, 0.666);
        int point = (width*state_r);
        cairo_rectangle(w->crb, point, 2 , width - point, height-4);
        cairo_fill(w->crb);

        if (self->loopPoint_l_auto && self->loopPoint_r_auto) {
            double lstate = (double)self->loopPoint_l_auto/ (double)self->af.samplesize;
            double rstate = (double)self->loopPoint_r_auto/ (double)self->af.samplesize;
            int lpoint = (width*lstate);
            int rpoint = (width*rstate);
            cairo_set_source_rgba(w->crb, 0.25, 0.25, 0.65, 0.444);
            cairo_rectangle(w->crb, lpoint, 2 , max(1,rpoint-lpoint), height-4);
            cairo_fill(w->crb);
        }

        if (!self->ready) 
            show_spinning_wheel(w, nullptr);
        if (self->record && self->timer > 0)
            show_spinning_wheel(w, nullptr);
    }

    static void draw_lwview(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width_t = metrics.width;
        int height_t = metrics.height;
        if (!metrics.visible) return;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        int width, height;
        static bool clearImage = false;
        static bool clearImageDone = false;
        if (!self->ready && !clearImageDone) clearImage = true;
        if (w->image) {
            os_get_surface_size(w->image, &width, &height);
            if (((width != width_t || height != height_t) || self->loadLoopNew) && self->ready) {
                clearImageDone = false;
                self->create_waveview_image(w, width_t, height_t);
                os_get_surface_size(w->image, &width, &height);
                self->loadLoopNew = false;
            }
        } else {
            self->create_waveview_image(w, width_t, height_t);
            os_get_surface_size(w->image, &width, &height);
        }
        if (clearImage) {
            clearImage = false;
            clearImageDone = true;
            self->create_waveview_image(w, width_t, height_t);
            os_get_surface_size(w->image, &width, &height);
        }
        cairo_set_source_surface (w->crb, w->image, 0, 0);
        cairo_rectangle(w->crb,0, 0, width, height);
        cairo_fill(w->crb);

        if (!self->ready) 
            show_spinning_wheel(w, nullptr);

    }

    static void drawWheel(Widget_t *w, float di, int x, int y, int radius, float s) {
        cairo_set_line_width(w->crb,10 / w->scale.ascale);
        cairo_set_line_cap (w->crb, CAIRO_LINE_CAP_ROUND);
        int i;
        const int d = 1;
        for (i=375; i<455; i++) {
            double angle = i * 0.01 * 2 * M_PI;
            double rx = radius * sin(angle);
            double ry = radius * cos(angle);
            double length_x = x - rx;
            double length_y = y + ry;
            double radius_x = x - rx * s ;
            double radius_y = y + ry * s ;
            double z = i/420.0;
            if ((int)di < d) {
                cairo_set_source_rgba(w->crb, 0.66*z, 0.66*z, 0.66*z, 0.3);
                cairo_move_to(w->crb, radius_x, radius_y);
                cairo_line_to(w->crb,length_x,length_y);
                cairo_stroke_preserve(w->crb);
            }
            di++;
            if (di>8.0) di = 0.0;
        }
    }

    static void show_spinning_wheel(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width = metrics.width;
        int height = metrics.height;
        if (!metrics.visible) return;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        static const float sCent = 0.666;
        static float collectCents = 0;
        collectCents -= sCent;
        if (collectCents>8.0) collectCents = 0.0;
        else if (collectCents<0.0) collectCents = 8.0;
        self->drawWheel (w, collectCents,width*0.5, height*0.5, height*0.3, 0.98);
        cairo_stroke(w->crb);
    }

    static void draw_window(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Widget_t *p = (Widget_t*)w->parent;
        Metrics_t metrics;
        os_get_window_metrics(p, &metrics);
        if (!metrics.visible) return;
        use_bg_color_scheme(w, NORMAL_);
        cairo_paint (w->crb);
    }

/****************************************************************
                      Preset handling
****************************************************************/

    void showExportWindow() {
        Widget_t *dia = save_file_dialog(w_top, "", "audio");
        dia->private_struct = (void*)this;
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        XSetTransientForHint(w_top->app->dpy, dia->widget, w_top->widget);
        #endif
        Widget_t *rootKey = add_combobox(dia, "", 260, 355, 70, 30);
        rootKey->parent_struct = (void*)this;
        for (auto & element : keys) {
            combobox_add_entry(rootKey, element.c_str());
        }
        combobox_set_menu_size(rootKey, 12);
        combobox_set_active_entry(rootKey, saveRootkey);
        rootKey->func.value_changed_callback = set_root_key;
        widget_show_all(dia);
        w_top->func.dialog_callback = [] (void *w_, void* user_data) {
            Widget_t *w = (Widget_t*)w_;
            if(user_data !=NULL && strlen(*(const char**)user_data)) {
                Loopino *self = static_cast<Loopino*>(w->parent_struct);
                std::string filename = *(const char**)user_data;
                std::string::size_type idx;
                idx = filename.rfind('.');
                if(idx != std::string::npos) {
                    std::string extension = filename.substr(0, idx-1);
                    filename = extension;
                }
                std::string sampleFileName = filename + self->keys[self->saveRootkey] + ".wav";
                std::string loopFileName = filename + self->keys[self->saveRootkey] + "_loop" + ".wav";
                std::vector<float> s;
                std::vector<float> l;
                self->synth.getSaveBuffer(false, s, self->saveRootkey, 1);
                self->synth.getSaveBuffer(true, l, self->saveRootkey, 48);
                self->af.saveAudioFile(sampleFileName, s.data(), s.size(), self->jack_sr);
                self->af.saveAudioFile(loopFileName, l.data(), l.size(), self->jack_sr);
            }
        };
    }

    std::string getPathFor(const std::string& name) const {
        return presetDir + name + ".presets";
    }

    void createPrestList() {
        presetFiles.clear();
        std::filesystem::path p = std::filesystem::path(presetFile).parent_path().u8string();
        for (auto &f : std::filesystem::directory_iterator(p)) {
            if (f.path().extension() == ".presets") {
                presetFiles.push_back(f.path().stem().string());
            }
        }
    }

    // pop up a text entry to enter a name for a preset to save
    void saveAs() {
        Widget_t* dia = showTextEntry(w_top, 
                    "Loopino - save preset as:", "Save preset as:");
        int x1, y1;
        os_translate_coords( w_top, w_top->widget, 
            os_get_root_window(w_top->app, IS_WIDGET), 0, 0, &x1, &y1);
        os_move_window(w_top->app->dpy,dia,x1+190, y1+80);
        w_top->func.dialog_callback = [] (void *w_, void* user_data) {
            Widget_t *w = (Widget_t*)w_;
            if(user_data !=NULL && strlen(*(const char**)user_data)) {
                Loopino *self = static_cast<Loopino*>(w->parent_struct);
                self->presetName = (*(const char**)user_data);
                self->savePreset(self->getPathFor(self->presetName));
            }
        };
    }

    // save menu callback
    void save() {
        if (presetName.empty()) saveAs();
        savePreset(getPathFor(presetName));
    }

    void showPresetMenu(Widget_t *w) {
        createPrestList();
        Widget_t *menu = create_menu(w, 20);
        menu->parent_struct = (void*)this;
        Widget_t *menuSave = menu_add_item(menu, "Save");
        menuSave->parent_struct = (void*)this;
        Widget_t *menuSaveAs = menu_add_item(menu, "Save As...");
        menuSaveAs->parent_struct = (void*)this;
        Widget_t *loadSub = cmenu_add_submenu(menu, "Load");
        loadSub->parent_struct = (void*)this;
        for (size_t i = 0; i < presetFiles.size(); ++i) {
            menu_add_entry(loadSub, presetFiles[i].c_str());
        }
        Widget_t *def = menu_add_item(menu, "Default");
        def->parent_struct = (void*)this;
        Widget_t *expo = menu_add_item(menu, "Export");
        expo->parent_struct = (void*)this;
        menuSave->func.button_release_callback = [](void *w_, void*item_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            self->save();
        };
        menuSaveAs->func.button_release_callback = [](void *w_, void*item_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            self->saveAs();
        };
        loadSub->func.value_changed_callback = [](void *w_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            int id = (int)w->adj->value;
            if (id >= 0 && id < (int)self->presetFiles.size()) {
                std::string name = self->presetFiles[id];
                std::string path = self->getPathFor(name);
                self->loadPreset(path);
            }
        };
        def->func.button_release_callback = [](void *w_, void*item_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            self->generateSine();
        };
        expo->func.button_release_callback = [](void *w_, void*item_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            self->showExportWindow();
        };
        pop_menu_show(w, menu, 8, true);

    }

    static void presets_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (w->flags & HAS_POINTER && !*(int*)user_data){
            self->showPresetMenu(w);
        }
    }

    void getConfigFilePath() {
         if (getenv("XDG_CONFIG_HOME")) {
            std::string path = getenv("XDG_CONFIG_HOME");
            configFile = path + "/loopino/loopino.conf";
            presetFile = path + "/loopino/loopino.presets";
            presetDir = path + "/loopino/";
        } else {
        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
            std::string path = getenv("HOME");
            configFile = path +"/.config/loopino/loopino.conf";
            presetFile = path +"/.config/loopino/loopino.presets";
            presetDir = path +"/.config/loopino/";
        #else
            std::string path = getenv("APPDATA");
            configFile = path +"\\.config\\loopino\\loopino.conf";
            presetFile = path +"\\.config\\loopino\\loopino.presets";
            presetDir = path +"\\.config\\loopino\\";
        #endif
       }
        std::filesystem::path p = std::filesystem::path(presetFile).parent_path().u8string();
        if (!std::filesystem::exists(p)) {
            std::filesystem::create_directory(p);
        }
    }

    struct PresetHeader {
        char magic[8];
        uint32_t version;
        uint64_t dataSize;
    };

    // Helper functions
    template <typename O, typename T>
    void writeString(O& out, T& v) {
        out.write(reinterpret_cast<char*>(&v), sizeof(T));
    }

    template <typename O, typename T>
    void writeValue(O& out, const T& v) {
        out.write(reinterpret_cast<const char*>(&v), sizeof(T));
    }

    template <typename I, typename T>
    void readValue(I& in, T& v) {
        in.read(reinterpret_cast<char*>(&v), sizeof(T));
    }

    template <typename T>
    void writeControllerValue(T& out, Widget_t *w) {
        float v = adj_get_value(w->adj);
        writeValue(out, v);
    }

    template <typename T>
    void readControllerValue(T& in, Widget_t *w) {
        float v = 0.0;
        readValue(in , v);
        adj_set_value(w->adj, v);
    }

    template <typename T>
    bool writeSampleBuffer(T& out, const float* samples, uint32_t numData) {
        if (!samples || numData == 0) return false;

        if (!out) return false;

        writeValue(out, numData);

        float maxVal = 0.0f;
        for (size_t i = 0; i < numData; ++i) {
            maxVal = max(maxVal, std::fabs(samples[i]));
        }
        if (maxVal < 1e-9f) maxVal = 1.0f;
        
        for (size_t i = 0; i < numData; ++i) {
            float normalized = samples[i] / maxVal;
            int16_t encoded = static_cast<int16_t>(std::round(normalized * 32767.0f));
            writeValue(out,encoded);
        }
        return true;
    }

    template <typename T>
    bool readSampleBuffer(T& in, float*& samples, uint32_t& numData) {
        if (!in) return false;

        readValue(in, numData);
        if (numData == 0) return false;

        samples = new float[numData];
        
        for (size_t i = 0; i < numData; ++i) {
            int16_t encoded;
            readValue(in,encoded);
            samples[i] = static_cast<float>(encoded) / 32767.0f;
        }
        return true;
    }

    // Preset Save 
    bool savePreset(const std::string& filename) {
        std::filesystem::path p = std::filesystem::path(filename).parent_path().u8string();
        if (!std::filesystem::exists(p)) {
            std::filesystem::create_directory(p);
        }
        std::ofstream out(filename, std::ios::binary);
        if (!out) return false;
        PresetHeader header;
        std::memcpy(header.magic, "LOOPINO", 8);
        header.version = 8; // guard for future proof
        header.dataSize = af.samplesize;
        writeString(out, header);

        writeValue(out, currentLoop);
        writeControllerValue(out, Attack);
        writeControllerValue(out, Decay);
        writeControllerValue(out, Sustain);
        writeControllerValue(out, Release);
        writeControllerValue(out, Frequency);
        writeControllerValue(out, setLoop);
        writeControllerValue(out, setLoopSize);
        // since version 3
        writeControllerValue(out, Resonance);
        writeControllerValue(out, CutOff);
        // since version 4
        writeControllerValue(out, Sharp);
        // since version 5
        writeControllerValue(out, Saw);
        // since version 6
        writeControllerValue(out, FadeOut);
        // since version 7
        writeControllerValue(out, PmFreq);
        writeControllerValue(out, PmDepth);
        writeValue(out, pmmode);
        // since version 8
        writeControllerValue(out, VibDepth);
        writeControllerValue(out, VibRate);
        writeControllerValue(out, TremDepth);
        writeControllerValue(out, TremRate);

        writeSampleBuffer(out, af.samples, af.samplesize);
        out.close();
        std::string tittle = "loopino: " + presetName;
        widget_set_title(w_top, tittle.data());
        return true;
    }

    // Preset  Load
    bool loadPreset(const std::string& filename) {
        std::ifstream in(filename, std::ios::binary);
        if (!in) return false;
        PresetHeader header{};
        readValue(in, header);
        if (std::strncmp(header.magic, "LOOPINO", 7) != 0) {
            std::cerr << "Invalid preset file\n";
            return false;
        }

        // we need to update the header version when change the preset format
        // then we could protect new values with a guard by check the header version
        if (header.version > 8) {
            std::cerr << "Warning: newer preset version (" << header.version << ")\n";
        }

        readValue(in, currentLoop);
        readControllerValue(in, Attack);
        readControllerValue(in, Decay);
        readControllerValue(in, Sustain);
        readControllerValue(in, Release);
        readControllerValue(in, Frequency);
        readControllerValue(in, setLoop);
        readControllerValue(in, setLoopSize);
        if (header.version > 2) {
            readControllerValue(in, Resonance);
            readControllerValue(in, CutOff);
        }
        if (header.version > 3) {
            readControllerValue(in, Sharp);
        }
        if (header.version > 4) {
            readControllerValue(in, Saw);
        }
        if (header.version > 5) {
            readControllerValue(in, FadeOut);
        }
        if (header.version > 6) {
            readControllerValue(in, PmFreq);
            readControllerValue(in, PmDepth);
            readValue(in, pmmode);
            radio_box_set_active(PmMode[pmmode]);
        }
        if (header.version > 7) {
            readControllerValue(in, VibDepth);
            readControllerValue(in, VibRate);
            readControllerValue(in, TremDepth);
            readControllerValue(in, TremRate);
        }

        readSampleBuffer(in, af.samples, af.samplesize);
        in.close();
        adj_set_max_value(wview->adj, (float)af.samplesize);
        adj_set_state(loopMark_L->adj, 0.0);
        adj_set_state(loopMark_R->adj,1.0);
        loadLoopNew = true;
        //loadNew = true;
       // update_waveview(wview, af.samples, af.samplesize);
        haveDefault = false;
        loadPresetToSynth();
        std::filesystem::path p = filename;
        presetName = p.stem().string();
        std::string tittle = "loopino: " + presetName;
        widget_set_title(w_top, tittle.data());
        return true;
    }

};

#endif

