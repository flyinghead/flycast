/*********************************************************************
 PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights
 reserved.  See LICENSE and COPYING for usage.

 Authors: Jelle De Vleeschouwer
 *********************************************************************/
#ifndef INCLUDE_PICO_802154
#define INCLUDE_PICO_802154

#include "pico_device.h"
#include "pico_config.h"
#include "pico_6lowpan_ll.h"

/*******************************************************************************
 * Size definitions
 ******************************************************************************/

#define MTU_802154_PHY                  (128u)
#define MTU_802154_MAC                  (125u) // 127 - Frame Check Sequence

#define SIZE_802154_MHR_MIN             (5u)
#define SIZE_802154_MHR_MAX             (23u)
#define SIZE_802154_FCS                 (2u)
#define SIZE_802154_LEN                 (1u)
#define SIZE_802154_PAN                 (2u)

/*******************************************************************************
 * Structure definitions
 ******************************************************************************/

PACKED_STRUCT_DEF pico_802154_hdr
{
    uint16_t fcf;
    uint8_t seq;
    uint16_t pan_id;
};

extern const struct pico_6lowpan_ll_protocol pico_6lowpan_ll_802154;

#endif /* INCLUDE_PICO_802154 */
