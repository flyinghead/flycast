/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Authors: Daniele Lacamera
 *********************************************************************/


#include "pico_udp.h"
#include "pico_config.h"
#include "pico_eth.h"
#include "pico_socket.h"
#include "pico_stack.h"

#ifdef DEBUG_UDP
#define udp_dbg dbg
#else
#define udp_dbg(...) do {} while(0)
#endif

#define UDP_FRAME_OVERHEAD (sizeof(struct pico_frame))

/* Queues */
static struct pico_queue udp_in = {
    0
};
static struct pico_queue udp_out = {
    0
};


/* Functions */

uint16_t pico_udp_checksum_ipv4(struct pico_frame *f)
{
    struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *) f->net_hdr;
    struct pico_udp_hdr *udp_hdr = (struct pico_udp_hdr *) f->transport_hdr;
    struct pico_socket *s = f->sock;
    struct pico_ipv4_pseudo_hdr pseudo;

    if (s) {
        /* Case of outgoing frame */
        udp_dbg("UDP CRC: on outgoing frame\n");
        pseudo.src.addr = s->local_addr.ip4.addr;
        pseudo.dst.addr = s->remote_addr.ip4.addr;
    } else {
        /* Case of incomming frame */
        udp_dbg("UDP CRC: on incomming frame\n");
        pseudo.src.addr = hdr->src.addr;
        pseudo.dst.addr = hdr->dst.addr;
    }

    pseudo.zeros = 0;
    pseudo.proto = PICO_PROTO_UDP;
    pseudo.len = short_be(f->transport_len);

    return pico_dualbuffer_checksum(&pseudo, sizeof(struct pico_ipv4_pseudo_hdr), udp_hdr, f->transport_len);
}

#ifdef PICO_SUPPORT_IPV6
uint16_t pico_udp_checksum_ipv6(struct pico_frame *f)
{
    struct pico_ipv6_hdr *ipv6_hdr = (struct pico_ipv6_hdr *)f->net_hdr;
    struct pico_udp_hdr *udp_hdr = (struct pico_udp_hdr *)f->transport_hdr;
    struct pico_ipv6_pseudo_hdr pseudo = {
        .src = {{0}}, .dst = {{0}}, .len = 0, .zero = {0}, .nxthdr = 0
    };
    struct pico_socket *s = f->sock;
    struct pico_remote_endpoint *remote_endpoint = (struct pico_remote_endpoint *)f->info;

    /* XXX If the IPv6 packet contains a Routing header, the Destination
     *     Address used in the pseudo-header is that of the final destination */
    if (s) {
        /* Case of outgoing frame */
        pseudo.src = s->local_addr.ip6;
        if (remote_endpoint)
            pseudo.dst = remote_endpoint->remote_addr.ip6;
        else
            pseudo.dst = s->remote_addr.ip6;
    } else {
        /* Case of incomming frame */
        pseudo.src = ipv6_hdr->src;
        pseudo.dst = ipv6_hdr->dst;
    }

    pseudo.len = long_be(f->transport_len);
    pseudo.nxthdr = PICO_PROTO_UDP;

    return pico_dualbuffer_checksum(&pseudo, sizeof(struct pico_ipv6_pseudo_hdr), udp_hdr, f->transport_len);
}
#endif



static int pico_udp_process_out(struct pico_protocol *self, struct pico_frame *f)
{
    IGNORE_PARAMETER(self);
    return (int)pico_network_send(f);
}

static int pico_udp_push(struct pico_protocol *self, struct pico_frame *f)
{
    struct pico_udp_hdr *hdr = (struct pico_udp_hdr *) f->transport_hdr;
    struct pico_remote_endpoint *remote_endpoint = (struct pico_remote_endpoint *) f->info;

    /* this (fragmented) frame should contain a transport header */
    if (f->transport_hdr != f->payload) {
        hdr->trans.sport = f->sock->local_port;
        if (remote_endpoint) {
            hdr->trans.dport = remote_endpoint->remote_port;
        } else {
            hdr->trans.dport = f->sock->remote_port;
        }

        hdr->len = short_be(f->transport_len);

        /* do not perform CRC validation. If you want to, a system needs to be
           implemented to calculate the CRC over the total payload of a
           fragmented payload
         */
        hdr->crc = 0;
    }

    if (pico_enqueue(self->q_out, f) > 0) {
        return f->payload_len;
    } else {
        return 0;
    }
}

/* Interface: protocol definition */
struct pico_protocol pico_proto_udp = {
    .name = "udp",
    .proto_number = PICO_PROTO_UDP,
    .layer = PICO_LAYER_TRANSPORT,
    .process_in = pico_transport_process_in,
    .process_out = pico_udp_process_out,
    .push = pico_udp_push,
    .q_in = &udp_in,
    .q_out = &udp_out,
};



struct pico_socket *pico_udp_open(void)
{
    struct pico_socket_udp *u = PICO_ZALLOC(sizeof(struct pico_socket_udp));
    if (!u)
        return NULL;

    u->mode = PICO_UDP_MODE_UNICAST;

#ifdef PICO_SUPPORT_MCAST
    u->mc_ttl = PICO_IP_DEFAULT_MULTICAST_TTL;
    /* enable multicast loopback by default */
    u->sock.opt_flags |= (1 << PICO_SOCKET_OPT_MULTICAST_LOOP);
#endif

    return &u->sock;
}

static void pico_udp_get_msginfo(struct pico_frame *f, struct pico_msginfo *msginfo)
{
    if (!msginfo || !f->net_hdr)
        return;

    msginfo->dev = f->dev;

    if (IS_IPV4(f)) { /* IPV4 */
#ifdef PICO_SUPPORT_IPV4
        struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *)(f->net_hdr);
        msginfo->ttl = hdr->ttl;
        msginfo->tos = hdr->tos;
#endif
    } else {
#ifdef PICO_SUPPORT_IPV6
        struct pico_ipv6_hdr *hdr = (struct pico_ipv6_hdr *)(f->net_hdr);
        msginfo->ttl = hdr->hop;
        msginfo->tos = (hdr->vtf >> 20) & 0xFF; /* IPv6 traffic class */
#endif
    }
}

uint16_t pico_udp_recv(struct pico_socket *s, void *buf, uint16_t len, void *src, uint16_t *port, struct pico_msginfo *msginfo)
{
    struct pico_frame *f = pico_queue_peek(&s->q_in);
    if (f) {
        if(!f->payload_len) {
            f->payload = f->transport_hdr + sizeof(struct pico_udp_hdr);
            f->payload_len = (uint16_t)(f->transport_len - sizeof(struct pico_udp_hdr));
        }

        udp_dbg("expected: %d, got: %d\n", len, f->payload_len);
        if (src)
            pico_store_network_origin(src, f);

        if (port) {
            struct pico_trans *hdr = (struct pico_trans *)f->transport_hdr;
            *port = hdr->sport;
        }

        if (msginfo) {
            pico_udp_get_msginfo(f, msginfo);
        }

        if (f->payload_len > len) {
            memcpy(buf, f->payload, len);
            f->payload += len;
            f->payload_len = (uint16_t)(f->payload_len - len);
            return len;
        } else {
            uint16_t ret = f->payload_len;
            memcpy(buf, f->payload, f->payload_len);
            f = pico_dequeue(&s->q_in);
            pico_frame_discard(f);
            return ret;
        }
    } else return 0;
}

