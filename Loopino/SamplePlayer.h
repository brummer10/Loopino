
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
    void addSample(std::shared_ptr<const SampleInfo> s) {
        samples.push_back(std::move(s));
    }

    std::shared_ptr<const SampleInfo> getSample(size_t index) const {
        if (index >= samples.size())
            return nullptr;
        return samples[index];
    }

    size_t size() const { return samples.size(); }

private:
    std::vector<std::shared_ptr<const SampleInfo>> samples;
};

/****************************************************************
        SamplePlayer: play voice with Resampling & Loop
****************************************************************/

class SamplePlayer {
public:
    std::atomic<const SampleInfo*> sample { nullptr };
    std::shared_ptr<const SampleInfo> currentSampleOwner;
    double pmPhase = 0.0;
    double pmFreq = 0.0;
    double pmDepthNorm = 0.0;
    float vibRate = 5.0f;      // Hz
    float vibDepth = 0.0f;     // 0..1 (depth)
    float tremRate = 5.0f;     // Hz
    float tremDepth = 0.0f;    // 0..1 (depth)


    SamplePlayer(double outputRate = 44100.0)
        :  sample(nullptr), srOut(outputRate), phase(0.0), phaseInc(0.0),
          loopStart(0), loopEnd(0), looping(false) {}

    void setSampleRate(double sr) {srOut = sr;}

    void setSample(std::shared_ptr<const SampleInfo> s, double sourceRate) {
        currentSampleOwner = s;
        sample.store(s.get(), std::memory_order_release);
        srIn = sourceRate;
        const SampleInfo* p = sample.load(std::memory_order_acquire);
        if (p) loopEnd = p->data.size() > 0 ? p->data.size() - 1 : 0;
        phase = 0.0;
        pmPhase = 0.0f;
        driftState = 0.0f;
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

    void setPmMode(int m) {
        switch(m) {
            case 0:
                pmShape = PMShape::SoftSine;
                break;
            case 1:
                pmShape = PMShape::Triangle;
                break;
            case 2:
                pmShape = PMShape::Drift;
                break;
        }
    }

    inline float tanh_fast(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float smoothPM(float x, float cutoff = 0.15f) {
        pm_s1 += cutoff * (x - pm_s1);
        pm_s2 += cutoff * (pm_s1 - pm_s2);
        return pm_s2;
    }

    inline float pmSoftSine(float p) {
        return tanh_fast(1.5f * sinf(p * 2.0f * M_PI));
    }

    inline float saturatePM(float x) {
        return tanh_fast(x * 0.5f) * (float)M_PI;
    }

    inline float pmTriangle(float p) {
        float t = p - floorf(p);
        float tri = (t < 0.5f) ? (t * 4.0f - 1.0f) : ((1.0f - t) * 4.0f - 1.0f);
        return tri;
    }

    inline float fastNoise(uint32_t& state) {
        state = state * 1664525u + 1013904223u;
        return float(int32_t(state >> 9)) * (1.0f / 8388607.0f);
    }

    inline float pmDrift(float p, float d) {
        float noise = fastNoise(noiseState);
        driftState = driftCoeff * driftState + (1.0f - driftCoeff) * noise;
        float phaseMod = sinf(p * 2.0f * M_PI);
        float mixed = driftState + phaseMod * (d * 0.25f);
        return mixed;
    }

    inline float pmJuno(float p, float d) {
        float noise = fastNoise(noiseState);
        driftState = driftCoeff * driftState + (1.0f - driftCoeff) * noise;
        float trend = sinf(p * 2.0f * M_PI) * d * 0.1f;
        driftState += trend;
        driftState *= 0.9995f;
        return driftState;
    }

    inline float advancePhase(float ph, float rate, float sr) {
        ph += rate / sr;
        if (ph >= 1.0f) ph -= 1.0f;
        return ph;
    }

    void reset() { phase = 0.0; }

    inline float hermite_interpolation(const float* s, float t) {
        const float xm1 = s[-1];
        const float x0  = s[0];
        const float x1  = s[1];
        const float x2  = s[2];

        float c0 = x0;
        float c1 = 0.5f * (x1 - xm1);
        float c2 = xm1 - 2.5f*x0 + 2.0f*x1 - 0.5f*x2;
        float c3 = 0.5f * (x2 - xm1) + 1.5f*(x0 - x1);

        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    float process() {
        const SampleInfo* p = sample.load(std::memory_order_acquire);
        if (!p || p->data.empty())
            return 0.0f;

        const auto& s = p->data;
        const size_t size = s.size();
        if (size == 0) return 0.0f;
        float pm = 0.0f;

        // Phase Modulators
        if (pmFreq > 0.01f && pmDepthNorm > 0.0f) {
            pmPhase = advancePhase(pmPhase, pmFreq, srOut);
            float pmDepth = pmDepthNorm * pmDepthSamplesMax;
            float rawPM = 0.0f;
            switch(pmShape) {
                case PMShape::SoftSine:
                    rawPM = pmSoftSine(pmPhase) * pmDepth;
                    break;
                case PMShape::Triangle:
                    rawPM = pmTriangle(pmPhase) * pmDepth;
                    break;
                case PMShape::Drift:
                    rawPM = pmDrift(pmPhase, pmDepth);
                    break;
                case PMShape::Juno:
                    rawPM = pmJuno(pmPhase, pmDepth);
                    break;
            }
            float mod = smoothPM(rawPM);
            pm = saturatePM(mod);
        }
        double readPos = phase + pm;
        float phaseIncMod = phaseInc;
        float gainMod = 1.0f;

        // Vibrato (Pitch LFO)
        if (vibDepth > 0.0001f && vibRate > 0.1f) {
            vibPhase = advancePhase(vibPhase, vibRate, srOut);
            float lfo = pmSoftSine(vibPhase);
            float vib = lfo * vibDepth * 0.01f;
            phaseIncMod *= (1.0f + vib);
        }

        // Tremolo (Amplitude LFO)
        if (tremDepth > 0.0001f && tremRate > 0.1f) {
            tremPhase = advancePhase(tremPhase, tremRate, srOut);
            float lfo = pmSoftSine(tremPhase);
            float lfoUni = 0.5f * (lfo + 1.0f);
            gainMod = 1.0f - tremDepth * (1.0f - lfoUni);
        }

        if (looping) {
            double loopLen = std::max(1.0, (double)(loopEnd - loopStart));
            while (readPos <  loopStart) readPos += loopLen;
            while (readPos >= loopEnd)   readPos -= loopLen;
        } else {
            readPos = std::clamp(readPos, 0.0, (double)size - 1.0);
        }

        size_t i = (size_t)readPos;
        float frac = readPos - (double)i;
        size_t i_m1 = (i == 0 ? 0 : i - 1);
        size_t i_p1 = (i + 1 < size ? i + 1 : size - 1);
        size_t i_p2 = (i + 2 < size ? i + 2 : size - 1);
        float tmp[4] = {s[i_m1], s[i], s[i_p1], s[i_p2]};
        float val = hermite_interpolation(&tmp[1], frac);
        phase += phaseIncMod;

        if (looping) {
            if (phase >= loopEnd)
                phase = loopStart + std::fmod(phase - loopStart, loopEnd - loopStart);
        } else {
            if (phase >= size) 
                return 0.0f;
        }

        return val * gainMod;
    }

    void processSave(int duration, std::vector<float>& abuf) {
        const SampleInfo* p = sample.load(std::memory_order_acquire);
        if (!p || p->data.empty())
            return;

        const auto& s = p->data;
        const size_t size = s.size();
        if (size == 0) return;
        int roll = duration;

        while(roll > 0) {
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
                    roll--;
                }
            } else {
                if (phase >= size) {
                    val = 0.0f; // end of sample
                    roll = 0;
                }
            }
            abuf.push_back(val);
        }
    }

private:
    enum class PMShape {
        SoftSine,
        Triangle,
        Drift,
        Juno
    };

    PMShape pmShape = PMShape::SoftSine;
    double srIn = 44100.0;
    double srOut = 44100.0;
    double phase = 0.0;
    double phaseInc = 0.0;
    float driftState = 0.0f;
    float driftCoeff = 0.9995f;
    float pmDepthSamplesMax = 80.0f;
    float pm_s1 = 0.0f;
    float pm_s2 = 0.0f;
    float vibPhase = 0.0f;
    float tremPhase = 0.0f;

    uint32_t noiseState = 1;
    size_t loopStart;
    size_t loopEnd;
    bool looping;
};

/****************************************************************
        4-Pole Ladder Filter (Moog style)
****************************************************************/

enum class LadderVoicing { Warm, Classic, Bright };

struct LadderFilter {
    double z1=0, z2=0, z3=0, z4=0; // 4 integrator stages
    double cutoff = 1000.0;
    double resonance = 0.0;
    double sampleRate = 44100.0;
    double feedback = 1.0;
    double tunning = 0.5;
    double voicing = 1.16;
    int ccCutoff = 127;
    int ccReso = 0;
    float keyTracking = 0.5f;
    bool filterOff = true;
    bool highpass = false;

    void setSampleRate(double sr) {sampleRate = sr;}

    void reset() { z1=z2=z3=z4=0.0; }

    void setVoicing(LadderVoicing v) {
        switch(v) {
            case LadderVoicing::Warm:      voicing = 1.12; break;
            case LadderVoicing::Classic:   voicing = 1.16; break;
            case LadderVoicing::Bright:    voicing = 1.30; break;
            default:                       voicing = 1.16; break;
        }
    }

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

        double lp = z4 * resGainComp;

        if (!highpass) return lp;
        return in - lp;
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
        filterLP.setSampleRate(sr);
        filterHP.setSampleRate(sr);
        filterHP.highpass = true;
        filterHP.ccCutoff = 0;
        player.setSampleRate(sr);
    }

    void setAttack(float a) {env.setAttack(a);}
    void setDecay(float d) {env.setDecay(d);}
    void setSustain(float s) {env.setSustain(s);}
    void setRelease(float r) {env.setRelease(r);}
    void setPmFreq(float f) {player.pmFreq = f;}
    void setPmDepth(float d) {player.pmDepthNorm = d;}
    void setPmMode(int m) {player.setPmMode(m);}
    void setvibDepth(float v) {player.vibDepth = v;}
    void setvibRate(float r) {player.vibRate = r;}
    void settremDepth(float t) {player.tremDepth = t;}
    void settremRate(float r) {player.tremRate = r;}

    void setRootFreq(float freq_) {freq = freq_;}

    void setCutoffLP(int value) {
        value = std::clamp(value, 0, 127);
        filterLP.ccCutoff = value;
        if (filterLP.ccCutoff == 127 && filterLP.ccReso == 0) filterLP.filterOff = true;
        else filterLP.filterOff = false;
    }

    void setResoLP(int value) {
        value = std::clamp(value, 0, 127);
        filterLP.ccReso   = value;
        if (filterLP.ccCutoff == 127 && filterLP.ccReso == 0) filterLP.filterOff = true;
        else filterLP.filterOff = false;
    }

    void setCutoffHP(int value) {
        value = std::clamp(value, 0, 127);
        filterHP.ccCutoff = value;
        if (filterHP.ccCutoff == 0 && filterHP.ccReso == 0) filterHP.filterOff = true;
        else filterHP.filterOff = false;
    }

    void setResoHP(int value) {
        value = std::clamp(value, 0, 127);
        filterHP.ccReso   = value;
        if (filterHP.ccCutoff == 0 && filterHP.ccReso == 0) filterHP.filterOff = true;
        else filterHP.filterOff = false;
    }

    void setKeyTracking(float amt) {
        filterLP.keyTracking = std::clamp(amt, 0.0f, 1.0f);
        filterHP.keyTracking = std::clamp(amt, 0.0f, 1.0f);
    }

    void noteOn(int midiNote, float velocity,
                std::shared_ptr<const SampleInfo> sampleData,
                double sourceRate, double rootFreq, bool looping = true) {

        this->midiNote = midiNote;
        active = true;
        vel = velocity;
        player.setSample(sampleData, sourceRate);
        player.setFrequency(midiToFreq(midiNote), rootFreq);
        player.setLoop(0, sampleData->data.size() - 1, looping);
        player.reset();
        recalcFilter(filterLP);
        filterLP.reset();
        recalcFilter(filterHP);
        filterHP.reset();
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
            active = false;
        }
    }

    float process() {
        if (!active) return 0.0f;
        float amp = env.process();
        float out = player.process() * vel * amp;
        if (!env.isActive()) active = false;
        return filterLP.process(filterHP.process(out));
    }

    void getAnalyseBuffer(float *abuf, int frames,
                        std::shared_ptr<const SampleInfo> sampleData,
                        double sourceRate, double rootFreq) {

        player.setSample(sampleData, sourceRate);
        player.setFrequency(440.0, rootFreq);
        player.setLoop(0, sampleData->data.size() - 1, true);
        player.reset();
        for (int i = 0; i < frames; i++) {
            abuf[i] = player.process();
        }
    }

    void getSaveBuffer(bool loop, std::vector<float>& abuf, uint8_t rootKey, int duration,
                        std::shared_ptr<const SampleInfo> sampleData,
                        double sourceRate, double rootFreq) {

        player.setSample(sampleData, sourceRate);
        player.setFrequency(midiToFreq(rootKey), rootFreq);
        player.setLoop(0, sampleData->data.size() - 1, loop);
        player.reset();
        player.processSave(duration, abuf);
        for (uint32_t i = 0; i < abuf.size(); i++) {
            abuf[i] = filterLP.process(filterHP.process(abuf[i]));
        }
    }

    bool isActive() const { return active; }

private:
    SamplePlayer player;
    ADSR env;
    LadderFilter filterLP;
    LadderFilter filterHP;

    bool active = false;
    bool looping = true;
    float vel = 1.0f;
    float freq = 440.0f;
    int midiNote = -1;

    double pmFreq = 0.0;
    double pmDepth = 0.0;

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

    void recalcFilter(LadderFilter& filter) {
        if (filter.filterOff) return;
        // Base cutoff from CC74
        float baseCut = ccToFreq(filter.ccCutoff);

        // Key tracking
        float semi = midiNote - 60;
        float keyFactor = std::pow(2.0f, (semi / 12.0f) * filter.keyTracking);
        double finalCut = std::clamp(baseCut * keyFactor, minFreq, maxFreq);

        filter.cutoff = finalCut;

        // Resonance from CC71 (scaled to 0..1 for ladder)
        double Q = ccToQ(filter.ccReso);
        filter.resonance = std::clamp((Q - 0.5) * 0.22, 0.0, 0.95); // safe mapping

        // Normalized cutoff + simple tuning correction
        filter.tunning = 2.0 * (filter.cutoff / filter.sampleRate) * filter.voicing;
        filter.feedback = filter.resonance * 4.0 * (1.0 - 0.15 * filter.tunning * filter.tunning); // resonance feedback
    }

};


/****************************************************************
        PolySynth: play multiple voices
****************************************************************/

class PolySynth {
public:
    PolySynth() {}

    void init(double sr, size_t maxVoices = 8) {
        //voices.resize(maxVoices);
        voices.clear();
        voices.reserve(maxVoices);
        for (size_t i = 0; i < maxVoices; ++i)
            voices.push_back(std::make_unique<SampleVoice>());
        sampleRate = sr;
        masterGain = (1.0f / std::sqrt((float)maxVoices));
        playLoop = false;
        for (auto& v : voices) {
            v->setADSR(0.01f, 0.2f, 0.7f, 0.4f); // Attack, Decay, Sustain, Release (in Seconds)
            v->setSampleRate(sr);
        }
    }

    void setLoop(bool loop) { playLoop = loop;}

    void setLoopBank(const SampleBank* lbank) {loopBank = lbank;}

    void setBank(const SampleBank* sbank) {sampleBank = sbank;}

    void getAnalyseBuffer(float *abuf, int frames) {
        const auto s = loopBank->getSample(0);
        voices[voices.size() - 1]->getAnalyseBuffer(abuf, frames, s, s->sourceRate, s->rootFreq);
    }

    void getSaveBuffer(bool loop, std::vector<float>& abuf, uint8_t rootKey, uint32_t duration) {
        auto s = sampleBank->getSample(0);
        if (loop) s = loopBank->getSample(0);
        voices[voices.size() - 1]->getSaveBuffer(loop, abuf, rootKey, duration, s, s->sourceRate, s->rootFreq);
    }

    void setAttack(float a) {
        for (auto& v : voices) {
            v->setAttack(a);
        }
    }

    void setDecay(float d) {
        for (auto& v : voices) {
            v->setDecay(d);
        }
    }

    void setSustain(float s) {
        for (auto& v : voices) {
            v->setSustain(s);
        }
    }

    void setRelease(float r) {
        for (auto& v : voices) {
            v->setRelease(r);
        }
    }

    void setRootFreq(float freq) {
        for (auto& v : voices) {
            v->setRootFreq(freq);
        }
    }

    void setCutoffLP(int value) {
        for (auto& v : voices) {
            v->setCutoffLP(value);
        }
    }

    void setResoLP(int value) {
        for (auto& v : voices) {
            v->setResoLP(value);
        }
    }

    void setCutoffHP(int value) {
        for (auto& v : voices) {
            v->setCutoffHP(value);
        }
    }

    void setResoHP(int value) {
        for (auto& v : voices) {
            v->setResoHP(value);
        }
    }

    void setKeyTracking(float amt) {
        float value = std::clamp(amt, 0.0f, 1.0f);
        for (auto& v : voices) {
            v->setKeyTracking(value);
        }
    }

    void setPmFreq(float f) {
        for (auto& v : voices) {
            v->setPmFreq(f);
        }
    }

    void setPmDepth(float d) {
        for (auto& v : voices) {
            v->setPmDepth(d);
        }
    }

    void setPmMode(int m) {
        for (auto& v : voices) {
            v->setPmMode(m);
        }
    }

    void setvibDepth(float d) {
        for (auto& v : voices) {
            v->setvibDepth(d);
        }
    }

    void setvibRate(float r) {
        for (auto& v : voices) {
            v->setvibRate(r);
        }
    }

    void settremDepth(float t) {
        for (auto& v : voices) {
            v->settremDepth(t);
        }
    }

    void settremRate(float r) {
        for (auto& v : voices) {
            v->settremRate(r);
        }
    }

    void noteOn(int midiNote, float velocity, size_t sampleIndex = 0) {
        if (playLoop ? !loopBank : !sampleBank) return;
        const auto s = playLoop ? loopBank->getSample(sampleIndex) : sampleBank->getSample(sampleIndex);
        if (!s) return;

        // Find free voice
        for (auto& v : voices) {
            if (!v->isActive()) {
                v->noteOn(midiNote, velocity, s, s->sourceRate, s->rootFreq, playLoop);
                return;
            }
        }

        // steal voice (simple)
        voices[0]->noteOn(midiNote, velocity, s, s->sourceRate, s->rootFreq, playLoop);
    }

    void noteOff(int midiNote) {
        for (auto& v : voices) v->noteOff(midiNote);
    }

    void allNoteOff() {
        for (auto& v : voices) v->noteOff();
    }

    float process() {
        float mix = 0.0f;
        for (auto& v : voices) {
            if (v->isActive()) {
                mix += v->process();
            }
        }
        // avoid clipping
        mix *= masterGain;
        return mix;
    }

private:
    std::vector<std::unique_ptr<SampleVoice>> voices;
    const SampleBank* sampleBank = nullptr;
    const SampleBank* loopBank = nullptr;
    double sampleRate;
    float masterGain;
    bool playLoop;
};

#endif
