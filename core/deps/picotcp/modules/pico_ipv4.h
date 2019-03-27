/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

 *********************************************************************/
#ifndef INCLUDE_PICO_IPV4
#define INCLUDE_PICO_IPV4
#include "pico_addressing.h"
#include "pico_protocol.h"
#include "pico_tree.h"

#define PICO_IPV4_INADDR_ANY 0x00000000U

#define PICO_IPV4_MTU (1500u)
#define PICO_SIZE_IP4HDR (uint32_t)((sizeof(struct pico_ipv4_hdr)))
#define PICO_IPV4_MAXPAYLOAD (PICO_IPV4_MTU - PICO_SIZE_IP4HDR)
#define PICO_IPV4_DONTFRAG 0x4000U
#define PICO_IPV4_MOREFRAG 0x2000U
#define PICO_IPV4_EVIL      0x8000U
#define PICO_IPV4_FRAG_MASK 0x1FFFU
#define PICO_IPV4_DEFAULT_TTL 64
#ifndef MBED
    #define PICO_IPV4_FRAG_MAX_SIZE (uint32_t)(63 * 1024)
#else
    #define PICO_IPV4_FRAG_MAX_SIZE PICO_DEFAULT_SOCKETQ
#endif

extern struct pico_protocol pico_proto_ipv4;

PACKED_STRUCT_DEF pico_ipv4_hdr {
    uint8_t vhl;
    uint8_t tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag;
    uint8_t ttl;
    uint8_t proto;
    uint16_t crc;
    struct pico_ip4 src;
    struct pico_ip4 dst;
    uint8_t options[];
};

PACKED_STRUCT_DEF pico_ipv4_pseudo_hdr
{
    struct pico_ip4 src;
    struct pico_ip4 dst;
    uint8_t zeros;
    uint8_t proto;
    uint16_t len;
};

/* Interface: link to device */
struct pico_mcast_list;

struct pico_ipv4_link
{
    struct pico_device *dev;
    struct pico_ip4 address;
    struct pico_ip4 netmask;
#ifdef PICO_SUPPORT_MCAST
    struct pico_tree *MCASTGroups;
    uint8_t mcast_compatibility;
    uint8_t mcast_last_query_interval;
#endif
};


struct pico_ipv4_route
{
    struct pico_ip4 dest;
    struct pico_ip4 netmask;
    struct pico_ip4 gateway;
    struct pico_ipv4_link *link;
    uint32_t metric;
};

extern struct pico_tree Routes;


int pico_ipv4_compare(struct pico_ip4 *a, struct pico_ip4 *b);
int pico_ipv4_to_string(char *ipbuf, const uint32_t ip);
int pico_string_to_ipv4(const char *ipstr, uint32_t *ip);
int pico_ipv4_valid_netmask(uint32_t mask);
int pico_ipv4_is_unicast(uint32_t address);
int pico_ipv4_is_multicast(uint32_t address);
int pico_ipv4_is_broadcast(uint32_t addr);
int pico_ipv4_is_loopback(uint32_t addr);
int pico_ipv4_is_valid_src(uint32_t addr, struct pico_device *dev);

int pico_ipv4_link_add(struct pico_device *dev, struct pico_ip4 address, struct pico_ip4 netmask);
int pico_ipv4_link_del(struct pico_device *dev, struct pico_ip4 address);
int pico_ipv4_rebound(struct pico_frame *f);

int pico_ipv4_frame_push(struct pico_frame *f, struct pico_ip4 *dst, uint8_t proto);
struct pico_ipv4_link *pico_ipv4_link_get(struct pico_ip4 *address);
struct pico_ipv4_link *pico_ipv4_link_by_dev(struct pico_device *dev);
struct pico_ipv4_link *pico_ipv4_link_by_dev_next(struct pico_device *dev, struct pico_ipv4_link *last);
struct pico_device *pico_ipv4_link_find(struct pico_ip4 *address);
struct pico_ip4 *pico_ipv4_source_find(const struct pico_ip4 *dst);
struct pico_device *pico_ipv4_source_dev_find(const struct pico_ip4 *dst);
int pico_ipv4_route_add(struct pico_ip4 address, struct pico_ip4 netmask, struct pico_ip4 gateway, int metric, struct pico_ipv4_link *link);
int pico_ipv4_route_del(struct pico_ip4 address, struct pico_ip4 netmask, int metric);
struct pico_ip4 pico_ipv4_route_get_gateway(struct pico_ip4 *addr);
void pico_ipv4_route_set_bcast_link(struct pico_ipv4_link *link);
void pico_ipv4_unreachable(struct pico_frame *f, int err);

int pico_ipv4_mcast_join(struct pico_ip4 *mcast_link, struct pico_ip4 *mcast_group, uint8_t reference_count, uint8_t filter_mode, struct pico_tree *MCASTFilter);
int pico_ipv4_mcast_leave(struct pico_ip4 *mcast_link, struct pico_ip4 *mcast_group, uint8_t reference_count, uint8_t filter_mode, struct pico_tree *MCASTFilter);
struct pico_ipv4_link *pico_ipv4_get_default_mcastlink(void);
int pico_ipv4_cleanup_links(struct pico_device *dev);

#endif /* _INCLUDE_PICO_IPV4 */
