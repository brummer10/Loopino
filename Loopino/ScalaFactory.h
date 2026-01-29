/*
 * ScalaFactory.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <string>

/****************************************************************
        ScalaFactory -  define some default tuning scales
                        or parse scl and kbm files
****************************************************************/

namespace Scala {

struct TuningTable {
    int periodSteps = 12; 
    int notesPerOctave = 12;
    int rootMidi = 69;
    double rootFreq = 440.0;

    std::vector<double> cents;
    std::array<int, 128> keymap;

    TuningTable() {
        keymap.fill(-1);
    }
};

/****************************************************************
        Factory helper
****************************************************************/

inline void fillKeymapChromatic(TuningTable& t) {
    t.keymap.fill(-1);
    for (int midi = 0; midi < 128; ++midi) {
        int delta = midi - t.rootMidi;
        t.keymap[midi] = ((delta % t.periodSteps) + t.periodSteps) % t.periodSteps;
    }
}

inline void resetRoot(TuningTable& t) {
    t.rootMidi = 69;
    t.rootFreq = 440.0;
}

/****************************************************************
        Factory Scala tunings
****************************************************************/

inline void makeEqual12(TuningTable& t) {
    t.periodSteps = 12;
    t.notesPerOctave = 13;
    resetRoot(t);

    t.cents.clear();
    for (int i = 0; i <= 12; ++i)
        t.cents.push_back(i * 100.0);

    fillKeymapChromatic(t);
}

inline void makeEqualN(TuningTable& t, int divisions) {
    t.periodSteps = divisions;
    t.notesPerOctave = divisions + 1;
    resetRoot(t);

    t.cents.clear();
    for (int i = 0; i <= divisions; ++i)
        t.cents.push_back(1200.0 * i / divisions);

    fillKeymapChromatic(t);
}

inline void makeJust12(TuningTable& t) {
    t.periodSteps = 12;
    t.notesPerOctave = 13;
    resetRoot(t);
    t.cents.clear();
    t.cents.push_back(0.0);

    const double ratios[12][2] = {
        {16,15}, {9,8}, {6,5}, {5,4}, {4,3}, {45,32},
        {3,2}, {8,5}, {5,3}, {9,5}, {15,8}, {2,1} };

    for (auto& r : ratios)
        t.cents.push_back(1200.0 * std::log2(r[0] / r[1]));

    fillKeymapChromatic(t);
}

inline void makePythagorean12(TuningTable& t) {
    t.periodSteps = 12;
    t.notesPerOctave = 13;
    resetRoot(t);
    t.cents.clear();
    t.cents.push_back(0.0);

    const double ratios[12][2] = {
        {2187,2048}, {9,8}, {19683,16384}, {81,64},
        {4,3}, {729,512}, {3,2}, {6561,4096},
        {27,16}, {59049,32768}, {243,128}, {2,1} };

    for (auto& r : ratios)
        t.cents.push_back(1200.0 * std::log2(r[0] / r[1]));

    fillKeymapChromatic(t);
}

inline void makeMeantoneQuarterComma(TuningTable& t) {
    t.periodSteps = 12;
    t.notesPerOctave = 13;
    resetRoot(t);
    t.cents.clear();

    static const double cents[] = {
        0.0, 76.049, 193.156, 310.263, 386.314,
        503.421, 579.472, 696.578, 772.629,
        889.736, 1006.843, 1082.894, 1200.0 };

    t.cents.assign(cents, cents + 13);
    fillKeymapChromatic(t);
}

inline void makeWerckmeisterIII(TuningTable& t) {
    t.periodSteps = 12;
    t.notesPerOctave = 13;
    resetRoot(t);
    t.cents.clear();

    static const double cents[] = {
        0.0, 90.225, 192.180, 294.135, 390.225,
        498.045, 588.270, 696.090, 792.180,
        888.270, 996.090, 1092.180, 1200.0 };

    t.cents.assign(cents, cents + 13);
    fillKeymapChromatic(t);
}

inline void makeKirnbergerIII(TuningTable& t) {
    t.periodSteps = 12;
    t.notesPerOctave = 13;
    resetRoot(t);
    t.cents.clear();

    static const double cents[] = {
        0.0, 90.225, 193.156, 294.135, 386.314,
        498.045, 588.270, 696.090, 792.180,
        888.270, 996.090, 1088.269, 1200.0 };

    t.cents.assign(cents, cents + 13);
    fillKeymapChromatic(t);
}

inline void makeHarmonicSeries(TuningTable& t) {
    t.periodSteps = 8;
    t.notesPerOctave = 9;
    resetRoot(t);
    t.cents.clear();

    t.cents.clear();
    for (int i = 8; i <= 16; ++i)
        t.cents.push_back(1200.0 * std::log2(i / 8.0));

    fillKeymapChromatic(t);
}

inline void makeBohlenPierce(TuningTable& t) {
    t.periodSteps = 13;
    t.notesPerOctave = 13;
    resetRoot(t);
    t.cents.clear();

    static const double cents[] = {
        0, 146.3, 292.6, 439.0, 585.3, 731.6,
        878.0, 1024.3, 1170.6, 1317.0,
        1463.3, 1609.6, 1755.9 };

    t.cents.assign(cents, cents + 13);
    fillKeymapChromatic(t);
}

inline void makePelog(TuningTable& t) {
    t.periodSteps = 7;
    t.notesPerOctave = 8;
    resetRoot(t);
    t.cents.clear();

    static const double cents[] = {
        0.0, 150.0, 290.0, 440.0, 590.0,
        720.0, 870.0, 1200.0 };

    t.cents.assign(cents, cents + 8);
    fillKeymapChromatic(t);
}

inline void makeSlendro(TuningTable& t) {
    t.periodSteps = 5;
    t.notesPerOctave = 6;
    resetRoot(t);
    t.cents.clear();

    static const double cents[] = {
        0.0, 240.0, 480.0, 720.0, 960.0, 1200.0 };

    t.cents.assign(cents, cents + 6);
    fillKeymapChromatic(t);
}

inline void makeCarlosAlpha(TuningTable& t) {
    t.periodSteps = 9;
    t.notesPerOctave = 10;
    resetRoot(t);
    t.cents.clear();

    static const double cents[] = {
        0.0, 78.0, 156.0, 234.0, 312.0,
        390.0, 468.0, 546.0, 624.0, 1200.0 };

    t.cents.assign(cents, cents + 10);
    fillKeymapChromatic(t);
}

inline void makeCarlosBeta(TuningTable& t) {
    t.periodSteps = 11;
    t.notesPerOctave = 12;
    resetRoot(t);
    t.cents.clear();

    static const double cents[] = {
        0.0, 63.8, 127.6, 191.4, 255.2, 319.0,
        382.8, 446.6, 510.4, 574.2, 638.0, 1200.0 };

    t.cents.assign(cents, cents + 12);
    fillKeymapChromatic(t);
}

inline void makeCarlosGamma(TuningTable& t) {
    t.periodSteps = 20;
    t.notesPerOctave = 21;
    resetRoot(t);
    t.cents.clear();

    t.cents.clear();
    for (int i = 0; i <= 20; ++i)
        t.cents.push_back(i * (1200.0 / 20.0));

    fillKeymapChromatic(t);
}

/****************************************************************
        file parse helper
****************************************************************/

inline void trim(std::string& s) {
    const char* ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    s.erase(s.find_last_not_of(ws) + 1);
}

/****************************************************************
        parse scl files
****************************************************************/

inline bool loadSCL(const std::string& path, TuningTable& t) {
    std::ifstream file(path);
    if (!file) return false;

    std::string scaleName;
    bool haveName = false;
    bool haveCount = false;
    int expectedNotes = 0;

    std::string line;
    std::vector<double> cents;

    while (std::getline(file, line)) {
        trim(line);
        auto excl = line.find('!');
        if (excl != std::string::npos) line = line.substr(0, excl);
        trim(line);
        if (line.empty()) continue;

        if (!haveName) {
            scaleName = line;
            haveName = true;
            continue;
        }

        if (!haveCount) {
            expectedNotes = std::stoi(line);
            haveCount = true;
            continue;
        }

        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token.empty()) continue;
        if (token.find('/') != std::string::npos) {
            double num = 0.0, den = 1.0;
            char slash;
            std::stringstream(token) >> num >> slash >> den;
            if (den != 0.0) cents.push_back(1200.0 * std::log2(num / den));
        } else { 
            cents.push_back(std::stod(token));
        }
    }

    if (cents.empty()) return false;

    cents.insert(cents.begin(), 0.0);
    t.periodSteps   = expectedNotes;
    t.notesPerOctave = expectedNotes + 1;
    t.cents = std::move(cents);

    if ((int)t.cents.size() != t.notesPerOctave) return false;

    for (int midi = 0; midi < 128; ++midi) {
        int delta = midi - t.rootMidi;
        int n = t.periodSteps;
        t.keymap[midi] = ((delta % n) + n) % n;
    }

    return true;
}

/****************************************************************
        parse kbm files
****************************************************************/

inline bool loadKBM(const std::string& path, TuningTable& t) {
    std::ifstream file(path);
    if (!file) return false;

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(file, line)) {
        auto excl = line.find('!');
        if (excl != std::string::npos)
            line = line.substr(0, excl);

        trim(line);
        if (!line.empty()) lines.push_back(line);
    }

    if (lines.size() < 7) return false;

    size_t idx = 0;
    int mapSize   = std::stoi(lines[idx++]);
    int firstMidi = std::stoi(lines[idx++]);
    int lastMidi  = std::stoi(lines[idx++]);

    idx++; // middleMidi (ignored, but consumed)
    t.rootMidi = std::stoi(lines[idx++]);
    t.rootFreq = std::stod(lines[idx++]);
    idx++; // formalOctaveDegree (ignored)

    t.keymap.fill(-1);
    // explicit mapping present
    if (mapSize > 0) {
        for (int i = 0; i < mapSize && idx < lines.size(); ++i, ++idx) {
            int degree = std::stoi(lines[idx]);
            int midi = firstMidi + i;
            if (midi < 0 || midi > 127) continue;
            t.keymap[midi] = degree;
        }
    }
    // no mapping → linear fallback
    else {
        const int n = t.periodSteps;

        for (int midi = firstMidi; midi <= lastMidi && midi < 128; ++midi) {
            if (midi < 0) continue;
            int delta = midi - t.rootMidi;
            int degree = ((delta % n) + n) % n;
            t.keymap[midi] = degree;
        }
    }

    return true;
}

/****************************************************************
        parse scl/kbm files
****************************************************************/

inline bool loadScala( const std::string& sclPath,
        const std::string& kbmPath, TuningTable& out ) {

    TuningTable tmp;
     makeEqual12(tmp);

    if (!sclPath.empty()) {
        if (!loadSCL(sclPath, tmp)) {
            makeEqual12(out);
            return false;
        }
    }

    if (!kbmPath.empty()) {
        if (!loadKBM(kbmPath, tmp)) {
            makeEqual12(out);
            return false;
        }
    }

    out = std::move(tmp);
    return true;
}

/****************************************************************
        Factory interface
****************************************************************/

inline std::string scaleName(int s) {
    switch (s) {
        case 0:             return "12-TET";
        case 1:             return "Just Intonation 12";
        case 2:             return "Pythagorean 12";

        case 3:             return "Meantone (1/4 comma)";
        case 4:             return "Werckmeister III";
        case 5:             return "Kirnberger III";

        case 6:             return "19-TET";
        case 7:             return "24-TET (Quartertone)";
        case 8:             return "31-TET";

        case 9:             return "Harmonic Series";
        case 10:            return "Pelog";
        case 11:            return "Slendro";

        case 12:            return "Bohlen–Pierce";

        case 13:            return "Carlos Alpha";
        case 14:            return "Carlos Beta";
        case 15:            return "Carlos Gamma";
    }
    return "Custom";
}

inline void setFactoryScale(int s, TuningTable& t) {
    switch (s) {
        case 0:             makeEqual12(t); break;
        case 1:             makeJust12(t); break;
        case 2:             makePythagorean12(t); break;

        case 3:             makeMeantoneQuarterComma(t); break;
        case 4:             makeWerckmeisterIII(t); break;
        case 5:             makeKirnbergerIII(t); break;

        case 6:             makeEqualN(t, 19); break;
        case 7:             makeEqualN(t, 24); break;
        case 8:             makeEqualN(t, 31); break;

        case 9:             makeHarmonicSeries(t); break;
        case 10:            makePelog(t); break;
        case 11:            makeSlendro(t); break;

        case 12:            makeBohlenPierce(t); break;

        case 13:            makeCarlosAlpha(t); break;
        case 14:            makeCarlosBeta(t); break;
        case 15:            makeCarlosGamma(t); break;
        default:            break;
    }
}

} // namespace Scala

