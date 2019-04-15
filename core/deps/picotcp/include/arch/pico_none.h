/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 *********************************************************************/

#ifndef PICO_SUPPORT_ARCHNONE
#define PICO_SUPPORT_ARCHNONE

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#define dbg(...) do {} while(0)
#define pico_zalloc(x) NULL
#define pico_free(x) do {} while(0)
#define PICO_TIME() 666
#define PICO_TIME_MS() 666000
#define PICO_IDLE() do {} while(0)

#endif  /* PICO_SUPPORT_ARCHNONE */

