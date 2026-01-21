
/*
 * Denormals.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


/****************************************************************
        Denormals.h  - set SSE flags for Denormal Protection, when possible
                       reset flags to original when processing is done.
                        
****************************************************************/

#pragma once

#ifdef __SSE__
 #include <immintrin.h>
 #ifndef _IMMINTRIN_H_INCLUDED
  #include <fxsrintrin.h>
 #endif
 #ifdef __SSE3__
  #ifndef _PMMINTRIN_H_INCLUDED
   #include <pmmintrin.h>
  #endif
 #else
  #ifndef _XMMINTRIN_H_INCLUDED
   #include <xmmintrin.h>
  #endif
 #endif //__SSE3__
#endif //__SSE__


class DenormalProtection {
private:
#ifdef __SSE__
    uint32_t  mxcsr_mask;
    uint32_t  mxcsr;
    uint32_t  old_mxcsr;
#endif

public:
    inline void set_() {
#ifdef __SSE__
        old_mxcsr = _mm_getcsr();
        mxcsr = old_mxcsr;
        _mm_setcsr((mxcsr | _MM_DENORMALS_ZERO_MASK | _MM_FLUSH_ZERO_MASK) & mxcsr_mask);
#endif
    };
    inline void reset_() {
#ifdef __SSE__
        _mm_setcsr(old_mxcsr);
#endif
    };

    inline DenormalProtection() {
#ifdef __SSE__
        mxcsr_mask = 0xffbf; // Default MXCSR mask
        mxcsr      = 0;
        uint8_t fxsave[512] __attribute__ ((aligned (16))); // Structure for storing FPU state with FXSAVE command

        memset(fxsave, 0, sizeof(fxsave));
        __builtin_ia32_fxsave(&fxsave);
        uint32_t mask = *(reinterpret_cast<uint32_t *>(&fxsave[0x1c])); // Obtain the MXCSR mask from FXSAVE structure
        if (mask != 0)
            mxcsr_mask = mask;
#endif
    };

    inline ~DenormalProtection() {};
};

