
/*
 * DcBlocker.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

/****************************************************************
        DcBlocker.h dc blocker at 5 Hz, 
        
****************************************************************/

#include <cmath>
#include <algorithm>

class DcBlocker {
private:
    float factor;
    float statef[2];
    float stateo[2];

public:
    DcBlocker() = default;
    ~DcBlocker() = default;

    void setSampleRate(float sample_rate) {
        factor = 31.415926f / std::min<float>(1.92e+05f, std::max<float>(1.0f, sample_rate));
        for (int l0 = 0; l0 < 2; l0 = l0 + 1) statef[l0] = 0.0f;
        for (int l1 = 0; l1 < 2; l1 = l1 + 1) stateo[l1] = 0.0f;
    }

    inline float process(float in) {
        statef[0] = statef[1] + factor * stateo[1];
        stateo[0] = in - statef[0];
        float out = stateo[0];
        statef[1] = statef[0];
        stateo[1] = stateo[0];
        return out;
    }
};
