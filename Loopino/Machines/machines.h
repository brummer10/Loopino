
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
#include <atomic>
#include <vector>

#include "BrickWall.h"
#include "LM_CMP12Dac.h"
#include "LM_MIR8Brk.h"
#include "Staircase.h"
#include "LM_S1K16.h"
#include "VFX_EPS_CLASSIC.h"
#include "TimeMachine.h"



class Machines {
private:
    using ProcFn = void (*)(void*, std::vector<float>&);

    template<class T>
    static void call(void* obj, std::vector<float>& s) {
        return static_cast<T*>(obj)->processV(s);
    }

    struct DspSlot {
        void*  instance;
        ProcFn fn;
    };

    struct DspChain {
        std::vector<DspSlot> slots;
    };

public:
    Brickwall bw;
    LM_CMP12Dac cmp12dac;
    LM_MIR8Brk mrg;
    LM_EII12 emu_12;
    LM_S1K16 studio16;
    VFX_EPS_CLASSIC eps;
    TimeMachine tm;

    Machines() = default;
    ~Machines() {
        delete activeChain.exchange(nullptr);
        delete retired.exchange(nullptr);
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        bw.setSampleRate(sr);
        cmp12dac.setSampleRate(sr);
        mrg.setSampleRate(sr);
        emu_12.setSampleRate(sr);
        studio16.setSampleRate(sr);
        eps.setSampleRate(sr);
        if (!isInitied) {
            std::vector<int>n = {20,21,22,23,24,25};
            rebuildChain(n);
            isInitied = true;
        }
    }

    void applyState() {
        cmp12dac.applyState();
        mrg.applyState();
        emu_12.applyState();
        studio16.applyState();
        tm.applyState();
        eps.applyState();
    }

    bool rebuildChain(const std::vector<int>& newOrder) {
        std::vector<int> newActive;
        buildActiveSignature(newOrder, newActive);
        bool activeChanged = (newActive != lastActiveOrder);
        lastActiveOrder = newActive;
        auto* newChain = new DspChain;
        newChain->slots.reserve(newOrder.size());
        
        newChain->slots.push_back({&bw, &call<Brickwall>});
        for (int id : newOrder) {
            switch(id) {
                case 20: newChain->slots.push_back({&mrg, &call<LM_MIR8Brk>}); break;
                case 21: newChain->slots.push_back({&emu_12, &call<LM_EII12>}); break;
                case 22: newChain->slots.push_back({&cmp12dac, &call<LM_CMP12Dac>}); break;
                case 23: newChain->slots.push_back({&studio16, &call<LM_S1K16>}); break;
                case 24: newChain->slots.push_back({&tm, &call<TimeMachine>}); break;
                case 25: newChain->slots.push_back({&eps, &call<VFX_EPS_CLASSIC>}); break;
            }
        }

        DspChain* old = activeChain.exchange(newChain, std::memory_order_acq_rel);
        retire(old);
        return activeChanged;
    }



    inline void process(std::vector<float>& s) {
        DspChain* c = activeChain.load(std::memory_order_acquire);
        for (auto& m : c->slots)
            m.fn(m.instance, s);
    }

private:
    double sampleRate = 44100.0;
    bool isInitied = false;
    std::atomic<DspChain*> activeChain { nullptr };
    std::atomic<DspChain*> retired { nullptr };
    std::vector<int> lastActiveOrder;

    bool isActive(int id) const {
        switch(id) {
            case 0: return bw.getOnOff();
            case 20: return mrg.getOnOff();
            case 21: return emu_12.getOnOff();
            case 22: return cmp12dac.getOnOff();
            case 23: return studio16.getOnOff();
            case 24: return tm.getOnOff();
            case 25: return eps.getOnOff();
        }
        return false;
    }

    void buildActiveSignature(const std::vector<int>& order,
                              std::vector<int>& out) {
        out.clear();
        for (int id : order) {
            if (isActive(id))
                out.push_back(id);
        }
    }

    void retire(DspChain* old) {
        DspChain* prev = retired.exchange(old, std::memory_order_acq_rel);
        delete prev;
    }

};
