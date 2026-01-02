/*
 * CheckResample.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2024 brummer <brummer@web.de>
 */

/****************************************************************
        CheckResample.h - resample buffer when needed
                          using cubic hermite interpolation
****************************************************************/

#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>

class CheckResample {
public:
    CheckResample() {}

    float *checkSampleRate(uint32_t *count, uint32_t chan, float *input,
                           uint32_t fs_in, uint32_t fs_out) {
        if (fs_in == fs_out) return input;

        double ratio = double(fs_in) / double(fs_out);
        uint32_t outFrames = (uint32_t)std::ceil(*count / ratio);
        float *out = new float[outFrames * chan];

        for (uint32_t ch = 0; ch < chan; ++ch) {
            double srcPos = 0.0;

            for (uint32_t i = 0; i < outFrames; ++i) {
                uint32_t ip = (uint32_t)srcPos;
                float t = srcPos - ip;

                auto S = [&](int idx)->float {
                    if (idx < 0) return input[ch];
                    if ((uint32_t)idx >= *count)
                        return input[(*count - 1) * chan + ch];
                    return input[idx * chan + ch];
                };

                float x0 = S(ip - 1);
                float x1 = S(ip);
                float x2 = S(ip + 1);
                float x3 = S(ip + 2);

                out[i * chan + ch] = hermite(x0,x1,x2,x3,t);
                srcPos += ratio;
            }
        }

        delete[] input;
        *count = outFrames;
        return out;
    }

private:
    static inline float hermite(float x0, float x1, float x2, float x3, float t) {
        float c0 = x1;
        float c1 = 0.5f * (x2 - x0);
        float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3*t + c2)*t + c1)*t + c0;
    }
};

