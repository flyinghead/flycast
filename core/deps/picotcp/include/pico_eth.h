/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_ETH
#define INCLUDE_PICO_ETH
#include "pico_addressing.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"


PACKED_STRUCT_DEF pico_eth_hdr {
    uint8_t daddr[6];
    uint8_t saddr[6];
    uint16_t proto;
};

#define PICO_SIZE_ETHHDR 14

#endif
