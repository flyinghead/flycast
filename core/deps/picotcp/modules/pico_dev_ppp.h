/*********************************************************************
   PicoTCP. Copyright (c) 2012 TASS Belgium NV. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_PPP
#define INCLUDE_PICO_PPP

#include "pico_config.h"
#include "pico_device.h"

void pico_ppp_destroy(struct pico_device *ppp);
struct pico_device *pico_ppp_create(void);

int pico_ppp_connect(struct pico_device *dev);
int pico_ppp_disconnect(struct pico_device *dev);

int pico_ppp_set_serial_read(struct pico_device *dev, int (*sread)(struct pico_device *, void *, int));
int pico_ppp_set_serial_write(struct pico_device *dev, int (*swrite)(struct pico_device *, const void *, int));
int pico_ppp_set_serial_set_speed(struct pico_device *dev, int (*sspeed)(struct pico_device *, uint32_t));

int pico_ppp_set_apn(struct pico_device *dev, const char *apn);
int pico_ppp_set_username(struct pico_device *dev, const char *username);
int pico_ppp_set_password(struct pico_device *dev, const char *password);

#endif /* INCLUDE_PICO_PPP */
