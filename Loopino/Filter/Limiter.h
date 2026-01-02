
/*
 * Limiter.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        Limiter.h  a fixed compressor acting as limiter
        
ration = 10.0 threshold = -6.0 dB attack 0.0008 sec release 0.5 sec
****************************************************************/

#pragma once

#include <cmath>

struct Limiter {
    float fSampleRate = 44100.0f;
    float fConst0 = 0.0f;
    float fConst1 = 0.0f;
    float fConst2 = 0.0f;
    float fRec1[2] = {0.0f};
    float fConst3 = 0.0f;
    float fConst4 = 0.0f;
    float fRec0[2] = {0.0f};

    void setSampleRate(float sample_rate) {
        fSampleRate = sample_rate;
        fConst0 = std::min<float>(1.92e+05f, std::max<float>(1.0f, float(fSampleRate)));
        fConst1 = std::exp(-(2.0f / fConst0));
        fConst2 = std::exp(-(1.25e+03f / fConst0));
        fConst3 = std::exp(-(2.5e+03f / fConst0));
        fConst4 = 0.9f * (1.0f - fConst3);
        for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec1[l0] = 0.0f;
        for (int l1 = 0; l1 < 2; l1 = l1 + 1) fRec0[l1] = 0.0f;
    }

    inline float process(float in) {
        float fTemp0 = in;
        float fTemp1 = std::fabs(fTemp0);
        float fTemp2 = ((fTemp1 > fRec1[1]) ? fConst2 : fConst1);
        fRec1[0] = fTemp1 * (1.0f - fTemp2) + fRec1[1] * fTemp2;
        fRec0[0] = fConst3 * fRec0[1] - fConst4 * std::max<float>(2e+01f *
            std::log10(std::max<float>(1.1754944e-38f, fRec1[0])) + 6.0f, 0.0f);

        float out = fTemp0 * std::pow(1e+01f, 0.05f * fRec0[0]);
        fRec1[1] = fRec1[0];
        fRec0[1] = fRec0[0];
        return out;
    }

};


