
/****************************************************************
        SizeGroup - implement a minimal flexbox for libxputty
****************************************************************/

#pragma once
#include "xwidgets.h"
#include <vector>
#include <cmath>

class SizeGroup {
public:
    SizeGroup() = default;

    void setParent(Widget_t* p,int sx, int sy,
                   int spx, int spy, int rackH) {

        parent   = p;
        startX   = sx;
        startY   = sy;
        startX1  = sx;
        startY1  = sy;
        spacingX = spx;
        spacingY = spy;
        cellH    = rackH;
        entries.clear();
        tweens.clear();
        animateOnAdd = true;
        relayout();
    }

    void add(Widget_t* w) {
        entries.push_back(w);
        relayout();
    }

    void relayoutNow() {
        animateOnAdd = false;
        startX = startX1;
        startY = startY1;
        relayout();
    }

    // call from GUI idle loop (~60 fps)
    void updateTweens(float dt) {
        if (!tweensActive || !parent) return;
        Display* dpy = parent->app->dpy;
        bool anyActive = false;

        for (auto& t : tweens) {
            if (t.t >= 1.0f) continue;

            t.t += dt * 6.0f;
            if (t.t > 1.0f) t.t = 1.0f;

            float s = t.t * t.t * (3.0f - 2.0f * t.t);
            int x = t.x0 + (t.x1 - t.x0) * s;
            int y = t.y0 + (t.y1 - t.y0) * s;
            os_move_window(dpy, t.w, x, y);
            if (t.t < 1.0f) anyActive = true;
        }
        if (!anyActive) tweensActive = false; 
    }

    void beginDrag(Widget_t* w, int mx, int my) {
        dragWidget = w;
        dragOffsetX = mx;
        dragOffsetY = my;
        os_raise_widget(w);
    }

    void dragMove(int mx, int my) {
        if (!dragWidget) return;
        wmx = dragWidget->scale.init_x + mx - dragOffsetX;
        wmy = dragWidget->scale.init_y + my - dragOffsetY;
        os_move_window(parent->app->dpy, dragWidget, wmx, wmy);
    }

    void endDrag(int mx, int my) {
        if (!dragWidget) return;
        int newIndex = findDropIndex(wmx, wmy);
        auto it = std::find(entries.begin(), entries.end(), dragWidget);
        int oldIndex = it - entries.begin();

        if (newIndex != oldIndex) {
            entries.erase(it);
            entries.insert(entries.begin() + newIndex, dragWidget);
            relayout();
        }

        dragWidget = nullptr;
    }

private:
    Widget_t* parent = nullptr;
    Widget_t* dragWidget = nullptr;
    std::vector<Widget_t*> entries;

    bool tweensActive = false;
    int startX = 0, startY = 0;
    int startX1 = 0, startY1 = 0;
    int spacingX = 0, spacingY = 0;
    int cellH = 0;
    int spaceNextRox = 0;
    int dragOffsetX = 0;
    int dragOffsetY = 0;
    int wmx = 0;
    int wmy = 0;

    struct Tween {
        Widget_t* w;
        int x0,y0;
        int x1,y1;
        float t;
    };

    std::vector<Tween> tweens;
    bool animateOnAdd = false;

    void relayout() {
        if (!parent) return;
        Display* dpy = parent->app->dpy;

        int maxX = parent->width;
        int x = startX;
        int y = startY;
        int rowUnits = 1;

        for (Widget_t* w : entries) {
            int units = (w->height + cellH - 1) / cellH;

            if (x + w->width > maxX && x != startX) {
                x = startX + spaceNextRox;
                y += rowUnits * (cellH + spacingY);
                rowUnits = 1;
                spaceNextRox = 0;
            }

            if (animateOnAdd) {
                int slideX = startX - w->width - 20;
                tweens.push_back({ w, slideX, y, x, y, 0.0f });
                os_move_window(dpy, w, slideX, y);
                tweensActive = true; 
            } else {
                os_move_window(dpy, w, x, y);
            }

            w->scale.init_x = x;
            w->scale.init_y = y;

            x += w->width + spacingX;

            if (units > rowUnits)
                spaceNextRox = x - spacingX;
                //rowUnits = units;
        }
    }

    int findDropIndex(int mx, int my) {
        int best = 0;
        int bestDist = 1e9;

        for (size_t i = 0; i < entries.size(); ++i) {
            auto* w = entries[i];
            int cx = w->scale.init_x + w->width/2;
            int cy = w->scale.init_y + w->height/2;
            int dx = mx - cx;
            int dy = my - cy;
            int d = dx*dx + dy*dy;
            if (d < bestDist) { best = i; bestDist = d; }
        }
        return best;
    }

};
