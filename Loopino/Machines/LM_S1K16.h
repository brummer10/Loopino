
/*
 * LM_S1K16.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        LM_S1K16.h - simulate the LM_S1K16 sampler machine

****************************************************************/

#pragma once
#include <vector>
#include <cmath>

class LM_S1K16 {
public:

    void setDrive(float d) { drive = d; }
    void setWarmth(float w) { warmth  = (1.0f - exp(-w * 3.5f)) * 0.85f; }
    void setHfTilt(float h) { hfTilt = pow(h, 1.7f) * 1.2f;; }

    void setOnOff(bool on) {
        onOff = on;
    }

    void setSampleRate(double sr) {
        fs = sr;
        brick = blur = tiltLP = tiltHP = 0.0f;
        
    }

    inline float process(float x) {
        if (!onOff) return x;
        x *= drive;
        float fc = 18000.0f / fs;
        brick += fc * (x - brick);
        x = brick;
        x = dacCurve(x);
        tiltLP += 0.02f * (x - tiltLP);
        tiltHP = x - tiltLP;
        x = x + hfTilt * tiltHP;
        blur += 0.04f * (x - blur);
        x = warmth * blur + (1.0f - warmth) * x;

        return x;
    }

private:
    float drive   = 1.1f;
    float warmth  = 0.65f;
    float hfTilt  = 0.45f;
    bool  onOff     = false;

    double fs = 44100.0;
    float brick = 0, blur = 0;
    float tiltLP = 0, tiltHP = 0;

    inline float dacCurve(float x) {
        const float q = 32768.0f;
        x = std::round(x * q) / q;
        x += 0.00015f * x * x * x;
        return x;
    }
};

