/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

 *********************************************************************/
#ifndef _INCLUDE_PICO_IPV6
#define _INCLUDE_PICO_IPV6
#include "pico_addressing.h"
#include "pico_protocol.h"
#include "pico_ipv4.h"

#define PICO_SIZE_IP6HDR ((uint32_t)(sizeof(struct pico_ipv6_hdr)))
#define PICO_IPV6_DEFAULT_HOP 64
#define PICO_IPV6_MIN_MTU 1280
#define PICO_IPV6_STRING 46

#define PICO_IPV6_EXTHDR_HOPBYHOP 0
#define PICO_IPV6_EXTHDR_ROUTING 43
#define PICO_IPV6_EXTHDR_FRAG 44
#define PICO_IPV6_EXTHDR_ESP 50
#define PICO_IPV6_EXTHDR_AUTH 51
#define PICO_IPV6_EXTHDR_NONE 59
#define PICO_IPV6_EXTHDR_DESTOPT 60

#define PICO_IPV6_EXTHDR_OPT_ROUTER_ALERT 5
#define PICO_IPV6_EXTHDR_OPT_ROUTER_ALERT_DATALEN 2

#define HBH_LEN(hbh) ((((hbh->ext.hopbyhop.len + 1) << 3) - 2)) /* len in bytes, minus nxthdr and len byte */
#define IPV6_OPTLEN(x) ((uint16_t)(((x + 1) << 3)))

extern const uint8_t PICO_IP6_ANY[PICO_SIZE_IP6];
extern struct pico_protocol pico_proto_ipv6;
extern struct pico_tree IPV6Routes;

PACKED_STRUCT_DEF pico_ipv6_hdr {
    uint32_t vtf;
    uint16_t len;
    uint8_t nxthdr;
    uint8_t hop;
    struct pico_ip6 src;
    struct pico_ip6 dst;
};

PACKED_STRUCT_DEF pico_ipv6_pseudo_hdr
{
    struct pico_ip6 src;
    struct pico_ip6 dst;
    uint32_t len;
    uint8_t zero[3];
    uint8_t nxthdr;
};

struct pico_ipv6_link
{
    struct pico_device *dev;
    struct pico_ip6 address;
    struct pico_ip6 netmask;
    uint8_t istentative : 1;
    uint8_t isduplicate : 1;
    uint32_t dad_timer;
    uint16_t dup_detect_retrans;
    uint8_t retrans;
    pico_time expire_time;
#ifdef PICO_SUPPORT_MCAST
    struct pico_tree *MCASTGroups;
    uint8_t mcast_compatibility;
    uint8_t mcast_last_query_interval;
#endif
};

union pico_link {
    struct pico_ipv4_link ipv4;
    struct pico_ipv6_link ipv6;
};

struct pico_ipv6_hbhoption {
    uint8_t type;
    uint8_t len;
};
#ifdef PICO_SUPPORT_MCAST
struct pico_ipv6_mcast_group {
    uint8_t filter_mode;
    uint16_t reference_count;
    struct pico_ip6 mcast_addr;
    struct pico_tree MCASTSources;
};
#endif
struct pico_ipv6_destoption {
    uint8_t type;
    uint8_t len;
};

struct pico_ipv6_route
{
    struct pico_ip6 dest;
    struct pico_ip6 netmask;
    struct pico_ip6 gateway;
    pico_time backoff;
    uint8_t retrans;
    struct pico_ipv6_link *link;
    uint32_t metric;
};

PACKED_STRUCT_DEF pico_ipv6_exthdr {
    uint8_t nxthdr;

    PACKED_UNION_DEF ipv6_ext_u {
        PEDANTIC_STRUCT_DEF hopbyhop_s {
            uint8_t len;
        } hopbyhop;

        PEDANTIC_STRUCT_DEF destopt_s {
            uint8_t len;
        } destopt;

        PEDANTIC_STRUCT_DEF routing_s {
            uint8_t len;
            uint8_t routtype;
            uint8_t segleft;
        } routing;

        PEDANTIC_STRUCT_DEF fragmentation_s {
            uint8_t res;
            uint8_t om[2];
            uint8_t id[4];
        } frag;
    } ext;
};

int pico_ipv6_compare(struct pico_ip6 *a, struct pico_ip6 *b);
int pico_string_to_ipv6(const char *ipstr, uint8_t *ip);
int pico_ipv6_to_string(char *ipbuf, const uint8_t ip[PICO_SIZE_IP6]);
int pico_ipv6_is_unicast(struct pico_ip6 *a);
int pico_ipv6_is_multicast(const uint8_t addr[PICO_SIZE_IP6]);
int pico_ipv6_is_allhosts_multicast(const uint8_t addr[PICO_SIZE_IP6]);
int pico_ipv6_is_solnode_multicast(const uint8_t addr[PICO_SIZE_IP6], struct pico_device *dev);
int pico_ipv6_is_global(const uint8_t addr[PICO_SIZE_IP6]);
int pico_ipv6_is_uniquelocal(const uint8_t addr[PICO_SIZE_IP6]);
int pico_ipv6_is_sitelocal(const uint8_t addr[PICO_SIZE_IP6]);
int pico_ipv6_is_linklocal(const uint8_t addr[PICO_SIZE_IP6]);
int pico_ipv6_is_solicited(const uint8_t addr[PICO_SIZE_IP6]);
int pico_ipv6_is_unspecified(const uint8_t addr[PICO_SIZE_IP6]);
int pico_ipv6_is_localhost(const uint8_t addr[PICO_SIZE_IP6]);

int pico_ipv6_frame_push(struct pico_frame *f, struct pico_ip6 *src, struct pico_ip6 *dst, uint8_t proto, int is_dad);
int pico_ipv6_route_add(struct pico_ip6 address, struct pico_ip6 netmask, struct pico_ip6 gateway, int metric, struct pico_ipv6_link *link);
int pico_ipv6_route_del(struct pico_ip6 address, struct pico_ip6 netmask, struct pico_ip6 gateway, int metric, struct pico_ipv6_link *link);
void pico_ipv6_unreachable(struct pico_frame *f, uint8_t code);

struct pico_ipv6_link *pico_ipv6_link_add(struct pico_device *dev, struct pico_ip6 address, struct pico_ip6 netmask);
struct pico_ipv6_link *pico_ipv6_link_add_no_dad(struct pico_device *dev, struct pico_ip6 address, struct pico_ip6 netmask);
int pico_ipv6_link_del(struct pico_device *dev, struct pico_ip6 address);
int pico_ipv6_cleanup_links(struct pico_device *dev);
struct pico_ipv6_link *pico_ipv6_link_istentative(struct pico_ip6 *address);
struct pico_ipv6_link *pico_ipv6_link_get(struct pico_ip6 *address);
struct pico_device *pico_ipv6_link_find(struct pico_ip6 *address);
struct pico_ip6 pico_ipv6_route_get_gateway(struct pico_ip6 *addr);
struct pico_ip6 *pico_ipv6_source_find(const struct pico_ip6 *dst);
struct pico_device *pico_ipv6_source_dev_find(const struct pico_ip6 *dst);
struct pico_ipv6_link *pico_ipv6_link_by_dev(struct pico_device *dev);
struct pico_ipv6_link *pico_ipv6_link_by_dev_next(struct pico_device *dev, struct pico_ipv6_link *last);
struct pico_ipv6_link *pico_ipv6_global_get(struct pico_device *dev);
struct pico_ipv6_link *pico_ipv6_linklocal_get(struct pico_device *dev);
struct pico_ipv6_link *pico_ipv6_sitelocal_get(struct pico_device *dev);
struct pico_ipv6_link *pico_ipv6_prefix_configured(struct pico_ip6 *prefix);
struct pico_ipv6_route *pico_ipv6_gateway_by_dev(struct pico_device *dev);
struct pico_ipv6_route *pico_ipv6_gateway_by_dev_next(struct pico_device *dev, struct pico_ipv6_route *last);
int pico_ipv6_lifetime_set(struct pico_ipv6_link *l, pico_time expire);
void pico_ipv6_check_lifetime_expired(pico_time now, void *arg);
int pico_ipv6_dev_routing_enable(struct pico_device *dev);
int pico_ipv6_dev_routing_disable(struct pico_device *dev);
void pico_ipv6_router_down(struct pico_ip6 *address);

int pico_ipv6_mcast_join(struct pico_ip6 *mcast_link, struct pico_ip6 *mcast_group, uint8_t reference_count, uint8_t filter_mode, struct pico_tree *_MCASTFilter);
int pico_ipv6_mcast_leave(struct pico_ip6 *mcast_link, struct pico_ip6 *mcast_group, uint8_t reference_count, uint8_t filter_mode, struct pico_tree *_MCASTFilter);

struct pico_ipv6_link *pico_ipv6_get_default_mcastlink(void);

int pico_ipv6_is_null_address(struct pico_ip6 *ip6);
#endif
