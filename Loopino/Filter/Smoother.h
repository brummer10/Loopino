/*
 * Smoother.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        Smoother.h - a 4 x one pole multi mode filter
****************************************************************/

#pragma once
#include <cmath>
#include <algorithm>


class Smoother
{
public:
    Smoother() = default;

    float cutoff = 1000.0f;

    void setSampleRate(float sr) {
        sampleRate = sr;
    }

    void reset() {
        s1.reset();
        s2.reset();
        s3.reset();
        s4.reset();
    }

    inline float process(float in) {

        float g = 1.0f - std::exp(-2.0f * float(M_PI) * cutoff / sampleRate);
        float x = in;
        x = saturate(x);

        float o1 = s1.process(x, g);
        float o2 = s2.process(saturate(o1), g);
        float o3 = s3.process(saturate(o2), g);
        float o4 = s4.process(saturate(o3), g);

        s1.z = antiDenormal(s1.z);
        s2.z = antiDenormal(s2.z);
        s3.z = antiDenormal(s3.z);
        s4.z = antiDenormal(s4.z);

        float lp = o4;
        float bp = o2 - o4;
        float hp = in - o4;

        return mixOutputs(hp, bp, lp);
    }

private:

    struct OnePole {
        float z = 0.0f;

        inline float process(float x, float g) {
            z += g * (x - z);
            return z;
        }

        void reset() { z = 0.0f; }
    };

    inline float tanh_fast(float x) const {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float saturate(float x) const {
        return tanh_fast(x * 1.4f) + 0.15f * x * x * x;
    }

    inline float mixOutputs(float hp, float bp, float lp) const {
        float hpAmt = std::clamp(-mix, 0.0f, 1.0f);
        float lpAmt = std::clamp( mix, 0.0f, 1.0f);
        float bpAmt = 1.0f - std::abs(mix);
        bpAmt = std::pow(bpAmt, 0.7f);
        return hp * hpAmt + bp * bpAmt + lp * lpAmt;
    }

    inline float antiDenormal(float x) {
        return (std::abs(x) < 1e-15f) ? 0.0f : x;
    }

    float sampleRate = 44100.0f;
    float mix       = -0.18f;

    OnePole s1, s2, s3, s4;
};

