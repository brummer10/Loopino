
/*
 * SamplePlayer.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>


#ifndef SAMPLEPLAYER_H
#define SAMPLEPLAYER_H

/****************************************************************
                ADSR: Attack Decay Sustain Release
****************************************************************/

class ADSR {
public:
    ADSR(double sr = 44100.0)
        : sampleRate(sr) {}

    void setParams(float a, float d, float s, float r) {
        attack = std::max(a, 0.001f);
        decay = std::max(d, 0.001f);
        sustain = std::clamp(s, 0.001f, 1.0f);
        release = std::max(r, 0.001f);
        attackCoef = recalcCoef(attack);
        decayCoef = recalcCoef(decay);
        releaseCoef = recalcCoef(release);
    }

    void setSampleRate(double sr) {sampleRate = sr;}

    void setAttack(float a) {
        attack = std::max(a, 0.001f);
        attackCoef = recalcCoef(attack);
    }

    void setDecay(float d) {
        decay = std::max(d, 0.001f);
        decayCoef = recalcCoef(decay);
    }

    void setSustain(float s) {
        sustain = std::clamp(s, 0.001f, 1.0f);
    }

    void setRelease(float r) {
        release = std::max(r, 0.001f);
        releaseCoef = recalcCoef(release);
    }

    void noteOn() {state = ATTACK;}

    void noteOff() {state = RELEASE;}

    float process() {
        switch (state) {
        case ATTACK:
            level += attackCoef * (1.0f - level);
            if (level >= 0.999f) {
                level = 1.0f;
                state = DECAY;
            }
            break;

        case DECAY:
            level += decayCoef * (sustain - level);
            if (level <= sustain) {
                level = sustain;
                state = SUSTAIN;
            }
            break;

        case SUSTAIN:
            break;

        case RELEASE:
            level += releaseCoef * (0.0f - level);
            if (level <= 0.0001f) {
                level = 0.0f;
                state = IDLE;
            }
            break;

        case IDLE:
        default:
            break;
        }

        return level;
    }

    bool isActive() const { return state != IDLE; }

private:
    enum State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    State state = IDLE;

    double sampleRate;
    float attack = 0.01f;
    float decay = 0.1f;
    float sustain = 0.8f;
    float release = 0.3f;

    float level = 0.0f;

    float attackCoef  = 0.0f;
    float decayCoef   = 0.0f;
    float releaseCoef = 0.0f;

    float recalcCoef(float val) {
        return 1.0f - std::exp(-1.0f / (val * sampleRate));
    }

};

/****************************************************************
            SampleBank: store Samples & Metadata
****************************************************************/

struct SampleInfo {
    std::vector<float> data;
    double sourceRate = 44100.0;
    double rootFreq = 440.0;
};

class SampleBank {
public:
    void addSample(const SampleInfo& s) { samples.push_back(s); }

    void clear() {samples.clear(); }

    const SampleInfo* getSample(size_t index) const {
        if (index >= samples.size()) return nullptr;
        return &samples[index];
    }

    size_t size() const { return samples.size(); }

private:
    std::vector<SampleInfo> samples;
};

/****************************************************************
        SamplePlayer: play voice with Resampling & Loop
****************************************************************/

class SamplePlayer {
public:
    SamplePlayer(double outputRate = 44100.0)
        : srOut(outputRate), phase(0.0), phaseInc(0.0),
          loopStart(0), loopEnd(0), looping(false), sample(nullptr) {}

    void setSampleRate(double sr) {srOut = sr;}

    void setSample(const std::vector<float>* s, double sourceRate) {
        sample = s;
        srIn = sourceRate;
        if (sample && !sample->empty())
            loopEnd = sample->size() - 1;
        phase = 0.0;
    }

    void setFrequency(double targetFreq, double rootFreq) {
        if (!sample || srIn <= 0.0) return;
        double ratio = targetFreq / rootFreq;
        phaseInc = ratio * (srIn / srOut);
    }

    void setLoop(size_t start, size_t end, bool enabled = true) {
        loopStart = std::min(start, end);
        loopEnd = std::max(start, end);
        looping = enabled;
    }

    void reset() { phase = 0.0; }

    float process() {
        if (!sample || sample->empty()) return 0.0f;

        const auto& s = *sample;
        const size_t size = s.size();

        size_t i0 = static_cast<size_t>(phase);
        size_t i1 = std::min(i0 + 1, size - 1);
        i0 = std::clamp<size_t>(i0, 0, s.size() -1);
        i1 = std::clamp<size_t>(i1, 0, s.size() -1);
        double frac = phase - (double)i0;

        float val = static_cast<float>(s[i0] + frac * (s[i1] - s[i0]));

        phase += phaseInc;

        if (looping) {
            if (phase >= loopEnd) {
                phase = loopStart + std::fmod(phase - loopStart, (loopEnd - loopStart));
            }
        } else {
            if (phase >= size)
                return 0.0f; // end of sample
        }

        return val;
    }

private:
    double srIn = 44100.0;
    double srOut = 44100.0;
    double phase = 0.0;
    double phaseInc = 0.0;
    size_t loopStart;
    size_t loopEnd;
    bool looping;
    const std::vector<float>* sample = nullptr;
};

/****************************************************************
        4-Pole Ladder Filter (Moog style)
****************************************************************/

struct LadderFilter {
    double z1=0, z2=0, z3=0, z4=0; // 4 integrator stages
    double cutoff = 1000.0;
    double resonance = 0.0;
    double sampleRate = 44100.0;
    double feedback = 1.0;
    double tunning = 0.5;
    bool filterOff = true;

    void setSampleRate(double sr) {sampleRate = sr;}

    void reset() { z1=z2=z3=z4=0.0; }

    inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline double process(double in) {
        if (filterOff) return in;
        // Feedback from 4th stage
        in -= z4 * feedback;
        in = tanh_fast(in); // std::tanh

        // 4 cascaded one-pole filters
        z1 += tunning * (in - z1);
        z2 += tunning * (z1 - z2);
        z3 += tunning * (z2 - z3);
        z4 += tunning * (z3 - z4);

        // resonance loudness compensation
        double resGainComp = 1.0 + (resonance * resonance) * 2.0;

        return z4 * resGainComp;
    }
};

/****************************************************************
        SampleVoice: play one voice
****************************************************************/

class SampleVoice {
public:
    SampleVoice(double sr = 44100.0)
        : env(sr) {}

    // Attack, Decay, Sustain, Release 
    void setADSR(float a, float d, float s, float r) {
        env.setParams(a, d, s, r);
    }

    void setSampleRate(double sr) {
        env.setSampleRate(sr);
        filter.setSampleRate(sr);
        player.setSampleRate(sr);
    }

    void setAttack(float a) {env.setAttack(a);}

    void setDecay(float d) {env.setDecay(d);}

    void setSustain(float s) {env.setSustain(s);}

    void setRelease(float r) {env.setRelease(r);}

    void setRootFreq(float freq_) {freq = freq_;}

    void setCutoff(int value) {
        value = std::clamp(value, 0, 127);
        ccCutoff = value;
        if (ccCutoff == 127 && ccReso == 0) filter.filterOff = true;
        else filter.filterOff = false;
    }

    void setReso(int value) {
        value = std::clamp(value, 0, 127);
        ccReso   = value;
        if (ccCutoff == 127 && ccReso == 0) filter.filterOff = true;
        else filter.filterOff = false;
    }

    void setKeyTracking(float amt) {
        keyTracking = std::clamp(amt, 0.0f, 1.0f);
    }

    void noteOn(int midiNote, float velocity,
                const std::vector<float>* sampleData,
                double sourceRate, double rootFreq, bool looping = true) {

        this->midiNote = midiNote;
        active = true;
        vel = velocity;
        player.setSample(sampleData, sourceRate);
        player.setFrequency(midiToFreq(midiNote), rootFreq);
        player.setLoop(0, sampleData->size() - 1, looping);
        player.reset();
        recalcFilter();
        filter.reset();
        env.noteOn();
    }

    void noteOff(int midiNote) {
        if (active && midiNote == this->midiNote) {
            env.noteOff();
        }
    }

    void noteOff() {
        if (active) {
            env.noteOff();
        }
    }

    float process() {
        if (!active) return 0.0f;
        float amp = env.process();
        float out = player.process() * vel * amp;
        if (!env.isActive()) active = false;
        return filter.process(out);
    }

    void getAnalyseBuffer(float *abuf, int frames,
                        const std::vector<float>* sampleData,
                        double sourceRate, double rootFreq) {

        player.setSample(sampleData, sourceRate);
        player.setFrequency(440.0, rootFreq);
        player.setLoop(0, sampleData->size() - 1, true);
        player.reset();
        for (int i = 0; i < frames; i++) {
            abuf[i] = player.process();
        }
    }

    bool isActive() const { return active; }

private:
    SamplePlayer player;
    ADSR env;
    LadderFilter filter;

    bool active = false;
    bool looping = true;
    float vel = 1.0f;
    float freq = 440.0f;
    int midiNote = -1;

    int ccCutoff = 127;
    int ccReso = 0;
    float keyTracking = 0.5f;

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

    inline double midiToFreq(int midiNote) {
        return  freq * std::pow(2.0, (midiNote - 69) / 12.0);
    }

    void recalcFilter() {
        if (filter.filterOff) return;
        // Base cutoff from CC74
        float baseCut = ccToFreq(ccCutoff);

        // Key tracking
        float semi = midiNote - 60;
        float keyFactor = std::pow(2.0f, (semi / 12.0f) * keyTracking);
        double finalCut = std::clamp(baseCut * keyFactor, minFreq, maxFreq);

        filter.cutoff = finalCut;

        // Resonance from CC71 (scaled to 0..1 for ladder)
        double Q = ccToQ(ccReso);
        filter.resonance = std::clamp((Q - 0.5) * 0.22, 0.0, 0.95); // safe mapping

        // Normalized cutoff + simple tuning correction
        double f = filter.cutoff / filter.sampleRate;
        f = std::clamp(f, 0.0, 0.45);
        filter.tunning = 2.0 * f;                                    // cheap approximate tuning
        filter.feedback = filter.resonance * 4.0 * (1.0 - 0.15 * filter.tunning * filter.tunning); // resonance feedback
    }

};


/****************************************************************
        PolySynth: play multiple voices
****************************************************************/

class PolySynth {
public:
    PolySynth() {}

    void init(double sr, int maxVoices = 8) {
        voices.resize(maxVoices);
        sampleRate = sr; 
        masterGain = (1.0f / std::sqrt((float)maxVoices));
        playLoop = false;
        for (auto& v : voices) {
            v.setADSR(0.01f, 0.2f, 0.7f, 0.4f); // Attack, Decay, Sustain, Release (in Seconds)
            v.setSampleRate(sr);
        }
    }

    void setLoop(bool loop) { playLoop = loop;}

    void setLoopBank(const SampleBank* lbank) {loopBank = lbank;}

    void setBank(const SampleBank* sbank) {sampleBank = sbank;}

    void getAnalyseBuffer(float *abuf, int frames) {
        const auto s = loopBank->getSample(0);
        voices[voices.size() - 1].getAnalyseBuffer(abuf, frames, &s->data, s->sourceRate, s->rootFreq);
    }

    void setAttack(float a) {
        for (auto& v : voices) {
            v.setAttack(a);
        }
    }

    void setDecay(float d) {
        for (auto& v : voices) {
            v.setDecay(d);
        }
    }

    void setSustain(float s) {
        for (auto& v : voices) {
            v.setSustain(s);
        }
    }

    void setRelease(float r) {
        for (auto& v : voices) {
            v.setRelease(r);
        }
    }

    void setRootFreq(float freq) {
        for (auto& v : voices) {
            v.setRootFreq(freq);
        }
    }

    void setCutoff(int value) {
        for (auto& v : voices) {
            v.setCutoff(value);
        }
    }

    void setReso(int value) {
        for (auto& v : voices) {
            v.setReso(value);
        }
    }

    void setKeyTracking(float amt) {
        float value = std::clamp(amt, 0.0f, 1.0f);
        for (auto& v : voices) {
            v.setKeyTracking(value);
        }
    }

    void noteOn(int midiNote, float velocity, size_t sampleIndex = 0) {
        if (playLoop ? !loopBank : !sampleBank) return;
        const auto* s = playLoop ? loopBank->getSample(sampleIndex) : sampleBank->getSample(sampleIndex);
        if (!s) return;

        // Find free voice
        for (auto& v : voices) {
            if (!v.isActive()) {
                v.noteOn(midiNote, velocity, &s->data, s->sourceRate, s->rootFreq, playLoop);
                return;
            }
        }

        // steal voice (simple)
        voices[0].noteOn(midiNote, velocity, &s->data, s->sourceRate, s->rootFreq, playLoop);
    }

    void noteOff(int midiNote) {
        for (auto& v : voices) v.noteOff(midiNote);
    }

    void allNoteOff() {
        for (auto& v : voices) v.noteOff();
    }

    float process() {
        float mix = 0.0f;
        for (auto& v : voices) {
            if (v.isActive()) {
                mix += v.process();
            }
        }
        // avoid clipping
        mix *= masterGain;
        return mix;
    }

private:
    std::vector<SampleVoice> voices;
    const SampleBank* sampleBank = nullptr;
    const SampleBank* loopBank = nullptr;
    double sampleRate;
    float masterGain;
    bool playLoop;
};

#endif
