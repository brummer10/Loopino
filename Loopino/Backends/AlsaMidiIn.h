
/*
 * AlsaRawMidiIn.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        AlsaRawMidiIn.h open a RAW ALSA MIDI Input port
        
****************************************************************/

#pragma once

#include <alsa/asoundlib.h>
#include <atomic>
#include <thread>
#include <cstdint>
#include <unistd.h>
#include <string>
#include <vector>

struct AlsaMidiDevice {
    std::string id;
    std::string label;
};

struct AlsaRawMidiIn {

    snd_rawmidi_t* midiIn = nullptr;
    std::thread midiThread;
    std::atomic<bool> running{false};

    decltype(ui)* uiPtr = nullptr;

    AlsaRawMidiIn() = default;

    ~AlsaRawMidiIn() {
        stop();
        close();
    }

    inline std::vector<AlsaMidiDevice> listAlsaRawMidiInputs() {
        std::vector<AlsaMidiDevice> list;
        int card = -1;
        snd_card_next(&card);

        while (card >= 0) {
            snd_ctl_t* ctl = nullptr;
            char cardName[32];
            snprintf(cardName, sizeof(cardName), "hw:%d", card);
            if (snd_ctl_open(&ctl, cardName, 0) < 0) {
                snd_card_next(&card);
                continue;
            }
            int device = -1;
            snd_ctl_rawmidi_next_device(ctl, &device);

            while (device >= 0) {
                snd_rawmidi_info_t* info;
                snd_rawmidi_info_alloca(&info);
                snd_rawmidi_info_set_device(info, device);
                snd_rawmidi_info_set_stream(
                    info, SND_RAWMIDI_STREAM_INPUT);
                if (snd_ctl_rawmidi_info(ctl, info) < 0) {
                    snd_ctl_rawmidi_next_device(ctl, &device);
                    continue;
                }
                // UX filter 1: ignore empty devices
                if (snd_rawmidi_info_get_subdevices_count(info) < 1) {
                    snd_ctl_rawmidi_next_device(ctl, &device);
                    continue;
                }
                const char* name = snd_rawmidi_info_get_name(info);
                if (!name) {
                    snd_ctl_rawmidi_next_device(ctl, &device);
                    continue;
                }
                // UX filter 2: ignore ALSA "Through"
                if (std::strstr(name, "Through")) {
                    snd_ctl_rawmidi_next_device(ctl, &device);
                    continue;
                }
                // subdevice 0 is sufficient for most hardware
                int sub = 0;
                char hwId[32];
                snprintf(hwId, sizeof(hwId),
                         "hw:%d,%d,%d",
                         card, device, sub);
                char label[128];
                snprintf(label, sizeof(label),
                         "%s (%s)", name, hwId);
                list.push_back({
                    hwId,
                    label
                });
                snd_ctl_rawmidi_next_device(ctl, &device);
            }
            snd_ctl_close(ctl);
            snd_card_next(&card);
        }
        return list;
    }

    bool open(const char* device, decltype(ui)* ui) {
        uiPtr = ui;

        if (snd_rawmidi_open(&midiIn, nullptr, device, SND_RAWMIDI_NONBLOCK) < 0) {
            std::cout << "Fail to open RAW_MIDI_DEVICE: " << device << std::endl;
            return false;
        }

        snd_rawmidi_nonblock(midiIn, 1);
        return true;
    }

    void close() {
        if (midiIn) {
            snd_rawmidi_close(midiIn);
            midiIn = nullptr;
        }
    }

    void start() {
        if (!midiIn || running.load())
            return;
        running.store(true);
        midiThread = std::thread(&AlsaRawMidiIn::run, this);
    }

    void stop() {
        if (!running.load())
            return;

        running.store(false);
        if (midiThread.joinable())
            midiThread.join();
    }

private:
    inline int dataLen(uint8_t status) const {
        switch (status & 0xF0) {
        case 0xC0: // Program Change
        case 0xD0: // Channel Pressure
            return 1;
        default:
            return 2;
        }
    }

    void run() {
        uint8_t byte = 0;
        uint8_t status = 0;
        uint8_t data[3] = {0};
        int idx = 0;

        MidiKeyboard* keys =
            (MidiKeyboard*)uiPtr->keyboard->private_struct;

        while (running.load()) {
            int r = snd_rawmidi_read(midiIn, &byte, 1);
            if (r == -EAGAIN) {
                usleep(1000);
                continue;
            }
            if (r <= 0) continue;

            if (byte & 0x80) {
                status = byte;
                idx = 0;
                continue;
            }

            if (!status) continue;
            data[idx++] = byte;
            if (idx < dataLen(status)) continue;

            switch (status & 0xF0) {
            case 0xC0: // Program Change
                uiPtr->loadPresetNum((int)data[0]);
                break;
            case 0xB0: // CC
                handleCC(data[0], data[1]);
                break;
            case 0xE0: { // Pitch Bend
                int v14 = data[0] | (data[1] << 7);
                float pw = (v14 - 8192) * 0.00012207; //(1.0f / 8192.0f);
                uiPtr->synth.setPitchWheel(pw);
                wheel_set_value(uiPtr->PitchWheel, pw);
                break;
            }
            case 0x90: // Note On
                if (data[1] == 0) {
                    uiPtr->synth.noteOff(data[0]);
                    set_key_in_matrix(keys->in_key_matrix[0], data[0], false);
                } else {
                    uiPtr->synth.noteOn(data[0], data[1] / 127.0f);
                    set_key_in_matrix(keys->in_key_matrix[0], data[0], true);
                }
                break;
            case 0x80: // Note Off
                uiPtr->synth.noteOff(data[0]);
                set_key_in_matrix(keys->in_key_matrix[0], data[0], false);
                break;
            }

            idx = 0;
        }
    }

    void handleCC(uint8_t cc, uint8_t val) {

        switch (cc) {
        case 71: // Reso LP
            uiPtr->synth.setResoLP(val);
            break;
        case 74: // Cutoff LP
            uiPtr->synth.setCutoffLP(val);
            break;
        case 7: { // Volume
            constexpr float min_dB = -20.0f;
            constexpr float max_dB =  12.0f;
            uiPtr->volume =
                min_dB + (val / 127.0f) * (max_dB - min_dB);
            break;
        }
        default:
            break;
        }
    }
};

