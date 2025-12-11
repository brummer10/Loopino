
/*
 * LoopGenerator.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

/****************************************************************
  LoopGenerator - find best Loop segment from a sample buffer 

  Find a pair of zero crossing points to match a given frequency
  and get a click-free sample buffer for looping
****************************************************************/

#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>

#ifndef LOOPGENERATOR_H
#define LOOPGENERATOR_H

class LoopGenerator {
public:
    struct LoopInfo {
        size_t start = 0;
        size_t end = 0;
        size_t length = 0;
        size_t matches = 0;
        float fundamental = 0.0f;
    };

    struct Match {
        size_t start;
        size_t end;
        float score;
    };

    std::vector<Match> matches;

/****************************************************************
     Generate a clean loop based on zero-crossing matching
****************************************************************/

    bool generateLoop(const float* inputBuffer, size_t startFrames, size_t endFrames,
                            size_t numFrames, uint32_t numChannels, uint32_t sampleRate,
                            float fundamental, std::vector<float>& outputBuffer, LoopInfo& info,
                            int numPeriods = 1, float zeroCrossTolerance = 0.0005f) {

        if (!inputBuffer || numFrames == 0 || sampleRate == 0 || fundamental <= 0.0f || endFrames > numFrames)
            return false;

        const float periodLength = static_cast<float>(sampleRate) / fundamental;
        const float targetLength = periodLength * numPeriods;

        // Find zero crossing points
        auto zeros = findZeroCrossings(inputBuffer, startFrames, endFrames, numFrames, numChannels, zeroCrossTolerance);
        if (zeros.empty()) return false;

        // Find best start/end pair
        auto best = findBestLoopPair(zeros, targetLength, 0.1f, matches);
        if (best.score == std::numeric_limits<float>::max())
            return false;

        if (best.start >= best.end || best.end > numFrames)
            return false;

        // Copy loop segment
        outputBuffer.resize(best.end - best.start);
        for (size_t i = best.start; i < best.end; ++i) {
            outputBuffer[i - best.start] = inputBuffer[i * numChannels];
        }

        // Fill info
        info.start = best.start;
        info.end = best.end;
        info.length = best.end - best.start;
        info.matches = matches.size();
        info.fundamental = fundamental;

        return true;
    }

    bool getNextMatch(const float* inputBuffer, size_t numFrames, uint32_t numChannels, 
            float fundamental, std::vector<float>& outputBuffer, LoopInfo& info, size_t num) {

        auto best = matches[num];
        if (best.score == std::numeric_limits<float>::max())
            return false;

        if (best.start >= best.end || best.end > numFrames)
            return false;

        // Copy loop segment
        outputBuffer.resize(best.end - best.start);
        for (size_t i = best.start; i < best.end; ++i) {
            outputBuffer[i - best.start] = inputBuffer[i * numChannels];
        }

        // Fill info
        info.start = best.start;
        info.end = best.end;
        info.length = best.end - best.start;
        info.matches = matches.size();
        info.fundamental = fundamental;

        return true;
    }

private:
    struct ZeroCross {
        size_t index;
        int direction;
        float amplitude;
    };

/****************************************************************
     Scan buffer for zero crossings on channel 0
****************************************************************/

    static std::vector<ZeroCross> findZeroCrossings(const float* buffer,  size_t startFrames,
                                                    size_t endFrames, size_t numFrames,
                                                    uint32_t numChannels, float tolerance) {

        std::vector<ZeroCross> zeros;
        zeros.reserve(numFrames / 10);

        for (size_t i = startFrames + 1; i < endFrames; ++i) {
            float prev = buffer[(i - 1) * numChannels];
            float curr = buffer[i * numChannels];

            // Ignore crossings too close to zero
            if (std::fabs(curr) > tolerance) {
                if ((prev <= 0.0f && curr > 0.0f) || (prev >= 0.0f && curr < 0.0f)) {
                    int dir = (curr > prev) ? 1 : -1;
                    zeros.push_back({i, dir, curr});
                }
            }
        }
        return zeros;
    }

/****************************************************************
     Find best zero-crossing pair that matches target loop length
****************************************************************/

    static Match findBestLoopPair(const std::vector<ZeroCross>& zeros, float targetLength, float alpha, std::vector<Match>& matches ) {

        Match best{0, 0, std::numeric_limits<float>::max()};
        matches.clear();

        for (size_t i = 0; i < zeros.size(); ++i) {
            const auto& s = zeros[i];

            // Ideal end location for this start
            float idealEnd = static_cast<float>(s.index) + targetLength;

            for (size_t j = i + 1; j < zeros.size(); ++j) {
                const auto& e = zeros[j];
                if (e.direction != s.direction) continue;

                float lengthError = std::fabs(static_cast<float>(e.index) - idealEnd);
                float ampError = std::fabs(s.amplitude) + std::fabs(e.amplitude);
                float score = lengthError + alpha * ampError;

                if (score < best.score) {
                    best.start = s.index;
                    best.end = e.index;
                    best.score = score;
                    matches.push_back(best);
                }
            }
        }
        return best;
    }
};

#endif
