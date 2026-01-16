
/*
 * LM_ACD18Filter.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        LM_ACD18Filter.h simulate the tb303 filter
        
****************************************************************/

#pragma once

#include <cmath>
#include <algorithm>

class LM_ACD18Filter {
public:
    void setSampleRate(float sr) {
        sampleRate = sr;
        fadeStep = 1.0f / (0.02f * sampleRate);
        update();
    }

    bool getOnOff() const { return onoff; }

    void dumpOff() {
        targetOn = false;
        onoff = false;
        reset();
        update();
        fadeGain = 0.0f;
    }

    void setOnOff(bool v) {
        targetOn = v;
        if (v && !onoff) {
            onoff = true;
            reset();
            update();
        }
    }

    void setCutoff(float hz) {
        cutoff = std::clamp(hz, 20.0f, sampleRate * 0.45f);
        update();
    }

    void setResonance(float r) {
        resonance = std::clamp(r, 0.0f, 1.1f);
        update();
    }

    void setVintageAmount(float v) {
        vintageAmount = std::clamp(v, 0.0f, 1.0f);
    }

    void reset() {
        z1 = z2 = z3 = 0.0f;
        bassDropEnv = 0.0f;
        driftPhase = 0.0f;
        punchEnv = 1.0f;
        prePunchEnv = 1.0f;
    }

    void noteOn(int midiNote) {
        punchEnv = 1.0f;
        prePunchEnv = 1.0f;
        bassDropEnv = 0.0f;
        noteHz =  440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
        update();
    }

    inline float process(float in) {
        if (targetOn) {
            fadeGain = std::min(1.0f, fadeGain + fadeStep);
        } else {
            fadeGain = std::max(0.0f, fadeGain - fadeStep);
            if (fadeGain == 0.0f) {
                onoff = false;
                return in;
            }
        }
        driftPhase += driftSpeed * vintageAmount;
        if (driftPhase > 1.0f) driftPhase -= 1.0f;
        float drift = std::sin(driftPhase * 2.0f * M_PI);
        float resDrift = 1.0f + drift * 0.03f * vintageAmount;
        prePunchEnv += 0.004f * (0.0f - prePunchEnv);
        float punchDrive = 1.0f + prePunchEnv * 4.0f;
        float x = preSaturate(in * punchDrive, vintageAmount);
        float fb = resonanceGain * resDrift * z3;

        x -= fb;
        z1 += g * (x  - z1);
        z2 += g * (z1 - z2);
        z3 += g * (z2 - z3);

        float resActivity = std::fabs(z3) * resonance;
        bassDropEnv += 0.0008f * (resActivity - bassDropEnv);
        float bassDrop = 1.0f - bassDropEnv * 0.35f;
        bassDrop = std::clamp(bassDrop, 0.65f, 1.0f);
        punchEnv += 0.002f * (0.0f - punchEnv);
        float punchGain = 1.0f + punchEnv * 1.4f;
        float y = z3 * bassDrop * punchGain * 1.1f;
        float out = tanh_fast(y * 1.6f) * 1.9f;
        return in * (1.0f - fadeGain) + out * fadeGain;
    }

private:
    float sampleRate = 44100.0f;
    float cutoff     = 800.0f;
    float resonance  = 0.5f;
    float vintageAmount = 0.4f;
    float bassDropEnv = 0.0f;
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    bool  targetOn = false;
    bool onoff = false;
    float keyTrack = 0.08f;
    float noteHz = 440.0f;

    float g = 0.0f;
    float resonanceGain = 0.0f;
    float punchEnv = 0.0f;
    float prePunchEnv = 0.0f;
    float outLevel = 1.0f;

    float z1 = 0.0f, z2 = 0.0f, z3 = 0.0f;

    float driftPhase = 0.0f;
    float driftSpeed = 0.000002f;

    void update() {
        float keyLeak = 1.0f + (noteHz / 440.0f - 1.0f) * keyTrack;
        float trackedCutoff = cutoff * keyLeak;
        trackedCutoff = std::clamp(trackedCutoff, 30.0f, sampleRate * 0.45f);
        float wc = 2.0f * M_PI * trackedCutoff;
        float T  = 1.0f / sampleRate;

        g = wc * T;
        g = g / (1.0f + g);

        resonanceGain = resonance * 1.3f;
    }

    static inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    static inline float preSaturate(float x, float vintage) {
        float bias = 0.482f * vintage;
        return tanh_fast((x + bias) * 1.2f) - tanh_fast(bias * 1.2f);
    }

    static inline float postSaturate(float x) {
        return x / (1.0f + std::fabs(x));
    }
};
