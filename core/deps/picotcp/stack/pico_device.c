/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Authors: Daniele Lacamera
 *********************************************************************/

#include "pico_config.h"
#include "pico_device.h"
#include "pico_stack.h"
#include "pico_protocol.h"
#include "pico_tree.h"
#include "pico_ipv6.h"
#include "pico_ipv4.h"
#include "pico_icmp6.h"
#include "pico_eth.h"
#include "pico_802154.h"
#include "pico_6lowpan.h"
#include "pico_6lowpan_ll.h"
#include "pico_addressing.h"
#define PICO_DEVICE_DEFAULT_MTU (1500)

struct pico_devices_rr_info {
    struct pico_tree_node *node_in, *node_out;
};

static struct pico_devices_rr_info Devices_rr_info = {
    NULL, NULL
};

static int pico_dev_cmp(void *ka, void *kb)
{
    struct pico_device *a = ka, *b = kb;
    if (a->hash < b->hash)
        return -1;


    if (a->hash > b->hash)
        return 1;

    return 0;
}

PICO_TREE_DECLARE(Device_tree, pico_dev_cmp);

#ifdef PICO_SUPPORT_6LOWPAN
static struct pico_ipv6_link * pico_6lowpan_link_add(struct pico_device *dev, const struct pico_ip6 *prefix)
{
    struct pico_ip6 netmask64 = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    struct pico_6lowpan_info *info = (struct pico_6lowpan_info *)dev->eth;
    struct pico_ipv6_link *link = NULL; /* Make sure to return NULL */
    struct pico_ip6 newaddr;

    memcpy(newaddr.addr, prefix->addr, PICO_SIZE_IP6);
    memcpy(newaddr.addr + 8, info->addr_ext.addr, SIZE_6LOWPAN_EXT);
    newaddr.addr[8] = newaddr.addr[8] ^ 0x02; /* Toggle U/L bit */

    /* RFC6775: No Duplicate Address Detection (DAD) is performed if
        * EUI-64-based IPv6 addresses are used (as these addresses are assumed
        * to be globally unique). */
    if ((link = pico_ipv6_link_add_no_dad(dev, newaddr, netmask64))) {
        if (pico_ipv6_is_linklocal(newaddr.addr))
            pico_6lp_nd_start_soliciting(link, NULL);
        else
            pico_6lp_nd_register(link);
    }

    return link;
}

static int pico_6lowpan_store_info(struct pico_device *dev, const uint8_t *mac)
{
    if ((dev->eth = PICO_ZALLOC(sizeof(struct pico_6lowpan_info)))) {
        memcpy(dev->eth, mac, sizeof(struct pico_6lowpan_info));
        return 0;
    } else {
        pico_err = PICO_ERR_ENOMEM;
        return -1;
    }
}
#endif

#ifdef PICO_SUPPORT_IPV6
static void device_init_ipv6_final(struct pico_device *dev, struct pico_ip6 *linklocal)
{
    dev->hostvars.basetime = PICO_ND_REACHABLE_TIME;
    /* RFC 4861 $6.3.2 value between 0.5 and 1.5 times basetime */
    dev->hostvars.reachabletime = ((5 + (pico_rand() % 10)) * PICO_ND_REACHABLE_TIME) / 10;
    dev->hostvars.retranstime = PICO_ND_RETRANS_TIMER;
    pico_icmp6_router_solicitation(dev, linklocal, NULL);
    dev->hostvars.hoplimit = PICO_IPV6_DEFAULT_HOP;
}

struct pico_ipv6_link *pico_ipv6_link_add_local(struct pico_device *dev, const struct pico_ip6 *prefix)
{
    struct pico_ip6 netmask64 = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    struct pico_ipv6_link *link = NULL; /* Make sure to return NULL */
    struct pico_ip6 newaddr;

    if (0) {}
#ifdef PICO_SUPPORT_6LOWPAN
    else if (PICO_DEV_IS_6LOWPAN(dev)) {
        link = pico_6lowpan_link_add(dev, prefix);
    }
#endif
    else {
        memcpy(newaddr.addr, prefix->addr, PICO_SIZE_IP6);
        /* modified EUI-64 + invert universal/local bit */
        newaddr.addr[8] = (dev->eth->mac.addr[0] ^ 0x02);
        newaddr.addr[9] = dev->eth->mac.addr[1];
        newaddr.addr[10] = dev->eth->mac.addr[2];
        newaddr.addr[11] = 0xff;
        newaddr.addr[12] = 0xfe;
        newaddr.addr[13] = dev->eth->mac.addr[3];
        newaddr.addr[14] = dev->eth->mac.addr[4];
        newaddr.addr[15] = dev->eth->mac.addr[5];
        if ((link = pico_ipv6_link_add(dev, newaddr, netmask64))) {
            device_init_ipv6_final(dev, &newaddr);
        }
    }

    return link;
}
#endif
static int device_init_mac(struct pico_device *dev, const uint8_t *mac)
{
#ifdef PICO_SUPPORT_IPV6
    struct pico_ip6 linklocal = {{0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0xff, 0xfe, 0xaa, 0xaa, 0xaa}};
#endif

    if (0) {}
#ifdef PICO_SUPPORT_6LOWPAN
    else if (PICO_DEV_IS_6LOWPAN(dev)) {
        if (pico_6lowpan_store_info(dev, mac))
            return -1;
    }
#endif
    else {
        if ((dev->eth = PICO_ZALLOC(sizeof(struct pico_ethdev)))) {
            memcpy(dev->eth->mac.addr, mac, PICO_SIZE_ETH);
        } else {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if (pico_ipv6_link_add_local(dev, &linklocal) == NULL) {
        PICO_FREE(dev->q_in);
        PICO_FREE(dev->q_out);
        PICO_FREE(dev->eth);
        return -1;
    }
#endif

    return 0;
}

int pico_device_ipv6_random_ll(struct pico_device *dev)
{
#ifdef PICO_SUPPORT_IPV6
    struct pico_ip6 linklocal = {{0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0xff, 0xfe, 0xaa, 0xaa, 0xaa}};
    struct pico_ip6 netmask6 = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    uint32_t len = (uint32_t)strlen(dev->name);
    if (strcmp(dev->name, "loop")) {
        do {
            /* privacy extension + unset universal/local and individual/group bit */
            len = pico_rand();
            linklocal.addr[8]  = (uint8_t)((len & 0xffu) & (uint8_t)(~0x03));
            linklocal.addr[9]  = (uint8_t)(len >> 8);
            linklocal.addr[10] = (uint8_t)(len >> 16);
            linklocal.addr[11] = (uint8_t)(len >> 24);
            len = pico_rand();
            linklocal.addr[12] = (uint8_t)len;
            linklocal.addr[13] = (uint8_t)(len >> 8);
            linklocal.addr[14] = (uint8_t)(len >> 16);
            linklocal.addr[15] = (uint8_t)(len >> 24);
            pico_rand_feed(dev->hash);
        } while (pico_ipv6_link_get(&linklocal));

        if (pico_ipv6_link_add(dev, linklocal, netmask6) == NULL) {
            return -1;
        }
    }

#endif
    return 0;
}

static int device_init_nomac(struct pico_device *dev)
{
    if (pico_device_ipv6_random_ll(dev) < 0) {
        PICO_FREE(dev->q_in);
        PICO_FREE(dev->q_out);
        return -1;
    }

    dev->eth = NULL;
    return 0;
}

#define DEBUG_IPV6(ip)  { \
                            char ipstr[40] = { 0 }; \
                            pico_ipv6_to_string(ipstr, (ip).addr); \
                            dbg("IPv6 (%s)\n", ipstr); \
                        }

int pico_device_init(struct pico_device *dev, const char *name, const uint8_t *mac)
{
    uint32_t len = (uint32_t)strlen(name);
    int ret = 0;

    if(len > MAX_DEVICE_NAME)
        len = MAX_DEVICE_NAME;

    memcpy(dev->name, name, len);
    dev->hash = pico_hash(dev->name, len);

    Devices_rr_info.node_in  = NULL;
    Devices_rr_info.node_out = NULL;
    dev->q_in = PICO_ZALLOC(sizeof(struct pico_queue));
    if (!dev->q_in)
        return -1;

    dev->q_out = PICO_ZALLOC(sizeof(struct pico_queue));
    if (!dev->q_out) {
        PICO_FREE(dev->q_in);
        return -1;
    }

    if (pico_tree_insert(&Device_tree, dev)) {
		PICO_FREE(dev->q_in);
		PICO_FREE(dev->q_out);
		return -1;
	}
    if (!dev->mtu)
        dev->mtu = PICO_DEVICE_DEFAULT_MTU;

#ifdef PICO_SUPPORT_6LOWPAN
    if (PICO_DEV_IS_6LOWPAN(dev) && LL_MODE_ETHERNET == dev->mode)
        return -1;
#endif

    if (mac) {
        ret = device_init_mac(dev, mac);
    } else {
        if (!dev->mode) {
            ret = device_init_nomac(dev);
        }
#ifdef PICO_SUPPORT_6LOWPAN
        else {
            /* RFC6775: Link Local to be formed based on EUI-64 as per RFC6775 */
            dbg("Link local address to be formed based on EUI-64\n");
            return -1;
        }
#endif
    }
    return ret;
}

static void pico_queue_destroy(struct pico_queue *q)
{
    if (q) {
        pico_queue_empty(q);
        PICO_FREE(q);
    }
}

void pico_device_destroy(struct pico_device *dev)
{

    pico_queue_destroy(dev->q_in);
    pico_queue_destroy(dev->q_out);

    if (!dev->mode && dev->eth)
        PICO_FREE(dev->eth);

#ifdef PICO_SUPPORT_IPV4
    pico_ipv4_cleanup_links(dev);
#endif
#ifdef PICO_SUPPORT_IPV6
    pico_ipv6_cleanup_links(dev);
#endif
    pico_tree_delete(&Device_tree, dev);

    if (dev->destroy)
        dev->destroy(dev);

    Devices_rr_info.node_in  = NULL;
    Devices_rr_info.node_out = NULL;
    PICO_FREE(dev);
}

static int check_dev_serve_interrupt(struct pico_device *dev, int loop_score)
{
    if ((dev->__serving_interrupt) && (dev->dsr)) {
        /* call dsr routine */
        loop_score = dev->dsr(dev, loop_score);
    }

    return loop_score;
}

static int check_dev_serve_polling(struct pico_device *dev, int loop_score)
{
    if (dev->poll) {
        loop_score = dev->poll(dev, loop_score);
    }

    return loop_score;
}

static int devloop_in(struct pico_device *dev, int loop_score)
{
    struct pico_frame *f;
    while(loop_score > 0) {
        if (dev->q_in->frames == 0)
            break;

        /* Receive */
        f = pico_dequeue(dev->q_in);
        if (f) {
            pico_datalink_receive(f);
            loop_score--;
        }
    }
    return loop_score;
}

static int devloop_sendto_dev(struct pico_device *dev, struct pico_frame *f)
{
#ifdef PICO_SUPPORT_6LOWPAN
    if (PICO_DEV_IS_6LOWPAN(dev)) {
        return (pico_6lowpan_ll_sendto_dev(dev, f) <= 0);
    }
#endif
    return (dev->send(dev, f->start, (int)f->len) <= 0);
}

static int devloop_out(struct pico_device *dev, int loop_score)
{
    struct pico_frame *f;
    while(loop_score > 0) {
        if (dev->q_out->frames == 0)
            break;

        /* Device dequeue + send */
        f = pico_queue_peek(dev->q_out);
        if (!f)
            break;

        if (devloop_sendto_dev(dev, f) == 0) { /* success. */
            f = pico_dequeue(dev->q_out);
            pico_frame_discard(f); /* SINGLE POINT OF DISCARD for OUTGOING FRAMES */
            loop_score--;
        } else
            break; /* Don't discard */

    }

    return loop_score;
}

static int devloop(struct pico_device *dev, int loop_score, int direction)
{
    /* If device supports interrupts, read the value of the condition and trigger the dsr */
    loop_score = check_dev_serve_interrupt(dev, loop_score);

    /* If device supports polling, give control. Loop score is managed internally,
     * remaining loop points are returned. */
    loop_score = check_dev_serve_polling(dev, loop_score);

    if (direction == PICO_LOOP_DIR_OUT)
        loop_score = devloop_out(dev, loop_score);
    else
        loop_score = devloop_in(dev, loop_score);

    return loop_score;
}


static struct pico_tree_node *pico_dev_roundrobin_start(int direction)
{
    if (Devices_rr_info.node_in == NULL)
        Devices_rr_info.node_in = pico_tree_firstNode(Device_tree.root);

    if (Devices_rr_info.node_out == NULL)
        Devices_rr_info.node_out = pico_tree_firstNode(Device_tree.root);

    if (direction == PICO_LOOP_DIR_IN)
        return Devices_rr_info.node_in;
    else
        return Devices_rr_info.node_out;
}

static void pico_dev_roundrobin_end(int direction, struct pico_tree_node *last)
{
    if (direction == PICO_LOOP_DIR_IN)
        Devices_rr_info.node_in = last;
    else
        Devices_rr_info.node_out = last;
}

#define DEV_LOOP_MIN  16

int pico_devices_loop(int loop_score, int direction)
{
    struct pico_device *start, *next;
    struct pico_tree_node *next_node  = pico_dev_roundrobin_start(direction);

    if (!next_node)
        return loop_score;

    next = next_node->keyValue;
    start = next;

    /* round-robin all devices, break if traversed all devices */
    while ((loop_score > DEV_LOOP_MIN) && (next != NULL)) {
        loop_score = devloop(next, loop_score, direction);
        next_node = pico_tree_next(next_node);
        next = next_node->keyValue;
        if (next == NULL)
        {
            next_node = pico_tree_firstNode(Device_tree.root);
            next = next_node->keyValue;
        }

        if (next == start)
            break;
    }
    pico_dev_roundrobin_end(direction, next_node);
    return loop_score;
}

struct pico_device *pico_get_device(const char*name)
{
    struct pico_device *dev;
    struct pico_tree_node *index;
    pico_tree_foreach(index, &Device_tree){
        dev = index->keyValue;
        if(strcmp(name, dev->name) == 0)
            return dev;
    }
    return NULL;
}

int32_t pico_device_broadcast(struct pico_frame *f)
{
    struct pico_tree_node *index;
    int32_t ret = -1;

    pico_tree_foreach(index, &Device_tree)
    {
        struct pico_device *dev = index->keyValue;
        if(dev != f->dev)
        {
            struct pico_frame *copy = pico_frame_copy(f);

            if(!copy)
                break;

            copy->dev = dev;
            copy->dev->send(copy->dev, copy->start, (int)copy->len);
            pico_frame_discard(copy);
        }
        else
        {
            ret = f->dev->send(f->dev, f->start, (int)f->len);
        }
    }
    return ret;
}

int pico_device_link_state(struct pico_device *dev)
{
    if (!dev->link_state)
        return 1; /* Not supported, assuming link is always up */

    return dev->link_state(dev);
}
