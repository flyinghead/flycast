/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#define dbg(...) do {} while(0)
/* #define dbg printf */

/*************************/

/*** MACHINE CONFIGURATION ***/
/* Temporary (POSIX) stuff. */
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "pico_mm.h"

extern volatile uint32_t __avr_tick;

#define pico_zalloc(x) calloc(x, 1)
#define pico_free(x) free(x)

static inline unsigned long PICO_TIME(void)
{
    register uint32_t tick = __avr_tick;
    return tick / 1000;
}

static inline unsigned long PICO_TIME_MS(void)
{
    return __avr_tick;
}

static inline void PICO_IDLE(void)
{
    unsigned long tick_now = __avr_tick;
    while(tick_now == __avr_tick) ;
}

