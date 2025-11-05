/*
 * main.c
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#include <cmath>
#include <vector>
#include <signal.h>
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <string>
#include <condition_variable>

#include "ParallelThread.h"
#include "Loopino_ui.h"

Loopino ui;

#include "jack.cc"

#if defined(__linux__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__OpenBSD__)
// catch signals and exit clean
void
signal_handler (int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGHUP:
        case SIGTERM:
        case SIGQUIT:
            std::cerr << "\nsignal "<< sig <<" received, exiting ...\n"  <<std::endl;
            XLockDisplay(ui.w->app->dpy);
            ui.onExit();
            XFlush(ui.w->app->dpy);
            XUnlockDisplay(ui.w->app->dpy);
        break;
        default:
        break;
    }
}
#endif

int main(int argc, char *argv[]){

    #if defined(__linux__) || defined(__FreeBSD__) || \
        defined(__NetBSD__) || defined(__OpenBSD__)
    if(0 == XInitThreads()) 
        std::cerr << "Warning: XInitThreads() failed\n" << std::endl;
    #endif

    Xputty app;
    std::condition_variable Sync;

    main_init(&app);
    ui.createGUI(&app);

    #if defined(__linux__) || defined(__FreeBSD__) || \
        defined(__NetBSD__) || defined(__OpenBSD__)
    signal (SIGQUIT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGHUP, signal_handler);
    signal (SIGINT, signal_handler);
    #endif

    startJack();

    main_run(&app);

    ui.pa.stop();

    quitJack();

    main_quit(&app);

    printf("bye bye\n");
    return 0;
}

