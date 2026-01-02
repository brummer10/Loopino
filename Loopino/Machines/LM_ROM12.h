
/*
 * LM_ROM12.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        LM_ROM12.h - simulate a Early Digital ROM Sampler machine

****************************************************************/

#pragma once
#include <cmath>


class LM_ROM12 {
public:

    void setDrive(float d) { drive = d; }

    void setOnOff(bool on) {
        onOff = on;
    }

    void setSampleRate(double sr) {
    }

    float process(float x) {
        if (!onOff) return x;
        x *= drive;
        x = interpolate(x);
        x = emuQuantize(x);
        x = airEQ(x);
        return x;
    }

private:
    float z=0, z1=0,z2=0,z3=0;
    float drive = 1.0f;
    bool  onOff     = false;

    inline float interpolate(float x) {
        float y = x*1.8f - z1*0.95f + z2*0.55f - z3*0.12f;
        z3=z2; z2=z1; z1=x;
        return y;
    }

    inline float airEQ(float x) {
        float air = x - z;
        z = x;
        return x + air * 0.45f;
    }

    inline float emuQuantize(float x) {
        return std::round(x * 32768.0f) * (1.0f / 32768.0f);
    }

};

