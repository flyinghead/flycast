/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

 *********************************************************************/
#ifndef _INCLUDE_PICO_ICMP6
#define _INCLUDE_PICO_ICMP6
#include "pico_addressing.h"
#include "pico_protocol.h"
#include "pico_mld.h"
/* ICMP header sizes */
#define PICO_ICMP6HDR_DRY_SIZE          4
#define PICO_ICMP6HDR_ECHO_REQUEST_SIZE 8
#define PICO_ICMP6HDR_DEST_UNREACH_SIZE 8
#define PICO_ICMP6HDR_TIME_XCEEDED_SIZE 8
#define PICO_ICMP6HDR_PARAM_PROBLEM_SIZE 8
#define PICO_ICMP6HDR_NEIGH_SOL_SIZE    24
#define PICO_ICMP6HDR_NEIGH_ADV_SIZE    24
#define PICO_ICMP6HDR_ROUTER_SOL_SIZE   8
#define PICO_ICMP6HDR_ROUTER_SOL_SIZE_6LP 16
#define PICO_ICMP6HDR_ROUTER_ADV_SIZE   16
#define PICO_ICMP6HDR_REDIRECT_SIZE     40

/* ICMP types */
#define PICO_ICMP6_DEST_UNREACH        1
#define PICO_ICMP6_PKT_TOO_BIG         2
#define PICO_ICMP6_TIME_EXCEEDED       3
#define PICO_ICMP6_PARAM_PROBLEM       4
#define PICO_ICMP6_ECHO_REQUEST        128
#define PICO_ICMP6_ECHO_REPLY          129
#define PICO_ICMP6_ROUTER_SOL          133
#define PICO_ICMP6_ROUTER_ADV          134
#define PICO_ICMP6_NEIGH_SOL           135
#define PICO_ICMP6_NEIGH_ADV           136
#define PICO_ICMP6_REDIRECT            137

/* destination unreachable codes */
#define PICO_ICMP6_UNREACH_NOROUTE     0
#define PICO_ICMP6_UNREACH_ADMIN       1
#define PICO_ICMP6_UNREACH_SRCSCOPE    2
#define PICO_ICMP6_UNREACH_ADDR        3
#define PICO_ICMP6_UNREACH_PORT        4
#define PICO_ICMP6_UNREACH_SRCFILTER   5
#define PICO_ICMP6_UNREACH_REJROUTE    6

/* time exceeded codes */
#define PICO_ICMP6_TIMXCEED_INTRANS    0
#define PICO_ICMP6_TIMXCEED_REASS      1

/* parameter problem codes */
#define PICO_ICMP6_PARAMPROB_HDRFIELD  0
#define PICO_ICMP6_PARAMPROB_NXTHDR    1
#define PICO_ICMP6_PARAMPROB_IPV6OPT   2

/* ping error codes */
#define PICO_PING6_ERR_REPLIED         0
#define PICO_PING6_ERR_TIMEOUT         1
#define PICO_PING6_ERR_UNREACH         2
#define PICO_PING6_ERR_ABORTED         3
#define PICO_PING6_ERR_PENDING         0xFFFF

/* ND configuration */
#define PICO_ND_MAX_FRAMES_QUEUED      4 /* max frames queued while awaiting address resolution */

/* ND RFC constants */
#define PICO_ND_MAX_SOLICIT            3
#define PICO_ND_MAX_NEIGHBOR_ADVERT    3
#define PICO_ND_DELAY_INCOMPLETE       1000 /* msec */
#define PICO_ND_DELAY_FIRST_PROBE_TIME 5000 /* msec */

/* neighbor discovery options */
#define PICO_ND_OPT_LLADDR_SRC         1
#define PICO_ND_OPT_LLADDR_TGT         2
#define PICO_ND_OPT_PREFIX             3
#define PICO_ND_OPT_REDIRECT           4
#define PICO_ND_OPT_MTU                5
#define PICO_ND_OPT_RDNSS             25 /* RFC 5006 */
#define PICO_ND_OPT_ARO               33 /* RFC 6775 */
#define PICO_ND_OPT_6CO               34 /* RFC 6775 */
#define PICO_ND_OPT_ABRO              35 /* RFC 6775 */

/* ND advertisement flags */
#define PICO_ND_ROUTER             0x80000000
#define PICO_ND_SOLICITED          0x40000000
#define PICO_ND_OVERRIDE           0x20000000
#define IS_ROUTER(x) (long_be(x->msg.info.neigh_adv.rsor) & (PICO_ND_ROUTER))           /* router flag set? */
#define IS_SOLICITED(x) (long_be(x->msg.info.neigh_adv.rsor) & (PICO_ND_SOLICITED))     /* solicited flag set? */
#define IS_OVERRIDE(x) (long_be(x->msg.info.neigh_adv.rsor) & (PICO_ND_OVERRIDE))   /* override flag set? */

#define PICO_ND_PREFIX_LIFETIME_INF    0xFFFFFFFFu
/* #define PICO_ND_DESTINATION_LRU_TIME   600000u / * msecs (10min) * / */

/* custom defines */
#define PICO_ICMP6_ND_UNICAST          0
#define PICO_ICMP6_ND_ANYCAST          1
#define PICO_ICMP6_ND_SOLICITED        2
#define PICO_ICMP6_ND_DAD              3
#define PICO_ICMP6_ND_DEREGISTER       4

#define PICO_ICMP6_MAX_RTR_SOL_DELAY   1000

#define PICO_ICMP6_OPT_LLADDR_SIZE (8)

/******************************************************************************
 *  6LoWPAN Constants
 ******************************************************************************/

/* Address registration lifetime */
#define PICO_6LP_ND_DEFAULT_LIFETIME    (120) /* TWO HOURS */

extern struct pico_protocol pico_proto_icmp6;

PACKED_STRUCT_DEF pico_icmp6_hdr {
    uint8_t type;
    uint8_t code;
    uint16_t crc;

    PACKED_UNION_DEF icmp6_msg_u {
        /* error messages */
        PACKED_UNION_DEF icmp6_err_u {
            PEDANTIC_STRUCT_DEF dest_unreach_s {
                uint32_t unused;
            } dest_unreach;
            PEDANTIC_STRUCT_DEF pkt_too_big_s {
                uint32_t mtu;
            } pkt_too_big;
            PEDANTIC_STRUCT_DEF time_exceeded_s {
                uint32_t unused;
            } time_exceeded;
            PEDANTIC_STRUCT_DEF param_problem_s {
                uint32_t ptr;
            } param_problem;
        } err;

        /* informational messages */
        PACKED_UNION_DEF icmp6_info_u {
            PEDANTIC_STRUCT_DEF echo_request_s {
                uint16_t id;
                uint16_t seq;
            } echo_request;
            PEDANTIC_STRUCT_DEF echo_reply_s {
                uint16_t id;
                uint16_t seq;
            } echo_reply;
            PEDANTIC_STRUCT_DEF router_sol_s {
                uint32_t unused;
            } router_sol;
            PEDANTIC_STRUCT_DEF router_adv_s {
                uint8_t hop;
                uint8_t mor;
                uint16_t life_time;
                uint32_t reachable_time;
                uint32_t retrans_time;
            } router_adv;
            PEDANTIC_STRUCT_DEF neigh_sol_s {
                uint32_t unused;
                struct pico_ip6 target;
            } neigh_sol;
            PEDANTIC_STRUCT_DEF neigh_adv_s {
                uint32_t rsor;
                struct pico_ip6 target;
            } neigh_adv;
            PEDANTIC_STRUCT_DEF redirect_s {
                uint32_t reserved;
                struct pico_ip6 target;
                struct pico_ip6 dest;
            } redirect;
            PEDANTIC_STRUCT_DEF mld_s {
                uint16_t max_resp_time;
                uint16_t reserved;
                struct pico_ip6 mmcast_group;
                /*MLDv2*/
                uint8_t reserverd; /* With S and QRV */
                uint8_t QQIC;
                uint16_t nbr_src;
                struct pico_ip6 src[1];
            } mld;
            /* 6LoWPAN Duplicate Address Message */
            PEDANTIC_STRUCT_DEF da_s {
                uint8_t status;
                uint8_t reserved;
                uint16_t lifetime;
                struct pico_6lowpan_ext eui64;
                struct pico_ip6 addr;
            } da;
        } info;
    } msg;
};

PACKED_UNION_DEF pico_hw_addr {
    struct pico_eth mac;
#ifdef PICO_SUPPORT_6LOWPAN
    union pico_6lowpan_u pan;
#endif /* PICO_SUPPORT_6LOWPAN */
    uint8_t data[8];
};

/******************************************************************************
 *  ICMP6 Neighbor Discovery Options
 ******************************************************************************/

PACKED_STRUCT_DEF pico_icmp6_opt_lladdr
{
    uint8_t type;
    uint8_t len;
    union pico_hw_addr addr;
};

PACKED_STRUCT_DEF pico_icmp6_opt_prefix
{
    uint8_t type;
    uint8_t len;
    uint8_t prefix_len;
    uint8_t res : 6;
    uint8_t aac : 1;
    uint8_t onlink : 1;
    uint32_t val_lifetime;
    uint32_t pref_lifetime;
    uint32_t reserved;
    struct pico_ip6 prefix;
};

PACKED_STRUCT_DEF pico_icmp6_opt_mtu
{
    uint8_t type;
    uint8_t len;
    uint16_t res;
    uint32_t mtu;
};

PACKED_STRUCT_DEF pico_icmp6_opt_redirect
{
    uint8_t type;
    uint8_t len;
    uint16_t res0;
    uint32_t res1;
};

PACKED_STRUCT_DEF pico_icmp6_opt_rdnss
{
    uint8_t type;
    uint8_t len;
    uint16_t res0;
    uint32_t lifetime;
    struct pico_ip6 *addr;
};

PACKED_STRUCT_DEF pico_icmp6_opt_na
{
    uint8_t type;
    uint8_t len;
};

/* 6LoWPAN Address Registration Option (ARO) */
PACKED_STRUCT_DEF pico_icmp6_opt_aro
{
    uint8_t type;
    uint8_t len;
    uint8_t status;
    uint8_t res0;
    uint16_t res1;
    uint16_t lifetime;
    struct pico_6lowpan_ext eui64;
};

#define ICMP6_ARO_SUCCES    (0u)
#define ICMP6_ARO_DUP       (1u)
#define ICMP6_ARO_FULL      (2u)

/* 6LoWPAN Context Option (6CO) */
PACKED_STRUCT_DEF pico_icmp6_opt_6co
{
    uint8_t type;
    uint8_t len;
    uint8_t clen;
    uint8_t id: 4;
    uint8_t res: 3;
    uint8_t c: 1;
    uint16_t lifetime;
    uint8_t prefix;
};

/* 6LoWPAN Authoritative Border Router Option (ABRO) */
PACKED_STRUCT_DEF pico_icmp6_opt_abro
{
    uint8_t type;
    uint8_t len;
    uint16_t version_low;
    uint16_t version_high;
    uint16_t lifetime;
    struct pico_ip6 addr;
};

struct pico_icmp6_stats
{
    unsigned long size;
    unsigned long seq;
    pico_time time;
    unsigned long ttl;
    int err;
    struct pico_ip6 dst;
};

int pico_icmp6_ping(char *dst, int count, int interval, int timeout, int size, void (*cb)(struct pico_icmp6_stats *), struct pico_device *dev);
int pico_icmp6_ping_abort(int id);


int pico_icmp6_neighbor_solicitation(struct pico_device *dev, struct pico_ip6 *tgt, uint8_t type, struct pico_ip6 *dst);
int pico_icmp6_neighbor_advertisement(struct pico_frame *f, struct pico_ip6 *target);
int pico_icmp6_router_solicitation(struct pico_device *dev, struct pico_ip6 *src, struct pico_ip6 *dst);

int pico_icmp6_port_unreachable(struct pico_frame *f);
int pico_icmp6_proto_unreachable(struct pico_frame *f);
int pico_icmp6_dest_unreachable(struct pico_frame *f);
int pico_icmp6_ttl_expired(struct pico_frame *f);
int pico_icmp6_packet_filtered(struct pico_frame *f);
int pico_icmp6_parameter_problem(struct pico_frame *f, uint8_t problem, uint32_t ptr);
int pico_icmp6_pkt_too_big(struct pico_frame *f);
int pico_icmp6_frag_expired(struct pico_frame *f);

uint16_t pico_icmp6_checksum(struct pico_frame *f);
int pico_icmp6_router_advertisement(struct pico_device *dev, struct pico_ip6 *dst);

#endif
