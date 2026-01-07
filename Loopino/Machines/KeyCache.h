
/*
 * KeyCache.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        KeyCache.h  - stretch the rootKey for unison length across 
                      the Octave spectrum, cache one Key peer Octave (8)
                      to re-pitch the MIDI notes between the Root Keys
                      from there. Max jitter stays below 0.2 ms
****************************************************************/

#pragma once
#include <map>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <rubberband/RubberBandStretcher.h>

#include "machines.h"

class KeyCache
{
public:
    KeyCache() { worker = std::thread([this]{ workerLoop(); }); }
    ~KeyCache() { stop=true; cv.notify_all(); worker.join(); }

    Machines machines;
    Machines loopMachines;

    void setLM_MIR8OnOff(bool on) {
        machines.mrg.setOnOff(on);
        loopMachines.mrg.setOnOff(on);
        rebuild(); 
    }
    void setLM_MIR8Drive(float d) {
        machines.mrg.setDrive(d);
        loopMachines.mrg.setDrive(d);
    }
    void setLM_MIR8Amount(float a) {
        machines.mrg.setAmount(a);
        loopMachines.mrg.setAmount(a);
    }

    void setEmu_12OnOff(bool on) {
        machines.emu_12.setOnOff(on);
        loopMachines.emu_12.setOnOff(on);
        rebuild();
    }
    void setEmu_12Drive(float d) {
        machines.emu_12.setDrive(d);
        loopMachines.emu_12.setDrive(d);
    }
    void setEmu_12Amount(float a) {
        machines.emu_12.setAmount(a);
        loopMachines.emu_12.setAmount(a);
    }

    void setLM_CMP12OnOff(bool on) {
        machines.cmp12dac.setOnOff(on);
        loopMachines.cmp12dac.setOnOff(on);
        rebuild();
    }
    void setLM_CMP12Drive(float d) {
        machines.cmp12dac.setDrive(d);
        loopMachines.cmp12dac.setDrive(d);
    }
    void setLM_CMP12Ratio(float r) {
        machines.cmp12dac.setRatio(r);
        loopMachines.cmp12dac.setRatio(r);
    }

    void setStudio_16OnOff(bool on)  {
        machines.studio16.setOnOff(on);
        loopMachines.studio16.setOnOff(on);
        rebuild();
        }
    void setStudio_16Drive(float d)  {
        machines.studio16.setDrive(d);
        loopMachines.studio16.setDrive(d);
    }
    void setStudio_16Warmth(float w) {
        machines.studio16.setWarmth(w);
        loopMachines.studio16.setWarmth(w);
    }
    void setStudio_16HfTilt(float h) {
        machines.studio16.setHfTilt(h);
        loopMachines.studio16.setHfTilt(h);
    }

    void setVFX_EPSOnOff(bool on)  {
        machines.eps.setOnOff(on);
        loopMachines.eps.setOnOff(on);
        rebuild();
    }
    void setVFX_EPSDrive(float d)  {
        machines.eps.setDrive(d);
        loopMachines.eps.setDrive(d);
    }

    void rebuild() {
        if (!root) return;
        clear();
        prewarmOctaves();
        prewarmQuints();
        makeLoop();
    }

    void prewarmOctaves() {
        for (int note = 24; note <= 108; note += 12)
            request(note);
    }

    void prewarmQuints() {
        for (int note = 24; note <= 108; note += 12) {
            int q = note + 7;
            if (q >= 0 && q <= 127)
                request(q);
        }
    }

    void setRoot(std::shared_ptr<const SampleInfo> s) {
        {
            std::lock_guard<std::mutex> g(qm);
            root = s;
            while(!jobs.empty()) jobs.pop();
            pending.clear();
        }
        {
            std::lock_guard<std::mutex> g2(cacheMutex);
            cache.clear();
        }
        prewarmOctaves();
        prewarmQuints();
    }

    void setLoopRoot(std::shared_ptr<const SampleInfo> s) {
        loop_cache = s;
    }

    void makeLoop() {
        if (!loop_cache) return;
        auto s = std::make_shared<SampleInfo>();
        s->data = std::move(loop_cache->data);
        s->rootFreq = loop_cache->rootFreq;
        s->sourceRate = loop_cache->sourceRate;
        loopMachines.setSampleRate(s->sourceRate);
        for(auto& x : s->data)
            x = loopMachines.process(x);
        loop = s;
    }

    std::shared_ptr<const SampleInfo> getLoop() {
        if (!loop) return nullptr;
        return loop;
    }

    std::shared_ptr<const SampleInfo> getNearestOctave(int note) {
        int rootMidi = 48; //freqToMidi(130.81); // C3
        int o = int(std::round((note - rootMidi) * 0.083333333)); // 12.0));
        int base = rootMidi + o * 12;

        if (auto s = get(base)) { return s; }
        if (auto s = get(base + 12)) { return s; }
        if (auto s = get(base - 12)) { return s; }
        return nullptr;
    }

    std::shared_ptr<const SampleInfo> getNearest(int note) {
        std::lock_guard<std::mutex> g(cacheMutex);

        if (cache.empty()) return nullptr;
        auto it = cache.lower_bound(note);
        if (it == cache.begin()) return it->second;
        if (it == cache.end()) return std::prev(it)->second;
        auto hi = it;
        auto lo = std::prev(it);
        if (std::abs(hi->first - note) < std::abs(note - lo->first)) {
            return hi->second;
        } else {
            return lo->second;
        }
    }

    std::shared_ptr<const SampleInfo> get(int note) {
        std::lock_guard<std::mutex> g(cacheMutex);
        auto it = cache.find(note);
        return it!=cache.end() ? it->second : nullptr;
    }

    void request(int note) {
        std::lock_guard<std::mutex> g(qm);
        if (pending.insert(note).second)
            jobs.push(note);
        cv.notify_one();
    }

    void clear() {
        {
            std::lock_guard<std::mutex> g(qm);
            while(!jobs.empty()) jobs.pop();
            pending.clear();
        }
        {
            std::lock_guard<std::mutex> g2(cacheMutex);
            cache.clear();
        }
    }

private:
    static constexpr int CHUNK = 4096;
    static constexpr auto WORKER_YIELD = std::chrono::microseconds(250);
    std::shared_ptr<const SampleInfo> root;
    std::shared_ptr<const SampleInfo> loop;
    std::shared_ptr<const SampleInfo> loop_cache;
    std::map<int,std::shared_ptr<SampleInfo>> cache;
    std::set<int> pending;

    std::queue<int> jobs;
    std::mutex qm;
    std::mutex cacheMutex;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> stop{false};

    inline double midiToFreq(int midiNote) {
        return  440.0f * std::pow(2.0, (midiNote - 69 ) / 12.0);
    }

    inline int freqToMidi(double f) {
        return int(std::round(69.0 + 12.0 * std::log2(f / 440.0)));
    }

    void workerLoop() {
        while(!stop) {
            int note = 0;
            {
                std::unique_lock<std::mutex> lk(qm);
                cv.wait(lk,[&]{return stop||!jobs.empty();});
                if(stop) break;
                if (jobs.size()) {
                    note=jobs.front(); jobs.pop();
                }
            }
            if (note)build(note);
        }
    }

    void build(int note) {

        RubberBand::RubberBandStretcher rb(root->sourceRate,1,
            RubberBand::RubberBandStretcher::OptionProcessOffline|
            RubberBand::RubberBandStretcher::OptionEngineFiner|
            RubberBand::RubberBandStretcher::OptionFormantPreserved |
            RubberBand::RubberBandStretcher::OptionPhaseIndependent);

        double ratio = midiToFreq(note)/root->rootFreq;
        //rb.reset();
        rb.setTimeRatio(ratio);
        rb.setPitchScale(1.0);
        const float* sin[1] = { root->data.data() };
        rb.study(sin, root->data.size(), true);

        rb.setExpectedInputDuration(root->data.size());
        rb.setMaxProcessSize(root->sourceRate * 4);
        const float* in[1];
        std::vector<float> out;
        out.reserve(int(root->data.size() * ratio) + 1024);
        int pos = 0;
        while ((size_t)pos < root->data.size()) {
            int n = std::min<int>(CHUNK, int(root->data.size() - pos));
            in[0] = root->data.data() + pos;
            rb.process(in, n, false);
            int avail;
            while ((avail = rb.available()) > 0) {
                size_t old = out.size();
                out.resize(old + avail);
                float* chans[1] = { out.data() + old };
                rb.retrieve(chans, avail);
            }
            pos += n;
        }
        // flush
        rb.process(nullptr, 0, true);
        int idle = 0;
        while (idle < 4) {
            int avail = rb.available();
            if (avail > 0) {
                idle = 0;
                size_t old = out.size();
                out.resize(old + avail);
                float* chans[1] = { out.data() + old };
                rb.retrieve(chans, avail);
            }
            else idle++;
        }

        auto s = std::make_shared<SampleInfo>();
        s->data = std::move(out);
        s->rootFreq = root->rootFreq;
        s->sourceRate = root->sourceRate;

        machines.setSampleRate(root->sourceRate);
        for(auto& x : s->data)
            x = machines.process(x);

        std::lock_guard<std::mutex> g(cacheMutex);
        cache[note] = s;
        {
            std::lock_guard<std::mutex> g(qm);
            pending.erase(note);
        }
        std::this_thread::sleep_for(WORKER_YIELD);
    }
};
