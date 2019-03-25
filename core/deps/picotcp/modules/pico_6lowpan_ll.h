/*********************************************************************
 PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights
 reserved.  See LICENSE and COPYING for usage.

 Authors: Jelle De Vleeschouwer
 *********************************************************************/

#ifndef INCLUDE_PICO_6LOWPAN_LL
#define INCLUDE_PICO_6LOWPAN_LL

#include "pico_addressing.h"
#include "pico_protocol.h"
#include "pico_6lowpan.h"
#include "pico_device.h"
#include "pico_config.h"
#include "pico_frame.h"
#include "pico_ipv6.h"

/* Possible actions to perform on a received frame */
#define FRAME_6LOWPAN_LL_RELEASE    (-1)
#define FRAME_6LOWPAN_LL_DISCARD    (-2)

/*******************************************************************************
 *  CTX
 ******************************************************************************/

#ifdef PICO_6LOWPAN_IPHC_ENABLED

#define PICO_IPHC_CTX_COMPRESS (0x01u)

struct iphc_ctx
{
    struct pico_device *dev;
    struct pico_ip6 prefix;
    uint8_t id;
    uint8_t size;
    uint8_t flags;
    pico_time lifetime;
};

/*
 *  Looks up a context entry for a particular IPv6-address contained in 'addr' and returns it.
 *  Returns NULL if no entry is found. (See RFC4944)
 */
struct iphc_ctx * ctx_lookup(struct pico_ip6 addr);

/*
 *  Looks up a context entry that belongs to a certain context identifier.
 *  Returns NULL if no belonging entry is found. (See RFC4944)
 */
struct iphc_ctx * ctx_lookup_id(uint8_t id);

/*
 *  Creates a new, or updates and existing, context entry for a certain IPv6 address. (See RFC4944)
 */
void ctx_update(struct pico_ip6 addr, uint8_t id, uint8_t size, pico_time lifetime, uint8_t flags, struct pico_device *dev);

#endif

/******************************************************************************
 * Interface with device drivers
 ******************************************************************************/

struct pico_dev_6lowpan
{
    /* Interface with picoTCP */
    struct pico_device dev;

    /* Transmit-function:
     *
     *  @param dev  The device who's send-function got called
     *  @param _buf Buffer containing the frame to be send over the network
     *  @param len  Length of _buf
     *  @param src  Link Layer source address of the device (IETF-endianness)
     *  @param dst  Link layer destination address of the device (IETF-endianness)
     *
     *  @return length of the frame that is transmitted on success, -1 on failure
     */
    int (* send)(struct pico_device *dev, void *_buf, int len, union pico_ll_addr src, union pico_ll_addr dst);
};

/* Initialisation routine for 6LoWPAN specific devices */
int pico_dev_6lowpan_init(struct pico_dev_6lowpan *dev, const char *name, uint8_t *mac, enum pico_ll_mode ll_mode, uint16_t mtu, uint8_t nomac,
                          int (* send)(struct pico_device *dev, void *_buf, int len, union pico_ll_addr src, union pico_ll_addr dst),
                          int (* poll)(struct pico_device *dev, int loop_score));

/******************************************************************************
 * Interface with link layer
 ******************************************************************************/

struct pico_6lowpan_ll_protocol
{
    int32_t (* process_in)(struct pico_frame *f);
    int32_t (* process_out)(struct pico_frame *f);
    int32_t (* estimate)(struct pico_frame *f);
    int32_t (* addr_from_buf)(union pico_ll_addr *addr, uint8_t *buf);
    int32_t (* addr_from_net)(union pico_ll_addr *addr, struct pico_frame *f, int32_t dest);
    int32_t (* addr_len)(union pico_ll_addr *addr);
    int32_t (* addr_cmp)(union pico_ll_addr *a, union pico_ll_addr *b);
    int32_t (* addr_iid)(uint8_t *iid, union pico_ll_addr *addr);
    struct pico_frame * (*alloc)(struct pico_device *dev, uint16_t size);
};

/******************************************************************************
 * Public variables
 ******************************************************************************/

extern struct pico_6lowpan_ll_protocol pico_6lowpan_lls[];
extern struct pico_protocol pico_proto_6lowpan_ll;

/******************************************************************************
 * Public functions
 ******************************************************************************/

void pico_6lowpan_ll_init(void);
int32_t pico_6lowpan_ll_push(struct pico_frame *f);
int32_t pico_6lowpan_ll_pull(struct pico_frame *f);
int32_t frame_6lowpan_ll_store_addr(struct pico_frame *f);
int32_t pico_6lowpan_ll_sendto_dev(struct pico_device *dev, struct pico_frame *f);
int32_t pico_6lowpan_stack_recv(struct pico_device *dev, uint8_t *buffer, uint32_t len, union pico_ll_addr *src, union pico_ll_addr *dst);

#endif /* INCLUDE_PICO_6LOWPAN_LL */
