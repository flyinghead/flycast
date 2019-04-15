/*
 * This is a picoTCP arch file for the DOS 16 bit target using OpenWatcom v1.9
 * Copyright (C) 2015 Mateusz Viste
 *
 * This code is donated to the picoTCP project, and shares the same licensing,
 * that is GNU GPLv2.
 *
 * See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 */

#include <dos.h>   /* provides int86() along with the union REGS type */

#ifndef PICO_SUPPORT_DOS_WATCOM
#define PICO_SUPPORT_DOS_WATCOM

#define dbg(...)

#define pico_zalloc(x) calloc(x, 1)
#define pico_free(x) free(x)

static inline unsigned long PICO_TIME_MS(void)
{
    union REGS regs;
    unsigned long ticks;
    regs.h.ah = 0;          /* get system time (IBM BIOS call) - INT 1A,0 */
    int86(0x1A, &regs, &regs);
    ticks = regs.x.cx;      /* number of ticks since midnight (high word) */
    ticks <<= 16;
    ticks |= regs.x.dx;     /* number of ticks since midnight (low word) */
    return (ticks * 55);    /* a tick is 55ms because the i8253 PIT runs at 18.2 Hz */
}

static inline unsigned long PICO_TIME(void)
{
    return (PICO_TIME_MS() / 1000);
}

static inline void PICO_IDLE(void)
{
    union REGS regs;
    int86(0x28, &regs, &regs); /* DOS 2+ IDLE INTERRUPT */
}

#endif /* PICO_SUPPORT_DOS_WATCOM */
