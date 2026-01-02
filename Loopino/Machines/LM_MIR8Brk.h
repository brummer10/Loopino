
/*
 * LM_MIR8Brk.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        LM_MIR8Bk.h - simulate the LM_MIR8 sampler machine

****************************************************************/

#pragma once
#include <cmath>

class LM_MIR8Brk {
public:

    void setCutOff(float c) { cutoff = c; }
    void setDrive(float d)  { drive = d; }
    void setAmount(float a){ amount = a; }

    void setOnOff(bool on) {
        onOff = on;
    }

    void setSampleRate(float sr) {
        float wc = 2.f * M_PI * cutoff;
        float k  = wc / (wc + sr);
        a = k; b = 1.f - k;
    }

    inline float process(float x) {
        if (!onOff) return x;
        x *= drive;
        constexpr float mu = 255.f;
        float s = copysignf(1.f, x);
        x = s * log1p(mu * fabsf(x)) / log1p(mu);
        constexpr float q = 1.f / 32.f;
        x = std::round(x / q) * q;
        x = tanh_fast(x * 2.5f);
        lp = a * x + b * lp;
        float out =  lp * amount;
        return out;
    }

private:
    float cutoff    = 5800.0f;
    float drive     = 1.3f;
    float amount   = 0.25f;
    bool  onOff     = false;

    float lp = 0, a=0, b=0;

    inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
};

