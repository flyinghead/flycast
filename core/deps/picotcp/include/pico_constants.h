/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_CONST
#define INCLUDE_PICO_CONST
/* Included from pico_config.h */

/** Non-endian dependant constants */
#define PICO_SIZE_IP4    4
#define PICO_SIZE_IP6   16
#define PICO_SIZE_ETH    6
#define PICO_SIZE_TRANS  8
#define PICO_SIZE_IEEE802154_EXT (8u)
#define PICO_SIZE_IEEE802154_SHORT (2u)

/** Endian-dependant constants **/
typedef uint64_t pico_time;
extern volatile uint64_t pico_tick;


/*** *** *** *** *** *** ***
 ***     ARP CONFIG      ***
 *** *** *** *** *** *** ***/

#include "pico_addressing.h"

/* Maximum amount of accepted ARP requests per burst interval */
#define PICO_ARP_MAX_RATE 1
/* Duration of the burst interval in milliseconds */
#define PICO_ARP_INTERVAL 1000

/* Add well-known host numbers here. (bigendian constants only beyond this point) */
#define PICO_IP4_ANY (0x00000000U)
#define PICO_IP4_BCAST (0xffffffffU)

#define PICO_IEEE802154_BCAST (0xffffu)

/* defined in modules/pico_ipv6.c */
#ifdef PICO_SUPPORT_IPV6
extern const uint8_t PICO_IPV6_ANY[PICO_SIZE_IP6];
#endif

static inline uint32_t pico_hash(const void *buf, uint32_t size)
{
    uint32_t hash = 5381;
    uint32_t i;
    const uint8_t *ptr = (const uint8_t *)buf;
    for(i = 0; i < size; i++)
        hash = ((hash << 5) + hash) + ptr[i]; /* hash * 33 + char */
    return hash;
}

/* Debug */
/* #define PICO_SUPPORT_DEBUG_MEMORY */
/* #define PICO_SUPPORT_DEBUG_TOOLS */
#endif
