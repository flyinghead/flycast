/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_STACK
#define INCLUDE_PICO_STACK
#include "pico_config.h"
#include "pico_frame.h"
#include "pico_constants.h"

#define PICO_MAX_TIMERS 20

#define PICO_ETH_MRU (1514u)
#define PICO_IP_MRU (1500u)

/*******************************************************************************
 *  TRANSPORT LAYER
 ******************************************************************************/

/* From dev up to socket */
int32_t pico_transport_receive(struct pico_frame *f, uint8_t proto);

/*******************************************************************************
 *  NETWORK LAYER
 ******************************************************************************/

/* From socket down to dev */
int32_t pico_network_send(struct pico_frame *f);

/* From dev up to socket */
int32_t pico_network_receive(struct pico_frame *f);

/*******************************************************************************
 *  DATALINK LAYER
 ******************************************************************************/

/* From socket down to dev */
int pico_datalink_send(struct pico_frame *f);

/* From dev up to socket */
int pico_datalink_receive(struct pico_frame *f);

/*******************************************************************************
 *  PHYSICAL LAYER
 ******************************************************************************/

/* Enqueues the frame in the device-queue. From socket down to dev */
int32_t pico_sendto_dev(struct pico_frame *f);

/* LOWEST LEVEL: interface towards stack from devices */
/* Device driver will call this function which returns immediately.
 * Incoming packet will be processed later on in the dev loop.
 * The zerocopy version will associate the current buffer to the newly created frame.
 * Warning: the buffer used in the zerocopy version MUST have been allocated using PICO_ZALLOC()
 */
int32_t pico_stack_recv(struct pico_device *dev, uint8_t *buffer, uint32_t len);
int32_t pico_stack_recv_zerocopy(struct pico_device *dev, uint8_t *buffer, uint32_t len);
int32_t pico_stack_recv_zerocopy_ext_buffer(struct pico_device *dev, uint8_t *buffer, uint32_t len);
int32_t pico_stack_recv_zerocopy_ext_buffer_notify(struct pico_device *dev, uint8_t *buffer, uint32_t len, void (*notify_free)(uint8_t *buffer));
struct pico_frame *pico_stack_recv_new_frame(struct pico_device *dev, uint8_t *buffer, uint32_t len);

/* ----- Initialization ----- */
int pico_stack_init(void);

/* ----- Loop Function. ----- */
void pico_stack_tick(void);
void pico_stack_loop(void);

/* ---- Notifications for stack errors */
int pico_notify_socket_unreachable(struct pico_frame *f);
int pico_notify_proto_unreachable(struct pico_frame *f);
int pico_notify_dest_unreachable(struct pico_frame *f);
int pico_notify_ttl_expired(struct pico_frame *f);
int pico_notify_frag_expired(struct pico_frame *f);
int pico_notify_pkt_too_big(struct pico_frame *f);

/* Various. */
int pico_source_is_local(struct pico_frame *f);
int pico_frame_dst_is_unicast(struct pico_frame *f);
void pico_store_network_origin(void *src, struct pico_frame *f);
uint32_t pico_timer_add(pico_time expire, void (*timer)(pico_time, void *), void *arg);
uint32_t pico_timer_add_hashed(pico_time expire, void (*timer)(pico_time, void *), void *arg, uint32_t hash);
void pico_timer_cancel_hashed(uint32_t hash);
void pico_timer_cancel(uint32_t id);
uint32_t pico_rand(void);
void pico_rand_feed(uint32_t feed);
void pico_to_lowercase(char *str);
int pico_address_compare(union pico_address *a, union pico_address *b, uint16_t proto);
int32_t pico_seq_compare(uint32_t a, uint32_t b);

#endif
