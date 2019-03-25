/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 *********************************************************************/
#define dbg(...) do {} while(0)

/******************/

/*** MACHINE CONFIGURATION ***/
/* Temporary (POSIX) stuff. */
#include <string.h>
#include <unistd.h>

extern volatile uint32_t __str9_tick;

#define pico_native_malloc(x) calloc(x, 1)
#define pico_native_free(x) free(x)

static inline unsigned long PICO_TIME(void)
{
    register uint32_t tick = __str9_tick;
    return tick / 1000;
}

static inline unsigned long PICO_TIME_MS(void)
{
    return __str9_tick;
}

static inline void PICO_IDLE(void)
{
    unsigned long tick_now = __str9_tick;
    while(tick_now == __str9_tick) ;
}

