
/*
 * Loopino.h
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
#include "Smoother.h"
#include "SamplePlayer.h"
#include "Parameter.h"

#include "SizeGroup.h"
#include "TextEntry.h"
#include "Wheel.h"
#include "ADSRview.h"

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

#define MAX_FLOAT_BINDINGS  70
#define MAX_INT_BINDINGS    25

#define WINDOW_WIDTH  966
#define WINDOW_HEIGHT 570

#define COL_ENV       0.18f,0.75f,0.72f,1.0f
#define COL_PITCH     0.85f,0.52f,0.24f,1.0f
#define COL_SYS       0.65f,0.65f,0.65f,1.0f
#define COL_FILTER    0.42f,0.78f,0.38f,1.0f
#define COL_RESO      0.60f,0.90f,0.45f,1.0f
#define COL_MOD       0.42f,0.52f,0.90f,1.0f
#define COL_VINT      0.75f,0.65f,0.45f,1.0f
#define COL_REC       0.85f,0.30f,0.25f,1.0f
#define UI_PANEL      0.10f,0.11f,0.11f,1.0f
#define UI_FRAME      0.16f,0.18f,0.18f,1.0f
#define UI_TEXT       0.78f,0.80f,0.78f,1.0f
#define UI_SUBTEXT    0.45f,0.48f,0.48f,1.0f

using ExposeFunc = void (*)(void* w_, void* user_data);

class Loopino;

template<typename T>
struct ValueBinding {
    T Loopino::*member;
    int dirtyIndex;
    void (*extra)(Loopino*, T);
};

/****************************************************************
    class Loopino - create the GUI for loopino
****************************************************************/

class Loopino : public TextEntry
{
public:
    Xputty app;
    Widget_t *w_top;
    Widget_t *PitchWheel;
    Widget_t *keyboard;
    ParallelThread pa;
    ParallelThread fetch;
    AudioFile af;
    PolySynth synth;
    Params param;

    SampleBank sbank;
    std::shared_ptr<SampleInfo> sampleData { nullptr };
    SampleBank lbank;
    std::shared_ptr<SampleInfo> loopData { nullptr };
    std::vector<int> machineOrder;
    std::vector<int> filterOrder;

    uint32_t jack_sr;
    uint32_t position;
    uint32_t loopPoint_l;
    uint32_t loopPoint_r;
    uint32_t loopPoint_l_auto;
    uint32_t loopPoint_r_auto;
    uint32_t frameSize;

    uint8_t rootkey;
    uint8_t customRootkey;
    uint8_t loopRootkey;
    uint8_t saveRootkey;

    int16_t pitchCorrection;
    int16_t loopPitchCorrection;
    int16_t matches;
    int16_t currentLoop;
    int16_t loopPeriods;
    int16_t timer;

    float freq;
    float customFreq;
    float loopFreq;
    float gain;
    float volume;
    int glowDragX;
    int glowDragY;

    bool loadNew;
    bool loadLoopNew;
    bool play;
    bool play_loop;
    bool ready;
    bool havePresetToLoad;
    bool record;

    Loopino() : af() {
        sampleData = std::make_shared<SampleInfo>();
        loopData = std::make_shared<SampleInfo>();
        machineOrder = {20,21,22,23,24,25};
        filterOrder = {8,9,10,11,12};
        jack_sr = 0;
        position = 0;
        loopPoint_l = 0;
        loopPoint_r = 1000;
        loopPoint_l_auto = 0;
        loopPoint_r_auto = 0;
        frameSize = 0;
        gain = std::pow(1e+01, 0.05 * 0.0);
        glowDragX = -1;
        glowDragY = -1;
        is_loaded = false;
        loadNew = false;
        loadLoopNew = false;
        play = false;
        play_loop = false;
        ready = true;
        havePresetToLoad = false;
        record = false;
        inDrag = false;
        matches = 0;
        currentLoop = 0;
        loopPeriods = 1;
        freq = 0.0;
        customFreq = 0.0;
        pitchCorrection = 0;
        rootkey = 60;
        customRootkey = 60;
        saveRootkey = 69;
        loopFreq = 0.0;
        loopPitchCorrection = 0;
        loopRootkey = 69;
        loadPresetMIDI = -1;
        lastPresetMIDI = -1;
        currentPresetNum = -1;
        p = 0;
        firstLoop = true;

        useLoop = 0;
        xruns = 0;
        timer = 30;
        generateKeys();
        guiIsCreated = false;
        registerParameters();
        param.resetParams();
    };

    ~Loopino() {
        pa.stop();
        #if defined(DEBUG)
        //fftwf_cleanup();
        //cairo_debug_reset_static_data();
        #endif
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
        syncValuesToSynth();
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
       // if (v < 0 || v > (int)presetFiles.size()-1) return;
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

    void getXrun() {
        xruns++;
    }

/****************************************************************
                 Clap wrapper
****************************************************************/

    void clearValueBindings() {
        floatBindingCount = 0;
        intBindingCount = 0;
    }

    void markDirty(int num) {
        #if defined (RUN_AS_PLUGIN)
        param.setParamDirty(num , true);
        param.controllerChanged.store(true, std::memory_order_release);
        #endif
    }

//#if defined (RUN_AS_PLUGIN)
#include "Clap/LoopinoClapWrapper.cc"
//#endif

/****************************************************************
                      main window
****************************************************************/

    void setCursor(Widget_t *frame) {
        #ifdef _WIN32
        frame->cursor = LoadCursor(NULL, IDC_HAND);
        frame->cursor2 = LoadCursor(NULL, IDC_SIZEALL);
        #else
        frame->cursor = XCreateFontCursor(frame->app->dpy, XC_hand2);
        frame->cursor2 = XCreateFontCursor(frame->app->dpy, XC_sb_h_double_arrow);
        #endif
    }

    void setFrameCallbacks(Widget_t *frame) {
        setCursor(frame);
        frame->func.button_press_callback = drag_frame;
        frame->func.motion_callback = move_frame;;
        frame->func.button_release_callback = drop_frame;
    }

    // create the main GUI
    void createGUI(Xputty *app) {
        #ifndef RUN_AS_PLUGIN
        set_custom_theme(app);
        // top level window
        w_top = create_window(app, os_get_root_window(app, IS_WINDOW), 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
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
        os_set_window_min_size(w_top, WINDOW_WIDTH, 390, WINDOW_WIDTH, WINDOW_HEIGHT);

        // sample view
        w = create_widget(app, w_top, 0, 0, 484, 140);
        w->parent = w_top;
        w->scale.gravity = NORTCENTER;
        w->func.expose_callback = draw_window;
        commonWidgetSettings(w);

        loopMark_L = add_hslider(w, "",15, 2, 18, 18);
        loopMark_L->scale.gravity = NONE;
        setCursor(loopMark_L);
        loopMark_L->parent_struct = (void*)this;
        set_adjustment(loopMark_L->adj_x,0.0, 0.0, 0.0, 1000.0,1.0, CL_METER);
        //loopMark_L->adj_x = loopMark_L->adj_x_x;
        add_tooltip(loopMark_L, "Set left clip point ");
        os_set_window_attrb(loopMark_L);
        loopMark_L->func.expose_callback = draw_slider;
        loopMark_L->func.button_release_callback = slider_l_released;
        loopMark_L->func.button_press_callback = slider_pressed;
        loopMark_L->func.motion_callback = move_loopMark_L;
        loopMark_L->func.value_changed_callback = slider_l_changed_callback;

        loopMark_R = add_hslider(w, "",463, 2, 18, 18);
        loopMark_R->scale.gravity = NONE;
        setCursor(loopMark_R);
        loopMark_R->parent_struct = (void*)this;
        set_adjustment(loopMark_R->adj_x,0.0, 0.0, -1000.0, 0.0,1.0, CL_METER);
        //loopMark_R->adj_x = loopMark_R->adj_x_x;
        add_tooltip(loopMark_R, "Set right clip point ");
        os_set_window_attrb(loopMark_R);
        loopMark_R->func.expose_callback = draw_slider;
        loopMark_R->func.button_release_callback = slider_r_released;
        loopMark_R->func.button_press_callback = slider_pressed;
        loopMark_R->func.motion_callback = move_loopMark_R;
        loopMark_R->func.value_changed_callback = slider_r_changed_callback;

        wview = add_waveview(w, "", 20, 20, 448, 120);
        wview->scale.gravity = NORTHWEST;
        wview->adj_x = add_adjustment(wview,0.0, 0.0, 0.0, 1000.0,1.0, CL_METER);
        wview->adj = wview->adj_x;
        wview->func.expose_callback = draw_wview;
        wview->func.button_release_callback = set_playhead;
        commonWidgetSettings(wview);

        lw = create_widget(app, w_top, 484, 0, 484, 140);
        lw->parent = w_top;
        lw->scale.gravity = NORTCENTER;
        lw->func.expose_callback = draw_window;
        commonWidgetSettings(lw);

        loopview = add_waveview(lw, "", 20, 20, 448, 120);
        loopview->scale.gravity = NORTHWEST;
        loopview->adj_x = add_adjustment(loopview,0.0, 0.0, 0.0, 1000.0,1.0, CL_METER);
        loopview->adj = loopview->adj_x;
        loopview->func.expose_callback = draw_lwview;
        //loopview->func.button_release_callback = set_playhead;
        commonWidgetSettings(loopview);
        // Controls Window takes all space between wave view and keyboard
        Controls = create_widget(app, w_top, 0, 140, WINDOW_WIDTH, WINDOW_HEIGHT - 140 - 80);
        Controls->parent = w_top;
        Controls->scale.gravity = WESTEAST;
        Controls->func.expose_callback = draw_window_box;
        commonWidgetSettings(Controls);
        sz.setParent(Controls, 10, 10, 5, 10, 75 * app->hdpi, &glowDragX, &glowDragY);
        Widget_t* frame = nullptr;

        sz.add(frame = add_frame(Controls, "Sample Buffer", 0, 0, 285, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addSampleBufferControlls(frame);

        sz.add(frame = add_frame(Controls, "Phase Modulator", 0, 0, 175, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addPhaseModulatorControlls(frame);

        sz.add(frame = add_frame(Controls, "Loop Buffer", 0, 0, 168, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addLoopBufferControlls(frame);

        sz.add(frame = add_frame(Controls, "Sharp", 0, 0, 100, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addSharpControlls(frame);

        sz.add(frame = add_frame(Controls, "Tone", 0, 0, 63, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addToneControlls(frame);

        sz.add(frame = add_frame(Controls, "Gain", 0, 0, 63, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addGainControlls(frame);

        #ifndef RUN_AS_PLUGIN
        sz.add(frame = add_frame(Controls, "Exit", 0, 0, 63, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addExitControlls(frame);
        #endif
        /*
        frame = add_frame(Controls, "Age", 808+86, 10, 62, 75);
        frame->scale.gravity = ASPECT;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);

        Age = add_knob(frame, "Age",12,25,38,38);
        set_adjustment(Age->adj, 0.25, 0.25, 0.00, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Age, (Color_state)1, (Color_mod)2, 0.894, 0.106, 0.623, 1.0);
        commonWidgetSettings(Age);
        connectValueChanged(Age, &Loopino::age, 50, "Vintage Age", draw_knob,
            [](Loopino* self, float v) {self->synth.setAge(v);});
        */

        sz.add(frame = add_frame(Controls, "Frequency", 0, 0, 91, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addFreqControlls(frame);

        sz.add(frame = add_frame(Controls, "Acid-18 Filter", 0, 0, 170, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 8;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addAcidControlls(frame);

        sz.add(frame = add_frame(Controls, "Wasp Filter", 0, 0, 184, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 9;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addWaspControlls(frame);

        sz.add(frame = add_frame(Controls, "LP Ladder Filter", 0, 0, 147, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 10;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addLPLadderControlls(frame);

        sz.add(frame = add_frame(Controls, "HP Ladder Filter", 0, 0, 147, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 11;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addHPLadderControlls(frame);

        sz.add(frame = add_frame(Controls, "SEM12 Filter", 0, 0, 184, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 12;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addSEM12Controlls(frame);

        Widget_t *eframe = nullptr;
        sz.add(eframe = add_frame(Controls, "Envelope", 0, 0, 178, 75));
        eframe->scale.gravity = ASPECT;
        eframe->data = -1;
        eframe->func.expose_callback = draw_frame;
        commonWidgetSettings(eframe);

        sz.add(frame = add_frame(Controls, "Dynamic", 0, 0, 83, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addDynamicControlls(frame);

        sz.add(frame = add_frame(Controls, "Vibrato", 0, 0, 130, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addVibratoControlls(frame);

        sz.add(frame = add_frame(Controls, "Tremolo",0, 0, 130, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addTremoloControlls(frame);

        sz.add(frame = add_frame(Controls, "Chorus",0, 0, 205, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addChorusControlls(frame);

        sz.add(frame = add_frame(Controls, "Reverb",0, 0, 165, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addReverbControlls(frame);


        PitchWheel = add_wheel(Controls, "", 845+86, 185, 20, 75);
        PitchWheel->scale.gravity = SOUTHWEST;
        PitchWheel->flags |= HAS_TOOLTIP;
        add_tooltip(PitchWheel, "Pitch Bend");
        commonWidgetSettings(PitchWheel);
        PitchWheel->func.value_changed_callback = wheel_callback;

        sz.add(frame = add_frame(Controls, "ADSR", 0, 0, 178, 75));
        frame->scale.gravity = ASPECT;
        frame->data = -1;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        addADSRControlls(frame);
        addEnvelopeControlls(eframe);

        sz.add(frame = add_frame(Controls, "8-bit Machine",0, 0, 130, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 20;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        add8bitControlls(frame);

        sz.add(frame = add_frame(Controls, "12-bit Machine",0, 0, 130, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 21;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        add12bitControlls(frame);

        sz.add(frame = add_frame(Controls, "Pump Machine",0, 0, 130, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 22;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addPumpControlls(frame);

        sz.add(frame = add_frame(Controls, "Studio-16 Machine",0, 0, 170, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 23;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addStudio16Controllse(frame);

        sz.add(frame = add_frame(Controls, "Vintage",0, 0, 90, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 24;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addTimeControlls(frame);

        sz.add(frame = add_frame(Controls, "Smooth",0, 0, 90, 75));
        frame->scale.gravity = ASPECT;
        frame->data = 25;
        frame->func.expose_callback = draw_frame;
        commonWidgetSettings(frame);
        setFrameCallbacks(frame);
        addSmoothControlls(frame);

        keyboard = add_midi_keyboard(w_top, "Organ", 0, WINDOW_HEIGHT - 80, WINDOW_WIDTH, 80);
        keyboard->flags |= HIDE_ON_DELETE;
        keyboard->parent_struct = (void*)this;
        MidiKeyboard* keys = (MidiKeyboard*)keyboard->private_struct;
        Widget_t *view_port = keys->context_menu->childlist->childs[0];
        Widget_t *octavemap = view_port->childlist->childs[1];
        keys->octave = 12*3;
        keys->velocity = 100;
        keys->key_size = 23;
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
        setValuesFromHost();
        //sz.applyPresetOrder(machineOrder);
    }

private:
    SizeGroup sz;
    Widget_t *w, *lw, *Controls;
    Widget_t *w_quit;
    Widget_t *filebutton;
    Widget_t *loopview, *wview;
    Widget_t *loopMark_L, *loopMark_R, *setLoop, *setLoopSize,
             *setNextLoop, *setPrevLoop;
    Widget_t *playbutton;
    Widget_t *Volume, *Tone, *Age;
    Widget_t *clip;
    Widget_t *Presets;
    Widget_t *Record, *Reverse;
    Widget_t *RootKey;

    Widget_t *Attack, *Decay, *Sustain, *Release;
    Widget_t *Envelope;
    Widget_t *Frequency;
    Widget_t *Resonance, *CutOff, *LpOnOff, *LpKeyTracking;
    Widget_t *HpResonance, *HpCutOff, *HpOnOff, *HpKeyTracking;
    Widget_t *Sharp, *Saw;
    Widget_t *FadeOut, *PmFreq, *PmDepth, *PmMode[4];
    Widget_t *VibDepth, *VibRate, *VibOnOff;
    Widget_t *TremDepth, *TremRate, *TremOnOff;
    Widget_t *VelMode[3];

    Widget_t *ObfResonance, *ObfCutOff, *ObfKeyTracking, *ObfMode, *ObfOnOff;
    Widget_t *ChorusFreq, *ChorusDelay, *ChorusLev, *ChorusDepth, *ChorusOnOff;
    Widget_t *RevRoomSize, *RevDamp, *RevMix, *RevOnOff, *WaspOnOff;
    Widget_t *WaspCutOff, *WaspResonance, *WaspKeyTracking, *WaspMix;
    Widget_t *TBCutOff, *TBResonance, *TBVintage, *TBOnOff;
    Widget_t *LM_MIR8OnOff, *LM_MIR8Drive, *LM_MIR8Amount;
    Widget_t *Emu_12OnOff, *Emu_12Drive, *Emu_12Amount;
    Widget_t *LM_CMP12OnOff, *LM_CMP12Drive, *LM_CMP12Ratio;
    Widget_t *Studio_16OnOff, *Studio_16Drive, *Studio_16Warmth, *Studio_16HfTilt;
    Widget_t *EPSOnOff, *EPSDrive;
    Widget_t *TMOnOff, *TMTime;

    Window    p;

    SupportedFormats supportedFormats;
    PitchTracker pt;
    LoopGenerator lg;
    Smoother smooth;

    std::vector<float> loopBuffer;
    std::vector<float> loopBufferSave;
    std::vector<float> sampleBuffer;
    std::vector<float> sampleBufferSave;

    bool is_loaded;
    bool firstLoop;
    bool guiIsCreated;
    bool inDrag;
    std::string newLabel;
    std::vector<std::string> keys;
    std::vector<std::string> presetFiles;

    std::string configFile;
    std::string presetFile;
    std::string presetDir;
    std::string presetName;
    std::string filename;

    int loadPresetMIDI;
    int lastPresetMIDI;
    int currentPresetNum;
    
    float attack, decay, sustain, release;
    float frequency, tone, age;
    float resonance, cutoff, lpkeytracking;
    float hpresonance, hpcutoff, hpkeytracking;
    float sharp, saw;
    float fadeout;
    float pmfreq, pmdepth;
    float vibdepth, vibrate;
    float tremdepth, tremrate;
    float pitchwheel;
    float obfresonance, obfcutoff, obfkeytracking, obfmode;
    float chorusfreq, chorusdelay, choruslev, chorusdepth;
    float revroomsize, revdamp, revmix;
    float waspcutoff, waspresonance, waspkeytracking, waspmix;
    float tbcutoff, tbresonance, tbvintage;
    float mrgdrive, mrgamount;
    float emu_12drive, emu_12amount;
    float cmp12drive, cmp12ratio;
    float studio16drive, studio16warmth, studio16hftilt;
    float epsdrive, tmtime;
    int vibonoff, tremonoff, lponoff, hponoff, obfonoff, chorusonoff, revonoff,
        wasponoff, tbonoff, mrgonoff, emu_12onoff, cmp12onoff, studio16onoff, epsonoff, tmonoff;
    int pmmode;
    int velmode;
    int useLoop;
    int reverse;
    int xruns;
    int pressMark = 0;
    int LMark = 0;
    
    std::vector<float> analyseBuffer;


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
        customRootkey = rootkey;
        if (guiIsCreated) combobox_set_active_entry(RootKey, rootkey);
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
        if (std::abs(saw) <= 0.0001f) return;

        float sawAmount = std::abs(saw);
        bool reverseSaw = (saw < 0.0f);

        const size_t N = buffer.size();
        float* out = buffer.data();

        const float snapTime = 0.003f * sawAmount;
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

            float mn = out[start], mx = out[start];
            for (size_t i = start; i < end; ++i) {
                mn = std::min<float>(mn, out[i]);
                mx = std::max<float>(mx, out[i]);
            }

            // linear ramp (forward / reverse)
            for (size_t i = 0; i < len; ++i) {
                float t = float(i) / float(len - 1);
                float linear;

                if (!reverseSaw) {
                    if (sgn > 0.0f) linear = mn + t * (mx - mn);
                    else           linear = mx + t * (mn - mx);
                } else {
                    if (sgn > 0.0f) linear = mx - t * (mx - mn);
                    else           linear = mn - t * (mn - mx);
                }

                out[start + i] =
                    (1.0f - sawAmount) * out[start + i] +
                    sawAmount * linear;
            }

            // snap
            size_t snapSamples = size_t(snapTime * float(len));
            snapSamples = std::clamp(snapSamples, size_t(1), len / 3);

            float alpha = 0.25f + sawAmount * 0.35f;
            float beta  = 1.20f + sawAmount * 0.50f;

            for (size_t i = 0; i < snapSamples; ++i) {
                float t = float(i) / float(snapSamples - 1);
                float snapEnv = std::pow(t, alpha) * std::pow(1.0f - t, beta);

                size_t idx = reverseSaw ? (start + i) : (end - 1 - i);

                float snapTarget;
                if (!reverseSaw)
                    snapTarget = (sgn > 0.0f ? mn : mx);
                else
                    snapTarget = (sgn > 0.0f ? mx : mn);

                out[idx] =
                    out[idx] * (1.0f - snapEnv) +
                    snapTarget * snapEnv;
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

            loopBuffer[i] = (x + sharp * (shaped - x)) * compensation;
        }
        process_saw(loopBuffer);
        normalize(loopBuffer, 0.6f);
        if (guiIsCreated) {
            loadLoopNew = true;
            update_waveview(loopview, loopBuffer.data(), loopBuffer.size());
        }
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

    void setOneShootBank(bool custom = false) {
        if (!sampleBuffer.size()) return;
        if (!sampleData) sampleData = std::make_shared<SampleInfo>();
        if (!custom) getPitch();
        //sbank.clear();
        sampleData->data = sampleBuffer;
        sampleData->sourceRate = (double)jack_sr;
        sampleData->rootFreq = custom ? (double) customFreq : (double) freq;
        sbank.addSample(std::const_pointer_cast<const SampleInfo>(sampleData));
        synth.setBank(&sbank);
    }

    void setOneShootToBank(bool custom = false) {
        if (!af.samples) return;
        
        sampleBuffer.clear();
        sampleBuffer.resize(af.samplesize);
        smooth.setSampleRate((float)jack_sr);
        smooth.reset();
        smooth.cutoff = std::clamp(freq * 2.4f, 600.0f, 3000.0f);
        float maxAbs = 0.0f;
        for (size_t i = 0; i < af.samplesize; i++) {
            sampleBuffer[i] = smooth.process(af.samples[i * af.channels] ) * 0.92f;
            float a = std::fabs(sampleBuffer[i]);
            if (a > maxAbs) maxAbs = a;
        }

        float gain = 0.6f/maxAbs;
        for (size_t i = 0; i < af.samplesize; i++) {
            sampleBuffer[i] *= gain;
        }

        sampleBufferSave.clear();
        sampleBufferSave.resize(sampleBuffer.size());
        for (size_t i = 0; i < sampleBuffer.size(); ++i) {
            sampleBufferSave[i] = sampleBuffer[i];
        }

        process_sample_sharp();
        custom ? setOneShootBank(true) : setOneShootBank(false);
    }

    void setLoopBank() {
        if (!loopBuffer.size()) return;
        if (!loopData) loopData = std::make_shared<SampleInfo>();
        loopData->data = loopBufferSave;
        loopData->sourceRate = (double)jack_sr;
        loopData->rootFreq = (double)freq;
        lbank.addSample(std::const_pointer_cast<const SampleInfo>(loopData));
        synth.setLoopBank(&lbank);
        analyseBuffer.clear();
        analyseBuffer.resize(40960);
        synth.getAnalyseBuffer(analyseBuffer.data(), 40960);
        loopFreq = pt.analyseBuffer(analyseBuffer.data(), 40960, jack_sr, loopRootkey);
        double cor = 1.0;
        loopRootkey = rootkey;
        if (loopFreq > 30.0f && loopFreq < 999.0f) {
            cor = loopFreq / 440.0f;
            float midiFloat = 69.0f + 12.0f * std::log2((freq * cor) / 440.0f);
            int midiNote = static_cast<int>(std::floor(midiFloat + 0.5f));
            loopRootkey = std::clamp(midiNote, 0, 127);
        } else {
            loopRootkey = pt.getPitch(analyseBuffer.data(), 40960 , 1, (float)jack_sr, &loopPitchCorrection, &loopFreq);
            if (loopFreq > 30.0f && loopFreq < 999.0f) {
                cor = loopFreq / 440.0f;
                float midiFloat = 69.0f + 12.0f * std::log2((freq * cor) / 440.0f);
                int midiNote = static_cast<int>(std::floor(midiFloat + 0.5f));
                loopRootkey = std::clamp(midiNote, 0, 127);
            } else {
                loopRootkey = rootkey;
            }
        }
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
        play_loop = true;
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
        adj_set_state(loopMark_L->adj_x, 0.0);
        loopPoint_l = 0;
        adj_set_state(loopMark_R->adj_x,1.0);
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
        if (af.samples) {
            std::filesystem::path p = file;
            if (guiIsCreated) {
                adj_set_max_value(wview->adj, (float)af.samplesize);
                adj_set_state(loopMark_L->adj_x, 0.0);
                adj_set_state(loopMark_R->adj_x,1.0);
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
        int new_size = static_cast<int>(4.0 * jack_sr);
        delete[] af.samples;
        af.samples = nullptr;
        af.samples =  new float[new_size];
        af.samplesize = new_size;
        af.samplerate = jack_sr;
        af.channels = 1;
        std::memset(af.samples, 0, new_size * sizeof(float));
        const float duration = new_size / jack_sr / 2;
        const float f = 440.0f;
        for (int i = 0; i < new_size; ++i) {
            float t = (float)i / jack_sr;
            float s = 1.00f * sinf(2.0f * M_PI * f * t) +
                0.03f * sinf(2.0f * M_PI * 2.0f * f * t) +
                0.01f * sinf(2.0f * M_PI * 3.0f * f * t);
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
            adj_set_state(loopMark_L->adj_x, 0.0);
            adj_set_state(loopMark_R->adj_x,1.0);
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
        if (guiIsCreated) {
            loadNew = true;
            update_waveview(wview, af.samples, af.samplesize);
        }
    }

    void set_record() {
        timer = 30;
        position = 0;
        if (guiIsCreated) {
            adj_set_max_value(wview->adj, (float)af.samplesize);
            adj_set_state(loopMark_L->adj_x, 0.0);
            adj_set_state(loopMark_R->adj_x,1.0);
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
            int loadNew = -1;
            if (loadPresetMIDI > lastPresetMIDI) {
                loadNew = currentPresetNum + 1;
            } else if (loadPresetMIDI < lastPresetMIDI) {
                loadNew = currentPresetNum - 1;
            }
            if (loadNew > (int)presetFiles.size() - 1) {
                loadNew = 0;
            } else if (loadNew < 0) {
                loadNew = presetFiles.size() - 1;
            }
            currentPresetNum = loadNew;
            lastPresetMIDI = loadPresetMIDI;
            std::string name = presetFiles[currentPresetNum];
            std::string path = getPathFor(name);
            loadPreset(path);
            loadPresetMIDI = -1;
        }
        wview->func.adj_callback = dummy_callback;
        playbutton->func.adj_callback = dummy_callback;
        Volume->func.adj_callback = dummy_callback;
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
        sz.updateTweens(1.0f / 60.0f);
        if (synth.rb.getKeyCacheState())
            expose_widget(Controls);
        #ifndef RUN_AS_PLUGIN
        if (!record && timer == 0) {
            set_record();
            adj_set_value(Record->adj, 0.0);
            expose_widget(Record);
        }
        #endif
        adj_set_value(Volume->adj, volume);
        markDirty(5);
        gain = std::pow(1e+01, 0.05 * volume);

        wheel_idle_callback(PitchWheel, nullptr);
        expose_widget(keyboard);
        expose_widget(wview);
        expose_widget(Volume);
        #ifndef RUN_AS_PLUGIN
        if(xruns) expose_widget(Controls);
        #endif

        #if defined(__linux__) || defined(__FreeBSD__) || \
            defined(__NetBSD__) || defined(__OpenBSD__)
        XFlush(w->app->dpy);
        XUnlockDisplay(w->app->dpy);
        #endif
        wview->func.adj_callback = transparent_draw;
        playbutton->func.adj_callback = transparent_draw;
        Volume->func.adj_callback = transparent_draw;
    }

/****************************************************************
        create controllers
****************************************************************/

    void addSampleBufferControlls(Widget_t *frame) {
        filebutton = add_file_button(frame, 10, 25, 35, 35, getenv("HOME") ? getenv("HOME") : PATH_SEPARATOR, "audio");
        filebutton->scale.gravity = ASPECT;
        widget_get_png(filebutton, LDVAR(load__png));
        filebutton->flags |= HAS_TOOLTIP;
        add_tooltip(filebutton, "Load audio file");
        filebutton->func.user_callback = dialog_response;
        commonWidgetSettings(filebutton);

        Presets = add_button(frame, "", 45, 25, 35, 35);
        Presets->scale.gravity = ASPECT;
        widget_get_png(Presets, LDVAR(presets_png));
        Presets->flags |= HAS_TOOLTIP;
        add_tooltip(Presets, "Load/Save Presets");
        Presets->func.value_changed_callback = presets_callback;
        commonWidgetSettings(Presets);

        Reverse = add_image_toggle_button(frame, "", 80, 25, 35, 35);
        widget_get_png(Reverse, LDVAR(reverse_png));
        Reverse->scale.gravity = ASPECT;
        Reverse->flags |= HAS_TOOLTIP;
        add_tooltip(Reverse, "Reverse Sample");
        Reverse->func.value_changed_callback = reverse_callback;
        commonWidgetSettings(Reverse);

        FadeOut = add_knob(frame, "FadeOut",122,23,38,38);
        FadeOut->scale.gravity = ASPECT;
        FadeOut->flags |= HAS_TOOLTIP;
        add_tooltip(FadeOut, "Fade Out Samplebuffer");
        set_adjustment(FadeOut->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(FadeOut, (Color_state)1, (Color_mod)2, 0.15, 0.52, 0.55, 1.0);
        FadeOut->func.expose_callback = draw_knob;
        FadeOut->func.value_changed_callback = fade_callback;
        commonWidgetSettings(FadeOut);

        clip = add_button(frame, "", 170, 25, 35, 35);
        clip->scale.gravity = ASPECT;
        widget_get_png(clip, LDVAR(clip__png));
        clip->flags |= HAS_TOOLTIP;
        add_tooltip(clip, "Clip Sample to clip marks");
        clip->func.value_changed_callback = button_clip_callback;
        commonWidgetSettings(clip);

        playbutton = add_image_toggle_button(frame, "", 205, 25, 35, 35);
        playbutton->scale.gravity = ASPECT;
        widget_get_png(playbutton, LDVAR(play_png));
        playbutton->flags |= HAS_TOOLTIP;
        add_tooltip(playbutton, "Play Sample");
        playbutton->func.value_changed_callback = button_playbutton_callback;
        commonWidgetSettings(playbutton);

        #ifndef RUN_AS_PLUGIN
        Record = add_image_toggle_button(frame, "", 240, 25, 35, 35);
        Record->scale.gravity = ASPECT;
        widget_get_png(Record, LDVAR(record_png));
        Record->flags |= HAS_TOOLTIP;
        add_tooltip(Record, "Record Sample");
        Record->func.value_changed_callback = button_record_callback;
        commonWidgetSettings(Record);
        #endif        
    }

    void addPhaseModulatorControlls(Widget_t *frame) {
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

        PmDepth = add_knob(frame, "Depth",85,25,38,38);
        set_adjustment(PmDepth->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(PmDepth, (Color_state)1, (Color_mod)2, 0.55, 0.95, 0.80, 1.0);
        commonWidgetSettings(PmDepth);
        connectValueChanged(PmDepth, &Loopino::pmdepth, 14, "PM Depth", draw_knob,
            [](Loopino* self, float v) {self->synth.setPmDepth(v);});

        PmFreq = add_knob(frame, "Freq",125,25,38,38);
        set_adjustment(PmFreq->adj, 0.01, 0.01, 0.01, 30.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(PmFreq, (Color_state)1, (Color_mod)2, 0.60, 0.80, 1.00, 1.0);
        commonWidgetSettings(PmFreq);
        connectValueChanged(PmFreq, &Loopino::pmfreq, 13, "PM Freq", draw_knob,
            [](Loopino* self, float v) {self->synth.setPmFreq(v);});
    }

    void addLoopBufferControlls(Widget_t *frame) {
        setLoop = add_image_toggle_button(frame, "", 10, 25, 35, 35);
        setLoop->scale.gravity = ASPECT;
        widget_get_png(setLoop, LDVAR(loop_png));
        setLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setLoop, "Use Loop Sample");
        setLoop->func.value_changed_callback = button_set_callback;
        commonWidgetSettings(setLoop);

        setLoopSize = add_knob(frame, "S",48,23,38,38);
        setLoopSize->scale.gravity = ASPECT;
        setLoopSize->flags |= HAS_TOOLTIP;
        add_tooltip(setLoopSize, "Loop Periods");
        set_adjustment(setLoopSize->adj, 1.0, 1.0, 1.0, 512.0, 1.0, CL_CONTINUOS);
        setLoopSize->func.expose_callback = draw_knob;
        setLoopSize->func.button_press_callback = setLoopSize_indrag;
        setLoopSize->func.button_release_callback = setLoopSize_released;
        setLoopSize->func.value_changed_callback = setLoopSize_callback;
        commonWidgetSettings(setLoopSize);

        setPrevLoop = add_button(frame, "", 90, 25, 35, 35);
        setPrevLoop->scale.gravity = ASPECT;
        widget_get_png(setPrevLoop, LDVAR(prev_png));
        setPrevLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setPrevLoop, "Load previous loop");
        setPrevLoop->func.value_changed_callback = setPrevLoop_callback;
        commonWidgetSettings(setPrevLoop);

        setNextLoop = add_button(frame, "", 125, 25, 35, 35);
        setNextLoop->scale.gravity = ASPECT;
        widget_get_png(setNextLoop, LDVAR(next_png));
        setNextLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setNextLoop, "Load next loop");
        setNextLoop->func.value_changed_callback = setNextLoop_callback;
        commonWidgetSettings(setNextLoop);
    }

    void addSharpControlls(Widget_t *frame) {
        Sharp = add_knob(frame, "Square",10,25,38,38);
        Sharp->scale.gravity = ASPECT;
        Sharp->flags |= HAS_TOOLTIP;
        Sharp->data = 1;
        add_tooltip(Sharp, "Square");
        set_adjustment(Sharp->adj, 0.0, 0.0, -1.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Sharp, (Color_state)1, (Color_mod)2, 0.55, 0.42, 0.15, 1.0);
        Sharp->func.expose_callback = draw_knob;
        Sharp->func.button_release_callback = sharp_released;
        Sharp->func.value_changed_callback = sharp_callback;
        commonWidgetSettings(Sharp);

        Saw = add_knob(frame, "Saw",50,25,38,38);
        Saw->scale.gravity = ASPECT;
        Saw->flags |= HAS_TOOLTIP;
        Saw->data = 1;
        add_tooltip(Saw, "Saw Tooth");
        set_adjustment(Saw->adj, 0.0, 0.0, -1.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Saw, (Color_state)1, (Color_mod)2, 0.55, 0.52, 0.15, 1.0);
        Saw->func.expose_callback = draw_knob;
        Saw->func.button_release_callback = sharp_released;
        Saw->func.value_changed_callback = saw_callback;
        commonWidgetSettings(Saw);
    }

    void addToneControlls(Widget_t *frame) {
        Tone = add_knob(frame, "Tone",14,25,38,38);
        set_adjustment(Tone->adj, 0.0, 0.0, -1.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Tone, (Color_state)1, (Color_mod)2, 0.38, 0.62, 0.94, 1.0);
        Tone->data = 1;
        commonWidgetSettings(Tone);
        connectValueChanged(Tone, &Loopino::tone, 0, "Tone", draw_knob,
            [](Loopino* self, float v) {self->synth.setTone(v);});
    }

    void addGainControlls(Widget_t *frame) {
        Volume = add_knob(frame, "dB",14,25,38,38);
        Volume->scale.gravity = ASPECT;
        Volume->flags |= HAS_TOOLTIP;
        add_tooltip(Volume, "Volume (dB)");
        set_adjustment(Volume->adj, 0.0, 0.0, -20.0, 12.0, 0.01, CL_LOGSCALE);
        set_widget_color(Volume, (Color_state)1, (Color_mod)2, 0.38, 0.62, 0.94, 1.0);
        Volume->func.expose_callback = draw_knob;
        Volume->func.value_changed_callback = volume_callback;
        commonWidgetSettings(Volume);
    }

    void addExitControlls(Widget_t *frame) {
        w_quit = add_button(frame, "", 16, 25, 35, 35);
        widget_get_png(w_quit, LDVAR(exit__png));
        w_quit->scale.gravity = ASPECT;
        w_quit->flags |= HAS_TOOLTIP;
        add_tooltip(w_quit, "Exit");
        w_quit->func.value_changed_callback = button_quit_callback;
        commonWidgetSettings(w_quit);
    }

    void addADSRControlls(Widget_t *frame) {
        Attack = add_knob(frame, "Attack",10,25,38,38);
        set_adjustment(Attack->adj, 0.01, 0.01, 0.001, 5.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Attack, (Color_state)1, (Color_mod)2, 0.894, 0.106, 0.623, 1.0);
        commonWidgetSettings(Attack);
        connectValueChanged(Attack, &Loopino::attack, 0, "Attack", draw_knob,
            [](Loopino* self, float v) {self->setAttack(v);});

        Decay = add_knob(frame, "Decay",50,25,38,38);
        set_adjustment(Decay->adj, 0.1, 0.1, 0.005, 5.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Decay, (Color_state)1, (Color_mod)2, 0.902, 0.098, 0.117, 1.0);
        commonWidgetSettings(Decay);
        connectValueChanged(Decay, &Loopino::decay, 1, "Decay", draw_knob,
            [](Loopino* self, float v) {self->setDecay(v);});

        Sustain = add_knob(frame, "Sustain",90,25,38,38);
        set_adjustment(Sustain->adj, 0.8, 0.8, 0.001, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Sustain, (Color_state)1, (Color_mod)2, 0.377, 0.898, 0.109, 1.0);
        commonWidgetSettings(Sustain);
        connectValueChanged(Sustain, &Loopino::sustain, 2, "Sustain", draw_knob,
            [](Loopino* self, float v) {self->setSustain(v);});

        Release = add_knob(frame, "Release",130,25,38,38);
        set_adjustment(Release->adj, 0.3, 0.3, 0.005, 10.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Release, (Color_state)1, (Color_mod)2, 0.486, 0.106, 0.894, 1.0);
        commonWidgetSettings(Release);
        connectValueChanged(Release, &Loopino::release, 3, "Release", draw_knob,
            [](Loopino* self, float v) {self->setRelease(v);});
    }

    void addEnvelopeControlls(Widget_t *frame) {
        Envelope = add_adsr_widget(frame,10, 15, 158, 52, 
                    Attack->adj, Decay->adj, Sustain->adj, Release->adj);
        Envelope->parent = w_top;
        Envelope->scale.gravity = ASPECT;
        commonWidgetSettings(Envelope);
    }

    void addDynamicControlls(Widget_t *frame) {
        VelMode[0] = add_check_box(frame,"Soft" , 12, 20, 15, 15);
        VelMode[0]->flags |= IS_RADIO;
        set_widget_color(VelMode[0], (Color_state)0, (Color_mod)3, 0.55, 0.65, 0.55, 1.0);
        add_tooltip(VelMode[0], "Velocity Dynamic Curve Soft");
        commonWidgetSettings(VelMode[0]);
        VelMode[0]->func.value_changed_callback = radio_box_velocity_pressed;

        VelMode[1] = add_check_box(frame,"Piano" , 12, 37, 15, 15);
        VelMode[1]->flags |= IS_RADIO;
        set_widget_color(VelMode[1], (Color_state)0, (Color_mod)3, 0.55, 0.65, 0.55, 1.0);
        add_tooltip(VelMode[1], "Velocity Dynamic Curve Piano");
        commonWidgetSettings(VelMode[1]);
        VelMode[1]->func.value_changed_callback = radio_box_velocity_pressed;

        VelMode[2] = add_check_box(frame,"Punch" , 12, 54, 15, 15);
        VelMode[2]->flags |= IS_RADIO;
        add_tooltip(VelMode[2], "Velocity Dynamic Curve Punch");
        commonWidgetSettings(VelMode[2]);
        set_widget_color(VelMode[2], (Color_state)0, (Color_mod)3, 0.55, 0.65, 0.55, 1.0);
        VelMode[2]->func.value_changed_callback = radio_box_velocity_pressed;
        velocity_box_set_active(VelMode[velmode]);
    }

    void addWaspControlls(Widget_t * frame) {

        WaspOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(WaspOnOff);
        connectValueChanged(WaspOnOff, &Loopino::wasponoff, 41, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setOnOffWasp(v);});

        WaspMix = add_knob(frame, "WaspMix",40,25,38,38);
        set_adjustment(WaspMix->adj, 0.0, 0.0, -1.0, 1.0, 0.01, CL_CONTINUOS);
        WaspMix->data = 1;
        set_widget_color(WaspMix, (Color_state)1, (Color_mod)2, 0.55, 0.42, 0.55, 1.0);
        commonWidgetSettings(WaspMix);
        connectValueChanged(WaspMix, &Loopino::waspmix, 42, "Mix", draw_knob,
            [](Loopino* self, float v) {self->synth.setFilterMixWasp(v);});

        WaspResonance = add_knob(frame, "WaspResonance",80,25,38,38);
        set_adjustment(WaspResonance->adj, 0.4, 0.4, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(WaspResonance, (Color_state)1, (Color_mod)2, 0.95, 0.42, 0.15, 1.0);
        commonWidgetSettings(WaspResonance);
        connectValueChanged(WaspResonance, &Loopino::waspresonance, 43, "Resonance", draw_knob,
            [](Loopino* self, float v) {self->synth.setResonanceWasp(v);});

        WaspCutOff = add_knob(frame, "WaspCutOff",120,25,38,38);
        set_adjustment(WaspCutOff->adj, 1000.0, 1000.0, 40.0, 12000.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(WaspCutOff, (Color_state)1, (Color_mod)2, 0.20, 0.60, 0.95, 1.0);
        commonWidgetSettings(WaspCutOff);
        connectValueChanged(WaspCutOff, &Loopino::waspcutoff, 44, "CutOff", draw_knob,
            [](Loopino* self, float v) {self->synth.setCutoffWasp(v);});

        WaspKeyTracking = add_wheel(frame, "", 162, 15, 12, 55);
        WaspKeyTracking->scale.gravity = ASPECT;
        WaspKeyTracking->flags |= HAS_TOOLTIP;
        Wheel *wheelw = (Wheel*)WaspKeyTracking->private_struct;
        wheelw->value = (waspkeytracking * 2.0f) - 1.0f;
        add_tooltip(WaspKeyTracking, "Key-tracking");
        commonWidgetSettings(WaspKeyTracking);
        WaspKeyTracking->func.value_changed_callback = waspkeytracking_callback;
    }

    void addLPLadderControlls(Widget_t *frame) {
        LpOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(LpOnOff);
        connectValueChanged(LpOnOff, &Loopino::lponoff, 28, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setOnOffLP(v);});

        Resonance = add_knob(frame, "Resonance",40,25,38,38);
        set_adjustment(Resonance->adj, 68.0, 68.0, 0.0, 127.0, 1.0, CL_CONTINUOS);
        set_widget_color(Resonance, (Color_state)1, (Color_mod)2, 0.95, 0.42, 0.15, 1.0);
        commonWidgetSettings(Resonance);
        connectValueChanged(Resonance, &Loopino::resonance, 8, "Resonance", draw_knob,
            [](Loopino* self, float v) {self->synth.setResoLP(v);});

        CutOff = add_knob(frame, "CutOff",80,25,38,38);
        set_adjustment(CutOff->adj, 68.0, 68.0, 0.0, 127.0, 1.0, CL_CONTINUOS);
        set_widget_color(CutOff, (Color_state)1, (Color_mod)2, 0.20, 0.60, 0.95, 1.0);
        commonWidgetSettings(CutOff);
        connectValueChanged(CutOff, &Loopino::cutoff, 9, "CutOff", draw_knob,
            [](Loopino* self, float v) {self->synth.setCutoffLP(v);});

        LpKeyTracking = add_wheel(frame, "", 125, 15, 12, 55);
        LpKeyTracking->parent_struct = (void*)this;
        LpKeyTracking->flags |= HAS_TOOLTIP;
        Wheel *wheellp = (Wheel*)LpKeyTracking->private_struct;
        wheellp->value = 1.0;
        add_tooltip(LpKeyTracking, "Key-tracking");
        LpKeyTracking->func.value_changed_callback = lpkeytracking_callback;
        commonWidgetSettings(LpKeyTracking);
    }

    void addHPLadderControlls(Widget_t *frame) {
        HpOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(HpOnOff);
        connectValueChanged(HpOnOff, &Loopino::hponoff, 29, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setOnOffHP(v);});

        HpResonance = add_knob(frame, "HpResonance",40,25,38,38);
        set_adjustment(HpResonance->adj, 50.0, 50.0, 0.0, 127.0, 1.0, CL_CONTINUOS);
        set_widget_color(HpResonance, (Color_state)1, (Color_mod)2, 0.95, 0.42, 0.15, 1.0);
        commonWidgetSettings(HpResonance);
        connectValueChanged(HpResonance, &Loopino::hpresonance, 15, "Resonance", draw_knob,
            [](Loopino* self, float v) {self->synth.setResoHP(v);});

        HpCutOff = add_knob(frame, "HpCutOff",80,25,38,38);
        set_adjustment(HpCutOff->adj, 48.0, 48.0, 0.0, 127.0, 1.0, CL_CONTINUOS);
        set_widget_color(HpCutOff, (Color_state)1, (Color_mod)2, 0.20, 0.60, 0.95, 1.0);
        commonWidgetSettings(HpCutOff);
        connectValueChanged(HpCutOff, &Loopino::hpcutoff, 16, "CutOff", draw_knob,
            [](Loopino* self, float v) {self->synth.setCutoffHP(v);});

        HpKeyTracking = add_wheel(frame, "", 125, 15, 12, 55);
        HpKeyTracking->parent_struct = (void*)this;
        HpKeyTracking->scale.gravity = ASPECT;
        HpKeyTracking->flags |= HAS_TOOLTIP;
        Wheel *wheelhp = (Wheel*)HpKeyTracking->private_struct;
        wheelhp->value = 1.0;
        add_tooltip(HpKeyTracking, "Key-tracking");
        HpKeyTracking->func.value_changed_callback = hpkeytracking_callback;
        commonWidgetSettings(HpKeyTracking);
    }

    void addSEM12Controlls(Widget_t *frame) {
        ObfOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(ObfOnOff);
        connectValueChanged(ObfOnOff, &Loopino::obfonoff, 27, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setOnOffObf(v);});

        ObfMode = add_knob(frame, "ObfMode",40,25,38,38);
        set_adjustment(ObfMode->adj, -0.6, -0.6, -1.0, 1.0, 0.01, CL_CONTINUOS);
        ObfMode->data = 1;
        set_widget_color(ObfMode, (Color_state)1, (Color_mod)2, 0.55, 0.42, 0.55, 1.0);
        commonWidgetSettings(ObfMode);
        connectValueChanged(ObfMode, &Loopino::obfmode, 23, "Mode LP <-> BP <-> HP", draw_knob,
            [](Loopino* self, float v) {self->synth.setModeObf(v);});

        ObfResonance = add_knob(frame, "ObfResonance",80,25,38,38);
        set_adjustment(ObfResonance->adj, 0.3, 0.3, 0.0, 0.6, 0.01, CL_CONTINUOS);
        set_widget_color(ObfResonance, (Color_state)1, (Color_mod)2, 0.95, 0.42, 0.15, 1.0);
        commonWidgetSettings(ObfResonance);
        connectValueChanged(ObfResonance, &Loopino::obfresonance, 25, "Resonance", draw_knob,
            [](Loopino* self, float v) {self->synth.setResonanceObf(v);});

        ObfCutOff = add_knob(frame, "ObfCutOff",120,25,38,38);
        set_adjustment(ObfCutOff->adj, 200.0, 200.0, 40.0, 12000.0, 0.1, CL_LOGARITHMIC);
        set_widget_color(ObfCutOff, (Color_state)1, (Color_mod)2, 0.20, 0.60, 0.95, 1.0);
        commonWidgetSettings(ObfCutOff);
        connectValueChanged(ObfCutOff, &Loopino::obfcutoff, 26, "CutOff", draw_knob,
            [](Loopino* self, float v) {self->synth.setCutoffObf(v);});

        ObfKeyTracking = add_wheel(frame, "", 162, 15, 12, 55);
        ObfKeyTracking->scale.gravity = ASPECT;
        ObfKeyTracking->flags |= HAS_TOOLTIP;
        Wheel *wheel = (Wheel*)ObfKeyTracking->private_struct;
        wheel->value = 0.0;
        add_tooltip(ObfKeyTracking, "Key-tracking");
        commonWidgetSettings(ObfKeyTracking);
        ObfKeyTracking->func.value_changed_callback = obfkeytracking_callback;
    }

    void addFreqControlls(Widget_t *frame) {
        Frequency = add_valuedisplay(frame, _(" Hz"), 10, 15, 66, 25);
        set_adjustment(Frequency->adj, 440.0, 440.0, 220.0, 880.0, 0.1, CL_CONTINUOS);
        commonWidgetSettings(Frequency);
        connectValueChanged(Frequency, &Loopino::frequency, 4, "Synth Root Frequency", nullptr,
            [](Loopino* self, float v) {self->synth.setRootFreq(v);});

        RootKey = add_combobox(frame, "", 10, 40, 66, 25);
        commonWidgetSettings(RootKey);
        for (auto & element : keys) {
            combobox_add_entry(RootKey, element.c_str());
        }
        RootKey->func.expose_callback = draw_combobox;
        RootKey->childlist->childs[0]->func.expose_callback = draw_combo_button;
        RootKey->func.value_changed_callback = set_custom_root_key;
        add_tooltip(RootKey, "Set Sample Root Key ");
        combobox_set_menu_size(RootKey, 12);
        combobox_set_active_entry(RootKey, rootkey);


    }

    void addAcidControlls(Widget_t *frame) {
        TBOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(TBOnOff);
        connectValueChanged(TBOnOff, &Loopino::tbonoff, 46, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setTBOnOff(v);});

        TBVintage = add_knob(frame, "Vintage",40,25,38,38);
        set_adjustment(TBVintage->adj, 0.3, 0.3, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(TBVintage, (Color_state)1, (Color_mod)2, 0.00, 0.78, 1.00, 1.0);
        commonWidgetSettings(TBVintage);
        connectValueChanged(TBVintage, &Loopino::tbvintage, 47, "Vintage", draw_knob,
            [](Loopino* self, float v) {self->synth.setVintageAmountTB(v);});

        TBResonance = add_knob(frame, "Resonance",80,25,38,38);
        set_adjustment(TBResonance->adj, 0.3, 0.3, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(TBResonance, (Color_state)1, (Color_mod)2, 0.95, 0.42, 0.15, 1.0);
        commonWidgetSettings(TBResonance);
        connectValueChanged(TBResonance, &Loopino::tbresonance, 48, "Resonance", draw_knob,
            [](Loopino* self, float v) {self->synth.setResonanceTB(v);});

        TBCutOff = add_knob(frame, "CutOff",120,25,38,38);
        set_adjustment(TBCutOff->adj, 880.0, 880.0, 40.0, 12000.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(TBCutOff, (Color_state)1, (Color_mod)2, 0.20, 0.60, 0.95, 1.0);
        commonWidgetSettings(TBCutOff);
        connectValueChanged(TBCutOff, &Loopino::tbcutoff, 49, "CutOff", draw_knob,
            [](Loopino* self, float v) {self->synth.setCutoffTB(v);});
    }

    void addVibratoControlls(Widget_t *frame) {
        VibOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(VibOnOff);
        connectValueChanged(VibOnOff, &Loopino::vibonoff, 30, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setOnOffVib(v);});

        VibDepth = add_knob(frame, "VibDepth",40,25,38,38);
        set_adjustment(VibDepth->adj, 0.6, 0.6, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(VibDepth, (Color_state)1, (Color_mod)2, 0.00, 0.78, 1.00, 1.0);
        commonWidgetSettings(VibDepth);
        connectValueChanged(VibDepth, &Loopino::vibdepth, 15, "Depth", draw_knob,
            [](Loopino* self, float v) {self->synth.setvibDepth(v);});

        VibRate = add_knob(frame, "VibRate",80,25,38,38);
        set_adjustment(VibRate->adj, 5.0, 5.0, 0.1, 12.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(VibRate, (Color_state)1, (Color_mod)2, 0.00, 1.00, 0.78, 1.0);
        commonWidgetSettings(VibRate);
        connectValueChanged(VibRate, &Loopino::vibrate, 16, "Rate", draw_knob,
            [](Loopino* self, float v) {self->synth.setvibRate(v);});
    }

    void addTremoloControlls(Widget_t *frame) {
        TremOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(TremOnOff);
        connectValueChanged(TremOnOff, &Loopino::tremonoff, 31, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setOnOffTrem(v);});

        TremDepth = add_knob(frame, "TremDepth",40,25,38,38);
        set_adjustment(TremDepth->adj, 0.3, 0.3, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(TremDepth, (Color_state)1, (Color_mod)2, 1.00, 0.67, 0.47, 1.0);
        commonWidgetSettings(TremDepth);
        connectValueChanged(TremDepth, &Loopino::tremdepth, 17, "Depth", draw_knob,
            [](Loopino* self, float v) {self->synth.settremDepth(v);});

        TremRate = add_knob(frame, "TremRate",80,25,38,38);
        set_adjustment(TremRate->adj, 5.0, 5.0, 0.1, 15.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(TremRate, (Color_state)1, (Color_mod)2, 1.00, 0.78, 0.59, 1.0);
        commonWidgetSettings(TremRate);
        connectValueChanged(TremRate, &Loopino::tremrate, 18, "Rate", draw_knob,
            [](Loopino* self, float v) {self->synth.settremRate(v);});
    }

    void addChorusControlls(Widget_t *frame) {
        ChorusOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(ChorusOnOff);
        connectValueChanged(ChorusOnOff, &Loopino::chorusonoff, 32, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setChorusOnOff(v);});

        ChorusLev = add_knob(frame, "ChorusLev",40,25,38,38);
        set_adjustment(ChorusLev->adj, 0.5, 0.5, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(ChorusLev, (Color_state)1, (Color_mod)2, 0.59, 0.78, 1.0, 1.0);
        commonWidgetSettings(ChorusLev);
        connectValueChanged(ChorusLev, &Loopino::choruslev, 33, "Level", draw_knob,
            [](Loopino* self, float v) {self->synth.setChorusLevel(v);});

        ChorusDelay = add_knob(frame, "ChorusDelay",80,25,38,38);
        set_adjustment(ChorusDelay->adj, 0.02, 0.02, 0.0, 0.2, 0.001, CL_CONTINUOS);
        set_widget_color(ChorusDelay, (Color_state)1, (Color_mod)2, 0.44, 0.78, 0.59, 1.0);
        commonWidgetSettings(ChorusDelay);
        connectValueChanged(ChorusDelay, &Loopino::chorusdelay, 34, "Delay", draw_knob,
            [](Loopino* self, float v) {self->synth.setChorusDelay(v);});

        ChorusDepth = add_knob(frame, "ChorusDepth",120,25,38,38);
        set_adjustment(ChorusDepth->adj, 0.02, 0.02, 0.0, 1.0, 0.001, CL_CONTINUOS);
        set_widget_color(ChorusDepth, (Color_state)1, (Color_mod)2, 0.66, 0.33, 0.33, 1.0);
        commonWidgetSettings(ChorusDepth);
        connectValueChanged(ChorusDepth, &Loopino::chorusdepth, 35, "Depth", draw_knob,
            [](Loopino* self, float v) {self->synth.setChorusDepth(v);});

        ChorusFreq = add_knob(frame, "ChorusFreq",160,25,38,38);
        set_adjustment(ChorusFreq->adj, 3.0, 3.0, 0.1, 10.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(ChorusFreq, (Color_state)1, (Color_mod)2, 0.1, 0.67, 0.47, 1.0);
        commonWidgetSettings(ChorusFreq);
        connectValueChanged(ChorusFreq, &Loopino::chorusfreq, 36, "Frequency", draw_knob,
            [](Loopino* self, float v) {self->synth.setChorusFreq(v);});
    }

    void addReverbControlls(Widget_t *frame) {
        RevOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(RevOnOff);
        connectValueChanged(RevOnOff, &Loopino::revonoff, 37, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setReverbOnOff(v);});

        RevRoomSize = add_knob(frame, "RevRoomSize",40,25,38,38);
        set_adjustment(RevRoomSize->adj, 0.0, 0.0, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(RevRoomSize, (Color_state)1, (Color_mod)2, 0.59, 0.78, 1.0, 1.0);
        commonWidgetSettings(RevRoomSize);
        connectValueChanged(RevRoomSize, &Loopino::revroomsize, 38, "Room Size", draw_knob,
            [](Loopino* self, float v) {self->synth.setReverbRoomSize(v);});

        RevDamp = add_knob(frame, "RevDamp",80,25,38,38);
        set_adjustment(RevDamp->adj, 0.25, 0.25, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(RevDamp, (Color_state)1, (Color_mod)2, 0.44, 0.78, 0.59, 1.0);
        commonWidgetSettings(RevDamp);
        connectValueChanged(RevDamp, &Loopino::revdamp, 39, "Damp", draw_knob,
            [](Loopino* self, float v) {self->synth.setReverbDamp(v);});

        RevMix = add_knob(frame, "RevMix",120,25,38,38);
        set_adjustment(RevMix->adj, 50.0, 50.0, 0.0, 100.0, 1.0, CL_CONTINUOS);
        set_widget_color(RevMix, (Color_state)1, (Color_mod)2, 0.66, 0.33, 0.33, 1.0);
        commonWidgetSettings(RevMix);
        connectValueChanged(RevMix, &Loopino::revmix, 40, "Mix", draw_knob,
            [](Loopino* self, float v) {self->synth.setReverbMix(v);});
    }

    void add8bitControlls(Widget_t *frame) {
        LM_MIR8OnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(LM_MIR8OnOff);
        connectValueChanged(LM_MIR8OnOff, &Loopino::mrgonoff, 50, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setLM_MIR8OnOff(v);});

        LM_MIR8Drive = add_knob(frame, "LM_MIR8 Drive",40,25,38,38);
        set_adjustment(LM_MIR8Drive->adj, 1.3, 1.3, 0.25, 1.5, 0.01, CL_CONTINUOS);
        set_widget_color(LM_MIR8Drive, (Color_state)1, (Color_mod)2, 0.32f, 0.62f, 0.78f, 1.0f);
        commonWidgetSettings(LM_MIR8Drive);
        LM_MIR8Drive->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.mrg.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(LM_MIR8Drive, &Loopino::mrgdrive, 51, "Drive", draw_knob,
            [](Loopino* self, float v) {self->synth.setLM_MIR8Drive(v);});

        LM_MIR8Amount = add_knob(frame, "LM_MIR8 Amount",80,25,38,38);
        set_adjustment(LM_MIR8Amount->adj, 0.25, 0.25, 0.1, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(LM_MIR8Amount, (Color_state)1, (Color_mod)2, 0.48f, 0.78f, 0.46f, 1.0f);
        commonWidgetSettings(LM_MIR8Amount);
        LM_MIR8Amount->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.mrg.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(LM_MIR8Amount, &Loopino::mrgamount, 52, "Amount", draw_knob,
            [](Loopino* self, float v) {self->synth.setLM_MIR8Amount(v);});
    }

    void add12bitControlls(Widget_t *frame) {
        Emu_12OnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(Emu_12OnOff);
        connectValueChanged(Emu_12OnOff, &Loopino::emu_12onoff, 53, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setEmu_12OnOff(v);});

        Emu_12Drive = add_knob(frame, "Emu_12 Drive",40,25,38,38);
        set_adjustment(Emu_12Drive->adj, 1.2, 1.2, 0.25, 2.5, 0.01, CL_CONTINUOS);
        set_widget_color(Emu_12Drive, (Color_state)1, (Color_mod)2, 0.32f, 0.62f, 0.78f, 1.0f);
        commonWidgetSettings(Emu_12Drive);
        Emu_12Drive->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.emu_12.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(Emu_12Drive, &Loopino::emu_12drive, 54, "Drive", draw_knob,
            [](Loopino* self, float v) {self->synth.setEmu_12Drive(v);});

        Emu_12Amount = add_knob(frame, "Emu_12 Amount",80,25,38,38);
        set_adjustment(Emu_12Amount->adj, 1.0, 1.0, 0.1, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Emu_12Amount, (Color_state)1, (Color_mod)2, 0.48f, 0.78f, 0.46f, 1.0f);
        commonWidgetSettings(Emu_12Amount);
        Emu_12Amount->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.emu_12.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(Emu_12Amount, &Loopino::emu_12amount, 55, "Amount", draw_knob,
            [](Loopino* self, float v) {self->synth.setEmu_12Amount(v);});
    }

    void addPumpControlls(Widget_t *frame) {
        LM_CMP12OnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(LM_CMP12OnOff);
        connectValueChanged(LM_CMP12OnOff, &Loopino::cmp12onoff, 56, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setLM_CMP12OnOff(v);});

        LM_CMP12Drive = add_knob(frame, "LM_CMP12 Drive",40,25,38,38);
        set_adjustment(LM_CMP12Drive->adj, 1.0, 1.0, 0.25, 2.5, 0.01, CL_CONTINUOS);
        set_widget_color(LM_CMP12Drive, (Color_state)1, (Color_mod)2, 0.32f, 0.62f, 0.78f, 1.0f);
        commonWidgetSettings(LM_CMP12Drive);
        LM_CMP12Drive->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.cmp12dac.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(LM_CMP12Drive, &Loopino::cmp12drive, 57, "Drive", draw_knob,
            [](Loopino* self, float v) {self->synth.setLM_CMP12Drive(v);});

        LM_CMP12Ratio = add_knob(frame, "LM_CMP12 Ratio",80,25,38,38);
        set_adjustment(LM_CMP12Ratio->adj, 1.65, 1.65, 0.1, 4.0, 0.01, CL_CONTINUOS);
        set_widget_color(LM_CMP12Ratio, (Color_state)1, (Color_mod)2, 0.48f, 0.78f, 0.46f, 1.0f);
        commonWidgetSettings(LM_CMP12Ratio);
        LM_CMP12Ratio->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.cmp12dac.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(LM_CMP12Ratio, &Loopino::cmp12ratio, 58, "Ratio", draw_knob,
            [](Loopino* self, float v) {self->synth.setLM_CMP12Ratio(v);});
    }

    void addStudio16Controllse(Widget_t *frame) {
        Studio_16OnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(Studio_16OnOff);
        connectValueChanged(Studio_16OnOff, &Loopino::studio16onoff, 59, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setStudio_16OnOff(v);});

        Studio_16Drive = add_knob(frame, "Studio_16 Drive",40,25,38,38);
        set_adjustment(Studio_16Drive->adj, 1.1, 1.1, 0.25, 1.5, 0.01, CL_CONTINUOS);
        set_widget_color(Studio_16Drive, (Color_state)1, (Color_mod)2, 0.32f, 0.62f, 0.78f, 1.0f);
        commonWidgetSettings(Studio_16Drive);
        Studio_16Drive->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.studio16.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(Studio_16Drive, &Loopino::studio16drive, 60, "Drive", draw_knob,
            [](Loopino* self, float v) {self->synth.setStudio_16Drive(v);});

        Studio_16Warmth = add_knob(frame, "Studio_16 Warmth",80,25,38,38);
        set_adjustment(Studio_16Warmth->adj, 0.65, 0.65, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Studio_16Warmth, (Color_state)1, (Color_mod)2, 0.48f, 0.78f, 0.46f, 1.0f);
        commonWidgetSettings(Studio_16Warmth);
        Studio_16Warmth->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.studio16.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(Studio_16Warmth, &Loopino::studio16warmth, 61, "Warmth", draw_knob,
            [](Loopino* self, float v) {self->synth.setStudio_16Warmth(v);});

        Studio_16HfTilt = add_knob(frame, "Studio_16 HfTilt",120,25,38,38);
        set_adjustment(Studio_16HfTilt->adj, 0.45, 0.45, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Studio_16HfTilt, (Color_state)1, (Color_mod)2, 0.44, 0.78, 0.59, 1.0);
        commonWidgetSettings(Studio_16HfTilt);
        Studio_16HfTilt->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.studio16.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(Studio_16HfTilt, &Loopino::studio16hftilt, 62, "HfTilt", draw_knob,
            [](Loopino* self, float v) {self->synth.setStudio_16HfTilt(v);});
    }

    void addSmoothControlls(Widget_t *frame) {
        EPSOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(EPSOnOff);
        connectValueChanged(EPSOnOff, &Loopino::epsonoff, 63, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setVFX_EPSOnOff(v);});

        EPSDrive = add_knob(frame, "EPS Drive",40,25,38,38);
        set_adjustment(EPSDrive->adj, 1.0, 1.0, 0.25, 1.5, 0.01, CL_CONTINUOS);
        set_widget_color(EPSDrive, (Color_state)1, (Color_mod)2, 0.32f, 0.62f, 0.78f, 1.0f);
        commonWidgetSettings(EPSDrive);
        EPSDrive->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.eps.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(EPSDrive, &Loopino::epsdrive, 64, "Drive", draw_knob,
            [](Loopino* self, float v) {self->synth.setVFX_EPSDrive(v);});
    }

    void addTimeControlls(Widget_t *frame) {
        TMOnOff = add_toggle_button(frame, "Off",10,15,25,58);
        commonWidgetSettings(TMOnOff);
        connectValueChanged(TMOnOff, &Loopino::tmonoff, 63, nullptr, draw_my_vswitch,
            [](Loopino* self, int v) {self->synth.setTMOnOff(v);});

        TMTime = add_knob(frame, "Time",40,25,38,38);
        set_adjustment(TMTime->adj, 0.2, 0.2, 0.0, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(TMTime, (Color_state)1, (Color_mod)2, 0.32f, 0.62f, 0.78f, 1.0f);
        commonWidgetSettings(TMTime);
        TMTime->func.button_release_callback = [](void *w_, void*button_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            if (self->synth.rb.machines.tm.getOnOff())
                self->synth.rebuildKeyCache();
        };
        connectValueChanged(TMTime, &Loopino::tmtime, 64, "Time", draw_knob,
            [](Loopino* self, float v) {self->synth.setTMTime(v);});
    }

/****************************************************************
        connect controller value change callbacks
****************************************************************/

    ValueBinding<float> floatBindings[MAX_FLOAT_BINDINGS];
    int floatBindingCount = 0;

    ValueBinding<int> intBindings[MAX_INT_BINDINGS];
    int intBindingCount = 0;

    template<typename T>
    static void valueChangedCallback(void* w_, void* /*unused*/) {
        Widget_t* w = (Widget_t*)w_;
        Loopino* self = (Loopino*)w->parent_struct;
        auto* binding = (ValueBinding<T>*)w->user_data;

        if (!binding || !self) return;

        T value = (T)adj_get_value(w->adj);

        self->*(binding->member) = value;
        self->markDirty(binding->dirtyIndex);

        if (binding->extra) {
            binding->extra(self, value);
        }
    }

    void connectValueChanged( Widget_t* widget, float Loopino::*member,
                            int dirtyIndex, const char* tooltip,
                            ExposeFunc expose, void (*extra)(Loopino*, float)) {
        if (floatBindingCount >= MAX_FLOAT_BINDINGS) {
            return;
        }

        auto* binding = &floatBindings[floatBindingCount++];
        binding->member = member;
        binding->dirtyIndex = dirtyIndex;
        binding->extra = extra;

        widget->user_data = binding;
        widget->scale.gravity = ASPECT;
        widget->func.value_changed_callback = valueChangedCallback<float>;

        if (expose) {
            widget->func.expose_callback = expose;
        }

        if (tooltip) {
            widget->flags |= HAS_TOOLTIP;
            add_tooltip(widget, tooltip);
        }
    }

    void connectValueChanged( Widget_t* widget, int Loopino::*member,
                            int dirtyIndex, const char* tooltip,
                            ExposeFunc expose, void (*extra)(Loopino*, int)) {
        if (intBindingCount >= MAX_INT_BINDINGS) {
            return;
        }

        auto* binding = &intBindings[intBindingCount++];
        binding->member = member;
        binding->dirtyIndex = dirtyIndex;
        binding->extra = extra;

        widget->user_data = binding;
        widget->scale.gravity = ASPECT;
        widget->func.value_changed_callback = valueChangedCallback<int>;

        if (expose) {
            widget->func.expose_callback = expose;
        }

        if (tooltip) {
            widget->flags |= HAS_TOOLTIP;
            add_tooltip(widget, tooltip);
        }
    }


/****************************************************************
                      Button callbacks 
****************************************************************/


    void setAttack(float v)          { synth.setAttack(v); expose_widget(Envelope); }
    void setDecay(float v)           { synth.setDecay(v);  expose_widget(Envelope); }
    void setSustain(float v)         { synth.setSustain(v);  expose_widget(Envelope); }
    void setRelease(float v)         { synth.setRelease(v);  expose_widget(Envelope); }

    void widget_set_cursor(Widget_t *w, OS_CURSOR c) {
        #ifdef _WIN32
        if (GetCapture() == w->widget || w->mouse_inside)
            SetCursor(c);
        #else
        XDefineCursor(w->app->dpy, w->widget, c);
        #endif
    }

    static void drop_frame(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->widget_set_cursor(w, w->cursor);
        if (w->data > 19) { // machines
            self->sz.endDrag(self->machineOrder, 1);
            self->synth.rebuildMachineChain(self->machineOrder);
        } else { // filters
            self->sz.endDrag(self->filterOrder, 0);
            //for (int i : self->filterOrder) std::cout << i << std::endl;
            self->synth.rebuildFilterChain(self->filterOrder);
            self->synth.resetFilter(w->data);
        }
        expose_widget(w);
    }

    static void drag_frame(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->widget_set_cursor(w, w->cursor2);
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        self->sz.beginDrag(w, xbutton->x_root, xbutton->y_root);
        self->synth.setFilterOff(w->data);
        
    }

    // move left loop point following the mouse pointer
    static void move_frame(void *w_, void *xmotion_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        self->sz.dragMove(xmotion->x_root, xmotion->y_root);
    }


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
    static void setLoopSize_released(void *w_, void *button_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->inDrag = false;
        if (w->flags & HAS_POINTER) w->state= 1;
        expose_widget(w);
        if (self->af.samples) button_setLoop_callback(self->setLoop, NULL);
    }

    // loop size control (Periods to use)
    static void setLoopSize_indrag(void *w_, void *button_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->inDrag = true;
        if (self->af.samples) button_setLoop_callback(self->setLoop, NULL);
    }

    // loop size control (Periods to use)
    static void setLoopSize_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->loopPeriods = (int)(adj_get_value(w->adj));
        self->markDirty(7);
        if (self->af.samples && ! self->inDrag) button_setLoop_callback(self->setLoop, NULL);
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

    // Custom Root Key
    static void set_custom_root_key(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        uint8_t key = static_cast<uint8_t>(adj_get_value(w->adj));
        self->customFreq =  440.0 * std::pow(2.0, (key - 69 ) / 12.0);
        if (key != self->rootkey || key != self->customRootkey) {
            self->customRootkey = key;
            self->setOneShootBank(true);
        }
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

    // playbutton
    static void reverse_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->reverse = (int)adj_get_value(w->adj);
        self->synth.setReverse(self->reverse);
    }

    // set left loop point by value change
    static void slider_l_changed_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        float st = adj_get_state(w->adj_x);
        uint32_t lp = (self->af.samplesize) * st;
        if (lp > self->position) {
            lp = self->position;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        st = std::clamp(st, 0.0f, 0.99f);
        adj_set_state(w->adj_x, st);
        if (adj_get_state(self->loopMark_R->adj_x) < st+0.01)adj_set_state(self->loopMark_R->adj_x, st+0.01);
        int width = self->w->width-36;
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
                adj_set_value(w->adj_x, adj_get_value(w->adj_x) + 1.0);
            } else if(xbutton->button == Button5) {
                adj_set_value(w->adj_x, adj_get_value(w->adj_x) - 1.0);
            }
        }
        expose_widget(w);
    }

    static void slider_pressed(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        self->pressMark = xbutton->x_root;
        self->LMark = metrics.x + metrics.width * 0.5;
    }

    // move left loop point following the mouse pointer
    static void move_loopMark_L(void *w_, void *xmotion_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int x1 = xmotion->x_root - self->pressMark;
        int x2 = self->LMark;
        x2 += x1;
        int width = self->w->width-36;
        int pos = max(15, min (width+15,x2-5));
        float st =  (float)( (float)(pos-15.0)/(float)width);
        uint32_t lp = (self->af.samplesize) * st;
        if (lp > self->position) {
            self->position = lp;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        st = std::clamp(st, 0.0f, 0.99f);
        float st_ = adj_get_state(w->adj);
        st = std::clamp(st, st_ - 0.01f, st_ + 0.01f);
        adj_set_state(w->adj_x, st);
    }

    // set right loop point by value changes
    static void slider_r_changed_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        float st = adj_get_state(w->adj_x);
        uint32_t lp = (self->af.samplesize * st);
        if (lp < self->position) {
            self->position = lp;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        st = std::clamp(st, 0.01f, 1.0f);
        adj_set_state(w->adj_x, st);
        if (adj_get_state(self->loopMark_L->adj_x) > st-0.01)adj_set_state(self->loopMark_L->adj_x, st-0.01);
        int width = self->w->width-36;
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
                adj_set_value(w->adj_x, adj_get_value(w->adj_x) - 1.0);
            } else if(xbutton->button == Button5) {
                adj_set_value(w->adj_x, adj_get_value(w->adj_x) + 1.0);
            }
        }
        expose_widget(w);
    }

    // move right loop point following the mouse pointer
    static void move_loopMark_R(void *w_, void *xmotion_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int x1 = xmotion->x_root - self->pressMark;
        int x2 = self->LMark;
        x2 += x1;
        int width = self->w->width-36;
        int pos = max(15, min (width+15,x2-5));
        float st =  (float)( (float)(pos-15.0)/(float)width);
         uint32_t lp = (self->af.samplesize * st);
        if (lp < self->position) {
            self->position = lp;
            st = max(0.0, min(1.0, (float)((float)self->position/(float)self->af.samplesize)));
        }
        st = std::clamp(st, 0.01f, 1.0f);
        float st_ = adj_get_state(w->adj_x);
        st = std::clamp(st, st_ - 0.01f, st_ + 0.01f);
        adj_set_state(w->adj_x, st);
    }

    // set loop mark positions on window resize
    static void resize_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        float st = adj_get_state(self->loopMark_L->adj_x);
        int width = self->w->width-40;
        os_move_window(w->app->dpy, self->loopMark_L, 15+ (width * st), 2);
        st = adj_get_state(self->loopMark_R->adj_x);
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

    // LP keytracking control
    static void lpkeytracking_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Wheel *wheel = (Wheel*)w->private_struct;
        self->lpkeytracking = (wheel->value + 1.0f) * 0.5f;
        self->markDirty(20);
        self->synth.setLpKeyTracking(self->lpkeytracking);
    }

    // HP keytracking control
    static void hpkeytracking_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Wheel *wheel = (Wheel*)w->private_struct;
        self->hpkeytracking = (wheel->value + 1.0f) * 0.5f;
        self->markDirty(21);
        self->synth.setHpKeyTracking(self->hpkeytracking);
    }

    // OBF keytracking control
    static void obfkeytracking_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Wheel *wheel = (Wheel*)w->private_struct;
        self->obfkeytracking = (wheel->value + 0.3f) * 0.3f;  // (value -0.3) / 0.3
        self->markDirty(24);
        self->synth.setKeyTrackingObf(self->obfkeytracking);
    }

    // Wasp keytracking control
    static void waspkeytracking_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Wheel *wheel = (Wheel*)w->private_struct;
        self->waspkeytracking = (wheel->value + 1.0f) * 0.5f;  // (value -0.3) / 0.3
        self->markDirty(45);
        self->synth.setKeyTrackingWasp(self->waspkeytracking);
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

    void velocity_box_set_active(Widget_t *w) {
        Widget_t * p = (Widget_t*)w->parent;
        int response = 0;
        int i = 0;
        for(;i<p->childlist->elem;i++) {
            Widget_t *wid = p->childlist->childs[i];
            if (wid->adj && wid->flags & IS_RADIO) {
                if (wid != w) adj_set_value(wid->adj_y, 0.0);
                else if (wid == w) {
                    velmode = response;
                    markDirty(22);
                    if (adj_get_value(wid->adj) != 1.0)
                        adj_set_value(wid->adj, 1.0);
                    synth.setVelMode(velmode);
                }
                ++response;
            }
        }
    }

    // velmode control
    static void radio_box_velocity_pressed(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (w->flags & HAS_FOCUS) {
            self->velocity_box_set_active(w);
        }
    }

    // Pitch wheel control
    static void wheel_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        Wheel *wheel = (Wheel*)w->private_struct;
        self->pitchwheel = wheel->value;
        self->markDirty(19);
        self->synth.setPitchWheel(self->pitchwheel);
    }

    // volume control
    static void volume_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->volume = adj_get_value(w->adj);
        self->markDirty(5); // FIXME
        self->gain = std::pow(1e+01, 0.05 * self->volume);
        self->synth.setGain(self->gain);
    }

    static void sharp_released(void *w_, void* xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->setOneShootBank();
        self->setLoopToBank();

    }
    // sharp control
    static void sharp_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->sharp = adj_get_value(w->adj);
        self->markDirty(10);
        self->process_sharp();
        self->process_sample_sharp();
    }

    // saw control
    static void saw_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->saw = adj_get_value(w->adj);
        self->markDirty(11);
        self->process_sharp();
        self->process_sample_sharp();
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
            .bg     = { UI_PANEL }, // Hintergrund
            .base   = { 0.125, 0.125, 0.125, 1.000 }, // Panel / Widget
            .text   = { UI_TEXT },
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

    static void setFrameColour(Widget_t* w, cairo_t *cr, int x, int y, int wi, int h) {
        Colors *c = get_color_scheme(w, NORMAL_);
        Colors *c1 = get_color_scheme(w, PRELIGHT_);
        cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x, y + h);
        cairo_pattern_add_color_stop_rgba
            (pat, 0, c1->base[0], c1->base[1], c1->base[2],0.3);
        cairo_pattern_add_color_stop_rgba 
            (pat, 1, c->bg[0]*0.1, c->bg[1]*0.1, c->bg[2]*0.1,1.0);
        cairo_set_source(cr, pat);
        cairo_pattern_destroy (pat);
    }

    static void setReverseFrameColour(Widget_t* w, cairo_t *cr, int x, int y, int wi, int h) {
        Colors *c = get_color_scheme(w, NORMAL_);
        Colors *c1 = get_color_scheme(w, PRELIGHT_);
        cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x, y + h);
        cairo_pattern_add_color_stop_rgba
            (pat, 1, c1->base[0], c1->base[1], c1->base[2],0.3);
        cairo_pattern_add_color_stop_rgba 
            (pat, 0, c->bg[0]*0.1, c->bg[1]*0.1, c->bg[2]*0.1,1.0);
        cairo_set_source(cr, pat);
        cairo_pattern_destroy (pat);
    }

    static void rounded_frame(cairo_t *cr,float x, float y, float w, float h, float lsize) {
        cairo_new_path (cr);
        float r = 10.0;
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

    static void round_area(cairo_t *cr, float x, float y, float x1, float y1, float w, float h, float lsize) {
        cairo_new_path (cr);
        float r = 10.0;
        cairo_move_to(cr, x+lsize+r,y1);
        cairo_line_to(cr, x+w-r,y1);
        cairo_curve_to(cr, x+w,y1,x+w,y1,x+w,y1+r);
        cairo_line_to(cr, x+w,y+h-r);
        cairo_curve_to(cr, x+w,y+h,x+w,y+h,x+w-r,y+h);
        cairo_line_to(cr, x+r,y+h);
        cairo_curve_to(cr, x,y+h,x,y+h,x,y+h-r);
        cairo_line_to(cr, x,y+r);
        cairo_curve_to(cr, x,y,x,y,x+r,y);
        cairo_line_to(cr, x+lsize-r,y);
        cairo_curve_to(cr, x+lsize,y, x+lsize,y, x+lsize,y1-r);
        //cairo_arc(cr, x+lsize,y1, r, 3*M_PI/2, 0);
        cairo_arc_negative(cr, x+lsize+r,y1-r, r, M_PI, M_PI/2);
        //cairo_curve_to(cr, x+lsize,y1-r, x+lsize,y1-r, x+lsize+r,y1);
    }

    static void draw_frame(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        if (!metrics.visible) return;
        int width_t = metrics.width;
        int height_t = metrics.height;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);

        cairo_text_extents_t extents;
        //use_text_color_scheme(w, get_color_state(w));
        cairo_set_font_size (w->crb, w->app->normal_font/w->scale.ascale);
        cairo_text_extents(w->crb,"Abc" , &extents);
        int pt = extents.height;

        cairo_text_extents(w->crb,w->label , &extents);
        int pta = (w->width - extents.width) * 0.5;
        cairo_set_line_width(w->crb,2);
        cairo_set_source_rgba(w->crb, UI_FRAME);
        self->roundrec(w->crb, 5, 0, width_t-10, height_t, 5);
        //round_area(w->crb, 5 , 0 ,
        //    extents.width+10 , pt,
        //    w->width-10 , w->height , extents.width+6);
 
        //rounded_frame(w->crb, 5, 5, width_t-10, height_t-8, extents.width+7);
        cairo_fill_preserve(w->crb);
        setFrameColour(w, w->crb, 5, 5, width_t-10, height_t-10);
        cairo_stroke(w->crb);
        cairo_new_path (w->crb);
        cairo_set_source_rgba(w->crb, 0.55, 0.65, 0.55, 1);
        cairo_move_to (w->crb, pta, pt+2);
        cairo_show_text(w->crb, w->label);
        cairo_new_path (w->crb);
        cairo_set_source_rgba(w->crb, UI_PANEL);
        cairo_arc(w->crb,10,5,2,0,2*M_PI);
        cairo_fill(w->crb);
        cairo_arc(w->crb,width_t - 10, 5,2,0,2*M_PI);
        cairo_fill(w->crb);
        cairo_arc(w->crb,width_t - 10, height_t -5,2,0,2*M_PI);
        cairo_fill(w->crb);
        cairo_arc(w->crb,10, height_t -5,2,0,2*M_PI);
        cairo_fill(w->crb);
    }

    static void draw_slider(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        if (!metrics.visible) return;
        int height = metrics.height;
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

    static void pattern_out(Widget_t *w, Color_state st, int height) {
        Colors *c = get_color_scheme(w,st);
        if (!c) return;
        cairo_pattern_t *pat = cairo_pattern_create_linear (2, 2, 2, height);
        cairo_pattern_add_color_stop_rgba(pat, 0.0, c->light[0],  c->light[1], c->light[2],  c->light[3]);
        cairo_pattern_add_color_stop_rgba(pat, 0.5, 0.0, 0.0, 0.0, 0.0);
        cairo_pattern_add_color_stop_rgba(pat, 1.0, c->light[0],  c->light[1], c->light[2],  c->light[3]);
        cairo_set_source(w->crb, pat);
        cairo_pattern_destroy (pat);
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
        setFrameColour(w, w->crb, 0, 0, width, height);
        cairo_set_line_width(w->crb,  2.0/w->scale.ascale);
        cairo_arc (w->crb, knobx1+arc_offset, knoby1+arc_offset, radius+3,
              add_angle, add_angle + 360 * (M_PI/180));
        cairo_stroke(w->crb);
        // base
        use_base_color_scheme(w, INSENSITIVE_);
        if(w->state==1) pattern_out(w, PRELIGHT_, height);
        cairo_set_line_width(w->crb,  5.0/w->scale.ascale);
        cairo_arc (w->crb, knobx1+arc_offset, knoby1+arc_offset, radius,
              add_angle + scale_zero, add_angle + scale_zero + 320 * (M_PI/180));
        cairo_stroke(w->crb);

        // indicator
        cairo_set_line_width(w->crb,  3.0/w->scale.ascale);
        cairo_new_sub_path(w->crb);
        //cairo_set_source_rgba(w->crb, 0.75, 0.75, 0.75, 1);
        use_base_color_scheme(w, PRELIGHT_);
        if (! w->data) {
            cairo_arc (w->crb,knobx1+arc_offset, knoby1+arc_offset, radius,
                  add_angle + scale_zero, add_angle + angle);
        } else {
            const double mid_angle = scale_zero + 0.5 * 2 * (M_PI - scale_zero);
            if (knobstate < 0.5f)
                cairo_arc_negative (w->crb, knobx1+arc_offset, knoby1+arc_offset, radius,
                  add_angle + mid_angle, add_angle + angle);
            else
                cairo_arc (w->crb, knobx1+arc_offset, knoby1+arc_offset, radius,
                  add_angle + mid_angle, add_angle + angle);
        }
        cairo_stroke(w->crb);
        cairo_new_sub_path(w->crb);

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
        cairo_set_source_rgba(cri, 0.16f*0.5f,0.18f*0.5f,0.18f*0.5f,1.0f);
        roundrec(cri, 0, 0, width, height, 5);
        cairo_fill_preserve(cri);
        //cairo_set_source_rgba(cri,  0.302, 0.714, 0.675, 0.4);
        setFrameColour(w, cri, 0, 0, width, height);
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

        if (self->play) {
            double state = adj_get_state(w->adj);
            cairo_set_source_rgba(w->crb, 0.55, 0.05, 0.05, 1);
            cairo_rectangle(w->crb, (width * state) - 1.5,2,3, height-4);
            cairo_fill(w->crb);
        }

        //int halfWidth = width*0.5;

        double state_l = adj_get_state(self->loopMark_L->adj_x);
        cairo_set_source_rgba(w->crb, 0.25, 0.25, 0.05, 0.666);
        cairo_rectangle(w->crb, 0, 2, (width*state_l), height-4);
        cairo_fill(w->crb);

        double state_r = adj_get_state(self->loopMark_R->adj_x);
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

    static void draw_metal(cairo_t* const cr, int x, int y, int w, int h) {
        cairo_pattern_t *metal = cairo_pattern_create_linear(0, y, 0, y+h);
        cairo_pattern_add_color_stop_rgb(metal,0.0, 0.10,0.10,0.11);
        cairo_pattern_add_color_stop_rgb(metal,0.5, 0.18,0.18,0.19);
        cairo_pattern_add_color_stop_rgb(metal,1.0, 0.07,0.07,0.08);
        cairo_set_source(cr, metal);
        //round_rectangle(cr, x,y,w,h, r);
        cairo_paint(cr);
        cairo_pattern_destroy(metal);

        for (int i=0;i<h;i+=2) {
            double a = 0.03 + (rand()%1000)/1000.0 * 0.03;
            cairo_set_source_rgba(cr,1,1,1,a);
            cairo_move_to(cr, x+3, y+i+0.5);
            cairo_line_to(cr, x+w-3, y+i+0.5);
            cairo_stroke(cr);
        }
        cairo_pattern_t *spec = cairo_pattern_create_linear(0,y-0.2*h, 0,y+0.8*h);
        cairo_pattern_add_color_stop_rgba(spec,0.0, 1,1,1,0.0);
        cairo_pattern_add_color_stop_rgba(spec,0.45,1,1,1,0.12);
        cairo_pattern_add_color_stop_rgba(spec,0.55,1,1,1,0.05);
        cairo_pattern_add_color_stop_rgba(spec,1.0, 1,1,1,0.0);
        cairo_set_source(cr, spec);
        //round_rectangle(cr, x,y,w,h, r);
        cairo_paint(cr);
        cairo_pattern_destroy(spec);

        //cairo_set_source_rgba(cr,0,0,0,0.35);
        //round_rectangle(cr, x,y+1,w,h,r);
        //cairo_stroke(cr);
    }

    static void draw_combo_button(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        if (!w) return;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width = metrics.width-3;
        int height = metrics.height-4;
        if (!metrics.visible) return;
        if (!w->state && (int)w->adj_y->value)
            w->state = 3;

        float offset = 0.0;
        if(w->state==0) {
            use_fg_color_scheme(w, NORMAL_);
        } else if(w->state==1) {
            use_fg_color_scheme(w, PRELIGHT_);
            offset = 1.0;
        } else if(w->state==2) {
            use_fg_color_scheme(w, SELECTED_);
            offset = 2.0;
        } else if(w->state==3) {
            use_fg_color_scheme(w, ACTIVE_);
            offset = 1.0;
        }
        use_text_color_scheme(w, get_color_state(w));
        int wa = width/1.1;
        int h = height/2.2;
        int wa1 = width/1.55;
        int h1 = height/1.3;
        int wa2 = width/2.8;
       
        cairo_move_to(w->crb, wa+offset, h+offset);
        cairo_line_to(w->crb, wa1+offset, h1+offset);
        cairo_line_to(w->crb, wa2+offset, h+offset);
        cairo_line_to(w->crb, wa+offset, h+offset);
        cairo_fill(w->crb);
    }

    static void draw_combobox(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        if (!w) return;
        Metrics_t metrics;
        os_get_window_metrics(w, &metrics);
        int width = metrics.width-2;
        int height = metrics.height-2;
        if (!metrics.visible) return;
        int v = (int)adj_get_value(w->adj);
        int vl = v - (int) w->adj->min_value;
       // if (v<0) return;
        Widget_t * menu = w->childlist->childs[1];
        Widget_t* view_port =  menu->childlist->childs[0];
        ComboBox_t *comboboxlist = (ComboBox_t*)view_port->parent_struct;

        cairo_rectangle(w->crb,2.0, 2.0, width, height);

        if(w->state==0) {
            cairo_set_line_width(w->crb, 1.0);
            use_shadow_color_scheme(w, NORMAL_);
            cairo_fill_preserve(w->crb);
            use_frame_color_scheme(w, NORMAL_);
        } else if(w->state==1) {
            use_shadow_color_scheme(w, PRELIGHT_);
            cairo_fill_preserve(w->crb);
            cairo_set_line_width(w->crb, 1.5);
            use_frame_color_scheme(w, NORMAL_);
        } else if(w->state==2) {
            use_shadow_color_scheme(w, SELECTED_);
            cairo_fill_preserve(w->crb);
            cairo_set_line_width(w->crb, 1.0);
            use_frame_color_scheme(w, SELECTED_);
        } else if(w->state==3) {
            use_shadow_color_scheme(w, ACTIVE_);
            cairo_fill_preserve(w->crb);
            cairo_set_line_width(w->crb, 1.0);
            use_frame_color_scheme(w, ACTIVE_);
        } else if(w->state==4) {
            use_shadow_color_scheme(w, INSENSITIVE_);
            cairo_fill_preserve(w->crb);
            cairo_set_line_width(w->crb, 1.0);
            use_frame_color_scheme(w, INSENSITIVE_);
        }
        cairo_stroke(w->crb);

        cairo_rectangle(w->crb,4.0, 4.0, width, height);
        cairo_stroke(w->crb);
        cairo_rectangle(w->crb,3.0, 3.0, width, height);
        cairo_stroke(w->crb);
        if (comboboxlist->list_size<1) return;
        if (vl<0) return;

        cairo_text_extents_t extents;

        use_text_color_scheme(w, get_color_state(w));
        float font_size = w->app->normal_font/comboboxlist->sc;
        cairo_set_font_size (w->crb, font_size);
        cairo_text_extents(w->crb,"Ay", &extents);
        double h = extents.height;

        cairo_move_to (w->crb, 15, (height+h)*0.55);
        cairo_show_text(w->crb, comboboxlist->list_names[vl]);
        cairo_new_path (w->crb);
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

    static void draw_window_box(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Widget_t *p = (Widget_t*)w->parent;
        Metrics_t metrics;
        os_get_window_metrics(p, &metrics);
        if (!metrics.visible) return;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        use_bg_color_scheme(w, NORMAL_);
        cairo_paint (w->crb);
        if (self->glowDragX > 0) {
            cairo_set_line_cap (w->crb, CAIRO_LINE_CAP_ROUND);
            cairo_set_source_rgba(w->crb, 0.55, 0.65, 0.55, 0.4);
            cairo_set_line_width(w->crb, 5);
            cairo_move_to(w->crb, self->glowDragX, self->glowDragY);
            cairo_line_to(w->crb, self->glowDragX, self->glowDragY + 75 * w->app->hdpi);
            cairo_stroke(w->crb);
        }
        int keyCacheState = self->synth.rb.getKeyCacheState();
        if (keyCacheState > 0 ) {
            cairo_set_source_rgba(w->crb, keyCacheState/16.0, 1.0 - keyCacheState/16.0,  0.15, 0.4);
            cairo_set_line_width(w->crb, 5);
            cairo_move_to(w->crb, 70, w->height-5);
            cairo_line_to(w->crb, 15 + 55 * keyCacheState, w->height-5);
            cairo_stroke(w->crb);
        }
        
        #ifndef RUN_AS_PLUGIN
        cairo_text_extents_t extents;
        /** show value on the kob**/
        char s[14];
        snprintf(s, 13, " Xruns: %d", self->xruns);
        use_fg_color_scheme(w, NORMAL_);
        if (self->xruns)
            cairo_set_source_rgba(w->crb, 0.671, 0.0, 0.051, 1.0);
        cairo_set_font_size (w->crb, (w->app->small_font-2)/w->scale.ascale);
        cairo_text_extents(w->crb, s, &extents);
        cairo_move_to (w->crb, w->width-extents.width-20, w->height-2);
        cairo_show_text(w->crb, s);
        cairo_new_path (w->crb);
        
        #endif
    }



void knobShadowOutset(cairo_t* const cr, int width, int height, int x, int y) {
    cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x + width, y + height);
    cairo_pattern_add_color_stop_rgba
        (pat, 0, 0.33, 0.33, 0.33, 1);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.45, 0.33 * 0.6, 0.33 * 0.6, 0.33 * 0.6, 0.4);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.65, 0.05 * 2.0, 0.05 * 2.0, 0.05 * 2.0, 0.4);
    cairo_pattern_add_color_stop_rgba 
        (pat, 1, 0.05, 0.05, 0.05, 1);
    cairo_pattern_set_extend(pat, CAIRO_EXTEND_NONE);
    cairo_set_source(cr, pat);
    cairo_fill_preserve (cr);
    cairo_pattern_destroy (pat);
}

void knobShadowInset(cairo_t* const cr, int width, int height, int x, int y) {
    cairo_pattern_t* pat = cairo_pattern_create_linear (x, y, x + width, y + height);
    cairo_pattern_add_color_stop_rgba
        (pat, 1, 0.33, 0.33, 0.33, 1);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.65, 0.33 * 0.6, 0.33 * 0.6, 0.33 * 0.6, 0.4);
    cairo_pattern_add_color_stop_rgba
        (pat, 0.55, 0.05 * 2.0, 0.05 * 2.0, 0.05 * 2.0, 0.4);
    cairo_pattern_add_color_stop_rgba
        (pat, 0, 0.05, 0.05, 0.05, 1);
    cairo_pattern_set_extend(pat, CAIRO_EXTEND_NONE);
    cairo_set_source(cr, pat);
    cairo_fill (cr);
    cairo_pattern_destroy (pat);
}

static void draw_my_vswitch(void *w_, void* user_data) {
    Widget_t *wid = (Widget_t*)w_;
    Loopino *self = static_cast<Loopino*>(wid->parent_struct);
    const int x = wid->width * 0.125;
    const int y = wid->height * 0.2;
    const int w = wid->width * 0.75;
    const int h = wid->height * 0.6;
    const int state = (int)adj_get_state(wid->adj);

    const int centerW = w * 0.5;
    const int centerH = state ? centerW : h - centerW ;
    const int offset = w * 0.21;

    cairo_push_group (wid->crb);
    
    self->roundrec(wid->crb, x+1, y+1, w-2, h-2, centerW);
    self->knobShadowOutset(wid->crb, w  , h, x, y);
    cairo_stroke_preserve (wid->crb);

    cairo_new_path(wid->crb);
    self->roundrec(wid->crb, x+offset, y+offset, w - (offset * 2), h - (offset * 2), centerW-offset);
    cairo_set_source_rgba(wid->crb, 0.05, 0.05, 0.05, 1);
    if (wid->state == 1) {
         pattern_out(wid, PRELIGHT_,  wid->height);
    }
    cairo_fill_preserve(wid->crb);

    cairo_set_source_rgba(wid->crb, 0.05, 0.05, 0.05, 1);
    cairo_set_line_width(wid->crb,1);
    cairo_stroke_preserve (wid->crb);

    cairo_new_path(wid->crb);
    cairo_arc(wid->crb,x+centerW, y+centerH, w/2.8, 0, 2 * M_PI );
    use_bg_color_scheme(wid, PRELIGHT_);
    cairo_fill_preserve(wid->crb);
    self->knobShadowOutset(wid->crb, w * 0.5 , h, x+centerH - centerW, y);
    cairo_set_source_rgba(wid->crb, 0.05, 0.05, 0.05, 1);
    cairo_set_line_width(wid->crb,1);
    cairo_stroke_preserve (wid->crb);

    cairo_new_path(wid->crb);
    cairo_arc(wid->crb,x+centerW, y+centerH, w/3.6, 0, 2 * M_PI );
    if(wid->state==1) use_bg_color_scheme(wid, PRELIGHT_);
    else use_bg_color_scheme(wid, NORMAL_);
    cairo_fill_preserve(wid->crb);
    self->knobShadowInset(wid->crb, w * 0.5 , h, x+centerH - centerW, y);
    cairo_stroke (wid->crb);

    /** show label below the switch**/
    cairo_text_extents_t extents;
    cairo_select_font_face (wid->crb, "Sans", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
    if (!state) {
        use_fg_color_scheme(wid, INSENSITIVE_);
    } else {
        use_fg_color_scheme(wid, PRELIGHT_);
    }

    cairo_set_font_size (wid->crb, wid->app->small_font/wid->scale.ascale);
    cairo_text_extents(wid->crb,"On" , &extents);
    cairo_move_to (wid->crb, (wid->width*0.5)-(extents.width*0.5), 4+(extents.height));
    cairo_show_text(wid->crb, "On");
    cairo_new_path (wid->crb);
    /** show label above the switch**/
    if (state) {
        use_fg_color_scheme(wid, INSENSITIVE_);
    } else {
        use_fg_color_scheme(wid, PRELIGHT_);
    }
    cairo_set_font_size (wid->crb, wid->app->small_font/wid->scale.ascale);
    cairo_text_extents(wid->crb,wid->label , &extents);
    cairo_move_to (wid->crb, (wid->width*0.5)-(extents.width*0.5), wid->height -(extents.height*0.8));
    cairo_show_text(wid->crb, wid->label);
    cairo_new_path (wid->crb);

    cairo_pop_group_to_source (wid->crb);
    cairo_paint (wid->crb);
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
                self->createPrestList();
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
        loadSub->func.enter_callback = [](void *w_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Metrics_t metrics;
            os_get_window_metrics(w, &metrics);
            if (!metrics.visible) return;
            if (childlist_has_child(w->childlist)) {
                if (w->app->submenu) {
                    if (w->app->submenu != w->childlist->childs[0]) {
                        widget_hide(w->app->submenu);
                        w->app->submenu = NULL;
                    }
                }
                pop_submenu_show(w, w->childlist->childs[0], 24, false);
            }
            os_transparent_draw(w_, user_data);
        };
        loadSub->func.value_changed_callback = [](void *w_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            int id = (int)w->adj->value;
            if (id >= 0 && id < (int)self->presetFiles.size()) {
                self->currentPresetNum = id;
                std::string name = self->presetFiles[id];
                std::string path = self->getPathFor(name);
                self->loadPreset(path);
            }
        };
        def->func.button_release_callback = [](void *w_, void*item_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            self->generateSine();
            self->param.resetParams();
            self->setValuesFromHost();
            std::vector<int> d = {8,9,10,11,12,20,21,22,23,24,25};
            std::vector<int> m = {20,21,22,23,24,25};
            std::vector<int> f = {8,9,10,11,12};
            self->sz.applyPresetOrder(d);
            self->synth.rebuildMachineChain(m);
            self->synth.rebuildFilterChain(f);
        };
        expo->func.button_release_callback = [](void *w_, void*item_, void *user_data) {
            Widget_t *w = (Widget_t*)w_;
            Loopino *self = static_cast<Loopino*>(w->parent_struct);
            self->showExportWindow();
        };
        pop_menu_show(w, menu, 24, true);

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
            std::filesystem::create_directories(p);
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
        if (maxVal < 0.9999f) maxVal = 1.0f;

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
        header.version = 15; // guard for future proof
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
        // since version 9
        writeControllerValue(out, HpResonance);
        writeControllerValue(out, HpCutOff);
        // since version 10
        writeValue(out, lpkeytracking);
        writeValue(out, hpkeytracking);
        writeValue(out, velmode);
        // since version 11
        writeControllerValue(out, Volume);
        writeControllerValue(out, ObfMode);
        writeValue(out, obfkeytracking);
        writeControllerValue(out, ObfResonance);
        writeControllerValue(out, ObfCutOff);
        writeControllerValue(out, ObfOnOff);
        writeControllerValue(out, LpOnOff);
        writeControllerValue(out, HpOnOff);
        writeControllerValue(out, VibOnOff);
        writeControllerValue(out, TremOnOff);

        writeControllerValue(out, ChorusOnOff);
        writeControllerValue(out, ChorusLev);
        writeControllerValue(out, ChorusDelay);
        writeControllerValue(out, ChorusDepth);
        writeControllerValue(out, ChorusFreq);
        writeControllerValue(out, RevOnOff);
        writeControllerValue(out, RevRoomSize);
        writeControllerValue(out, RevDamp);
        writeControllerValue(out, RevMix);
        // since version 12
        writeControllerValue(out, WaspOnOff);
        writeControllerValue(out, WaspMix);
        writeControllerValue(out, WaspResonance);
        writeControllerValue(out, WaspCutOff);
        writeValue(out, waspkeytracking);
        // since version 14
        writeControllerValue(out, TBOnOff);
        writeControllerValue(out, TBVintage);
        writeControllerValue(out, TBResonance);
        writeControllerValue(out, TBCutOff);
        writeControllerValue(out, Tone);

        writeControllerValue(out, LM_MIR8OnOff);
        writeControllerValue(out, LM_MIR8Drive);
        writeControllerValue(out, LM_MIR8Amount);
        writeControllerValue(out, Emu_12OnOff);
        writeControllerValue(out, Emu_12Drive);
        writeControllerValue(out, Emu_12Amount);
        writeControllerValue(out, LM_CMP12OnOff);
        writeControllerValue(out, LM_CMP12Drive);
        writeControllerValue(out, LM_CMP12Ratio);
        writeControllerValue(out, Studio_16OnOff);
        writeControllerValue(out, Studio_16Drive);
        writeControllerValue(out, Studio_16Warmth);
        writeControllerValue(out, Studio_16HfTilt);
        writeControllerValue(out, EPSOnOff);
        writeControllerValue(out, EPSDrive);
        // since version 15
        writeControllerValue(out, TMOnOff);
        writeControllerValue(out, TMTime);
        writeControllerValue(out, Reverse);
        for(auto x : filterOrder)
            writeValue(out, x);
        for(auto x : machineOrder)
            writeValue(out, x);

        writeSampleBuffer(out, af.samples, af.samplesize);
        // since version 13
        writeValue(out, jack_sr);
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
        if (header.version > 15) {
            std::cerr << "Warning: newer preset version (" << header.version << ")\n";
            return false;
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
        if (header.version > 8) {
            readControllerValue(in, HpResonance);
            readControllerValue(in, HpCutOff);
        }
        if (header.version > 9) {
            readValue(in, lpkeytracking);
            wheel_set_value(LpKeyTracking, (lpkeytracking * 2.0f) - 1.0f);
            synth.setLpKeyTracking(lpkeytracking);
            readValue(in, hpkeytracking);
            wheel_set_value(HpKeyTracking, (hpkeytracking * 2.0f) - 1.0f);
            synth.setHpKeyTracking(hpkeytracking);
            readValue(in, velmode);
            velocity_box_set_active(VelMode[velmode]);
            expose_widget(LpKeyTracking);
            expose_widget(HpKeyTracking);
        }
        if (header.version > 10) {
            readControllerValue(in, Volume);
            readControllerValue(in, ObfMode);
            readValue(in, obfkeytracking);
            wheel_set_value(ObfKeyTracking, (obfkeytracking -0.3) / 0.3);
            readControllerValue(in, ObfResonance);
            readControllerValue(in, ObfCutOff);
            readControllerValue(in, ObfOnOff);
            readControllerValue(in, LpOnOff);
            readControllerValue(in, HpOnOff);
            readControllerValue(in, VibOnOff);
            readControllerValue(in, TremOnOff);

            readControllerValue(in, ChorusOnOff);
            readControllerValue(in, ChorusLev);
            readControllerValue(in, ChorusDelay);
            readControllerValue(in, ChorusDepth);
            readControllerValue(in, ChorusFreq);
            readControllerValue(in, RevOnOff);
            readControllerValue(in, RevRoomSize);
            readControllerValue(in, RevDamp);
            readControllerValue(in, RevMix);

            expose_widget(ObfKeyTracking);
        }
        if (header.version > 11) {
            readControllerValue(in, WaspOnOff);
            readControllerValue(in, WaspMix);
            readControllerValue(in, WaspResonance);
            readControllerValue(in, WaspCutOff);
            readValue(in, waspkeytracking);
            wheel_set_value(WaspKeyTracking, (waspkeytracking * 2.0f) - 1.0f);
            expose_widget(WaspKeyTracking);
        }
        if (header.version > 13) {
            readControllerValue(in, TBOnOff);
            readControllerValue(in, TBVintage);
            readControllerValue(in, TBResonance);
            readControllerValue(in, TBCutOff);
            readControllerValue(in, Tone);

            readControllerValue(in, LM_MIR8OnOff);
            readControllerValue(in, LM_MIR8Drive);
            readControllerValue(in, LM_MIR8Amount);
            readControllerValue(in, Emu_12OnOff);
            readControllerValue(in, Emu_12Drive);
            readControllerValue(in, Emu_12Amount);
            readControllerValue(in, LM_CMP12OnOff);
            readControllerValue(in, LM_CMP12Drive);
            readControllerValue(in, LM_CMP12Ratio);
            readControllerValue(in, Studio_16OnOff);
            readControllerValue(in, Studio_16Drive);
            readControllerValue(in, Studio_16Warmth);
            readControllerValue(in, Studio_16HfTilt);
            readControllerValue(in, EPSOnOff);
            readControllerValue(in, EPSDrive);
        }
        if (header.version > 14) {
            readControllerValue(in, TMOnOff);
            readControllerValue(in, TMTime);
            readControllerValue(in, Reverse);
            for(auto& x : filterOrder)
                readValue(in, x);
            for(auto& x : machineOrder)
                readValue(in, x);
        }

        readSampleBuffer(in, af.samples, af.samplesize);
        if (header.version > 12) {
            uint32_t sampleRate = jack_sr;
            readValue(in, sampleRate);
            if (sampleRate != jack_sr) {
                af.checkSampleRate(&af.samplesize, 1, af.samples, sampleRate, jack_sr);
            }
        }
        in.close();
        adj_set_max_value(wview->adj, (float)af.samplesize);
        adj_set_state(loopMark_L->adj_x, 0.0);
        adj_set_state(loopMark_R->adj_x,1.0);
        loadLoopNew = true;
        //loadNew = true;
       // update_waveview(wview, af.samples, af.samplesize);
        loadPresetToSynth();

        std::vector<int> rackOrder;
        rackOrder.reserve(filterOrder.size() + machineOrder.size());
        rackOrder.insert(rackOrder.end(), filterOrder.begin(),  filterOrder.end());
        rackOrder.insert(rackOrder.end(), machineOrder.begin(), machineOrder.end());
        sz.applyPresetOrder(rackOrder);

        synth.rebuildMachineChain(machineOrder);
        synth.rebuildFilterChain(filterOrder);

        std::filesystem::path p = filename;
        presetName = p.stem().string();
        std::string tittle = "loopino: " + presetName;
        widget_set_title(w_top, tittle.data());
        return true;
    }

};

#endif

