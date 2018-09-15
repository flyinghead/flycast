/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_TUN
#define INCLUDE_PICO_TUN
#include "pico_config.h"
#include "pico_device.h"

void pico_tun_destroy(struct pico_device *tun);
struct pico_device *pico_tun_create(char *name);

#endif

