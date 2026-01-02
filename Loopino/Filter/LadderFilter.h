
/*
 * LadderFilter.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#pragma once
#include <cmath>
#include <algorithm>

enum class LadderVoicing { Warm, Classic, Bright };

/****************************************************************
        Zero Delay Feedback Ladder Filter (Moog style)
****************************************************************/

struct ZDFLadderFilter {
    const double leak = 0.99996;
    double z1=0, z2=0, z3=0, z4=0;
    double cutoff = 1000.0;
    double resonance = 0.0;
    double sampleRate = 44100.0;
    double feedback = 1.0;
    double voicing = 1.16;
    int ccCutoff = 48;
    int ccReso = 50;
    float keyTracking = 1.0f;
    bool filterOff = false;
    bool highpass = false;
    double g = 0.0;
    double lastY4 = 0.0;
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    bool  targetOn = false; 

    const float minFreq = 20.0;
    const float maxFreq = 20000.0;
    const float minQ = 0.6;
    const float maxQ = 10.0;

    double ccToFreq(int v) const {
        double t = v / 127.0;
        return minFreq * std::pow(maxFreq/minFreq, t);
    }

    double ccToQ(int v) const {
        double t = std::pow(v / 127.0, 0.8);
        return minQ + t * (maxQ - minQ);
    }

    inline void update() {
        // avoid > Nyquist and keep argument small for tan()
        double fc = cutoff * voicing;
        double nyq = 0.5 * sampleRate;
        if (fc > nyq * 0.99) fc = nyq * 0.99;
        if (fc < 1.0) fc = 1.0;
        double x = (M_PI * fc) / sampleRate;
        g = std::tan(x);
        if (!std::isfinite(g) || g <= 0.0) g = 1e-12;
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        fadeStep = 1.0f / (0.02f * sampleRate);
        update();
    }

    void reset() {
        z1=z2=z3=z4=0.0;
        lastY4 = 0.0;
    }

    void setVoicing(LadderVoicing v) {
        switch(v) {
            case LadderVoicing::Warm:      voicing = 1.12; break;
            case LadderVoicing::Classic:   voicing = 1.16; break;
            case LadderVoicing::Bright:    voicing = 1.30; break;
            default:                       voicing = 1.16; break;
        }
        update();
    }

    inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline double tpt(double x, double &s) {
        double y = (x - s) * (g / (1.0 + g)) + s;
        s = y + (y - s);
        return y;
    }

    inline double process(double in) {
        if (targetOn) {
            fadeGain = std::min(1.0f, fadeGain + fadeStep);
        } else {
            fadeGain = std::max(0.0f, fadeGain - fadeStep);
            if (fadeGain == 0.0f) {
                filterOff = false;
                return in;
            }
        }

        double gComp = 1.0 / (1.0 + 0.5 * g);
        double fb = feedback * gComp;
        double u = in - fb * lastY4;

        u = tanh_fast(u);

        double y1 = tpt(u,  z1);
        double y2 = tpt(y1, z2);
        double y3 = tpt(y2, z3);
        double y4 = tpt(y3, z4);
        // leak
        z1 *= leak;
        z2 *= leak;
        z3 *= leak;
        z4 *= leak;

        lastY4 = y4;

        double resGainComp = 1.0 + (resonance * resonance) * 2.0;
        double lp = y4 * resGainComp;
        double hp = (in - 4.0 * y1) * 0.33 * resGainComp * 0.5; // (input - (y1 * 2.0 + y2)) * 0.33;  input - lp; //

        if (!highpass) return in * (1.0f - fadeGain) + lp * fadeGain;
        return in * (1.0f - fadeGain) + hp * fadeGain;
    }

    void recalcFilter(int midiNote) {
        if (!filterOff) return;

        float baseCut = ccToFreq(ccCutoff);
        float semi = midiNote - 60;
        float keyFactor = std::pow(2.0f, (semi / 12.0f) * keyTracking);
        double finalCut = std::clamp(baseCut * keyFactor, minFreq, maxFreq);
        cutoff = finalCut;
        double Q = ccToQ(ccReso);
        resonance = std::clamp((Q - 0.5) * 0.22, 0.0, 0.95);
        update();
        double gComp = 1.0 / (1.0 + 0.5 * g);
        feedback = resonance * 3.5 * gComp;
    }
};

/****************************************************************
        4-Pole Ladder Filter (Moog style)
****************************************************************/

struct LadderFilter {
    const double leak = 0.99996;
    double z1=0, z2=0, z3=0, z4=0; // 4 integrator stages
    double cutoff = 1000.0;
    double resonance = 0.0;
    double sampleRate = 44100.0;
    double feedback = 1.0;
    double tunning = 0.5;
    double voicing = 1.16; // classic
    int ccCutoff = 68;
    int ccReso = 68;
    float keyTracking = 1.0f;
    bool filterOff = false;
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    bool  targetOn = false; 

    double bias = 0.0;
    double biasCoeff = 0.00005; 
    double dc_x1 = 0.0;
    double dc_y1 = 0.0;
    double dc_R  = 0.996;

    const float minFreq = 20.0;
    const float maxFreq = 20000.0;
    const float minQ = 0.6;
    const float maxQ = 10.0;

    double ccToFreq(int v) const {
        double t = v / 127.0;
        return minFreq * std::pow(maxFreq/minFreq, t);
    }

    double ccToQ(int v) const {
        double t = std::pow(v / 127.0, 0.8);
        return minQ + t * (maxQ - minQ);
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        fadeStep = 1.0f / (0.01f * sampleRate);
        dc_R = exp(-2.0 * M_PI * 5.0 / sampleRate);
    }

    void reset() {
        z1=z2=z3=z4=0.0;
        dc_x1 = dc_y1 = 0.0;
        bias = 0.0;
    }

    void setVoicing(LadderVoicing v) {
        switch(v) {
            case LadderVoicing::Warm:      voicing = 1.12; break;
            case LadderVoicing::Classic:   voicing = 1.16; break;
            case LadderVoicing::Bright:    voicing = 1.30; break;
            default:                       voicing = 1.16; break;
        }
    }

    inline double dcBlock(double x) {
        double y = x - dc_x1 + dc_R * dc_y1;
        dc_x1 = x;
        dc_y1 = y;
        return y;
    }

    inline double removeBias(double x) {
        bias += biasCoeff * (x - bias);
        return x - bias;
    }

    inline double tanh_fast(double x) {
        double x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline double process(double in) {
        if (targetOn) {
            fadeGain = std::min(1.0f, fadeGain + fadeStep);
        } else {
            fadeGain = std::max(0.0f, fadeGain - fadeStep);
            if (fadeGain == 0.0f) {
                filterOff = false;
                return in;
            }
        }
        double input = in;
        in -= z4 * feedback;
        double drive = 1.0 + resonance * 0.05;
        in *= drive;
        in = removeBias(in);
        in = tanh_fast(in);
        // 4 cascaded one-pole filters
        z1 += tunning * (in - z1);
        z2 += tunning * (z1 - z2);
        z3 += tunning * (z2 - z3);
        z4 += tunning * (z3 - z4);
        // leak
        z1 *= leak;
        z2 *= leak;
        z3 *= leak;
        z4 *= leak;

        double resGainComp = 1.0 + (resonance * resonance) * 2.0;
        double lp = z4 * resGainComp;
        lp = dcBlock(lp);
        return input * (1.0f - fadeGain) + lp * fadeGain;
    }

    void recalcFilter(int midiNote) {
        if (!filterOff) return;

        float baseCut = ccToFreq(ccCutoff);
        float semi = midiNote - 60;
        float keyFactor = std::pow(2.0f, (semi / 12.0f) * keyTracking);
        double finalCut = std::clamp(baseCut * keyFactor, minFreq, maxFreq);
        cutoff = finalCut;
        //std::cout << finalCut << std::endl;
        double Q = ccToQ(ccReso);
        resonance = std::clamp((Q - 0.5) * 0.22, 0.0, 0.95);
        tunning = 2.0 * (cutoff / sampleRate) * voicing;
        feedback = resonance * 4.0 * (1.0 - 0.15 * tunning * tunning);
    }
};

