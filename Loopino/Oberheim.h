
/*
 * Oberheim.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#pragma once
#include <cmath>

struct LFO {
    float phase = 0.0f;
    float inc   = 0.0f;

    void setFreq(float hz, float sr) {
        inc = hz / sr;
    }

    inline float process() {
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        return sinf(2.0f * M_PI * phase);
    }
};


struct SEMFilter {

    float sampleRate = 44100.0f;
    // Base params
    float cutoff    = 1000.0f; // 40.0f  …  12000.0f log
    float resonance = 0.3f;    // 0.0f  …  0.6f
    float keytrack  = 0.3f;    // 0.0f … 0.6f
    int   midiNote  = 69;      // 0 - 127
    bool onOff      = false;   // off
    float mode      = 0.0f;    // 0.0f  …  1.0f

    // Internal
    float g = 0.0f;
    float R = 0.0f;
    float lp = 0.0f;
    float bp = 0.0f;
    float freqComp = 0.0f;
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    float fadedStep = 0.0f;
    bool  targetOn = false; 

    void setSampleRate(float sr) {
        sampleRate = sr;
        fadeStep = 1.0f / (0.02f * sampleRate);
        fadedStep = 1.0f / (0.9f * sampleRate);
    }

    void setCutOff(float c) { cutoff = c; }
    void setResonance(float r) { resonance = r; }
    void setKeyTracking(float k) { keytrack = k; }
    void setMode(float m) { mode = m; }

    void setOnOff(bool on) {
        targetOn = on;
        if (on && !onOff) {
            reset();
            onOff = true;
        }
    }

    void recalcFilter(int midiNote_) {
        if (!onOff) return;
        float cutoffHz = cutoff;
        float keyHz = 440.0f * powf(2.0f, (midiNote_ - 69) / 12.0f);
        cutoffHz = cutoffHz * (1.0f - keytrack) + keyHz * keytrack;
        cutoffHz = std::clamp(cutoffHz, 40.0f, 12000.0f);
        g = 2.0f * sinf(M_PI * cutoffHz / sampleRate);
        g = std::min(g, 0.99f);
        float r = std::clamp(resonance, 0.0f, 1.0f);
        freqComp = 0.8f + 0.2f * (cutoffHz / 12000.0f);
        R = 0.5f + r * 1.6f;
    }

    void reset() { lp = bp = 0.0f; }

    inline float saturate(float x) {
        return x / (1.0f + fabsf(x));
    }

    inline float process(float in) {
        if (targetOn) {
            fadeGain = std::min<float>(1.0f, fadeGain + fadeStep);
        } else {
            fadeGain = std::max<float>(0.0f, fadeGain - fadedStep);
            if (fadeGain == 0.0f) {
                onOff = false;
                return in;
            }
        }
        // Chamberlin SVF (SEM-style)
        float hp = in - lp - R * bp;
        bp += g * hp;
        lp += g * bp;

        // SEM mode morph
        float m = std::clamp(mode, 0.0f, 1.0f);
        float bp_norm = saturate(bp * (1.0f + 1.5f * resonance));

        float out = 0.0f;
        if (m < 0.5f) {
        float t = m * 2.0f;
            out = lp * (1.0f - t)
                + bp_norm * t;
        } else {
            float t = (m - 0.5f) * 2.0f;
            out = bp_norm * (1.0f - t)
                + hp * t;
        }
        out *= (1.0f + 0.5f * resonance) * freqComp;
        return in * (1.0f - fadeGain) + out * fadeGain;
    }

};

