
/*
 * Brickwall.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        BrickWall.h - a little Key Cache smoother

****************************************************************/


#pragma once
#include <vector>
#include <cmath>
#include <cstdlib>


struct Biquad {
    float a0=1,a1=0,a2=0,b1=0,b2=0;
    float z1=0,z2=0;

    inline float process(float x){
        float y = x*a0 + z1;
        z1 = x*a1 + z2 - b1*y;
        z2 = x*a2 - b2*y;
        return y;
    }
};

class Brickwall {
public:
    void setSampleRate(float samplerate) {
        makeLP(s1, samplerate*0.21f, samplerate, 0.54f);
        makeLP(s2, samplerate*0.23f, samplerate, 0.63f);
        makeLP(s3, samplerate*0.25f, samplerate, 0.78f);
    }

    inline float process(float x){
        return s3.process(s2.process(s1.process(x)));
    }

private:
    Biquad s1,s2,s3;

    inline void makeLP(Biquad& b, float fc, float sr, float Q) {
        float w = 2.0f * M_PI * fc / sr;
        float c = cos(w), s = sin(w);
        float alpha = s/(2*Q);

        float a0 = 1 + alpha;
        b.a0 = (1-c)/(2*a0);
        b.a1 = (1-c)/a0;
        b.a2 = (1-c)/(2*a0);
        b.b1 = -2*c/a0;
        b.b2 = (1-alpha)/a0;
    }
};

