/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Authors: Frederik Van Slycken
 *********************************************************************/

#include "pico_config.h"
#include "pico_stack.h"
#include "pico_dhcp_common.h"

#if defined (PICO_SUPPORT_DHCPC) || defined (PICO_SUPPORT_DHCPD)
/* pico_dhcp_are_options_valid needs to be called first to prevent illegal memory access */
/* The argument pointer is moved forward to the next option */
struct pico_dhcp_opt *pico_dhcp_next_option(struct pico_dhcp_opt **ptr)
{
    uint8_t **p = (uint8_t **)ptr;
    struct pico_dhcp_opt *opt = *ptr;

    if (opt->code == PICO_DHCP_OPT_END)
        return NULL;

    if (opt->code == PICO_DHCP_OPT_PAD) {
        *p += 1;
        return *ptr;
    }

    *p += (opt->len + 2); /* (len + 2) to account for code and len octet */
    return *ptr;
}

uint8_t pico_dhcp_are_options_valid(void *ptr, int32_t len)
{
    uint8_t optlen = 0, *p = ptr;

    while (len > 0) {
        switch (*p)
        {
        case PICO_DHCP_OPT_END:
            return 1;

        case PICO_DHCP_OPT_PAD:
            p++;
            len--;
            break;

        default:
            p++; /* move pointer from code octet to len octet */
            len--;
            if ((len <= 0) || (len - (*p + 1) < 0)) /* (*p + 1) to account for len octet */
                return 0;

            optlen = *p;
            p += optlen + 1;
            len -= optlen;
            break;
        }
    }
    return 0;
}

uint8_t pico_dhcp_opt_netmask(void *ptr, struct pico_ip4 *ip)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: netmask */
    opt->code = PICO_DHCP_OPT_NETMASK;
    opt->len = PICO_DHCP_OPTLEN_NETMASK - PICO_DHCP_OPTLEN_HDR;
    opt->ext.netmask.ip = *ip;
    return PICO_DHCP_OPTLEN_NETMASK;
}

uint8_t pico_dhcp_opt_router(void *ptr, struct pico_ip4 *ip)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: router */
    opt->code = PICO_DHCP_OPT_ROUTER;
    opt->len = PICO_DHCP_OPTLEN_ROUTER - PICO_DHCP_OPTLEN_HDR;
    opt->ext.router.ip = *ip;
    return PICO_DHCP_OPTLEN_ROUTER;
}

uint8_t pico_dhcp_opt_dns(void *ptr, struct pico_ip4 *ip)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: dns */
    opt->code = PICO_DHCP_OPT_DNS;
    opt->len = PICO_DHCP_OPTLEN_DNS - PICO_DHCP_OPTLEN_HDR;
    opt->ext.dns1.ip = *ip;
    return PICO_DHCP_OPTLEN_DNS;
}

uint8_t pico_dhcp_opt_broadcast(void *ptr, struct pico_ip4 *ip)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: broadcast */
    opt->code = PICO_DHCP_OPT_BROADCAST;
    opt->len = PICO_DHCP_OPTLEN_BROADCAST - PICO_DHCP_OPTLEN_HDR;
    opt->ext.broadcast.ip = *ip;
    return PICO_DHCP_OPTLEN_BROADCAST;
}

uint8_t pico_dhcp_opt_reqip(void *ptr, struct pico_ip4 *ip)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: request IP address */
    opt->code = PICO_DHCP_OPT_REQIP;
    opt->len = PICO_DHCP_OPTLEN_REQIP - PICO_DHCP_OPTLEN_HDR;
    opt->ext.req_ip.ip = *ip;
    return PICO_DHCP_OPTLEN_REQIP;
}

uint8_t pico_dhcp_opt_leasetime(void *ptr, uint32_t time)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: lease time */
    opt->code = PICO_DHCP_OPT_LEASETIME;
    opt->len = PICO_DHCP_OPTLEN_LEASETIME - PICO_DHCP_OPTLEN_HDR;
    opt->ext.lease_time.time = time;
    return PICO_DHCP_OPTLEN_LEASETIME;
}

uint8_t pico_dhcp_opt_msgtype(void *ptr, uint8_t type)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: message type */
    opt->code = PICO_DHCP_OPT_MSGTYPE;
    opt->len = PICO_DHCP_OPTLEN_MSGTYPE - PICO_DHCP_OPTLEN_HDR;
    opt->ext.msg_type.type = type;
    return PICO_DHCP_OPTLEN_MSGTYPE;
}

uint8_t pico_dhcp_opt_serverid(void *ptr, struct pico_ip4 *ip)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: server identifier */
    opt->code = PICO_DHCP_OPT_SERVERID;
    opt->len = PICO_DHCP_OPTLEN_SERVERID - PICO_DHCP_OPTLEN_HDR;
    opt->ext.server_id.ip = *ip;
    return PICO_DHCP_OPTLEN_SERVERID;
}

uint8_t pico_dhcp_opt_paramlist(void *ptr)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;
    uint8_t *param_code = &(opt->ext.param_list.code[0]);

    /* option: parameter list */
    opt->code = PICO_DHCP_OPT_PARAMLIST;
    opt->len = PICO_DHCP_OPTLEN_PARAMLIST - PICO_DHCP_OPTLEN_HDR;
    param_code[0] = PICO_DHCP_OPT_NETMASK;
    param_code[1] = PICO_DHCP_OPT_TIME;
    param_code[2] = PICO_DHCP_OPT_ROUTER;
    param_code[3] = PICO_DHCP_OPT_HOSTNAME;
    param_code[4] = PICO_DHCP_OPT_RENEWALTIME;
    param_code[5] = PICO_DHCP_OPT_REBINDINGTIME;
    param_code[6] = PICO_DHCP_OPT_DNS;
    return PICO_DHCP_OPTLEN_PARAMLIST;
}

uint8_t pico_dhcp_opt_maxmsgsize(void *ptr, uint16_t size)
{
    struct pico_dhcp_opt *opt = (struct pico_dhcp_opt *)ptr;

    /* option: maximum message size */
    opt->code = PICO_DHCP_OPT_MAXMSGSIZE;
    opt->len = PICO_DHCP_OPTLEN_MAXMSGSIZE - PICO_DHCP_OPTLEN_HDR;
    opt->ext.max_msg_size.size = short_be(size);
    return PICO_DHCP_OPTLEN_MAXMSGSIZE;
}

uint8_t pico_dhcp_opt_end(void *ptr)
{
    uint8_t *opt = (uint8_t *)ptr;

    /* option: end of options */
    *opt = PICO_DHCP_OPT_END;
    return PICO_DHCP_OPTLEN_END;
}

#endif
