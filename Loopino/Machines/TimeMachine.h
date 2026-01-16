
/*
 * TimeMachine.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        TimeMachine.h - simulate a broken sampler machine

****************************************************************/



#pragma once
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>


struct LP {
    float z = 0;
    void process(std::vector<float>& s, float cutoff) {
        float a = cutoff * 0.18f;
        for(auto& x:s){ z += a*(x - z); x = z; }
    }
};

class TimeMachine {
public:

    bool getOnOff() const { return onoff; }
    void setOnOff(bool o)  { onoff = o; }

    void setTimeDial(float t) {
        t = std::clamp(t, 0.f, 1.f);
        drive  = 0.15f + t*t * 1.6f;
        grit   = 0.10f + t * 0.90f;
        jitter = t*t*t * 0.75f;
        cutoff = 0.92f - t*t * 0.72f;
    }

    void processV(std::vector<float>& s) {
        if (!onoff) return;
        for(auto& x:s) x = sat(x* (1+drive));
        for(auto& x:s) x = compand(x*grit + x*(1-grit));
        jitterResample(s, jitter);
        lp.process(s, cutoff);
    }
private:
    float drive=0.3f, grit=0.4f, jitter=0.4f, cutoff=0.6f;
    bool onoff = false;
    LP lp;

    inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float sat(float x) {
        return tanh_fast(x * 1.4f);
    }

    inline float compand(float x) {
        x = std::copysign(std::pow(std::abs(x), 0.65f), x);
        return std::round(x * 2047.f) / 2047.f;
    }

    void jitterResample(std::vector<float>& s, float amount) {
        if (amount < 0.0001f) return;

        std::vector<float> out(s.size());
        double p = 0.0;
        double drift = 1.0;

        for (size_t i = 0; i < out.size(); ++i) {
            drift += ((rand() / float(RAND_MAX)) - 0.5f) * 0.0005 * amount;
            p += drift;

            int ip = int(p);
            float t = p - ip;

            auto S=[&](int x){
                if(x<0) return s[0];
                if(x>=int(s.size())) return s.back();
                return s[x];
            };

            float x0=S(ip-1),x1=S(ip),x2=S(ip+1),x3=S(ip+2);
            out[i] = x1 + 0.5f*t*(x2-x0 + t*(2*x0-5*x1+4*x2-x3 + t*(3*(x1-x2)+x3-x0)));
        }
        s.swap(out);
    }
};
