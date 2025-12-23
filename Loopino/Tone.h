
/*
 * Tone.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        Tone.h - Baxandall style tone control filter
        
****************************************************************/

#pragma once
#include <cmath>

class Baxandall {
public:
    void setSampleRate(float sr) {
        sampleRate = sr;
        update();
    }

    void setTone(float t) {
        tone = std::clamp(t, -1.0f, 1.0f);
        update();
    }

    inline float process(float x) {
        float v = x - fb;
        lp += gLow * (v - lp);
        hp += gHigh * (v - hp);
        float y = v + bassGain * lp + trebleGain * hp;
        fb = y * feedback;
        return y;
    }

private:

    inline float tanh_fast(float x) const {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    void update() {
        constexpr float lowFreq  = 250.0f;
        constexpr float highFreq = 4000.0f;
        gLow  = tanh_fast(float(M_PI) * lowFreq  / sampleRate);
        gHigh = tanh_fast(float(M_PI) * highFreq / sampleRate);
        constexpr float maxGain = 12.0f;
        float bassDB   = -tone * maxGain;
        float trebleDB =  tone * maxGain;
        bassGain   = std::pow(10.0f, bassDB   / 20.0f) - 1.0f;
        trebleGain = std::pow(10.0f, trebleDB / 20.0f) - 1.0f;
        feedback = 0.45f;
    }

    float sampleRate = 44100.0f;
    float tone = 0.0f;

    float lp = 0.0f;
    float hp = 0.0f;
    float fb = 0.0f;

    float gLow = 0.0f;
    float gHigh = 0.0f;
    float bassGain = 0.0f;
    float trebleGain = 0.0f;
    float feedback = 0.0f;
};
