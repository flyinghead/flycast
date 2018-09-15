/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef _INCLUDE_PICO_ND
#define _INCLUDE_PICO_ND
#include "pico_frame.h"
#include "pico_ipv6.h"

/* RFC constants */
#define PICO_ND_REACHABLE_TIME         30000 /* msec */
#define PICO_ND_RETRANS_TIMER          1000 /* msec */

struct pico_nd_hostvars {
    uint8_t routing;
    uint8_t hoplimit;
    pico_time basetime;
    pico_time reachabletime;
    pico_time retranstime;
#ifdef PICO_SUPPORT_6LOWPAN
    uint8_t lowpan_flags;
#endif
};

void pico_ipv6_nd_init(void);
struct pico_eth *pico_ipv6_get_neighbor(struct pico_frame *f);
void pico_ipv6_nd_postpone(struct pico_frame *f);
int pico_ipv6_nd_recv(struct pico_frame *f);

#ifdef PICO_SUPPORT_6LOWPAN
int pico_6lp_nd_start_soliciting(struct pico_ipv6_link *l, struct pico_ipv6_route *gw);
void pico_6lp_nd_register(struct pico_ipv6_link *link);
#endif

#endif
