/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Simon Maes
 *********************************************************************/
#ifndef INCLUDE_PICO_IPFILTER
#define INCLUDE_PICO_IPFILTER

#include "pico_device.h"

enum filter_action {
    FILTER_PRIORITY = 0,
    FILTER_REJECT,
    FILTER_DROP,
    FILTER_COUNT
};

uint32_t pico_ipv4_filter_add(struct pico_device *dev, uint8_t proto,
                              struct pico_ip4 *out_addr, struct pico_ip4 *out_addr_netmask, struct pico_ip4 *in_addr,
                              struct pico_ip4 *in_addr_netmask, uint16_t out_port, uint16_t in_port,
                              int8_t priority, uint8_t tos, enum filter_action action);

int pico_ipv4_filter_del(uint32_t filter_id);

int ipfilter(struct pico_frame *f);

#endif /* _INCLUDE_PICO_IPFILTER */

