
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
        player.setSampleRate(sr);
    }

    void setAttack(float a) {env.setAttack(a);}

    void setDecay(float d) {env.setDecay(d);}

    void setSustain(float s) {env.setSustain(s);}

    void setRelease(float r) {env.setRelease(r);}

    void setRootFreq(float freq_) {freq = freq_;}

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
        return out;
    }

    bool isActive() const { return active; }

private:
    SamplePlayer player;
    ADSR env;
    bool active = false;
    bool looping = true;
    float vel = 1.0f;
    float freq = 440.0;
    int midiNote = -1;

    inline double midiToFreq(int midiNote) {
        return freq * std::pow(2.0, (midiNote - 69) / 12.0);
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
