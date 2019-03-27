/*********************************************************************
   PicoTCP. Copyright (c) 2012 TASS Belgium NV. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Authors: Kristof Roelants
 *********************************************************************/

#ifndef INCLUDE_PICO_DNS_CLIENT
#define INCLUDE_PICO_DNS_CLIENT

#define PICO_DNS_NS_DEL 0
#define PICO_DNS_NS_ADD 1
#include "pico_config.h"

/* Compression values */
#define PICO_DNS_LABEL 0
#define PICO_DNS_POINTER 3

/* Label len */
#define PICO_DNS_LABEL_INITIAL 1u
#define PICO_DNS_LABEL_ROOT 1

/* TTL values */
#define PICO_DNS_MAX_TTL 604800 /* one week */

/* Len of an IPv4 address string */
#define PICO_DNS_IPV4_ADDR_LEN 16
#define PICO_DNS_IPV6_ADDR_LEN 54

/* Default nameservers + port */
#define PICO_DNS_NS_DEFAULT "208.67.222.222"
#define PICO_DNS_NS_PORT 53

/* RDLENGTH for A and AAAA RR's */
#define PICO_DNS_RR_A_RDLENGTH 4
#define PICO_DNS_RR_AAAA_RDLENGTH 16

int pico_dns_client_init(void);
/* flag is PICO_DNS_NS_DEL or PICO_DNS_NS_ADD */
int pico_dns_client_nameserver(struct pico_ip4 *ns, uint8_t flag);
int pico_dns_client_getaddr(const char *url, void (*callback)(char *ip, void *arg), void *arg);
int pico_dns_client_getname(const char *ip, void (*callback)(char *url, void *arg), void *arg);
#ifdef PICO_SUPPORT_IPV6
int pico_dns_client_getaddr6(const char *url, void (*callback)(char *, void *), void *arg);
int pico_dns_client_getname6(const char *url, void (*callback)(char *, void *), void *arg);
#endif

#endif /* _INCLUDE_PICO_DNS_CLIENT */
