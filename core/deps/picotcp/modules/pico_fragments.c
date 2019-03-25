/*********************************************************************
   PicoTCP. Copyright (c) 2012 TASS Belgium NV. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Laurens Miers, Daniele Lacamera
 *********************************************************************/


#include "pico_config.h"
#ifdef PICO_SUPPORT_IPV6
#include "pico_ipv6.h"
#include "pico_icmp6.h"
#endif
#ifdef PICO_SUPPORT_IPV4
#include "pico_ipv4.h"
#include "pico_icmp4.h"
#endif
#include "pico_stack.h"
#include "pico_eth.h"
#include "pico_udp.h"
#include "pico_tcp.h"
#include "pico_socket.h"
#include "pico_device.h"
#include "pico_tree.h"
#include "pico_constants.h"
#include "pico_fragments.h"

#ifdef DEBUG_FRAG
    #define frag_dbg dbg
#else
    #define frag_dbg(...) do {} while(0)
#endif

#if defined(PICO_SUPPORT_IPV6) && defined(PICO_SUPPORT_IPV6FRAG)
#define IP6_FRAG_OFF(x)         ((x & 0xFFF8u))
#define IP6_FRAG_MORE(x)        ((x & 0x0001))
#define IP6_FRAG_ID(x)          ((uint32_t)(((uint32_t)x->ext.frag.id[0] << 24) + ((uint32_t)x->ext.frag.id[1] << 16) + \
                                            ((uint32_t)x->ext.frag.id[2] << 8) + (uint32_t)x->ext.frag.id[3]))

#else
#define IP6_FRAG_OFF(x)         (0)
#define IP6_FRAG_MORE(x)        (0)
#define IP6_FRAG_ID(x)          (0)
#endif

#if defined(PICO_SUPPORT_IPV4) && defined(PICO_SUPPORT_IPV4FRAG)
#define IP4_FRAG_OFF(frag)      (((uint32_t)frag & PICO_IPV4_FRAG_MASK) << 3ul)
#define IP4_FRAG_MORE(frag)     ((frag & PICO_IPV4_MOREFRAG) ? 1 : 0)
#define IP4_FRAG_ID(hdr)        (hdr->id)
#else
#define IP4_FRAG_OFF(frag)      (0)
#define IP4_FRAG_MORE(frag)     (0)
#define IP4_FRAG_ID(hdr)        (0)
#endif

#define PICO_IPV6_FRAG_TIMEOUT   60000
#define PICO_IPV4_FRAG_TIMEOUT   15000

static void pico_frag_expire(pico_time now, void *arg);
static void pico_fragments_complete(unsigned int bookmark, uint8_t proto, uint8_t net);
static int pico_fragments_check_complete(struct pico_tree *tree, uint8_t proto, uint8_t net);
static int pico_fragments_reassemble(struct pico_tree *tree, unsigned int len, uint8_t proto, uint8_t net);
static int pico_fragments_get_more_flag(struct pico_frame *frame, uint8_t net);
static uint32_t pico_fragments_get_offset(struct pico_frame *frame, uint8_t net);
static void pico_fragments_send_notify(struct pico_frame *first);
static uint16_t pico_fragments_get_header_length(uint8_t net);
static void pico_fragments_empty_tree(struct pico_tree *tree);

#if defined(PICO_SUPPORT_IPV6) && defined(PICO_SUPPORT_IPV6FRAG)
static uint32_t ipv6_cur_frag_id = 0u;
static uint32_t ipv6_fragments_timer = 0u;

static int pico_ipv6_frag_compare(void *ka, void *kb)
{
    struct pico_frame *a = ka, *b = kb;
    if (IP6_FRAG_OFF(a->frag) > IP6_FRAG_OFF(b->frag))
        return 1;

    if (IP6_FRAG_OFF(a->frag) < IP6_FRAG_OFF(b->frag))
        return -1;

    return 0;
}
static PICO_TREE_DECLARE(ipv6_fragments, pico_ipv6_frag_compare);

static void pico_ipv6_fragments_complete(unsigned int len, uint8_t proto)
{
    if (pico_fragments_reassemble(&ipv6_fragments, len, proto, PICO_PROTO_IPV6) == 0)
    {
        pico_timer_cancel(ipv6_fragments_timer);
        ipv6_fragments_timer = 0;
    }
}

static void pico_ipv6_frag_timer_on(void)
{
    ipv6_fragments_timer = pico_timer_add(PICO_IPV6_FRAG_TIMEOUT, pico_frag_expire, &ipv6_fragments);
    if (!ipv6_fragments_timer) {
        frag_dbg("FRAG: Failed to start IPv6 expiration timer\n");
        pico_fragments_empty_tree(&ipv6_fragments);
    }
}

static int pico_ipv6_frag_match(struct pico_frame *a, struct pico_frame *b)
{
    struct pico_ipv6_hdr *ha = NULL, *hb = NULL;
    if (!a || !b)
        return -1;

    ha = (struct pico_ipv6_hdr *)a->net_hdr;
    hb = (struct pico_ipv6_hdr *)b->net_hdr;
    if (!ha || !hb)
        return -2;

    if (memcmp(ha->src.addr, hb->src.addr, PICO_SIZE_IP6) != 0)
        return 1;

    if (memcmp(ha->dst.addr, hb->dst.addr, PICO_SIZE_IP6) != 0)
        return 2;

    return 0;
}
#endif

#if defined(PICO_SUPPORT_IPV4) && defined(PICO_SUPPORT_IPV4FRAG)
static uint32_t ipv4_cur_frag_id = 0u;
static uint32_t ipv4_fragments_timer = 0u;

static int pico_ipv4_frag_compare(void *ka, void *kb)
{
    struct pico_frame *a = ka, *b = kb;
    if (IP4_FRAG_OFF(a->frag) > IP4_FRAG_OFF(b->frag))
        return 1;

    if (IP4_FRAG_OFF(a->frag) < IP4_FRAG_OFF(b->frag))
        return -1;

    return 0;
}
static PICO_TREE_DECLARE(ipv4_fragments, pico_ipv4_frag_compare);

static void pico_ipv4_fragments_complete(unsigned int len, uint8_t proto)
{
    if (pico_fragments_reassemble(&ipv4_fragments, len, proto, PICO_PROTO_IPV4) == 0)
    {
        pico_timer_cancel(ipv4_fragments_timer);
        ipv4_fragments_timer = 0;
    }
}

static void pico_ipv4_frag_timer_on(void)
{
    ipv4_fragments_timer = pico_timer_add( PICO_IPV4_FRAG_TIMEOUT, pico_frag_expire, &ipv4_fragments);
    if (!ipv4_fragments_timer) {
        frag_dbg("FRAG: Failed to start IPv4 expiration timer\n");
        pico_fragments_empty_tree(&ipv4_fragments);
    }
}

static int pico_ipv4_frag_match(struct pico_frame *a, struct pico_frame *b)
{
    struct pico_ipv4_hdr *ha, *hb;
    if (!a || !b)
        return -1;

    ha = (struct pico_ipv4_hdr *)a->net_hdr;
    hb = (struct pico_ipv4_hdr *)b->net_hdr;
    if (!ha || !hb)
        return -2;

    if (memcmp(&(ha->src.addr), &(hb->src.addr), PICO_SIZE_IP4) != 0)
        return 1;

    if (memcmp(&(ha->dst.addr), &(hb->dst.addr), PICO_SIZE_IP4) != 0)
        return 2;

    return 0;
}
#endif

static void pico_fragments_complete(unsigned int bookmark, uint8_t proto, uint8_t net)
{
    if (0) {}

#if defined(PICO_SUPPORT_IPV4) && defined(PICO_SUPPORT_IPV4FRAG)
    else if (net == PICO_PROTO_IPV4)
    {
        pico_ipv4_fragments_complete(bookmark, proto);
    }
#endif
#if defined(PICO_SUPPORT_IPV6) && defined(PICO_SUPPORT_IPV6FRAG)
    else if (net == PICO_PROTO_IPV6)
    {
        pico_ipv6_fragments_complete(bookmark, proto);
    }
#endif
}

static void pico_fragments_empty_tree(struct pico_tree *tree)
{
    struct pico_tree_node *index, *tmp;

    if (!tree)
    {
        return;
    }

    pico_tree_foreach_safe(index, tree, tmp) {
        struct pico_frame * old = index->keyValue;
        pico_tree_delete(tree, old);
        pico_frame_discard(old);
    }

}

static int pico_fragments_check_complete(struct pico_tree *tree, uint8_t proto, uint8_t net)
{
    struct pico_tree_node *index, *temp;
    struct pico_frame *cur;
    unsigned int bookmark = 0;

    if (!tree)
        return 0;

    pico_tree_foreach_safe(index, tree, temp) {
        cur = index->keyValue;
        if (cur) {
            if (pico_fragments_get_offset(cur, net) != bookmark)
                return -1;

            bookmark += cur->transport_len;
            if (!pico_fragments_get_more_flag(cur, net)) {
                pico_fragments_complete(bookmark, proto, net);
                return 0;
            }
        }
    }
    return 1;
}

static void pico_frag_expire(pico_time now, void *arg)
{
    struct pico_tree *tree = (struct pico_tree *) arg;
    struct pico_frame *first = NULL;
    IGNORE_PARAMETER(now);

    if (!tree)
    {
        frag_dbg("Expired packet but no tree supplied!\n");
        return;
    }

    first = pico_tree_first(tree);

    if (!first) {
        frag_dbg("Empty tree - not sending notify\n");
        return;
    }

    pico_fragments_send_notify(first);

    pico_fragments_empty_tree(tree);
}

static void pico_fragments_send_notify(struct pico_frame *first)
{
    uint8_t net = 0;

    if (!first)
    {
        return;
    }

    if (0) {}

#if defined(PICO_SUPPORT_IPV4) && defined(PICO_SUPPORT_IPV4FRAG)
    else if (IS_IPV4(first))
    {
        net = PICO_PROTO_IPV4;
        frag_dbg("Packet expired! ID:%hu\n", ipv4_cur_frag_id);
    }

#endif
#if defined(PICO_SUPPORT_IPV6) && defined(PICO_SUPPORT_IPV6FRAG)
    else if (IS_IPV6(first))
    {
        net = PICO_PROTO_IPV6;
        frag_dbg("Packet expired! ID:%hu\n", ipv6_cur_frag_id);
    }

#endif

    if (((pico_fragments_get_offset(first, net) == 0) && (pico_frame_dst_is_unicast(first))))
    {
        frag_dbg("sending notify\n");
        pico_notify_frag_expired(first);
    }
    else
    {
        frag_dbg("Not first packet or not unicast address, not sending notify");
    }
}

static int pico_fragments_reassemble(struct pico_tree *tree, unsigned int len, uint8_t proto, uint8_t net)
{
    struct pico_tree_node *index, *tmp;
    struct pico_frame *f;
    uint16_t header_length = 0;
    unsigned int bookmark = 0;
    struct pico_frame *full = NULL;
    struct pico_frame *first = NULL;

    if (!tree)
    {
        frag_dbg("Cannot reassemble packet, no tree supplied!\n");
        return -1;
    }

    first = pico_tree_first(tree);

    if (!first)
    {
        frag_dbg("Cannot reassemble packet, empty tree supplied!\n");
        return -2;
    }

    header_length = pico_fragments_get_header_length(net);

    if (!header_length)
    {
        return -3;
    }

    full = pico_frame_alloc((uint16_t)(header_length + len));
    if (full) {
        full->net_hdr = full->buffer;
        full->net_len = header_length;
        memcpy(full->net_hdr, first->net_hdr, full->net_len);
        full->transport_hdr = full->net_hdr + full->net_len;
        full->transport_len = (uint16_t)len;
        full->dev = first->dev;
        pico_tree_foreach_safe(index, tree, tmp) {
            f = index->keyValue;
            memcpy(full->transport_hdr + bookmark, f->transport_hdr, f->transport_len);
            bookmark += f->transport_len;
            pico_tree_delete(tree, f);
            pico_frame_discard(f);
        }
        if (pico_transport_receive(full, proto) == -1)
        {
            pico_frame_discard(full);
        }

        return 0;
    }

    return 1;
}

static uint16_t pico_fragments_get_header_length(uint8_t net)
{
    if (0) {}

#if defined(PICO_SUPPORT_IPV4) && defined(PICO_SUPPORT_IPV4FRAG)
    else if (net == PICO_PROTO_IPV4)
    {
        return PICO_SIZE_IP4HDR;
    }
#endif
#if defined(PICO_SUPPORT_IPV6) && defined(PICO_SUPPORT_IPV6FRAG)
    else if (net == PICO_PROTO_IPV6)
    {
        return PICO_SIZE_IP6HDR;
    }
#endif

    return 0;
}

static int pico_fragments_get_more_flag(struct pico_frame *frame, uint8_t net)
{
    if (!frame)
    {
      frag_dbg("no frame given to determine more flag\n");
      return 0;
    }

    if (0) {}

#if defined(PICO_SUPPORT_IPV4) && defined(PICO_SUPPORT_IPV4FRAG)
    else if (net == PICO_PROTO_IPV4)
    {
      return IP4_FRAG_MORE(frame->frag);
    }
#endif
#if defined(PICO_SUPPORT_IPV6) && defined(PICO_SUPPORT_IPV6FRAG)
    else if (net == PICO_PROTO_IPV6)
    {
      return IP6_FRAG_MORE(frame->frag);
    }
#endif

    return 0;
}

static uint32_t pico_fragments_get_offset(struct pico_frame *frame, uint8_t net)
{
    if (!frame)
    {
      frag_dbg("no frame given to determine offset\n");
      return 0;
    }

    if (0) {}

#if defined(PICO_SUPPORT_IPV4) && defined(PICO_SUPPORT_IPV4FRAG)
    else if (net == PICO_PROTO_IPV4)
    {
      return IP4_FRAG_OFF(frame->frag);
    }
#endif
#if defined(PICO_SUPPORT_IPV6) && defined(PICO_SUPPORT_IPV6FRAG)
    else if (net == PICO_PROTO_IPV6)
    {
      return IP6_FRAG_OFF(frame->frag);
    }
#endif

    return 0;
}

void pico_ipv6_process_frag(struct pico_ipv6_exthdr *frag, struct pico_frame *f, uint8_t proto)
{
#if defined(PICO_SUPPORT_IPV6) && defined(PICO_SUPPORT_IPV6FRAG)
    struct pico_frame *first = NULL;

    if (!f || !frag)
    {
        frag_dbg("Bad arguments provided to pico_ipv6_process_frag\n");
        return;
    }

    first = pico_tree_first(&ipv6_fragments);

    if (first)
    {
        if ((pico_ipv6_frag_match(f, first) == 0 && (IP6_FRAG_ID(frag) == ipv6_cur_frag_id))) {
            struct pico_frame *temp = NULL;

            temp = pico_frame_copy(f);

            if (!temp) {
                frag_dbg("Could not allocate memory to continue reassembly of IPV6 fragmented packet (id: %hu)\n", ipv6_cur_frag_id);
                return;
            }

            if (pico_tree_insert(&ipv6_fragments, temp)) {
                frag_dbg("FRAG: Could not insert picoframe in tree\n");
                pico_frame_discard(temp);
                return;
            }
        }
    }
    else
    {
        struct pico_frame *temp = NULL;

        if (ipv6_cur_frag_id && (IP6_FRAG_ID(frag) == ipv6_cur_frag_id)) {
            /* Discard late arrivals, without firing the timer. */
            frag_dbg("discarded late arrival, exp:%hu found:%hu\n", ipv6_cur_frag_id, IP6_FRAG_ID(frag));
            return;
        }

        temp = pico_frame_copy(f);

        if (!temp) {
            frag_dbg("Could not allocate memory to start reassembly of fragmented packet\n");
            return;
        }

        pico_ipv6_frag_timer_on();
        ipv6_cur_frag_id = IP6_FRAG_ID(frag);
        frag_dbg("Started new reassembly, ID:%hu\n", ipv6_cur_frag_id);

        if (pico_tree_insert(&ipv6_fragments, temp)) {
            frag_dbg("FRAG: Could not insert picoframe in tree\n");
            pico_frame_discard(temp);
            return;
        }
    }

    pico_fragments_check_complete(&ipv6_fragments, proto, PICO_PROTO_IPV6);
#else
    IGNORE_PARAMETER(frag);
    IGNORE_PARAMETER(f);
    IGNORE_PARAMETER(proto);
#endif
}

void pico_ipv4_process_frag(struct pico_ipv4_hdr *hdr, struct pico_frame *f, uint8_t proto)
{
#if defined(PICO_SUPPORT_IPV4) && defined(PICO_SUPPORT_IPV4FRAG)
    struct pico_frame *first = NULL;

    if (!f || !hdr)
    {
        frag_dbg("Bad arguments provided to pico_ipv4_process_frag\n");
        return;
    }

    first = pico_tree_first(&ipv4_fragments);

    if (first)
    {
        /* fragments from old packets still in tree, and new first fragment ? */
        if ((IP4_FRAG_ID(hdr) != ipv4_cur_frag_id) && (IP4_FRAG_OFF(f->frag) == 0)) {
            pico_fragments_empty_tree(&ipv4_fragments);

            first = NULL;
            ipv4_cur_frag_id = 0;
        }

        if ((pico_ipv4_frag_match(f, first) == 0 && (IP4_FRAG_ID(hdr) == ipv4_cur_frag_id))) {
            struct pico_frame *temp = NULL;

            temp = pico_frame_copy(f);

            if (!temp) {
                frag_dbg("Could not allocate memory to continue reassembly of IPV4 fragmented packet (id: %hu)\n", ipv4_cur_frag_id);
                return;
            }

            if (pico_tree_insert(&ipv4_fragments, temp)) {
            	frag_dbg("FRAG: Could not insert picoframe in tree\n");
                pico_frame_discard(temp);
                return;
			}
        }
    }
    else
    {
        struct pico_frame *temp = NULL;

        if (ipv4_cur_frag_id && (IP4_FRAG_ID(hdr) == ipv4_cur_frag_id)) {
            /* Discard late arrivals, without firing the timer */
            return;
        }

        temp = pico_frame_copy(f);

        if (!temp) {
            frag_dbg("Could not allocate memory to start reassembly fragmented packet\n");
            return;
        }

        pico_ipv4_frag_timer_on();
        ipv4_cur_frag_id = IP4_FRAG_ID(hdr);
        frag_dbg("Started new reassembly, ID:%hu\n", ipv4_cur_frag_id);

        if (pico_tree_insert(&ipv4_fragments, temp)) {
            frag_dbg("FRAG: Could not insert picoframe in tree\n");
            pico_frame_discard(temp);
            return;
        }
    }

    pico_fragments_check_complete(&ipv4_fragments, proto, PICO_PROTO_IPV4);
#else
    IGNORE_PARAMETER(hdr);
    IGNORE_PARAMETER(f);
    IGNORE_PARAMETER(proto);
#endif
}
