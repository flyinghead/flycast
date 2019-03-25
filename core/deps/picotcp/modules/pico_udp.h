/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

 *********************************************************************/
#ifndef INCLUDE_PICO_UDP
#define INCLUDE_PICO_UDP
#include "pico_addressing.h"
#include "pico_protocol.h"
#include "pico_socket.h"
#define PICO_UDP_MODE_UNICAST 0x01
#define PICO_UDP_MODE_MULTICAST 0x02
#define PICO_UDP_MODE_BROADCAST 0xFF

struct pico_socket_udp
{
    struct pico_socket sock;
    int mode;
    uint8_t mc_ttl; /* Multicasting TTL */
};


extern struct pico_protocol pico_proto_udp;

PACKED_STRUCT_DEF pico_udp_hdr {
    struct pico_trans trans;
    uint16_t len;
    uint16_t crc;
};
#define PICO_UDPHDR_SIZE 8

struct pico_socket *pico_udp_open(void);
uint16_t pico_udp_recv(struct pico_socket *s, void *buf, uint16_t len, void *src, uint16_t *port, struct pico_msginfo *msginfo);
uint16_t pico_udp_checksum_ipv4(struct pico_frame *f);

#ifdef PICO_SUPPORT_IPV6
uint16_t pico_udp_checksum_ipv6(struct pico_frame *f);
#endif


int pico_udp_setsockopt(struct pico_socket *s, int option, void *value);

#endif
