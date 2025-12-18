
/*
 * Chorus.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        Chorus.h mono chorus effect
        
****************************************************************/


#pragma once

#include <cmath>

class mydspSIG0 {
private:
    int iRec4[2];

public:
    mydspSIG0() { iRec4[0] = iRec4[1] = 0; }

    void instanceInit(int /*sample_rate*/) {
        iRec4[0] = iRec4[1] = 0;
    }

    void fill(int count, float* table) {
        for (int i = 0; i < count; ++i) {
            iRec4[0] = iRec4[1] + 1;
            table[i] = std::sin(9.58738e-05f * float(iRec4[1]));
            iRec4[1] = iRec4[0];
        }
    }
};

class Chorus {
private:
    static constexpr int kDelaySize = 131072;
    static constexpr int kTableSize = 65536;

    float fSampleRate = 44100.0f;

    int   IOTA0 = 0;
    float fVec0[kDelaySize];

    // parameters
    float fHslider0 = 3.0f; // freq
    float fHslider1 = 0.02f; // depth
    float fHslider2 = 0.02f; // delay
    float fHslider3 = 0.5f; // level

    // state
    float fRec0[2]{};
    float fRec1[2]{};
    float fRec2[2]{};
    float fRec3[2]{};
    float fRec5[2]{};
    float fRec6[2]{};
    float fRec7[2]{};
    float fRec8[2]{};
    float fRec9[2]{};

    // constants
    float fConst0 = 0.0f;
    float fConst1 = 0.0f;
    float fConst2 = 0.0f;
    float fConst3 = 0.0f;
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    bool  targetOn = false; 

    float ftbl0[kTableSize];

    bool onoff = false;

    void clear_state_f() {
        for (int i = 0; i < kDelaySize; ++i) {
            fVec0[i] = 0.0f;
        }

        auto clear2 = [](float r[2]) { r[0] = r[1] = 0.0f; };

        clear2(fRec0);
        clear2(fRec1);
        clear2(fRec2);
        clear2(fRec3);
        clear2(fRec5);
        clear2(fRec6);
        clear2(fRec7);
        clear2(fRec8);
        clear2(fRec9);
        IOTA0 = 0;
    }

public:
    Chorus() = default;
    ~Chorus() = default;

    void setSampleRate(float sample_rate) {
        fSampleRate = sample_rate;

        mydspSIG0 sig0;
        sig0.instanceInit(static_cast<int>(sample_rate));
        sig0.fill(kTableSize, ftbl0);

        fConst0 = std::min(192000.0f, std::max(1.0f, fSampleRate));
        fConst1 = 1.0f / fConst0;
        fConst2 = 0.5f * fConst0;
        fConst3 = 1000.0f / fConst0;
        fadeStep = 1.0f / (0.02f * fSampleRate);

        clear_state_f();
    }

    void setChorusLevel(float v) { fHslider3 = v; } // , 0.5f, 0.0f, 1.0f, 0.01f 
    void setChorusDelay(float v) { fHslider2 = v; } // , 0.02f, 0.0f, 0.2f, 0.01f 
    void setChorusDepth(float v) { fHslider1 = v; } // , 0.02f, 0.0f, 1.0f, 0.01f 
    void setChorusFreq(float v)  { fHslider0 = v; } // , 3.00f, 0.0f, 10.f, 0.01f 

    void setOnOff(bool v) {
        targetOn = v;
        if (v && !onoff) {
            clear_state_f();
            onoff = true;
        }
    }

    inline float process(float in) {
        if (targetOn) {
            fadeGain = std::min(1.0f, fadeGain + fadeStep);
        } else {
            fadeGain = std::max(0.0f, fadeGain - fadeStep);
            if (fadeGain == 0.0f) {
                onoff = false;
                return in;
            }
        }

        float fSlow0 = 0.007f * fHslider0;
        float fSlow1 = 0.007f * fHslider1;
        float fSlow2 = 0.007f * fHslider2;
        float fSlow3 = 0.007f * fHslider3;
        float fTemp0 = in;
        fVec0[IOTA0 & (kDelaySize - 1)] = fTemp0;
        fRec6[0] = fSlow0 + 0.993f * fRec6[1];
        float fTemp1 = fRec5[1] + fConst1 * fRec6[0];
        fRec5[0] = fTemp1 - std::floor(fTemp1);
        float fTemp2 = float(kTableSize) *
                       (fRec5[0] + (0.25f - std::floor(fRec5[0] + 0.25f)));
        float fTemp3 = std::floor(fTemp2);
        int   iTemp4 = int(fTemp3);
        fRec7[0] = fSlow1 + 0.993f * fRec7[1];
        fRec8[0] = fSlow2 + 0.993f * fRec8[1];
        float lfo = (fTemp3 + (1.0f - fTemp2)) * ftbl0[iTemp4 & (kTableSize - 1)] +
            (fTemp2 - fTemp3) * ftbl0[(iTemp4 + 1) & (kTableSize - 1)];
        float fTemp5 = fConst2 * fRec8[0] * (fRec7[0] * lfo + 1.0f);
        float fTemp6 = (fRec0[1] != 0.0f)
                ? ((fRec1[1] > 0.0f && fRec1[1] < 1.0f) ? fRec0[1] : 0.0f)
                : ((fRec1[1] == 0.0f && fTemp5 != fRec2[1]) ? fConst3
                :  ((fRec1[1] == 1.0f && fTemp5 != fRec3[1]) ? -fConst3 : 0.0f));
        fRec0[0] = fTemp6;
        fRec1[0] = std::clamp(fRec1[1] + fTemp6, 0.0f, 1.0f);
        fRec2[0] = (fRec1[1] >= 1.0f && fRec3[1] != fTemp5) ? fTemp5 : fRec2[1];
        fRec3[0] = (fRec1[1] <= 0.0f && fRec2[1] != fTemp5) ? fTemp5 : fRec3[1];
        float fTemp7 = fVec0[(IOTA0 - int(std::clamp(fRec2[0], 0.0f, 65536.0f)))
                   & (kDelaySize - 1)];
        fRec9[0] = fSlow3 + 0.993f * fRec9[1];
        float out = fTemp0 + fRec9[0] * (fTemp7 + fRec1[0] *
             (fVec0[(IOTA0 - int(std::clamp(fRec3[0], 0.0f, 65536.0f)))
                    & (kDelaySize - 1)] - fTemp7));
        IOTA0++;
        fRec6[1] = fRec6[0];
        fRec5[1] = fRec5[0];
        fRec7[1] = fRec7[0];
        fRec8[1] = fRec8[0];
        fRec0[1] = fRec0[0];
        fRec1[1] = fRec1[0];
        fRec2[1] = fRec2[0];
        fRec3[1] = fRec3[0];
        fRec9[1] = fRec9[0];

        return in * (1.0f - fadeGain) + out * fadeGain;
    }
};
