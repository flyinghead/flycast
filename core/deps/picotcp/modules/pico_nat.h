/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Authors: Kristof Roelants, Simon Maes, Brecht Van Cauwenberghe
 *********************************************************************/

#ifndef INCLUDE_PICO_NAT
#define INCLUDE_PICO_NAT
#include "pico_frame.h"

#define PICO_NAT_PORT_FORWARD_DEL 0
#define PICO_NAT_PORT_FORWARD_ADD 1

#ifdef PICO_SUPPORT_NAT
void pico_ipv4_nat_print_table(void);
int pico_ipv4_nat_find(uint16_t nat_port, struct pico_ip4 *src_addr, uint16_t src_port, uint8_t proto);
int pico_ipv4_port_forward(struct pico_ip4 nat_addr, uint16_t nat_port, struct pico_ip4 src_addr, uint16_t src_port, uint8_t proto, uint8_t flag);

int pico_ipv4_nat_inbound(struct pico_frame *f, struct pico_ip4 *link_addr);
int pico_ipv4_nat_outbound(struct pico_frame *f, struct pico_ip4 *link_addr);
int pico_ipv4_nat_enable(struct pico_ipv4_link *link);
int pico_ipv4_nat_disable(void);
int pico_ipv4_nat_is_enabled(struct pico_ip4 *link_addr);
#else

#define pico_ipv4_nat_print_table() do {} while(0)
static inline int pico_ipv4_nat_inbound(struct pico_frame *f, struct pico_ip4 *link_addr)
{
    (void)f;
    (void)link_addr;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

static inline int pico_ipv4_nat_outbound(struct pico_frame *f, struct pico_ip4 *link_addr)
{
    (void)f;
    (void)link_addr;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

static inline int pico_ipv4_nat_enable(struct pico_ipv4_link *link)
{
    (void)link;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

static inline int pico_ipv4_nat_disable(void)
{
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

static inline int pico_ipv4_nat_is_enabled(struct pico_ip4 *link_addr)
{
    (void)link_addr;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

static inline int pico_ipv4_nat_find(uint16_t nat_port, struct pico_ip4 *src_addr, uint16_t src_port, uint8_t proto)
{
    (void)nat_port;
    (void)src_addr;
    (void)src_port;
    (void)proto;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

static inline int pico_ipv4_port_forward(struct pico_ip4 nat_addr, uint16_t nat_port, struct pico_ip4 src_addr, uint16_t src_port, uint8_t proto, uint8_t flag)
{
    (void)nat_addr;
    (void)nat_port;
    (void)src_addr;
    (void)src_port;
    (void)proto;
    (void)flag;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}
#endif

#endif /* _INCLUDE_PICO_NAT */

