
/*
 * Staircase.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        Staircase.h - simulate the EmulatorII sampler machine

****************************************************************/


#pragma once
#include <cmath>

class LM_EII12 {
public:

    void setCutOff(float c) { cutoff = c; }
    void setDrive(float d) { drive = d; }
    void setAmount(float a) { amount = a; }

    void setOnOff(bool on) {
        onOff = on;
    }

    void setSampleRate(float sr) {
        float wc = 2.0f * M_PI * cutoff;
        float k  = wc / (wc + sr);
        a = k; b = 1.f - k;
    }

    inline float process(float x) {
        if (!onOff) return x;
        x *= drive;
        constexpr float q = 0.000488281f; //1.0f / 2048.f;
        x = std::round(x / q) * q;
        x = tanh_fast(x * 1.4f);
        lp = a * x + b * lp;
        lp *= amount;
        return lp;
    }

private:
    float cutoff = 12000.0f;
    float drive  = 1.2f;
    float amount = 1.0f;
    bool  onOff     = false;

    float lp = 0.0f, a=0.0f, b=0.0f;

    inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
};

