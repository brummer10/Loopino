
/*
 * Wheel.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2024 brummer <brummer@web.de>
 */

#pragma once

#ifndef WHEEL_H
#define WHEEL_H

#include <math.h>
#include "xwidgets.h"

#define WHEEL_MIN -1.0f
#define WHEEL_MAX  1.0f


typedef struct {
    float value;
    float sensitivity;
    int is_dragging;
    int drag_start_y;
    float drag_start_value;
    int spring_active;
    int set_from_extern;
    float spring_velocity;
} Wheel;

void round_rectangle(cairo_t *cr, float x, float y, float width, float height, float r) {
    cairo_arc(cr, x+r, y+r, r, M_PI, 3*M_PI/2);
    cairo_arc(cr, x+width-r, y+r, r, 3*M_PI/2, 0);
    cairo_arc(cr, x+width-r, y+height-r, r, 0, M_PI/2);
    cairo_arc(cr, x+r, y+height-r, r, M_PI/2, M_PI);
    cairo_close_path(cr);
}

static void wheel_draw(void *w_, void *user_data) {
    Widget_t *w = (Widget_t*)w_;
    Wheel *wheel = (Wheel*)w->private_struct;
    cairo_t *crb = w->crb;

    int width  = w->width - 4.0f;
    int height = w->height;

    float wheel_h = height * 0.70f;
    float wheel_y = (height - wheel_h) * 0.5f;

    float angle = wheel->value * 1.3f;
    float disp = sin(angle) * wheel_h * 0.45f;

    float slot_h = wheel_h * 1.25f;
    float slot_y = (height - slot_h) * 0.5f;

    cairo_pattern_t *slot = cairo_pattern_create_linear(0, slot_y, 0, slot_y + slot_h);
    cairo_pattern_add_color_stop_rgb(slot,0.00, 0.03, 0.03, 0.03);
    cairo_pattern_add_color_stop_rgb(slot,0.50, 0.06, 0.06, 0.06);
    cairo_pattern_add_color_stop_rgb(slot,1.00, 0.02, 0.02, 0.02);
    cairo_set_source(crb, slot);
    round_rectangle(crb, 0, slot_y, width+4.0f, slot_h, wheel_h * 0.20f);
    cairo_fill(crb);
    cairo_pattern_destroy(slot);

    cairo_pattern_t *ao = cairo_pattern_create_linear(0, slot_y, 0, slot_y + slot_h);
    cairo_pattern_add_color_stop_rgba(ao,0,   0,0,0,0.25);
    cairo_pattern_add_color_stop_rgba(ao,1,   0,0,0,0.00);
    cairo_set_source(crb, ao);
    round_rectangle(crb, 0, slot_y + slot_h*0.2f, width+4.0f, slot_h*0.8f, wheel_h*0.20f);
    cairo_fill(crb);
    cairo_pattern_destroy(ao);

    cairo_pattern_t *pat = cairo_pattern_create_linear(0, wheel_y, 0, wheel_y + wheel_h);
    cairo_pattern_add_color_stop_rgb(pat,0,   0.06, 0.06, 0.06);
    cairo_pattern_add_color_stop_rgb(pat,0.5, 0.10, 0.10, 0.10);
    cairo_pattern_add_color_stop_rgb(pat,1,   0.05, 0.05, 0.05);
    cairo_set_source(crb, pat);
    round_rectangle(crb, 2, wheel_y + disp*0.08f, width, wheel_h, wheel_h*0.15f);
    cairo_fill(crb);
    cairo_pattern_destroy(pat);

    cairo_pattern_t *bloom = cairo_pattern_create_radial(
        width*0.5f, wheel_y + wheel_h*0.5f + disp*0.10f, wheel_h*0.05f,
        width*0.5f, wheel_y + wheel_h*0.5f + disp*0.10f, wheel_h*0.55f
    );
    cairo_pattern_add_color_stop_rgba(bloom,0, 1,1,1,0.05);
    cairo_pattern_add_color_stop_rgba(bloom,1, 1,1,1,0.00);
    cairo_set_source(crb, bloom);
    round_rectangle(crb, 2, wheel_y, width, wheel_h, wheel_h*0.15f);
    cairo_fill(crb);
    cairo_pattern_destroy(bloom);

    int grooves = 12;
    float spacing = wheel_h / (grooves + 1);
    round_rectangle(crb, 2, wheel_y, width, wheel_h, wheel_h*0.15f);
    cairo_clip (crb);
    for (int i = 1; i <= grooves*2; i++) {
        float gy = -wheel_y + i * spacing + disp;

        cairo_set_source_rgba(crb, 0, 0, 0, 0.35);
        cairo_set_line_width(crb, 3.0);
        cairo_move_to(crb, (width+4.0f) * 0.18f, gy);
        cairo_line_to(crb, (width+4.0f) * 0.82f, gy);
        cairo_stroke(crb);

        cairo_set_source_rgba(crb, 1, 1, 1, 0.06);
        cairo_set_line_width(crb, 1.2);
        cairo_move_to(crb, (width+4.0f) * 0.18f, gy - 1.8f);
        cairo_line_to(crb, (width+4.0f) * 0.82f, gy - 1.8f);
        cairo_stroke(crb);
    }
    cairo_new_path (crb); 

    float notch_w = width * 0.65f;
    float notch_x = width*0.5f - notch_w*0.5f;
    float notch_y = wheel_y + wheel_h*0.5f + disp;

    cairo_set_source_rgba(crb, 0.72, 0.72, 0.72, 0.75);
    round_rectangle(crb, notch_x+2.0f,
                    notch_y - wheel_h*0.045f,
                    notch_w,
                    wheel_h*0.06f,
                    wheel_h*0.035f);
    cairo_fill(crb);
}

static void wheel_button_press(void *w_, void* button_, void *user_data) {
    Widget_t *w = (Widget_t*)w_;
    Wheel *wheel = (Wheel*)w->private_struct;
    XButtonEvent *xbutton = (XButtonEvent*)button_;
    wheel->is_dragging = 1;
    wheel->drag_start_y = xbutton->y;
    wheel->drag_start_value = wheel->value;
    wheel->spring_active = 0;
}

static void wheel_button_release(void *w_, void* button_, void *user_data) {
    Widget_t *w = (Widget_t*)w_;
    Wheel *wheel = (Wheel*)w->private_struct;
    wheel->is_dragging = 0;
    wheel->spring_active = 1;
    wheel->spring_velocity = 0.0f;
}

static void wheel_motion(void *w_, void* xmotion_, void *user_data) {
    Widget_t *w = (Widget_t*)w_;
    Wheel *wheel = (Wheel*)w->private_struct;
    XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
    if (!wheel->is_dragging) return;

    int dy = xmotion->y - wheel->drag_start_y;
    float v = wheel->drag_start_value + dy * wheel->sensitivity;
    if (v < WHEEL_MIN) v = WHEEL_MIN;
    if (v > WHEEL_MAX) v = WHEEL_MAX;

    wheel->value = v;
    w->func.value_changed_callback(w, user_data);
    expose_widget(w);
}


static void wheel_set_value(Widget_t *w, float v) {
    Wheel *wheel = (Wheel*)w->private_struct;
    wheel->value = v;
    wheel->set_from_extern = 1;
}

static void wheel_idle_callback(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Wheel *wheel = (Wheel*)w->private_struct;
    if (!wheel->spring_active && wheel->value == 0.0f && !wheel->set_from_extern) return;

    if (!wheel->set_from_extern && wheel->spring_active) {
        float k = 0.08f;
        float d = 0.25f;

        float force = -wheel->value * k;
        wheel->spring_velocity += force;
        wheel->spring_velocity *= (1.0f - d);
        wheel->value += wheel->spring_velocity;

        if (fabs(wheel->value) < 0.001f && fabs(wheel->spring_velocity) < 0.001f) {
            wheel->value = 0.0f;
            wheel->spring_active = 0;
            wheel->set_from_extern = 0;
        }

        w->func.value_changed_callback(w, user_data);
    }
    if (wheel->value == 0.0f) wheel->set_from_extern = 0;
    expose_widget(w);
}

static void wheel_mem_free(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    Wheel *wheel = (Wheel*)w->private_struct;
    free(wheel);
}

Widget_t *add_wheel(Widget_t *parent, const char *label, int x, int y, int w, int h) {
    Widget_t *wheel_w = create_widget(parent->app, parent, x, y, w, h);
    wheel_w->func.expose_callback = wheel_draw;
    wheel_w->func.button_press_callback = wheel_button_press;
    wheel_w->func.button_release_callback = wheel_button_release;
    wheel_w->func.motion_callback = wheel_motion;
    wheel_w->func.mem_free_callback = wheel_mem_free;

    Wheel *wheel = (Wheel*)calloc(1, sizeof(Wheel));
    wheel_w->private_struct = wheel;
    wheel_w->flags |= HAS_MEM;
    wheel->value = 0.0f;
    wheel->sensitivity = 0.025f;
    wheel->spring_active = 0;
    wheel->spring_velocity = 0;
    wheel->set_from_extern = 0;

    return wheel_w;
}

#endif
