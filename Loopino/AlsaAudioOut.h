
/*
 * AlsaAudioOut.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        AlsaAudioOut.h  a ALSA Stereo Output Device
                        and a ALSA mono input port
****************************************************************/

#pragma once

#include <alsa/asoundlib.h>
#include <atomic>
#include <thread>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <atomic>

#include "RtCheck.h"

class AlsaAudioOut {
public:
    std::atomic<uint32_t> xruns {0};
    AlsaAudioOut(const std::string& device = "default")
        : deviceName(device) {}

    ~AlsaAudioOut() { 
        shutdown();
        }

    void setThreadPolicy(int32_t rt_prio, int32_t rt_policy) noexcept {
        sched_param sch_params;
        if (rt_prio == 0) {
            rt_prio = sched_get_priority_max(rt_policy);
        }
        if ((rt_prio/5) > 0) rt_prio = rt_prio/5;
        sch_params.sched_priority = rt_prio;
        if (pthread_setschedparam(audioThread.native_handle(), rt_policy, &sch_params)) {
            std::cout << "Fail to set RT Priority" << std::endl;
        }
    }


    bool open(const char* device = "default", uint32_t preferredRate = 48000) {
        int err;

        if ((err = snd_pcm_open(&pcm_in, device,
                SND_PCM_STREAM_CAPTURE, 0)) < 0) {
            std::cerr << "ALSA capture open failed: "
                      << snd_strerror(err) << "\n";
            return false;
        }

        snd_pcm_hw_params_t* hw;
        snd_pcm_hw_params_alloca(&hw);
        snd_pcm_hw_params_any(pcm_in, hw);

        snd_pcm_hw_params_set_access(
            pcm_in, hw, SND_PCM_ACCESS_RW_INTERLEAVED);

        snd_pcm_hw_params_set_format(
            pcm_in, hw, SND_PCM_FORMAT_S16_LE);

        snd_pcm_hw_params_set_channels(pcm_in, hw, 1);

        unsigned int rate = preferredRate;;
        snd_pcm_hw_params_set_rate_near(pcm_in, hw, &rate, 0);
        snd_pcm_hw_params_get_rate(hw, &rate, 0);
        rateHz = rate;

        snd_pcm_hw_params_get_period_size(hw,
            (snd_pcm_uframes_t*)&framesPerBuffer, 0);

        snd_pcm_hw_params(pcm_in, hw);

        in_i16.resize(framesPerBuffer);
        in_f32.resize(framesPerBuffer);

        snd_pcm_prepare(pcm_in);
        return true;
    }

    bool init(decltype(ui)* ui, uint32_t preferredRate = 48000,
              uint32_t preferredPeriod_ = 256, uint32_t preferredPeriods = 2) {
        preferredPeriod = preferredPeriod_;
        uiPtr = ui;
        int err = snd_pcm_open(&pcm, deviceName.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            throw std::runtime_error("snd_pcm_open failed");
            return false;
        }

        snd_pcm_hw_params_alloca(&hw);
        snd_pcm_hw_params_any(pcm, hw);

        snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_FLOAT_LE);
        snd_pcm_hw_params_set_channels(pcm, hw, 2);

        unsigned int rate = preferredRate;
        snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);

        snd_pcm_uframes_t period = preferredPeriod;
        snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0);

        periods = preferredPeriods;
        snd_pcm_hw_params_set_periods_near(pcm, hw, &periods, 0);

        err = snd_pcm_hw_params(pcm, hw);
        if (err < 0) {
            throw std::runtime_error("snd_pcm_hw_params failed");
            return false;
        }

        snd_pcm_hw_params_get_rate(hw, &rate, 0);
        snd_pcm_hw_params_get_period_size(hw, &period, 0);
        snd_pcm_hw_params_get_periods(hw, &periods, 0);

        rateHz = rate;
        framesPerBuffer = static_cast<uint32_t>(period);

        snd_pcm_prepare(pcm);

        stereo.resize(framesPerBuffer * 2);

        snd_pcm_sw_params_t* sw;
        snd_pcm_sw_params_alloca(&sw);
        snd_pcm_sw_params_current(pcm, sw);
        snd_pcm_sw_params_set_start_threshold(pcm, sw, period);
        snd_pcm_sw_params_set_avail_min(pcm, sw, period);
        snd_pcm_sw_params(pcm, sw);
        if (!open(deviceName.c_str(), preferredRate)) return false;
        uiPtr->setJackSampleRate(rate);
        return true;
    }

    void read() {
        if (!pcm_in) return;
        static constexpr float THRESHOLD = 0.25f; // -12db

        snd_pcm_sframes_t n =
            snd_pcm_readi(pcm_in, in_i16.data(), framesPerBuffer);

        if (n == -EPIPE) {
            xruns.fetch_add(1, std::memory_order_relaxed);
            //std::cout << "Xrun Overrun " << xruns.load(std::memory_order_relaxed) << std::endl;
            uiPtr->getXrun();
            snd_pcm_prepare(pcm_in);
            uiPtr->record = false;
            rec = false;
            r = 0;
            return;
        }

        if (n <= 0) {
            snd_pcm_prepare(pcm_in);
            uiPtr->record = false;
            rec = false;
            r = 0;
            return;
        }

        if (!uiPtr->record) {
            rec = false;
            r = 0;
            return;
        }

        uint32_t frames = (uint32_t)n;

        for (uint32_t i = 0; i < frames; ++i)
            in_f32[i] = (float)in_i16[i] * 0.000030519f; //(1.0f / 32768.0f);

        if (!rec) {
            float peak = 0.0f;
            for (uint32_t i = 0; i < frames; ++i)
                peak = std::max<float>(peak, fabsf(in_f32[i]));

            if (peak < THRESHOLD)
                return;

            rec = true;
            uiPtr->timer = 0;
        }

        for (uint32_t i = 0; i < frames; ++i) {
            if (r >= uiPtr->af.samplesize) {
                uiPtr->record = false;
                rec = false;
                r = 0;
                break;
            }
            uiPtr->af.samples[r++] = in_f32[i];
            uiPtr->position++;
        }
    }

    void run() {
        if (!pcm) return;
        snd_pcm_start(pcm);

        while (running.load()) {
            for (uint32_t i = 0; i < framesPerBuffer; ++i) {
                float s = uiPtr->synth.process();
                stereo[i * 2 + 0] = s;
                stereo[i * 2 + 1] = s;
            }

            snd_pcm_sframes_t written =
                snd_pcm_writei(pcm, stereo.data(), framesPerBuffer);

            if (written == -EPIPE) {
                xruns.fetch_add(1, std::memory_order_relaxed);
                //std::cout << "Xrun Underrun " << xruns.load(std::memory_order_relaxed) << std::endl;
                uiPtr->getXrun();
                snd_pcm_prepare(pcm);
            }
            else if (written == -ESTRPIPE) {
                while ((written = snd_pcm_resume(pcm)) == -EAGAIN)
                    usleep(1000);
                if (written < 0)
                    snd_pcm_prepare(pcm);
            }
            else if (written < 0) {
                snd_pcm_prepare(pcm);
            }
            read();
        }
    }

    void start() {
        if (!pcm || running.load()) return;
        running.store(true);
        audioThread = std::thread(&AlsaAudioOut::run, this);
        setThreadPolicy(25, 1);
        std::cout << "Running ALSA at: " << rateHz << " Hz SampleRate with " << framesPerBuffer << "/" << periods << " Frames/Periode" << std::endl;
    }

    void stop() {
        if (!running.load()) return;
        running.store(false);
        if (audioThread.joinable())
            audioThread.join();
    }

private:
    void shutdown() {
        running.store(false);
        if (audioThread.joinable())
            audioThread.join();
        if (pcm) {
            snd_pcm_drain(pcm);
            snd_pcm_close(pcm);
            pcm = nullptr;
        }
        if (pcm_in) {
            snd_pcm_close(pcm_in);
            pcm_in = nullptr;
        }
    }

    std::string deviceName;
    std::thread audioThread;
    snd_pcm_t* pcm = nullptr;
    snd_pcm_t* pcm_in = nullptr;
    snd_pcm_hw_params_t* hw = nullptr;
    decltype(ui)* uiPtr = nullptr;

    uint32_t rateHz = 44100;
    uint32_t framesPerBuffer = 256;
    uint32_t preferredPeriod = 256;
    uint32_t r = 0;
    unsigned int periods = 2;
    bool rec = false;

    std::vector<int16_t> in_i16;
    std::vector<float>  in_f32;

    std::vector<float> stereo;
    std::atomic<bool> running{false};
};
