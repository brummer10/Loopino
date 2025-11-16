/*
 * ClapPlug.cpp
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#include <clap.h>
#include <ext/params.h>
#include <events.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct plugin_t plugin_t;

#define RUN_AS_CLAP_PLUGIN

#define WINDOW_WIDTH  880
#define WINDOW_HEIGHT 290

#if defined(_WIN32)
#define GUIAPI CLAP_WINDOW_API_WIN32
#else
#define GUIAPI CLAP_WINDOW_API_X11
#endif


/****************************************************************
 ** plugin_t -> the plugin struct
 */

#include "Loopino_ui.h"

// Plugin data structure
struct plugin_t {
    clap_plugin_t plugin;
    const clap_host_t *host;
    Loopino *r;
    bool isInited;
    bool guiIsCreated;
    uint32_t latency;
    uint32_t width;
    uint32_t height;
};

/****************************************************************
 ** Parameter handling
 */

static uint32_t params_count(const clap_plugin_t* plugin) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    return (uint32_t)plug->r->param.getParamCount();
}

static bool params_get_info(const clap_plugin_t* plugin, uint32_t param_index, clap_param_info_t* param_info) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (param_index >= (uint32_t)plug->r->param.getParamCount()) return false;
    const auto& def = plug->r->param.getParameter(param_index);
    memset(param_info, 0, sizeof(*param_info));
    param_info->id = def.id;
    strncpy(param_info->name, def.name.c_str(), CLAP_NAME_SIZE-1);
    strncpy(param_info->module, def.group.c_str(), CLAP_PATH_SIZE-1);
    param_info->default_value = def.def;
    param_info->min_value = def.min;
    param_info->max_value = def.max;
    uint32_t flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (def.isStepped) flags |= CLAP_PARAM_IS_STEPPED;
    param_info->flags = flags;
    param_info->cookie = nullptr;
    return true;
}

static bool params_get_value(const clap_plugin_t* plugin, clap_id param_id, double* value) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (param_id < 0 || (int)param_id >= plug->r->param.getParamCount()) return false;
    *value = plug->r->param.getParam(param_id);
    return true;
}

static bool params_value_to_text(const clap_plugin_t* plugin, clap_id param_id, double value, char* out, uint32_t size) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (param_id < 0 || (int)param_id >= plug->r->param.getParamCount()) return false;
    snprintf(out, size, "%.2f", value);
    return true;
}

static bool params_text_to_value(const clap_plugin_t* plugin, clap_id param_id, const char* text, double* out_value) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (param_id < 0 || (int)param_id >= plug->r->param.getParamCount()) return false;
    *out_value = atof(text);
    return true;
}

static void sync_params_to_plug(const clap_plugin_t *plugin, const clap_event_header_t *hdr) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID) {
        switch (hdr->type) {
            case CLAP_EVENT_PARAM_VALUE: {
                const clap_event_param_value_t *ev = (const clap_event_param_value_t *)hdr;
                plug->r->param.setParam(ev->param_id, ev->value);
                break;
            }
        }
    }
}

static void sync_params_to_host(const clap_plugin_t *plugin, const clap_output_events_t *out) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    for (int i = 0; i < plug->r->param.getParamCount(); i++) {
        if (plug->r->param.isParamDirty(i)) {
            clap_event_param_value_t event = {};
            event.header.size = sizeof(event);
            event.header.time = 0;
            event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            event.header.type = CLAP_EVENT_PARAM_VALUE;
            event.header.flags = 0;
            event.param_id = i;
            event.cookie = NULL;
            event.note_id = -1;
            event.port_index = -1;
            event.channel = -1;
            event.key = -1;
            event.value = plug->r->param.getParam(i);
            out->try_push(out, &event.header);
            plug->r->param.setParamDirty(i, false);
        }
    }
}

static void params_flush(const clap_plugin_t *plugin,
                        const clap_input_events_t *in,
                        const clap_output_events_t *out) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    for (uint32_t i = 0; i < in->size(in); ++i) {
        const clap_event_header_t *ev = in->get(in, i);
        if (ev->type == CLAP_EVENT_PARAM_VALUE) {
            auto *p = (const clap_event_param_value_t *)ev;
            if (p->param_id >= 0 && (int)p->param_id < plug->r->param.getParamCount()) {
                plug->r->param.setParam(p->param_id, p->value);
            }
        }
    }
}

const clap_plugin_params_t params = {
    .count         = params_count,
    .get_info      = params_get_info,
    .get_value     = params_get_value,
    .value_to_text = params_value_to_text,
    .text_to_value = params_text_to_value,
    .flush         = params_flush,
};

/****************************************************************
 ** define the audio ports
 */

static uint32_t audio_ports_count(const clap_plugin_t* plugin, bool is_input) {
    if (is_input) return 0; // 1 input
    else return 2; // and 2 output
}

static bool audio_ports_get(const clap_plugin_t* plugin, uint32_t index, 
                            bool is_input, clap_audio_port_info_t *info) {
    if (index > 0) return false;
    info->id = index;
    snprintf(info->name, sizeof(info->name), "%s", is_input ? "Input" : "Output");
    if (is_input) {
        info->channel_count = 0; // none
        info->port_type = CLAP_PORT_MONO;
    } else {
        info->channel_count = 2; // Stereo
        info->port_type = CLAP_PORT_STEREO;
    }
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    return true;
}

static const clap_plugin_audio_ports_t audio_ports = {
    .count = audio_ports_count,
    .get = audio_ports_get,
};


/****************************************************************
 ** define the MIDI port
 */

static uint32_t midi_ports_count(const clap_plugin_t *plugin, bool is_input) {
    if (is_input) return 1;
    return 0;
}

static bool midi_ports_get(const clap_plugin_t *plugin, uint32_t index,
                            bool is_input, clap_note_port_info_t *info) {
    if (index > 0)
        return false;
    info->id = 0;
    snprintf(info->name, sizeof(info->name), "%s", "loopino");
    info->supported_dialects = CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect = CLAP_NOTE_DIALECT_MIDI;
    return true;
}

static const clap_plugin_note_ports_t midi_ports = {
    .count = midi_ports_count,
    .get = midi_ports_get,
};

/****************************************************************
 ** Latency reporting
 */

static uint32_t latency_get(const clap_plugin_t *plugin) {
    return 0;
}

static const clap_plugin_latency_t latency_extension = {
    .get = latency_get,
};

/****************************************************************
 ** save and load states
 */

// State Management
static bool state_save(const clap_plugin_t *plugin, const clap_ostream_t *stream) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    plug->r->saveState(stream);
    return true;
}

static bool state_load(const clap_plugin_t *plugin, const clap_istream_t *stream) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (plug->r->readState(stream)) {
        plug->r->loadPresetToSynth();
        return true;
    }
    return false;
}

static const clap_plugin_state_t state_extension = {
    .save = state_save,
    .load = state_load,
};

/****************************************************************
 ** GUI handling
 */

static bool gui_is_api_supported(const clap_plugin *plugin, const char *api, bool is_floating) {
    return strcmp(api, GUIAPI) == 0;
}

static bool gui_get_preferred_api(const clap_plugin_t *plugin, const char **api, bool *isFloating) {
    *api = GUIAPI;
    *isFloating = false;
    return true;
}

static bool gui_set_scale(const clap_plugin_t *plugin, double scale) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    plug->r->getMain()->hdpi = scale;
    return true;
}

static bool gui_get_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    *width = plug->r->w_top->width;
    *height = plug->r->w_top->height;
    return true;
}

static bool gui_can_resize(const clap_plugin_t *plugin) {
    return true;
}

static bool gui_get_resize_hints(const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints) {
    return false;
}

static bool gui_adjust_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    plug->width = *width;
    plug->height = *height;
    return true;   
}

static bool gui_set_transient(const clap_plugin_t *plugin, const clap_window_t *window) {
    return false;
}

static void gui_suggest_title(const clap_plugin_t *plugin, const char *title) {
    title = "Loopino";
}

static bool gui_create(const clap_plugin *plugin, const char *api, bool is_floating) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (strcmp(api, GUIAPI) == 0) {
        if (!plug->guiIsCreated) {
            plug->r->startGui();
        }
        plug->guiIsCreated = true;
        return true;
    }
    return false;
}

static void gui_destroy(const clap_plugin *plugin) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (plug->guiIsCreated) {
        plug->r->quitGui();
    }
    plug->guiIsCreated = false;
}

static bool gui_show(const clap_plugin *plugin) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    plug->r->showGui();
    return true;
}

static bool gui_hide(const clap_plugin *plugin) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    plug->r->hideGui();
    return true;
}

// embed the GUI
static bool gui_set_parent(const clap_plugin_t *plugin, const clap_window_t *window) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    if (!plug->guiIsCreated) {
        #if defined(_WIN32)
        plug->r->startGui((Window)window->win32);
        #else
        plug->r->startGui(window->x11);
        #endif
    }
    plug->guiIsCreated = true;
    #if defined(_WIN32)
    plug->r->setParent((Window)window->win32);
    #else
    plug->r->setParent(window->x11);
    #endif
    return true;
}

static bool gui_set_size(const clap_plugin_t *plugin, uint32_t width, uint32_t height) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    os_resize_window(plug->r->getMain()->dpy, plug->r->w_top, width, height);
    return true;
}


// Main thread callback (we run our own main thread)
static void on_main_thread(const clap_plugin_t *plugin) {
   // plugin_t *plug = (plugin_t *)plugin->plugin_data;
   // plug->r->runGui();
}

static const clap_plugin_gui_t extensionGUI = {
    .is_api_supported = gui_is_api_supported,
    .get_preferred_api = gui_get_preferred_api,
    .create = gui_create,
    .destroy = gui_destroy,
    .set_scale = gui_set_scale,
    .get_size = gui_get_size,
    .can_resize = gui_can_resize,
    .get_resize_hints = gui_get_resize_hints,
    .adjust_size = gui_adjust_size,
    .set_size = gui_set_size,
    .set_parent = gui_set_parent,
    .set_transient = gui_set_transient,
    .suggest_title = gui_suggest_title,
    .show = gui_show,
    .hide = gui_hide,
};

/****************************************************************
 ** Plugin handling
 */

// Initialize the plugin
static bool init(const clap_plugin_t *plugin) {
    //plugin_t *plug = (plugin_t *)plugin->plugin_data;
    //plug->r->initEngine(48000, 25, 1);
    return true;
}

// Destroy the plugin
static void destroy(const clap_plugin_t *plugin) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    gui_destroy(plugin);
    delete plug->r;
    free(plug);
}

// MIDI processing
static void clap_plug_process_event(plugin_t *plug, const clap_event_header_t *hdr) {
    if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID) {
        switch (hdr->type) {
            case CLAP_EVENT_NOTE_ON: {
                const clap_event_note_t *ev = (const clap_event_note_t *)hdr;
                plug->r->synth.noteOn(ev->key, ev->velocity);
                break;
            }

            case CLAP_EVENT_NOTE_OFF: {
                const clap_event_note_t *ev = (const clap_event_note_t *)hdr;
                plug->r->synth.noteOff(ev->key);
                break;
            }

            case CLAP_EVENT_NOTE_CHOKE: {
                const clap_event_note_t *ev = (const clap_event_note_t *)hdr;
                plug->r->synth.noteOff(ev->key);
                break;
            }

            case CLAP_EVENT_MIDI: {
                const clap_event_midi_t *ev = (const clap_event_midi_t *)hdr;
                MidiKeyboard* keys = nullptr;
                if (plug->guiIsCreated)
                    keys = (MidiKeyboard*)plug->r->keyboard->private_struct;
                if ((ev->data[0] & 0xf0) == 0xc0) {  // program change on any midi channel
                    plug->r->loadPresetNum((int)(ev->data[1]));
                } else if ((in_event.buffer[0] & 0xf0) == 0xb0) {   // controller
                    if (in_event.buffer[1]== 71) {
                        ui.synth.setReso((int)in_event.buffer[2]);
                    } else if (in_event.buffer[1]== 74) {
                        ui.synth.setCutoff((int)in_event.buffer[2]);
                    }
                } else if ((ev->data[0] & 0xf0) == 0x90) {   // Note On
                    int velocity = (int)ev->data[2];
                    if (velocity < 1) {
                        plug->r->synth.noteOff((int)(ev->data[1]));
                        if (plug->guiIsCreated)
                            set_key_in_matrix(keys->in_key_matrix[0], (int)ev->data[1], false);
                    } else {
                        plug->r->synth.noteOn((int)(ev->data[1]), (float)((float)velocity/127.0f));
                        if (plug->guiIsCreated)
                            set_key_in_matrix(keys->in_key_matrix[0], (int)ev->data[1], true);
                    }
                } else if ((ev->data[0] & 0xf0) == 0x80) {   // Note Off
                    plug->r->synth.noteOff((int)(ev->data[1]));
                    if (plug->guiIsCreated)
                        set_key_in_matrix(keys->in_key_matrix[0], (int)ev->data[1], false);
                }
                break;
            }
        }
    }
}

// Audio processing
static clap_process_status process(const clap_plugin_t *plugin, const clap_process_t *process) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    // Ensure there is two outputs
    if (process->audio_outputs[0].channel_count < 2) {
        return false; // Invalid format
    }

    //float *input = process->audio_inputs[0].data32[0]; // Mono input channel
    float *left_output = process->audio_outputs[0].data32[0]; // Left channel of stereo output
    float *right_output = process->audio_outputs[0].data32[1]; // Right channel of stereo output
    uint32_t nframes = process->frames_count;
    const uint32_t nev = process->in_events->size(process->in_events);
    uint32_t ev_index = 0;
    uint32_t next_ev_frame = nev > 0 ? 0 : nframes;

    if (plug->r->param.controllerChanged.load(std::memory_order_acquire)) {
        sync_params_to_host(plugin, process->out_events);
        plug->r->param.controllerChanged.store(false, std::memory_order_release);
    }


    static float fRec0[2] = {0};
    if (( plug->r->af.samplesize && plug->r->af.samples != nullptr) && plug->r->play && plug->r->ready) {
        float fSlow0 = 0.0010000000000000009 * plug->r->gain;
        for (uint32_t i = 0; i < nframes; i++) {
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
    for (uint32_t i = 0; i < nframes; i++) {
        // process events
        while (ev_index < nev && next_ev_frame == i) {
            const clap_event_header_t *hdr = process->in_events->get(process->in_events, ev_index);
            if (hdr->time != i) {
                next_ev_frame = hdr->time;
                break;
            }
            clap_plug_process_event(plug, hdr);
            sync_params_to_plug(plugin, hdr);
            ++ev_index;

            if (ev_index == nev) {
                // we reached the end of the event list
                next_ev_frame = nframes;
                break;
            }
        }
        // process synth
        fRec0[0] = fSlow0 + 0.999 * fRec0[1];
        float out = plug->r->synth.process() * fRec0[0];
        left_output[i] += out;
        right_output[i] += out;
        fRec0[1] = fRec0[0];
    }

    return CLAP_PROCESS_CONTINUE;
}

// Finally get the sample rate and init the engine
static bool activate(const struct clap_plugin *plugin,
                             double                    sample_rate,
                             uint32_t                  min_frames_count,
                             uint32_t                  max_frames_count) {
    plugin_t *plug = (plugin_t *)plugin->plugin_data;
    plug->r->setJackSampleRate(sample_rate);
    plug->isInited = true;
    return true;
}

// clear the state string when we get deactivated
static void deactivate(const struct clap_plugin *plugin) {
   // plugin_t *plug = (plugin_t *)plugin->plugin_data;
}

static bool start_processing(const struct clap_plugin *plugin) { return true; }

static void stop_processing(const struct clap_plugin *plugin) {}

static void reset(const struct clap_plugin *plugin) {}

// CLAP plugin descriptor
static const clap_plugin_descriptor_t descriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id = "com.brummer10.Loopino",
    .name = "Loopino",
    .vendor = "brummer10",
    .url = "https://github.com/brummer10/Loopino",
    .manual_url = "https://github.com/brummer10/Loopino",
    .support_url = "https://github.com/brummer10/Loopino",
    .version = "0.1.9",
    .description = "CLAP plugin wrapper for Loopino",
    .features = (const char *[]){ CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, CLAP_PLUGIN_FEATURE_STEREO,  NULL },
};

// Extensions
static const void *get_extension(const clap_plugin_t *plugin, const char *id) {
    if (!strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audio_ports;
    if (!strcmp(id, CLAP_EXT_NOTE_PORTS)) return &midi_ports;
    if (!strcmp(id, CLAP_EXT_LATENCY)) return &latency_extension;
    if (!strcmp(id, CLAP_EXT_GUI)) return &extensionGUI;
    if (!strcmp(id, CLAP_EXT_PARAMS)) return &params;
    if (!strcmp(id, CLAP_EXT_STATE)) return &state_extension;
    return NULL;
}

// Create the plugin
static const clap_plugin_t *create(const clap_host_t *host) {
    plugin_t *plug = (plugin_t *)calloc(1, sizeof(plugin_t));
    if (!plug) return NULL;
    plug->r = new Loopino();
    plug->guiIsCreated = false;
    plug->isInited = false;
    plug->width = WINDOW_WIDTH;
    plug->height = WINDOW_HEIGHT;
    plug->plugin.desc = &descriptor;
    plug->plugin.plugin_data = plug;
    plug->plugin.init = init;
    plug->plugin.destroy = destroy;
    plug->plugin.activate = activate;
    plug->plugin.deactivate = deactivate;
    plug->plugin.start_processing = start_processing;
    plug->plugin.stop_processing = stop_processing;
    plug->plugin.reset = reset;
    plug->plugin.process = process;
    plug->plugin.get_extension = get_extension;
    plug->plugin.on_main_thread = on_main_thread;
    plug->host = host;
    return &plug->plugin;
}

/****************************************************************
 ** the factory entry
 */

static uint32_t plugin_factory_get_plugin_count(const struct clap_plugin_factory *factory) {
   return 1;
}

static const clap_plugin_descriptor_t *plugin_factory_get_descriptor
                    (const struct clap_plugin_factory *factory, uint32_t index) {
   return  &descriptor; //s_plugins[index].desc;
}

static const clap_plugin_t *plugin_factory_create_neuralrack
                        (const struct clap_plugin_factory *factory,
                        const clap_host_t *host, const char *plugin_id) {

   if (!clap_version_is_compatible(host->clap_version)) {
      return NULL;
   }
   return create(host);
}

static const clap_plugin_factory_t plugin_factory = {
    .get_plugin_count = plugin_factory_get_plugin_count,
    .get_plugin_descriptor = plugin_factory_get_descriptor,
    .create_plugin = plugin_factory_create_neuralrack,
};

static const void *entry_get_factory(const char *factory_id) {
    return &plugin_factory;
}

static bool entry_init(const char *plugin_path) {
   // perform the plugin initialization
   return true;
}

static void entry_deinit(void) {
   // perform the plugin de-initialization
}

/****************************************************************
 ** Finally the CLAP plugin entry export
 */

extern "C" const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,
    .init = entry_init,
    .deinit = entry_deinit,
    .get_factory = entry_get_factory,
};
