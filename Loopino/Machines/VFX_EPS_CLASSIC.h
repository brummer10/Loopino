
/*
 * VFX_EPS_CLASSIC.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        VFX_EPS_CLASSIC.h - simulate a Classic Digital Synth (90s) sampler machine

****************************************************************/

#pragma once
#include <cmath>

struct EPSFilter {
    float z1=0,z2=0,z3=0,z4=0;

    inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    float process(float x) {
        constexpr float c = 0.32f;
        z1 += c * (x - z1);
        z2 += c * (z1 - z2);
        z3 += c * (z2 - z3);
        z4 += c * (z3 - z4);
        return tanh_fast(z4 * 1.4f);
    }
};


class VFX_EPS_CLASSIC {
public:

    bool getOnOff() const  { return onOffState; }

    void setDrive(float d) { driveState = d; }
    void setOnOff(bool on) { onOffState = on; }

    void setSampleRate(double sr) {
    }

    void applyState() {
        drive = driveState;
        onOff = onOffState;
    }

    inline void processV(std::vector<float>& s) {
        if (!onOff) return;
        for (auto& x : s) x = process(x);
    }

    float process(float x) {
        epsPhase += 0.000015;
        if (epsPhase >= 1.0) epsPhase -= 1.0;

        x = eps_adc(x);
        x = eps_fixed(x);
        x = eps_loop_jitter(x, epsPhase);
        x = eps_dac(x);
        x = epsFilter.process(x);
        return x;
    }

private:
    EPSFilter epsFilter;
    float drive = 1.0f;
    bool  onOff     = false;
    float driveState = 1.0f;
    bool  onOffState     = false;
    double epsPhase = 0.0;

    inline float eps_adc(float x) {
        x *= drive;
        x = std::clamp(x, -1.f, 1.f);
        constexpr int BITS = 13;
        constexpr int LEVELS = 1 << BITS;
        float bias = x * x * x * 0.12f;
        x += bias;
        return std::round(x * LEVELS) / LEVELS;
    }

    inline float eps_fixed(float x) {
        return std::floor(x * 32768.f) / 32768.f;
    }

    inline float eps_loop_jitter(float x, float phase) {
        float jitter = 0.00008f * std::sin(phase * 6.28318f * 7.0f);
        return x + jitter;
    }

    inline float eps_dac(float x) {
        constexpr int LEVELS = 8192; // 13 bit
        return std::round(x * LEVELS) / LEVELS;
    }

};
