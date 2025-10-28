
/*
 * SupportedFormats.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 Christopher Arndt <chris@chrisarndt.de>
 */

#include <filesystem>
#include <set>
#include <string>
#include <sndfile.hh>

#pragma once

#ifndef SUPPORTEDFORMATS_H
#define SUPPORTEDFORMATS_H

/****************************************************************
    class SupportedFormats - check libsndfile for supported file formats
****************************************************************/

class SupportedFormats {
public:
    SupportedFormats() {
        supportedExtensions = getSupportedFileExtensions();
    }

    bool isSupported(std::string filename) {
        std::filesystem::path p(filename);
        std::string ext = p.extension().string();

        if (not ext.empty()) {
            // check for lower-cased file extension
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
            return supportedExtensions.count(ext.substr(1)) >= 1;
        }

        return false;
    }

private:
    std::set<std::string> supportedExtensions;

    std::set<std::string> getSupportedFileExtensions() {
        std::set<std::string> extensions;

        // Get the number of supported simple formats
        int simpleFormatCount;
        sf_command(nullptr, SFC_GET_SIMPLE_FORMAT_COUNT, &simpleFormatCount, sizeof(int));

        // Get the number of supported major formats
        int majorFormatCount;
        sf_command(nullptr, SFC_GET_FORMAT_MAJOR_COUNT, &majorFormatCount, sizeof(int));

        // Get the number of supported sub formats
        int subFormatCount;
        sf_command(nullptr, SFC_GET_FORMAT_SUBTYPE_COUNT, &subFormatCount, sizeof(int));

        // Get information about each simple format
        for (int i = 0; i < simpleFormatCount; ++i) {
            SF_FORMAT_INFO formatInfo;
            formatInfo.format = i;
            sf_command(nullptr, SFC_GET_SIMPLE_FORMAT, &formatInfo, sizeof(formatInfo));

            if (formatInfo.extension != nullptr)
                extensions.insert(formatInfo.extension);
        }

        // Get information about each major format
        for (int i = 0; i < majorFormatCount; i++) {
            SF_FORMAT_INFO formatInfo;
            formatInfo.format = i;
            sf_command(nullptr, SFC_GET_FORMAT_MAJOR, &formatInfo, sizeof(formatInfo));

            if (formatInfo.extension != nullptr)
                extensions.insert(formatInfo.extension);
        }

        // Get information about each sub format
        for (int j = 0; j < subFormatCount; j++) {
            SF_FORMAT_INFO formatInfo;
            formatInfo.format = j;
            sf_command(nullptr, SFC_GET_FORMAT_SUBTYPE, &formatInfo, sizeof(SF_FORMAT_INFO));

            if (formatInfo.extension != nullptr)
                extensions.insert(formatInfo.extension);
        }

        return extensions;
    }
};

#endif
