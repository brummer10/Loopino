
/*
 * SizeGroup.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

/****************************************************************
      SizeGroup.h - implement a flex-box for libxputty
                    to order child widgets in a grid
                    with animated drag and drop support 
                    and non animated preset switching
****************************************************************/

#pragma once
#include <unordered_map> // must stay above xwidgets.h for min/max symbol clashing
#include "xwidgets.h"
#include <vector>
#include <cmath>

class SizeGroup {
public:
    SizeGroup() = default;

    // set the parent widget which act as a flex-box
    void setParent(Widget_t* p,int sx, int sy,
            int spx, int spy, int rackH, int *_glowX, int *_glowY) {

        parent   = p;
        startX   = sx;
        startY   = sy;
        startX1  = sx;
        startY1  = sy;
        spacingX = spx;
        spacingY = spy;
        cellH    = rackH;
        glowX = _glowX;
        glowY = _glowY;
        entries.clear();
        tweens.clear();
        animateOnAdd = true;
        relayout();
    }

    // add a widget to the flex-box
    void add(Widget_t* w) {
        entries.push_back(w);
        to = (int) entries.size();
        relayout();
    }

    // load a setting without animation
    void relayoutNow() {
        animateOnAdd = false;
        startX = startX1;
        startY = startY1;
        relayout();
    }

    // call from GUI idle loop (~60 fps) for animation
    void updateTweens(float dt) {
        if (!tweensActive || !parent) return;
        Display* dpy = parent->app->dpy;
        bool anyActive = false;

        for (auto& t : tweens) {
            if (t.t >= 1.0f) continue;

            t.t += dt * 6.0f;
            if (t.t > 1.0f) t.t = 1.0f;
            float s = t.t * t.t * (3.0f - 2.0f * t.t);
            int x = t.x0;
            int y = t.y0;
            x = t.x0 + (t.x1 - t.x0) * s;
            y = t.y0 + (t.y1 - t.y0) * s;
            os_move_window(dpy, t.w, x, y);
            if (t.t < 1.0f) anyActive = true;
        }

        if (!anyActive) tweensActive = false; 
    }

    // register a widget for dragging
    void beginDrag(Widget_t* w, int mx, int my) {
        dragWidget = w;
        dragOffsetX = mx;
        dragOffsetY = my;
        os_raise_widget(w);
    }

    // move a widget and draw drop indicator on parent widget
    void dragMove(int mx, int my) {
        if (!dragWidget) return;
        wmx = dragWidget->scale.init_x + mx - dragOffsetX;
        wmy = dragWidget->scale.init_y + my - dragOffsetY;
        os_move_window(parent->app->dpy, dragWidget, wmx, wmy);
        lastInRow = 0;
        auto it = std::find(entries.begin(), entries.end(), dragWidget);
        oldIndex = it - entries.begin();
        newIndex = findDropIndex(wmx, dragWidget->scale.init_y, oldIndex, lastInRow);
        expose_widget(parent);
        //std::cout << "Index " << oldIndex << std::endl;
    }

    // drop a widget to new position, do animation for reorder
    void endDrag(std::vector<int>& newOrder, int fm) {
        if (!dragWidget) return;
        animateOnAdd = true;
        auto it = std::find(entries.begin(), entries.end(), dragWidget);
        if (newIndex != oldIndex) {
            if (oldIndex < newIndex && !lastInRow) newIndex--;
            entries.erase(it);
            entries.insert(entries.begin() + newIndex, dragWidget);
        }
        from = oldIndex < newIndex ? oldIndex : newIndex;
        to = lastInRow;
        relayout();
        dragWidget = nullptr;
        (*glowX) = -1;
        newOrder.clear();
        //newOrder.push_back(0);
        if (fm) { // machines
            for (std::size_t i = 20; i < entries.size(); ++i) {
                newOrder.push_back(entries[i]->data);
            }
        } else { // filters
            for (std::size_t i = 8; i < 13; ++i) {
                newOrder.push_back(entries[i]->data);
            }
        }
      //  for (Widget_t* w : entries) {
      //      if (w->data > 0) {
      //          newOrder.push_back(w->data);
      //      }
      //  }
    }

    // machines from 20 - 25
    // apply a preset order non animated
    void applyPresetOrder(const std::vector<int>& presetOrder) {
        std::vector<Widget_t*> fx;
        fx.reserve(entries.size());

        for (Widget_t* w : entries)
            if (w && w->data > 0) fx.push_back(w);

        if (fx.size() != presetOrder.size()) return;

        std::unordered_map<int, Widget_t*> map;
        for (Widget_t* w : fx) map[w->data] = w;

        for (size_t i = 0; i < fx.size(); ++i)
            fx[i] = map[presetOrder[i]];

        size_t fxIndex = 0;
        for (Widget_t*& w : entries)
            if (w->data > 0) w = fx[fxIndex++];

        relayoutNow();
    }

private:

    struct Tween {
        Widget_t* w;
        int x0,y0;
        int x1,y1;
        float t;
    };

    Widget_t* parent = nullptr;
    Widget_t* dragWidget = nullptr;
    std::vector<Widget_t*> entries;
    std::vector<Tween> tweens;

    bool tweensActive = false;
    bool animateOnAdd = false;

    int startX = 0, startY = 0;
    int startX1 = 0, startY1 = 0;
    int spacingX = 0, spacingY = 0;
    int cellH = 0;
    int dragOffsetX = 0;
    int dragOffsetY = 0;
    int wmx = 0;
    int wmy = 0;
    int from = 0;
    int to = 0;
    int oldIndex = 0;
    int newIndex = 0;
    int lastInRow = 0;

    int *glowX = nullptr;
    int *glowY = nullptr;

    void relayout() {
        if (!parent) return;
        Display* dpy = parent->app->dpy;
        tweens.clear();
        int index = 0;

        int maxX = parent->width;
        int x = startX;
        int y = startY;
        int rowUnits = 1;

        for (Widget_t* w : entries) {
            if (x + w->width > maxX && x != startX) {
                x = startX ;
                y += rowUnits * (cellH + spacingY);
                rowUnits = 1;
            }

            if (animateOnAdd) {
                int slideX = startX - w->width - 20;
                if (index >= from && index <= to) {
                    if (from == 0 && to == (int)entries.size()) { // initial animation
                        tweens.push_back({ w, slideX, y, x, y, 0.0f });
                    } else { // partial animation
                        int oldX = w->scale.init_x;
                        int oldY = w->scale.init_y;
                        if (index == newIndex) { oldX = wmx; oldY = wmy;}
                        tweens.push_back({ w, oldX, oldY, x, y, 0.0f });
                    }
                    if (!dragWidget) os_move_window(dpy, w, slideX, y);
                }
                tweensActive = true; 
            } else {
                os_move_window(dpy, w, x, y);
            }

            w->scale.init_x = x;
            w->scale.init_y = y;
            x += w->width + spacingX;
            index++;
        }
    }

    int findDropIndex(int mx, int my, int oldIndex, int& lastInRow) {
        int best = 0;
        int bestDist = 1e9;

        for (size_t i = 0; i < entries.size(); ++i) {
            auto* w = entries[i];
            if (w->data == -1) continue; // fixed frame
            int cx = w->scale.init_x ;
            if ((int)i >= oldIndex) cx += w->width + spacingX;
            int cy = w->scale.init_y;
            int dx = std::abs(mx - cx);
            if (cy < my) continue;
            if (cy == my && dx < bestDist) {
                best = i;
                bestDist = dx;
                (*glowX) = cx - spacingX/2;
                (*glowY) = my ;
            }
            if (cy > my) {
                lastInRow = (int)i;
                break;
            }
            lastInRow = (int)i;
        }
        return best;
    }

};
