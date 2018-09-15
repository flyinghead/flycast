/*********************************************************************
   PicoTCP. Copyright (c) 2012 TASS Belgium NV. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Serge Gadeyne, Daniele Lacamera, Maxime Vincent

 *********************************************************************/


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pico_device.h"
#include "pico_dev_ppp.h"
#include "pico_stack.h"
#include "pico_ipv4.h"
#include "pico_md5.h"
#include "pico_dns_client.h"

#ifdef DEBUG_PPP
    #define ppp_dbg dbg
#else
    #define ppp_dbg(...) do {} while(0)
#endif

/* We should define this in a global header. */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define PICO_PPP_MRU 1514 /* RFC default MRU */
#define PICO_PPP_MTU 1500
#define PPP_MAXPKT 2048
#define PPP_MAX_APN 134
#define PPP_MAX_USERNAME 134
#define PPP_MAX_PASSWORD 134
#define PPP_HDR_SIZE 3u
#define PPP_PROTO_SLOT_SIZE 2u
#define PPP_FCS_SIZE 2u
#define PPP_PROTO_LCP short_be(0xc021)
#define PPP_PROTO_IP  short_be(0x0021)
#define PPP_PROTO_PAP short_be(0xc023)
#define PPP_PROTO_CHAP short_be(0xc223)
#define PPP_PROTO_IPCP short_be(0x8021)

#define PICO_CONF_REQ 1
#define PICO_CONF_ACK 2
#define PICO_CONF_NAK 3
#define PICO_CONF_REJ 4
#define PICO_CONF_TERM      5
#define PICO_CONF_TERM_ACK  6
#define PICO_CONF_CODE_REJ  7
#define PICO_CONF_PROTO_REJ 8
#define PICO_CONF_ECHO_REQ  9
#define PICO_CONF_ECHO_REP  10
#define PICO_CONF_DISCARD_REQ 11

#define LCPOPT_MRU          1u /* param size: 4, fixed: MRU     */
#define LCPOPT_AUTH         3u /* param size: 4-5: AUTH proto   */
#define LCPOPT_QUALITY      4u /* unused for now                */
#define LCPOPT_MAGIC        5u /* param size: 6, fixed: Magic   */
#define LCPOPT_PROTO_COMP   7u /* param size: 0, flag           */
#define LCPOPT_ADDRCTL_COMP 8u /* param size: 0, flag           */

#define CHAP_MD5_SIZE   16u
#define CHAP_CHALLENGE  1
#define CHAP_RESPONSE   2
#define CHAP_SUCCESS    3
#define CHAP_FAILURE    4
#define CHALLENGE_SIZE(ppp, ch) ((size_t)((1 + strlen(ppp->password) + short_be((ch)->len))))

#define PAP_AUTH_REQ 1
#define PAP_AUTH_ACK 2
#define PAP_AUTH_NAK 3


#define PICO_PPP_DEFAULT_TIMER (3) /* seconds */
#define PICO_PPP_DEFAULT_MAX_TERMINATE (2)
#define PICO_PPP_DEFAULT_MAX_CONFIGURE (10)
#define PICO_PPP_DEFAULT_MAX_FAILURE   (5)
#define PICO_PPP_DEFAULT_MAX_DIALTIME  (20)

#define IPCP_ADDR_LEN 6u
#define IPCP_VJ_LEN 6u
#define IPCP_OPT_IP 0x03
#define IPCP_OPT_VJ 0x02
#define IPCP_OPT_DNS1 0x81
#define IPCP_OPT_NBNS1 0x82
#define IPCP_OPT_DNS2 0x83
#define IPCP_OPT_NBNS2 0x84

static uint8_t LCPOPT_LEN[9] = {
    0, 4, 0, 4, 4, 6, 2, 2, 2
};


/* Protocol defines */
static const unsigned char AT_S3          = 0x0du;
static const unsigned char AT_S4          = 0x0au;
static const unsigned char PPPF_FLAG_SEQ  = 0x7eu;
static const unsigned char PPPF_CTRL_ESC  = 0x7du;
static const unsigned char PPPF_ADDR      = 0xffu;
static const unsigned char PPPF_CTRL      = 0x03u;

static int ppp_devnum = 0;
static uint8_t ppp_recv_buf[PPP_MAXPKT];

PACKED_STRUCT_DEF pico_lcp_hdr {
    uint8_t code;
    uint8_t id;
    uint16_t len;
};

PACKED_STRUCT_DEF pico_chap_hdr {
    uint8_t code;
    uint8_t id;
    uint16_t len;
};

PACKED_STRUCT_DEF pico_pap_hdr {
    uint8_t code;
    uint8_t id;
    uint16_t len;
};

PACKED_STRUCT_DEF pico_ipcp_hdr {
    uint8_t code;
    uint8_t id;
    uint16_t len;
};

enum ppp_modem_state {
    PPP_MODEM_STATE_INITIAL = 0,
    PPP_MODEM_STATE_RESET,
    PPP_MODEM_STATE_ECHO,
    PPP_MODEM_STATE_CREG,
    PPP_MODEM_STATE_CGREG,
    PPP_MODEM_STATE_CGDCONT,
    PPP_MODEM_STATE_CGATT,
    PPP_MODEM_STATE_DIAL,
    PPP_MODEM_STATE_CONNECTED,
    PPP_MODEM_STATE_MAX
};

enum ppp_modem_event {
    PPP_MODEM_EVENT_START = 0,
    PPP_MODEM_EVENT_STOP,
    PPP_MODEM_EVENT_OK,
    PPP_MODEM_EVENT_CONNECT,
    PPP_MODEM_EVENT_TIMEOUT,
    PPP_MODEM_EVENT_MAX
};

enum ppp_lcp_state {
    PPP_LCP_STATE_INITIAL = 0,
    PPP_LCP_STATE_STARTING,
    PPP_LCP_STATE_CLOSED,
    PPP_LCP_STATE_STOPPED,
    PPP_LCP_STATE_CLOSING,
    PPP_LCP_STATE_STOPPING,
    PPP_LCP_STATE_REQ_SENT,
    PPP_LCP_STATE_ACK_RCVD,
    PPP_LCP_STATE_ACK_SENT,
    PPP_LCP_STATE_OPENED,
    PPP_LCP_STATE_MAX
};

enum ppp_lcp_event {
    PPP_LCP_EVENT_UP = 0,
    PPP_LCP_EVENT_DOWN,
    PPP_LCP_EVENT_OPEN,
    PPP_LCP_EVENT_CLOSE,
    PPP_LCP_EVENT_TO_POS,
    PPP_LCP_EVENT_TO_NEG,
    PPP_LCP_EVENT_RCR_POS,
    PPP_LCP_EVENT_RCR_NEG,
    PPP_LCP_EVENT_RCA,
    PPP_LCP_EVENT_RCN,
    PPP_LCP_EVENT_RTR,
    PPP_LCP_EVENT_RTA,
    PPP_LCP_EVENT_RUC,
    PPP_LCP_EVENT_RXJ_POS,
    PPP_LCP_EVENT_RXJ_NEG,
    PPP_LCP_EVENT_RXR,
    PPP_LCP_EVENT_MAX
};

enum ppp_auth_state {
    PPP_AUTH_STATE_INITIAL = 0,
    PPP_AUTH_STATE_STARTING,
    PPP_AUTH_STATE_RSP_SENT,
    PPP_AUTH_STATE_REQ_SENT,
    PPP_AUTH_STATE_AUTHENTICATED,
    PPP_AUTH_STATE_MAX
};

enum ppp_auth_event {
    PPP_AUTH_EVENT_UP_NONE = 0,
    PPP_AUTH_EVENT_UP_PAP,
    PPP_AUTH_EVENT_UP_CHAP,
    PPP_AUTH_EVENT_DOWN,
    PPP_AUTH_EVENT_RAC,
    PPP_AUTH_EVENT_RAA,
    PPP_AUTH_EVENT_RAN,
    PPP_AUTH_EVENT_TO,
    PPP_AUTH_EVENT_MAX
};

enum ppp_ipcp_state {
    PPP_IPCP_STATE_INITIAL = 0,
    PPP_IPCP_STATE_REQ_SENT,
    PPP_IPCP_STATE_ACK_RCVD,
    PPP_IPCP_STATE_ACK_SENT,
    PPP_IPCP_STATE_OPENED,
    PPP_IPCP_STATE_MAX
};

enum ppp_ipcp_event {
    PPP_IPCP_EVENT_UP = 0,
    PPP_IPCP_EVENT_DOWN,
    PPP_IPCP_EVENT_RCR_POS,
    PPP_IPCP_EVENT_RCR_NEG,
    PPP_IPCP_EVENT_RCA,
    PPP_IPCP_EVENT_RCN,
    PPP_IPCP_EVENT_TO,
    PPP_IPCP_EVENT_MAX
};

enum pico_ppp_state {
    PPP_MODEM_RST = 0,
    PPP_MODEM_CREG,
    PPP_MODEM_CGREG,
    PPP_MODEM_CGDCONT,
    PPP_MODEM_CGATT,
    PPP_MODEM_CONNECT,
    /* From here on, PPP states */
    PPP_ESTABLISH,
    PPP_AUTH,
    PPP_NETCONFIG,
    PPP_NETWORK,
    PPP_TERMINATE,
    /* MAXSTATE is the last one */
    PPP_MODEM_MAXSTATE
};


#define IPCP_ALLOW_IP 0x01u
#define IPCP_ALLOW_DNS1 0x02u
#define IPCP_ALLOW_DNS2 0x04u
#define IPCP_ALLOW_NBNS1 0x08u
#define IPCP_ALLOW_NBNS2 0x10u

struct pico_device_ppp {
    struct pico_device dev;
    int autoreconnect;
    enum ppp_modem_state modem_state;
    enum ppp_lcp_state lcp_state;
    enum ppp_auth_state auth_state;
    enum ppp_ipcp_state ipcp_state;
    enum pico_ppp_state state;
    char apn[PPP_MAX_APN];
    char password[PPP_MAX_PASSWORD];
    char username[PPP_MAX_USERNAME];
    uint16_t lcpopt_local;
    uint16_t lcpopt_peer;
    uint8_t *pkt;
    uint32_t len;
    uint16_t rej;
    uint16_t auth;
    int (*serial_recv)(struct pico_device *dev, void *buf, int len);
    int (*serial_send)(struct pico_device *dev, const void *buf, int len);
    int (*serial_set_speed)(struct pico_device *dev, uint32_t speed);
    uint32_t ipcp_allowed_fields;
    uint32_t ipcp_ip;
    uint32_t ipcp_dns1;
    uint32_t ipcp_nbns1;
    uint32_t ipcp_dns2;
    uint32_t ipcp_nbns2;
    uint32_t timer;
    uint8_t timer_val;
    uint8_t timer_count;
    uint8_t frame_id;
    uint8_t timer_on;
    uint16_t mru;
};


/* Unit test interceptor */
static void (*mock_modem_state)(struct pico_device_ppp *ppp, enum ppp_modem_event event) = NULL;
static void (*mock_lcp_state)(struct pico_device_ppp *ppp, enum ppp_lcp_event event) = NULL;
static void (*mock_auth_state)(struct pico_device_ppp *ppp, enum ppp_auth_event event) = NULL;
static void (*mock_ipcp_state)(struct pico_device_ppp *ppp, enum ppp_ipcp_event event) = NULL;

/* Debug prints */
#ifdef PPP_DEBUG
static void lcp_optflags_print(struct pico_device_ppp *ppp, uint8_t *opts, uint32_t opts_len);
#endif

#define PPP_TIMER_ON_MODEM      0x01u
#define PPP_TIMER_ON_LCPREQ     0x04u
#define PPP_TIMER_ON_LCPTERM    0x08u
#define PPP_TIMER_ON_AUTH       0x10u
#define PPP_TIMER_ON_IPCP       0x20u

/* Escape and send */
static int ppp_serial_send_escape(struct pico_device_ppp *ppp, void *buf, int len)
{
    uint8_t *in_buf = (uint8_t *)buf;
    uint8_t *out_buf = NULL;
    int esc_char_count = 0;
    int newlen = 0, ret = -1;
    int i, j;

#ifdef PPP_DEBUG
    {
        uint32_t idx;
        if (len > 0) {
            ppp_dbg("PPP >>>> ");
            for(idx = 0; idx < (uint32_t)len; idx++) {
                ppp_dbg(" %02x", ((uint8_t *)buf)[idx]);
            }
            ppp_dbg("\n");
        }
    }
#endif

    for (i = 1; i < (len - 1); i++) /* from 1 to len -1, as start/stop are not escaped */
    {
        if (((in_buf[i] + 1u) >> 1) == 0x3Fu)
            esc_char_count++;
    }
    if (!esc_char_count) {
        return ppp->serial_send(&ppp->dev, buf, len);
    }

    newlen = len + esc_char_count;
    out_buf = PICO_ZALLOC((uint32_t)newlen);
    if(!out_buf)
        return -1;

    /* Start byte. */
    out_buf[0] = in_buf[0];
    for(i = 1, j = 1; i < (len - 1); i++) {
        if (((in_buf[i] + 1u) >> 1) == 0x3Fu) {
            out_buf[j++] = PPPF_CTRL_ESC;
            out_buf[j++] = in_buf[i] ^ 0x20;
        } else {
            out_buf[j++] = in_buf[i];
        }
    }
    /* Stop byte. */
    out_buf[newlen - 1] = in_buf[len - 1];

    ret = ppp->serial_send(&ppp->dev, out_buf, newlen);

    PICO_FREE(out_buf);

    if (ret == newlen)
        return len;

    return ret;

}

static void lcp_timer_start(struct pico_device_ppp *ppp, uint8_t timer_type)
{
    uint8_t count = 0;
    ppp->timer_on |= timer_type;

    if (ppp->timer_val == 0) {
        ppp->timer_val = PICO_PPP_DEFAULT_TIMER;
    }

    if (timer_type == PPP_TIMER_ON_LCPTERM) {
        count = PICO_PPP_DEFAULT_MAX_TERMINATE;
    }

    if (timer_type == PPP_TIMER_ON_LCPREQ) {
        count = PICO_PPP_DEFAULT_MAX_CONFIGURE;
    }

    if (timer_type == 0) {
        ppp->timer_on |= PPP_TIMER_ON_LCPREQ;
        ppp->timer_count = 0;
    }

    if (ppp->timer_count == 0)
        ppp->timer_count = count;
}

static void lcp_zero_restart_count(struct pico_device_ppp *ppp)
{
    lcp_timer_start(ppp, 0);
}

static void lcp_timer_stop(struct pico_device_ppp *ppp, uint8_t timer_type)
{
    ppp->timer_on = (uint8_t)ppp->timer_on & (uint8_t)(~timer_type);
}


#define PPP_FSM_MAX_ACTIONS 3

struct pico_ppp_fsm {
    int next_state;
    void (*event_handler[PPP_FSM_MAX_ACTIONS]) (struct pico_device_ppp *);
};

#define LCPOPT_SET_LOCAL(ppp, opt)          ppp->lcpopt_local |= (uint16_t)(1u << opt)
#define LCPOPT_SET_PEER(ppp, opt)           ppp->lcpopt_peer  |= (uint16_t)(1u << opt)
#define LCPOPT_UNSET_LOCAL(ppp, opt)        ppp->lcpopt_local &= (uint16_t) ~(1u << opt)
#define LCPOPT_UNSET_LOCAL_MASK(ppp, opt)   ppp->lcpopt_local &= (uint16_t) ~(opt)
#define LCPOPT_UNSET_PEER(ppp, opt)         ppp->lcpopt_peer  &= (uint16_t) ~(1u << opt)
#define LCPOPT_ISSET_LOCAL(ppp, opt)      ((ppp->lcpopt_local & (uint16_t)(1u << opt)) != 0)
#define LCPOPT_ISSET_PEER(ppp, opt)       ((ppp->lcpopt_peer  & (uint16_t)(1u << opt)) != 0)

static void evaluate_modem_state(struct pico_device_ppp *ppp, enum ppp_modem_event event);
static void evaluate_lcp_state(struct pico_device_ppp *ppp, enum ppp_lcp_event event);
static void evaluate_auth_state(struct pico_device_ppp *ppp, enum ppp_auth_event event);
static void evaluate_ipcp_state(struct pico_device_ppp *ppp, enum ppp_ipcp_event event);


static uint32_t ppp_ctl_packet_size(struct pico_device_ppp *ppp, uint16_t proto, uint32_t *size)
{
    uint32_t prefix = 0;

    IGNORE_PARAMETER(ppp);
    IGNORE_PARAMETER(proto);

    prefix += PPP_HDR_SIZE; /* 7e ff 03 ... */
    prefix += PPP_PROTO_SLOT_SIZE;
    *size  += prefix;
    *size  += PPP_FCS_SIZE;
    (*size)++; /* STOP byte 0x7e */
    return prefix;
}

/* CRC16 / FCS Calculation */
static uint16_t ppp_fcs_char(uint16_t old_crc, uint8_t data)
{
    uint16_t word = (old_crc ^ data) & (uint16_t)0x00FFu;
    word = (uint16_t)(word ^ (uint16_t)((word << 4u) & (uint16_t)0x00FFu));
    word = (uint16_t)((word << 8u) ^ (word << 3u) ^ (word >> 4u));
    return ((old_crc >> 8u) ^ word);
}

static uint16_t ppp_fcs_continue(uint16_t fcs, uint8_t *buf, uint32_t len)
{
    uint8_t *pos = buf;
    for (pos = buf; pos < buf + len; pos++)
    {
        fcs = ppp_fcs_char(fcs, *pos);
    }
    return fcs;
}

static uint16_t ppp_fcs_finish(uint16_t fcs)
{
    return fcs ^ 0xFFFF;
}

static uint16_t ppp_fcs_start(uint8_t *buf, uint32_t len)
{
    uint16_t fcs = 0xFFFF;
    return ppp_fcs_continue(fcs, buf, len);
}

static int ppp_fcs_verify(uint8_t *buf, uint32_t len)
{
    uint16_t fcs = ppp_fcs_start(buf, len - 2);
    fcs = ppp_fcs_finish(fcs);
    if ((((fcs & 0xFF00u) >> 8) != buf[len - 1]) || ((fcs & 0xFFu) != buf[len - 2])) {
        return -1;
    }

    return 0;
}

/* Serial send (DTE->DCE) functions */
static int pico_ppp_ctl_send(struct pico_device *dev, uint16_t code, uint8_t *pkt, uint32_t len)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *) dev;
    uint16_t fcs;
    uint8_t *ptr = pkt;
    int i = 0;

    if (!ppp->serial_send)
        return (int)len;

    /* PPP Header */
    ptr[i++] = PPPF_FLAG_SEQ;
    ptr[i++] = PPPF_ADDR;
    ptr[i++] = PPPF_CTRL;
    /* protocol */
    ptr[i++] = (uint8_t)(code & 0xFFu);
    ptr[i++] = (uint8_t)((code & 0xFF00u) >> 8);

    /* payload is already in place. Calculate FCS. */
    fcs = ppp_fcs_start(pkt + 1, len - 4); /* FCS excludes: start (1), FCS(2), stop(1), total 4 bytes */
    fcs = ppp_fcs_finish(fcs);
    pkt[len - 3] = (uint8_t)(fcs & 0xFFu);
    pkt[len - 2] = (uint8_t)((fcs & 0xFF00u) >> 8);
    pkt[len - 1] = PPPF_FLAG_SEQ;
    ppp_serial_send_escape(ppp, pkt, (int)len);
    return (int)len;
}

static uint8_t pico_ppp_data_buffer[PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + PICO_PPP_MTU + PPP_FCS_SIZE + 1];
static int pico_ppp_send(struct pico_device *dev, void *buf, int len)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *) dev;
    uint16_t fcs = 0;
    int fcs_start;
    int i = 0;

    ppp_dbg(" >>>>>>>>> PPP OUT\n");

    if (ppp->ipcp_state != PPP_IPCP_STATE_OPENED)
        return len;

    if (!ppp->serial_send)
        return len;

    pico_ppp_data_buffer[i++] = PPPF_FLAG_SEQ;
    if (!LCPOPT_ISSET_PEER(ppp, LCPOPT_ADDRCTL_COMP))
    {
        pico_ppp_data_buffer[i++] = PPPF_ADDR;
        pico_ppp_data_buffer[i++] = PPPF_CTRL;
    }

    fcs_start = i;

    if (!LCPOPT_ISSET_PEER(ppp, LCPOPT_PROTO_COMP))
    {
        pico_ppp_data_buffer[i++] = 0x00;
    }

    pico_ppp_data_buffer[i++] = 0x21;
    memcpy(pico_ppp_data_buffer + i, buf, (uint32_t)len);
    i += len;
    fcs = ppp_fcs_start(pico_ppp_data_buffer + fcs_start, (uint32_t)(i - fcs_start));
    fcs = ppp_fcs_finish(fcs);
    pico_ppp_data_buffer[i++] = (uint8_t)(fcs & 0xFFu);
    pico_ppp_data_buffer[i++] = (uint8_t)((fcs & 0xFF00u) >> 8);
    pico_ppp_data_buffer[i++] = PPPF_FLAG_SEQ;

    ppp_serial_send_escape(ppp, pico_ppp_data_buffer, i);
    return len;
}


/* FSM functions */

static void ppp_modem_start_timer(struct pico_device_ppp *ppp)
{
    ppp->timer_on = ppp->timer_on | PPP_TIMER_ON_MODEM;
    ppp->timer_val = PICO_PPP_DEFAULT_TIMER;
}

#define PPP_AT_CREG0 "ATZ\r\n"
static void ppp_modem_send_reset(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CREG0, strlen(PPP_AT_CREG0));

    ppp_modem_start_timer(ppp);
}

#define PPP_AT_CREG1 "ATE0\r\n"
static void ppp_modem_send_echo(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CREG1, strlen(PPP_AT_CREG1));

    ppp_modem_start_timer(ppp);
}

#define PPP_AT_CREG2 "AT+CREG=1\r\n"
static void ppp_modem_send_creg(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CREG2, strlen(PPP_AT_CREG2));

    ppp_modem_start_timer(ppp);
}

#define PPP_AT_CGREG "AT+CGREG=1\r\n"
static void ppp_modem_send_cgreg(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CGREG, strlen(PPP_AT_CGREG));

    ppp_modem_start_timer(ppp);
}


#define PPP_AT_CGDCONT "AT+CGDCONT=1,\"IP\",\"%s\",,,\r\n"
static void ppp_modem_send_cgdcont(struct pico_device_ppp *ppp)
{
    char at_cgdcont[200];

    if (ppp->serial_send) {
        snprintf(at_cgdcont, 200, PPP_AT_CGDCONT, ppp->apn);
        ppp->serial_send(&ppp->dev, at_cgdcont, (int)strlen(at_cgdcont));
    }

    ppp_modem_start_timer(ppp);
}


#define PPP_AT_CGATT "AT+CGATT=1\r\n"
static void ppp_modem_send_cgatt(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CGATT, strlen(PPP_AT_CGATT));

    ppp_modem_start_timer(ppp);
}

#ifdef PICOTCP_PPP_SUPPORT_QUERIES
#define PPP_AT_CGATT_Q "AT+CGATT?\r\n"
static void ppp_modem_send_cgatt_q(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CGATT_Q, strlen(PPP_AT_CGATT_Q));

    ppp_modem_start_timer(ppp);
}
#define PPP_AT_CGDCONT_Q "AT+CGDCONT?\r\n"
static void ppp_modem_send_cgdcont_q(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CGDCONT_Q, strlen(PPP_AT_CGDCONT_Q));

    ppp_modem_start_timer(ppp);
}

#define PPP_AT_CGREG_Q "AT+CGREG?\r\n"
static void ppp_modem_send_cgreg_q(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CGREG_Q, strlen(PPP_AT_CGREG_Q));

    ppp_modem_start_timer(ppp);
}

#define PPP_AT_CREG3 "AT+CREG?\r\n"
static void ppp_modem_send_creg_q(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_CREG3, strlen(PPP_AT_CREG3));

    ppp_modem_start_timer(ppp);
}
#endif /* PICOTCP_PPP_SUPPORT_QUERIES */

#define PPP_AT_DIALIN "ATD*99***1#\r\n"
static void ppp_modem_send_dial(struct pico_device_ppp *ppp)
{
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_AT_DIALIN, strlen(PPP_AT_DIALIN));

    ppp_modem_start_timer(ppp);
    ppp->timer_val = PICO_PPP_DEFAULT_MAX_DIALTIME;
}

static void ppp_modem_connected(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: Modem connected to peer.\n");
    evaluate_lcp_state(ppp, PPP_LCP_EVENT_UP);
}

#define PPP_ATH "+++ATH\r\n"
static void ppp_modem_disconnected(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: Modem disconnected.\n");
    if (ppp->serial_send)
        ppp->serial_send(&ppp->dev, PPP_ATH, strlen(PPP_ATH));

    evaluate_lcp_state(ppp, PPP_LCP_EVENT_DOWN);
}

static const struct pico_ppp_fsm ppp_modem_fsm[PPP_MODEM_STATE_MAX][PPP_MODEM_EVENT_MAX] = {
    [PPP_MODEM_STATE_INITIAL] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_RESET, {ppp_modem_send_reset} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_INITIAL, {ppp_modem_send_reset} }
    },
    [PPP_MODEM_STATE_RESET] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_RESET, {} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_ECHO, { ppp_modem_send_echo } },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_RESET, {} },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_RESET, {ppp_modem_send_reset} }
    },
    [PPP_MODEM_STATE_ECHO] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_ECHO, {} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_CREG, { ppp_modem_send_creg } },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_ECHO, {} },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_RESET, {ppp_modem_send_reset} }
    },
    [PPP_MODEM_STATE_CREG] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_CREG, {} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_CGREG, { ppp_modem_send_cgreg } },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_CREG, {} },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_RESET, {ppp_modem_send_reset} }
    },
    [PPP_MODEM_STATE_CGREG] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_CGREG, {} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_CGDCONT, { ppp_modem_send_cgdcont } },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_CGREG, {} },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_RESET, {ppp_modem_send_reset} }
    },
    [PPP_MODEM_STATE_CGDCONT] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_CGDCONT, {} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_CGATT, { ppp_modem_send_cgatt } },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_CGDCONT, {} },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_RESET, {ppp_modem_send_reset} }
    },
    [PPP_MODEM_STATE_CGATT] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_CGATT, {} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_DIAL, { ppp_modem_send_dial } },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_CGATT, {} },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_RESET, {ppp_modem_send_reset} }
    },
    [PPP_MODEM_STATE_DIAL] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_DIAL, {} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, {} },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_DIAL, {} },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_CONNECTED, { ppp_modem_connected } },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_RESET, {ppp_modem_send_reset} }
    },
    [PPP_MODEM_STATE_CONNECTED] = {
        [PPP_MODEM_EVENT_START]   = { PPP_MODEM_STATE_CONNECTED, {} },
        [PPP_MODEM_EVENT_STOP]    = { PPP_MODEM_STATE_INITIAL, { ppp_modem_disconnected } },
        [PPP_MODEM_EVENT_OK]      = { PPP_MODEM_STATE_CONNECTED, {} },
        [PPP_MODEM_EVENT_CONNECT] = { PPP_MODEM_STATE_CONNECTED, {} },
        [PPP_MODEM_EVENT_TIMEOUT] = { PPP_MODEM_STATE_CONNECTED, {} }
    }
};
static void evaluate_modem_state(struct pico_device_ppp *ppp, enum ppp_modem_event event)
{
    const struct pico_ppp_fsm *fsm;
    int i;
    if (mock_modem_state) {
        mock_modem_state(ppp, event);
        return;
    }

    fsm = &ppp_modem_fsm[ppp->modem_state][event];

    ppp->modem_state = (enum ppp_modem_state)fsm->next_state;

    for (i = 0; i < PPP_FSM_MAX_ACTIONS; i++) {
        if (fsm->event_handler[i])
            fsm->event_handler[i](ppp);
    }
}

static void ppp_modem_recv(struct pico_device_ppp *ppp, void *data, uint32_t len)
{
    IGNORE_PARAMETER(len);

    ppp_dbg("PPP: Recv: '%s'\n", (char *)data);

    if (strcmp(data, "OK") == 0) {
        evaluate_modem_state(ppp, PPP_MODEM_EVENT_OK);
    }

    if (strcmp(data, "ERROR") == 0) {
        evaluate_modem_state(ppp, PPP_MODEM_EVENT_STOP);
    }

    if (strncmp(data, "CONNECT", 7) == 0) {
        evaluate_modem_state(ppp, PPP_MODEM_EVENT_CONNECT);
    }
}

static void lcp_send_configure_request(struct pico_device_ppp *ppp)
{
#   define MY_LCP_REQ_SIZE 12 /* Max value. */
    struct pico_lcp_hdr *req;
    uint8_t *lcpbuf, *opts;
    uint32_t size = MY_LCP_REQ_SIZE;
    uint32_t prefix;
    uint32_t optsize = 0;

    prefix = ppp_ctl_packet_size(ppp, PPP_PROTO_LCP, &size);
    lcpbuf = PICO_ZALLOC(size);
    if (!lcpbuf)
        return;

    req = (struct pico_lcp_hdr *)(lcpbuf + prefix);

    opts = lcpbuf + prefix + (sizeof(struct pico_lcp_hdr));
    /* uint8_t my_pkt[] = { 0x7e, 0xff, 0x03, 0xc0, 0x21, 0x01, 0x00, 0x00, 0x06, 0x07, 0x02, 0x64, 0x7b, 0x7e }; */

    ppp_dbg("Sending LCP CONF REQ\n");
    req->code = PICO_CONF_REQ;
    req->id = ppp->frame_id++;

    if (LCPOPT_ISSET_LOCAL(ppp, LCPOPT_PROTO_COMP)) {
        opts[optsize++] = LCPOPT_PROTO_COMP;
        opts[optsize++] = LCPOPT_LEN[LCPOPT_PROTO_COMP];
    }

    if (LCPOPT_ISSET_LOCAL(ppp, LCPOPT_MRU)) {
        opts[optsize++] = LCPOPT_MRU;
        opts[optsize++] = LCPOPT_LEN[LCPOPT_MRU];
        opts[optsize++] = (uint8_t)((ppp->mru >> 8) & 0xFF);
        opts[optsize++] = (uint8_t)(ppp->mru & 0xFF);
    } else {
        ppp->mru = PICO_PPP_MRU;
    }

    if (LCPOPT_ISSET_LOCAL(ppp, LCPOPT_ADDRCTL_COMP)) {
        opts[optsize++] = LCPOPT_ADDRCTL_COMP;
        opts[optsize++] = LCPOPT_LEN[LCPOPT_ADDRCTL_COMP];
    }

    req->len = short_be((uint16_t)((unsigned long)optsize + sizeof(struct pico_lcp_hdr)));

#ifdef PPP_DEBUG
    lcp_optflags_print(ppp, opts, optsize);
#endif

    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_LCP,
                      lcpbuf,               /* Start of PPP packet */
                      (uint32_t)(prefix +              /* PPP Header, etc. */
                                 sizeof(struct pico_lcp_hdr) + /* LCP HDR */
                                 optsize +  /* Actual options size */
                                 PPP_FCS_SIZE + /* FCS at the end of the frame */
                                 1u)          /* STOP Byte */
                      );
    PICO_FREE(lcpbuf);
    ppp->timer_val = PICO_PPP_DEFAULT_TIMER;
    lcp_timer_start(ppp, PPP_TIMER_ON_LCPREQ);
}

#ifdef PPP_DEBUG
static void lcp_optflags_print(struct pico_device_ppp *ppp, uint8_t *opts, uint32_t opts_len)
{
    uint8_t *p = opts;
    int off;
    IGNORE_PARAMETER(ppp);
    ppp_dbg("Parsing options:\n");
    while(p < (opts + opts_len)) {
        int i;

        ppp_dbg("-- LCP opt: %d - len: %d - data:", p[0], p[1]);
        for (i = 0; i < p[1] - 2; i++)
        {
            ppp_dbg(" %02X", p[2 + i]);
        }
        ppp_dbg("\n");

        off = p[1];
        if (!off)
            break;

        p += off;
    }
}
#endif

/* setting adjust_opts will adjust our options to the ones supplied */
static uint16_t lcp_optflags(struct pico_device_ppp *ppp, uint8_t *pkt, uint32_t len, int adjust_opts)
{
    uint16_t flags = 0;
    uint8_t *p = pkt +  sizeof(struct pico_lcp_hdr);
    int off;
    while(p < (pkt + len)) {
        flags = (uint16_t)((uint16_t)(1u << (uint16_t)p[0]) | flags);

        if (adjust_opts && ppp)
        {
            switch (p[0])
            {
            case LCPOPT_MRU:
                /* XXX: Can we accept any MRU ? */
                ppp_dbg("Adjusting MRU to %02x%02x\n", p[2], p[3]);
                ppp->mru = (uint16_t)((p[2] << 8) + p[3]);
                break;
            case LCPOPT_AUTH:
                ppp_dbg("Setting AUTH to %02x%02x\n", p[2], p[3]);
                ppp->auth = (uint16_t)((p[2] << 8) + p[3]);
                break;
            default:
                break;
            }
        }

        off = p[1]; /* opt length field */
        if (!off)
            break;

        p += off;
    }
#ifdef PPP_DEBUG
    lcp_optflags_print(ppp, pkt +  sizeof(struct pico_lcp_hdr), (uint32_t)(len - sizeof(struct pico_lcp_hdr)));
#endif
    return flags;
}


static void lcp_send_configure_ack(struct pico_device_ppp *ppp)
{
    uint8_t ack[ppp->len + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_lcp_hdr) + PPP_FCS_SIZE + 1];
    struct pico_lcp_hdr *ack_hdr = (struct pico_lcp_hdr *) (ack + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE);
    struct pico_lcp_hdr *lcpreq = (struct pico_lcp_hdr *)ppp->pkt;
    memcpy(ack + PPP_HDR_SIZE +  PPP_PROTO_SLOT_SIZE, ppp->pkt, ppp->len);
    ack_hdr->code = PICO_CONF_ACK;
    ack_hdr->id = lcpreq->id;
    ack_hdr->len = lcpreq->len;
    ppp_dbg("Sending LCP CONF ACK\n");
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_LCP, ack,
                      PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE +  /* PPP Header, etc. */
                      short_be(lcpreq->len) +               /* Actual options size + hdr (whole lcp packet) */
                      PPP_FCS_SIZE +                        /* FCS at the end of the frame */
                      1                                     /* STOP Byte */
                      );
}

static void lcp_send_terminate_request(struct pico_device_ppp *ppp)
{
    uint8_t term[PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_lcp_hdr) + PPP_FCS_SIZE + 1];
    struct pico_lcp_hdr *term_hdr = (struct pico_lcp_hdr *) (term + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE);
    term_hdr->code = PICO_CONF_TERM;
    term_hdr->id = ppp->frame_id++;
    term_hdr->len = short_be((uint16_t)sizeof(struct pico_lcp_hdr));
    ppp_dbg("Sending LCP TERMINATE REQUEST\n");
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_LCP, term,
                      PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE +  /* PPP Header, etc. */
                      sizeof(struct pico_lcp_hdr) +         /* Actual options size + hdr (whole lcp packet) */
                      PPP_FCS_SIZE +                        /* FCS at the end of the frame */
                      1                                     /* STOP Byte */
                      );
    lcp_timer_start(ppp, PPP_TIMER_ON_LCPTERM);
}

static void lcp_send_terminate_ack(struct pico_device_ppp *ppp)
{
    uint8_t ack[ppp->len + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_lcp_hdr) + PPP_FCS_SIZE + 1];
    struct pico_lcp_hdr *ack_hdr = (struct pico_lcp_hdr *) (ack + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE);
    struct pico_lcp_hdr *lcpreq = (struct pico_lcp_hdr *)ppp->pkt;
    memcpy(ack + PPP_HDR_SIZE +  PPP_PROTO_SLOT_SIZE, ppp->pkt, ppp->len);
    ack_hdr->code = PICO_CONF_TERM_ACK;
    ack_hdr->id = lcpreq->id;
    ack_hdr->len = lcpreq->len;
    ppp_dbg("Sending LCP TERM ACK\n");
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_LCP, ack,
                      PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE +  /* PPP Header, etc. */
                      short_be(lcpreq->len) +               /* Actual options size + hdr (whole lcp packet) */
                      PPP_FCS_SIZE +                        /* FCS at the end of the frame */
                      1                                     /* STOP Byte */
                      );
}

static void lcp_send_configure_nack(struct pico_device_ppp *ppp)
{
    uint8_t reject[64];
    uint8_t *p = ppp->pkt +  sizeof(struct pico_lcp_hdr);
    struct pico_lcp_hdr *lcpreq = (struct pico_lcp_hdr *)ppp->pkt;
    struct pico_lcp_hdr *lcprej = (struct pico_lcp_hdr *)(reject + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE);
    uint8_t *dst_opts = reject + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_lcp_hdr);
    uint32_t dstopts_len = 0;
    ppp_dbg("CONF_NACK: rej = %04X\n", ppp->rej);
    while (p < (ppp->pkt + ppp->len)) {
        uint8_t i = 0;
        if ((1u << p[0]) & ppp->rej || (p[0] > 8u)) {       /* Reject anything we dont support or with option id >8 */
            ppp_dbg("rejecting option %d -- ", p[0]);
            dst_opts[dstopts_len++] = p[0];

            ppp_dbg("len: %d -- ", p[1]);
            dst_opts[dstopts_len++] = p[1];

            ppp_dbg("data: ");
            for(i = 0; i < p[1] - 2u; i++) {                   /* length includes type, length and data fields */
                dst_opts[dstopts_len++] = p[2 + i];
                ppp_dbg("%02X ", p[2 + i]);
            }
            ppp_dbg("\n");
        }

        p += p[1];
    }
    lcprej->code = PICO_CONF_REJ;
    lcprej->id = lcpreq->id;
    lcprej->len = short_be((uint16_t)(dstopts_len + sizeof(struct pico_lcp_hdr)));

    ppp_dbg("Sending LCP CONF REJ\n");
#ifdef PPP_DEBUG
    lcp_optflags_print(ppp, dst_opts, dstopts_len);
#endif

    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_LCP, reject,
                      (uint32_t)(PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE +  /* PPP Header, etc. */
                                 sizeof(struct pico_lcp_hdr) + /* LCP HDR */
                                 dstopts_len +              /* Actual options size */
                                 PPP_FCS_SIZE +             /* FCS at the end of the frame */
                                 1u)                          /* STOP Byte */
                      );
}

static void lcp_process_in(struct pico_device_ppp *ppp, uint8_t *pkt, uint32_t len)
{
    uint16_t optflags;
    if (!ppp)
        return;

    if (pkt[0] == PICO_CONF_REQ) {
        uint16_t rejected = 0;
        ppp_dbg("Received LCP CONF REQ\n");
        optflags = lcp_optflags(ppp, pkt, len, 1u);
        rejected = (uint16_t)(optflags & (~ppp->lcpopt_local));
        ppp->pkt = pkt;
        ppp->len = len;
        ppp->rej = rejected;
        if (rejected) {
            evaluate_lcp_state(ppp, PPP_LCP_EVENT_RCR_NEG);
        } else {
            ppp->lcpopt_peer = optflags;
            evaluate_lcp_state(ppp, PPP_LCP_EVENT_RCR_POS);
        }

        return;
    }

    if (pkt[0] == PICO_CONF_ACK) {
        ppp_dbg("Received LCP CONF ACK\nOptflags: %04x\n", lcp_optflags(NULL, pkt, len, 0u));
        evaluate_lcp_state(ppp, PPP_LCP_EVENT_RCA);
        return;
    }

    if (pkt[0] == PICO_CONF_NAK) {
        /* Every instance of the received Configuration Options is recognizable, but some values are not acceptable */
        optflags = lcp_optflags(ppp, pkt, len, 1u); /* We want our options adjusted */
        ppp_dbg("Received LCP CONF NAK - changed optflags: %04X\n", optflags);
        evaluate_lcp_state(ppp, PPP_LCP_EVENT_RCN);
        return;
    }

    if (pkt[0] == PICO_CONF_REJ) {
        /* Some Configuration Options received in a Configure-Request are not recognizable or are not acceptable for negotiation */
        optflags = lcp_optflags(ppp, pkt, len, 0u);
        ppp_dbg("Received LCP CONF REJ - will disable optflags: %04X\n", optflags);
        /* Disable the options that are not supported by the peer */
        LCPOPT_UNSET_LOCAL_MASK(ppp, optflags);
        evaluate_lcp_state(ppp, PPP_LCP_EVENT_RCN);
        return;
    }

    if (pkt[0] == PICO_CONF_ECHO_REQ) {
        ppp_dbg("Received LCP ECHO REQ\n");
        evaluate_lcp_state(ppp, PPP_LCP_EVENT_RXR);
        return;
    }
}

static void pap_process_in(struct pico_device_ppp *ppp, uint8_t *pkt, uint32_t len)
{
    struct pico_pap_hdr *p = (struct pico_pap_hdr *)pkt;
    (void)len;
    if (!p)
        return;

    if (ppp->auth != 0xc023)
        return;

    switch(p->code) {
    case PAP_AUTH_ACK:
        ppp_dbg("PAP: Received Authentication OK!\n");
        evaluate_auth_state(ppp, PPP_AUTH_EVENT_RAA);
        break;
    case PAP_AUTH_NAK:
        ppp_dbg("PAP: Received Authentication Reject!\n");
        evaluate_auth_state(ppp, PPP_AUTH_EVENT_RAN);
        break;

    default:
        ppp_dbg("PAP: Received invalid packet with code %d\n", p->code);
    }
}


static void chap_process_in(struct pico_device_ppp *ppp, uint8_t *pkt, uint32_t len)
{
    struct pico_chap_hdr *ch = (struct pico_chap_hdr *)pkt;

    if (!pkt)
        return;

    if (ppp->auth != 0xc223)
        return;

    switch(ch->code) {
    case CHAP_CHALLENGE:
        ppp_dbg("Received CHAP CHALLENGE\n");
        ppp->pkt = pkt;
        ppp->len = len;
        evaluate_auth_state(ppp, PPP_AUTH_EVENT_RAC);
        break;
    case CHAP_SUCCESS:
        ppp_dbg("Received CHAP SUCCESS\n");
        evaluate_auth_state(ppp, PPP_AUTH_EVENT_RAA);
        break;
    case CHAP_FAILURE:
        ppp_dbg("Received CHAP FAILURE\n");
        evaluate_auth_state(ppp, PPP_AUTH_EVENT_RAN);
        break;
    }
}


static void ipcp_send_ack(struct pico_device_ppp *ppp)
{
    uint8_t ack[ppp->len + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_lcp_hdr) + PPP_FCS_SIZE + 1];
    struct pico_ipcp_hdr *ack_hdr = (struct pico_ipcp_hdr *) (ack + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE);
    struct pico_ipcp_hdr *ipcpreq = (struct pico_ipcp_hdr *)ppp->pkt;
    memcpy(ack + PPP_HDR_SIZE +  PPP_PROTO_SLOT_SIZE, ppp->pkt, ppp->len);
    ack_hdr->code = PICO_CONF_ACK;
    ack_hdr->id = ipcpreq->id;
    ack_hdr->len = ipcpreq->len;
    ppp_dbg("Sending IPCP CONF ACK\n");
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_IPCP, ack,
                      PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE +  /* PPP Header, etc. */
                      short_be(ipcpreq->len) +               /* Actual options size + hdr (whole ipcp packet) */
                      PPP_FCS_SIZE +                        /* FCS at the end of the frame */
                      1                                     /* STOP Byte */
                      );
}

static inline uint32_t ipcp_request_options_size(struct pico_device_ppp *ppp)
{
    uint32_t size = 0;

/*    if (ppp->ipcp_ip) */
    size += IPCP_ADDR_LEN;
/*    if (ppp->ipcp_dns1) */
    size += IPCP_ADDR_LEN;
/*    if (ppp->ipcp_dns2) */
    size += IPCP_ADDR_LEN;
    if (ppp->ipcp_nbns1)
        size += IPCP_ADDR_LEN;

    if (ppp->ipcp_nbns2)
        size += IPCP_ADDR_LEN;

    return size;
}

static int ipcp_request_add_address(uint8_t *dst, uint8_t tag, uint32_t arg)
{
    uint32_t addr = long_be(arg);
    dst[0] = tag;
    dst[1] = IPCP_ADDR_LEN;
    dst[2] = (uint8_t)((addr & 0xFF000000u) >> 24);
    dst[3] = (uint8_t)((addr & 0x00FF0000u) >> 16);
    dst[4] = (uint8_t)((addr & 0x0000FF00u) >> 8);
    dst[5] = (addr & 0x000000FFu);
    return IPCP_ADDR_LEN;
}

static void ipcp_request_fill(struct pico_device_ppp *ppp, uint8_t *opts)
{
    if (ppp->ipcp_allowed_fields & IPCP_ALLOW_IP)
        opts += ipcp_request_add_address(opts, IPCP_OPT_IP, ppp->ipcp_ip);

    if (ppp->ipcp_allowed_fields & IPCP_ALLOW_DNS1)
        opts += ipcp_request_add_address(opts, IPCP_OPT_DNS1, ppp->ipcp_dns1);

    if (ppp->ipcp_allowed_fields & IPCP_ALLOW_DNS2)
        opts += ipcp_request_add_address(opts, IPCP_OPT_DNS2, ppp->ipcp_dns2);

    if ((ppp->ipcp_allowed_fields & IPCP_ALLOW_NBNS1) &&  (ppp->ipcp_nbns1))
        opts += ipcp_request_add_address(opts, IPCP_OPT_NBNS1, ppp->ipcp_nbns1);

    if ((ppp->ipcp_allowed_fields & IPCP_ALLOW_NBNS2) &&  (ppp->ipcp_nbns2))
        opts += ipcp_request_add_address(opts, IPCP_OPT_NBNS2, ppp->ipcp_nbns2);
}

static void ipcp_send_req(struct pico_device_ppp *ppp)
{
    uint8_t ipcp_req[PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_ipcp_hdr) + ipcp_request_options_size(ppp) + PPP_FCS_SIZE + 1];
    uint32_t prefix = PPP_HDR_SIZE +  PPP_PROTO_SLOT_SIZE;
    struct pico_ipcp_hdr *ih = (struct pico_ipcp_hdr *) (ipcp_req + prefix);
    uint8_t *p = ipcp_req + prefix + sizeof(struct pico_ipcp_hdr);
    uint16_t len = (uint16_t)(ipcp_request_options_size(ppp) + sizeof(struct pico_ipcp_hdr));

    ih->id = ppp->frame_id++;
    ih->code = PICO_CONF_REQ;
    ih->len = short_be(len);
    ipcp_request_fill(ppp, p);

    ppp_dbg("Sending IPCP CONF REQ, ipcp size = %d\n", len);
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_IPCP,
                      ipcp_req,             /* Start of PPP packet */
                      (uint32_t)(prefix +   /* PPP Header, etc. */
                                 (uint32_t)len + /* IPCP Header + options */
                                 PPP_FCS_SIZE + /* FCS at the end of the frame */
                                 1u)        /* STOP Byte */
                      );
}

static void ipcp_reject_vj(struct pico_device_ppp *ppp, uint8_t *comp_req)
{
    uint8_t ipcp_req[PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_ipcp_hdr) + IPCP_VJ_LEN + PPP_FCS_SIZE + 1];
    uint32_t prefix = PPP_HDR_SIZE +  PPP_PROTO_SLOT_SIZE;
    struct pico_ipcp_hdr *ih = (struct pico_ipcp_hdr *) (ipcp_req + prefix);
    uint8_t *p = ipcp_req + prefix + sizeof(struct pico_ipcp_hdr);
    uint32_t i;

    ih->id = ppp->frame_id++;
    ih->code = PICO_CONF_REQ;
    ih->len = short_be(IPCP_VJ_LEN + sizeof(struct pico_ipcp_hdr));
    for(i = 0; i < IPCP_OPT_VJ; i++)
        p[i] = comp_req[i + sizeof(struct pico_ipcp_hdr)];
    ppp_dbg("Sending IPCP CONF REJ VJ\n");
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_IPCP,
                      ipcp_req,             /* Start of PPP packet */
                      (uint32_t)(prefix +              /* PPP Header, etc. */
                                 sizeof(struct pico_ipcp_hdr) + /* LCP HDR */
                                 IPCP_VJ_LEN + /* Actual options size */
                                 PPP_FCS_SIZE + /* FCS at the end of the frame */
                                 1u)         /* STOP Byte */
                      );
}

static void ppp_ipv4_conf(struct pico_device_ppp *ppp)
{
    struct pico_ip4 ip;
    struct pico_ip4 nm;
    struct pico_ip4 dns1;
    struct pico_ip4 dns2;
    struct pico_ip4 any = {
        0
    };
    ip.addr = ppp->ipcp_ip;
    nm.addr = 0xFFFFFF00;
    pico_ipv4_link_add(&ppp->dev, ip, nm);
    pico_ipv4_route_add(any, any, any, 1, pico_ipv4_link_by_dev(&ppp->dev));

    dns1.addr = ppp->ipcp_dns1;
    dns2.addr = ppp->ipcp_dns2;
    pico_dns_client_nameserver(&dns1, PICO_DNS_NS_ADD);
    pico_dns_client_nameserver(&dns2, PICO_DNS_NS_ADD);
}


static void ipcp_process_in(struct pico_device_ppp *ppp, uint8_t *pkt, uint32_t len)
{
    struct pico_ipcp_hdr *ih = (struct pico_ipcp_hdr *)pkt;
    uint8_t *p = pkt + sizeof(struct pico_ipcp_hdr);
    int reject = 0;
    while (p < pkt + len) {
        if (p[0] == IPCP_OPT_VJ) {
            reject++;
        }

        if (p[0] == IPCP_OPT_IP) {
            if (ih->code != PICO_CONF_REJ)
                ppp->ipcp_ip = long_be((uint32_t)((p[2] << 24) + (p[3] << 16) + (p[4] << 8) + p[5]));
            else {
                ppp->ipcp_allowed_fields &= (~IPCP_ALLOW_IP);
                ppp->ipcp_ip = 0;
            }
        }

        if (p[0] == IPCP_OPT_DNS1) {
            if (ih->code != PICO_CONF_REJ)
                ppp->ipcp_dns1 = long_be((uint32_t)((p[2] << 24) + (p[3] << 16) + (p[4] << 8) + p[5]));
            else {
                ppp->ipcp_allowed_fields &= (~IPCP_ALLOW_DNS1);
                ppp->ipcp_dns1 = 0;
            }
        }

        if (p[0] == IPCP_OPT_NBNS1) {
            if (ih->code != PICO_CONF_REJ)
                ppp->ipcp_nbns1 = long_be((uint32_t)((p[2] << 24) + (p[3] << 16) + (p[4] << 8) + p[5]));
            else {
                ppp->ipcp_allowed_fields &= (~IPCP_ALLOW_NBNS1);
                ppp->ipcp_nbns1 = 0;
            }
        }

        if (p[0] == IPCP_OPT_DNS2) {
            if (ih->code != PICO_CONF_REJ)
                ppp->ipcp_dns2 = long_be((uint32_t)((p[2] << 24) + (p[3] << 16) + (p[4] << 8) + p[5]));
            else {
                ppp->ipcp_allowed_fields &= (~IPCP_ALLOW_DNS2);
                ppp->ipcp_dns2 = 0;
            }
        }

        if (p[0] == IPCP_OPT_NBNS2) {
            if (ih->code != PICO_CONF_REJ)
                ppp->ipcp_nbns2 = long_be((uint32_t)((p[2] << 24) + (p[3] << 16) + (p[4] << 8) + p[5]));
            else {
                ppp->ipcp_allowed_fields &= (~IPCP_ALLOW_NBNS2);
                ppp->ipcp_nbns2 = 0;
            }
        }

        p += p[1];
    }
    if (reject) {
        ipcp_reject_vj(ppp, p);
        return;
    }

    ppp->pkt = pkt;
    ppp->len = len;

    switch(ih->code) {
    case PICO_CONF_ACK:
        ppp_dbg("Received IPCP CONF ACK\n");
        evaluate_ipcp_state(ppp, PPP_IPCP_EVENT_RCA);
        break;
    case PICO_CONF_REQ:
        ppp_dbg("Received IPCP CONF REQ\n");
        evaluate_ipcp_state(ppp, PPP_IPCP_EVENT_RCR_POS);
        break;
    case PICO_CONF_NAK:
        ppp_dbg("Received IPCP CONF NAK\n");
        evaluate_ipcp_state(ppp, PPP_IPCP_EVENT_RCN);
        break;
    case PICO_CONF_REJ:
        ppp_dbg("Received IPCP CONF REJ\n");

        evaluate_ipcp_state(ppp, PPP_IPCP_EVENT_RCN);
        break;
    }
}

static void ipcp6_process_in(struct pico_device_ppp *ppp, uint8_t *pkt, uint32_t len)
{
    IGNORE_PARAMETER(ppp);
    IGNORE_PARAMETER(pkt);
    IGNORE_PARAMETER(len);
}

static void ppp_process_packet_payload(struct pico_device_ppp *ppp, uint8_t *pkt, uint32_t len)
{
    if (pkt[0] == 0xc0) {
        /* Link control packet */
        if (pkt[1] == 0x21) {
            /* LCP */
            lcp_process_in(ppp, pkt + 2, len - 2);
        }

        if (pkt[1] == 0x23) {
            /* PAP */
            pap_process_in(ppp, pkt + 2, len - 2);
        }

        return;
    }

    if ((pkt[0] == 0xc2) && (pkt[1] == 0x23)) {
        /*  CHAP */
        chap_process_in(ppp, pkt + 2, len - 2);
        return;
    }

    if (pkt[0] == 0x80) {
        /* IP assignment (IPCP/IPCP6) */
        if (pkt[1] == 0x21) {
            /* IPCP */
            ipcp_process_in(ppp, pkt + 2, len - 2);
        }

        if (pkt[1] == 0x57) {
            /* IPCP6 */
            ipcp6_process_in(ppp, pkt + 2, len - 2);
        }

        return;
    }

    if (pkt[0] == 0x00) {
        /* Uncompressed protocol: leading zero. */
        pkt++;
        len--;
    }

    if ((pkt[0] == 0x21) || (pkt[0] == 0x57)) {
        /* IPv4 /v6 Data */
        pico_stack_recv(&ppp->dev, pkt + 1, len - 1);
        return;
    }

    ppp_dbg("PPP: Unrecognized protocol %02x%02x\n", pkt[0], pkt[1]);
}

static void ppp_process_packet(struct pico_device_ppp *ppp, uint8_t *pkt, uint32_t len)
{
    /* Verify incoming FCS */
    if (ppp_fcs_verify(pkt, len) != 0)
        return;

    /* Remove trailing FCS */
    len -= 2;

    /* Remove ADDR/CTRL, then process */
    if ((pkt[0] == PPPF_ADDR) && (pkt[1] == PPPF_CTRL)) {
        pkt += 2;
        len -= 2;
    }

    ppp_process_packet_payload(ppp, pkt, len);

}

static void ppp_recv_data(struct pico_device_ppp *ppp, void *data, uint32_t len)
{
    uint8_t *pkt = (uint8_t *)data;

#ifdef PPP_DEBUG
    uint32_t idx;
    if (len > 0) {
        ppp_dbg("PPP   <<<<< ");
        for(idx = 0; idx < len; idx++) {
            ppp_dbg(" %02x", ((uint8_t *)data)[idx]);
        }
        ppp_dbg("\n");
    }

#endif

    ppp_process_packet(ppp, pkt, len);
}

static void lcp_this_layer_up(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: LCP up.\n");

    switch (ppp->auth) {
    case 0x0000:
        evaluate_auth_state(ppp, PPP_AUTH_EVENT_UP_NONE);
        break;
    case 0xc023:
        evaluate_auth_state(ppp, PPP_AUTH_EVENT_UP_PAP);
        break;
    case 0xc223:
        evaluate_auth_state(ppp, PPP_AUTH_EVENT_UP_CHAP);
        break;
    default:
        ppp_dbg("PPP: Unknown authentication protocol.\n");
        break;
    }
}

static void lcp_this_layer_down(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: LCP down.\n");
    evaluate_auth_state(ppp, PPP_AUTH_EVENT_DOWN);
}

static void lcp_this_layer_started(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: LCP started.\n");
    evaluate_modem_state(ppp, PPP_MODEM_EVENT_START);
}

static void lcp_this_layer_finished(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: LCP finished.\n");
    evaluate_modem_state(ppp, PPP_MODEM_EVENT_STOP);
}

static void lcp_initialize_restart_count(struct pico_device_ppp *ppp)
{
    lcp_timer_start(ppp, PPP_TIMER_ON_LCPREQ);
}

static void lcp_send_code_reject(struct pico_device_ppp *ppp)
{
    IGNORE_PARAMETER(ppp);
}

static void lcp_send_echo_reply(struct pico_device_ppp *ppp)
{
    uint8_t reply[ppp->len + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_lcp_hdr) + PPP_FCS_SIZE + 1];
    struct pico_lcp_hdr *reply_hdr = (struct pico_lcp_hdr *) (reply + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE);
    struct pico_lcp_hdr *lcpreq = (struct pico_lcp_hdr *)ppp->pkt;
    memcpy(reply + PPP_HDR_SIZE +  PPP_PROTO_SLOT_SIZE, ppp->pkt, ppp->len);
    reply_hdr->code = PICO_CONF_ECHO_REP;
    reply_hdr->id = lcpreq->id;
    reply_hdr->len = lcpreq->len;
    ppp_dbg("Sending LCP ECHO REPLY\n");
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_LCP, reply,
                      PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE +  /* PPP Header, etc. */
                      short_be(lcpreq->len) +               /* Actual options size + hdr (whole lcp packet) */
                      PPP_FCS_SIZE +                        /* FCS at the end of the frame */
                      1                                     /* STOP Byte */
                      );
}

static const struct pico_ppp_fsm ppp_lcp_fsm[PPP_LCP_STATE_MAX][PPP_LCP_EVENT_MAX] = {
    [PPP_LCP_STATE_INITIAL] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_CLOSED, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_STARTING, { lcp_this_layer_started } },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_INITIAL, {} }
    },
    [PPP_LCP_STATE_STARTING] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_REQ_SENT, {  lcp_send_configure_request } },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_INITIAL, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_STARTING, {} }
    },
    [PPP_LCP_STATE_CLOSED] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_CLOSED, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_REQ_SENT, { lcp_send_configure_request} },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_CLOSED, {} },
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_CLOSED, {} },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_CLOSED, {} },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_CLOSED, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_CLOSED, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_CLOSED, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_CLOSED, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_CLOSED, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_CLOSED, {} },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_CLOSED, { lcp_send_code_reject } },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_CLOSED, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_CLOSED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_CLOSED, {} }
    },
    [PPP_LCP_STATE_STOPPED] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_STOPPED, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_STARTING, { lcp_this_layer_started } },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_STOPPED, {}},
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_CLOSED, {}},
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_STOPPED, {} },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_STOPPED, {} },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_ACK_SENT,
                                    { lcp_send_configure_request, lcp_send_configure_ack}},
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_REQ_SENT,
                                    { lcp_send_configure_request, lcp_send_configure_nack}},
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_STOPPED, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_STOPPED, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_STOPPED, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_STOPPED, {} },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_STOPPED, { lcp_send_code_reject } },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_STOPPED, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_STOPPED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_STOPPED, {} }
    },
    [PPP_LCP_STATE_CLOSING] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_CLOSING, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_INITIAL, {} },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_STOPPING, {} },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_CLOSING, {} },
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_CLOSING, { lcp_send_terminate_request } },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_CLOSED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_CLOSING, {} },
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_CLOSING, {} },
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_CLOSING, {} },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_CLOSING, {} },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_CLOSING, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_CLOSED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_CLOSING, { lcp_send_code_reject } },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_CLOSING, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_CLOSED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_CLOSING, {} }
    },
    [PPP_LCP_STATE_STOPPING] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_STOPPING, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_STOPPING, {} },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_CLOSING, {} },
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_STOPPING, { lcp_send_terminate_request } },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_STOPPED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_STOPPING, {} },
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_STOPPING, {} },
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_STOPPING, {} },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_STOPPING, {} },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_STOPPING, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_STOPPED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_STOPPING, { lcp_send_code_reject } },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_STOPPING, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_STOPPED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_STOPPING, {} }
    },
    [PPP_LCP_STATE_REQ_SENT] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_REQ_SENT, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_REQ_SENT, {} },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_CLOSING, { lcp_send_terminate_request } },
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_REQ_SENT, { lcp_send_configure_request } },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_STOPPED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_ACK_SENT, { lcp_send_configure_ack } },
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_REQ_SENT, { lcp_send_configure_nack } },
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_ACK_RCVD, { lcp_initialize_restart_count } },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_REQ_SENT, {  lcp_send_configure_request} },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_REQ_SENT, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_REQ_SENT, {} },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_REQ_SENT, { lcp_send_code_reject } },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_REQ_SENT, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_STOPPED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_REQ_SENT, {} }
    },
    [PPP_LCP_STATE_ACK_RCVD] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_ACK_RCVD, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_ACK_RCVD, {} },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_CLOSING,  {  lcp_send_terminate_request} },
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_REQ_SENT, { lcp_send_configure_request } },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_STOPPED,  { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_OPENED,   { lcp_send_configure_ack, lcp_this_layer_up} },
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_ACK_RCVD, { lcp_send_configure_nack } },
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_REQ_SENT, { lcp_send_configure_request } },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_REQ_SENT, { lcp_send_configure_request } },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_REQ_SENT, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_REQ_SENT, {} },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_ACK_RCVD, { lcp_send_code_reject } },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_REQ_SENT, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_STOPPED,  { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_ACK_RCVD, {} }
    },
    [PPP_LCP_STATE_ACK_SENT] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_ACK_SENT, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_STARTING, {} },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_ACK_SENT, {} },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_CLOSING,  { lcp_send_terminate_request} },
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_ACK_SENT, { lcp_send_configure_request } },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_STOPPED,  { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_ACK_SENT, { lcp_send_configure_ack } },
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_REQ_SENT, { lcp_send_configure_nack } },
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_OPENED,   { lcp_this_layer_up} },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_ACK_SENT, { lcp_send_configure_request} },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_REQ_SENT, { lcp_send_terminate_ack } },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_ACK_SENT, {} },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_ACK_SENT, { lcp_send_code_reject } },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_ACK_SENT, {} },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_STOPPED, { lcp_this_layer_finished } },
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_ACK_SENT, {} }
    },
    [PPP_LCP_STATE_OPENED] = {
        [PPP_LCP_EVENT_UP]      = { PPP_LCP_STATE_OPENED, {} },
        [PPP_LCP_EVENT_DOWN]    = { PPP_LCP_STATE_STARTING, {lcp_this_layer_down } },
        [PPP_LCP_EVENT_OPEN]    = { PPP_LCP_STATE_OPENED, {} },
        [PPP_LCP_EVENT_CLOSE]   = { PPP_LCP_STATE_CLOSING,
                                    { lcp_this_layer_down, lcp_send_terminate_request }},
        [PPP_LCP_EVENT_TO_POS]  = { PPP_LCP_STATE_OPENED, {} },
        [PPP_LCP_EVENT_TO_NEG]  = { PPP_LCP_STATE_OPENED, {} },
        [PPP_LCP_EVENT_RCR_POS] = { PPP_LCP_STATE_ACK_SENT,
                                    { lcp_this_layer_down, lcp_send_terminate_request, lcp_send_configure_ack }},
        [PPP_LCP_EVENT_RCR_NEG] = { PPP_LCP_STATE_REQ_SENT,
                                    { lcp_this_layer_down, lcp_send_configure_request, lcp_send_configure_nack }},
        [PPP_LCP_EVENT_RCA]     = { PPP_LCP_STATE_REQ_SENT, { lcp_this_layer_down, lcp_send_terminate_request } },
        [PPP_LCP_EVENT_RCN]     = { PPP_LCP_STATE_REQ_SENT, { lcp_this_layer_down, lcp_send_terminate_request } },
        [PPP_LCP_EVENT_RTR]     = { PPP_LCP_STATE_STOPPING, { lcp_this_layer_down, lcp_zero_restart_count, lcp_send_terminate_ack} },
        [PPP_LCP_EVENT_RTA]     = { PPP_LCP_STATE_REQ_SENT, { lcp_this_layer_down, lcp_send_terminate_request} },
        [PPP_LCP_EVENT_RUC]     = { PPP_LCP_STATE_OPENED,   { lcp_send_code_reject } },
        [PPP_LCP_EVENT_RXJ_POS] = { PPP_LCP_STATE_OPENED,   { } },
        [PPP_LCP_EVENT_RXJ_NEG] = { PPP_LCP_STATE_STOPPING,
                                    {lcp_this_layer_down, lcp_send_terminate_request}},
        [PPP_LCP_EVENT_RXR]     = { PPP_LCP_STATE_OPENED, { lcp_send_echo_reply} }
    }
};

static void evaluate_lcp_state(struct pico_device_ppp *ppp, enum ppp_lcp_event event)
{
    const struct pico_ppp_fsm *fsm, *next_fsm_to;
    int i;
    if (!ppp)
        return;

    if (mock_lcp_state) {
        mock_lcp_state(ppp, event);
        return;
    }

    fsm = &ppp_lcp_fsm[ppp->lcp_state][event];
    ppp->lcp_state = (enum ppp_lcp_state)fsm->next_state;
    /* RFC1661: The states in which the Restart timer is running are identifiable by
     * the presence of TO events.
     */
    next_fsm_to = &ppp_lcp_fsm[ppp->lcp_state][PPP_LCP_EVENT_TO_POS];
    if (!next_fsm_to->event_handler[0]) {
        /* The Restart timer is stopped when transitioning
         * from any state where the timer is running to a state where the timer
         * is not running.
         */
        lcp_timer_stop(ppp, PPP_TIMER_ON_LCPREQ);
        lcp_timer_stop(ppp, PPP_TIMER_ON_LCPTERM);
    }

    for (i = 0; i < PPP_FSM_MAX_ACTIONS; i++) {
        if (fsm->event_handler[i])
            fsm->event_handler[i](ppp);
    }
}

static void auth(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: Authenticated.\n");
    ppp->ipcp_allowed_fields = 0xFFFF;
    evaluate_ipcp_state(ppp, PPP_IPCP_EVENT_UP);
}

static void deauth(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: De-authenticated.\n");
    evaluate_ipcp_state(ppp, PPP_IPCP_EVENT_DOWN);
}

static void auth_abort(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: Authentication failed!\n");
    ppp->timer_on = (uint8_t) (ppp->timer_on & (~PPP_TIMER_ON_AUTH));
    evaluate_lcp_state(ppp, PPP_LCP_EVENT_CLOSE);

}

static void auth_req(struct pico_device_ppp *ppp)
{
    uint16_t ppp_usr_len = 0;
    uint16_t ppp_pwd_len = 0;
    uint8_t *req = NULL, *p;
    struct pico_pap_hdr *hdr;
    uint16_t pap_len = 0;
    uint8_t field_len = 0;
    ppp_usr_len = (uint16_t)strlen(ppp->username);
    ppp_pwd_len = (uint16_t)strlen(ppp->password);

    pap_len = (uint16_t)(sizeof(struct pico_pap_hdr) + 1u + 1u + ppp_usr_len + ppp_pwd_len);

    req = PICO_ZALLOC(PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + pap_len + PPP_FCS_SIZE + 1);
    if (!req)
        return;

    hdr = (struct pico_pap_hdr *) (req + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE);

    hdr->code = PAP_AUTH_REQ;
    hdr->id = ppp->frame_id++;
    hdr->len = short_be(pap_len);

    p = req + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_pap_hdr);

    /* Populate authentication domain */
    field_len = (uint8_t)(ppp_usr_len & 0xFF);
    *p = field_len;
    ++p;
    if (ppp_usr_len > 0) {
        memcpy(p, ppp->username, ppp_usr_len);
        p += ppp_usr_len;
    }

    /* Populate authentication password */
    field_len = (uint8_t)(ppp_pwd_len & 0xFF);
    *p = field_len;
    ++p;
    if (ppp_pwd_len > 0) {
        memcpy(p, ppp->password, ppp_pwd_len);
        p += ppp_pwd_len;
    }

    ppp_dbg("PAP: Sending authentication request.\n");
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_PAP,
                      req, /* Start of PPP packet */
                      (uint32_t)(
                          PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + /* PPP Header, etc. */
                          pap_len + /* Authentication packet len */
                          PPP_FCS_SIZE + /* FCS */
                          1)   /* STOP Byte */
                      );
    PICO_FREE(req);
}

static void auth_rsp(struct pico_device_ppp *ppp)
{
    struct pico_chap_hdr *ch = (struct pico_chap_hdr *)ppp->pkt;
    uint8_t resp[PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_chap_hdr) + CHAP_MD5_SIZE + PPP_FCS_SIZE + 2];
    struct pico_chap_hdr *rh = (struct pico_chap_hdr *) (resp + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE);
    uint8_t *md5resp = resp + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_chap_hdr) + 1;
    uint8_t *md5resp_len = resp + PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + sizeof(struct pico_chap_hdr);
    uint8_t *challenge;
    uint32_t i = 0, pwdlen;
    uint8_t *recvd_challenge_len = ppp->pkt + sizeof(struct pico_chap_hdr);
    uint8_t *recvd_challenge = recvd_challenge_len + 1;
    size_t challenge_size = CHALLENGE_SIZE(ppp, ch);

    challenge = PICO_ZALLOC(challenge_size);

    if (!challenge)
        return;


    pwdlen = (uint32_t)strlen(ppp->password);
    challenge[i++] = ch->id;
    memcpy(challenge + i, ppp->password, pwdlen);
    i += pwdlen;
    memcpy(challenge + i, recvd_challenge, *recvd_challenge_len);
    i += *recvd_challenge_len;
    pico_md5sum(md5resp, challenge, i);
    PICO_FREE(challenge);
    rh->id = ch->id;
    rh->code = CHAP_RESPONSE;
    rh->len = short_be(CHAP_MD5_SIZE + sizeof(struct pico_chap_hdr) + 1);
    *md5resp_len = CHAP_MD5_SIZE;
    ppp_dbg("Sending CHAP RESPONSE, \n");
    pico_ppp_ctl_send(&ppp->dev, PPP_PROTO_CHAP,
                      resp,           /* Start of PPP packet */
                      (uint32_t)(
                          PPP_HDR_SIZE + PPP_PROTO_SLOT_SIZE + /* PPP Header, etc. */
                          sizeof(struct pico_chap_hdr) + /* CHAP HDR */
                          1                            + /* Value length */
                          CHAP_MD5_SIZE + /* Actual payload size */
                          PPP_FCS_SIZE + /* FCS at the end of the frame */
                          1)             /* STOP Byte */
                      );
}

static void auth_start_timer(struct pico_device_ppp *ppp)
{
    ppp->timer_on = ppp->timer_on | PPP_TIMER_ON_AUTH;
    ppp->timer_val = PICO_PPP_DEFAULT_TIMER;
}

static const struct pico_ppp_fsm ppp_auth_fsm[PPP_AUTH_STATE_MAX][PPP_AUTH_EVENT_MAX] = {
    [PPP_AUTH_STATE_INITIAL] = {
        [PPP_AUTH_EVENT_UP_NONE] = { PPP_AUTH_STATE_AUTHENTICATED, {auth} },
        [PPP_AUTH_EVENT_UP_PAP]  = { PPP_AUTH_STATE_REQ_SENT, {auth_req, auth_start_timer} },
        [PPP_AUTH_EVENT_UP_CHAP] = { PPP_AUTH_STATE_STARTING, {} },
        [PPP_AUTH_EVENT_DOWN]    = { PPP_AUTH_STATE_INITIAL, {} },
        [PPP_AUTH_EVENT_RAC]     = { PPP_AUTH_STATE_INITIAL, {} },
        [PPP_AUTH_EVENT_RAA]     = { PPP_AUTH_STATE_INITIAL, {} },
        [PPP_AUTH_EVENT_RAN]     = { PPP_AUTH_STATE_INITIAL, {auth_abort} },
        [PPP_AUTH_EVENT_TO]     =  { PPP_AUTH_STATE_INITIAL, {} }
    },
    [PPP_AUTH_STATE_STARTING] = {
        [PPP_AUTH_EVENT_UP_NONE] = { PPP_AUTH_STATE_STARTING, {} },
        [PPP_AUTH_EVENT_UP_PAP]  = { PPP_AUTH_STATE_STARTING, {} },
        [PPP_AUTH_EVENT_UP_CHAP] = { PPP_AUTH_STATE_STARTING, {} },
        [PPP_AUTH_EVENT_DOWN]    = { PPP_AUTH_STATE_INITIAL, {deauth} },
        [PPP_AUTH_EVENT_RAC]     = { PPP_AUTH_STATE_RSP_SENT, {auth_rsp, auth_start_timer} },
        [PPP_AUTH_EVENT_RAA]     = { PPP_AUTH_STATE_STARTING, {auth_start_timer} },
        [PPP_AUTH_EVENT_RAN]     = { PPP_AUTH_STATE_STARTING, {auth_abort} },
        [PPP_AUTH_EVENT_TO]     =  { PPP_AUTH_STATE_INITIAL, {auth_req, auth_start_timer} }
    },
    [PPP_AUTH_STATE_RSP_SENT] = {
        [PPP_AUTH_EVENT_UP_NONE] = { PPP_AUTH_STATE_RSP_SENT, {} },
        [PPP_AUTH_EVENT_UP_PAP]  = { PPP_AUTH_STATE_RSP_SENT, {} },
        [PPP_AUTH_EVENT_UP_CHAP] = { PPP_AUTH_STATE_RSP_SENT, {} },
        [PPP_AUTH_EVENT_DOWN]    = { PPP_AUTH_STATE_INITIAL, {deauth} },
        [PPP_AUTH_EVENT_RAC]     = { PPP_AUTH_STATE_RSP_SENT, {auth_rsp, auth_start_timer} },
        [PPP_AUTH_EVENT_RAA]     = { PPP_AUTH_STATE_AUTHENTICATED, {auth} },
        [PPP_AUTH_EVENT_RAN]     = { PPP_AUTH_STATE_STARTING, {auth_abort} },
        [PPP_AUTH_EVENT_TO]     =  { PPP_AUTH_STATE_STARTING, {auth_start_timer} }
    },
    [PPP_AUTH_STATE_REQ_SENT] = {
        [PPP_AUTH_EVENT_UP_NONE] = { PPP_AUTH_STATE_REQ_SENT, {} },
        [PPP_AUTH_EVENT_UP_PAP]  = { PPP_AUTH_STATE_REQ_SENT, {} },
        [PPP_AUTH_EVENT_UP_CHAP] = { PPP_AUTH_STATE_REQ_SENT, {} },
        [PPP_AUTH_EVENT_DOWN]    = { PPP_AUTH_STATE_INITIAL, {deauth} },
        [PPP_AUTH_EVENT_RAC]     = { PPP_AUTH_STATE_REQ_SENT, {} },
        [PPP_AUTH_EVENT_RAA]     = { PPP_AUTH_STATE_AUTHENTICATED, {auth} },
        [PPP_AUTH_EVENT_RAN]     = { PPP_AUTH_STATE_REQ_SENT, {auth_abort} },
        [PPP_AUTH_EVENT_TO]     =  { PPP_AUTH_STATE_REQ_SENT, {auth_req, auth_start_timer} }
    },
    [PPP_AUTH_STATE_AUTHENTICATED] = {
        [PPP_AUTH_EVENT_UP_NONE] = { PPP_AUTH_STATE_AUTHENTICATED, {} },
        [PPP_AUTH_EVENT_UP_PAP]  = { PPP_AUTH_STATE_AUTHENTICATED, {} },
        [PPP_AUTH_EVENT_UP_CHAP] = { PPP_AUTH_STATE_AUTHENTICATED, {} },
        [PPP_AUTH_EVENT_DOWN]    = { PPP_AUTH_STATE_INITIAL, {deauth} },
        [PPP_AUTH_EVENT_RAC]     = { PPP_AUTH_STATE_RSP_SENT, {auth_rsp} },
        [PPP_AUTH_EVENT_RAA]     = { PPP_AUTH_STATE_AUTHENTICATED, {} },
        [PPP_AUTH_EVENT_RAN]     = { PPP_AUTH_STATE_AUTHENTICATED, {} },
        [PPP_AUTH_EVENT_TO]      = { PPP_AUTH_STATE_AUTHENTICATED, {} },
    }
};

static void evaluate_auth_state(struct pico_device_ppp *ppp, enum ppp_auth_event event)
{
    const struct pico_ppp_fsm *fsm;
    int i;
    if (mock_auth_state) {
        mock_auth_state(ppp, event);
        return;
    }

    fsm = &ppp_auth_fsm[ppp->auth_state][event];

    ppp->auth_state = (enum ppp_auth_state)fsm->next_state;
    for (i = 0; i < PPP_FSM_MAX_ACTIONS; i++) {
        if (fsm->event_handler[i])
            fsm->event_handler[i](ppp);
    }
}

static void ipcp_send_nack(struct pico_device_ppp *ppp)
{
    IGNORE_PARAMETER(ppp);
}

static void ipcp_bring_up(struct pico_device_ppp *ppp)
{
    ppp_dbg("PPP: IPCP up.\n");

    if (ppp->ipcp_ip) {
        char my_ip[16], my_dns[16];
        pico_ipv4_to_string(my_ip, ppp->ipcp_ip);
        ppp_dbg("Received IP config %s\n", my_ip);
        pico_ipv4_to_string(my_dns, ppp->ipcp_dns1);
        ppp_dbg("Received DNS: %s\n", my_dns);
        ppp_ipv4_conf(ppp);
    }
}

static void ipcp_bring_down(struct pico_device_ppp *ppp)
{
    IGNORE_PARAMETER(ppp);

    ppp_dbg("PPP: IPCP down.\n");
}

static void ipcp_start_timer(struct pico_device_ppp *ppp)
{
    ppp->timer_on = ppp->timer_on | PPP_TIMER_ON_IPCP;
    ppp->timer_val = PICO_PPP_DEFAULT_TIMER * PICO_PPP_DEFAULT_MAX_FAILURE;
}

static const struct pico_ppp_fsm ppp_ipcp_fsm[PPP_IPCP_STATE_MAX][PPP_IPCP_EVENT_MAX] = {
    [PPP_IPCP_STATE_INITIAL] = {
        [PPP_IPCP_EVENT_UP]      = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_req, ipcp_start_timer} },
        [PPP_IPCP_EVENT_DOWN]    = { PPP_IPCP_STATE_INITIAL, {} },
        [PPP_IPCP_EVENT_RCR_POS] = { PPP_IPCP_STATE_INITIAL, {} },
        [PPP_IPCP_EVENT_RCR_NEG] = { PPP_IPCP_STATE_INITIAL, {} },
        [PPP_IPCP_EVENT_RCA]     = { PPP_IPCP_STATE_INITIAL, {} },
        [PPP_IPCP_EVENT_RCN]     = { PPP_IPCP_STATE_INITIAL, {} },
        [PPP_IPCP_EVENT_TO]      = { PPP_IPCP_STATE_INITIAL, {} }
    },
    [PPP_IPCP_STATE_REQ_SENT] = {
        [PPP_IPCP_EVENT_UP]      = { PPP_IPCP_STATE_REQ_SENT, {} },
        [PPP_IPCP_EVENT_DOWN]    = { PPP_IPCP_STATE_INITIAL, {} },
        [PPP_IPCP_EVENT_RCR_POS] = { PPP_IPCP_STATE_ACK_SENT, {ipcp_send_ack} },
        [PPP_IPCP_EVENT_RCR_NEG] = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_nack} },
        [PPP_IPCP_EVENT_RCA]     = { PPP_IPCP_STATE_ACK_RCVD, {} },
        [PPP_IPCP_EVENT_RCN]     = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_req, ipcp_start_timer} },
        [PPP_IPCP_EVENT_TO]      = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_req, ipcp_start_timer} }
    },
    [PPP_IPCP_STATE_ACK_RCVD] = {
        [PPP_IPCP_EVENT_UP]      = { PPP_IPCP_STATE_ACK_RCVD, {} },
        [PPP_IPCP_EVENT_DOWN]    = { PPP_IPCP_STATE_INITIAL, {} },
        [PPP_IPCP_EVENT_RCR_POS] = { PPP_IPCP_STATE_OPENED, {ipcp_send_ack, ipcp_bring_up} },
        [PPP_IPCP_EVENT_RCR_NEG] = { PPP_IPCP_STATE_ACK_RCVD, {ipcp_send_nack} },
        [PPP_IPCP_EVENT_RCA]     = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_req, ipcp_start_timer} },
        [PPP_IPCP_EVENT_RCN]     = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_req, ipcp_start_timer} },
        [PPP_IPCP_EVENT_TO]      = { PPP_IPCP_STATE_ACK_RCVD, {ipcp_send_req, ipcp_start_timer} }
    },
    [PPP_IPCP_STATE_ACK_SENT] = {
        [PPP_IPCP_EVENT_UP]      = { PPP_IPCP_STATE_ACK_SENT, {} },
        [PPP_IPCP_EVENT_DOWN]    = { PPP_IPCP_STATE_INITIAL, {} },
        [PPP_IPCP_EVENT_RCR_POS] = { PPP_IPCP_STATE_ACK_SENT, {ipcp_send_ack} },
        [PPP_IPCP_EVENT_RCR_NEG] = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_nack} },
        [PPP_IPCP_EVENT_RCA]     = { PPP_IPCP_STATE_OPENED, {ipcp_bring_up} },
        [PPP_IPCP_EVENT_RCN]     = { PPP_IPCP_STATE_ACK_SENT, {ipcp_send_req, ipcp_start_timer} },
        [PPP_IPCP_EVENT_TO]      = { PPP_IPCP_STATE_ACK_SENT, {ipcp_send_req, ipcp_start_timer} }
    },
    [PPP_IPCP_STATE_OPENED] = {
        [PPP_IPCP_EVENT_UP]      = { PPP_IPCP_STATE_OPENED, {} },
        [PPP_IPCP_EVENT_DOWN]    = { PPP_IPCP_STATE_INITIAL, {ipcp_bring_down} },
        [PPP_IPCP_EVENT_RCR_POS] = { PPP_IPCP_STATE_ACK_SENT, {ipcp_bring_down, ipcp_send_req, ipcp_send_ack} },
        [PPP_IPCP_EVENT_RCR_NEG] = { PPP_IPCP_STATE_REQ_SENT, {ipcp_bring_down, ipcp_send_req, ipcp_send_nack} },
        [PPP_IPCP_EVENT_RCA]     = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_req} },
        [PPP_IPCP_EVENT_RCN]     = { PPP_IPCP_STATE_REQ_SENT, {ipcp_send_req} },
        [PPP_IPCP_EVENT_TO]      = { PPP_IPCP_STATE_OPENED, {} }
    }
};

static void evaluate_ipcp_state(struct pico_device_ppp *ppp, enum ppp_ipcp_event event)
{
    const struct pico_ppp_fsm *fsm;
    int i;
    if (mock_ipcp_state) {
        mock_ipcp_state(ppp, event);
        return;
    }

    fsm = &ppp_ipcp_fsm[ppp->ipcp_state][event];

    ppp->ipcp_state = (enum ppp_ipcp_state)fsm->next_state;
    for (i = 0; i < PPP_FSM_MAX_ACTIONS; i++) {
        if (fsm->event_handler[i])
            fsm->event_handler[i](ppp);
    }
}

static int pico_ppp_poll(struct pico_device *dev, int loop_score)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *) dev;
    static uint32_t len = 0;
    int r;
    if (ppp->serial_recv) {
        do {
            r = ppp->serial_recv(&ppp->dev, &ppp_recv_buf[len], 1);
            if (r <= 0)
                break;

            if (ppp->modem_state == PPP_MODEM_STATE_CONNECTED) {
                static int control_escape = 0;

                if (ppp_recv_buf[len] == PPPF_FLAG_SEQ) {
                    if (control_escape) {
                        /* Illegal sequence, discard frame */
                        ppp_dbg("Illegal sequence, ppp_recv_buf[%d] = %d\n", len, ppp_recv_buf[len]);
                        control_escape = 0;
                        len = 0;
                    }

                    if (len > 1) {
                        ppp_recv_data(ppp, ppp_recv_buf, len);
                        loop_score--;
                        len = 0;
                    }
                } else if (control_escape) {
                    ppp_recv_buf[len] ^= 0x20;
                    control_escape = 0;
                    len++;
                } else if (ppp_recv_buf[len] == PPPF_CTRL_ESC) {
                    control_escape = 1;
                } else {
                    len++;
                }
            } else {
                static int s3 = 0;

                if (ppp_recv_buf[len] == AT_S3) {
                    s3 = 1;
                    if (len > 0) {
                        ppp_recv_buf[len] = '\0';
                        ppp_modem_recv(ppp, ppp_recv_buf, len);
                        len = 0;
                    }
                } else if (ppp_recv_buf[len] == AT_S4) {
                    if (!s3) {
                        len++;
                    }

                    s3 = 0;
                } else {
                    s3 = 0;
                    len++;
                }
            }
        } while ((r > 0) && (len < ARRAY_SIZE(ppp_recv_buf)) && (loop_score > 0));
    }

    return loop_score;
}

/* Public interface: create/destroy. */

static int pico_ppp_link_state(struct pico_device *dev)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;
    if (ppp->ipcp_state == PPP_IPCP_STATE_OPENED)
        return 1;

    return 0;
}

void pico_ppp_destroy(struct pico_device *ppp)
{
    if (!ppp)
        return;

    /* Perform custom cleanup here before calling 'pico_device_destroy'
     * or register a custom cleanup function during initialization
     * by setting 'ppp->dev.destroy'. */

    pico_device_destroy(ppp);
}

static void check_to_modem(struct pico_device_ppp *ppp)
{
    if (ppp->timer_on & PPP_TIMER_ON_MODEM) {
        if (ppp->timer_val == 0) {
            ppp->timer_on = (uint8_t) (ppp->timer_on & (~PPP_TIMER_ON_MODEM));
            evaluate_modem_state(ppp, PPP_MODEM_EVENT_TIMEOUT);
        }
    }
}

static void check_to_lcp(struct pico_device_ppp *ppp)
{
    if (ppp->timer_on & (PPP_TIMER_ON_LCPREQ | PPP_TIMER_ON_LCPTERM)) {
        if (ppp->timer_val == 0) {
            if (ppp->timer_count == 0)
                evaluate_lcp_state(ppp, PPP_LCP_EVENT_TO_NEG);
            else{
                evaluate_lcp_state(ppp, PPP_LCP_EVENT_TO_POS);
                ppp->timer_count--;
            }
        }
    }
}

static void check_to_auth(struct pico_device_ppp *ppp)
{
    if (ppp->timer_on & PPP_TIMER_ON_AUTH) {
        if (ppp->timer_val == 0) {
            ppp->timer_on = (uint8_t) (ppp->timer_on & (~PPP_TIMER_ON_AUTH));
            evaluate_auth_state(ppp, PPP_AUTH_EVENT_TO);
        }
    }
}

static void check_to_ipcp(struct pico_device_ppp *ppp)
{
    if (ppp->timer_on & PPP_TIMER_ON_IPCP) {
        if (ppp->timer_val == 0) {
            ppp->timer_on = (uint8_t) (ppp->timer_on & (~PPP_TIMER_ON_IPCP));
            evaluate_ipcp_state(ppp, PPP_IPCP_EVENT_TO);
        }
    }
}

static void pico_ppp_tick(pico_time t, void *arg)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *) arg;
    (void)t;
    if (ppp->timer_val > 0)
        ppp->timer_val--;

    check_to_modem(ppp);
    check_to_lcp(ppp);
    check_to_auth(ppp);
    check_to_ipcp(ppp);

    if (ppp->autoreconnect && ppp->lcp_state == PPP_LCP_STATE_INITIAL) {
        ppp_dbg("(Re)connecting...\n");
        evaluate_lcp_state(ppp, PPP_LCP_EVENT_OPEN);
    }

    if (!pico_timer_add(1000, pico_ppp_tick, arg)) {
        ppp_dbg("PPP: Failed to start tick timer\n");
        /* TODO No more PPP ticks now */
    }
}

struct pico_device *pico_ppp_create(void)
{
    struct pico_device_ppp *ppp = PICO_ZALLOC(sizeof(struct pico_device_ppp));
    char devname[MAX_DEVICE_NAME];

    if (!ppp)
        return NULL;

    snprintf(devname, MAX_DEVICE_NAME, "ppp%d", ppp_devnum++);

    if( 0 != pico_device_init((struct pico_device *)ppp, devname, NULL)) {
        return NULL;
    }

    ppp->dev.overhead = PPP_HDR_SIZE;
    ppp->dev.mtu = PICO_PPP_MTU;
    ppp->dev.send = pico_ppp_send;
    ppp->dev.poll = pico_ppp_poll;
    ppp->dev.link_state  = pico_ppp_link_state;
    ppp->frame_id = (uint8_t)(pico_rand() % 0xFF);

    ppp->modem_state = PPP_MODEM_STATE_INITIAL;
    ppp->lcp_state = PPP_LCP_STATE_INITIAL;
    ppp->auth_state = PPP_AUTH_STATE_INITIAL;
    ppp->ipcp_state = PPP_IPCP_STATE_INITIAL;

    ppp->timer = pico_timer_add(1000, pico_ppp_tick, ppp);
    if (!ppp->timer) {
        ppp_dbg("PPP: Failed to start tick timer\n");
        pico_device_destroy((struct pico_device*) ppp);
        return NULL;
    }
    ppp->mru = PICO_PPP_MRU;

    LCPOPT_SET_LOCAL(ppp, LCPOPT_MRU);
    LCPOPT_SET_LOCAL(ppp, LCPOPT_AUTH); /* We support authentication, even if it's not part of the req */
    LCPOPT_SET_LOCAL(ppp, LCPOPT_PROTO_COMP);
    LCPOPT_SET_LOCAL(ppp, LCPOPT_ADDRCTL_COMP);


    ppp_dbg("Device %s created.\n", ppp->dev.name);
    return (struct pico_device *)ppp;
}

int pico_ppp_connect(struct pico_device *dev)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;
    ppp->autoreconnect = 1;
    return 0;
}

int pico_ppp_disconnect(struct pico_device *dev)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;
    ppp->autoreconnect = 0;
    evaluate_lcp_state(ppp, PPP_LCP_EVENT_CLOSE);

    pico_ipv4_cleanup_links(dev);

    return 0;
}

int pico_ppp_set_serial_read(struct pico_device *dev, int (*sread)(struct pico_device *, void *, int))
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;

    if (!dev)
        return -1;

    ppp->serial_recv = sread;
    return 0;
}

int pico_ppp_set_serial_write(struct pico_device *dev, int (*swrite)(struct pico_device *, const void *, int))
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;

    if (!dev)
        return -1;

    ppp->serial_send = swrite;
    return 0;
}

int pico_ppp_set_serial_set_speed(struct pico_device *dev, int (*sspeed)(struct pico_device *, uint32_t))
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;

    if (!dev)
        return -1;

    ppp->serial_set_speed = sspeed;
    return 0;
}

int pico_ppp_set_apn(struct pico_device *dev, const char *apn)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;

    if (!dev)
        return -1;

    if (!apn)
        return -1;

    strncpy(ppp->apn, apn, sizeof(ppp->apn) - 1);
    return 0;
}

int pico_ppp_set_username(struct pico_device *dev, const char *username)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;

    if (!dev)
        return -1;

    if (!username)
        return -1;

    strncpy(ppp->username, username, sizeof(ppp->username) - 1);
    return 0;
}

int pico_ppp_set_password(struct pico_device *dev, const char *password)
{
    struct pico_device_ppp *ppp = (struct pico_device_ppp *)dev;

    if (!dev)
        return -1;

    if (!password)
        return -1;

    strncpy(ppp->password, password, sizeof(ppp->password) - 1);
    return 0;
}
