/*********************************************************************
 PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights
 reserved.  See LICENSE and COPYING for usage.

 Authors: Jelle De Vleeschouwer
 *********************************************************************/

#ifndef INCLUDE_PICO_6LOWPAN
#define INCLUDE_PICO_6LOWPAN

#include "pico_protocol.h"
#include "pico_device.h"
#include "pico_config.h"
#include "pico_frame.h"

#define PICO_6LP_FLAG_LOWPAN (0x01)
#define PICO_6LP_FLAG_NOMAC  (0x02)

#ifdef PICO_SUPPORT_6LOWPAN
#define PICO_DEV_IS_6LOWPAN(dev) ((dev) && ((dev)->hostvars.lowpan_flags & PICO_6LP_FLAG_LOWPAN))
#define PICO_DEV_IS_NOMAC(dev) ((dev) && ((dev)->hostvars.lowpan_flags & PICO_6LP_FLAG_NOMAC))
#else
#define PICO_DEV_IS_6LOWPAN(dev) (0)
#define PICO_DEV_IS_NOMAC(dev) (0)
#endif

/******************************************************************************
 * Public variables
 ******************************************************************************/

extern struct pico_protocol pico_proto_6lowpan;

/******************************************************************************
 * Public functions
 ******************************************************************************/

int32_t pico_6lowpan_pull(struct pico_frame *f);
int pico_6lowpan_init(void);

#endif /* INCLUDE_PICO_6LOWPAN */
