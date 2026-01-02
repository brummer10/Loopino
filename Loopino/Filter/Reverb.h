
/*
 * Reverb.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        Reverb.h mono reverb effect
        
****************************************************************/

#pragma once

#include <cmath>

class Reverb {
private:
    float fSampleRate = 44100.0f;
    float mix = 50.0f;
    float damp = 0.25f;
    float fRec9[2];
    float roomsize = 0.9f;
    int IOTA0 = 0;
    float fVec0[2048];
    float fRec8[2];
    float fRec11[2];
    float fVec1[2048];
    float fRec10[2];
    float fRec13[2];
    float fVec2[2048];
    float fRec12[2];
    float fRec15[2];
    float fVec3[2048];
    float fRec14[2];
    float fRec17[2];
    float fVec4[2048];
    float fRec16[2];
    float fRec19[2];
    float fVec5[2048];
    float fRec18[2];
    float fRec21[2];
    float fVec6[2048];
    float fRec20[2];
    float fRec23[2];
    float fVec7[2048];
    float fRec22[2];
    float fVec8[1024];
    float fRec6[2];
    float fVec9[512];
    float fRec4[2];
    float fVec10[512];
    float fRec2[2];
    float fVec11[256];
    float fRec0[2];
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    float fadedStep = 0.0f;
    bool  targetOn = false; 
    bool onoff = false;

public:
    Reverb() = default;
    ~Reverb() = default;

    void clear_state_f() {
        for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec9[l0] = 0.0f;
        for (int l1 = 0; l1 < 2048; l1 = l1 + 1) fVec0[l1] = 0.0f;
        for (int l2 = 0; l2 < 2; l2 = l2 + 1) fRec8[l2] = 0.0f;
        for (int l3 = 0; l3 < 2; l3 = l3 + 1) fRec11[l3] = 0.0f;
        for (int l4 = 0; l4 < 2048; l4 = l4 + 1) fVec1[l4] = 0.0f;
        for (int l5 = 0; l5 < 2; l5 = l5 + 1) fRec10[l5] = 0.0f;
        for (int l6 = 0; l6 < 2; l6 = l6 + 1) fRec13[l6] = 0.0f;
        for (int l7 = 0; l7 < 2048; l7 = l7 + 1) fVec2[l7] = 0.0f;
        for (int l8 = 0; l8 < 2; l8 = l8 + 1) fRec12[l8] = 0.0f;
        for (int l9 = 0; l9 < 2; l9 = l9 + 1) fRec15[l9] = 0.0f;
        for (int l10 = 0; l10 < 2048; l10 = l10 + 1) fVec3[l10] = 0.0f;
        for (int l11 = 0; l11 < 2; l11 = l11 + 1) fRec14[l11] = 0.0f;
        for (int l12 = 0; l12 < 2; l12 = l12 + 1) fRec17[l12] = 0.0f;
        for (int l13 = 0; l13 < 2048; l13 = l13 + 1) fVec4[l13] = 0.0f;
        for (int l14 = 0; l14 < 2; l14 = l14 + 1) fRec16[l14] = 0.0f;
        for (int l15 = 0; l15 < 2; l15 = l15 + 1) fRec19[l15] = 0.0f;
        for (int l16 = 0; l16 < 2048; l16 = l16 + 1) fVec5[l16] = 0.0f;
        for (int l17 = 0; l17 < 2; l17 = l17 + 1) fRec18[l17] = 0.0f;
        for (int l18 = 0; l18 < 2; l18 = l18 + 1) fRec21[l18] = 0.0f;
        for (int l19 = 0; l19 < 2048; l19 = l19 + 1) fVec6[l19] = 0.0f;
        for (int l20 = 0; l20 < 2; l20 = l20 + 1) fRec20[l20] = 0.0f;
        for (int l21 = 0; l21 < 2; l21 = l21 + 1) fRec23[l21] = 0.0f;
        for (int l22 = 0; l22 < 2048; l22 = l22 + 1) fVec7[l22] = 0.0f;
        for (int l23 = 0; l23 < 2; l23 = l23 + 1) fRec22[l23] = 0.0f;
        for (int l24 = 0; l24 < 1024; l24 = l24 + 1) fVec8[l24] = 0.0f;
        for (int l25 = 0; l25 < 2; l25 = l25 + 1) fRec6[l25] = 0.0f;
        for (int l26 = 0; l26 < 512; l26 = l26 + 1) fVec9[l26] = 0.0f;
        for (int l27 = 0; l27 < 2; l27 = l27 + 1) fRec4[l27] = 0.0f;
        for (int l28 = 0; l28 < 512; l28 = l28 + 1) fVec10[l28] = 0.0f;
        for (int l29 = 0; l29 < 2; l29 = l29 + 1) fRec2[l29] = 0.0f;
        for (int l30 = 0; l30 < 256; l30 = l30 + 1) fVec11[l30] = 0.0f;
        for (int l31 = 0; l31 < 2; l31 = l31 + 1) fRec0[l31] = 0.0f;
    }

    void setSampleRate(float sample_rate) {
        fSampleRate = sample_rate;
        fadeStep = 1.0f / (0.02f * fSampleRate);
        fadedStep = 1.0f / (0.9f * fSampleRate);
        IOTA0 = 0;
        clear_state_f();
    }

    void setRoomSize(float v) { roomsize = v; } // 0.5f, 0.0f, 1.0f, 0.025f 
    void setDamp(float v) { damp = v; }         // 0.5f, 0.0f, 1.0f, 0.025f 
    void setMix(float v) { mix = v; }           // 5e+01f, 0.0f, 1e+02f, 1.0f 

    void setOnOff(bool v) {
        targetOn = v;
        if (v && !onoff) {
            clear_state_f();
            onoff = true;
        }
    }

    float process(float in) {
        if (targetOn) {
            fadeGain = std::min(1.0f, fadeGain + fadeStep);
        } else {
            fadeGain = std::max(0.0f, fadeGain - fadedStep);
            if (fadeGain == 0.0f) {
                onoff = false;
                return in;
            }
        }

        float fSlow0 = mix;
        float fSlow1 = 1.0f - 0.01f * fSlow0;
        float fSlow2 = fSlow1 + fSlow0 * (0.01f * fSlow1 + 0.00015f);
        float fSlow3 = damp;
        float fSlow4 = 1.0f - fSlow3;
        float fSlow5 = 0.28f * roomsize + 0.7f;
        float fSlow6 = 0.00015f * fSlow0;
        float fTemp0 = in;
        fRec9[0] = fSlow3 * fRec9[1] + fSlow4 * fRec8[1];
        float fTemp1 = fSlow6 * fTemp0;
        fVec0[IOTA0 & 2047] = fTemp1 + fSlow5 * fRec9[0];
        fRec8[0] = fVec0[(IOTA0 - 1640) & 2047];
        fRec11[0] = fSlow3 * fRec11[1] + fSlow4 * fRec10[1];
        fVec1[IOTA0 & 2047] = fTemp1 + fSlow5 * fRec11[0];
        fRec10[0] = fVec1[(IOTA0 - 1580) & 2047];
        fRec13[0] = fSlow3 * fRec13[1] + fSlow4 * fRec12[1];
        fVec2[IOTA0 & 2047] = fTemp1 + fSlow5 * fRec13[0];
        fRec12[0] = fVec2[(IOTA0 - 1514) & 2047];
        fRec15[0] = fSlow3 * fRec15[1] + fSlow4 * fRec14[1];
        fVec3[IOTA0 & 2047] = fTemp1 + fSlow5 * fRec15[0];
        fRec14[0] = fVec3[(IOTA0 - 1445) & 2047];
        fRec17[0] = fSlow3 * fRec17[1] + fSlow4 * fRec16[1];
        fVec4[IOTA0 & 2047] = fTemp1 + fSlow5 * fRec17[0];
        fRec16[0] = fVec4[(IOTA0 - 1379) & 2047];
        fRec19[0] = fSlow3 * fRec19[1] + fSlow4 * fRec18[1];
        fVec5[IOTA0 & 2047] = fTemp1 + fSlow5 * fRec19[0];
        fRec18[0] = fVec5[(IOTA0 - 1300) & 2047];
        fRec21[0] = fSlow3 * fRec21[1] + fSlow4 * fRec20[1];
        fVec6[IOTA0 & 2047] = fTemp1 + fSlow5 * fRec21[0];
        fRec20[0] = fVec6[(IOTA0 - 1211) & 2047];
        fRec23[0] = fSlow3 * fRec23[1] + fSlow4 * fRec22[1];
        fVec7[IOTA0 & 2047] = fTemp1 + fSlow5 * fRec23[0];
        fRec22[0] = fVec7[(IOTA0 - 1139) & 2047];
        float fTemp2 = fRec22[0] + fRec20[0] + fRec18[0] + fRec16[0] + fRec14[0] + fRec12[0] + fRec10[0] + fRec8[0];
        fVec8[IOTA0 & 1023] = fTemp2 + 0.5f * fRec6[1];
        fRec6[0] = fVec8[(IOTA0 - 579) & 1023];
        float fRec7 = fRec6[1] - fTemp2;
        fVec9[IOTA0 & 511] = fRec7 + 0.5f * fRec4[1];
        fRec4[0] = fVec9[(IOTA0 - 464) & 511];
        float fRec5 = fRec4[1] - fRec7;
        fVec10[IOTA0 & 511] = fRec5 + 0.5f * fRec2[1];
        fRec2[0] = fVec10[(IOTA0 - 364) & 511];
        float fRec3 = fRec2[1] - fRec5;
        fVec11[IOTA0 & 255] = fRec3 + 0.5f * fRec0[1];
        fRec0[0] = fVec11[(IOTA0 - 248) & 255];
        float fRec1 = fRec0[1] - fRec3;
        float out = fRec1 + fSlow2 * fTemp0;
        fRec9[1] = fRec9[0];
        IOTA0 = IOTA0 + 1;
        fRec8[1] = fRec8[0];
        fRec11[1] = fRec11[0];
        fRec10[1] = fRec10[0];
        fRec13[1] = fRec13[0];
        fRec12[1] = fRec12[0];
        fRec15[1] = fRec15[0];
        fRec14[1] = fRec14[0];
        fRec17[1] = fRec17[0];
        fRec16[1] = fRec16[0];
        fRec19[1] = fRec19[0];
        fRec18[1] = fRec18[0];
        fRec21[1] = fRec21[0];
        fRec20[1] = fRec20[0];
        fRec23[1] = fRec23[0];
        fRec22[1] = fRec22[0];
        fRec6[1] = fRec6[0];
        fRec4[1] = fRec4[0];
        fRec2[1] = fRec2[0];
        fRec0[1] = fRec0[0];

        return in * (1.0f - fadeGain) + out * fadeGain;
    }

};
