/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Authors: Daniele Lacamera
 *********************************************************************/


#include "pico_config.h"
#include "pico_frame.h"
#include "pico_device.h"
#include "pico_protocol.h"
#include "pico_stack.h"
#include "pico_addressing.h"
#include "pico_dns_client.h"

#include "pico_6lowpan_ll.h"
#include "pico_ethernet.h"
#include "pico_6lowpan.h"
#include "pico_olsr.h"
#include "pico_aodv.h"
#include "pico_eth.h"
#include "pico_arp.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_icmp4.h"
#include "pico_icmp6.h"
#include "pico_igmp.h"
#include "pico_udp.h"
#include "pico_tcp.h"
#include "pico_socket.h"
#include "heap.h"

/* Mockables */
#if defined UNIT_TEST
#   define MOCKABLE __attribute__((weak))
#else
#   define MOCKABLE
#endif


volatile pico_time pico_tick;
volatile pico_err_t pico_err;

static uint32_t _rand_seed;

void WEAK pico_rand_feed(uint32_t feed)
{
    if (!feed)
        return;

    _rand_seed *= 1664525;
    _rand_seed += 1013904223;
    _rand_seed ^= ~(feed);
}

uint32_t WEAK pico_rand(void)
{
    pico_rand_feed((uint32_t)pico_tick);
    return _rand_seed;
}

void pico_to_lowercase(char *str)
{
    int i = 0;
    if (!str)
        return;

    while(str[i]) {
        if ((str[i] <= 'Z') && (str[i] >= 'A'))
            str[i] = (char) (str[i] - (char)('A' - 'a'));

        i++;
    }
}

/* NOTIFICATIONS: distributed notifications for stack internal errors.
 */

int pico_notify_socket_unreachable(struct pico_frame *f)
{
    if (0) {}

#ifdef PICO_SUPPORT_ICMP4
    else if (IS_IPV4(f)) {
        pico_icmp4_port_unreachable(f);
    }
#endif
#ifdef PICO_SUPPORT_ICMP6
    else if (IS_IPV6(f)) {
        pico_icmp6_port_unreachable(f);
    }
#endif

    return 0;
}

int pico_notify_proto_unreachable(struct pico_frame *f)
{
    if (0) {}

#ifdef PICO_SUPPORT_ICMP4
    else if (IS_IPV4(f)) {
        pico_icmp4_proto_unreachable(f);
    }
#endif
#ifdef PICO_SUPPORT_ICMP6
    else if (IS_IPV6(f)) {
        pico_icmp6_proto_unreachable(f);
    }
#endif
    return 0;
}

int pico_notify_dest_unreachable(struct pico_frame *f)
{
    if (0) {}

#ifdef PICO_SUPPORT_ICMP4
    else if (IS_IPV4(f)) {
        pico_icmp4_dest_unreachable(f);
    }
#endif
#ifdef PICO_SUPPORT_ICMP6
    else if (IS_IPV6(f)) {
        pico_icmp6_dest_unreachable(f);
    }
#endif
    return 0;
}

int pico_notify_ttl_expired(struct pico_frame *f)
{
    if (0) {}

#ifdef PICO_SUPPORT_ICMP4
    else if (IS_IPV4(f)) {
        pico_icmp4_ttl_expired(f);
    }
#endif
#ifdef PICO_SUPPORT_ICMP6
    else if (IS_IPV6(f)) {
        pico_icmp6_ttl_expired(f);
    }
#endif
    return 0;
}

int pico_notify_frag_expired(struct pico_frame *f)
{
    if (0) {}

#ifdef PICO_SUPPORT_ICMP4
    else if (IS_IPV4(f)) {
        pico_icmp4_frag_expired(f);
    }
#endif
#ifdef PICO_SUPPORT_ICMP6
    else if (IS_IPV6(f)) {
        pico_icmp6_frag_expired(f);
    }
#endif
    return 0;
}

int pico_notify_pkt_too_big(struct pico_frame *f)
{
    if (0) {}

#ifdef PICO_SUPPORT_ICMP4
    else if (IS_IPV4(f)) {
        pico_icmp4_mtu_exceeded(f);
    }
#endif
#ifdef PICO_SUPPORT_ICMP6
    else if (IS_IPV6(f)) {
        pico_icmp6_pkt_too_big(f);
    }
#endif
    return 0;
}

/*******************************************************************************
 *  TRANSPORT LAYER
 ******************************************************************************/

MOCKABLE int32_t pico_transport_receive(struct pico_frame *f, uint8_t proto)
{
    int32_t ret = -1;
    switch (proto) {

#ifdef PICO_SUPPORT_ICMP4
    case PICO_PROTO_ICMP4:
        ret = pico_enqueue(pico_proto_icmp4.q_in, f);
        break;
#endif

#ifdef PICO_SUPPORT_ICMP6
    case PICO_PROTO_ICMP6:
        ret = pico_enqueue(pico_proto_icmp6.q_in, f);
        break;
#endif


#if defined(PICO_SUPPORT_IGMP) && defined(PICO_SUPPORT_MCAST)
    case PICO_PROTO_IGMP:
        ret = pico_enqueue(pico_proto_igmp.q_in, f);
        break;
#endif

#ifdef PICO_SUPPORT_UDP
    case PICO_PROTO_UDP:
        ret = pico_enqueue(pico_proto_udp.q_in, f);
        break;
#endif

#ifdef PICO_SUPPORT_TCP
    case PICO_PROTO_TCP:
        ret = pico_enqueue(pico_proto_tcp.q_in, f);
        break;
#endif

    default:
        /* Protocol not available */
        dbg("pkt: no such protocol (%d)\n", proto);
        pico_notify_proto_unreachable(f);
        pico_frame_discard(f);
        ret = -1;
    }
    return ret;
}

/*******************************************************************************
 *  NETWORK LAYER
 ******************************************************************************/

MOCKABLE int32_t pico_network_receive(struct pico_frame *f)
{
    if (0) {}

#ifdef PICO_SUPPORT_IPV4
    else if (IS_IPV4(f)) {
        pico_enqueue(pico_proto_ipv4.q_in, f);
    }
#endif
#ifdef PICO_SUPPORT_IPV6
    else if (IS_IPV6(f)) {
        pico_enqueue(pico_proto_ipv6.q_in, f);
    }
#endif
    else {
        dbg("Network not found.\n");
        pico_frame_discard(f);
        return -1;
    }
    return (int32_t)f->buffer_len;
}

/// Interface towards socket for frame sending
int32_t pico_network_send(struct pico_frame *f)
{
    if (!f || !f->sock || !f->sock->net) {
        pico_frame_discard(f);
        return -1;
    }

    return f->sock->net->push(f->sock->net, f);
}

int pico_source_is_local(struct pico_frame *f)
{
    if (0) { }

#ifdef PICO_SUPPORT_IPV4
    else if (IS_IPV4(f)) {
        struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *)f->net_hdr;
        if (hdr->src.addr == PICO_IPV4_INADDR_ANY)
            return 1;

        if (pico_ipv4_link_find(&hdr->src))
            return 1;
    }
#endif
#ifdef PICO_SUPPORT_IPV6
    else if (IS_IPV6(f)) {
        struct pico_ipv6_hdr *hdr = (struct pico_ipv6_hdr *)f->net_hdr;
        if (pico_ipv6_is_unspecified(hdr->src.addr) || pico_ipv6_link_find(&hdr->src))
            return 1;
    }
#endif
    return 0;
}

void pico_store_network_origin(void *src, struct pico_frame *f)
{
  #ifdef PICO_SUPPORT_IPV4
    struct pico_ip4 *ip4;
  #endif

  #ifdef PICO_SUPPORT_IPV6
    struct pico_ip6 *ip6;
  #endif

  #ifdef PICO_SUPPORT_IPV4
    if (IS_IPV4(f)) {
        struct pico_ipv4_hdr *hdr;
        hdr = (struct pico_ipv4_hdr *) f->net_hdr;
        ip4 = (struct pico_ip4 *) src;
        ip4->addr = hdr->src.addr;
    }

  #endif
  #ifdef PICO_SUPPORT_IPV6
    if (IS_IPV6(f)) {
        struct pico_ipv6_hdr *hdr;
        hdr = (struct pico_ipv6_hdr *) f->net_hdr;
        ip6 = (struct pico_ip6 *) src;
        memcpy(ip6->addr, hdr->src.addr, PICO_SIZE_IP6);
    }

  #endif
}

int pico_address_compare(union pico_address *a, union pico_address *b, uint16_t proto)
{
    #ifdef PICO_SUPPORT_IPV6
    if (proto == PICO_PROTO_IPV6) {
        return pico_ipv6_compare(&a->ip6, &b->ip6);
    }

    #endif
    #ifdef PICO_SUPPORT_IPV4
    if (proto == PICO_PROTO_IPV4) {
        return pico_ipv4_compare(&a->ip4, &b->ip4);
    }

    #endif
    return 0;

}

int pico_frame_dst_is_unicast(struct pico_frame *f)
{
    if (0) {
        return 0;
    }

#ifdef PICO_SUPPORT_IPV4
    if (IS_IPV4(f)) {
        struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *)f->net_hdr;
        if (pico_ipv4_is_multicast(hdr->dst.addr) || pico_ipv4_is_broadcast(hdr->dst.addr))
            return 0;

        return 1;
    }

#endif

#ifdef PICO_SUPPORT_IPV6
    if (IS_IPV6(f)) {
        struct pico_ipv6_hdr *hdr = (struct pico_ipv6_hdr *)f->net_hdr;
        if (pico_ipv6_is_multicast(hdr->dst.addr) || pico_ipv6_is_unspecified(hdr->dst.addr))
            return 0;

        return 1;
    }

#endif
    else return 0;
}

/*******************************************************************************
 *  DATALINK LAYER
 ******************************************************************************/

int pico_datalink_receive(struct pico_frame *f)
{
    if (f->dev->eth) {
        /* If device has stack with datalink-layer pass frame through it */
        switch (f->dev->mode) {
            #ifdef PICO_SUPPORT_802154
            case LL_MODE_IEEE802154:
                f->datalink_hdr = f->buffer;
                return pico_enqueue(pico_proto_6lowpan_ll.q_in, f);
            #endif
            default:
                #ifdef PICO_SUPPORT_ETH
                f->datalink_hdr = f->buffer;
                return pico_enqueue(pico_proto_ethernet.q_in,f);
                #else
                return -1;
                #endif
        }
    } else {
        /* If device handles raw IP-frames send it straight to network-layer */
        f->net_hdr = f->buffer;
        pico_network_receive(f);
    }

    return 0;
}

MOCKABLE int pico_datalink_send(struct pico_frame *f)
{
    if (f->dev->eth) {
        switch (f->dev->mode) {
            #ifdef PICO_SUPPORT_802154
            case LL_MODE_IEEE802154:
                return pico_enqueue(pico_proto_6lowpan.q_out, f);
            #endif
            default:
                #ifdef PICO_SUPPORT_ETH
                return pico_enqueue(pico_proto_ethernet.q_out, f);
                #else
                return -1;
                #endif
        }
    } else {
        /* non-ethernet: no post-processing needed */
        return pico_sendto_dev(f);
    }
}

/*******************************************************************************
 *  PHYSICAL LAYER
 ******************************************************************************/

struct pico_frame *pico_stack_recv_new_frame(struct pico_device *dev, uint8_t *buffer, uint32_t len)
{
    struct pico_frame *f;
    if (len == 0)
        return NULL;

    f = pico_frame_alloc(len);
    if (!f)
    {
        dbg("Cannot alloc incoming frame!\n");
        return NULL;
    }

    /* Association to the device that just received the frame. */
    f->dev = dev;

    /* Setup the start pointer, length. */
    f->start = f->buffer;
    f->len = f->buffer_len;
    if (f->len > 8) {
        uint32_t rand, mid_frame = (f->buffer_len >> 2) << 1;
        mid_frame -= (mid_frame % 4);
        memcpy(&rand, f->buffer + mid_frame, sizeof(uint32_t));
        pico_rand_feed(rand);
    }

    memcpy(f->buffer, buffer, len);
    return f;
}

/* LOWEST LEVEL: interface towards devices. */
/* Device driver will call this function which returns immediately.
 * Incoming packet will be processed later on in the dev loop.
 */
int32_t pico_stack_recv(struct pico_device *dev, uint8_t *buffer, uint32_t len)
{
    struct pico_frame *f = pico_stack_recv_new_frame (dev, buffer, len);
    int32_t ret;

    if (!f)
        return -1;

    ret = pico_enqueue(dev->q_in, f);
    if (ret <= 0) {
        pico_frame_discard(f);
    }
    return ret;
}

static int32_t _pico_stack_recv_zerocopy(struct pico_device *dev, uint8_t *buffer, uint32_t len, int ext_buffer, void (*notify_free)(uint8_t *))
{
    struct pico_frame *f;
    int ret;
    if (len == 0)
        return -1;

    f = pico_frame_alloc_skeleton(len, ext_buffer);
    if (!f)
    {
        dbg("Cannot alloc incoming frame!\n");
        return -1;
    }

    if (pico_frame_skeleton_set_buffer(f, buffer) < 0)
    {
        dbg("Invalid zero-copy buffer!\n");
        PICO_FREE(f->usage_count);
        PICO_FREE(f);
        return -1;
    }

    if (notify_free) {
        f->notify_free = notify_free;
    }

    f->dev = dev;
    ret = pico_enqueue(dev->q_in, f);
    if (ret <= 0) {
        pico_frame_discard(f);
    }

    return ret;
}

int32_t pico_stack_recv_zerocopy(struct pico_device *dev, uint8_t *buffer, uint32_t len)
{
    return _pico_stack_recv_zerocopy(dev, buffer, len, 0, NULL);
}

int32_t pico_stack_recv_zerocopy_ext_buffer(struct pico_device *dev, uint8_t *buffer, uint32_t len)
{
    return _pico_stack_recv_zerocopy(dev, buffer, len, 1, NULL);
}

int32_t pico_stack_recv_zerocopy_ext_buffer_notify(struct pico_device *dev, uint8_t *buffer, uint32_t len, void (*notify_free)(uint8_t *buffer))
{
    return _pico_stack_recv_zerocopy(dev, buffer, len, 1, notify_free);
}

int32_t pico_sendto_dev(struct pico_frame *f)
{
    if (!f->dev) {
        pico_frame_discard(f);
        return -1;
    } else {
        if (f->len > 8) {
            uint32_t rand, mid_frame = (f->buffer_len >> 2) << 1;
            mid_frame -= (mid_frame % 4);
            memcpy(&rand, f->buffer + mid_frame, sizeof(uint32_t));
            pico_rand_feed(rand);
        }

        return pico_enqueue(f->dev->q_out, f);
    }
}

struct pico_timer
{
    void *arg;
    void (*timer)(pico_time timestamp, void *arg);
};


static uint32_t tmr_id = 0u;
struct pico_timer_ref
{
    pico_time expire;
    uint32_t id;
    uint32_t hash;
    struct pico_timer *tmr;
};

typedef struct pico_timer_ref pico_timer_ref;

DECLARE_HEAP(pico_timer_ref, expire);

static heap_pico_timer_ref *Timers;

int32_t pico_seq_compare(uint32_t a, uint32_t b)
{
    uint32_t thresh = ((uint32_t)(-1)) >> 1;

    if (a > b) /* return positive number, if not wrapped */
    {
        if ((a - b) > thresh) /* b wrapped */
            return -(int32_t)(b - a); /* b = very small,     a = very big      */
        else
            return (int32_t)(a - b); /* a = biggest,        b = a bit smaller */

    }

    if (a < b) /* return negative number, if not wrapped */
    {
        if ((b - a) > thresh) /* a wrapped */
            return (int32_t)(a - b); /* a = very small,     b = very big      */
        else
            return -(int32_t)(b - a); /* b = biggest,        a = a bit smaller */

    }

    return 0;
}

static void pico_check_timers(void)
{
    struct pico_timer *t;
    struct pico_timer_ref tref_unused, *tref = heap_first(Timers);
    pico_tick = PICO_TIME_MS();
    while((tref) && (tref->expire < pico_tick)) {
        t = tref->tmr;
        if (t && t->timer)
            t->timer(pico_tick, t->arg);

        if (t)
        {
            PICO_FREE(t);
        }

        heap_peek(Timers, &tref_unused);
        tref = heap_first(Timers);
    }
}

void MOCKABLE pico_timer_cancel(uint32_t id)
{
    uint32_t i;
    struct pico_timer_ref *tref;
    if (id == 0u)
        return;

    for (i = 1; i <= Timers->n; i++) {
        tref = heap_get_element(Timers, i);
        if (tref->id == id) {
            if (tref->tmr)
            {
                PICO_FREE(tref->tmr);
                tref->tmr = NULL;
                tref->id = 0;
            }
            break;
        }
    }
}

void pico_timer_cancel_hashed(uint32_t hash)
{
    uint32_t i;
    struct pico_timer_ref *tref;
    if (hash == 0u)
        return;

    for (i = 1; i <= Timers->n; i++) {
        tref = heap_get_element(Timers, i);
        if (tref->hash == hash) {
            if (tref->tmr)
            {
                PICO_FREE(tref->tmr);
                tref->tmr = NULL;
                tref[i].id = 0;
            }
        }
    }
}

#define PROTO_DEF_NR      11
#define PROTO_DEF_AVG_NR  4
#define PROTO_DEF_SCORE   32
#define PROTO_MIN_SCORE   32
#define PROTO_MAX_SCORE   128
#define PROTO_LAT_IND     3   /* latency indication 0-3 (lower is better latency performance), x1, x2, x4, x8 */
#define PROTO_MAX_LOOP    (PROTO_MAX_SCORE << PROTO_LAT_IND) /* max global loop score, so per tick */

static int calc_score(int *score, int *index, int avg[][PROTO_DEF_AVG_NR], int *ret)
{
    int temp, i, j, sum;
    int max_total = PROTO_MAX_LOOP, total = 0;

    /* dbg("USED SCORES> "); */

    for (i = 0; i < PROTO_DEF_NR; i++) {

        /* if used looped score */
        if (ret[i] < score[i]) {
            temp = score[i] - ret[i]; /* remaining loop score */

            /* dbg("%3d - ",temp); */

            if (index[i] >= PROTO_DEF_AVG_NR)
                index[i] = 0;   /* reset index */

            j = index[i];
            avg[i][j] = temp;

            index[i]++;

            if (ret[i] == 0 && ((score[i] * 2) <= PROTO_MAX_SCORE) && ((total + (score[i] * 2)) < max_total)) { /* used all loop score -> increase next score directly */
                score[i] *= 2;
                total += score[i];
                continue;
            }

            sum = 0;
            for (j = 0; j < PROTO_DEF_AVG_NR; j++)
                sum += avg[i][j]; /* calculate sum */

            sum /= 4;           /* divide by 4 to get average used score */

            /* criterion to increase next loop score */
            if (sum > (score[i] - (score[i] / 4))  && ((score[i] * 2) <= PROTO_MAX_SCORE) && ((total + (score[i] / 2)) < max_total)) { /* > 3/4 */
                score[i] *= 2; /* double loop score */
                total += score[i];
                continue;
            }

            /* criterion to decrease next loop score */
            if ((sum < (score[i] / 4)) && ((score[i] / 2) >= PROTO_MIN_SCORE)) { /* < 1/4 */
                score[i] /= 2; /* half loop score */
                total += score[i];
                continue;
            }

            /* also add non-changed scores */
            total += score[i];
        }
        else if (ret[i] == score[i]) {
            /* no used loop score - gradually decrease */

            /*  dbg("%3d - ",0); */

            if (index[i] >= PROTO_DEF_AVG_NR)
                index[i] = 0;   /* reset index */

            j = index[i];
            avg[i][j] = 0;

            index[i]++;

            sum = 0;
            for (j = 0; j < PROTO_DEF_AVG_NR; j++)
                sum += avg[i][j]; /* calculate sum */

            sum /= 2;          /* divide by 4 to get average used score */

            if ((sum == 0) && ((score[i] / 2) >= PROTO_MIN_SCORE)) {
                score[i] /= 2; /* half loop score */
                total += score[i];
                for (j = 0; j < PROTO_DEF_AVG_NR; j++)
                    avg[i][j] = score[i];
            }

        }
    }
    /* dbg("\n"); */

    return 0;
}

void pico_stack_tick(void)
{
    static int score[PROTO_DEF_NR] = {
        PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE, PROTO_DEF_SCORE
    };
    static int index[PROTO_DEF_NR] = {
        0, 0, 0, 0, 0, 0
    };
    static int avg[PROTO_DEF_NR][PROTO_DEF_AVG_NR];
    static int ret[PROTO_DEF_NR] = {
        0
    };

    pico_check_timers();

    /* dbg("LOOP_SCORES> %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d\n",score[0],score[1],score[2],score[3],score[4],score[5],score[6],score[7],score[8],score[9],score[10]); */

    /* score = pico_protocols_loop(100); */

    ret[0] = pico_devices_loop(score[0], PICO_LOOP_DIR_IN);
    pico_rand_feed((uint32_t)ret[0]);

    ret[1] = pico_protocol_datalink_loop(score[1], PICO_LOOP_DIR_IN);
    pico_rand_feed((uint32_t)ret[1]);

    ret[2] = pico_protocol_network_loop(score[2], PICO_LOOP_DIR_IN);
    pico_rand_feed((uint32_t)ret[2]);

    ret[3] = pico_protocol_transport_loop(score[3], PICO_LOOP_DIR_IN);
    pico_rand_feed((uint32_t)ret[3]);


    ret[5] = score[5];
#if defined (PICO_SUPPORT_IPV4) || defined (PICO_SUPPORT_IPV6)
#if defined (PICO_SUPPORT_TCP) || defined (PICO_SUPPORT_UDP)
    ret[5] = pico_sockets_loop(score[5]); /* swapped */
    pico_rand_feed((uint32_t)ret[5]);
#endif
#endif

    ret[4] = pico_protocol_socket_loop(score[4], PICO_LOOP_DIR_IN);
    pico_rand_feed((uint32_t)ret[4]);


    ret[6] = pico_protocol_socket_loop(score[6], PICO_LOOP_DIR_OUT);
    pico_rand_feed((uint32_t)ret[6]);

    ret[7] = pico_protocol_transport_loop(score[7], PICO_LOOP_DIR_OUT);
    pico_rand_feed((uint32_t)ret[7]);

    ret[8] = pico_protocol_network_loop(score[8], PICO_LOOP_DIR_OUT);
    pico_rand_feed((uint32_t)ret[8]);

    ret[9] = pico_protocol_datalink_loop(score[9], PICO_LOOP_DIR_OUT);
    pico_rand_feed((uint32_t)ret[9]);

    ret[10] = pico_devices_loop(score[10], PICO_LOOP_DIR_OUT);
    pico_rand_feed((uint32_t)ret[10]);

    /* calculate new loop scores for next iteration */
    calc_score(score, index, (int (*)[])avg, ret);
}

void pico_stack_loop(void)
{
    while(1) {
        pico_stack_tick();
        PICO_IDLE();
    }
}

static uint32_t
pico_timer_ref_add(pico_time expire, struct pico_timer *t, uint32_t id, uint32_t hash)
{
    struct pico_timer_ref tref;

    tref.expire = PICO_TIME_MS() + expire;
    tref.tmr = t;
    tref.id = id;
    tref.hash = hash;

    if (heap_insert(Timers, &tref) < 0) {
        dbg("Error: failed to insert timer(ID %u) into heap\n", id);
        PICO_FREE(t);
        pico_err = PICO_ERR_ENOMEM;
        return 0;
    }
    if (Timers->n > PICO_MAX_TIMERS) {
        dbg("Warning: I have %d timers\n", (int)Timers->n);
    }

    return tref.id;
}

static struct pico_timer *
pico_timer_create(void (*timer)(pico_time, void *), void *arg)
{
    struct pico_timer *t = PICO_ZALLOC(sizeof(struct pico_timer));

    if (!t) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    t->arg = arg;
    t->timer = timer;

    return t;
}

MOCKABLE uint32_t pico_timer_add(pico_time expire, void (*timer)(pico_time, void *), void *arg)
{
    struct pico_timer *t = pico_timer_create(timer, arg);

    /* zero is guard for timers */
    if (tmr_id == 0u) {
        tmr_id++;
    }

    if (!t)
        return 0;

    return pico_timer_ref_add(expire, t, tmr_id++, 0);
}

uint32_t pico_timer_add_hashed(pico_time expire, void (*timer)(pico_time, void *), void *arg, uint32_t hash)
{
    struct pico_timer *t = pico_timer_create(timer, arg);

    /* zero is guard for timers */
    if (tmr_id == 0u) {
        tmr_id++;
    }

    if (!t)
        return 0;

    return pico_timer_ref_add(expire, t, tmr_id++, hash);
} /* Static path count: 4 */

int MOCKABLE pico_stack_init(void)
{
#ifdef PICO_SUPPORT_ETH
    pico_protocol_init(&pico_proto_ethernet);
#endif

#ifdef PICO_SUPPORT_6LOWPAN
    pico_protocol_init(&pico_proto_6lowpan);
    pico_protocol_init(&pico_proto_6lowpan_ll);
#endif

#ifdef PICO_SUPPORT_IPV4
    pico_protocol_init(&pico_proto_ipv4);
#endif

#ifdef PICO_SUPPORT_IPV6
    pico_protocol_init(&pico_proto_ipv6);
#endif

#ifdef PICO_SUPPORT_ICMP4
    pico_protocol_init(&pico_proto_icmp4);
#endif

#ifdef PICO_SUPPORT_ICMP6
    pico_protocol_init(&pico_proto_icmp6);
#endif

#if defined(PICO_SUPPORT_IGMP) && defined(PICO_SUPPORT_MCAST)
    pico_protocol_init(&pico_proto_igmp);
#endif

#ifdef PICO_SUPPORT_UDP
    pico_protocol_init(&pico_proto_udp);
#endif

#ifdef PICO_SUPPORT_TCP
    pico_protocol_init(&pico_proto_tcp);
#endif

#ifdef PICO_SUPPORT_DNS_CLIENT
    pico_dns_client_init();
#endif

    pico_rand_feed(123456);

    /* Initialize timer heap */
    Timers = heap_init();
    if (!Timers)
        return -1;

#if ((defined PICO_SUPPORT_IPV4) && (defined PICO_SUPPORT_ETH))
    /* Initialize ARP module */
    pico_arp_init();
#endif

#ifdef PICO_SUPPORT_IPV6
    /* Initialize Neighbor discovery module */
    pico_ipv6_nd_init();
#endif

#ifdef PICO_SUPPORT_OLSR
    pico_olsr_init();
#endif
#ifdef PICO_SUPPORT_AODV
    pico_aodv_init();
#endif
#ifdef PICO_SUPPORT_6LOWPAN
    if (pico_6lowpan_init())
       return -1;
#endif
    pico_stack_tick();
    pico_stack_tick();
    pico_stack_tick();
    return 0;
}

