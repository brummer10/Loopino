
/*
 * AudioFile.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#include <iostream>
#include <cstring>
#include <new>
#include <sndfile.hh>

#include "CheckResample.h"

#pragma once

#ifndef AUDIOFILE_H
#define AUDIOFILE_H

/****************************************************************
        class AudioFile - load a Audio File into buffer
                          and resample when needed
                          save a buffer to audio file
****************************************************************/

class AudioFile : public CheckResample {
public:
    uint32_t channels;
    uint32_t samplesize;
    uint32_t samplerate;
    float*   samples;
    float* saveBuffer;
    
    AudioFile() {
        channels   = 0;
        samplesize = 0;
        samplerate = 0;
        samples    = nullptr;
        saveBuffer = nullptr;
    }
    
    ~AudioFile() {
        delete[] samples;
        delete[] saveBuffer;
    }

    // load a Audio File into the buffer
    inline bool getAudioFile(const char* file, const uint32_t expectedSampleRate = 0) {
        SF_INFO info;
        info.format = 0;

        channels = 0;
        samplesize = 0;
        samplerate = 0;
        delete[] samples;
        samples = nullptr;
        // Open the wave file for reading
        SNDFILE *sndfile = sf_open(file, SFM_READ, &info);

        if (!sndfile) {
            std::cerr << "Error: could not open file " << sf_error (sndfile) << std::endl;
            return false;
        }
        if (info.channels > 2) {
            std::cerr << "Error: only two channels maximum are supported!" << std::endl;
            return false;
        }
        try {
            samples = new float[info.frames * info.channels];
        } catch (...) {
            std::cerr << "Error: could not load file" << std::endl;
            return false;
        }
        std::memset(samples, 0, info.frames * info.channels * sizeof(float));
        samplesize = (uint32_t) sf_readf_float(sndfile, &samples[0], info.frames);
        if (!samplesize ) samplesize = info.frames;
        channels = info.channels;
        samplerate = info.samplerate;
        sf_close(sndfile);
        if (expectedSampleRate)
            samples = checkSampleRate(&samplesize, channels, samples, samplerate, expectedSampleRate);
        return samples ? true : false;
    }

    // save a audio file from buffer to file
    void saveAudioFile(std::string name, const uint32_t from, const uint32_t to, const uint32_t SampleRate) {
        SF_INFO sfinfo ;
        sfinfo.channels = channels;
        sfinfo.samplerate = SampleRate;
        sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
        SNDFILE * sf = sf_open(name.c_str(), SFM_WRITE, &sfinfo);
        if (!sf) {
            std::cerr << "fail to open " << name << std::endl;
            return;
        }
        sf_writef_float(sf,&samples[from], to - from);
        sf_write_sync(sf);
        sf_close(sf);
    }

};

#endif
