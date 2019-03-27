/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 *********************************************************************/
#ifndef PICO_SUPPORT_PIC24
#define PICO_SUPPORT_PIC24
#define dbg printf
/* #define dbg(...) */

/*************************/

/*** MACHINE CONFIGURATION ***/
#include <stdio.h>
#include <stdint.h>

/* #include "phalox_development_board.h" */

#ifndef __PIC24F__
#define __PIC24F__
#endif

/*
   #ifndef __PIC24FJ256GA106__
   #define __PIC24FJ256GA106__
   #endif
 */

#ifndef PICO_MAX_SOCKET_FRAMES
#define PICO_MAX_SOCKET_FRAMES 16
#endif

/* Device header file */

#if defined(__PIC24E__)
# include <p24Exxxx.h>
#elif defined(__PIC24F__)
# include <p24Fxxxx.h>
#elif defined(__PIC24H__)
# include <p24Hxxxx.h>
#endif


#define TIMBASE_INT_E         IEC0bits.T2IE

#ifdef PICO_SUPPORT_DEBUG_MEMORY
static inline void *pico_zalloc(int len)
{
    /* dbg("%s: Alloc object of len %d, caller: %p\n", __FUNCTION__, len, __builtin_return_address(0)); */
    return calloc(len, 1);
}

static inline void pico_free(void *tgt)
{
    /* dbg("%s: Discarded object @%p, caller: %p\n", __FUNCTION__, tgt, __builtin_return_address(0)); */
    free(tgt);
}
#else
# define pico_zalloc(x) calloc(x, 1)
# define pico_free(x) free(x)
#endif

extern void *pvPortMalloc( size_t xWantedSize );
extern volatile pico_time __pic24_tick;

static inline unsigned long PICO_TIME(void)
{
    unsigned long tick;
    /* Disable timer interrupts */
    TIMBASE_INT_E = 0;
    tick = __pic24_tick;
    /* Enable timer interrupts */
    TIMBASE_INT_E = 1;
    return tick / 1000;
}

static inline unsigned long PICO_TIME_MS(void)
{
    unsigned long tick;
    /* Disable timer interrupts */
    TIMBASE_INT_E = 0;
    tick = __pic24_tick;
    /* Enable timer interrupts */
    TIMBASE_INT_E = 1;
    return tick;
}

static inline void PICO_IDLE(void)
{
    unsigned long tick_now;
    /* Disable timer interrupts */
    TIMBASE_INT_E = 0;
    tick_now = (unsigned long)pico_tick;
    /* Enable timer interrupts */
    TIMBASE_INT_E = 1;
    /* Doesn't matter that this call isn't interrupt safe, */
    /* we just check for the value to change */
    while(tick_now == __pic24_tick) ;
}

#endif
