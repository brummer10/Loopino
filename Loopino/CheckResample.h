/*
 * CheckResample.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2024 brummer <brummer@web.de>
 */


#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <cmath>
#include <cstring>
#include <zita-resampler/resampler.h>


#pragma once

#ifndef CHECKRESAMPLE_H
#define CHECKRESAMPLE_H

class CheckResample : Resampler{
public:
    CheckResample() {}

    float *checkSampleRate(uint32_t *count, uint32_t chan, float *impresp,
                            uint32_t imprate, uint32_t samplerate) {
        if (imprate != samplerate) {
            return process(imprate, *count, impresp, chan, samplerate, count, 32);
        }
        return impresp;
    }

    ~CheckResample() {
        clear();
    }

private:

    static uint32_t gcd (uint32_t a, uint32_t b) {
        if (a == 0) return b;
        if (b == 0) return a;
        while (1) {
            if (a > b) {
                a = a % b;
                if (a == 0) return b;
                if (a == 1) return 1;
            } else {
                b = b % a;
                if (b == 0) return a;
                if (b == 1) return 1;
            }
        }
        return 1;
    }

    float* process(int32_t fs_inp, int32_t ilen, float *input, uint32_t chan, 
                    int32_t fs_outp, uint32_t *olen, const int32_t qual){
        uint32_t d = gcd(fs_inp, fs_outp);
        uint32_t ratio_a = fs_inp / d;
        uint32_t ratio_b = fs_outp / d;

        clear();
        if (setup(fs_inp, fs_outp, chan, qual) != 0) {
            return 0;
        }
        // pre-fill with k/2-1 zeros
        int32_t k = inpsize();
        inp_count = k/2-1;
        inp_data = 0;
        out_count = 1; // must be at least 1 to get going
        out_data = 0;
        if (Resampler::process() != 0) {
            return 0;
        }
        inp_count = ilen;
        uint32_t nout = out_count = (ilen * ratio_b + ratio_a - 1) / ratio_a;
        inp_data = input;
        float *p = out_data = new float[out_count*chan];
        if (Resampler::process() != 0) {
            delete[] p;
            return 0;
        }
        inp_data = 0;
        inp_count = k/2;
        if (Resampler::process() != 0) {
            delete[] p;
            return 0;
        }
       // assert(inp_count == 0);
       // assert(out_count <= 1);
        *olen = nout - out_count;
        #ifndef NDEBUG
        if (inp_count)
            printf("resampled from %i to: %i, lost %i samples\n",fs_inp, fs_outp, inp_count);
        #endif
        delete[] input;
        input = nullptr;
        return p;
    }
};

#endif

