/*********************************************************************
   PicoTCP. Copyright (c) 2012 TASS Belgium NV. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
   .
   Authors: Kristof Roelants
 *********************************************************************/
#include "pico_config.h"
#include "pico_stack.h"
#include "pico_addressing.h"
#include "pico_socket.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_dns_client.h"
#include "pico_dns_common.h"
#include "pico_tree.h"

#ifdef PICO_SUPPORT_DNS_CLIENT

#ifdef PICO_SUPPORT_IPV4

#ifdef DEBUG_DNS
    #define dns_dbg dbg
#else
    #define dns_dbg(...) do {} while(0)
#endif

/* DNS response length */
#define PICO_DNS_MAX_QUERY_LEN 255
#define PICO_DNS_MAX_QUERY_LABEL_LEN 63

/* DNS client retransmission time (msec) + frequency */
#define PICO_DNS_CLIENT_RETRANS 4000
#define PICO_DNS_CLIENT_MAX_RETRANS 3

static void pico_dns_client_callback(uint16_t ev, struct pico_socket *s);
static void pico_dns_client_retransmission(pico_time now, void *arg);
static int pico_dns_client_getaddr_init(const char *url, uint16_t proto, void (*callback)(char *, void *), void *arg);

struct pico_dns_ns
{
    struct pico_ip4 ns; /* nameserver */
};

static int dns_ns_cmp(void *ka, void *kb)
{
    struct pico_dns_ns *a = ka, *b = kb;
    return pico_ipv4_compare(&a->ns, &b->ns);
}
static PICO_TREE_DECLARE(NSTable, dns_ns_cmp);

struct pico_dns_query
{
    char *query;
    uint16_t len;
    uint16_t id;
    uint16_t qtype;
    uint16_t qclass;
    uint8_t retrans;
    struct pico_dns_ns q_ns;
    struct pico_socket *s;
    void (*callback)(char *, void *);
    void *arg;
};

static int dns_query_cmp(void *ka, void *kb)
{
    struct pico_dns_query *a = ka, *b = kb;
    if (a->id == b->id)
        return 0;

    return (a->id < b->id) ? (-1) : (1);
}
static PICO_TREE_DECLARE(DNSTable, dns_query_cmp);

static int pico_dns_client_del_ns(struct pico_ip4 *ns_addr)
{
    struct pico_dns_ns test = {{0}}, *found = NULL;

    test.ns = *ns_addr;
    found = pico_tree_findKey(&NSTable, &test);
    if (!found)
        return -1;

    pico_tree_delete(&NSTable, found);
    PICO_FREE(found);

    /* no NS left, add default NS */
    if (pico_tree_empty(&NSTable))
        pico_dns_client_init();

    return 0;
}

static struct pico_dns_ns *pico_dns_client_add_ns(struct pico_ip4 *ns_addr)
{
    struct pico_dns_ns *dns = NULL, *found = NULL, test = {{0}};
    struct pico_ip4 zero = {
        0
    };                          /* 0.0.0.0 */

    /* Do not add 0.0.0.0 addresses, which some DHCP servers might reply */
    if (!pico_ipv4_compare(ns_addr, &zero))
    {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    dns = PICO_ZALLOC(sizeof(struct pico_dns_ns));
    if (!dns) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    dns->ns = *ns_addr;

    found = pico_tree_insert(&NSTable, dns);
    if (found) { /* nameserver already present or out of memory */
        PICO_FREE(dns);
        if ((void *)found == (void *)&LEAF)
            return NULL;
        else
            return found;
    }

    /* default NS found, remove it */
    pico_string_to_ipv4(PICO_DNS_NS_DEFAULT, (uint32_t *)&test.ns.addr);
    found = pico_tree_findKey(&NSTable, &test);
    if (found && (found->ns.addr != ns_addr->addr))
        pico_dns_client_del_ns(&found->ns);

    return dns;
}

static struct pico_dns_ns pico_dns_client_next_ns(struct pico_ip4 *ns_addr)
{
    struct pico_dns_ns dns = {{0}}, *nxtdns = NULL;
    struct pico_tree_node *node = NULL, *nxtnode = NULL;

    dns.ns = *ns_addr;
    node = pico_tree_findNode(&NSTable, &dns);
    if (!node)
        return dns; /* keep using current NS */

    nxtnode = pico_tree_next(node);
    nxtdns = nxtnode->keyValue;
    if (!nxtdns)
        nxtdns = (struct pico_dns_ns *)pico_tree_first(&NSTable);

    return *nxtdns;
}

static struct pico_dns_query *pico_dns_client_add_query(struct pico_dns_header *hdr, uint16_t len, struct pico_dns_question_suffix *suffix,
                                                        void (*callback)(char *, void *), void *arg)
{
    struct pico_dns_query *q = NULL, *found = NULL;

    q = PICO_ZALLOC(sizeof(struct pico_dns_query));
    if (!q)
        return NULL;

    q->query = (char *)hdr;
    q->len = len;
    q->id = short_be(hdr->id);
    q->qtype = short_be(suffix->qtype);
    q->qclass = short_be(suffix->qclass);
    q->retrans = 1;
    q->q_ns = *((struct pico_dns_ns *)pico_tree_first(&NSTable));
    q->callback = callback;
    q->arg = arg;
    q->s = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_UDP, &pico_dns_client_callback);
    if (!q->s) {
        PICO_FREE(q);
        return NULL;
    }

    found = pico_tree_insert(&DNSTable, q);
    if (found) {
        if ((void *)found != (void *)&LEAF) /* If found == &LEAF we're out of memory and pico_err is set */
            pico_err = PICO_ERR_EAGAIN;
        pico_socket_close(q->s);
        PICO_FREE(q);
        return NULL;
    }

    return q;
}

static int pico_dns_client_del_query(uint16_t id)
{
    struct pico_dns_query test = {
        0
    }, *found = NULL;

    test.id = id;
    found = pico_tree_findKey(&DNSTable, &test);
    if (!found)
        return -1;

    PICO_FREE(found->query);
    pico_socket_close(found->s);
    pico_tree_delete(&DNSTable, found);
    PICO_FREE(found);
    return 0;
}

static struct pico_dns_query *pico_dns_client_find_query(uint16_t id)
{
    struct pico_dns_query test = {
        0
    }, *found = NULL;

    test.id = id;
    found = pico_tree_findKey(&DNSTable, &test);
    if (found)
        return found;
    else
        return NULL;
}

/* seek end of string */
static char *pico_dns_client_seek(char *ptr)
{
    if (!ptr)
        return NULL;

    while (*ptr != 0)
        ptr++;
    return ptr + 1;
}

static struct pico_dns_query *pico_dns_client_idcheck(uint16_t id)
{
    struct pico_dns_query test = {
        0
    };

    test.id = id;
    return pico_tree_findKey(&DNSTable, &test);
}

static int pico_dns_client_query_header(struct pico_dns_header *hdr)
{
    uint16_t id = 0;
    uint8_t retry = 32;

    do {
        id = (uint16_t)(pico_rand() & 0xFFFFU);
        dns_dbg("DNS: generated id %u\n", id);
    } while (retry-- && pico_dns_client_idcheck(id));
    if (!retry)
        return -1;

    hdr->id = short_be(id);
    pico_dns_fill_packet_header(hdr, 1, 0, 0, 0); /* 1 question, 0 answers */

    return 0;
}

static int pico_dns_client_check_header(struct pico_dns_header *pre)
{
    if (pre->qr != PICO_DNS_QR_RESPONSE || pre->opcode != PICO_DNS_OPCODE_QUERY || pre->rcode != PICO_DNS_RCODE_NO_ERROR) {
        dns_dbg("DNS ERROR: OPCODE %d | TC %d | RCODE %d\n", pre->opcode, pre->tc, pre->rcode);
        return -1;
    }

    if (short_be(pre->ancount) < 1) {
        dns_dbg("DNS ERROR: ancount < 1\n");
        return -1;
    }

    return 0;
}

static int pico_dns_client_check_qsuffix(struct pico_dns_question_suffix *suf, struct pico_dns_query *q)
{
    if (!suf)
        return -1;

    if (short_be(suf->qtype) != q->qtype || short_be(suf->qclass) != q->qclass) {
        dns_dbg("DNS ERROR: received qtype (%u) or qclass (%u) incorrect\n", short_be(suf->qtype), short_be(suf->qclass));
        return -1;
    }

    return 0;
}

static int pico_dns_client_check_url(struct pico_dns_header *resp, struct pico_dns_query *q)
{
    char *recv_name = (char*)(resp) + sizeof(struct pico_dns_header) + PICO_DNS_LABEL_INITIAL;
    char *exp_name = (char *)(q->query) + sizeof(struct pico_dns_header) + PICO_DNS_LABEL_INITIAL;
    if (strcasecmp(recv_name,  exp_name) != 0)
        return -1;

    return 0;
}

static int pico_dns_client_check_asuffix(struct pico_dns_record_suffix *suf, struct pico_dns_query *q)
{
    if (!suf) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if (short_be(suf->rtype) != q->qtype || short_be(suf->rclass) != q->qclass) {
        dns_dbg("DNS WARNING: received qtype (%u) or qclass (%u) incorrect\n", short_be(suf->rtype), short_be(suf->rclass));
        return -1;
    }

    if (long_be(suf->rttl) > PICO_DNS_MAX_TTL) {
        dns_dbg("DNS WARNING: received TTL (%u) > MAX (%u)\n", long_be(suf->rttl), PICO_DNS_MAX_TTL);
        return -1;
    }

    return 0;
}

static char *pico_dns_client_seek_suffix(char *suf, struct pico_dns_header *pre, struct pico_dns_query *q)
{
    struct pico_dns_record_suffix *asuffix = NULL;
    uint16_t comp = 0, compression = 0;
    uint16_t i = 0;

    if (!suf)
        return NULL;

    while (i++ < short_be(pre->ancount)) {
        comp = short_from(suf);
        compression = short_be(comp);
        switch (compression >> 14)
        {
        case PICO_DNS_POINTER:
            while (compression >> 14 == PICO_DNS_POINTER) {
                dns_dbg("DNS: pointer\n");
                suf += sizeof(uint16_t);
                comp = short_from(suf);
                compression = short_be(comp);
            }
            break;

        case PICO_DNS_LABEL:
            dns_dbg("DNS: label\n");
            suf = pico_dns_client_seek(suf);
            break;

        default:
            dns_dbg("DNS ERROR: incorrect compression (%u) value\n", compression);
            return NULL;
        }

        asuffix = (struct pico_dns_record_suffix *)suf;
        if (!asuffix)
            break;

        if (pico_dns_client_check_asuffix(asuffix, q) < 0) {
            suf += (sizeof(struct pico_dns_record_suffix) + short_be(asuffix->rdlength));
            continue;
        }

        return suf;
    }
    return NULL;
}

static int pico_dns_client_send(struct pico_dns_query *q)
{
    uint16_t *paramID = PICO_ZALLOC(sizeof(uint16_t));
    if (!paramID) {
        pico_err = PICO_ERR_ENOMEM;
        return -1;
    }

    dns_dbg("DNS: sending query to %08X\n", q->q_ns.ns.addr);
    if (!q->s)
        goto failure;

    if (pico_socket_connect(q->s, &q->q_ns.ns, short_be(PICO_DNS_NS_PORT)) < 0)
        goto failure;

    pico_socket_send(q->s, q->query, q->len);
    *paramID = q->id;
    if (!pico_timer_add(PICO_DNS_CLIENT_RETRANS, pico_dns_client_retransmission, paramID)) {
        dns_dbg("DNS: Failed to start retransmission timer\n");
        goto failure;
    }

    return 0;

failure:
    PICO_FREE(paramID);
    return -1;
}

static void pico_dns_client_retransmission(pico_time now, void *arg)
{
    struct pico_dns_query *q = NULL;
    struct pico_dns_query dummy;
    IGNORE_PARAMETER(now);

    if(!arg)
        return;

    /* search for the dns query and free used space */
    dummy.id = *(uint16_t *)arg;
    q = (struct pico_dns_query *)pico_tree_findKey(&DNSTable, &dummy);
    PICO_FREE(arg);

    /* dns query successful? */
    if (!q) {
        return;
    }

    q->retrans++;
    if (q->retrans <= PICO_DNS_CLIENT_MAX_RETRANS) {
        q->q_ns = pico_dns_client_next_ns(&q->q_ns.ns);
        pico_dns_client_send(q);
    } else {
        pico_err = PICO_ERR_EIO;
        q->callback(NULL, q->arg);
        pico_dns_client_del_query(q->id);
    }
}

static int pico_dns_client_check_rdlength(uint16_t qtype, uint16_t rdlength)
{
    switch (qtype)
    {
        case PICO_DNS_TYPE_A:
            if (rdlength != PICO_DNS_RR_A_RDLENGTH)
                return -1;
            break;
#ifdef PICO_SUPPORT_IPV6
        case PICO_DNS_TYPE_AAAA:
            if (rdlength != PICO_DNS_RR_AAAA_RDLENGTH)
                return -1;
            break;
#endif
        default:
            break;
    }

    return 0;
}

static int pico_dns_client_user_callback(struct pico_dns_record_suffix *asuffix, struct pico_dns_query *q)
{
    uint32_t ip = 0;
    char *str = NULL;
    char *rdata = (char *) asuffix + sizeof(struct pico_dns_record_suffix);

    if (pico_dns_client_check_rdlength(q->qtype, short_be(asuffix->rdlength)) < 0) {
        dns_dbg("DNS ERROR: Invalid RR rdlength: %u\n", short_be(asuffix->rdlength));
        return -1;
    }

    switch (q->qtype)
    {
    case PICO_DNS_TYPE_A:
        ip = long_from(rdata);
        str = PICO_ZALLOC(PICO_DNS_IPV4_ADDR_LEN);
        pico_ipv4_to_string(str, ip);
        break;
#ifdef PICO_SUPPORT_IPV6
    case PICO_DNS_TYPE_AAAA:
    {
        struct pico_ip6 ip6;
        memcpy(&ip6.addr, rdata, sizeof(struct pico_ip6));
        str = PICO_ZALLOC(PICO_DNS_IPV6_ADDR_LEN);
        pico_ipv6_to_string(str, ip6.addr);
        break;
    }
#endif
    case PICO_DNS_TYPE_PTR:
        /* TODO: check for decompression / rdlength vs. decompressed length */
        pico_dns_notation_to_name(rdata, short_be(asuffix->rdlength));
        str = PICO_ZALLOC((size_t)(short_be(asuffix->rdlength) -
                                   PICO_DNS_LABEL_INITIAL));
        if (!str) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        memcpy(str, rdata + PICO_DNS_LABEL_INITIAL, short_be(asuffix->rdlength) - PICO_DNS_LABEL_INITIAL);
        break;

    default:
        dns_dbg("DNS ERROR: incorrect qtype (%u)\n", q->qtype);
        break;
    }

    if (q->retrans) {
        q->callback(str, q->arg);
        q->retrans = 0;
        pico_dns_client_del_query(q->id);
    }

    if (str)
        PICO_FREE(str);

    return 0;
}

static char dns_response[PICO_IP_MRU] = {
    0
};

static void pico_dns_try_fallback_cname(struct pico_dns_query *q, struct pico_dns_header *h, struct pico_dns_question_suffix *qsuffix)
{
    uint16_t type = q->qtype;
    uint16_t proto = PICO_PROTO_IPV4;
    struct pico_dns_record_suffix *asuffix = NULL;
    char *p_asuffix = NULL;
    char *cname_orig = NULL;
    char *cname = NULL;
    uint16_t cname_len;

    /* Try to use CNAME only if A or AAAA query is ongoing */
    if (type != PICO_DNS_TYPE_A && type != PICO_DNS_TYPE_AAAA)
        return;

    if (type == PICO_DNS_TYPE_AAAA)
        proto = PICO_PROTO_IPV6;

    q->qtype = PICO_DNS_TYPE_CNAME;
    p_asuffix = (char *)qsuffix + sizeof(struct pico_dns_question_suffix);
    p_asuffix = pico_dns_client_seek_suffix(p_asuffix, h, q);
    if (!p_asuffix) {
        return;
    }

    /* Found CNAME response. Re-initiating query. */
    asuffix = (struct pico_dns_record_suffix *)p_asuffix;
    cname = pico_dns_decompress_name((char *)asuffix + sizeof(struct pico_dns_record_suffix), (pico_dns_packet *)h); /* allocates memory! */
    cname_orig = cname; /* to free later */

    if (cname == NULL)
        return;

    cname_len = (uint16_t)(pico_dns_strlen(cname) + 1);

    pico_dns_notation_to_name(cname, cname_len);
    if (cname[0] == '.')
        cname++;

    dns_dbg("Restarting query for name '%s'\n", cname);
    pico_dns_client_getaddr_init(cname, proto, q->callback, q->arg);
    PICO_FREE(cname_orig);
    pico_dns_client_del_query(q->id);
}

static void pico_dns_client_callback(uint16_t ev, struct pico_socket *s)
{
    struct pico_dns_header *header = NULL;
    char *domain;
    struct pico_dns_question_suffix *qsuffix = NULL;
    struct pico_dns_record_suffix *asuffix = NULL;
    struct pico_dns_query *q = NULL;
    char *p_asuffix = NULL;

    if (ev == PICO_SOCK_EV_ERR) {
        dns_dbg("DNS: socket error received\n");
        return;
    }

    if (ev & PICO_SOCK_EV_RD) {
        if (pico_socket_read(s, dns_response, PICO_IP_MRU) < 0)
            return;
    }

    header = (struct pico_dns_header *)dns_response;
    domain = (char *)header + sizeof(struct pico_dns_header);
    qsuffix = (struct pico_dns_question_suffix *)pico_dns_client_seek(domain);
    /* valid asuffix is determined dynamically later on */

    if (pico_dns_client_check_header(header) < 0)
        return;

    q = pico_dns_client_find_query(short_be(header->id));
    if (!q)
        return;

    if (pico_dns_client_check_qsuffix(qsuffix, q) < 0)
        return;

    if (pico_dns_client_check_url(header, q) < 0)
        return;

    p_asuffix = (char *)qsuffix + sizeof(struct pico_dns_question_suffix);
    p_asuffix = pico_dns_client_seek_suffix(p_asuffix, header, q);
    if (!p_asuffix) {
        pico_dns_try_fallback_cname(q, header, qsuffix);
        return;
    }

    asuffix = (struct pico_dns_record_suffix *)p_asuffix;
    pico_dns_client_user_callback(asuffix, q);

    return;
}

static int pico_dns_create_message(struct pico_dns_header **header, struct pico_dns_question_suffix **qsuffix, enum pico_dns_arpa arpa, const char *url, uint16_t *urlen, uint16_t *hdrlen)
{
    char *domain;
    char inaddr_arpa[14];
    uint16_t strlen = 0, arpalen = 0;

    if (!url) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if(arpa == PICO_DNS_ARPA4) {
        strcpy(inaddr_arpa, ".in-addr.arpa");
        strlen = pico_dns_strlen(url);
    }

#ifdef PICO_SUPPORT_IPV6
    else if (arpa == PICO_DNS_ARPA6) {
        strcpy(inaddr_arpa, ".IP6.ARPA");
        strlen = STRLEN_PTR_IP6;
    }
#endif
    else {
        strcpy(inaddr_arpa, "");
        strlen = pico_dns_strlen(url);
    }

    arpalen = pico_dns_strlen(inaddr_arpa);
    *urlen = (uint16_t)(PICO_DNS_LABEL_INITIAL + strlen + arpalen + PICO_DNS_LABEL_ROOT);
    *hdrlen = (uint16_t)(sizeof(struct pico_dns_header) + *urlen + sizeof(struct pico_dns_question_suffix));
    *header = PICO_ZALLOC(*hdrlen);
    if (!*header) {
        pico_err = PICO_ERR_ENOMEM;
        return -1;
    }

    *header = (struct pico_dns_header *)*header;
    domain = (char *) *header + sizeof(struct pico_dns_header);
    *qsuffix = (struct pico_dns_question_suffix *)(domain + *urlen);

    if(arpa == PICO_DNS_ARPA4) {
        memcpy(domain + PICO_DNS_LABEL_INITIAL, url, strlen);
        pico_dns_mirror_addr(domain + PICO_DNS_LABEL_INITIAL);
        memcpy(domain + PICO_DNS_LABEL_INITIAL + strlen, inaddr_arpa, arpalen);
    }

#ifdef PICO_SUPPORT_IPV6
    else if (arpa == PICO_DNS_ARPA6) {
        pico_dns_ipv6_set_ptr(url, domain + PICO_DNS_LABEL_INITIAL);
        memcpy(domain + PICO_DNS_LABEL_INITIAL + STRLEN_PTR_IP6, inaddr_arpa, arpalen);
    }
#endif
    else {
        memcpy(domain + PICO_DNS_LABEL_INITIAL, url, strlen);
    }

    /* assemble dns message */
    pico_dns_client_query_header(*header);
    pico_dns_name_to_dns_notation(domain, strlen);

    return 0;
}

static int pico_dns_client_addr_label_check_len(const char *url)
{
    const char *p, *label;
    int count;
    label = url;
    p = label;

    while(*p != (char) 0) {
        count = 0;
        while((*p != (char)0)) {
            if (*p == '.') {
                label = ++p;
                break;
            }

            count++;
            p++;
            if (count > PICO_DNS_MAX_QUERY_LABEL_LEN)
                return -1;
        }
    }
    return 0;
}

static int pico_dns_client_getaddr_check(const char *url, void (*callback)(char *, void *))
{
    if (!url || !callback) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if (strlen(url) > PICO_DNS_MAX_QUERY_LEN) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if (pico_dns_client_addr_label_check_len(url) < 0) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    return 0;
}

static int pico_dns_client_getaddr_init(const char *url, uint16_t proto, void (*callback)(char *, void *), void *arg)
{
    struct pico_dns_header *header = NULL;
    struct pico_dns_question_suffix *qsuffix = NULL;
    struct pico_dns_query *q = NULL;
    uint16_t len = 0, lblen = 0;
    (void)proto;

    if (pico_dns_client_getaddr_check(url, callback) < 0)
        return -1;

    if(pico_dns_create_message(&header, &qsuffix, PICO_DNS_NO_ARPA, url, &lblen, &len) != 0)
        return -1;

#ifdef PICO_SUPPORT_IPV6
    if (proto == PICO_PROTO_IPV6) {
        pico_dns_question_fill_suffix(qsuffix, PICO_DNS_TYPE_AAAA, PICO_DNS_CLASS_IN);
    } else
#endif
    pico_dns_question_fill_suffix(qsuffix, PICO_DNS_TYPE_A, PICO_DNS_CLASS_IN);

    q = pico_dns_client_add_query(header, len, qsuffix, callback, arg);
    if (!q) {
        PICO_FREE(header);
        return -1;
    }

    if (pico_dns_client_send(q) < 0) {
        pico_dns_client_del_query(q->id); /* frees msg */
        return -1;
    }

    return 0;
}

int pico_dns_client_getaddr(const char *url, void (*callback)(char *, void *), void *arg)
{
    return pico_dns_client_getaddr_init(url, PICO_PROTO_IPV4, callback, arg);
}

int pico_dns_client_getaddr6(const char *url, void (*callback)(char *, void *), void *arg)
{
    return pico_dns_client_getaddr_init(url, PICO_PROTO_IPV6, callback, arg);
}

static int pico_dns_getname_univ(const char *ip, void (*callback)(char *, void *), void *arg, enum pico_dns_arpa arpa)
{
    struct pico_dns_header *header = NULL;
    struct pico_dns_question_suffix *qsuffix = NULL;
    struct pico_dns_query *q = NULL;
    uint16_t len = 0, lblen = 0;

    if (!ip || !callback) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if(pico_dns_create_message(&header, &qsuffix, arpa, ip, &lblen, &len) != 0)
        return -1;

    pico_dns_question_fill_suffix(qsuffix, PICO_DNS_TYPE_PTR, PICO_DNS_CLASS_IN);
    q = pico_dns_client_add_query(header, len, qsuffix, callback, arg);
    if (!q) {
        PICO_FREE(header);
        return -1;
    }

    if (pico_dns_client_send(q) < 0) {
        pico_dns_client_del_query(q->id); /* frees header */
        return -1;
    }

    return 0;
}

int pico_dns_client_getname(const char *ip, void (*callback)(char *, void *), void *arg)
{
    return pico_dns_getname_univ(ip, callback, arg, PICO_DNS_ARPA4);
}


int pico_dns_client_getname6(const char *ip, void (*callback)(char *, void *), void *arg)
{
    return pico_dns_getname_univ(ip, callback, arg, PICO_DNS_ARPA6);
}

int pico_dns_client_nameserver(struct pico_ip4 *ns, uint8_t flag)
{
    if (!ns) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    switch (flag)
    {
    case PICO_DNS_NS_ADD:
        if (!pico_dns_client_add_ns(ns))
            return -1;

        break;

    case PICO_DNS_NS_DEL:
        if (pico_dns_client_del_ns(ns) < 0) {
            pico_err = PICO_ERR_EINVAL;
            return -1;
        }

        break;

    default:
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }
    return 0;
}

int pico_dns_client_init(void)
{
    struct pico_ip4 default_ns = {
        0
    };

    if (pico_string_to_ipv4(PICO_DNS_NS_DEFAULT, (uint32_t *)&default_ns.addr) < 0)
        return -1;

    return pico_dns_client_nameserver(&default_ns, PICO_DNS_NS_ADD);
}

#else

int pico_dns_client_init(void)
{
    dbg("ERROR Trying to initialize DNS module: IPv4 not supported in this build.\n");
    return -1;
}
#endif /* PICO_SUPPORT_IPV4 */


#endif /* PICO_SUPPORT_DNS_CLIENT */

