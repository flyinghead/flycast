/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_ARP
#define INCLUDE_PICO_ARP
#include "pico_eth.h"
#include "pico_device.h"

int pico_arp_receive(struct pico_frame *);


struct pico_eth *pico_arp_get(struct pico_frame *f);
int32_t pico_arp_request(struct pico_device *dev, struct pico_ip4 *dst, uint8_t type);

#define PICO_ARP_STATUS_REACHABLE 0x00
#define PICO_ARP_STATUS_PERMANENT 0x01
#define PICO_ARP_STATUS_STALE     0x02

#define PICO_ARP_QUERY    0x00
#define PICO_ARP_PROBE    0x01
#define PICO_ARP_ANNOUNCE 0x02

#define PICO_ARP_CONFLICT_REASON_CONFLICT 0
#define PICO_ARP_CONFLICT_REASON_PROBE 1

struct pico_eth *pico_arp_lookup(struct pico_ip4 *dst);
struct pico_ip4 *pico_arp_reverse_lookup(struct pico_eth *dst);
int pico_arp_create_entry(uint8_t*hwaddr, struct pico_ip4 ipv4, struct pico_device*dev);
int pico_arp_get_neighbors(struct pico_device *dev, struct pico_ip4 *neighbors, int maxlen);
void pico_arp_register_ipconflict(struct pico_ip4 *ip, struct pico_eth *mac, void (*cb)(int reason));
void pico_arp_postpone(struct pico_frame *f);
void pico_arp_init(void);
#endif
