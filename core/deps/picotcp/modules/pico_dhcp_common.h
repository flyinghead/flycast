/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

 *********************************************************************/
#ifndef INCLUDE_PICO_DHCP_COMMON
#define INCLUDE_PICO_DHCP_COMMON
#include "pico_config.h"
#include "pico_addressing.h"

#define PICO_DHCPD_PORT (short_be(67))
#define PICO_DHCP_CLIENT_PORT (short_be(68))
#define PICO_DHCPD_MAGIC_COOKIE (long_be(0x63825363))
#define PICO_DHCP_HTYPE_ETH 1

/* Macro to get DHCP option field */
#define DHCP_OPT(hdr, off)              ((struct pico_dhcp_opt *)(((uint8_t *)hdr) + sizeof(struct pico_dhcp_hdr) + off))

/* flags */
#define PICO_DHCP_FLAG_BROADCAST        0x8000

/* options */
#define PICO_DHCP_OPT_PAD               0x00
#define PICO_DHCP_OPT_NETMASK           0x01
#define PICO_DHCP_OPT_TIME              0x02
#define PICO_DHCP_OPT_ROUTER            0x03
#define PICO_DHCP_OPT_DNS               0x06
#define PICO_DHCP_OPT_HOSTNAME          0x0c
#define PICO_DHCP_OPT_DOMAINNAME        0x0f
#define PICO_DHCP_OPT_MTU               0x1a
#define PICO_DHCP_OPT_BROADCAST         0x1c
#define PICO_DHCP_OPT_NETBIOSNS         0x2c
#define PICO_DHCP_OPT_NETBIOSSCOPE      0x2f
#define PICO_DHCP_OPT_REQIP             0x32
#define PICO_DHCP_OPT_LEASETIME         0x33
#define PICO_DHCP_OPT_OPTOVERLOAD       0x34
#define PICO_DHCP_OPT_MSGTYPE           0x35
#define PICO_DHCP_OPT_SERVERID          0x36
#define PICO_DHCP_OPT_PARAMLIST         0x37
#define PICO_DHCP_OPT_MESSAGE           0x38
#define PICO_DHCP_OPT_MAXMSGSIZE        0x39
#define PICO_DHCP_OPT_RENEWALTIME       0x3a
#define PICO_DHCP_OPT_REBINDINGTIME     0x3b
#define PICO_DHCP_OPT_VENDORID          0x3c
#define PICO_DHCP_OPT_CLIENTID          0x3d
#define PICO_DHCP_OPT_DOMAINSEARCH      0x77
#define PICO_DHCP_OPT_STATICROUTE       0x79
#define PICO_DHCP_OPT_END               0xFF

/* options len */
#define PICO_DHCP_OPTLEN_HDR            2 /* account for code and len field */
#define PICO_DHCP_OPTLEN_NETMASK        6
#define PICO_DHCP_OPTLEN_ROUTER         6
#define PICO_DHCP_OPTLEN_DNS            6
#define PICO_DHCP_OPTLEN_BROADCAST      6
#define PICO_DHCP_OPTLEN_REQIP          6
#define PICO_DHCP_OPTLEN_LEASETIME      6
#define PICO_DHCP_OPTLEN_OPTOVERLOAD    3
#define PICO_DHCP_OPTLEN_MSGTYPE        3
#define PICO_DHCP_OPTLEN_SERVERID       6
#define PICO_DHCP_OPTLEN_PARAMLIST      9 /* PicoTCP specific */
#define PICO_DHCP_OPTLEN_MAXMSGSIZE     4
#define PICO_DHCP_OPTLEN_RENEWALTIME    6
#define PICO_DHCP_OPTLEN_REBINDINGTIME  6
#define PICO_DHCP_OPTLEN_END            1

/* op codes */
#define PICO_DHCP_OP_REQUEST            1
#define PICO_DHCP_OP_REPLY              2

/* rfc message types */
#define PICO_DHCP_MSG_DISCOVER          1
#define PICO_DHCP_MSG_OFFER             2
#define PICO_DHCP_MSG_REQUEST           3
#define PICO_DHCP_MSG_DECLINE           4
#define PICO_DHCP_MSG_ACK               5
#define PICO_DHCP_MSG_NAK               6
#define PICO_DHCP_MSG_RELEASE           7
#define PICO_DHCP_MSG_INFORM            8

/* custom message types */
#define PICO_DHCP_EVENT_T1              9
#define PICO_DHCP_EVENT_T2              10
#define PICO_DHCP_EVENT_LEASE           11
#define PICO_DHCP_EVENT_RETRANSMIT      12
#define PICO_DHCP_EVENT_NONE            0xff

PACKED_STRUCT_DEF pico_dhcp_hdr
{
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops; /* zero */
    uint32_t xid; /* store this in the request */
    uint16_t secs; /* ignore */
    uint16_t flags;
    uint32_t ciaddr; /* client address - if asking for renewal */
    uint32_t yiaddr; /* your address (client) */
    uint32_t siaddr; /* dhcp offered address */
    uint32_t giaddr; /* relay agent, bootp. */
    uint8_t hwaddr[6];
    uint8_t hwaddr_padding[10];
    char hostname[64];
    char bootp_filename[128];
    uint32_t dhcp_magic;
};

PACKED_STRUCT_DEF pico_dhcp_opt
{
    uint8_t code;
    uint8_t len;
    PACKED_UNION_DEF dhcp_opt_ext_u {
        PEDANTIC_STRUCT_DEF netmask_s {
            struct pico_ip4 ip;
        } netmask;
        PEDANTIC_STRUCT_DEF router_s {
            struct pico_ip4 ip;
        } router;
        PEDANTIC_STRUCT_DEF dns_s {
            struct pico_ip4 ip;
        } dns1;
        struct dns_s dns2;
        PEDANTIC_STRUCT_DEF broadcast_s {
            struct pico_ip4 ip;
        } broadcast;
        PEDANTIC_STRUCT_DEF req_ip_s {
            struct pico_ip4 ip;
        } req_ip;
        PEDANTIC_STRUCT_DEF lease_time_s {
            uint32_t time;
        } lease_time;
        PEDANTIC_STRUCT_DEF opt_overload_s {
            uint8_t value;
        } opt_overload;
        PEDANTIC_STRUCT_DEF tftp_server_s {
            char name[1];
        } tftp_server;
        PEDANTIC_STRUCT_DEF bootfile_s {
            char name[1];
        } bootfile;
        PEDANTIC_STRUCT_DEF msg_type_s {
            uint8_t type;
        } msg_type;
        PEDANTIC_STRUCT_DEF server_id_s {
            struct pico_ip4 ip;
        } server_id;
        PEDANTIC_STRUCT_DEF param_list_s {
            uint8_t code[1];
        } param_list;
        PEDANTIC_STRUCT_DEF message_s {
            char error[1];
        } message;
        PEDANTIC_STRUCT_DEF max_msg_size_s {
            uint16_t size;
        } max_msg_size;
        PEDANTIC_STRUCT_DEF renewal_time_s {
            uint32_t time;
        } renewal_time;
        PEDANTIC_STRUCT_DEF rebinding_time_s {
            uint32_t time;
        } rebinding_time;
        PEDANTIC_STRUCT_DEF vendor_id_s {
            uint8_t id[1];
        } vendor_id;
        PEDANTIC_STRUCT_DEF client_id_s {
            uint8_t id[1];
        } client_id;
        PEDANTIC_STRUCT_DEF text_s {
            char txt[1];
        } string;
    } ext;
};

uint8_t dhcp_get_next_option(uint8_t *begin, uint8_t *data, int *len, uint8_t **nextopt);
struct pico_dhcp_opt *pico_dhcp_next_option(struct pico_dhcp_opt **ptr);
uint8_t pico_dhcp_are_options_valid(void *ptr, int32_t len);

uint8_t pico_dhcp_opt_netmask(void *ptr, struct pico_ip4 *ip);
uint8_t pico_dhcp_opt_router(void *ptr, struct pico_ip4 *ip);
uint8_t pico_dhcp_opt_dns(void *ptr, struct pico_ip4 *ip);
uint8_t pico_dhcp_opt_broadcast(void *ptr, struct pico_ip4 *ip);
uint8_t pico_dhcp_opt_reqip(void *ptr, struct pico_ip4 *ip);
uint8_t pico_dhcp_opt_leasetime(void *ptr, uint32_t time);
uint8_t pico_dhcp_opt_msgtype(void *ptr, uint8_t type);
uint8_t pico_dhcp_opt_serverid(void *ptr, struct pico_ip4 *ip);
uint8_t pico_dhcp_opt_paramlist(void *ptr);
uint8_t pico_dhcp_opt_maxmsgsize(void *ptr, uint16_t size);
uint8_t pico_dhcp_opt_end(void *ptr);
#endif
