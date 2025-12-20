
/*
 * RtCheck.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        RtCheck.h  check if real-time scheduling is available 
                        
****************************************************************/

#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <chrono> 
#include <condition_variable>

class RtCheck {
private:
    std::thread _thd;
    std::mutex m;
    std::condition_variable cv;
    bool isRT;
    std::atomic<bool> _execute;

    void run() {
        auto endTime = std::chrono::system_clock::now() + std::chrono::seconds(2);
        _thd = std::thread([this, endTime]() {
            while (_execute.load(std::memory_order_acquire)) {
                std::unique_lock<std::mutex> lk(m);
                cv.wait_until(lk, endTime);
                _execute.store(false, std::memory_order_release);
                break;
            }
        });
    }

public:

    RtCheck() : isRT(false), _execute(true) {}

    ~RtCheck() {}

    void start() {run();}
    void stop() {
        if (_thd.joinable()) {
            cv.notify_one();
            _thd.join();
        }
    }

    bool run_check() {
        isRT = true;
        sched_param sch_params;
        sch_params.sched_priority = 50;
        if (pthread_setschedparam(_thd.native_handle(), SCHED_FIFO, &sch_params)) {
            isRT = false;
        }
        _execute.store(false, std::memory_order_release);
        if (_thd.joinable()) {
            cv.notify_one();
            _thd.join();
        }
        return isRT;
    }

};
