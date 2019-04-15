/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef PICO_MODULE_IPV4_H
#define PICO_MODULE_IPV4_H

struct pico_arp_entry {
    struct eth dest;
#ifdef PICO_CONFIG_IPV4
    struct ipv4 addr_ipv4;
#endif
    RB_ENTRY(pico_arp_entry) node;
};

/* Configured device */
struct pico_eth_link {
    struct pico_device *dev;
    struct eth address;
    struct eth netmask;
    RB_ENTRY(pico_eth_link) node;
};

#ifndef IS_MODULE_ETH
# define _mod extern
#else
# define _mod
#endif
_mod struct pico_module pico_module_eth;
#undef _mod

#endif
