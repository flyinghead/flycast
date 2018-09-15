/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_FRAME
#define INCLUDE_PICO_FRAME
#include "pico_config.h"


#define PICO_FRAME_FLAG_BCAST               (0x01)
#define PICO_FRAME_FLAG_EXT_BUFFER          (0x02)
#define PICO_FRAME_FLAG_EXT_USAGE_COUNTER   (0x04)
#define PICO_FRAME_FLAG_SACKED              (0x80)
#define PICO_FRAME_FLAG_LL_SEC              (0x40)
#define PICO_FRAME_FLAG_SLP_FRAG            (0x20)
#define IS_BCAST(f) ((f->flags & PICO_FRAME_FLAG_BCAST) == PICO_FRAME_FLAG_BCAST)


struct pico_socket;


struct pico_frame {

    /* Connector for queues */
    struct pico_frame *next;

    /* Start of the whole buffer, total frame length. */
    unsigned char *buffer;
    uint32_t buffer_len;

    /* For outgoing packets: this is the meaningful buffer. */
    unsigned char *start;
    uint32_t len;

    /* Pointer to usage counter */
    uint32_t *usage_count;

    /* Pointer to protocol headers */
    uint8_t *datalink_hdr;

    uint8_t *net_hdr;
    uint16_t net_len;
    uint8_t *transport_hdr;
    uint16_t transport_len;
    uint8_t *app_hdr;
    uint16_t app_len;

    /* Pointer to the phisical device this packet belongs to.
     * Should be valid in both routing directions
     */
    struct pico_device *dev;

    pico_time timestamp;

    /* Failures due to bad datalink addressing. */
    uint16_t failure_count;

    /* Protocol over IP */
    uint8_t proto;

    /* PICO_FRAME_FLAG_* */
    uint8_t flags;

    /* Pointer to payload */
    unsigned char *payload;
    uint16_t payload_len;

#if defined(PICO_SUPPORT_IPV4FRAG) || defined(PICO_SUPPORT_IPV6FRAG)
    /* Payload fragmentation info */
    uint16_t frag;
#endif

#if defined(PICO_SUPPORT_6LOWPAN)
    uint32_t hash;
    union pico_ll_addr src;
    union pico_ll_addr dst;
#endif

    /* Pointer to socket */
    struct pico_socket *sock;

    /* Pointer to transport info, used to store remote UDP endpoint (IP + port) */
    void *info;

    /*Priority. "best-effort" priority, the default value is 0. Priority can be in between -10 and +10*/
    int8_t priority;
    uint8_t transport_flags_saved;

    /* Callback to notify listener when the buffer has been discarded */
    void (*notify_free)(uint8_t *);

    uint8_t send_ttl; /* Special TTL/HOPS value, 0 = auto assign */
    uint8_t send_tos; /* Type of service */
};

/** frame alloc/dealloc/copy **/
void pico_frame_discard(struct pico_frame *f);
struct pico_frame *pico_frame_copy(struct pico_frame *f);
struct pico_frame *pico_frame_deepcopy(struct pico_frame *f);
struct pico_frame *pico_frame_alloc(uint32_t size);
int pico_frame_grow(struct pico_frame *f, uint32_t size);
int pico_frame_grow_head(struct pico_frame *f, uint32_t size);
struct pico_frame *pico_frame_alloc_skeleton(uint32_t size, int ext_buffer);
int pico_frame_skeleton_set_buffer(struct pico_frame *f, void *buf);
uint16_t pico_checksum(void *inbuf, uint32_t len);
uint16_t pico_dualbuffer_checksum(void *b1, uint32_t len1, void *b2, uint32_t len2);

static inline int pico_is_digit(char c)
{
    if (c < '0' || c > '9')
        return 0;

    return 1;
}

static inline int pico_is_hex(char c)
{
    if (c >= '0' && c <= '9')
        return 1;

    if (c >= 'a' && c <= 'f')
        return 1;

    if (c >= 'A' && c <= 'F')
        return 1;

    return 0;
}

#endif
