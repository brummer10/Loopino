
/*
 * LM_CMP12Dac.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        LM_CMP12Dac.h - simulate the LM_CMP12 sampler machine

****************************************************************/

#pragma once
#include <cmath>

struct LM_CMP12HFBoost {
    float a = 0, z = 0;

    void setup(float amount) {
        float fc = 5500.0f;   // LM_CMP12 corner freq
        float g  = 1.0f + amount * 3.5f; // max ~+11dB
        float k  = tanf(M_PI * fc / 44100.0f);
        a = (g - 1.0f) * k;
    }

    inline float process(float x) {
        z += a * (x - z);
        return x + z;
    }
};

class LM_CMP12CompanderPre {
public:
    float env       = 0.0f;
    float threshold = 0.12f;
    float ratio     = 1.65f;
    float attack    = 0.002f;
    float release   = 0.06f;

    void setSampleRate(float sr) {
        a = std::exp(-1.f / (attack  * sr));
        r = std::exp(-1.f / (release * sr));
        hfb.setup(0.7f);
    }

    inline float process(float x) {
        x = hfb.process(x);
        float ax = std::fabs(x);
        env = ax > env ? ax + a*(env-ax) : ax + r*(env-ax);
        float g = 1.0f;
        if (env > threshold)
            g = std::pow(env/threshold, -(ratio-1.0f));

        return x * g;
    }

private:
    float a=0.0f, r=0.0f;
    LM_CMP12HFBoost hfb;
};

class LM_CMP12Brick {
public:
    float drive  = 1.0f;
    float lp = 0.0f;
    float zoh = 0.0f;

    void setSampleRate(float sr) {
        float wc = 2.f * M_PI * cutoff;
        float k  = wc / (wc + sr);
        a = k; b = 1.f - k;
    }

    inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float process(float x) {
        x *= drive;
        constexpr float q = 1.f / 2048.f; // 12 bit signed
        x = std::round(x / q) * q;
        zoh = zoh + a * (x - zoh);
        float y = tanh_fast(zoh * 1.8f);
        lp = a * y + b * lp;
        return lp;
    }

private:
    float cutoff = 7200.0f;
    float a=0.0f, b=0.0f;
};

struct LM_CMP12Deemph {
    float a = 0, z = 0;

    void setup(float amount, float sr) {
        float fc = 4200.0f;
        float g  = 0.4f + amount * 0.6f;
        float k  = tanf(M_PI * fc / sr);
        a = g * k;
    }

    inline float process(float x) {
        z += a * (x - z);
        return x - z * 0.65f;
    }
};

class LM_CMP12CompanderPost {
public:
    float ratio     = 1.65f;

    void setSampleRate(float sr) {
        hfd.setup(0.1f, sr);
    }

    inline float process(float x) {
        //x = hfd.process(x);
        float ax = std::fabs(x);
        float g = (ax > threshold)
            ? std::pow(ax/threshold, (ratio-1.0f))
            : 1.0f;
        return x * g;
    }
private:
    float threshold = 0.12f;
    LM_CMP12Deemph hfd;
};

class LM_CMP12Dac {
public:
    void setRatio(float r) {
        sppre.ratio = r;
        sppost.ratio = r;
    }

    void setDrive(float d) {spbrick.drive = d; }

    void setOnOff(bool on) {
        onOff = on;
    }

    void setSampleRate(float sr) {
        sppre.setSampleRate(sr);
        spbrick.setSampleRate(sr);
        sppost.setSampleRate(sr);
    }

    inline float process(float x) {
        if (!onOff) return x;
        x = sppre.process(x);
        x = spbrick.process(x);
        x = sppost.process(x);
        return x * 0.6f;
    }

private:
    LM_CMP12CompanderPre sppre;
    LM_CMP12Brick spbrick;
    LM_CMP12CompanderPost sppost;
    bool  onOff     = false;
};
