/*
 * LoopinoClapWrapper.ccc
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        Part of the Loopino class to build the Clap wrapper
****************************************************************/

    void registerParameters() {
        //                  name             group   min, max, def, step   value              isStepped  type
        param.registerParam("Attack",     "ADSR", 0.001, 5.0, 0.01, 0.01, (void*)&attack,      false,  IS_FLOAT);
        param.registerParam("Decay",      "ADSR", 0.005, 5.0, 0.1, 0.01,  (void*)&decay,       false,  IS_FLOAT);
        param.registerParam("Sustain",    "ADSR", 0.001, 1.0, 0.8, 0.01,  (void*)&sustain,     false,  IS_FLOAT);
        param.registerParam("Release",    "ADSR", 0.005, 10.0, 0.3, 0.01, (void*)&release,     false,  IS_FLOAT);
        param.registerParam("Frequency", "Synth",220.0, 880.0, 440.0, 0.1,(void*)&frequency,   false,  IS_FLOAT);
        param.registerParam("Volume",    "Synth", -20.0, 6.0, 0.0, 0.1,   (void*)&volume,      false,  IS_FLOAT);
        param.registerParam("Use Loop",  "Synth", 0, 1, 0, 1,             (void*)&useLoop,     true,   IS_INT);
        param.registerParam("Loop Size", "Synth", 1, 12, 1, 1,            (void*)&loopPeriods, true,   IS_INT);
        param.registerParam("Resonance", "Synth", 0.0, 127.0, 68.0, 1.0,  (void*)&resonance,   false,  IS_FLOAT);
        param.registerParam("Cutoff",    "Synth", 0.0, 127.0, 68.0, 1.0,  (void*)&cutoff,      false,  IS_FLOAT);
        param.registerParam("Sharp",     "Synth", 0.0, 1.0, 0.0, 0.01,    (void*)&sharp,       false,  IS_FLOAT);
        param.registerParam("Saw",       "Synth", 0.0, 1.0, 0.0, 0.01,    (void*)&saw,         false,  IS_FLOAT);
        param.registerParam("FadeOut",   "Synth", 0.0, 1.0, 0.0, 0.01,    (void*)&fadeout,     false,  IS_FLOAT);
        param.registerParam("PmFreq",       "PM", 0.01, 30.0, 0.01, 0.01, (void*)&pmfreq,      false,  IS_FLOAT);
        param.registerParam("PmDepth",      "PM", 0.0, 1.0, 0.0, 0.01,    (void*)&pmdepth,     false,  IS_FLOAT);
        param.registerParam("PmMode",       "PM", 0, 3, 1, 1,             (void*)&pmmode,      true,   IS_INT);
        param.registerParam("VibDepth","Vibrato", 0.0, 1.0, 0.6, 0.01,    (void*)&vibdepth,    false,  IS_FLOAT);
        param.registerParam("VibRate", "Vibrato", 0.1, 12.0, 5.0, 0.01,   (void*)&vibrate,     false,  IS_FLOAT);
        param.registerParam("TremDepth","Tremolo",0.0, 1.0, 0.3, 0.01,    (void*)&tremdepth,   false,  IS_FLOAT);
        param.registerParam("TremRate","Tremolo", 0.1, 15.0, 5.0, 0.01,   (void*)&tremrate,    false,  IS_FLOAT);
        param.registerParam("HP Resonance",  "HP",0.0, 127.0, 50.0, 1.0,  (void*)&hpresonance, false,  IS_FLOAT);
        param.registerParam("HP Cutoff",     "HP", 0.0, 127.0, 48.0, 1.0, (void*)&hpcutoff,    false,  IS_FLOAT);
        param.registerParam("Pitch Bend", "Synth", -1.0, 1.0, 0.0, 0.01,  (void*)&pitchwheel,  false,  IS_FLOAT);
        param.registerParam("LP Keytracking","LP", 0.0, 1.0, 1.0, 0.01,  (void*)&lpkeytracking,false,  IS_FLOAT);
        param.registerParam("HP Keytracking","HP", 0.0, 1.0, 1.0, 0.01,  (void*)&hpkeytracking,false,  IS_FLOAT);
        param.registerParam("Velocity Mode","Synth",  0, 2, 1, 1,         (void*)&velmode,      true,  IS_INT);

        //                  name           group   min, max, def, step   value              isStepped  type
        param.registerParam("Obf Mode",     "OBF", -1.0, 1.0, -0.6, 0.01, (void*)&obfmode,     false,  IS_FLOAT);
        param.registerParam("Obf Keytracking","OBF",0.0, 1.0, 0.3, 0.01,(void*)&obfkeytracking,false,  IS_FLOAT);
        param.registerParam("Obf Resonance","OBF",0.0, 0.6, 0.3, 0.01,    (void*)&obfresonance,false,  IS_FLOAT);
        param.registerParam("Obf CutOff",  "OBF",40.0, 12000.6, 200.0, 0.1,(void*)&obfcutoff, false,  IS_FLOAT);
        param.registerParam("Obf On/Off",  "OBF", 0, 1, 0, 1,             (void*)&obfonoff,     true,  IS_INT);
        param.registerParam("LP On/Off" ,   "LP", 0, 1, 0, 1,             (void*)&lponoff,      true,  IS_INT);
        param.registerParam("HP On/Off" ,   "HP", 0, 1, 0, 1,             (void*)&hponoff,      true,  IS_INT);
        param.registerParam("Vibe On/Off","Vibrato",0, 1, 0, 1,           (void*)&vibonoff,     true,  IS_INT);
        param.registerParam("Trem On/Off","Tremolo",0, 1, 0, 1,            (void*)&tremonoff,   true,  IS_INT);
        param.registerParam("Chorus On/Off","Chorus",0, 1, 0, 1,           (void*)&chorusonoff, true,  IS_INT);
        param.registerParam("Chorus Level","Chorus", 0.0, 1.0, 0.5, 0.01,  (void*)&choruslev,  false,  IS_FLOAT);
        param.registerParam("Chorus Delay","Chorus", 0.0, 0.2, 0.02, 0.001,(void*)&chorusdelay,false, IS_FLOAT);
        param.registerParam("Chorus Depth","Chorus", 0.0, 1.0, 0.02, 0.001,(void*)&chorusdepth,false,  IS_FLOAT);
        param.registerParam("Chorus Freq", "Chorus", 0.1, 10.0, 3.0, 0.001,(void*)&chorusfreq, false,  IS_FLOAT);
        param.registerParam("Reverb On/Off","Reverb",0, 1, 0, 1,           (void*)&revonoff,    true,  IS_INT);
        param.registerParam("Reverb Room",  "Reverb",0.0, 1.0, 0.0, 0.01, (void*)&revroomsize, false,  IS_FLOAT);
        param.registerParam("Reverb Damp",  "Reverb",0.0, 1.0, 0.25, 0.01,(void*)&revdamp,     false,  IS_FLOAT);
        param.registerParam("Reverb Mix",  "Reverb",0.0, 100.0, 50.0, 1.0,(void*)&revmix,      false,  IS_FLOAT);
        param.registerParam("Wasp On/Off", "Wasp",0, 1, 0, 1,             (void*)&wasponoff,    true,  IS_INT);
        param.registerParam("Wasp Mix",    "Wasp",-1.0, 1.0, 0.0, 0.01,   (void*)&waspmix,     false,  IS_FLOAT);
        param.registerParam("Wasp Resonance","Wasp",0.0, 1.0, 0.4, 0.01, (void*)&waspresonance,false,  IS_FLOAT);
        param.registerParam("Wasp CutOff","Wasp",40.0, 12000.0, 1000.0, 0.01,(void*)&waspcutoff,false, IS_FLOAT);
        param.registerParam("Wasp Keytracking","Wasp",0.0, 1.0, 0.5, 0.01,(void*)&waspkeytracking,false,IS_FLOAT);
        param.registerParam("TB On/Off", "LM_ACD18",0, 1, 0, 1,              (void*)&tbonoff,      true,  IS_INT);
        param.registerParam("TB Vintage","LM_ACD18",0.0, 1.0, 0.3, 0.01,    (void*)&tbvintage,   false,   IS_FLOAT);
        param.registerParam("TB Resonance","LM_ACD18",0.0, 1.0, 0.3, 0.01,  (void*)&tbresonance, false,   IS_FLOAT);
        param.registerParam("TB CutOff","LM_ACD18",40.0, 12000.0, 880.0, 0.01,(void*)&tbcutoff,   false,  IS_FLOAT);
        param.registerParam("Tone",     "Synth", -1.0, 1.0, 0.0, 0.01,       (void*)&tone,        false,  IS_FLOAT);
      //  param.registerParam("Age",      "Synth", 0.0, 1.0, 0.25, 0.01,    (void*)&age,          false,  IS_FLOAT);
        param.registerParam("LM_MIR8 On/Off", "Machine",0, 1, 0, 1,          (void*)&mrgonoff,     true,  IS_INT);
        param.registerParam("LM_MIR8 Drive","Machine",0.25, 1.5, 1.3, 0.01,  (void*)&mrgdrive,    false,  IS_FLOAT);
        param.registerParam("LM_MIR8 Amount","Machine",0.1, 1.0, 0.25, 0.01, (void*)&mrgamount,   false,  IS_FLOAT);
        param.registerParam("Emu_12 On/Off", "Machine",0, 1, 0, 1,           (void*)&emu_12onoff,  true,  IS_INT);
        param.registerParam("Emu_12 Drive","Machine",0.25, 2.5, 1.2, 0.01,   (void*)&emu_12drive, false,  IS_FLOAT);
        param.registerParam("Emu_12 Amount","Machine",0.1, 1.0, 1.0, 0.01,   (void*)&emu_12amount,false,  IS_FLOAT);
        param.registerParam("LM_CMP12 On/Off", "Machine",0, 1, 0, 1,         (void*)&cmp12onoff,   true,  IS_INT);
        param.registerParam("LM_CMP12 Drive","Machine",0.25, 2.5, 1.0, 0.01, (void*)&cmp12drive,  false,  IS_FLOAT);
        param.registerParam("LM_CMP12 Ratio","Machine",0.1, 1.0, 1.65, 0.01, (void*)&cmp12ratio,  false,  IS_FLOAT);
        param.registerParam("Studio16 On/Off","Machine",0, 1, 0, 1,          (void*)&studio16onoff, true, IS_INT);
        param.registerParam("Studio16 Drive","Machine",0.25, 1.5, 1.1, 0.01, (void*)&studio16drive,false, IS_FLOAT);
        param.registerParam("Studio16 Warmth","Machine",0.0, 1.0, 0.65, 0.01,(void*)&studio16warmth,false,IS_FLOAT);
        param.registerParam("Studio16 HfTilt","Machine",0.0, 1.0, 0.45, 0.01,(void*)&studio16hftilt,false,IS_FLOAT);
        param.registerParam("EPS On/Off",     "Machine",0, 1, 0, 1,          (void*)&epsonoff,      true, IS_INT);
        param.registerParam("EPS Drive",      "Machine",0.25, 1.5, 1.0, 0.01,(void*)&epsdrive,     false, IS_FLOAT);
        param.registerParam("Time On/Off",    "Machine",0, 1, 0, 1,          (void*)&tmonoff,       true, IS_INT);
        param.registerParam("Time ",          "Machine",0.0, 1.0, 0.2, 0.01, (void*)&tmtime,       false, IS_FLOAT);
        param.registerParam("Reverse",       "Machine",0, 1, 0, 1,           (void*)&reverse,       true, IS_INT);
        param.registerParam("UnisonKeys",    "Machine",0, 1, 0, 1,           (void*)&genrateKeyCache,true,IS_INT);
    }

    void setValuesFromHost() {
        if (guiIsCreated) {
            adj_set_value(Attack->adj, attack);
            adj_set_value(Decay->adj, decay);
            adj_set_value(Sustain->adj, sustain);
            adj_set_value(Release->adj, release);
            adj_set_value(Frequency->adj, frequency);
            adj_set_value(Volume->adj, volume);
            adj_set_value(setLoop->adj, (float)useLoop);
            adj_set_value(setLoopSize->adj, (float)loopPeriods);
            adj_set_value(Resonance->adj, resonance);
            adj_set_value(CutOff->adj, cutoff);
            adj_set_value(Sharp->adj, sharp);
            adj_set_value(Saw->adj, saw);
            adj_set_value(FadeOut->adj, fadeout);
            adj_set_value(PmFreq->adj, pmfreq);
            adj_set_value(PmDepth->adj, pmdepth);
            radio_box_set_active(PmMode[pmmode]);
            adj_set_value(VibDepth->adj, vibdepth);
            adj_set_value(VibRate->adj, vibrate);
            adj_set_value(TremDepth->adj, tremdepth);
            adj_set_value(TremRate->adj, tremrate);
            adj_set_value(HpResonance->adj, hpresonance);
            adj_set_value(HpCutOff->adj, hpcutoff);
            wheel_set_value(PitchWheel,pitchwheel);
            wheel_set_value(LpKeyTracking, (lpkeytracking * 2.0f) - 1.0f);
            wheel_set_value(HpKeyTracking, (hpkeytracking * 2.0f) - 1.0f);
            velocity_box_set_active(VelMode[velmode]);
            adj_set_value(ObfMode->adj, obfmode);
            adj_set_value(ObfOnOff->adj, (float)obfonoff);
            adj_set_value(LpOnOff->adj, (float)lponoff);
            adj_set_value(HpOnOff->adj, (float)hponoff);
            wheel_set_value(ObfKeyTracking, (obfkeytracking -0.3) * 3.33333);
            adj_set_value(ObfResonance->adj, obfresonance);
            adj_set_value(ObfCutOff->adj, obfcutoff);
            adj_set_value(VibOnOff->adj, (float)vibonoff);
            adj_set_value(TremOnOff->adj, (float)tremonoff);

            adj_set_value(ChorusOnOff->adj, (float)chorusonoff);
            adj_set_value(ChorusLev->adj, choruslev);
            adj_set_value(ChorusDelay->adj, chorusdelay);
            adj_set_value(ChorusDepth->adj, chorusdepth);
            adj_set_value(ChorusFreq->adj, chorusfreq);
            adj_set_value(RevOnOff->adj, (float)revonoff);
            adj_set_value(RevRoomSize->adj, revroomsize);
            adj_set_value(RevDamp->adj, revdamp);
            adj_set_value(RevMix->adj, revmix);

            adj_set_value(WaspOnOff->adj, (float)wasponoff);
            adj_set_value(WaspMix->adj, waspmix);
            adj_set_value(WaspResonance->adj, waspresonance);
            adj_set_value(WaspCutOff->adj, waspcutoff);
            wheel_set_value(WaspKeyTracking, (waspkeytracking * 2.0f) - 1.0f);

            adj_set_value(TBOnOff->adj, (float)tbonoff);
            adj_set_value(TBVintage->adj, tbvintage);
            adj_set_value(TBResonance->adj, tbresonance);
            adj_set_value(TBCutOff->adj, tbcutoff);
            adj_set_value(Tone->adj, tone);
           // adj_set_value(Age->adj, age);
            adj_set_value(LM_MIR8OnOff->adj, (float)mrgonoff);
            adj_set_value(LM_MIR8Drive->adj, mrgdrive);
            adj_set_value(LM_MIR8Amount->adj, mrgamount);
            adj_set_value(Emu_12OnOff->adj, (float)emu_12onoff);
            adj_set_value(Emu_12Drive->adj, emu_12drive);
            adj_set_value(Emu_12Amount->adj, emu_12amount);
            adj_set_value(LM_CMP12OnOff->adj, (float)cmp12onoff);
            adj_set_value(LM_CMP12Drive->adj, cmp12drive);
            adj_set_value(LM_CMP12Ratio->adj, cmp12ratio);
            adj_set_value(Studio_16OnOff->adj, (float)studio16onoff);
            adj_set_value(Studio_16Drive->adj, studio16drive);
            adj_set_value(Studio_16Warmth->adj, studio16warmth);
            adj_set_value(Studio_16HfTilt->adj, studio16hftilt);
            adj_set_value(EPSOnOff->adj, (float)epsonoff);
            adj_set_value(EPSDrive->adj, epsdrive);
            adj_set_value(TMOnOff->adj, (float)tmonoff);
            adj_set_value(TMTime->adj, tmtime);
            adj_set_value(Reverse->adj, (float)reverse);
            adj_set_value(GenKeyCache->adj, (float)genrateKeyCache);

            expose_widget(LpKeyTracking);
            expose_widget(HpKeyTracking);
            expose_widget(ObfKeyTracking);
            expose_widget(WaspKeyTracking);
            expose_widget(PitchWheel);
        } else {
            syncValuesToSynth();
        }
    }
    void syncValuesToSynth() {
        synth.setAttack(attack);
        synth.setDecay(decay);
        synth.setSustain(sustain);
        synth.setRelease(release);
        synth.setRootFreq(frequency);
        synth.setLoop(useLoop);
        gain = std::pow(1e+01, 0.05 * volume);
        synth.setGain(gain);
        synth.setResoLP(resonance);
        synth.setCutoffLP(cutoff);
        synth.setPmFreq(pmfreq);
        synth.setPmDepth(pmdepth);
        synth.setPmMode(pmmode);
        synth.setvibDepth(vibdepth);
        synth.setvibRate(vibrate);
        synth.settremDepth(tremdepth);
        synth.settremRate(tremrate);
        synth.setResoHP(hpresonance);
        synth.setCutoffHP(hpcutoff);
        synth.setPitchWheel(pitchwheel);
        synth.setLpKeyTracking(lpkeytracking);
        synth.setHpKeyTracking(hpkeytracking);
        synth.setVelMode(velmode);
        synth.setModeObf(obfmode);
        synth.setKeyTrackingObf(obfkeytracking);
        synth.setResonanceObf(obfresonance);
        synth.setCutoffObf(obfcutoff);
        synth.setOnOffObf(obfonoff);
        synth.setOnOffLP(lponoff);
        synth.setOnOffHP(hponoff);
        synth.setOnOffVib(vibonoff);
        synth.setOnOffTrem(tremonoff);
        synth.setChorusOnOff(chorusonoff);
        synth.setChorusLevel(choruslev);
        synth.setChorusDelay(chorusdelay);
        synth.setChorusDepth(chorusdepth);
        synth.setChorusFreq(chorusfreq);
        synth.setReverbOnOff(revonoff);
        synth.setReverbRoomSize(revroomsize);
        synth.setReverbDamp(revdamp);
        synth.setReverbMix(revmix);
        synth.setOnOffWasp(wasponoff);
        synth.setFilterMixWasp(waspmix);
        synth.setResonanceWasp(waspresonance);
        synth.setCutoffWasp(waspcutoff);
        synth.setKeyTrackingWasp(waspkeytracking);
        synth.setTBOnOff(tbonoff);
        synth.setVintageAmountTB(tbvintage);
        synth.setResonanceTB(tbresonance);
        synth.setCutoffTB(tbcutoff);
        synth.setTone(tone);
       // synth.setAge(age);
        synth.setLM_MIR8OnOff(mrgonoff);
        synth.setLM_MIR8Drive(mrgdrive);
        synth.setLM_MIR8Amount(mrgamount);
        synth.setEmu_12OnOff(emu_12onoff);
        synth.setEmu_12Drive(emu_12drive);
        synth.setEmu_12Amount(emu_12amount);
        synth.setLM_CMP12OnOff(cmp12onoff);
        synth.setLM_CMP12Drive(cmp12drive);
        synth.setLM_CMP12Ratio(cmp12ratio);
        synth.setStudio_16OnOff(studio16onoff);
        synth.setStudio_16Drive(studio16drive);
        synth.setStudio_16Warmth(studio16warmth);
        synth.setStudio_16HfTilt(studio16hftilt);
        synth.setVFX_EPSOnOff(epsonoff);
        synth.setVFX_EPSDrive(epsdrive);
        synth.setTMOnOff(tmonoff);
        synth.setTMTime(tmtime);
        synth.setReverse(reverse);
        synth.genCache(toBig ? 0 : genrateKeyCache);
        synth.rebuildMachineChain(machineOrder);
        synth.rebuildFilterChain(filterOrder);
    }

#if defined (RUN_AS_PLUGIN)

    void startGui(Window window) {
        main_init(&app);
        set_custom_theme(&app);
        int w = WINDOW_WIDTH;
        int h = WINDOW_HEIGHT;
        #if defined(_WIN32)
        w_top  = create_window(&app, (HWND) window, 0, 0, w, h);
        #else
        w_top  = create_window(&app, (Window) window, 0, 0, w, h);
        #endif
        w_top->flags |= HIDE_ON_DELETE;
        createGUI(&app);
        //fetch.startTimeout(60);
        //fetch.set<Loopino, &Loopino::runGui>(this);
    }

    void startGui() {
        main_init(&app);
        set_custom_theme(&app);
        int w = WINDOW_WIDTH;
        int h = WINDOW_HEIGHT;
        w_top  = create_window(&app, os_get_root_window(&app, IS_WINDOW), 0, 0, w, h);
        w_top->flags |= HIDE_ON_DELETE;
        createGUI(&app);
        //fetch.startTimeout(60);
        //fetch.set<Loopino, &Loopino::runGui>(this);
    }

    void showGui() {
        firstLoop = true;
        widget_show_all(w_top);
        setValuesFromHost();
        if (havePresetToLoad) {
            adj_set_max_value(wview->adj, (float)af.samplesize);
            adj_set_state(loopMark_L->adj, 0.0);
            adj_set_state(loopMark_R->adj,1.0);
            std::vector<int> rackOrder;
            rackOrder.reserve(filterOrder.size() + machineOrder.size());
            rackOrder.insert(rackOrder.end(), filterOrder.begin(),  filterOrder.end());
            rackOrder.insert(rackOrder.end(), machineOrder.begin(), machineOrder.end());
            sz.applyPresetOrder(rackOrder);
            havePresetToLoad = false;
        }
        loadNew = true;
        update_waveview(wview, af.samples, af.samplesize);
        loadLoopNew = true;
        update_waveview(loopview, loopBuffer.data(), loopBuffer.size());
    }
    
    void setParent(Window window) {
        #if defined(_WIN32)
        SetParent(w_top->widget, (HWND) window);
        #else
        XReparentWindow(app.dpy, w_top->widget, (Window) window, 0, 0);
        #endif
        p = window;
    }

    void checkParentWindowSize(int width, int height) {
        #if defined (IS_VST2)
        if (!p) return;
        int host_width = 1;
        int host_height = 1;
        #if defined(_WIN32)
        RECT rect;
        if (GetClientRect((HWND) p, &rect)) {
            host_width  = rect.right - rect.left;
            host_height = rect.bottom - rect.top;
        }
        #else
        XWindowAttributes attrs;
        if (XGetWindowAttributes(app.dpy, p, &attrs)) {
            host_width  = attrs.width;
            host_height = attrs.height;
        }
        #endif
        if (((host_width < width || host_width > width+40) && host_width != 1) ||
            ((host_height < height || host_height > height+40) && host_height != 1)) {
            os_resize_window(app.dpy, w_top, host_width, host_height);
        }
        #endif
    }

    void hideGui() {
        firstLoop = false;
        widget_hide(w_top);
    }

    void quitGui() {
        //fetch.stop();
        clearValueBindings();
        onExit();
    }

    #ifndef _WIN32
    #if defined (IS_VST2)
    static bool window_has_xdnd_proxy(Display* dpy, Window w) {
        Atom xdnd_proxy = XInternAtom(dpy, "XdndProxy", False);
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char* data = nullptr;

        int status = XGetWindowProperty(dpy, w, xdnd_proxy, 0, 1, False, XA_WINDOW,
                        &actual_type, &actual_format, &nitems, &bytes_after, &data);

        if (data) XFree(data);

        return (status == Success && actual_type == XA_WINDOW &&
                actual_format == 32 && nitems == 1);
    }

    static void set_xdnd_proxy(Display* dpy, Window plugin_window) {
        if (!dpy || !plugin_window) return;
        Atom xdnd_proxy = XInternAtom(dpy, "XdndProxy", False);
        if (xdnd_proxy == None) return;
        Window root, parent;
        Window* children = nullptr;
        unsigned int nchildren = 0;
        Window w = plugin_window;

        while (w != None) {
            XChangeProperty(dpy, w, xdnd_proxy, XA_WINDOW, 32, PropModeReplace,
                            (unsigned char*)&plugin_window, 1);

            if (!XQueryTree(dpy, w, &root, &parent, &children, &nchildren)) break;
            if (children) XFree(children);
            if (parent == root || parent == None) break;
            w = parent;
        }
        XFlush(dpy);
    }
    #endif
    #endif

    void runGui() {
        if (firstLoop) {
            checkParentWindowSize(w_top->width, w_top->height);
            firstLoop = false;
        }
        if (param.paramChanged.load(std::memory_order_acquire)) {
            setValuesFromHost();
            param.paramChanged.store(false, std::memory_order_release);
        }
        run_embedded(w_top->app);
        //updateUI();
    }

    Xputty *getMain() {
        return w_top->app;
    }

    bool writeSamples(StreamOut& out, const float* samples, uint32_t numData) {
        if (!samples || numData == 0) return false;

        out.write(&numData, sizeof(numData));
        float maxVal = 0.0f;
        for (size_t i = 0; i < numData; ++i) {
            maxVal = max(maxVal, std::fabs(samples[i]));
        }
        if (maxVal <  0.9999f) maxVal = 1.0f;
        
        for (size_t i = 0; i < numData; ++i) {
            float normalized = samples[i] / maxVal;
            int16_t encoded = static_cast<int16_t>(std::round(normalized * 32767.0f));
            out.write(&encoded, sizeof(encoded));
        }
        return true;
    }

    void saveState(StreamOut& out) {
        PresetHeader header;
        std::memcpy(header.magic, "LOOPINO", 8);
        header.version = 16; // guard for future proof
        header.dataSize = af.samplesize;
        out.write(&header, sizeof(header));

        out.write(&currentLoop, sizeof(currentLoop));
        out.write(&attack, sizeof(attack));
        out.write(&decay, sizeof(decay));
        out.write(&sustain, sizeof(sustain));
        out.write(&release, sizeof(release));
        out.write(&frequency, sizeof(frequency));
        out.write(&useLoop, sizeof(useLoop));
        out.write(&loopPeriods, sizeof(loopPeriods));
        // since version 3
        out.write(&resonance, sizeof(resonance));
        out.write(&cutoff, sizeof(cutoff));
        // since version 4
        out.write(&sharp, sizeof(sharp));
        // since version 5
        out.write(&saw, sizeof(saw));
        // since version 6
        out.write(&fadeout, sizeof(fadeout));
        // since version 7
        out.write(&pmfreq, sizeof(pmfreq));
        out.write(&pmdepth, sizeof(pmdepth));
        out.write(&pmmode, sizeof(pmmode));
        // since version 8
        out.write(&vibdepth, sizeof(vibdepth));
        out.write(&vibrate, sizeof(vibrate));
        out.write(&tremdepth, sizeof(tremdepth));
        out.write(&tremrate, sizeof(tremrate));
        // since version 9
        out.write(&hpresonance, sizeof(hpresonance));
        out.write(&hpcutoff, sizeof(hpcutoff));
        // since version 10
        out.write(&lpkeytracking, sizeof(lpkeytracking));
        out.write(&hpkeytracking, sizeof(hpkeytracking));
        out.write(&velmode, sizeof(velmode));
        // since version 11
        out.write(&volume, sizeof(volume));
        out.write(&obfmode, sizeof(obfmode)); 
        out.write(&obfkeytracking, sizeof(obfkeytracking));
        out.write(&obfresonance, sizeof(obfresonance)); 
        out.write(&obfcutoff, sizeof(obfcutoff)); 
        out.write(&obfonoff, sizeof(obfonoff)); 
        out.write(&lponoff, sizeof(lponoff)); 
        out.write(&hponoff, sizeof(hponoff)); 
        out.write(&vibonoff, sizeof(vibonoff)); 
        out.write(&tremonoff, sizeof(tremonoff)); 

        out.write(&chorusonoff, sizeof(chorusonoff));
        out.write(&choruslev, sizeof(choruslev));
        out.write(&chorusdelay, sizeof(chorusdelay));
        out.write(&chorusdepth, sizeof(chorusdepth));
        out.write(&chorusfreq, sizeof(chorusfreq));
        out.write(&revonoff, sizeof(revonoff));
        out.write(&revroomsize, sizeof(revroomsize));
        out.write(&revdamp, sizeof(revdamp));
        out.write(&revmix, sizeof(revmix));
        // since version 12
        out.write(&wasponoff, sizeof(wasponoff));
        out.write(&waspmix, sizeof(waspmix));
        out.write(&waspresonance, sizeof(waspresonance));
        out.write(&waspcutoff, sizeof(waspcutoff));
        out.write(&waspkeytracking, sizeof(waspkeytracking));
        // since version 14
        out.write(&tbonoff, sizeof(tbonoff));
        out.write(&tbvintage, sizeof(tbvintage));
        out.write(&tbresonance, sizeof(tbresonance));
        out.write(&tbcutoff, sizeof(tbcutoff));
        out.write(&tone, sizeof(tone));

        out.write(&mrgonoff, sizeof(mrgonoff));
        out.write(&mrgdrive, sizeof(mrgdrive));
        out.write(&mrgamount, sizeof(mrgamount));
        out.write(&emu_12onoff, sizeof(emu_12onoff));
        out.write(&emu_12drive, sizeof(emu_12drive));
        out.write(&emu_12amount, sizeof(emu_12amount));
        out.write(&cmp12onoff, sizeof(cmp12onoff));
        out.write(&cmp12drive, sizeof(cmp12drive));
        out.write(&cmp12ratio, sizeof(cmp12ratio));
        out.write(&studio16onoff, sizeof(studio16onoff));
        out.write(&studio16drive, sizeof(studio16drive));
        out.write(&studio16warmth, sizeof(studio16warmth));
        out.write(&studio16hftilt, sizeof(studio16hftilt));
        out.write(&epsonoff, sizeof(epsonoff));
        out.write(&epsdrive, sizeof(epsdrive));
        // since version 15
        out.write(&tmonoff, sizeof(tmonoff));
        out.write(&tmtime, sizeof(tmtime));
        out.write(&reverse, sizeof(reverse));
        for (auto x : filterOrder)
            out.write(&x, sizeof(x));
        for (auto x : machineOrder)
            out.write(&x, sizeof(x));
        // since version 16
        out.write(&genrateKeyCache, sizeof(genrateKeyCache));

        writeSamples(out, af.samples, af.samplesize);
        // since version 13
        out.write(&jack_sr, sizeof(jack_sr));
    }

    bool readSamples(StreamIn& in, float*& samples, uint32_t& numData) {
        in.read(&numData, sizeof(numData));
        if (numData == 0) return false;
        delete[] samples;
        samples = nullptr;
        samples = new float[numData];
        
        for (size_t i = 0; i < numData; ++i) {
            int16_t encoded;
            in.read(&encoded, sizeof(encoded));
            samples[i] = static_cast<float>(encoded) / 32767.0f;
        }
        return true;
    }

    bool readState(StreamIn& in) {
        PresetHeader header{};
        in.read(&header, sizeof(header));
        if (std::strncmp(header.magic, "LOOPINO", 7) != 0) {
            std::cerr << "Invalid preset file\n";
            return false;
        }

        // we need to update the header version when change the preset format
        // then we could protect new values with a guard by check the header version
        if (header.version > 16) {
            std::cerr << "Warning: newer preset version (" << header.version << ")\n";
            return false;
        }

        af.channels = 1;
        in.read(&currentLoop, sizeof(currentLoop));
        in.read(&attack, sizeof(attack));
        in.read(&decay, sizeof(decay));
        in.read(&sustain, sizeof(sustain));
        in.read(&release, sizeof(release));
        in.read(&frequency, sizeof(frequency));
        in.read(&useLoop, sizeof(useLoop));
        in.read(&loopPeriods, sizeof(loopPeriods));
        if (header.version > 2) {
            in.read(&resonance, sizeof(resonance));
            in.read(&cutoff, sizeof(cutoff));
        }
        if (header.version > 3) {
            in.read(&sharp, sizeof(sharp));
        }
        if (header.version > 4) {
            in.read(&saw, sizeof(saw));
        }
        if (header.version > 5) {
            in.read(&fadeout, sizeof(fadeout));
        }
        if (header.version > 6) {
            in.read(&pmfreq, sizeof(pmfreq));
            in.read(&pmdepth, sizeof(pmdepth));
            in.read(&pmmode, sizeof(pmmode));
        }
        if (header.version > 7) {
            in.read(&vibdepth, sizeof(vibdepth));
            in.read(&vibrate, sizeof(vibrate));
            in.read(&tremdepth, sizeof(tremdepth));
            in.read(&tremrate, sizeof(tremrate));
        }
        if (header.version > 8) {
            in.read(&hpresonance, sizeof(hpresonance));
            in.read(&hpcutoff, sizeof(hpcutoff));
        }
        if (header.version > 9) {
            in.read(&lpkeytracking, sizeof(lpkeytracking));
            in.read(&hpkeytracking, sizeof(hpkeytracking));
            in.read(&velmode, sizeof(velmode));
        }
        if (header.version > 10) {
            in.read(&volume, sizeof(volume));
            in.read(&obfmode, sizeof(obfmode));
            in.read(&obfkeytracking, sizeof(obfkeytracking));
            in.read(&obfresonance, sizeof(obfresonance));
            in.read(&obfcutoff, sizeof(obfcutoff));
            in.read(&obfonoff, sizeof(obfonoff));
            in.read(&lponoff, sizeof(lponoff));
            in.read(&hponoff, sizeof(hponoff));
            in.read(&vibonoff, sizeof(vibonoff));
            in.read(&tremonoff, sizeof(tremonoff));

            in.read(&chorusonoff, sizeof(chorusonoff));
            in.read(&choruslev, sizeof(choruslev));
            in.read(&chorusdelay, sizeof(chorusdelay));
            in.read(&chorusdepth, sizeof(chorusdepth));
            in.read(&chorusfreq, sizeof(chorusfreq));
            in.read(&revonoff, sizeof(revonoff));
            in.read(&revroomsize, sizeof(revroomsize));
            in.read(&revdamp, sizeof(revdamp));
            in.read(&revmix, sizeof(revmix));
        }
        if (header.version > 11) {
            in.read(&wasponoff, sizeof(wasponoff));
            in.read(&waspmix, sizeof(waspmix));
            in.read(&waspresonance, sizeof(waspresonance));
            in.read(&waspcutoff, sizeof(waspcutoff));
            in.read(&waspkeytracking, sizeof(waspkeytracking));
        }
        if (header.version > 13) {
            in.read(&tbonoff, sizeof(tbonoff));
            in.read(&tbvintage, sizeof(tbvintage));
            in.read(&tbresonance, sizeof(tbresonance));
            in.read(&tbcutoff, sizeof(tbcutoff));
            in.read(&tone, sizeof(tone));

            in.read(&mrgonoff, sizeof(mrgonoff));
            in.read(&mrgdrive, sizeof(mrgdrive));
            in.read(&mrgamount, sizeof(mrgamount));
            in.read(&emu_12onoff, sizeof(emu_12onoff));
            in.read(&emu_12drive, sizeof(emu_12drive));
            in.read(&emu_12amount, sizeof(emu_12amount));
            in.read(&cmp12onoff, sizeof(cmp12onoff));
            in.read(&cmp12drive, sizeof(cmp12drive));
            in.read(&cmp12ratio, sizeof(cmp12ratio));
            in.read(&studio16onoff, sizeof(studio16onoff));
            in.read(&studio16drive, sizeof(studio16drive));
            in.read(&studio16warmth, sizeof(studio16warmth));
            in.read(&studio16hftilt, sizeof(studio16hftilt));
            in.read(&epsonoff, sizeof(epsonoff));
            in.read(&epsdrive, sizeof(epsdrive));
        }
        if (header.version > 14) {
            in.read(&tmonoff, sizeof(tmonoff));
            in.read(&tmtime, sizeof(tmtime));
            in.read(&reverse, sizeof(reverse));
            for (auto& x : filterOrder)
                in.read(&x, sizeof(x));
            for (auto& x : machineOrder)
                in.read(&x, sizeof(x));
        }
        if (header.version > 15) {
            in.read(&genrateKeyCache, sizeof(genrateKeyCache));
        }

        readSamples(in, af.samples, af.samplesize);
        if (header.version > 12) {
            uint32_t sampleRate = jack_sr;
            in.read(&sampleRate, sizeof(sampleRate));
            if (sampleRate != jack_sr) {
                af.checkSampleRate(&af.samplesize, 1, af.samples, sampleRate, jack_sr);
            }
        }
        havePresetToLoad = true;
        return true;
    }

#endif
