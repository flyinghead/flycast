/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_DHCP_SERVER
#define INCLUDE_PICO_DHCP_SERVER
#include "pico_defines.h"
#ifdef PICO_SUPPORT_UDP

#include "pico_dhcp_common.h"
#include "pico_addressing.h"

struct pico_dhcp_server_setting
{
    uint32_t pool_start;
    uint32_t pool_next;
    uint32_t pool_end;
    uint32_t lease_time;
    struct pico_device *dev;
    struct pico_socket *s;
    struct pico_ip4 server_ip;
    struct pico_ip4 netmask;
    uint8_t flags; /* unused atm */
};

/* required field: IP address of the interface to serve, only IPs of this network will be served. */
int pico_dhcp_server_initiate(struct pico_dhcp_server_setting *dhcps);

/* To destroy an existing DHCP server configuration, running on a given interface */
int pico_dhcp_server_destroy(struct pico_device *dev);

#endif /* _INCLUDE_PICO_DHCP_SERVER */
#endif
