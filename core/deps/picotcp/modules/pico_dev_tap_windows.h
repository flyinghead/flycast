/*********************************************************************
   PicoTCP. Copyright (c) 2014-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifdef _WIN32
#ifndef INCLUDE_PICO_TAP
#define INCLUDE_PICO_TAP
#include "pico_config.h"
#include "pico_device.h"

/* will look for the first TAP device available, and use it */
struct pico_device *pico_tap_create(char *name, uint8_t *mac);
/* TODO: not implemented yet */
/* void pico_tap_destroy(struct pico_device *null); */
const char *pico_tap_get_guid(struct pico_device *dev);

#endif
#endif

