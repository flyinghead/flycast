/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Kristof Roelants, Frederik Van Slycken, Maxime Vincent
 *********************************************************************/


#include "pico_dhcp_client.h"
#include "pico_stack.h"
#include "pico_config.h"
#include "pico_device.h"
#include "pico_ipv4.h"
#include "pico_socket.h"
#include "pico_eth.h"

#if (defined PICO_SUPPORT_DHCPC && defined PICO_SUPPORT_UDP)

#ifdef DEBUG_DHCP_CLIENT
    #define dhcpc_dbg dbg
#else
    #define dhcpc_dbg(...) do {} while(0)
#endif

/* timer values */
#define DHCP_CLIENT_REINIT             6000 /* msec */
#define DHCP_CLIENT_RETRANS            4 /* sec */
#define DHCP_CLIENT_RETRIES            3

#define DHCP_CLIENT_TIMER_STOPPED      0
#define DHCP_CLIENT_TIMER_STARTED      1

/* maximum size of a DHCP message */
#define DHCP_CLIENT_MAXMSGZISE         (PICO_IP_MRU - PICO_SIZE_IP4HDR)
#define PICO_DHCP_HOSTNAME_MAXLEN  64U

/* Mockables */
#if defined UNIT_TEST
#   define MOCKABLE __attribute__((weak))
#else
#   define MOCKABLE
#endif

static char dhcpc_host_name[PICO_DHCP_HOSTNAME_MAXLEN] = "";
static char dhcpc_domain_name[PICO_DHCP_HOSTNAME_MAXLEN] = "";


enum dhcp_client_state {
    DHCP_CLIENT_STATE_INIT_REBOOT = 0,
    DHCP_CLIENT_STATE_REBOOTING,
    DHCP_CLIENT_STATE_INIT,
    DHCP_CLIENT_STATE_SELECTING,
    DHCP_CLIENT_STATE_REQUESTING,
    DHCP_CLIENT_STATE_BOUND,
    DHCP_CLIENT_STATE_RENEWING,
    DHCP_CLIENT_STATE_REBINDING
};


#define PICO_DHCPC_TIMER_INIT    0
#define PICO_DHCPC_TIMER_REQUEST 1
#define PICO_DHCPC_TIMER_RENEW   2
#define PICO_DHCPC_TIMER_REBIND  3
#define PICO_DHCPC_TIMER_T1      4
#define PICO_DHCPC_TIMER_T2      5
#define PICO_DHCPC_TIMER_LEASE   6
#define PICO_DHCPC_TIMER_ARRAY_SIZE 7

struct dhcp_client_timer
{
    uint8_t state;
    unsigned int type;
    uint32_t xid;
    uint32_t timer_id;
};

struct pico_dhcp_client_cookie
{
    uint8_t event;
    uint8_t retry;
    uint32_t xid;
    uint32_t *uid;
    enum dhcp_client_state state;
    void (*cb)(void*dhcpc, int code);
    pico_time init_timestamp;
    struct pico_socket *s;
    struct pico_ip4 address;
    struct pico_ip4 netmask;
    struct pico_ip4 gateway;
    struct pico_ip4 nameserver[2];
    struct pico_ip4 server_id;
    struct pico_device *dev;
    struct dhcp_client_timer *timer[PICO_DHCPC_TIMER_ARRAY_SIZE];
    uint32_t t1_time;
    uint32_t t2_time;
    uint32_t lease_time;
    uint32_t renew_time;
    uint32_t rebind_time;
};

static int pico_dhcp_client_init(struct pico_dhcp_client_cookie *dhcpc);
static int reset(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
static int8_t pico_dhcp_client_msg(struct pico_dhcp_client_cookie *dhcpc, uint8_t msg_type);
static void pico_dhcp_client_wakeup(uint16_t ev, struct pico_socket *s);
static void pico_dhcp_state_machine(uint8_t event, struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
static void pico_dhcp_client_callback(struct pico_dhcp_client_cookie *dhcpc, int code);

static const struct pico_ip4 bcast_netmask = {
    .addr = 0xFFFFFFFF
};

static struct pico_ip4 inaddr_any = {
    0
};


static int dhcp_cookies_cmp(void *ka, void *kb)
{
    struct pico_dhcp_client_cookie *a = ka, *b = kb;
    if (a->xid == b->xid)
        return 0;

    return (a->xid < b->xid) ? (-1) : (1);
}
static PICO_TREE_DECLARE(DHCPCookies, dhcp_cookies_cmp);

static struct pico_dhcp_client_cookie *pico_dhcp_client_add_cookie(uint32_t xid, struct pico_device *dev, void (*cb)(void *dhcpc, int code), uint32_t *uid)
{
    struct pico_dhcp_client_cookie *dhcpc = NULL, *found = NULL, test = {
        0
    };

    test.xid = xid;
    found = pico_tree_findKey(&DHCPCookies, &test);
    if (found) {
        pico_err = PICO_ERR_EAGAIN;
        return NULL;
    }

    dhcpc = PICO_ZALLOC(sizeof(struct pico_dhcp_client_cookie));
    if (!dhcpc) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    dhcpc->state = DHCP_CLIENT_STATE_INIT;
    dhcpc->xid = xid;
    dhcpc->uid = uid;
    *(dhcpc->uid) = 0;
    dhcpc->cb = cb;
    dhcpc->dev = dev;

    if (pico_tree_insert(&DHCPCookies, dhcpc)) {
		PICO_FREE(dhcpc);
		return NULL;
    }

    return dhcpc;
}

static void pico_dhcp_client_stop_timers(struct pico_dhcp_client_cookie *dhcpc);
static int pico_dhcp_client_del_cookie(uint32_t xid)
{
    struct pico_dhcp_client_cookie test = {
        0
    }, *found = NULL;

    test.xid = xid;
    found = pico_tree_findKey(&DHCPCookies, &test);
    if (!found)
        return -1;

    pico_dhcp_client_stop_timers(found);
    pico_socket_close(found->s);
    found->s = NULL;
    pico_ipv4_link_del(found->dev, found->address);
    pico_tree_delete(&DHCPCookies, found);
    PICO_FREE(found);
    return 0;
}

static struct pico_dhcp_client_cookie *pico_dhcp_client_find_cookie(uint32_t xid)
{
    struct pico_dhcp_client_cookie test = {
        0
    }, *found = NULL;

    test.xid = xid;
    found = pico_tree_findKey(&DHCPCookies, &test);
    if (found)
        return found;
    else
        return NULL;
}

static void pico_dhcp_client_timer_handler(pico_time now, void *arg);
static void pico_dhcp_client_reinit(pico_time now, void *arg);
static struct dhcp_client_timer *pico_dhcp_timer_add(uint8_t type, uint32_t time, struct pico_dhcp_client_cookie *ck)
{
    struct dhcp_client_timer *t = ck->timer[type];

    if (t) {
        /* Stale timer, mark to be freed in the callback */
        t->state = DHCP_CLIENT_TIMER_STOPPED;
    }

    /* allocate a new timer, the old one is still in the timer tree, and will be freed as soon as it expires */
    t = PICO_ZALLOC(sizeof(struct dhcp_client_timer));

    if (!t)
        return NULL;

    t->state = DHCP_CLIENT_TIMER_STARTED;
    t->xid = ck->xid;
    t->type = type;
    t->timer_id = pico_timer_add(time, pico_dhcp_client_timer_handler, t);
    if (!t->timer_id) {
        dhcpc_dbg("DHCP: Failed to start timer\n");
        PICO_FREE(t);
        return NULL;
    }

    /* store timer struct reference in cookie */
    ck->timer[type] = t;
    return t;
}

static int dhcp_get_timer_event(struct pico_dhcp_client_cookie *dhcpc, unsigned int type)
{
    const int events[PICO_DHCPC_TIMER_ARRAY_SIZE] =
    {
        PICO_DHCP_EVENT_RETRANSMIT,
        PICO_DHCP_EVENT_RETRANSMIT,
        PICO_DHCP_EVENT_RETRANSMIT,
        PICO_DHCP_EVENT_RETRANSMIT,
        PICO_DHCP_EVENT_T1,
        PICO_DHCP_EVENT_T2,
        PICO_DHCP_EVENT_LEASE
    };

    if (type == PICO_DHCPC_TIMER_REQUEST) {
        if (++dhcpc->retry > DHCP_CLIENT_RETRIES) {
            reset(dhcpc, NULL);
            return PICO_DHCP_EVENT_NONE;
        }
    } else if (type < PICO_DHCPC_TIMER_T1) {
        dhcpc->retry++;
    }

    return events[type];
}

static void pico_dhcp_client_timer_handler(pico_time now, void *arg)
{
    struct dhcp_client_timer *t = (struct dhcp_client_timer *)arg;
    struct pico_dhcp_client_cookie *dhcpc;

    if (!t)
        return;

    (void) now;
    if (t->state != DHCP_CLIENT_TIMER_STOPPED) {
        dhcpc = pico_dhcp_client_find_cookie(t->xid);
        if (dhcpc) {
            t->state = DHCP_CLIENT_TIMER_STOPPED;
            if ((t->type == PICO_DHCPC_TIMER_INIT) && (dhcpc->state < DHCP_CLIENT_STATE_SELECTING)) {
                /* this was an INIT timer */
                pico_dhcp_client_reinit(now, dhcpc);
            } else if (t->type != PICO_DHCPC_TIMER_INIT) {
                /* this was NOT an INIT timer */
                dhcpc->event = (uint8_t)dhcp_get_timer_event(dhcpc, t->type);
                if (dhcpc->event != PICO_DHCP_EVENT_NONE)
                    pico_dhcp_state_machine(dhcpc->event, dhcpc, NULL);
            }
        }
    }

    /* stale timer, it's associated struct should be freed */
    if (t->state == DHCP_CLIENT_TIMER_STOPPED)
        PICO_FREE(t);
}

static void pico_dhcp_client_reinit(pico_time now, void *arg)
{
    struct pico_dhcp_client_cookie *dhcpc = (struct pico_dhcp_client_cookie *)arg;
    (void) now;

    if (dhcpc->s) {
        pico_socket_close(dhcpc->s);
        dhcpc->s = NULL;
    }

    if (++dhcpc->retry > DHCP_CLIENT_RETRIES) {
        pico_err = PICO_ERR_EAGAIN;
        pico_dhcp_client_callback(dhcpc, PICO_DHCP_ERROR);

        pico_dhcp_client_del_cookie(dhcpc->xid);
        return;
    }

    pico_dhcp_client_init(dhcpc);
    return;
}


static void pico_dhcp_client_stop_timers(struct pico_dhcp_client_cookie *dhcpc)
{
    int i;
    dhcpc->retry = 0;
    for (i = 0; i < PICO_DHCPC_TIMER_ARRAY_SIZE; i++)
    {
        if (dhcpc->timer[i]) {
            /* Do not cancel timer, but rather set it's state to be freed when it expires */
            dhcpc->timer[i]->state = DHCP_CLIENT_TIMER_STOPPED;
            dhcpc->timer[i] = NULL;
        }
    }
}

static int pico_dhcp_client_start_init_timer(struct pico_dhcp_client_cookie *dhcpc)
{
    uint32_t time = 0;
    /* timer value is doubled with every retry (exponential backoff) */
    time = (uint32_t) (DHCP_CLIENT_RETRANS << dhcpc->retry);
    if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_INIT, time * 1000, dhcpc))
        return -1;

    return 0;
}

static int pico_dhcp_client_start_requesting_timer(struct pico_dhcp_client_cookie *dhcpc)
{
    uint32_t time = 0;

    /* timer value is doubled with every retry (exponential backoff) */
    time = (uint32_t)(DHCP_CLIENT_RETRANS << dhcpc->retry);
    if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_REQUEST, time * 1000, dhcpc))
        return -1;

    return 0;
}

static int pico_dhcp_client_start_renewing_timer(struct pico_dhcp_client_cookie *dhcpc)
{
    uint32_t halftime = 0;

    /* wait one-half of the remaining time until T2, down to a minimum of 60 seconds */
    /* (dhcpc->retry + 1): initial -> divide by 2, 1st retry -> divide by 4, 2nd retry -> divide by 8, etc */
    pico_dhcp_client_stop_timers(dhcpc);
    halftime = dhcpc->renew_time >> (dhcpc->retry + 1);
    if (halftime < 60)
        halftime = 60;

    if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_RENEW, halftime * 1000, dhcpc))
        return -1;

    return 0;
}

static int pico_dhcp_client_start_rebinding_timer(struct pico_dhcp_client_cookie *dhcpc)
{
    uint32_t halftime = 0;

    pico_dhcp_client_stop_timers(dhcpc);
    halftime = dhcpc->rebind_time >> (dhcpc->retry + 1);
    if (halftime < 60)
        halftime = 60;

    if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_REBIND, halftime * 1000, dhcpc))
        return -1;

    return 0;
}

static int pico_dhcp_client_start_reacquisition_timers(struct pico_dhcp_client_cookie *dhcpc)
{

    pico_dhcp_client_stop_timers(dhcpc);
    if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_T1, dhcpc->t1_time * 1000, dhcpc))
        goto fail;

    if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_T2, dhcpc->t2_time * 1000, dhcpc))
        goto fail;

    if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_LEASE, dhcpc->lease_time * 1000, dhcpc))
        goto fail;

    return 0;

fail:
    pico_dhcp_client_stop_timers(dhcpc);
    return -1;
}

static int pico_dhcp_client_init(struct pico_dhcp_client_cookie *dhcpc)
{
    uint16_t port = PICO_DHCP_CLIENT_PORT;
    if (!dhcpc)
        return -1;

    /* adding a link with address 0.0.0.0 and netmask 0.0.0.0,
     * automatically adds a route for a global broadcast */
    pico_ipv4_link_add(dhcpc->dev, inaddr_any, bcast_netmask);
    if (!dhcpc->s)
        dhcpc->s = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_UDP, &pico_dhcp_client_wakeup);

    if (!dhcpc->s) {
        if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_INIT, DHCP_CLIENT_REINIT, dhcpc))
            return -1;

        return 0;
    }

    dhcpc->s->dev = dhcpc->dev;
    if (pico_socket_bind(dhcpc->s, &inaddr_any, &port) < 0) {
        pico_socket_close(dhcpc->s);
        dhcpc->s = NULL;
        if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_INIT, DHCP_CLIENT_REINIT, dhcpc))
            return -1;

        return 0;
    }

    if (pico_dhcp_client_msg(dhcpc, PICO_DHCP_MSG_DISCOVER) < 0) {
        pico_socket_close(dhcpc->s);
        dhcpc->s = NULL;
        if (!pico_dhcp_timer_add(PICO_DHCPC_TIMER_INIT, DHCP_CLIENT_REINIT, dhcpc))
            return -1;

        return 0;
    }

    dhcpc->retry = 0;
    dhcpc->init_timestamp = PICO_TIME_MS();
    if (pico_dhcp_client_start_init_timer(dhcpc) < 0) {
        pico_socket_close(dhcpc->s);
        dhcpc->s = NULL;
        return -1;
    }

    return 0;
}

int MOCKABLE pico_dhcp_initiate_negotiation(struct pico_device *dev, void (*cb)(void *dhcpc, int code), uint32_t *uid)
{
    uint8_t retry = 32;
    uint32_t xid = 0;
    struct pico_dhcp_client_cookie *dhcpc = NULL;

    if (!dev || !cb || !uid) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if (!dev->eth) {
        pico_err = PICO_ERR_EOPNOTSUPP;
        return -1;
    }

    /* attempt to generate a correct xid, else fail */
    do {
        xid = pico_rand();
    } while (!xid && --retry);

    if (!xid) {
        pico_err = PICO_ERR_EAGAIN;
        return -1;
    }

    dhcpc = pico_dhcp_client_add_cookie(xid, dev, cb, uid);
    if (!dhcpc)
        return -1;

    dhcpc_dbg("DHCP client: cookie with xid %u\n", dhcpc->xid);
    *uid = xid;
    return pico_dhcp_client_init(dhcpc);
}

static void pico_dhcp_client_recv_params(struct pico_dhcp_client_cookie *dhcpc, struct pico_dhcp_opt *opt)
{
    do {
        switch (opt->code)
        {
        case PICO_DHCP_OPT_PAD:
            break;

        case PICO_DHCP_OPT_END:
            break;

        case PICO_DHCP_OPT_MSGTYPE:
            dhcpc->event = opt->ext.msg_type.type;
            dhcpc_dbg("DHCP client: message type %u\n", dhcpc->event);
            break;

        case PICO_DHCP_OPT_LEASETIME:
            dhcpc->lease_time = long_be(opt->ext.lease_time.time);
            dhcpc_dbg("DHCP client: lease time %u\n", dhcpc->lease_time);
            break;

        case PICO_DHCP_OPT_RENEWALTIME:
            dhcpc->t1_time = long_be(opt->ext.renewal_time.time);
            dhcpc_dbg("DHCP client: renewal time %u\n", dhcpc->t1_time);
            break;

        case PICO_DHCP_OPT_REBINDINGTIME:
            dhcpc->t2_time = long_be(opt->ext.rebinding_time.time);
            dhcpc_dbg("DHCP client: rebinding time %u\n", dhcpc->t2_time);
            break;

        case PICO_DHCP_OPT_ROUTER:
            dhcpc->gateway = opt->ext.router.ip;
            dhcpc_dbg("DHCP client: router %08X\n", dhcpc->gateway.addr);
            break;

        case PICO_DHCP_OPT_DNS:
            dhcpc->nameserver[0] = opt->ext.dns1.ip;
            dhcpc_dbg("DHCP client: dns1 %08X\n", dhcpc->nameserver[0].addr);
            if (opt->len >= 8) {
                dhcpc->nameserver[1] = opt->ext.dns2.ip;
                dhcpc_dbg("DHCP client: dns1 %08X\n", dhcpc->nameserver[1].addr);
            }

            break;

        case PICO_DHCP_OPT_NETMASK:
            dhcpc->netmask = opt->ext.netmask.ip;
            dhcpc_dbg("DHCP client: netmask %08X\n", dhcpc->netmask.addr);
            break;

        case PICO_DHCP_OPT_SERVERID:
            dhcpc->server_id = opt->ext.server_id.ip;
            dhcpc_dbg("DHCP client: server ID %08X\n", dhcpc->server_id.addr);
            break;

        case PICO_DHCP_OPT_OPTOVERLOAD:
            dhcpc_dbg("DHCP client: WARNING option overload present (not processed)");
            break;

        case PICO_DHCP_OPT_HOSTNAME:
        {
            uint32_t maxlen = PICO_DHCP_HOSTNAME_MAXLEN;
            if (opt->len < maxlen)
                maxlen = opt->len;

            strncpy(dhcpc_host_name, opt->ext.string.txt, maxlen);
        }
        break;

        case PICO_DHCP_OPT_DOMAINNAME:
        {
            uint32_t maxlen = PICO_DHCP_HOSTNAME_MAXLEN;
            if (opt->len < maxlen)
                maxlen = opt->len;

            strncpy(dhcpc_domain_name, opt->ext.string.txt, maxlen);
        }
        break;

        default:
            dhcpc_dbg("DHCP client: WARNING unsupported option %u\n", opt->code);
            break;
        }
    } while (pico_dhcp_next_option(&opt));

    /* default values for T1 and T2 when not provided */
    if (!dhcpc->t1_time)
        dhcpc->t1_time = dhcpc->lease_time >> 1;

    if (!dhcpc->t2_time)
        dhcpc->t2_time = (dhcpc->lease_time * 875) / 1000;

    return;
}

static int recv_offer(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf)
{
    struct pico_dhcp_hdr *hdr = (struct pico_dhcp_hdr *)buf;
    struct pico_dhcp_opt *opt = DHCP_OPT(hdr, 0);

    pico_dhcp_client_recv_params(dhcpc, opt);
    if ((dhcpc->event != PICO_DHCP_MSG_OFFER) || !dhcpc->server_id.addr || !dhcpc->netmask.addr || !dhcpc->lease_time)
        return -1;

    dhcpc->address.addr = hdr->yiaddr;

    /* we skip state SELECTING, process first offer received */
    dhcpc->state = DHCP_CLIENT_STATE_REQUESTING;
    dhcpc->retry = 0;
    pico_dhcp_client_msg(dhcpc, PICO_DHCP_MSG_REQUEST);
    if (pico_dhcp_client_start_requesting_timer(dhcpc) < 0)
        return -1;

    return 0;
}

static void pico_dhcp_client_update_link(struct pico_dhcp_client_cookie *dhcpc)
{
    struct pico_ip4 any_address = {
        0
    };
    struct pico_ip4 address = {
        0
    };
    struct pico_ipv4_link *l;

    dbg("DHCP client: update link\n");

    pico_ipv4_link_del(dhcpc->dev, address);
    l = pico_ipv4_link_by_dev(dhcpc->dev);
    while(l) {
        pico_ipv4_link_del(dhcpc->dev, l->address);
        l = pico_ipv4_link_by_dev_next(dhcpc->dev, l);
    }
    pico_ipv4_link_add(dhcpc->dev, dhcpc->address, dhcpc->netmask);

    /* If router option is received, use it as default gateway */
    if (dhcpc->gateway.addr != 0U) {
        pico_ipv4_route_add(any_address, any_address, dhcpc->gateway, 1, NULL);
    }
}

static int recv_ack(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf)
{
    struct pico_dhcp_hdr *hdr = (struct pico_dhcp_hdr *)buf;
    struct pico_dhcp_opt *opt = DHCP_OPT(hdr, 0);
    struct pico_ipv4_link *l;

    pico_dhcp_client_recv_params(dhcpc, opt);
    if ((dhcpc->event != PICO_DHCP_MSG_ACK) || !dhcpc->server_id.addr || !dhcpc->netmask.addr || !dhcpc->lease_time)
        return -1;

    /* Issue #20 the server can transmit on ACK a different IP than the one in OFFER */
    /* RFC2131 ch 4.3.2 ... The client SHOULD use the parameters in the DHCPACK message for configuration */
    if (dhcpc->state == DHCP_CLIENT_STATE_REQUESTING)
        dhcpc->address.addr = hdr->yiaddr;


    /* close the socket used for address (re)acquisition */
    pico_socket_close(dhcpc->s);
    dhcpc->s = NULL;

    /* Delete all the links before adding the new ip address
     * in case the new address doesn't match the old one */
    l = pico_ipv4_link_by_dev(dhcpc->dev);
    if (dhcpc->address.addr != (l->address).addr) {
        pico_dhcp_client_update_link(dhcpc);
    }

    dbg("DHCP client: renewal time (T1) %u\n", (unsigned int)dhcpc->t1_time);
    dbg("DHCP client: rebinding time (T2) %u\n", (unsigned int)dhcpc->t2_time);
    dbg("DHCP client: lease time %u\n", (unsigned int)dhcpc->lease_time);

    dhcpc->retry = 0;
    dhcpc->renew_time = dhcpc->t2_time - dhcpc->t1_time;
    dhcpc->rebind_time = dhcpc->lease_time - dhcpc->t2_time;
    if (pico_dhcp_client_start_reacquisition_timers(dhcpc) < 0) {
        pico_dhcp_client_callback(dhcpc, PICO_DHCP_ERROR);
        return -1;
    }


    *(dhcpc->uid) = dhcpc->xid;
    pico_dhcp_client_callback(dhcpc, PICO_DHCP_SUCCESS);

    dhcpc->state = DHCP_CLIENT_STATE_BOUND;
    return 0;
}

static int renew(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf)
{
    uint16_t port = PICO_DHCP_CLIENT_PORT;
    (void) buf;
    dhcpc->state = DHCP_CLIENT_STATE_RENEWING;
    dhcpc->s = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_UDP, &pico_dhcp_client_wakeup);
    if (!dhcpc->s) {
        dhcpc_dbg("DHCP client ERROR: failure opening socket on renew, aborting DHCP! (%s)\n", strerror(pico_err));
        pico_dhcp_client_callback(dhcpc, PICO_DHCP_ERROR);

        return -1;
    }

    if (pico_socket_bind(dhcpc->s, &dhcpc->address, &port) != 0) {
        dhcpc_dbg("DHCP client ERROR: failure binding socket on renew, aborting DHCP! (%s)\n", strerror(pico_err));
        pico_socket_close(dhcpc->s);
        dhcpc->s = NULL;
        pico_dhcp_client_callback(dhcpc, PICO_DHCP_ERROR);

        return -1;
    }

    dhcpc->retry = 0;
    pico_dhcp_client_msg(dhcpc, PICO_DHCP_MSG_REQUEST);
    if (pico_dhcp_client_start_renewing_timer(dhcpc) < 0) {
        pico_socket_close(dhcpc->s);
        dhcpc->s = NULL;
        pico_dhcp_client_callback(dhcpc, PICO_DHCP_ERROR);

        return -1;
    }

    return 0;
}

static int rebind(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf)
{
    (void) buf;

    dhcpc->state = DHCP_CLIENT_STATE_REBINDING;
    dhcpc->retry = 0;
    pico_dhcp_client_msg(dhcpc, PICO_DHCP_MSG_REQUEST);
    if (pico_dhcp_client_start_rebinding_timer(dhcpc) < 0)
        return -1;

    return 0;
}

static int reset(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf)
{
    struct pico_ip4 address = {
        0
    };
    (void) buf;

    if (dhcpc->state == DHCP_CLIENT_STATE_REQUESTING)
        address.addr = PICO_IP4_ANY;
    else
        address.addr = dhcpc->address.addr;

    /* close the socket used for address (re)acquisition */
    pico_socket_close(dhcpc->s);
    dhcpc->s = NULL;
    /* delete the link with the currently in use address */
    pico_ipv4_link_del(dhcpc->dev, address);

    pico_dhcp_client_callback(dhcpc, PICO_DHCP_RESET);

    if (dhcpc->state < DHCP_CLIENT_STATE_BOUND)
    {
        /* pico_dhcp_client_timer_stop(dhcpc, PICO_DHCPC_TIMER_INIT); */
    }


    dhcpc->state = DHCP_CLIENT_STATE_INIT;
    pico_dhcp_client_stop_timers(dhcpc);
    pico_dhcp_client_init(dhcpc);
    return 0;
}

static int retransmit(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf)
{
    (void) buf;
    switch (dhcpc->state)
    {
    case DHCP_CLIENT_STATE_INIT:
        pico_dhcp_client_msg(dhcpc, PICO_DHCP_MSG_DISCOVER);
        if (pico_dhcp_client_start_init_timer(dhcpc) < 0)
            return -1;
        break;

    case DHCP_CLIENT_STATE_REQUESTING:
        pico_dhcp_client_msg(dhcpc, PICO_DHCP_MSG_REQUEST);
        if (pico_dhcp_client_start_requesting_timer(dhcpc) < 0)
            return -1;
        break;

    case DHCP_CLIENT_STATE_RENEWING:
        pico_dhcp_client_msg(dhcpc, PICO_DHCP_MSG_REQUEST);
        if (pico_dhcp_client_start_renewing_timer(dhcpc) < 0)
            return -1;
        break;

    case DHCP_CLIENT_STATE_REBINDING:
        pico_dhcp_client_msg(dhcpc, PICO_DHCP_MSG_DISCOVER);
        if (pico_dhcp_client_start_rebinding_timer(dhcpc) < 0)
            return -1;
        break;

    default:
        dhcpc_dbg("DHCP client WARNING: retransmit in incorrect state (%u)!\n", dhcpc->state);
        return -1;
    }

    return 0;
}

struct dhcp_action_entry {
    int (*offer)(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
    int (*ack)(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
    int (*nak)(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
    int (*timer1)(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
    int (*timer2)(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
    int (*timer_lease)(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
    int (*timer_retransmit)(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf);
};

static struct dhcp_action_entry dhcp_fsm[] =
{ /* event                |offer      |ack      |nak    |T1    |T2     |lease  |retransmit */
/* state init-reboot */ { NULL,       NULL,     NULL,   NULL,  NULL,   NULL,  NULL       },
/* state rebooting   */ { NULL,       NULL,     NULL,   NULL,  NULL,   NULL,  NULL       },
/* state init        */ { recv_offer, NULL,     NULL,   NULL,  NULL,   NULL,  retransmit },
/* state selecting   */ { NULL,       NULL,     NULL,   NULL,  NULL,   NULL,  NULL       },
/* state requesting  */ { NULL,       recv_ack, reset,  NULL,  NULL,   NULL,  retransmit },
/* state bound       */ { NULL,       NULL,     NULL,   renew, NULL,   NULL,  NULL       },
/* state renewing    */ { NULL,       recv_ack, reset,  NULL,  rebind, NULL,  retransmit },
/* state rebinding   */ { NULL,       recv_ack, reset,  NULL,  NULL,   reset, retransmit },
};

static void dhcp_action_call( int (*call)(struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf), struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf)
{
    if (call)
        call(dhcpc, buf);
}

/* TIMERS REMARK:
 * In state bound we have T1, T2 and the lease timer running. If T1 goes off, we attempt to renew.
 * If the renew succeeds a new T1, T2 and lease timer is started. The former T2 and lease timer is
 * still running though. This poses no concerns as the T2 and lease event in state bound have a NULL
 * pointer in the fsm. If the former T2 or lease timer goes off, nothing happens. Same situation
 * applies for T2 and a succesfull rebind. */

static void pico_dhcp_state_machine(uint8_t event, struct pico_dhcp_client_cookie *dhcpc, uint8_t *buf)
{
    switch (event)
    {
    case PICO_DHCP_MSG_OFFER:
        dhcpc_dbg("DHCP client: received OFFER\n");
        dhcp_action_call(dhcp_fsm[dhcpc->state].offer, dhcpc, buf);
        break;

    case PICO_DHCP_MSG_ACK:
        dhcpc_dbg("DHCP client: received ACK\n");
        dhcp_action_call(dhcp_fsm[dhcpc->state].ack, dhcpc, buf);
        break;

    case PICO_DHCP_MSG_NAK:
        dhcpc_dbg("DHCP client: received NAK\n");
        dhcp_action_call(dhcp_fsm[dhcpc->state].nak, dhcpc, buf);
        break;

    case PICO_DHCP_EVENT_T1:
        dhcpc_dbg("DHCP client: received T1 timeout\n");
        dhcp_action_call(dhcp_fsm[dhcpc->state].timer1, dhcpc, buf);
        break;

    case PICO_DHCP_EVENT_T2:
        dhcpc_dbg("DHCP client: received T2 timeout\n");
        dhcp_action_call(dhcp_fsm[dhcpc->state].timer2, dhcpc, buf);
        break;

    case PICO_DHCP_EVENT_LEASE:
        dhcpc_dbg("DHCP client: received LEASE timeout\n");
        dhcp_action_call(dhcp_fsm[dhcpc->state].timer_lease, dhcpc, buf);
        break;

    case PICO_DHCP_EVENT_RETRANSMIT:
        dhcpc_dbg("DHCP client: received RETRANSMIT timeout\n");
        dhcp_action_call(dhcp_fsm[dhcpc->state].timer_retransmit, dhcpc, buf);
        break;

    default:
        dhcpc_dbg("DHCP client WARNING: unrecognized event (%u)!\n", dhcpc->event);
        return;
    }
    return;
}

static int16_t pico_dhcp_client_opt_parse(void *ptr, uint16_t len)
{
    uint32_t optlen = len - (uint32_t)sizeof(struct pico_dhcp_hdr);
    struct pico_dhcp_hdr *hdr = (struct pico_dhcp_hdr *)ptr;
    struct pico_dhcp_opt *opt = DHCP_OPT(hdr, 0);

    if (hdr->dhcp_magic != PICO_DHCPD_MAGIC_COOKIE)
        return -1;

    if (!pico_dhcp_are_options_valid(opt, (int32_t)optlen))
        return -1;

    do {
        if (opt->code == PICO_DHCP_OPT_MSGTYPE)
            return opt->ext.msg_type.type;
    } while (pico_dhcp_next_option(&opt));

    return -1;
}

static int8_t pico_dhcp_client_msg(struct pico_dhcp_client_cookie *dhcpc, uint8_t msg_type)
{
    int32_t r = 0;
    uint16_t optlen = 0, offset = 0;
    struct pico_ip4 destination = {
        .addr = 0xFFFFFFFF
    };
    struct pico_dhcp_hdr *hdr = NULL;


    /* RFC 2131 3.1.3: Request is always BROADCAST */

    /* Set again default route for the bcast request */
    pico_ipv4_route_set_bcast_link(pico_ipv4_link_by_dev(dhcpc->dev));

    switch (msg_type)
    {
    case PICO_DHCP_MSG_DISCOVER:
        dhcpc_dbg("DHCP client: sent DHCPDISCOVER\n");
        optlen = PICO_DHCP_OPTLEN_MSGTYPE + PICO_DHCP_OPTLEN_MAXMSGSIZE + PICO_DHCP_OPTLEN_PARAMLIST + PICO_DHCP_OPTLEN_END;
        hdr = PICO_ZALLOC((size_t)(sizeof(struct pico_dhcp_hdr) + optlen));
        if (!hdr) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        /* specific options */
        offset = (uint16_t)(offset + pico_dhcp_opt_maxmsgsize(DHCP_OPT(hdr, offset), DHCP_CLIENT_MAXMSGZISE));
        break;

    case PICO_DHCP_MSG_REQUEST:
        optlen = PICO_DHCP_OPTLEN_MSGTYPE + PICO_DHCP_OPTLEN_MAXMSGSIZE + PICO_DHCP_OPTLEN_PARAMLIST + PICO_DHCP_OPTLEN_REQIP + PICO_DHCP_OPTLEN_SERVERID
                 + PICO_DHCP_OPTLEN_END;
        hdr = PICO_ZALLOC(sizeof(struct pico_dhcp_hdr) + optlen);
        if (!hdr) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        /* specific options */
        offset = (uint16_t)(offset + pico_dhcp_opt_maxmsgsize(DHCP_OPT(hdr, offset), DHCP_CLIENT_MAXMSGZISE));
        if (dhcpc->state == DHCP_CLIENT_STATE_REQUESTING) {
            offset = (uint16_t)(offset + pico_dhcp_opt_reqip(DHCP_OPT(hdr, offset), &dhcpc->address));
            offset = (uint16_t)(offset + pico_dhcp_opt_serverid(DHCP_OPT(hdr, offset), &dhcpc->server_id));
        }

        break;

    default:
        return -1;
    }

    /* common options */
    offset = (uint16_t)(offset + pico_dhcp_opt_msgtype(DHCP_OPT(hdr, offset), msg_type));
    offset = (uint16_t)(offset + pico_dhcp_opt_paramlist(DHCP_OPT(hdr, offset)));
    offset = (uint16_t)(offset + pico_dhcp_opt_end(DHCP_OPT(hdr, offset)));

    switch (dhcpc->state)
    {
    case DHCP_CLIENT_STATE_BOUND:
        destination.addr = dhcpc->server_id.addr;
        hdr->ciaddr = dhcpc->address.addr;
        break;

    case DHCP_CLIENT_STATE_RENEWING:
        destination.addr = dhcpc->server_id.addr;
        hdr->ciaddr = dhcpc->address.addr;
        break;

    case DHCP_CLIENT_STATE_REBINDING:
        hdr->ciaddr = dhcpc->address.addr;
        break;

    default:
        /* do nothing */
        break;
    }

    /* header information */
    hdr->op = PICO_DHCP_OP_REQUEST;
    hdr->htype = PICO_DHCP_HTYPE_ETH;
    hdr->hlen = PICO_SIZE_ETH;
    hdr->xid = dhcpc->xid;
    /* hdr->flags = short_be(PICO_DHCP_FLAG_BROADCAST); / * Nope: see bug #96! * / */
    hdr->dhcp_magic = PICO_DHCPD_MAGIC_COOKIE;
    /* copy client hardware address */
    memcpy(hdr->hwaddr, &dhcpc->dev->eth->mac, PICO_SIZE_ETH);

    if (destination.addr == PICO_IP4_BCAST)
        pico_ipv4_route_set_bcast_link(pico_ipv4_link_get(&dhcpc->address));

    r = pico_socket_sendto(dhcpc->s, hdr, (int)(sizeof(struct pico_dhcp_hdr) + optlen), &destination, PICO_DHCPD_PORT);
    PICO_FREE(hdr);
    if (r < 0)
        return -1;

    return 0;
}

static void pico_dhcp_client_wakeup(uint16_t ev, struct pico_socket *s)
{

    uint8_t *buf;
    int r = 0;
    struct pico_dhcp_hdr *hdr = NULL;
    struct pico_dhcp_client_cookie *dhcpc = NULL;

    if ((ev & PICO_SOCK_EV_RD) == 0)
        return;

    buf = PICO_ZALLOC(DHCP_CLIENT_MAXMSGZISE);
    if (!buf) {
        return;
    }

    r = pico_socket_recvfrom(s, buf, DHCP_CLIENT_MAXMSGZISE, NULL, NULL);
    if (r < 0)
        goto out_discard_buf;

    /* If the 'xid' of an arriving message does not match the 'xid'
     * of the most recent transmitted message, the message must be
     * silently discarded. */
    hdr = (struct pico_dhcp_hdr *)buf;
    dhcpc = pico_dhcp_client_find_cookie(hdr->xid);
    if (!dhcpc)
        goto out_discard_buf;

    dhcpc->event = (uint8_t)pico_dhcp_client_opt_parse(buf, (uint16_t)r);
    pico_dhcp_state_machine(dhcpc->event, dhcpc, buf);

out_discard_buf:
    PICO_FREE(buf);
}

static void pico_dhcp_client_callback(struct pico_dhcp_client_cookie *dhcpc, int code)
{
    if(dhcpc->cb)
        dhcpc->cb(dhcpc, code);
}

void *MOCKABLE pico_dhcp_get_identifier(uint32_t xid)
{
    return (void *)pico_dhcp_client_find_cookie(xid);
}

struct pico_ip4 MOCKABLE pico_dhcp_get_address(void*dhcpc)
{
    return ((struct pico_dhcp_client_cookie*)dhcpc)->address;
}

struct pico_ip4 MOCKABLE pico_dhcp_get_gateway(void*dhcpc)
{
    return ((struct pico_dhcp_client_cookie*)dhcpc)->gateway;
}

struct pico_ip4 pico_dhcp_get_netmask(void *dhcpc)
{
    return ((struct pico_dhcp_client_cookie*)dhcpc)->netmask;
}

struct pico_ip4 pico_dhcp_get_nameserver(void*dhcpc, int index)
{
    struct pico_ip4 fault = {
        .addr = 0xFFFFFFFFU
    };
    if ((index != 0) && (index != 1))
        return fault;

    return ((struct pico_dhcp_client_cookie*)dhcpc)->nameserver[index];
}

int pico_dhcp_client_abort(uint32_t xid)
{
    return pico_dhcp_client_del_cookie(xid);
}


char *pico_dhcp_get_hostname(void)
{
    return dhcpc_host_name;
}

char *pico_dhcp_get_domain(void)
{
    return dhcpc_domain_name;
}

#endif
