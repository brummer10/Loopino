
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
    std::vector<float> sampleBuffer;

    SampleBank sbank;
    SampleInfo sampleData;
    SampleBank lbank;
    SampleInfo loopData;

    uint32_t jack_sr;
    uint32_t position;
    uint32_t loopPoint_l;
    uint32_t loopPoint_r;
    uint32_t loopPoint_l_auto;
    uint32_t loopPoint_r_auto;
    uint32_t frameSize;

    uint8_t rootkey;
    uint8_t loopRootkey;

    int16_t pitchCorrection;
    int16_t loopPitchCorrection;
    int16_t matches;
    int16_t currentLoop;
    int16_t loopPeriods;

    float freq;
    float loopFreq;
    float gain;

    std::string filename;
    std::string lname;
    std::string instrument_name;

    bool loadNew;
    bool loadLoopNew;
    bool play;
    bool play_loop;
    bool ready;
    bool havePresetToLoad;

    Loopino() : af() {
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
        matches = 0;
        currentLoop = 0;
        loopPeriods = 1;
        freq = 0.0;
        pitchCorrection = 0;
        rootkey = 60;
        loopFreq = 0.0;
        loopPitchCorrection = 0;
        loopRootkey = 60;
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
        useLoop = 0;
        generateKeys();
        guiIsCreated = false;
        #if defined (RUN_AS_CLAP_PLUGIN)
        registerParameters();
        #endif
    };

    ~Loopino() {
        pa.stop();
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
        af.channels = 1;
        loopPoint_l = 0;
        loopPoint_r = af.samplesize;
        setOneShootBank();
        if (createLoop()) {
            setLoopToBank();
        } 
        #if defined (RUN_AS_CLAP_PLUGIN)
        setValuesFromHost();
        #endif
    }

/****************************************************************
                 Clap wrapper
****************************************************************/

    void markDirty(int num) {
        #if defined (RUN_AS_CLAP_PLUGIN)
        param.setParamDirty(num , true);
        param.controllerChanged.store(true, std::memory_order_release);
        #endif
    }

#if defined (RUN_AS_CLAP_PLUGIN)
#include "Clap/LoopinoClapWrapper.cc"
#endif

/****************************************************************
                      main window
****************************************************************/

    // create the main GUI
    void createGUI(Xputty *app) {
        #ifndef RUN_AS_CLAP_PLUGIN
        set_custom_theme(app);
        w_top = create_window(app, os_get_root_window(app, IS_WINDOW), 0, 0, 880, 290);
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
        os_set_window_min_size(w_top, 798, 190, 880, 290);

        w = create_widget(app, w_top, 0, 0, 440, 190);
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

        lw = create_widget(app, w_top, 440, 0, 440, 190);
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

        filebutton = add_file_button(w, 20, 148, 35, 35, getenv("HOME") ? getenv("HOME") : PATH_SEPARATOR, "audio");
        filebutton->scale.gravity = SOUTHEAST;
        widget_get_png(filebutton, LDVAR(load__png));
        filebutton->flags |= HAS_TOOLTIP;
        add_tooltip(filebutton, "Load audio file");
        filebutton->func.user_callback = dialog_response;
        commonWidgetSettings(filebutton);

        Attack = add_knob(w, "Attack",60,145,38,38);
        Attack->scale.gravity = SOUTHWEST;
        Attack->flags |= HAS_TOOLTIP;
        add_tooltip(Attack, "Attack");
        set_adjustment(Attack->adj, 0.01, 0.01, 0.001, 5.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Attack, (Color_state)1, (Color_mod)2, 0.894, 0.106, 0.623, 1.0);
        Attack->func.expose_callback = draw_knob;
        Attack->func.value_changed_callback = attack_callback;
        commonWidgetSettings(Attack);

        Decay = add_knob(w, "Decay",100,145,38,38);
        Decay->scale.gravity = SOUTHWEST;
        Decay->flags |= HAS_TOOLTIP;
        add_tooltip(Decay, "Decay");
        set_adjustment(Decay->adj, 0.1, 0.1, 0.005, 5.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Decay, (Color_state)1, (Color_mod)2, 0.902, 0.098, 0.117, 1.0);
        Decay->func.expose_callback = draw_knob;
        Decay->func.value_changed_callback = decay_callback;
        commonWidgetSettings(Decay);

        Sustain = add_knob(w, "Sustain",140,145,38,38);
        Sustain->scale.gravity = SOUTHWEST;
        Sustain->flags |= HAS_TOOLTIP;
        add_tooltip(Sustain, "Sustain");
        set_adjustment(Sustain->adj, 0.8, 0.8, 0.001, 1.0, 0.01, CL_CONTINUOS);
        set_widget_color(Sustain, (Color_state)1, (Color_mod)2, 0.377, 0.898, 0.109, 1.0);
        Sustain->func.expose_callback = draw_knob;
        Sustain->func.value_changed_callback = sustain_callback;
        commonWidgetSettings(Sustain);

        Release = add_knob(w, "Release",180,145,38,38);
        Release->scale.gravity = SOUTHWEST;
        Release->flags |= HAS_TOOLTIP;
        add_tooltip(Release, "Release");
        set_adjustment(Release->adj, 0.3, 0.3, 0.005, 10.0, 0.01, CL_LOGARITHMIC);
        set_widget_color(Release, (Color_state)1, (Color_mod)2, 0.486, 0.106, 0.894, 1.0);
        Release->func.expose_callback = draw_knob;
        Release->func.value_changed_callback = release_callback;
        commonWidgetSettings(Release);

        Frequency = add_valuedisplay(w, _(" Hz"), 220, 150, 60, 30);
        set_adjustment(Frequency->adj, 440.0, 440.0, 370.0, 453.0, 0.1, CL_CONTINUOS);
        Frequency->scale.gravity = SOUTHWEST;
        Frequency->flags |= HAS_TOOLTIP;
        add_tooltip(Frequency, "Snyth Root Freqency");
        Frequency->func.value_changed_callback = frequency_callback;
        commonWidgetSettings(Frequency);

        Resonance = add_knob(w, "Resonance",285,145,38,38);
        Resonance->scale.gravity = SOUTHWEST;
        Resonance->flags |= HAS_TOOLTIP;
        add_tooltip(Resonance, "Resonance");
        set_adjustment(Resonance->adj, 0.0, 0.0, 0.0, 127.0, 1.0, CL_CONTINUOS);
        set_widget_color(Resonance, (Color_state)1, (Color_mod)2, 0.95, 0.42, 0.15, 1.0);
        Resonance->func.expose_callback = draw_knob;
        Resonance->func.value_changed_callback = resonance_callback;
        commonWidgetSettings(Resonance);

        CutOff = add_knob(w, "CutOff",325,145,38,38);
        CutOff->scale.gravity = SOUTHWEST;
        CutOff->flags |= HAS_TOOLTIP;
        add_tooltip(CutOff, "CutOff");
        set_adjustment(CutOff->adj, 127.0, 127.0, 0.0, 127.0, 1.0, CL_CONTINUOS);
        set_widget_color(CutOff, (Color_state)1, (Color_mod)2, 0.1, 0.78, 0.85, 1.0);
        CutOff->func.expose_callback = draw_knob;
        CutOff->func.value_changed_callback = cutoff_callback;
        commonWidgetSettings(CutOff);

        clip = add_button(w, "", 365, 148, 35, 35);
        clip->scale.gravity = SOUTHWEST;
        widget_get_png(clip, LDVAR(clip__png));
        clip->flags |= HAS_TOOLTIP;
        add_tooltip(clip, "Clip Sample to clip marks");
        clip->func.value_changed_callback = button_clip_callback;
        commonWidgetSettings(clip);

        playbutton = add_image_toggle_button(w, "", 400, 148, 35, 35);
        playbutton->scale.gravity = SOUTHWEST;
        widget_get_png(playbutton, LDVAR(play_png));
        playbutton->flags |= HAS_TOOLTIP;
        add_tooltip(playbutton, "Play Sample");
        playbutton->func.value_changed_callback = button_playbutton_callback;
        commonWidgetSettings(playbutton);


        setLoopSize = add_knob(lw, "S",135,145,38,38);
        setLoopSize->scale.gravity = SOUTHWEST;
        setLoopSize->flags |= HAS_TOOLTIP;
        add_tooltip(setLoopSize, "Loop Periods");
        set_adjustment(setLoopSize->adj, 1.0, 1.0, 1.0, 12.0, 1.0, CL_CONTINUOS);
        setLoopSize->func.expose_callback = draw_knob;
        setLoopSize->func.value_changed_callback = setLoopSize_callback;
        commonWidgetSettings(setLoopSize);

        setPrevLoop = add_button(lw, "<", 180, 148, 35, 35);
        setPrevLoop->scale.gravity = SOUTHWEST;
        setPrevLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setPrevLoop, "Load previous loop");
        setPrevLoop->func.value_changed_callback = setPrevLoop_callback;
        commonWidgetSettings(setPrevLoop);

        setNextLoop = add_button(lw, ">", 215, 148, 35, 35);
        setNextLoop->scale.gravity = SOUTHWEST;
        setNextLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setNextLoop, "Load next loop");
        setNextLoop->func.value_changed_callback = setNextLoop_callback;
        commonWidgetSettings(setNextLoop);

        Presets = add_button(lw, "Presets", 20, 150, 80, 30);
        Presets->scale.gravity = SOUTHWEST;
        Presets->flags |= HAS_TOOLTIP;
        add_tooltip(Presets, "Load next loop");
        Presets->func.value_changed_callback = presets_callback;
        commonWidgetSettings(Presets);

        Volume = add_knob(lw, "dB",255,145,38,38);
        Volume->scale.gravity = SOUTHWEST;
        Volume->flags |= HAS_TOOLTIP;
        add_tooltip(Volume, "Volume (dB)");
        set_adjustment(Volume->adj, 0.0, 0.0, -20.0, 12.0, 0.1, CL_CONTINUOS);
        set_widget_color(Volume, (Color_state)1, (Color_mod)2, 0.38, 0.62, 0.94, 1.0);
        Volume->func.expose_callback = draw_knob;
        Volume->func.value_changed_callback = volume_callback;
        commonWidgetSettings(Volume);

        setLoop = add_image_toggle_button(lw, "", 300, 148, 35, 35);
        setLoop->scale.gravity = SOUTHWEST;
        widget_get_png(setLoop, LDVAR(loop_png));
        setLoop->flags |= HAS_TOOLTIP;
        add_tooltip(setLoop, "Use Loop Sample");
        setLoop->func.value_changed_callback = button_set_callback;
        commonWidgetSettings(setLoop);

        #ifndef RUN_AS_CLAP_PLUGIN
        w_quit = add_button(lw, "", 390, 148, 35, 35);
        widget_get_png(w_quit, LDVAR(exit__png));
        w_quit->scale.gravity = SOUTHWEST;
        w_quit->flags |= HAS_TOOLTIP;
        add_tooltip(w_quit, "Exit");
        w_quit->func.value_changed_callback = button_quit_callback;
        commonWidgetSettings(w_quit);
        #endif

        keyboard = add_midi_keyboard(w_top, "Organ", 0, 190, 880, 100);
        keyboard->flags |= HIDE_ON_DELETE;
        //keyboard->scale.gravity = SOUTHCENTER;
        keyboard->parent_struct = (void*)this;
        MidiKeyboard* keys = (MidiKeyboard*)keyboard->private_struct;
        Widget_t *view_port = keys->context_menu->childlist->childs[0];
        Widget_t *octavemap = view_port->childlist->childs[1];
        keys->octave = 12*3;
        set_active_radio_entry_num(octavemap, keys->octave/12);
        keys->mk_send_note = get_note;
        keys->mk_send_all_sound_off = all_notes_off;

        #ifndef RUN_AS_CLAP_PLUGIN
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

    Widget_t *Attack;
    Widget_t *Decay;
    Widget_t *Sustain;
    Widget_t *Release;
    Widget_t *Frequency;
    Widget_t *Resonance;
    Widget_t *CutOff;

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
    int useLoop;

/****************************************************************
                    Create loop samples
****************************************************************/

    bool getNextLoop(int num) {
        if (num < 0 || num >= matches) return false;
        LoopGenerator::LoopInfo loopinfo;
        loopBuffer.clear();
        if (lg.getNextMatch(af.samples, af.samplesize , af.channels, 
            freq, loopBuffer, loopinfo, num)) {
                loopPoint_l_auto = loopinfo.start;
                loopPoint_r_auto = loopinfo.end;
                currentLoop = num;
                return true;
            }
        return false;
    }

    void getPitch() {
        freq = 0.0;
        pitchCorrection = 0;
        rootkey = 0;
        matches = 0;
        if (af.samples) rootkey = pt.getPitch(af.samples, af.samplesize , af.channels, (float)jack_sr, &pitchCorrection, &freq);
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
                    Load samples into synth
****************************************************************/

    void setOneShootBank() {
        if (!af.samples) return;
        getPitch();
        sbank.clear();
        sampleBuffer.clear();
        sampleBuffer.resize(af.samplesize);
        float maxAbs = 0.0f;
        for (uint32_t i = 0; i < af.samplesize; i++) {
            float a = std::fabs(af.samples[i * af.channels]);
            if (a > maxAbs) maxAbs = a;
        }
        float gain = 1.0f/maxAbs;
        for (uint32_t i = 0; i < af.samplesize; i++) {
            sampleBuffer[i] = af.samples[i * af.channels] * gain;
        }
        sampleData.data = sampleBuffer;
        sampleData.sourceRate = (double)jack_sr;
        sampleData.rootFreq = (double) freq;
        sbank.addSample(sampleData);
        synth.setBank(&sbank);
    }

    void setLoopBank() {
        if (!loopBuffer.size()) return;
        lbank.clear();
        loopData.data = loopBuffer;
        loopData.sourceRate = (double)jack_sr;
        loopData.rootFreq = (double)freq;
        lbank.addSample(loopData);
        synth.setLoopBank(&lbank);
        
    }

    void setBank() {
        setOneShootBank();
        setLoopBank();
        synth.setLoop(true);
    }

    void setLoopToBank() {
        // get max abs amplitude for normalisation
        float maxAbs = 0.0f;
        for (size_t i = 0; i < loopBuffer.size(); ++i) {
            float a = std::fabs(loopBuffer[i]);
            if (a > maxAbs) maxAbs = a;
        }
        // normalise loop buffer
        float gain = 0.6f/maxAbs;
        for (size_t i = 0; i < loopBuffer.size(); ++i) {
            loopBuffer[i] *=gain;
        }
        loadLoopNew = true;
        play_loop = true;
        if (guiIsCreated) {
            update_waveview(loopview, loopBuffer.data(), loopBuffer.size());
            uint32_t length = loopPoint_r_auto - loopPoint_l_auto;
            std::string tittle = "loopino: loop size " + std::to_string(length)
                                + " Samples | Key Note " + keys[loopRootkey]
                                + " | loop " + std::to_string(currentLoop)
                                + " from " + std::to_string(matches - 1);
            widget_set_title(w_top, tittle.data());
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
        uint32_t new_size = (loopPoint_r-loopPoint_l) * af.channels;
        delete[] af.saveBuffer;
        af.saveBuffer = nullptr;
        af.saveBuffer = new float[new_size];
        std::memset(af.saveBuffer, 0, new_size * sizeof(float));
        for (uint32_t i = 0; i<new_size; i++) {
            af.saveBuffer[i] = af.samples[i+loopPoint_l];
        }
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
        loadNew = true;
        update_waveview(wview, af.samples, af.samplesize);
        if (adj_get_value(playbutton->adj))
             play = true;
        ready = true;
        setOneShootBank();
        button_setLoop_callback(setLoop, NULL);
    }

/****************************************************************
                    Sound File loading
****************************************************************/

    // when Sound File loading fail, clear wave view and reset tittle
    void failToLoad() {
        loadNew = true;
        update_waveview(wview, af.samples, af.samplesize);
        widget_set_title(w_top, "loopino");
    }

    // load a Sound File when pre-load is the wrong file
    void load_soundfile(const char* file) {
        af.channels = 0;
        af.samplesize = 0;
        af.samplerate = 0;
        position = 0;

        ready = false;
        play_loop = false;
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
            instrument_name = p.stem();
            adj_set_max_value(wview->adj, (float)af.samplesize);
            adj_set_state(loopMark_L->adj, 0.0);
            loopPoint_l = 0;
            adj_set_state(loopMark_R->adj,1.0);
            loopPoint_r = af.samplesize;
            loadLoopNew = true;
            update_waveview(wview, af.samples, af.samplesize);
            setOneShootBank();
            button_setLoop_callback(setLoop, NULL);
        } else {
            af.samplesize = 0;
            std::cerr << "Error: could not resample file" << std::endl;
            failToLoad();
        }
        ready = true;
    }

/****************************************************************
            drag and drop handling for the main window
****************************************************************/

    static void dnd_load_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        if (user_data != NULL) {
            char* dndfile = NULL;
            dndfile = strtok(*(char**)user_data, "\r\n");
            while (dndfile != NULL) {
                if (self->supportedFormats.isSupported(dndfile) ) {
                    self->filename = dndfile;
                    self->loadFile();
                    break;
                } else {
                    std::cerr << "Unrecognized file extension: " << dndfile << std::endl;
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
        if (on_off == 0x90) {
            self->synth.noteOn((int)(*key), 0.8f);              
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
        if (self->af.samples) button_setLoop_callback(self->setLoop, NULL);
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

    // volume control
    static void volume_callback(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        Loopino *self = static_cast<Loopino*>(w->parent_struct);
        self->volume = adj_get_value(w->adj);
        self->markDirty(5);
        self->gain = std::pow(1e+01, 0.05 * self->volume);
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
            .frame  = { 0.100, 0.100, 0.100, 0.400 },
            .light  = { 0.150, 0.150, 0.150, 0.400 }
        };
    }

/****************************************************************
                      drawings 
****************************************************************/

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
        header.version = 3; // guard for future proof
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
        if (header.version > 3) {
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

        readSampleBuffer(in, af.samples, af.samplesize);
        in.close();
        adj_set_max_value(wview->adj, (float)af.samplesize);
        adj_set_state(loopMark_L->adj, 0.0);
        adj_set_state(loopMark_R->adj,1.0);
        loadLoopNew = true;
        loadNew = true;
        update_waveview(wview, af.samples, af.samplesize);
        loadPresetToSynth();
        std::filesystem::path p = filename;
        presetName = p.stem();
        std::string tittle = "loopino: " + presetName;
        widget_set_title(w_top, tittle.data());
        return true;
    }

};

#endif

