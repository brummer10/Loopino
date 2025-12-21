
/*
 * CmdParser.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        CmdParser.h  parse command-line args
                        
****************************************************************/

#pragma once

#include <optional>
#include <string>
#include <iostream>
#include <cstring>
#include <cstdlib>

struct CmdParser {

    struct CmdOptions {
        std::optional<std::string> midiDevice;
        std::optional<float> scaling;
        std::optional<int> bufferSize;
        std::optional<int> sampleRate;
    } opts;


    void printUsage(const char* progName) {
        std::cout
            << "Usage: " << progName << " [options]\n"
            << "Options:\n"
            << "  -d, --device <name>    MIDI device eg. hw:1,0,0\n"
            << "  -s, --scaling <value>  Scaling factor (float)\n"
            << "  -b, --buffer <value>   ALSA buffer size (int)\n"
            << "  -r, --rate <value>     ALSA Sample Rate (int)\n";
    }

    static bool parseFloat(const char* str, float& out) {
        char* end = nullptr;
        out = std::strtof(str, &end);
        return end != str && *end == '\0';
    }

    static bool parseInt(const char* str, int& out) {
        char* end = nullptr;
        out = (int)std::strtod(str, &end);
        return end != str && *end == '\0';
    }

    bool parseCmdLine(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            const char* arg = argv[i];

            if (std::strcmp(arg, "-d") == 0 || std::strcmp(arg, "--device") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --device requires a value\n";
                    return false;
                }
                opts.midiDevice = argv[++i];
            } else if (std::strcmp(arg, "-s") == 0 || std::strcmp(arg, "--scaling") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --scaling requires a value\n";
                    return false;
                }
                float value;
                if (!parseFloat(argv[++i], value)) {
                    std::cerr << "Error: invalid scaling value\n";
                    return false;
                }
                opts.scaling = value;
            } else if (std::strcmp(arg, "-b") == 0 || std::strcmp(arg, "--buffer") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --buffer requires a value\n";
                    return false;
                }
                int value;
                if (!parseInt(argv[++i], value)) {
                    std::cerr << "Error: invalid buffer value\n";
                    return false;
                }
                opts.bufferSize = value;
            } else if (std::strcmp(arg, "-r") == 0 || std::strcmp(arg, "--rate") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --rate requires a value\n";
                    return false;
                }
                int value;
                if (!parseInt(argv[++i], value)) {
                    std::cerr << "Error: invalid rate value\n";
                    return false;
                }
                opts.sampleRate = value;
            } else {
                std::cerr << "Error: unknown option '" << arg << "'\n";
                return false;
            }
        }
        return true;
    }

};
