
/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef _INCLUDE_PICO_PIC32
#define _INCLUDE_PICO_PIC32

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pico_constants.h"

/* monotonically increasing tick,
 * typically incremented every millisecond in a systick interrupt */
extern volatile unsigned int pico_ms_tick;

#ifdef PIC32_NO_PRINTF
#define dbg(...) do {} while(0)
#else
#define dbg printf
#endif

/* Use plain C-lib malloc and free */
#define pico_free(x) free(x)

static inline void *pico_zalloc(size_t size)
{
    void *ptr = malloc(size);
    if(ptr)
        memset(ptr, 0u, size);

    return ptr;
}

static inline pico_time PICO_TIME_MS(void)
{
    return (pico_time)pico_ms_tick;
}

static inline pico_time PICO_TIME(void)
{
    return (pico_time)(PICO_TIME_MS() / 1000);
}

static inline void PICO_IDLE(void)
{
    unsigned int now = pico_ms_tick;
    while(now == pico_ms_tick) ;
}

#endif  /* PICO_PIC32 */

