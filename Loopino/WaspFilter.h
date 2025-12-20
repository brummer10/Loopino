
#pragma once
#include <cmath>
#include <algorithm>

class WaspFilter
{
public:
    WaspFilter() = default;

    void setSampleRate(float sr) {
        sampleRate = sr;
        fadeStep = 1.0f / (0.02f * sampleRate);
    }

    void setCutoff(float freqHz) { baseCutoff = freqHz; } // 30.0f, 15000.0f
    void setResonance(float r) { resonance = r;} // 0.0f, 1.3f
    void setFilterMix(float m) { mix = m; } // -1.0f, 1.0f
    void setKeyTracking(float amt) { keyTrack = amt; } // 0.0f, 1.0f
    void setMidiNote(float note) { midiNote = note; } // 0.0f, 127.0f

    void setOnOff(bool v) {
        targetOn = v;
        if (v && !onoff) {
            reset();
            onoff = true;
        }
    }

    void reset() {
        s1.reset();
        s2.reset();
        s3.reset();
        s4.reset();
    }

    inline float process(float in) {
        if (targetOn) {
            fadeGain = std::min(1.0f, fadeGain + fadeStep);
        } else {
            fadeGain = std::max(0.0f, fadeGain - fadeStep);
            if (fadeGain == 0.0f) {
                onoff = false;
                return in;
            }
        }

        float cutoff = keyTrackCutoff();
        float g = 1.0f - std::exp(-2.0f * float(M_PI) * cutoff / sampleRate);
        float fb = mixFeedback(in, s2.z, s4.z);
        fb = fbFilter.process(fb, 0.01f);
        float x = in - resonance * fb;
        x = saturate(x);

        float o1 = s1.process(x, g);
        float o2 = s2.process(saturate(o1), g);
        float o3 = s3.process(saturate(o2), g);
        float o4 = s4.process(saturate(o3), g);

        s1.z = antiDenormal(s1.z);
        s2.z = antiDenormal(s2.z);
        s3.z = antiDenormal(s3.z);
        s4.z = antiDenormal(s4.z);
        fbFilter.z = antiDenormal(fbFilter.z);

        float lp = o4;
        float bp = o2 - o4;
        float hp = in - o4;

        float out = mixOutputs(hp, bp, lp);
        return in * (1.0f - fadeGain) + out * fadeGain;
    }

private:

    struct OnePole {
        float z = 0.0f;

        inline float process(float x, float g) {
            z += g * (x - z);
            return z;
        }

        void reset() { z = 0.0f; }
    };

    inline float tanh_fast(float x) const {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float saturate(float x) const {
        return tanh_fast(x * 1.4f) + 0.15f * x * x * x;
    }

    inline float keyTrackCutoff() const {
        float cutoff = baseCutoff;

        if (keyTrack > 0.0f) {
            float noteOffset = (midiNote - 60.0f) / 12.0f;
            float kt = std::pow(2.0f, noteOffset);

            // absichtlich nicht sauber
            kt = std::pow(kt, 0.85f + 0.3f * keyTrack);

            cutoff *= 1.0f + keyTrack * (kt - 1.0f);
        }

        return std::clamp(cutoff, 20.0f, 18000.0f);
    }

    inline float mixOutputs(float hp, float bp, float lp) const {
        float hpAmt = std::clamp(-mix, 0.0f, 1.0f);
        float lpAmt = std::clamp( mix, 0.0f, 1.0f);

        float bpAmt = 1.0f - std::abs(mix);
        bpAmt = std::pow(bpAmt, 0.7f);

        return hp * hpAmt + bp * bpAmt + lp * lpAmt;
    }

    inline float mixFeedback(float in, float bp, float lp) const {
        float hp = in - lp;

        float hpAmt = std::clamp(-mix, 0.0f, 1.0f);
        float lpAmt = std::clamp( mix, 0.0f, 1.0f);
        float bpAmt = 1.0f - std::abs(mix);
        bpAmt = std::pow(bpAmt, 0.7f);
        hpAmt *= 0.5f;
        return hp * hpAmt + bp * bpAmt + lp * lpAmt;
    }

    inline float antiDenormal(float x) {
        return (std::abs(x) < 1e-15f) ? 0.0f : x;
    }

    float sampleRate = 44100.0f;
    float fadeGain = 0.0f;
    float fadeStep = 0.0f;
    bool  targetOn = false; 
    bool onoff = false;

    float baseCutoff = 1000.0f;
    float resonance  = 0.4f;
    float mix       = 0.0f;
    float keyTrack  = 0.5f;

    float midiNote  = 60.0f;

    OnePole s1, s2, s3, s4;
    OnePole fbFilter;
};

/*
filter.setCutoff(800.0f);
filter.setResonance(0.6f);
filter.setFilterMix(0.2f); 
filter.setKeyTrack(0.7f);
*/

