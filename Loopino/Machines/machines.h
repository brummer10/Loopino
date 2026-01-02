
/*
 * machines.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        machines.h - simulate several sampler machines

****************************************************************/


#pragma once
#include <cmath>

#include "BrickWall.h"
#include "LM_CMP12Dac.h"
#include "LM_MIR8Brk.h"
#include "Staircase.h"
#include "LM_S1K16.h"
#include "VFX_EPS_CLASSIC.h"

class Machines {
public:
    Brickwall bw;
    LM_CMP12Dac cmp12dac;
    LM_MIR8Brk mrg;
    LM_EII12 emu_12;
    LM_S1K16 studio16;
    VFX_EPS_CLASSIC eps;

    void setSampleRate(double sr) {
        sampleRate = sr;
        bw.setSampleRate(sr);
        cmp12dac.setSampleRate(sr);
        mrg.setSampleRate(sr);
        emu_12.setSampleRate(sr);
        studio16.setSampleRate(sr);
        eps.setSampleRate(sr);
    }

    float process(float out) {
        out = bw.process(out);
        out = cmp12dac.process(out);
        out = mrg.process(out);
        out = emu_12.process(out);
        out = studio16.process(out);
        epsPhase += 0.000015;
        if (epsPhase >= 1.0) epsPhase -= 1.0;
        out = eps.process(out, epsPhase);
        return out;
    }

private:
    double sampleRate = 44100.0;
    double epsPhase = 0.0;

};
