
/*
 * filters.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        filters.h - simulate several filters

****************************************************************/


#pragma once
#include <cmath>
#include <atomic>
#include <vector>

#include "LM_SEM12.h"
#include "WaspFilter.h"
#include "LadderFilter.h"
#include "LM_ACD18Filter.h"


class Filters {
private:
    using ProcFn = float (*)(void*, float);

    template<class T>
    static float call(void* obj, float s) {
        return static_cast<T*>(obj)->process(s);
    }

    struct DspSlot {
        void*  instance;
        ProcFn fn;
    };

    struct DspChain {
        std::vector<DspSlot> slots;
    };

public:
    LM_ACD18Filter tbfilter;
    WaspFilter wasp;
    LadderFilter filterLP;
    ZDFLadderFilter filterHP;
    SEMFilter obf;

    Filters() = default;
    ~Filters() {
        delete activeChain.exchange(nullptr);
        delete retired.exchange(nullptr);
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        tbfilter.setSampleRate(sr);
        wasp.setSampleRate(sr);
        filterLP.setSampleRate(sr);
        filterHP.setSampleRate(sr);
        filterHP.highpass = true;
        obf.setSampleRate(sr);

        if (!isInitied) {
            std::vector<int>n = {8,9,10,11,12};
            rebuildFilterChain(n);
            isInitied = true;
        }
    }

    void noteOn(float targetFreq_) {
        targetFreq = targetFreq_;
        tbfilter.noteOn(targetFreq);
        tbfilter.reset();
        wasp.setMidiNote(targetFreq);
        wasp.reset();
        filterLP.recalcFilter(targetFreq);
        filterLP.reset();
        filterHP.recalcFilter(targetFreq);
        filterHP.reset();
        obf.recalcFilter(targetFreq);
        obf.reset();
    }

    void rebuildFilterChain(const std::vector<int>& newOrder) {
        auto* newChain = new DspChain;
        newChain->slots.reserve(newOrder.size());
        
        for (int id : newOrder) {
            switch(id) {
                case 8:  newChain->slots.push_back({&tbfilter, &call<LM_ACD18Filter>}); break;
                case 9:  newChain->slots.push_back({&wasp, &call<WaspFilter>}); break;
                case 10: newChain->slots.push_back({&filterLP, &call<LadderFilter>}); break;
                case 11: newChain->slots.push_back({&filterHP, &call<ZDFLadderFilter>}); break;
                case 12: newChain->slots.push_back({&obf, &call<SEMFilter>}); break;
            }
        }

        DspChain* old = activeChain.exchange(newChain, std::memory_order_acq_rel);
        retire(old);
    }



    inline float process(float x) {
        DspChain* c = activeChain.load(std::memory_order_acquire);
        for (auto& m : c->slots)
            x = m.fn(m.instance, x);
        return x;
    }

private:
    double sampleRate = 44100.0;
    float targetFreq = 440.0f;
    bool isInitied = false;
    std::atomic<DspChain*> activeChain { nullptr };
    std::atomic<DspChain*> retired { nullptr };

    void retire(DspChain* old) {
        DspChain* prev = retired.exchange(old, std::memory_order_acq_rel);
        delete prev;
    }

};
