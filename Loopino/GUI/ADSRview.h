
/*
 * ADSRview.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once
#include <math.h>
#include "xwidgets.h"

#define ADSR_PAD 4.0

typedef struct {
    Adjustment_t *a;
    Adjustment_t *d;
    Adjustment_t *s;
    Adjustment_t *r;
    double xA, yA, xD, yD, xS, yS, xR, yR;
    double drag_A0, drag_D0, drag_R0, drag_S0;
    double mx;
    int drag_part; // 0 none, 1=A 2=D 3=S 4=R
} ADSRWidget;

static void round_rec(cairo_t *cr, float x, float y, float w, float h, float r) {
    cairo_arc(cr, x+r, y+r, r, M_PI, 3*M_PI/2);
    cairo_arc(cr, x+w-r, y+r, r, 3*M_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r, 0, M_PI/2);
    cairo_arc(cr, x+r, y+h-r, r, M_PI/2, M_PI);
    cairo_close_path(cr);
}

static void draw_envelope(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    ADSRWidget *ad = (ADSRWidget*)w->private_struct;
    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    if (!metrics.visible) return;
    const int width_t = metrics.width;
    const int height_t = metrics.height;

    const double L = ADSR_PAD;
    const double R = width_t  - ADSR_PAD;
    const double T = ADSR_PAD;
    const double B = height_t - ADSR_PAD;

    const double VP_W = R - L;
    const double VP_H = B - T;

    // draw background
    cairo_set_line_width(w->crb,2);
    cairo_set_source_rgba(w->crb, 0.16f,0.18f,0.18f,1.0f);
    round_rec(w->crb, 0, 0, width_t, height_t, 5);
    cairo_fill_preserve(w->crb);
   // cairo_set_source_rgba(w->crb, 0.33, 0.33, 0.33, 1);
    cairo_stroke(w->crb);

    const double attack   = adj_get_value(ad->a)*1000.0;
    const double decay    = adj_get_value(ad->d)*1000.0;
    const double release  = adj_get_value(ad->r)*1000.0;
    const double sustain  = adj_get_value(ad->s);

    const double max_display_ms =  800.0;
    const double adsr_ms = attack + decay + release;
    const double timeline_ms = std::max<double>(max_display_ms, adsr_ms);
    const double sustain_ms = timeline_ms - adsr_ms;
    const double px_per_ms = VP_W / timeline_ms;

    double attack_px  = attack  * px_per_ms;
    double decay_px   = decay   * px_per_ms;
    double release_px = release * px_per_ms;
    double sustain_px = sustain_ms * px_per_ms;

    // scale when needed
    if (sustain_px < 0) {
        sustain_px = 0;
        const double scale = (attack_px + decay_px + release_px) / (double)width_t;
        if (scale > 0) { attack_px /= scale; decay_px /= scale; release_px /= scale; }
    }

    const double y_bottom = B;
    const double y_peak   = T;
    const double y_sustain = B - sustain * VP_H;
    const double x0 = L;

    ad->xA = x0 + attack_px;
    ad->yA = y_peak;
    ad->xD = ad->xA + decay_px;
    ad->yD = y_sustain;
    ad->xS = ad->xD + sustain_px;
    ad->yS = y_sustain;
    ad->xR = ad->xS + release_px;
    ad->yR = y_bottom;

    if (ad->xR > R) ad->xR = R;

    cairo_new_path(w->crb);
    cairo_move_to(w->crb, x0, y_bottom);
    // attack
    cairo_curve_to(w->crb,ad->xA * 0.15, y_bottom,ad->xA * 0.85, ad->yA + 2,ad->xA, ad->yA);
    // Decay
    const double dx1 = ad->xA + (ad->xD - ad->xA) * 0.25;
    const double dx2 = ad->xA + (ad->xD - ad->xA) * 0.75;
    cairo_curve_to(w->crb, dx1, y_peak, dx2, y_sustain, ad->xD, ad->yD);
    // Sustain
    cairo_line_to(w->crb, ad->xS, ad->yS);
    // Release
    const double rx1 = ad->xS + (ad->xR - ad->xS) * 0.25;
    const double rx2 = ad->xS + (ad->xR - ad->xS) * 0.75;
    cairo_curve_to(w->crb, rx1, y_sustain, rx2, ad->yR, ad->xR, ad->yR);
    // close path
    cairo_line_to(w->crb, x0, y_bottom);
    cairo_close_path(w->crb);
    // fill envelope curve
    cairo_set_source_rgba(w->crb, 0.302, 0.714, 0.675, 0.15);
    cairo_fill_preserve(w->crb);
    cairo_set_source_rgba(w->crb, 0.302, 0.714, 0.675, 1.0);
    cairo_set_line_width(w->crb, 1.0);
    cairo_stroke(w->crb);
    cairo_new_path(w->crb);

    // drag handles
    cairo_set_source_rgba(w->crb, 0.894, 0.106, 0.623, 0.8);
    cairo_arc(w->crb,ad->xA,ad->yA,4,0,2*M_PI);
    cairo_fill(w->crb);
    cairo_set_source_rgba(w->crb, 0.902, 0.098, 0.117, 0.8);
    cairo_arc(w->crb,ad->xD,ad->yD,4,0,2*M_PI);
    cairo_fill(w->crb);
    cairo_set_source_rgba(w->crb, 0.377, 0.898, 0.109, 0.8);
    cairo_arc(w->crb,ad->xS,ad->yS,4,0,2*M_PI);
    cairo_fill(w->crb);
    cairo_set_source_rgba(w->crb, 0.486, 0.106, 0.894, 0.8);
    cairo_arc(w->crb,ad->xR,ad->yR,4,0,2*M_PI);
    cairo_fill(w->crb);
}

static void adsr_press(void *w_, void *b_, void *u) {
    Widget_t *w = (Widget_t*)w_;
    ADSRWidget *ad = (ADSRWidget*)w->private_struct;
    XButtonEvent *ev = (XButtonEvent*)b_;

    ad->mx = ev->x - ADSR_PAD;
    ad->drag_A0 = adj_get_value(ad->a);
    ad->drag_D0 = adj_get_value(ad->d);
    ad->drag_R0 = adj_get_value(ad->r);
    ad->drag_S0 = adj_get_value(ad->s);

    if (fabs(ad->mx-ad->xA)<8) ad->drag_part=1;
    else if (fabs(ad->mx-ad->xD)<8) ad->drag_part=2;
    else if (fabs(ad->mx-ad->xS)<8) ad->drag_part=3;
    else if (fabs(ad->mx-ad->xR)<8) ad->drag_part=4;
    else ad->drag_part=0;
}

static double px_to_time(ADSRWidget *ad, double dx, double W) {
    const double total = ad->drag_A0 + ad->drag_D0 + ad->drag_R0 + 0.0001;
    return dx / W * total;
}

static void adsr_motion(void *w_, void *m_, void *u) {
    Widget_t *w = (Widget_t*)w_;
    ADSRWidget *ad = (ADSRWidget*)w->private_struct;
    XMotionEvent *ev = (XMotionEvent*)m_;

    Metrics_t metrics;
    os_get_window_metrics(w, &metrics);
    const double W = metrics.width  - 2*ADSR_PAD;
    const double H = metrics.height - 2*ADSR_PAD;
    const double mx = ev->x - ADSR_PAD;
    const double my = ev->y - ADSR_PAD;
    const double dx = mx - ad->mx;
    const double dt = px_to_time(ad, dx, W);

    if (ad->drag_part == 1) adj_set_value(ad->a, ad->drag_A0 + dt);
    else if (ad->drag_part == 2) adj_set_value(ad->d, ad->drag_D0 + dt);
    else if (ad->drag_part == 4) adj_set_value(ad->r, ad->drag_R0 - dt);
    else if (ad->drag_part == 3) adj_set_state(ad->s, 1.0 - my / H);

    //expose_widget(w);
}

static void adsr_mem_free(void *w_, void *u) {
    free(((Widget_t*)w_)->private_struct);
}

static Widget_t *add_adsr_widget(Widget_t *parent,int x,int y,int w,int h,
            Adjustment_t *a,Adjustment_t *d,Adjustment_t *s,Adjustment_t *r) {

    Widget_t *wid = create_widget(parent->app,parent,x,y,w,h);
    ADSRWidget *ad = (ADSRWidget*)calloc(1,sizeof(ADSRWidget));
    ad->a=a; ad->d=d; ad->s=s; ad->r=r;
    ad->xA = ad->yA = ad->xD = ad->yD = ad->xS = ad->yS = ad->xR = ad->yR = 0.0;
    ad->mx = 0.0;

    wid->private_struct = ad;
    wid->flags |= HAS_MEM;
    wid->func.mem_free_callback = adsr_mem_free;
    wid->func.expose_callback = draw_envelope;
    wid->func.button_press_callback = adsr_press;
    wid->func.motion_callback = adsr_motion;
    return wid;
}
