/*
 * Loopinovst.cpp
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#include "vestige.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <stddef.h>
#include <sstream>


#define RUN_AS_PLUGIN
#define IS_VST2
#include "Loopino_ui.h"

typedef struct ERect {
    short top;
    short left;
    short bottom;
    short right;
} ERect;

struct VstStreamOut : StreamOut {
    std::ostringstream ss;

    void write(const void* data, size_t size) override {
        ss.write(reinterpret_cast<const char*>(data), size);
    }
};

struct VstStreamIn : StreamIn {
    std::istringstream ss;

    VstStreamIn(const void* data, size_t size)
        : ss(std::string(reinterpret_cast<const char*>(data), size)) {}

    void read(void* data, size_t size) override {
        ss.read(reinterpret_cast<char*>(data), size);
    }
};

#define PLUGIN_UID 'LOPI'

#define WINDOW_WIDTH  880
#define WINDOW_HEIGHT 290

#define FlagsChunks (1 << 5)

/****************************************************************
 ** loopino_plugin_t -> the plugin struct
 */

class loopino_plugin_t {
public:
    AEffect* effect;
    Loopino *r;
    ERect editorRect;
    int width, height;
    float SampleRate;
    std::stringstream state;
    bool isInited;
    bool guiIsCreated;
    bool havePresetToLoad;
    int32_t process_events (VstEvents* events);
};

/****************************************************************
 ** Parameter handling not used here
 */

static void setParameter(AEffect* effect, int32_t index, float value) {
}

static float getParameter(AEffect* effect, int32_t index) {
    return 0.0;
}

static void getParameterName(AEffect*, int32_t index, char* label) {
}

/****************************************************************
 ** The MIDI process
 */

int32_t loopino_plugin_t::process_events (VstEvents* events) {
    if (!events) return 1;

    for (int32_t i = 0; i < events->numEvents; ++i) {

        VstMidiEvent* midi = reinterpret_cast<VstMidiEvent*>(events->events[i]);
        if (midi->type != kVstMidiType)
            continue;

        const uint8_t status = midi->midiData[0] & 0xF0;
        //const uint8_t channel = midi->midiData[0] & 0x0F;
        const uint8_t data1 = midi->midiData[1];
        const uint8_t data2 = midi->midiData[2];

        switch (status) {
            case 0x90: // NoteOn
                if (data2 > 0)
                {
                    r->synth.noteOn((int) data1, (float)(data2/127.0f));
                }
                else
                {
                    // NoteOn Velocity 0 = NoteOff
                    r->synth.noteOff((int)data1);
                }
                break;

            case 0x80: // NoteOff
                r->synth.noteOff((int)data1);
                break;

            default:
                break;
        }
    }
    return 1;
}


/****************************************************************
 ** The audio process
 */

static void processReplacing(AEffect* effect, float** inputs, float** outputs, int32_t nframes) {
    loopino_plugin_t* plug = (loopino_plugin_t*)effect->object;

    float* left_output = outputs[0];
    float* right_output = outputs[1];

    static float fRec0[2] = {0};
    if (( plug->r->af.samplesize && plug->r->af.samples != nullptr) && plug->r->play && plug->r->ready) {
        float fSlow0 = 0.0010000000000000009 * plug->r->gain;
        for (int32_t i = 0; i < nframes; i++) {
            // process playback
            fRec0[0] = fSlow0 + 0.999 * fRec0[1];
            for (uint32_t c = 0; c < plug->r->af.channels; c++) {
                if (!c) {
                    left_output[i] = plug->r->af.samples[plug->r->position*plug->r->af.channels] * fRec0[0];
                    if (plug->r->af.channels ==1)
                        right_output[i] = plug->r->af.samples[plug->r->position*plug->r->af.channels] * fRec0[0];
                } else right_output[i] = plug->r->af.samples[plug->r->position*plug->r->af.channels+c] * fRec0[0];
            }
            fRec0[1] = fRec0[0];
            // track play-head position
            plug->r->position++;
            if (plug->r->position > plug->r->loopPoint_r) {
                plug->r->position = plug->r->loopPoint_l;
                plug->r->play = false;
            } else if (plug->r->position <= plug->r->loopPoint_l) {
                plug->r->position = plug->r->loopPoint_r;
            }
        }
    } else {
        memset(left_output, 0.0, nframes * sizeof(float));
        memset(right_output, 0.0, nframes * sizeof(float));
    }
    float fSlow0 = 0.0010000000000000009 * plug->r->gain;
    for (int32_t i = 0; i < nframes; i++) {
        // process synth
        fRec0[0] = fSlow0 + 0.999 * fRec0[1];
        float out = plug->r->synth.process() * fRec0[0];
        left_output[i] += out;
        right_output[i] += out;
        fRec0[1] = fRec0[0];
    }
}

/****************************************************************
 ** Save and load state
 */

int32_t getChunk(loopino_plugin_t* plug, void** data, bool isPreset) {
    if (!isPreset) return 0;

    VstStreamOut stream;
    plug->r->saveState(stream);

    std::string bin = stream.ss.str();
    size_t size = bin.size();

    if (size == 0) return 0;

    void* mem = std::malloc(size);
    std::memcpy(mem, bin.data(), size);

    *data = mem;
    return static_cast<int32_t>(size);
}

int32_t setChunk(loopino_plugin_t* plug, void* data, int32_t size, bool isPreset) {
    if (!isPreset) return 0;

    if (!data || size <= 0) return 0;

    VstStreamIn stream(data, size);
    plug->r->readState(stream);
    plug->havePresetToLoad = true;
    plug->r->loadPresetToSynth();
    return 0;
}

/****************************************************************
 ** register MIDI input
 */

int32_t canDo(char* text) {
    if (!strcmp(text, "receiveVstEvents")) return 1;
    if (!strcmp(text, "receiveVstMidiEvent")) return 1;
    return 0;
}

/****************************************************************
 ** The Dispatcher
 */

static intptr_t dispatcher(AEffect* effect, int32_t opCode, int32_t index, intptr_t value, void* ptr, float opt) {
    loopino_plugin_t* plug = (loopino_plugin_t*)effect->object;
    switch (opCode) {
        case effEditGetRect:
            if (ptr) *(ERect**)ptr = &plug->editorRect;
            return 1;
        case effGetEffectName:
            strncpy((char*)ptr, "Loopino", VestigeMaxNameLen - 1);
            ((char*)ptr)[VestigeMaxNameLen - 1] = '\0';
            return 1;
        case effGetVendorString:
            strncpy((char*)ptr, "brummer", VestigeMaxNameLen - 1);
            ((char*)ptr)[VestigeMaxNameLen - 1] = '\0';
            return 1;
        case effGetProductString:
            strncpy((char*)ptr, "brummer", VestigeMaxNameLen - 1);
            ((char*)ptr)[VestigeMaxNameLen - 1] = '\0';
            return 1;
        case effCanDo:
            return canDo((char*)ptr);
        case effGetPlugCategory:
            return kPlugCategSynth;
        case effOpen:
            break;
        case effClose:
            if (plug->guiIsCreated) {
                plug->r->quitGui();
            }
            delete plug->r;
            free(plug);
            break;
        case effGetParamName:
            getParameterName(effect, index, (char*)ptr);
            break;
        case effSetSampleRate:
            plug->SampleRate = opt;
            plug->r->setJackSampleRate((uint32_t)plug->SampleRate);
            plug->isInited = true;
            if (plug->havePresetToLoad) plug->r->loadPresetToSynth();
            plug->havePresetToLoad = false;
            break;
        case effEditOpen: {
            Window hostWin = (Window)(size_t)ptr;
            plug->r->startGui();
            plug->r->setParent(hostWin);
            plug->r->showGui();
            plug->guiIsCreated = true;
            break;
        }
        case effEditClose: {
            if (plug->guiIsCreated) {
                plug->r->quitGui();
            }
            plug->guiIsCreated = false;
            break;
        }
        case effEditIdle:
            break;
        //case effGetProgram:
        case 23: { // effGetChunk
            return getChunk(plug, ptr ? (void**)ptr : nullptr, index == 0);
        }
        //case effSetProgram:
        case 24: { // effSetChunk
            return setChunk(plug, ptr, (int32_t)value, index == 0);
            break;
        }
        case effProcessEvents:
            plug->process_events ((VstEvents*)ptr);
            break;

        default: break;
    }
    return 0;
}

/****************************************************************
 ** The Main Entry
 */

extern "C" __attribute__ ((visibility ("default")))
AEffect* VSTPluginMain(audioMasterCallback audioMaster) {
    loopino_plugin_t* plug = (loopino_plugin_t*)calloc(1, sizeof(loopino_plugin_t));
    AEffect* effect = (AEffect*)calloc(1, sizeof(AEffect));
    plug->r = new Loopino();
    effect->object = plug;
    plug->effect = effect;
    plug->width = WINDOW_WIDTH;
    plug->height = WINDOW_HEIGHT;
    plug->editorRect = {0, 0, (short) plug->height, (short) plug->width};
    plug->SampleRate = 48000.0;
    plug->isInited = false;
    plug->guiIsCreated = false;
    plug->havePresetToLoad = false;

    effect->magic = kEffectMagic;
    effect->dispatcher = dispatcher;
    effect->processReplacing = processReplacing;
    effect->setParameter = setParameter;
    effect->getParameter = getParameter;
    effect->numPrograms = 1;
    effect->numParams = 0;
    effect->numInputs = 0;
    effect->numOutputs = 2;
    effect->flags = effFlagsHasEditor | effFlagsCanReplacing | FlagsChunks;
    effect->uniqueID = PLUGIN_UID;
    return effect;
}
