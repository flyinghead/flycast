/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

 *********************************************************************/
#ifndef INCLUDE_PICO_DHCP_CLIENT
#define INCLUDE_PICO_DHCP_CLIENT
#include "pico_defines.h"
#ifdef PICO_SUPPORT_UDP
#include "pico_dhcp_common.h"
#include "pico_addressing.h"
#include "pico_protocol.h"

int pico_dhcp_initiate_negotiation(struct pico_device *device, void (*callback)(void*cli, int code), uint32_t *xid);
void *pico_dhcp_get_identifier(uint32_t xid);
struct pico_ip4 pico_dhcp_get_address(void *cli);
struct pico_ip4 pico_dhcp_get_gateway(void *cli);
struct pico_ip4 pico_dhcp_get_netmask(void *cli);
struct pico_ip4 pico_dhcp_get_nameserver(void*cli, int index);
int pico_dhcp_client_abort(uint32_t xid);
char *pico_dhcp_get_hostname(void);
char *pico_dhcp_get_domain(void);

/* possible codes for the callback */
#define PICO_DHCP_SUCCESS 0
#define PICO_DHCP_ERROR   1
#define PICO_DHCP_RESET   2

#endif
#endif
