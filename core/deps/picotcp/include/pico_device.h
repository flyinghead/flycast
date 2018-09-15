/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_DEVICE
#define INCLUDE_PICO_DEVICE
#include "pico_queue.h"
#include "pico_frame.h"
#include "pico_addressing.h"
#include "pico_tree.h"
extern struct pico_tree Device_tree;
#include "pico_ipv6_nd.h"
#define MAX_DEVICE_NAME 16


struct pico_ethdev {
    struct pico_eth mac;
};

struct pico_device {
    char name[MAX_DEVICE_NAME];
    uint32_t hash;
    uint32_t overhead;
    uint32_t mtu;
    struct pico_ethdev *eth; /* Null if non-ethernet */
    enum pico_ll_mode mode;
    struct pico_queue *q_in;
    struct pico_queue *q_out;
    int (*link_state)(struct pico_device *self);
    int (*send)(struct pico_device *self, void *buf, int len); /* Send function. Return 0 if busy */
    int (*poll)(struct pico_device *self, int loop_score);
    void (*destroy)(struct pico_device *self);
    int (*dsr)(struct pico_device *self, int loop_score);
    int __serving_interrupt;
    /* used to signal the upper layer the number of events arrived since the last processing */
    volatile int eventCnt;
  #ifdef PICO_SUPPORT_IPV6
    struct pico_nd_hostvars hostvars;
  #endif
};


int pico_device_init(struct pico_device *dev, const char *name, const uint8_t *mac);
void pico_device_destroy(struct pico_device *dev);
int pico_devices_loop(int loop_score, int direction);
struct pico_device*pico_get_device(const char*name);
int32_t pico_device_broadcast(struct pico_frame *f);
int pico_device_link_state(struct pico_device *dev);
int pico_device_ipv6_random_ll(struct pico_device *dev);
#ifdef PICO_SUPPORT_IPV6
struct pico_ipv6_link *pico_ipv6_link_add_local(struct pico_device *dev, const struct pico_ip6 *prefix);
#endif

#endif
