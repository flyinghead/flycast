/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.


   Authors: Frederik Van Slycken, Kristof Roelants
 *********************************************************************/

#include "pico_dhcp_server.h"
#include "pico_config.h"
#include "pico_addressing.h"
#include "pico_socket.h"
#include "pico_udp.h"
#include "pico_stack.h"
#include "pico_arp.h"

#if (defined PICO_SUPPORT_DHCPD && defined PICO_SUPPORT_UDP)

#ifdef DEBUG_DHCP_SERVER
    #define dhcps_dbg dbg
#else
    #define dhcps_dbg(...) do {} while(0)
#endif

/* default configurations */
#define DHCP_SERVER_OPENDNS    long_be(0xd043dede) /* OpenDNS DNS server 208.67.222.222 */
#define DHCP_SERVER_POOL_START long_be(0x00000064)
#define DHCP_SERVER_POOL_END   long_be(0x000000fe)
#define DHCP_SERVER_LEASE_TIME long_be(0x00000078)

/* maximum size of a DHCP message */
#define DHCP_SERVER_MAXMSGSIZE (PICO_IP_MRU - sizeof(struct pico_ipv4_hdr) - sizeof(struct pico_udp_hdr))

enum dhcp_server_state {
    PICO_DHCP_STATE_DISCOVER = 0,
    PICO_DHCP_STATE_OFFER,
    PICO_DHCP_STATE_REQUEST,
    PICO_DHCP_STATE_BOUND,
    PICO_DHCP_STATE_RENEWING
};

struct pico_dhcp_server_negotiation {
    uint32_t xid;
    enum dhcp_server_state state;
    struct pico_dhcp_server_setting *dhcps;
    struct pico_ip4 ciaddr;
    struct pico_eth hwaddr;
    uint8_t bcast;
};

static inline int ip_address_is_in_dhcp_range(struct pico_dhcp_server_negotiation *n, uint32_t x)
{
    uint32_t ip_hostendian = long_be(x);
    if (ip_hostendian < long_be(n->dhcps->pool_start))
        return 0;

    if (ip_hostendian > long_be(n->dhcps->pool_end))
        return 0;

    return 1;
}


static void pico_dhcpd_wakeup(uint16_t ev, struct pico_socket *s);

static int dhcp_settings_cmp(void *ka, void *kb)
{
    struct pico_dhcp_server_setting *a = ka, *b = kb;
    if (a->dev == b->dev)
        return 0;

    return (a->dev < b->dev) ? (-1) : (1);
}
static PICO_TREE_DECLARE(DHCPSettings, dhcp_settings_cmp);

static int dhcp_negotiations_cmp(void *ka, void *kb)
{
    struct pico_dhcp_server_negotiation *a = ka, *b = kb;
    if (a->xid == b->xid)
        return 0;

    return (a->xid < b->xid) ? (-1) : (1);
}
static PICO_TREE_DECLARE(DHCPNegotiations, dhcp_negotiations_cmp);


static inline void dhcps_set_default_pool_start_if_not_provided(struct pico_dhcp_server_setting *dhcps)
{
    if (!dhcps->pool_start)
        dhcps->pool_start = (dhcps->server_ip.addr & dhcps->netmask.addr) | DHCP_SERVER_POOL_START;
}

static inline void dhcps_set_default_pool_end_if_not_provided(struct pico_dhcp_server_setting *dhcps)
{
    if (!dhcps->pool_end)
        dhcps->pool_end = (dhcps->server_ip.addr & dhcps->netmask.addr) | DHCP_SERVER_POOL_END;
}
static inline void dhcps_set_default_lease_time_if_not_provided(struct pico_dhcp_server_setting *dhcps)
{
    if (!dhcps->lease_time)
        dhcps->lease_time = DHCP_SERVER_LEASE_TIME;
}

static inline struct pico_dhcp_server_setting *dhcps_try_open_socket(struct pico_dhcp_server_setting *dhcps)
{
    uint16_t port = PICO_DHCPD_PORT;
    dhcps->s = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_UDP, &pico_dhcpd_wakeup);
    if (!dhcps->s) {
        dhcps_dbg("DHCP server ERROR: failure opening socket (%s)\n", strerror(pico_err));
        PICO_FREE(dhcps);
        return NULL;
    }

    if (pico_socket_bind(dhcps->s, &dhcps->server_ip, &port) < 0) {
        dhcps_dbg("DHCP server ERROR: failure binding socket (%s)\n", strerror(pico_err));
        PICO_FREE(dhcps);
        return NULL;
    }

    if (pico_tree_insert(&DHCPSettings, dhcps)) {
    	dhcps_dbg("DHCP server ERROR: could not insert settings in tree\n");
		PICO_FREE(dhcps);
		return NULL;
    }

    return dhcps;
}

static struct pico_dhcp_server_setting *pico_dhcp_server_add_setting(struct pico_dhcp_server_setting *setting)
{
    struct pico_dhcp_server_setting *dhcps = NULL, *found = NULL, test = {
        0
    };
    struct pico_ipv4_link *link = NULL;

    link = pico_ipv4_link_get(&setting->server_ip);
    if (!link) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    test.dev = setting->dev;
    found = pico_tree_findKey(&DHCPSettings, &test);
    if (found) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    dhcps = PICO_ZALLOC(sizeof(struct pico_dhcp_server_setting));
    if (!dhcps) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    dhcps->lease_time = setting->lease_time;
    dhcps->pool_start = setting->pool_start;
    dhcps->pool_next = setting->pool_next;
    dhcps->pool_end = setting->pool_end;
    dhcps->dev = link->dev;
    dhcps->server_ip = link->address;
    dhcps->netmask = link->netmask;

    /* default values if not provided */
    dhcps_set_default_lease_time_if_not_provided(dhcps);
    dhcps_set_default_pool_end_if_not_provided(dhcps);
    dhcps_set_default_pool_start_if_not_provided(dhcps);

    dhcps->pool_next = dhcps->pool_start;

    return dhcps_try_open_socket(dhcps);

}

static struct pico_dhcp_server_negotiation *pico_dhcp_server_find_negotiation(uint32_t xid)
{
    struct pico_dhcp_server_negotiation test = {
        0
    }, *found = NULL;

    test.xid = xid;
    found = pico_tree_findKey(&DHCPNegotiations, &test);
    if (found)
        return found;
    else
        return NULL;
}

static inline void dhcp_negotiation_set_ciaddr(struct pico_dhcp_server_negotiation *dhcpn)
{
    struct pico_ip4 *ciaddr = NULL;
    ciaddr = pico_arp_reverse_lookup(&dhcpn->hwaddr);
    if (!ciaddr) {
        dhcpn->ciaddr.addr = dhcpn->dhcps->pool_next;
        dhcpn->dhcps->pool_next = long_be(long_be(dhcpn->dhcps->pool_next) + 1);
        pico_arp_create_entry(dhcpn->hwaddr.addr, dhcpn->ciaddr, dhcpn->dhcps->dev);
    } else {
        dhcpn->ciaddr = *ciaddr;
    }
}

static struct pico_dhcp_server_negotiation *pico_dhcp_server_add_negotiation(struct pico_device *dev, struct pico_dhcp_hdr *hdr)
{
    struct pico_dhcp_server_negotiation *dhcpn = NULL;
    struct pico_dhcp_server_setting test = {
        0
    };

    if (pico_dhcp_server_find_negotiation(hdr->xid))
        return NULL;

    dhcpn = PICO_ZALLOC(sizeof(struct pico_dhcp_server_negotiation));
    if (!dhcpn) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    dhcpn->xid = hdr->xid;
    dhcpn->state = PICO_DHCP_STATE_DISCOVER;
    dhcpn->bcast = ((short_be(hdr->flags) & PICO_DHCP_FLAG_BROADCAST) != 0) ? (1) : (0);
    memcpy(dhcpn->hwaddr.addr, hdr->hwaddr, PICO_SIZE_ETH);

    test.dev = dev;
    dhcpn->dhcps = pico_tree_findKey(&DHCPSettings, &test);
    if (!dhcpn->dhcps) {
        dhcps_dbg("DHCP server WARNING: received DHCP message on unconfigured link %s\n", dev->name);
        PICO_FREE(dhcpn);
        return NULL;
    }

    dhcp_negotiation_set_ciaddr(dhcpn);
    if (pico_tree_insert(&DHCPNegotiations, dhcpn)) {
		dhcps_dbg("DHCP server ERROR: could not insert negotiations in tree\n");
		PICO_FREE(dhcpn);
		return NULL;
	}

    return dhcpn;
}

static void dhcpd_make_reply(struct pico_dhcp_server_negotiation *dhcpn, uint8_t msg_type)
{
    int r = 0, optlen = 0, offset = 0;
    struct pico_ip4 broadcast = {
        0
    }, dns = {
        0
    }, destination = {
        .addr = 0xFFFFFFFF
    };
    struct pico_dhcp_hdr *hdr = NULL;

    dns.addr = DHCP_SERVER_OPENDNS;
    broadcast.addr = dhcpn->dhcps->server_ip.addr | ~(dhcpn->dhcps->netmask.addr);

    optlen = PICO_DHCP_OPTLEN_MSGTYPE + PICO_DHCP_OPTLEN_SERVERID + PICO_DHCP_OPTLEN_LEASETIME + PICO_DHCP_OPTLEN_NETMASK + PICO_DHCP_OPTLEN_ROUTER
             + PICO_DHCP_OPTLEN_BROADCAST + PICO_DHCP_OPTLEN_DNS + PICO_DHCP_OPTLEN_END;
    hdr = PICO_ZALLOC(sizeof(struct pico_dhcp_hdr) + (uint32_t)optlen);
    if (!hdr) {
        return;
    }

    hdr->op = PICO_DHCP_OP_REPLY;
    hdr->htype = PICO_DHCP_HTYPE_ETH;
    hdr->hlen = PICO_SIZE_ETH;
    hdr->xid = dhcpn->xid;
    hdr->yiaddr = dhcpn->ciaddr.addr;
    hdr->siaddr = dhcpn->dhcps->server_ip.addr;
    hdr->dhcp_magic = PICO_DHCPD_MAGIC_COOKIE;
    memcpy(hdr->hwaddr, dhcpn->hwaddr.addr, PICO_SIZE_ETH);

    /* options */
    offset += pico_dhcp_opt_msgtype(DHCP_OPT(hdr, offset), msg_type);
    offset += pico_dhcp_opt_serverid(DHCP_OPT(hdr, offset), &dhcpn->dhcps->server_ip);
    offset += pico_dhcp_opt_leasetime(DHCP_OPT(hdr, offset), dhcpn->dhcps->lease_time);
    offset += pico_dhcp_opt_netmask(DHCP_OPT(hdr, offset), &dhcpn->dhcps->netmask);
    offset += pico_dhcp_opt_router(DHCP_OPT(hdr, offset), &dhcpn->dhcps->server_ip);
    offset += pico_dhcp_opt_broadcast(DHCP_OPT(hdr, offset), &broadcast);
    offset += pico_dhcp_opt_dns(DHCP_OPT(hdr, offset), &dns);
    offset += pico_dhcp_opt_end(DHCP_OPT(hdr, offset));

    if (dhcpn->bcast == 0)
        destination.addr = hdr->yiaddr;
    else {
        hdr->flags |= short_be(PICO_DHCP_FLAG_BROADCAST);
        destination.addr = broadcast.addr;
    }

    r = pico_socket_sendto(dhcpn->dhcps->s, hdr, (int)(sizeof(struct pico_dhcp_hdr) + (uint32_t)optlen), &destination, PICO_DHCP_CLIENT_PORT);
    if (r < 0)
        dhcps_dbg("DHCP server WARNING: failure sending: %s!\n", strerror(pico_err));

    PICO_FREE(hdr);

    return;
}

static inline void parse_opt_msgtype(struct pico_dhcp_opt *opt, uint8_t *msgtype)
{
    if (opt->code == PICO_DHCP_OPT_MSGTYPE) {
        *msgtype = opt->ext.msg_type.type;
        dhcps_dbg("DHCP server: message type %u\n", *msgtype);
    }
}

static inline void parse_opt_reqip(struct pico_dhcp_opt *opt, struct pico_ip4 *reqip)
{
    if (opt->code == PICO_DHCP_OPT_REQIP)
        reqip->addr = opt->ext.req_ip.ip.addr;
}

static inline void parse_opt_serverid(struct pico_dhcp_opt *opt,  struct pico_ip4 *serverid)
{
    if (opt->code == PICO_DHCP_OPT_SERVERID)
        *serverid = opt->ext.server_id.ip;
}

static inline void dhcps_make_reply_to_request_msg(struct pico_dhcp_server_negotiation *dhcpn, int bound_valid_flag)
{
    if ((dhcpn->state == PICO_DHCP_STATE_BOUND) && bound_valid_flag)
        dhcpd_make_reply(dhcpn, PICO_DHCP_MSG_ACK);

    if (dhcpn->state == PICO_DHCP_STATE_OFFER) {
        dhcpn->state = PICO_DHCP_STATE_BOUND;
        dhcpd_make_reply(dhcpn, PICO_DHCP_MSG_ACK);
    }
}

static inline void dhcps_make_reply_to_discover_or_request(struct pico_dhcp_server_negotiation *dhcpn,  uint8_t msgtype, int bound_valid_flag)
{
    if (PICO_DHCP_MSG_DISCOVER == msgtype) {
        dhcpd_make_reply(dhcpn, PICO_DHCP_MSG_OFFER);
        dhcpn->state = PICO_DHCP_STATE_OFFER;
    } else if (PICO_DHCP_MSG_REQUEST == msgtype) {
        dhcps_make_reply_to_request_msg(dhcpn, bound_valid_flag);
    }
}

static inline void dhcps_parse_options_loop(struct pico_dhcp_server_negotiation *dhcpn, struct pico_dhcp_hdr *hdr)
{
    struct pico_dhcp_opt *opt = DHCP_OPT(hdr, 0);
    uint8_t msgtype = 0;
    struct pico_ip4 reqip = {
        0
    }, server_id = {
        0
    };

    do {
        parse_opt_msgtype(opt, &msgtype);
        parse_opt_reqip(opt, &reqip);
        parse_opt_serverid(opt, &server_id);
    } while (pico_dhcp_next_option(&opt));
    dhcps_make_reply_to_discover_or_request(dhcpn, msgtype, (!reqip.addr) && (!server_id.addr) && (hdr->ciaddr == dhcpn->ciaddr.addr));
}

static void pico_dhcp_server_recv(struct pico_socket *s, uint8_t *buf, uint32_t len)
{
    int32_t optlen = (int32_t)(len - sizeof(struct pico_dhcp_hdr));
    struct pico_dhcp_hdr *hdr = (struct pico_dhcp_hdr *)buf;
    struct pico_dhcp_server_negotiation *dhcpn = NULL;
    struct pico_device *dev = NULL;

    if (!pico_dhcp_are_options_valid(DHCP_OPT(hdr, 0), optlen))
        return;

    dev = pico_ipv4_link_find(&s->local_addr.ip4);
    dhcpn = pico_dhcp_server_find_negotiation(hdr->xid);
    if (!dhcpn)
        dhcpn = pico_dhcp_server_add_negotiation(dev, hdr);

    if (!ip_address_is_in_dhcp_range(dhcpn, dhcpn->ciaddr.addr))
        return;

    dhcps_parse_options_loop(dhcpn, hdr);

}

static void pico_dhcpd_wakeup(uint16_t ev, struct pico_socket *s)
{
    uint8_t buf[DHCP_SERVER_MAXMSGSIZE] = {
        0
    };
    int r = 0;

    if (ev != PICO_SOCK_EV_RD)
        return;

    r = pico_socket_recvfrom(s, buf, DHCP_SERVER_MAXMSGSIZE, NULL, NULL);
    if (r < 0)
        return;

    pico_dhcp_server_recv(s, buf, (uint32_t)r);
    return;
}

int pico_dhcp_server_initiate(struct pico_dhcp_server_setting *setting)
{
    if (!setting || !setting->server_ip.addr) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if (pico_dhcp_server_add_setting(setting) == NULL)
        return -1;

    return 0;
}

int pico_dhcp_server_destroy(struct pico_device *dev)
{
    struct pico_dhcp_server_setting *found, test = {
        0
    };
    test.dev = dev;
    found = pico_tree_findKey(&DHCPSettings, &test);
    if (!found) {
        pico_err = PICO_ERR_ENOENT;
        return -1;
    }

    pico_tree_delete(&DHCPSettings, found);
    PICO_FREE(found);
    return 0;
}

#endif /* PICO_SUPPORT_DHCP */
