/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 *********************************************************************/

/*** MACHINE CONFIGURATION ***/
/* Temporary (POSIX) stuff. */
#include <string.h>
#include <unistd.h>

/* Temporary debugging stuff. */
#include <stdarg.h>
#include "halUart.h"
#include <stdio.h>

static void print_uart(char *str)
{
    int i, len;
    len = (int)strlen(str);
    for (i = 0; i < len; i++) {
        HAL_UartWriteByte(str[i]);
        if (HAL_UartTxFull())
            HAL_UartFlush();
    }
}

static inline void sam_dbg(const char *format, ...)
{
    char msg[128] = { 0 };
    va_list args;
    va_start(args, format);
    vsnprintf(msg, 256, format, args);
    va_end(args);
    print_uart(msg);
}

//#define dbg sam_dbg
#define dbg(...) do { } while(0)

extern volatile uint32_t sam_tick;

#define pico_zalloc(x) calloc(x, 1)
#define pico_free(x) free(x)

static inline unsigned long PICO_TIME(void)
{
    register uint32_t tick = sam_tick;
    return tick / 1000;
}

static inline unsigned long PICO_TIME_MS(void)
{
    return sam_tick;
}

static inline void PICO_IDLE(void)
{
    unsigned long tick_now = sam_tick;
    while(tick_now == sam_tick) ;
}

