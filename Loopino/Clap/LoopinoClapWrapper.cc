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
        param.registerParam("Resonance", "Synth", 0.0, 127.0, 0.0, 1.0,   (void*)&resonance,   false,  IS_FLOAT);
        param.registerParam("Cutoff",    "Synth", 0.0, 127.0, 127.0, 1.0, (void*)&cutoff,      false,  IS_FLOAT);
        param.registerParam("Sharp",     "Synth", 0.0, 1.0, 1.0, 0.01,    (void*)&sharp,       false,  IS_FLOAT);
        param.registerParam("Saw",       "Synth", 0.0, 1.0, 1.0, 0.01,    (void*)&saw,         false,  IS_FLOAT);
        param.registerParam("FadeOut",   "Synth", 0.0, 1.0, 1.0, 0.01,    (void*)&fadeout,     false,  IS_FLOAT);
        param.registerParam("PmFreq",    "Synth", 0.01, 200.0, 0.01, 0.01,(void*)&pmfreq,      false,  IS_FLOAT);
        param.registerParam("PmDepth",   "Synth", 0.0, 1.0, 1.0, 0.01,    (void*)&pmdepth,     false,  IS_FLOAT);
        param.registerParam("PmMode",    "Synth", 0, 3, 1, 1,             (void*)&pmmode,      true,   IS_INT);
        param.registerParam("VibDepth",  "Synth", 0.0, 1.0, 0.0, 0.01,    (void*)&vibdepth,    false,  IS_FLOAT);
        param.registerParam("VibRate",   "Synth", 0.1, 12.0, 5.0, 0.01,   (void*)&vibrate,     false,  IS_FLOAT);
        param.registerParam("TremDepth", "Synth", 0.0, 1.0, 0.0, 0.01,    (void*)&tremdepth,   false,  IS_FLOAT);
        param.registerParam("TremRate",  "Synth", 0.1, 15.0, 5.0, 0.01,   (void*)&tremrate,    false,  IS_FLOAT);
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
        } else {
            synth.setAttack(attack);
            synth.setDecay(decay);
            synth.setSustain(sustain);
            synth.setRelease(release);
            synth.setRootFreq(frequency);
            synth.setLoop(useLoop);
            gain = std::pow(1e+01, 0.05 * volume);
            synth.setReso(resonance);
            synth.setCutoff(cutoff);
            synth.setPmFreq(pmfreq);
            synth.setPmDepth(pmdepth);
            synth.setPmMode(pmmode);
            synth.setvibDepth(vibdepth);
            synth.setvibRate(vibrate);
            synth.settremDepth(tremdepth);
            synth.settremRate(tremrate);
        }
    }

    void startGui(Window window) {
        main_init(&app);
        set_custom_theme(&app);
        int w = 880;
        int h = 390;
        #if defined(_WIN32)
        w_top  = create_window(&app, (HWND) window, 0, 0, w, h);
        #else
        w_top  = create_window(&app, (Window) window, 0, 0, w, h);
        #endif
        w_top->flags |= HIDE_ON_DELETE;
        createGUI(&app);
        fetch.startTimeout(60);
        fetch.set<Loopino, &Loopino::runGui>(this);
    }

    void startGui() {
        main_init(&app);
        set_custom_theme(&app);
        int w = 880;
        int h = 390;
        w_top  = create_window(&app, os_get_root_window(&app, IS_WINDOW), 0, 0, w, h);
        w_top->flags |= HIDE_ON_DELETE;
        createGUI(&app);
        fetch.startTimeout(60);
        fetch.set<Loopino, &Loopino::runGui>(this);
    }
    

    void showGui() {
        firstLoop = true;
        widget_show_all(w_top);
        setValuesFromHost();
        if (havePresetToLoad) {
            adj_set_max_value(wview->adj, (float)af.samplesize);
            adj_set_state(loopMark_L->adj, 0.0);
            adj_set_state(loopMark_R->adj,1.0);
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
        if ((host_width != width && host_width != 1) ||
            (host_height != height && host_height != 1)) {
            os_resize_window(app.dpy, w_top, host_width, host_height);
        }
        #endif
    }

    void hideGui() {
        firstLoop = false;
        widget_hide(w_top);
    }

    void quitGui() {
        fetch.stop();
        onExit();
    }

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
        if (maxVal < 1e-9f) maxVal = 1.0f;
        
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
        header.version = 7; // guard for future proof
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

        writeSamples(out, af.samples, af.samplesize);
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
        if (header.version > 7) {
            std::cerr << "Warning: newer preset version (" << header.version << ")\n";
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

        readSamples(in, af.samples, af.samplesize);
        havePresetToLoad = true;
        haveDefault = false;
        return true;
    }

