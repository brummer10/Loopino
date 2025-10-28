/*
 * PitchTracker.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

/****************************************************************
  PitchTracker - Measure dominant pitch of a audio buffer
  
  process only the first channel from a multi channel buffers
  this is meant for running offline (non-rt)
  return the resulting MIDI key
****************************************************************/

#include <fftw3.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>

#pragma once

#ifndef PITCHTRACKER_H
#define PITCHTRACKER_H

class PitchTracker {
public:
    PitchTracker() = default;


    uint8_t getPitch( const float* buffer, size_t N, uint32_t channels,
            float sampleRate, int16_t* pitchCorrection = nullptr,
            float* frequency = nullptr, float minFreq = 20.0f,
            float maxFreq = 5000.0f) {

        if (N < 2 || channels <= 0) {
            if (pitchCorrection) *pitchCorrection = 0;
            if (frequency) *frequency = 0.0f;
            return 0;
        }

        // Max abs amplitude for normalization (first channel only)
        float maxAbs = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            float a = std::fabs(buffer[i * channels]);
            if (a > maxAbs) maxAbs = a;
        }

        // Reject too quiet signals
        const float minLoudness = 1e-4f;
        if (maxAbs < minLoudness) {
            if (pitchCorrection) *pitchCorrection = 0;
            if (frequency) *frequency = 0.0f;
            return 0;
        }

        // Okay, we get something to work with, now allocate FFTW buffers
        fftwf_complex* out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (N/2 + 1));
        float* in = (float*) fftwf_malloc(sizeof(float) * N);
        fftwf_plan plan = fftwf_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);

        // Normalize & apply Hann window
        float gain = 1.0f / maxAbs;
        for (size_t i = 0; i < N; ++i) {
            float w = 0.5f - 0.5f * std::cos(2.0f * M_PI * i / (N - 1));
            in[i] = buffer[i * channels] * gain * w;
        }

        // Execute FFT
        fftwf_execute(plan);

        // Limit Frequency range
        size_t minBin = std::max<size_t>(1, static_cast<size_t>(std::floor(minFreq * N / sampleRate)));
        size_t maxBin = std::min<size_t>(N/2, static_cast<size_t>(std::ceil(maxFreq * N / sampleRate)));

        // Magnitude spectrum
        std::vector<float> mags(N/2 + 1);
        for (size_t k = minBin; k <= maxBin; ++k) {
            float re = out[k][0];
            float im = out[k][1];
            mags[k] = std::sqrt(re * re + im * im);
        }

        // Harmonic Product Spectrum
        const int numHarmonics = 4;  // usually 3–5 works well
        std::vector<float> hps(mags.size());
        for (size_t k = 0; k < mags.size(); ++k) {
            hps[k] = mags[k];
        }

        for (int h = 2; h <= numHarmonics; ++h) {
            for (size_t k = 0; k < mags.size() / h; ++k) {
                hps[k] *= mags[k * h];
            }
        }

        // Find peak in HPS spectrum
        size_t peakIndex = 0;
        float peakVal = 0.0f;
        for (size_t k = minBin; k <= maxBin / numHarmonics; ++k) {
            if (hps[k] > peakVal) {
                peakVal = hps[k];
                peakIndex = k;
            }
        }

        // Parabolic interpolation around peak
        float interpolatedIndex = static_cast<float>(peakIndex);
        if (peakIndex > 0 && peakIndex < N/2) {
            float alpha = std::log(hps[peakIndex - 1] + 1e-12f);
            float beta  = std::log(hps[peakIndex]     + 1e-12f);
            float gamma = std::log(hps[peakIndex + 1] + 1e-12f);
            float p = 0.5f * (alpha - gamma) / (alpha - 2*beta + gamma);
            interpolatedIndex += p;
        }

        // Convert peak bin to frequency
        float freq = interpolatedIndex * sampleRate / N;

        // Output frequency
        if (frequency) *frequency = freq;
        if (freq <= 0.0f) {
            fftwf_destroy_plan(plan);
            fftwf_free(in);
            fftwf_free(out);
            if (pitchCorrection) *pitchCorrection = 0;
            return 0;
        }

        // Frequency -> MIDI note
        float midiFloat = 69.0f + 12.0f * std::log2(freq / 440.0f);
        int midiNote = static_cast<int>(std::floor(midiFloat + 0.5f));
        midiNote = std::clamp(midiNote, 0, 127);

        // Pitch correction in cents
        double targetFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
        double cents = 1200.0 * std::log2(freq / targetFreq);

        if (cents > 50.0) {
            if (midiNote < 127) midiNote++;
            targetFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
            cents = 1200.0 * std::log2(freq / targetFreq);
        } else if (cents < -50.0) {
            if (midiNote > 0) midiNote--;
            targetFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
            cents = 1200.0 * std::log2(freq / targetFreq);
        }

        int16_t correction = static_cast<int16_t>(std::lround(cents));
        correction = std::clamp<int16_t>(correction, -50, 50);
        if (pitchCorrection) *pitchCorrection = correction;

        // Cleanup
        fftwf_destroy_plan(plan);
        fftwf_free(in);
        fftwf_free(out);

        return static_cast<uint8_t>(midiNote);
    }

private:

};

#endif
