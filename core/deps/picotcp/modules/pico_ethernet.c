 /*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Authors: Daniele Lacamera
 *********************************************************************/

#include "pico_config.h"
#include "pico_stack.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_icmp4.h"
#include "pico_icmp6.h"
#include "pico_arp.h"
#include "pico_ethernet.h"

#define IS_LIMITED_BCAST(f) (((struct pico_ipv4_hdr *) f->net_hdr)->dst.addr == PICO_IP4_BCAST)

#ifdef PICO_SUPPORT_ETH

const uint8_t PICO_ETHADDR_ALL[6] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

# define PICO_SIZE_MCAST 3
static const uint8_t PICO_ETHADDR_MCAST[6] = {
    0x01, 0x00, 0x5e, 0x00, 0x00, 0x00
};

#ifdef PICO_SUPPORT_IPV6
# define PICO_SIZE_MCAST6 2
static const uint8_t PICO_ETHADDR_MCAST6[6] = {
    0x33, 0x33, 0x00, 0x00, 0x00, 0x00
};
#endif

/* DATALINK LEVEL: interface from network to the device
 * and vice versa.
 */

/* The pico_ethernet_receive() function is used by
 * those devices supporting ETH in order to push packets up
 * into the stack.
 */

/* Queues */
static struct pico_queue ethernet_in = {
    0
};
static struct pico_queue ethernet_out = {
    0
};

int32_t MOCKABLE pico_ethernet_send(struct pico_frame *f);
static int32_t pico_ethernet_receive(struct pico_frame *f);

static int pico_ethernet_process_out(struct pico_protocol *self, struct pico_frame *f)
{
    IGNORE_PARAMETER(self);
    return pico_ethernet_send(f);
}

static int pico_ethernet_process_in(struct pico_protocol *self, struct pico_frame *f)
{
    IGNORE_PARAMETER(self);
    return (pico_ethernet_receive(f) <= 0); /* 0 on success, which is ret > 0 */
}

static struct pico_frame *pico_ethernet_alloc(struct pico_protocol *self, struct pico_device *dev, uint16_t size)
{
    struct pico_frame *f = NULL;
    uint32_t overhead = 0;
    IGNORE_PARAMETER(self);

    if (dev)
        overhead = dev->overhead;

    f = pico_frame_alloc((uint32_t)(overhead + size + PICO_SIZE_ETHHDR));
    if (!f)
        return NULL;

    f->dev = dev;
    f->datalink_hdr = f->buffer + overhead;
    f->net_hdr = f->datalink_hdr + PICO_SIZE_ETHHDR;
    /* Stay of the rest, higher levels will take care */

    return f;
}

/* Interface: protocol definition */
struct pico_protocol pico_proto_ethernet = {
    .name = "ethernet",
    .layer = PICO_LAYER_DATALINK,
    .alloc = pico_ethernet_alloc,
    .process_in = pico_ethernet_process_in,
    .process_out = pico_ethernet_process_out,
    .q_in = &ethernet_in,
    .q_out = &ethernet_out,
};

static int destination_is_bcast(struct pico_frame *f)
{
    if (!f)
        return 0;

    if (IS_IPV6(f))
        return 0;

#ifdef PICO_SUPPORT_IPV4
    else {
        struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *) f->net_hdr;
        return pico_ipv4_is_broadcast(hdr->dst.addr);
    }
#else
    return 0;
#endif
}

static int destination_is_mcast(struct pico_frame *f)
{
    int ret = 0;
    if (!f)
        return 0;

#ifdef PICO_SUPPORT_IPV6
    if (IS_IPV6(f)) {
        struct pico_ipv6_hdr *hdr = (struct pico_ipv6_hdr *) f->net_hdr;
        ret = pico_ipv6_is_multicast(hdr->dst.addr);
    }

#endif
#ifdef PICO_SUPPORT_IPV4
    else {
        struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *) f->net_hdr;
        ret = pico_ipv4_is_multicast(hdr->dst.addr);
    }
#endif

    return ret;
}

#ifdef PICO_SUPPORT_IPV4
static int32_t pico_ipv4_ethernet_receive(struct pico_frame *f)
{
    if (IS_IPV4(f)) {
        if (pico_enqueue(pico_proto_ipv4.q_in, f) < 0) {
            pico_frame_discard(f);
            return -1;
        }
    } else {
        (void)pico_icmp4_param_problem(f, 0);
        pico_frame_discard(f);
        return -1;
    }

    return (int32_t)f->buffer_len;
}
#endif

#ifdef PICO_SUPPORT_IPV6
static int32_t pico_ipv6_ethernet_receive(struct pico_frame *f)
{
    if (IS_IPV6(f)) {
        if (pico_enqueue(pico_proto_ipv6.q_in, f) < 0) {
            pico_frame_discard(f);
            return -1;
        }
    } else {
        /* Wrong version for link layer type */
        pico_frame_discard(f);
        return -1;
    }

    return (int32_t)f->buffer_len;
}
#endif

static int32_t pico_eth_receive(struct pico_frame *f)
{
    struct pico_eth_hdr *hdr = (struct pico_eth_hdr *) f->datalink_hdr;
    f->net_hdr = f->datalink_hdr + sizeof(struct pico_eth_hdr);

#if (defined PICO_SUPPORT_IPV4) && (defined PICO_SUPPORT_ETH)
    if (hdr->proto == PICO_IDETH_ARP)
        return pico_arp_receive(f);
#endif

#if defined (PICO_SUPPORT_IPV4)
    if (hdr->proto == PICO_IDETH_IPV4)
        return pico_ipv4_ethernet_receive(f);
#endif

#if defined (PICO_SUPPORT_IPV6)
    if (hdr->proto == PICO_IDETH_IPV6)
        return pico_ipv6_ethernet_receive(f);
#endif

    pico_frame_discard(f);
    return -1;
}

static void pico_eth_check_bcast(struct pico_frame *f)
{
    struct pico_eth_hdr *hdr = (struct pico_eth_hdr *) f->datalink_hdr;
    /* Indicate a link layer broadcast packet */
    if (memcmp(hdr->daddr, PICO_ETHADDR_ALL, PICO_SIZE_ETH) == 0)
        f->flags |= PICO_FRAME_FLAG_BCAST;
}

static int32_t pico_ethernet_receive(struct pico_frame *f)
{
    struct pico_eth_hdr *hdr;
    if (!f || !f->dev || !f->datalink_hdr)
    {
        pico_frame_discard(f);
        return -1;
    }

    hdr = (struct pico_eth_hdr *) f->datalink_hdr;
    if ((memcmp(hdr->daddr, f->dev->eth->mac.addr, PICO_SIZE_ETH) != 0) &&
        (memcmp(hdr->daddr, PICO_ETHADDR_MCAST, PICO_SIZE_MCAST) != 0) &&
#ifdef PICO_SUPPORT_IPV6
        (memcmp(hdr->daddr, PICO_ETHADDR_MCAST6, PICO_SIZE_MCAST6) != 0) &&
#endif
        (memcmp(hdr->daddr, PICO_ETHADDR_ALL, PICO_SIZE_ETH) != 0))
    {
        pico_frame_discard(f);
        return -1;
    }

    pico_eth_check_bcast(f);
    return pico_eth_receive(f);
}

static struct pico_eth *pico_ethernet_mcast_translate(struct pico_frame *f, uint8_t *pico_mcast_mac)
{
    struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *) f->net_hdr;

    /* place 23 lower bits of IP in lower 23 bits of MAC */
    pico_mcast_mac[5] = (long_be(hdr->dst.addr) & 0x000000FFu);
    pico_mcast_mac[4] = (uint8_t)((long_be(hdr->dst.addr) & 0x0000FF00u) >> 8u);
    pico_mcast_mac[3] = (uint8_t)((long_be(hdr->dst.addr) & 0x007F0000u) >> 16u);

    return (struct pico_eth *)pico_mcast_mac;
}


#ifdef PICO_SUPPORT_IPV6
static struct pico_eth *pico_ethernet_mcast6_translate(struct pico_frame *f, uint8_t *pico_mcast6_mac)
{
    struct pico_ipv6_hdr *hdr = (struct pico_ipv6_hdr *)f->net_hdr;

    /* first 2 octets are 0x33, last four are the last four of dst */
    pico_mcast6_mac[5] = hdr->dst.addr[PICO_SIZE_IP6 - 1];
    pico_mcast6_mac[4] = hdr->dst.addr[PICO_SIZE_IP6 - 2];
    pico_mcast6_mac[3] = hdr->dst.addr[PICO_SIZE_IP6 - 3];
    pico_mcast6_mac[2] = hdr->dst.addr[PICO_SIZE_IP6 - 4];

    return (struct pico_eth *)pico_mcast6_mac;
}
#endif

static int pico_ethernet_ipv6_dst(struct pico_frame *f, struct pico_eth *const dstmac)
{
    int retval = -1;
    if (!dstmac)
        return -1;

    #ifdef PICO_SUPPORT_IPV6
    if (destination_is_mcast(f)) {
        uint8_t pico_mcast6_mac[6] = {
            0x33, 0x33, 0x00, 0x00, 0x00, 0x00
        };
        pico_ethernet_mcast6_translate(f, pico_mcast6_mac);
        memcpy(dstmac, pico_mcast6_mac, PICO_SIZE_ETH);
        retval = 0;
    } else {
        struct pico_eth *neighbor = pico_ipv6_get_neighbor(f);
        if (neighbor)
        {
            memcpy(dstmac, neighbor, PICO_SIZE_ETH);
            retval = 0;
        }
    }

    #else
    (void)f;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    #endif
    return retval;
}


/* Ethernet send, first attempt: try our own address.
 * Returns 0 if the packet is not for us.
 * Returns 1 if the packet is cloned to our own receive queue, so the caller can discard the original frame.
 * */
static int32_t pico_ethsend_local(struct pico_frame *f, struct pico_eth_hdr *hdr)
{
    if (!hdr)
        return 0;

    /* Check own mac */
    if(!memcmp(hdr->daddr, hdr->saddr, PICO_SIZE_ETH)) {
        struct pico_frame *clone = pico_frame_copy(f);
        dbg("sending out packet destined for our own mac\n");
        if (pico_ethernet_receive(clone) < 0) {
            dbg("pico_ethernet_receive() failed\n");
        }
        return 1;
    }

    return 0;
}

/* Ethernet send, second attempt: try bcast.
 * Returns 0 if the packet is not bcast, so it will be handled somewhere else.
 * Returns 1 if the packet is handled by the pico_device_broadcast() function, so it can be discarded.
 * */
static int32_t pico_ethsend_bcast(struct pico_frame *f)
{
    if (IS_LIMITED_BCAST(f)) {
        (void)pico_device_broadcast(f); /* We can discard broadcast even if it's not sent. */
        return 1;
    }

    return 0;
}

/* Ethernet send, third attempt: try unicast.
 * If the device driver is busy, we return 0, so the stack won't discard the frame.
 * In case of success, we can safely return 1.
 */
static int32_t pico_ethsend_dispatch(struct pico_frame *f)
{
    return (pico_sendto_dev(f) > 0); // Return 1 on success, ret > 0
}

/* Checks whether or not there's enough headroom allocated in the frame to
 * prepend the Ethernet header. Reallocates if this is not the case. */
static int eth_check_headroom(struct pico_frame *f)
{
    uint32_t headroom = (uint32_t)(f->net_hdr - f->buffer);
    uint32_t grow = (uint32_t)(PICO_SIZE_ETHHDR - headroom);
    if (headroom < (uint32_t)PICO_SIZE_ETHHDR) {
        return pico_frame_grow_head(f, (uint32_t)(f->buffer_len + grow));
    }
    return 0;
}

/* This function looks for the destination mac address
 * in order to send the frame being processed.
 */
int32_t MOCKABLE pico_ethernet_send(struct pico_frame *f)
{
    struct pico_eth dstmac;
    uint8_t dstmac_valid = 0;
    uint16_t proto = PICO_IDETH_IPV4;

#ifdef PICO_SUPPORT_IPV6
    /* Step 1: If the frame has an IPv6 packet,
     * destination address is taken from the ND tables
     */
    if (IS_IPV6(f)) {
        if (pico_ethernet_ipv6_dst(f, &dstmac) < 0)
        {
            /* Enqueue copy of frame in IPv6 ND-module to retry later. Discard
             * frame, otherwise we have a duplicate in IPv6-ND */
            pico_ipv6_nd_postpone(f);
            return (int32_t)f->len;
        }

        dstmac_valid = 1;
        proto = PICO_IDETH_IPV6;
    }
    else
#endif

    /* In case of broadcast (IPV4 only), dst mac is FF:FF:... */
    if (IS_BCAST(f) || destination_is_bcast(f))
    {
        memcpy(&dstmac, PICO_ETHADDR_ALL, PICO_SIZE_ETH);
        dstmac_valid = 1;
    }

    /* In case of multicast, dst mac is translated from the group address */
    else if (destination_is_mcast(f)) {
        uint8_t pico_mcast_mac[6] = {
            0x01, 0x00, 0x5e, 0x00, 0x00, 0x00
        };
        pico_ethernet_mcast_translate(f, pico_mcast_mac);
        memcpy(&dstmac, pico_mcast_mac, PICO_SIZE_ETH);
        dstmac_valid = 1;
    }

#if (defined PICO_SUPPORT_IPV4)
    else {
        struct pico_eth *arp_get;
        arp_get = pico_arp_get(f);
        if (arp_get) {
            memcpy(&dstmac, arp_get, PICO_SIZE_ETH);
            dstmac_valid = 1;
        } else {
            /* Enqueue copy of frame in ARP-module to retry later. Discard
             * frame otherwise we have a duplicate */
            pico_arp_postpone(f);
            return (int32_t)f->len;
        }
    }
#endif

    /* This sets destination and source address, then pushes the packet to the device. */
    if (dstmac_valid) {
        struct pico_eth_hdr *hdr;
        if (!eth_check_headroom(f)) {
            hdr = (struct pico_eth_hdr *) f->datalink_hdr;
            if ((f->start > f->buffer) && ((f->start - f->buffer) >= PICO_SIZE_ETHHDR))
            {
                f->start -= PICO_SIZE_ETHHDR;
                f->len += PICO_SIZE_ETHHDR;
                f->datalink_hdr = f->start;
                hdr = (struct pico_eth_hdr *) f->datalink_hdr;
                memcpy(hdr->saddr, f->dev->eth->mac.addr, PICO_SIZE_ETH);
                memcpy(hdr->daddr, &dstmac, PICO_SIZE_ETH);
                hdr->proto = proto;
            }

            if (pico_ethsend_local(f, hdr) || pico_ethsend_bcast(f) || pico_ethsend_dispatch(f)) {
                /* one of the above functions has delivered the frame accordingly.
                 * (returned != 0). It is safe to directly return successfully.
                 * Lower level queue has frame, so don't discard */
                return (int32_t)f->len;
            }
        }
    }

    /* Failure, frame could not be be enqueued in lower-level layer, safe
     * to discard since something clearly went wrong */
    pico_frame_discard(f);
    return 0;
}

#endif /* PICO_SUPPORT_ETH */


