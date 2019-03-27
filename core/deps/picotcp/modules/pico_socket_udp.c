#include "pico_config.h"
#include "pico_socket.h"
#include "pico_udp.h"
#include "pico_socket_multicast.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_socket_udp.h"

#define UDP_FRAME_OVERHEAD (sizeof(struct pico_frame))


struct pico_socket *pico_socket_udp_open(void)
{
    struct pico_socket *s = NULL;
#ifdef PICO_SUPPORT_UDP
    s = pico_udp_open();
    if (!s) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    s->proto = &pico_proto_udp;
    s->q_in.overhead = UDP_FRAME_OVERHEAD;
    s->q_out.overhead = UDP_FRAME_OVERHEAD;
#endif
    return s;
}


#if defined (PICO_SUPPORT_IPV4) || defined (PICO_SUPPORT_IPV6)
static int pico_enqueue_and_wakeup_if_needed(struct pico_queue *q_in, struct pico_socket* s, struct pico_frame* cpy)
{
        if (pico_enqueue(q_in, cpy) > 0) {
            if (s->wakeup){
                s->wakeup(PICO_SOCK_EV_RD, s);
            }
        }
        else {
            pico_frame_discard(cpy);
            return -1;
        }
        return 0;
}
#endif

#ifdef PICO_SUPPORT_IPV4
#ifdef PICO_SUPPORT_MCAST
static inline int pico_socket_udp_deliver_ipv4_mcast_initial_checks(struct pico_socket *s, struct pico_frame *f)
{
    struct pico_ip4 p_dst;
    struct pico_ipv4_hdr *ip4hdr;

    ip4hdr = (struct pico_ipv4_hdr*)(f->net_hdr);
    p_dst.addr = ip4hdr->dst.addr;
    if (pico_ipv4_is_multicast(p_dst.addr) && (pico_socket_mcast_filter(s, (union pico_address *)&ip4hdr->dst, (union pico_address *)&ip4hdr->src) < 0))
        return -1;


    if ((pico_ipv4_link_get(&ip4hdr->src)) && (PICO_SOCKET_GETOPT(s, PICO_SOCKET_OPT_MULTICAST_LOOP) == 0u)) {
        /* Datagram from ourselves, Loop disabled, discarding. */
        return -1;
    }

    return 0;
}


static int pico_socket_udp_deliver_ipv4_mcast(struct pico_socket *s, struct pico_frame *f)
{
    struct pico_ip4 s_local;
    struct pico_frame *cpy;
    struct pico_device *dev = pico_ipv4_link_find(&s->local_addr.ip4);

    s_local.addr = s->local_addr.ip4.addr;

    if (pico_socket_udp_deliver_ipv4_mcast_initial_checks(s, f) < 0)
        return 0;

    if ((s_local.addr == PICO_IPV4_INADDR_ANY) || /* If our local ip is ANY, or.. */
        (dev == f->dev)) {     /* the source of the bcast packet is a neighbor... */
        cpy = pico_frame_copy(f);
        if (!cpy)
            return -1;

        pico_enqueue_and_wakeup_if_needed(&s->q_in, s, cpy);
    }

    return 0;
}
#endif
static int pico_socket_udp_deliver_ipv4_unicast(struct pico_socket *s, struct pico_frame *f)
{
    struct pico_frame *cpy;
    /* Either local socket is ANY, or matches dst */
    cpy = pico_frame_copy(f);
    if (!cpy)
        return -1;

    pico_enqueue_and_wakeup_if_needed(&s->q_in, s, cpy);

    return 0;
}


static int pico_socket_udp_deliver_ipv4(struct pico_socket *s, struct pico_frame *f)
{
    int ret = 0;
    struct pico_ip4 s_local, p_dst;
    struct pico_ipv4_hdr *ip4hdr;
    ip4hdr = (struct pico_ipv4_hdr*)(f->net_hdr);
    s_local.addr = s->local_addr.ip4.addr;
    p_dst.addr = ip4hdr->dst.addr;
    if ((pico_ipv4_is_broadcast(p_dst.addr)) || pico_ipv4_is_multicast(p_dst.addr)) {
#ifdef PICO_SUPPORT_MCAST
        ret = pico_socket_udp_deliver_ipv4_mcast(s, f);
#endif
    } else if ((s_local.addr == PICO_IPV4_INADDR_ANY) || (s_local.addr == p_dst.addr)) {
        ret = pico_socket_udp_deliver_ipv4_unicast(s, f);
    }

    pico_frame_discard(f);
    return ret;
}
#endif

#ifdef PICO_SUPPORT_IPV6
#ifdef PICO_SUPPORT_MCAST
static inline int pico_socket_udp_deliver_ipv6_mcast(struct pico_socket *s, struct pico_frame *f)
{
    struct pico_ipv6_hdr *ip6hdr;
    struct pico_frame *cpy;
    struct pico_device *dev = pico_ipv6_link_find(&s->local_addr.ip6);

    ip6hdr = (struct pico_ipv6_hdr*)(f->net_hdr);

    if ((pico_ipv6_link_get(&ip6hdr->src)) && (PICO_SOCKET_GETOPT(s, PICO_SOCKET_OPT_MULTICAST_LOOP) == 0u)) {
        /* Datagram from ourselves, Loop disabled, discarding. */
        return 0;
    }


    if (pico_ipv6_is_unspecified(s->local_addr.ip6.addr) || /* If our local ip is ANY, or.. */
        (dev == f->dev)) {     /* the source of the bcast packet is a neighbor... */
        cpy = pico_frame_copy(f);
        if (!cpy)
        {
            return -1;
        }

        pico_enqueue_and_wakeup_if_needed(&s->q_in, s, cpy);
    }

    return 0;
}
#endif
static int pico_socket_udp_deliver_ipv6(struct pico_socket *s, struct pico_frame *f)
{
    struct pico_ip6 s_local, p_dst;
    struct pico_ipv6_hdr *ip6hdr;
    struct pico_frame *cpy;
    ip6hdr = (struct pico_ipv6_hdr*)(f->net_hdr);
    s_local = s->local_addr.ip6;
    p_dst = ip6hdr->dst;
    if ((pico_ipv6_is_multicast(p_dst.addr))) {
#ifdef PICO_SUPPORT_MCAST
        int retval = pico_socket_udp_deliver_ipv6_mcast(s, f);
        pico_frame_discard(f);
        return retval;
#endif
    }
    else if (pico_ipv6_is_unspecified(s->local_addr.ip6.addr) || (pico_ipv6_compare(&s_local, &p_dst) == 0))
    { /* Either local socket is ANY, or matches dst */
        cpy = pico_frame_copy(f);
        if (!cpy)
        {
            pico_frame_discard(f);
            return -1;
        }

        pico_enqueue_and_wakeup_if_needed(&s->q_in, s, cpy);
    }

    pico_frame_discard(f);
    return 0;
}
#endif


int pico_socket_udp_deliver(struct pico_sockport *sp, struct pico_frame *f)
{
    struct pico_tree_node *index = NULL;
    struct pico_tree_node *_tmp;
    struct pico_socket *s = NULL;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    #ifdef PICO_SUPPORT_UDP
    pico_err = PICO_ERR_NOERR;
    pico_tree_foreach_safe(index, &sp->socks, _tmp){
        s = index->keyValue;
        if (IS_IPV4(f)) { /* IPV4 */
#ifdef PICO_SUPPORT_IPV4
            return pico_socket_udp_deliver_ipv4(s, f);
#endif
        } else if (IS_IPV6(f)) {
#ifdef PICO_SUPPORT_IPV6
            return pico_socket_udp_deliver_ipv6(s, f);
#endif
        } else {
            /* something wrong in the packet header*/
        }
    } /* FOREACH */
    pico_frame_discard(f);
    if (s)
        return 0;

    pico_err = PICO_ERR_ENXIO;
  #endif
    return -1;
}

int pico_setsockopt_udp(struct pico_socket *s, int option, void *value)
{
    switch(option) {
    case PICO_SOCKET_OPT_RCVBUF:
        s->q_in.max_size = (*(uint32_t*)value);
        return 0;
    case PICO_SOCKET_OPT_SNDBUF:
        s->q_out.max_size = (*(uint32_t*)value);
        return 0;
    }

    /* switch's default */
#ifdef PICO_SUPPORT_MCAST
    return pico_setsockopt_mcast(s, option, value);
#else
    pico_err = PICO_ERR_EINVAL;
    return -1;
#endif
}

int pico_getsockopt_udp(struct pico_socket *s, int option, void *value)
{
    uint32_t *val = (uint32_t *)value;
    switch(option) {
    case PICO_SOCKET_OPT_RCVBUF:
        *val = s->q_in.max_size;
        return 0;
    case PICO_SOCKET_OPT_SNDBUF:
        *val = s->q_out.max_size;
        return 0;
    }

    /* switch's default */
#ifdef PICO_SUPPORT_MCAST
    return pico_getsockopt_mcast(s, option, value);
#else
    pico_err = PICO_ERR_EINVAL;
    return -1;
#endif
}

