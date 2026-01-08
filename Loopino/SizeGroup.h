

/*
 * SizeGroup.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

/****************************************************************
        SizeGroup - implement a minimal flexbox for libxputty
****************************************************************/

#pragma once
#include "xwidgets.h"
#include <vector>

class SizeGroup {
public:
    SizeGroup() = default;

    void setParent(Widget_t* p,int sx, int sy,
                   int spx, int spy, int rackH) {

        parent   = p;
        startX   = sx;
        startY   = sy;
        startX1   = sx;
        startY1   = sy;
        spacingX = spx;
        spacingY = spy;
        cellH    = rackH;
        entries.clear();
        relayout();
    }

    void add(Widget_t* w) {
        entries.push_back(w);
        relayout();
    }

    void relayoutNow() {
        startX = startX1;
        startY = startY1;
        relayout();
    }

private:
    Widget_t* parent = nullptr;
    std::vector<Widget_t*> entries;

    int startX = 0, startY = 0;
    int startX1 = 0, startY1 = 0;
    int spacingX = 0, spacingY = 0;
    int cellH = 0;
    int spaceNextRox = 0;

    void relayout() {
        if (!parent) return;
        Display* dpy = parent->app->dpy;
        int maxX = parent->width;
        int x = startX;
        int y = startY;
        int rowUnits = 1;

        for (Widget_t* w : entries) {
            int units = (w->height + cellH - 1) / cellH;
            // wrap?
            if (x + w->width > maxX && x != startX) {
                x = startX + spaceNextRox;
                y += rowUnits * (cellH + spacingY);
                rowUnits = 1;
                spaceNextRox = 0;
            }

            os_move_window(dpy, w, x, y);
            w->scale.init_x = x; w->scale.init_y = y;
            x += w->width + spacingX;

            if (units > rowUnits) 
                spaceNextRox = x - spacingX;
                //rowUnits = units;
        }
    }

};
