/*********************************************************************
   PicoTCP. Copyright (c) 2014-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef _INCLUDE_PICO_ESP8266
#define _INCLUDE_PICO_ESP8266

#include <stdio.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pico_constants.h"

/* -------------- DEBUG ------------- */

/* #define dbg(...) */
#define dbg             printf

/* -------------- MEMORY ------------- */
extern void *pvPortMalloc( size_t xWantedSize );
extern void vPortFree( void *pv );

#define pico_free   vPortFree

static inline void *pico_zalloc(size_t size)
{
    void *ptr = (void *)pvPortMalloc(size);

    if(ptr)
        memset(ptr, 0u, size);

    return ptr;
}

/* -------------- TIME ------------- */

extern volatile uint32_t esp_tick;

static inline pico_time PICO_TIME_MS(void)
{
    return (pico_time) esp_tick;
}

static inline pico_time PICO_TIME(void)
{
    return PICO_TIME_MS() / 1000;
}

static inline void PICO_IDLE(void)
{
    uint32_t now = esp_tick;
    while (now == esp_tick)
        ;
}

#endif
