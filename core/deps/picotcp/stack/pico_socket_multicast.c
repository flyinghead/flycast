#include "pico_config.h"
#include "pico_stack.h"
#include "pico_socket.h"
#include "pico_socket_multicast.h"
#include "pico_tree.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_udp.h"

#ifdef PICO_SUPPORT_MCAST

#ifdef DEBUG_MCAST
#define so_mcast_dbg dbg
#else
#define so_mcast_dbg(...) do { } while(0)
#endif

/*                       socket
 *                         |
 *                    MCASTListen
 *                    |    |     |
 *         ------------    |     ------------
 *         |               |                |
 *   MCASTSources    MCASTSources     MCASTSources
 *   |  |  |  |      |  |  |  |       |  |  |  |
 *   S  S  S  S      S  S  S  S       S  S  S  S
 *
 *   MCASTListen: RBTree(mcast_link, mcast_group)
 *   MCASTSources: RBTree(source)
 */
struct pico_mcast_listen
{
    int8_t filter_mode;
    union pico_address mcast_link;
    union pico_address mcast_group;
    struct pico_tree MCASTSources;
    struct pico_tree MCASTSources_ipv6;
    uint16_t proto;
};
/* Parameters */
struct pico_mcast
{
    struct pico_socket *s;
    struct pico_ip_mreq *mreq;
    struct pico_ip_mreq_source *mreq_s;
    union pico_address *address;
    union pico_link *mcast_link;
    struct pico_mcast_listen *listen;
};
static int mcast_listen_link_cmp(struct pico_mcast_listen *a, struct pico_mcast_listen *b)
{

    if (a->proto < b->proto)
        return -1;

    if (a->proto > b->proto)
        return 1;

    return pico_address_compare(&a->mcast_link, &b->mcast_link, a->proto);
}

static int mcast_listen_grp_cmp(struct pico_mcast_listen *a, struct pico_mcast_listen *b)
{
    if (a->mcast_group.ip4.addr < b->mcast_group.ip4.addr)
        return -1;

    if (a->mcast_group.ip4.addr > b->mcast_group.ip4.addr)
        return 1;

    return mcast_listen_link_cmp(a, b);
}
#ifdef PICO_SUPPORT_IPV6
static int mcast_listen_grp_cmp_ipv6(struct pico_mcast_listen *a, struct pico_mcast_listen *b)
{
    int tmp = memcmp(&a->mcast_group.ip6, &b->mcast_group.ip6, sizeof(struct pico_ip6));
    if(!tmp)
        return mcast_listen_link_cmp(a, b);

    return tmp;
}
#endif

static int mcast_listen_cmp(void *ka, void *kb)
{
    struct pico_mcast_listen *a = ka, *b = kb;
    if (a->proto < b->proto)
        return -1;

    if (a->proto > b->proto)
        return 1;

    return mcast_listen_grp_cmp(a, b);
}
#ifdef PICO_SUPPORT_IPV6
static int mcast_listen_cmp_ipv6(void *ka, void *kb)
{
    struct pico_mcast_listen *a = ka, *b = kb;
    if (a->proto < b->proto)
        return -1;

    if (a->proto > b->proto)
        return 1;

    return mcast_listen_grp_cmp_ipv6(a, b);
}
#endif
static int mcast_sources_cmp(void *ka, void *kb)
{
    union pico_address *a = ka, *b = kb;
    if (a->ip4.addr < b->ip4.addr)
        return -1;

    if (a->ip4.addr > b->ip4.addr)
        return 1;

    return 0;
}
#ifdef PICO_SUPPORT_IPV6
static int mcast_sources_cmp_ipv6(void *ka, void *kb)
{
    union pico_address *a = ka, *b = kb;
    return memcmp(&a->ip6, &b->ip6, sizeof(struct pico_ip6));
}
#endif
static int mcast_socket_cmp(void *ka, void *kb)
{
    struct pico_socket *a = ka, *b = kb;
    if (a < b)
        return -1;

    if (a > b)
        return 1;

    return 0;
}

/* gather all multicast sockets to hasten filter aggregation */
static PICO_TREE_DECLARE(MCASTSockets, mcast_socket_cmp);

static int mcast_filter_cmp(void *ka, void *kb)
{
    union pico_address *a = ka, *b = kb;
    if (a->ip4.addr < b->ip4.addr)
        return -1;

    if (a->ip4.addr > b->ip4.addr)
        return 1;

    return 0;
}
/* gather sources to be filtered */
static PICO_TREE_DECLARE(MCASTFilter, mcast_filter_cmp);

static int mcast_filter_cmp_ipv6(void *ka, void *kb)
{
    union pico_address *a = ka, *b = kb;
    return memcmp(&a->ip6, &b->ip6, sizeof(struct pico_ip6));
}
/* gather sources to be filtered */
static PICO_TREE_DECLARE(MCASTFilter_ipv6, mcast_filter_cmp_ipv6);

inline static struct pico_tree *mcast_get_src_tree(struct pico_socket *s, struct pico_mcast *mcast)
{
    if( IS_SOCK_IPV4(s)) {
        mcast->listen->MCASTSources.compare = mcast_sources_cmp;
        return &mcast->listen->MCASTSources;
    }

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s)) {
        mcast->listen->MCASTSources_ipv6.compare = mcast_sources_cmp_ipv6;
        return &mcast->listen->MCASTSources_ipv6;
    }
#endif
    return NULL;
}
inline static struct pico_tree *mcast_get_listen_tree(struct pico_socket *s)
{
    if( IS_SOCK_IPV4(s))
        return s->MCASTListen;

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s))
        return s->MCASTListen_ipv6;
#endif
    return NULL;
}
inline static void mcast_set_listen_tree_p_null(struct pico_socket *s)
{
    if( IS_SOCK_IPV4(s))
        s->MCASTListen = NULL;

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s))
        s->MCASTListen_ipv6 = NULL;
#endif
}
static struct pico_mcast_listen *listen_find(struct pico_socket *s, union pico_address *lnk, union pico_address *grp)
{
    struct pico_mcast_listen ltest = {
        0
    };
    ltest.mcast_link = *lnk;
    ltest.mcast_group = *grp;

    if(IS_SOCK_IPV4(s))
        return pico_tree_findKey(s->MCASTListen, &ltest);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s)) {
        ltest.proto = PICO_PROTO_IPV6;
        return pico_tree_findKey(s->MCASTListen_ipv6, &ltest);
    }
#endif
    return NULL;
}
static union pico_address *pico_mcast_get_link_address(struct pico_socket *s, union pico_link *mcast_link)
{
    if( IS_SOCK_IPV4(s))
        return (union pico_address *) &mcast_link->ipv4.address;

#ifdef PICO_SUPPORT_IPV6
    if( IS_SOCK_IPV6(s))
        return (union pico_address *) &mcast_link->ipv6.address;

#endif
    return NULL;
}
static int8_t pico_mcast_filter_excl_excl(struct pico_mcast_listen *listen)
{
    /* filter = intersection of EXCLUDEs */
    /* any record with filter mode EXCLUDE, causes the interface mode to be EXCLUDE */
    /* remove from the interface EXCLUDE filter any source not in the socket EXCLUDE filter */
    struct pico_tree_node *index = NULL, *_tmp = NULL;
    union pico_address *source = NULL;
    if(!pico_tree_empty(&MCASTFilter)) {
        pico_tree_foreach_safe(index, &MCASTFilter, _tmp)
        {
            source = pico_tree_findKey(&listen->MCASTSources, index->keyValue);
            if (!source)
                pico_tree_delete(&MCASTFilter, index->keyValue);
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if(!pico_tree_empty(&MCASTFilter_ipv6)) {
        pico_tree_foreach_safe(index, &MCASTFilter_ipv6, _tmp)
        {
            source = pico_tree_findKey(&listen->MCASTSources_ipv6, index->keyValue);
            if (!source)
                pico_tree_delete(&MCASTFilter_ipv6, index->keyValue);
        }
    }

#endif
    return PICO_IP_MULTICAST_EXCLUDE;
}

static int8_t pico_mcast_filter_excl_incl(struct pico_mcast_listen *listen)
{
    /* filter = EXCLUDE - INCLUDE */
    /* any record with filter mode EXCLUDE, causes the interface mode to be EXCLUDE */
    /* remove from the interface EXCLUDE filter any source in the socket INCLUDE filter */
    struct pico_tree_node *index = NULL, *_tmp = NULL;
    union pico_address *source = NULL;
    if(!pico_tree_empty(&listen->MCASTSources)) {
        pico_tree_foreach_safe(index, &listen->MCASTSources, _tmp)
        {
            source = pico_tree_findKey(&MCASTFilter, index->keyValue);
            if (source)
                pico_tree_delete(&MCASTFilter, source);
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if(!pico_tree_empty(&listen->MCASTSources_ipv6)) {
        pico_tree_foreach_safe(index, &listen->MCASTSources_ipv6, _tmp)
        {
            source = pico_tree_findKey(&MCASTFilter_ipv6, index->keyValue);
            if (source)
                pico_tree_delete(&MCASTFilter_ipv6, source);
        }
    }

#endif
    return PICO_IP_MULTICAST_EXCLUDE;
}

static int8_t pico_mcast_filter_incl_excl(struct pico_mcast_listen *listen)
{
    /* filter = EXCLUDE - INCLUDE */
    /* delete from the interface INCLUDE filter any source NOT in the socket EXCLUDE filter */
    struct pico_tree_node *index = NULL, *_tmp = NULL;
    union pico_address *source = NULL;
    if(!pico_tree_empty(&listen->MCASTSources)) {
        pico_tree_foreach_safe(index, &MCASTFilter, _tmp)
        {
            source = pico_tree_findKey(&listen->MCASTSources, index->keyValue);
            if (!source)
                pico_tree_delete(&MCASTFilter, index->keyValue);
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if(!pico_tree_empty(&listen->MCASTSources_ipv6)) {
        pico_tree_foreach_safe(index, &MCASTFilter_ipv6, _tmp)
        {
            source = pico_tree_findKey(&listen->MCASTSources_ipv6, index->keyValue);
            if (!source)
                pico_tree_delete(&MCASTFilter_ipv6, index->keyValue);
        }
    }

#endif
    /* any record with filter mode EXCLUDE, causes the interface mode to be EXCLUDE */

    /* add to the interface EXCLUDE filter any socket source NOT in the former interface INCLUDE filter */
    if(!pico_tree_empty(&listen->MCASTSources)) {
        pico_tree_foreach_safe(index, &listen->MCASTSources, _tmp)
        {
            source = pico_tree_insert(&MCASTFilter, index->keyValue);
            if (source) {
                if ((void *)source == (void *)&LEAF)
                    return -1;
                else
                    pico_tree_delete(&MCASTFilter, source);
            }
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if(!pico_tree_empty(&listen->MCASTSources_ipv6)) {
        pico_tree_foreach_safe(index, &listen->MCASTSources_ipv6, _tmp)
        {
            source = pico_tree_insert(&MCASTFilter_ipv6, index->keyValue);
            if (source) {
                if ((void *)source == (void *)&LEAF)
                    return -1;
                else
                    pico_tree_delete(&MCASTFilter_ipv6, source);
            }
        }
    }

#endif
    return PICO_IP_MULTICAST_EXCLUDE;
}

static int8_t pico_mcast_filter_incl_incl(struct pico_mcast_listen *listen)
{
    /* filter = summation of INCLUDEs */
    /* mode stays INCLUDE, add all sources to filter */
    struct pico_tree_node *index = NULL, *_tmp = NULL;
    union pico_address *source = NULL;

    if( !pico_tree_empty(&listen->MCASTSources)) {
        pico_tree_foreach_safe(index, &listen->MCASTSources, _tmp)
        {
            source = index->keyValue;
            if (pico_tree_insert(&MCASTFilter, source) == &LEAF)
                return -1;
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if( !pico_tree_empty(&listen->MCASTSources_ipv6)) {
        pico_tree_foreach_safe(index, &listen->MCASTSources_ipv6, _tmp)
        {
            source = index->keyValue;
            if (pico_tree_insert(&MCASTFilter_ipv6, source) == &LEAF)
                return -1;
        }
    }

#endif
    return PICO_IP_MULTICAST_INCLUDE;
}

struct pico_mcast_filter_aggregation
{
    int8_t (*call)(struct pico_mcast_listen *);
};

static const struct pico_mcast_filter_aggregation mcast_filter_aggr_call[2][2] =
{
    {
        /* EXCL + EXCL */ {.call = pico_mcast_filter_excl_excl},
        /* EXCL + INCL */ {.call = pico_mcast_filter_excl_incl}
    },

    {
        /* INCL + EXCL */ {.call = pico_mcast_filter_incl_excl},
        /* INCL + INCL */ {.call = pico_mcast_filter_incl_incl}
    }
};

static int mcast_aggr_validate(int8_t fm, struct pico_mcast_listen *l)
{
    if (!l)
        return -1;

    if (fm > 1 || fm < 0)
        return -1;

    if (l->filter_mode > 1)
        return -1;

    return 0;
}


/* MCASTFilter will be empty if no socket is listening on mcast_group on mcast_link anymore */
static int pico_socket_aggregate_mcastfilters(union pico_address *mcast_link, union pico_address *mcast_group)
{
    int8_t filter_mode = PICO_IP_MULTICAST_INCLUDE;
    struct pico_mcast_listen *listen = NULL;
    struct pico_socket *mcast_sock = NULL;
    struct pico_tree_node *index = NULL, *_tmp = NULL;

    /* cleanup old filter */
    if(!pico_tree_empty(&MCASTFilter)) {
        pico_tree_foreach_safe(index, &MCASTFilter, _tmp)
        {
            pico_tree_delete(&MCASTFilter, index->keyValue);
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if(!pico_tree_empty(&MCASTFilter_ipv6)) {
        pico_tree_foreach_safe(index, &MCASTFilter_ipv6, _tmp)
        {
            pico_tree_delete(&MCASTFilter_ipv6, index->keyValue);
        }
    }

#endif
    /* construct new filter */
    pico_tree_foreach_safe(index, &MCASTSockets, _tmp)
    {
        mcast_sock = index->keyValue;
        listen = listen_find(mcast_sock, mcast_link, mcast_group);
        if (listen) {
            if (mcast_aggr_validate(filter_mode, listen) < 0) {
                pico_err = PICO_ERR_EINVAL;
                return -1;
            }

            if (mcast_filter_aggr_call[filter_mode][listen->filter_mode].call) {
                filter_mode = mcast_filter_aggr_call[filter_mode][listen->filter_mode].call(listen);
                if (filter_mode > 1 || filter_mode < 0)
                    return -1;
            }
        }
    }
    return filter_mode;
}

static int pico_socket_mcast_filter_include(struct pico_mcast_listen *listen, union pico_address *src)
{
    struct pico_tree_node *index = NULL;
#ifdef PICO_DEBUG_MCAST
    char tmp_string[PICO_IPV6_STRING];
#endif
    if(!pico_tree_empty(&listen->MCASTSources)) {
        pico_tree_foreach(index, &listen->MCASTSources)
        {
            if (src->ip4.addr == ((union pico_address *)index->keyValue)->ip4.addr) {
                so_mcast_dbg("MCAST: IP %08X in included socket source list\n", src->ip4.addr);
                return 0;
            }
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if(!pico_tree_empty(&listen->MCASTSources_ipv6)) {
        pico_tree_foreach(index, &listen->MCASTSources_ipv6)
        {
            if (memcmp(&src->ip6, &((union pico_address *)index->keyValue)->ip6, sizeof(struct pico_ip6))) {
#ifdef PICO_DEBUG_MCAST
                pico_ipv6_to_string(tmp_string, src->ip6.addr);
                so_mcast_dbg("MCAST: IP %s in included socket source list\n", tmp_string);
#endif
                return 0;
            }
        }
    }

#endif
    /* XXX IPV6 ADDRESS */
    so_mcast_dbg("MCAST: IP %08X NOT in included socket source list\n", src->ip4.addr);
    return -1;

}

static int pico_socket_mcast_filter_exclude(struct pico_mcast_listen *listen, union pico_address *src)
{
    struct pico_tree_node *index = NULL;
#ifdef PICO_DEBUG_MCAST
    char tmp_string[PICO_IPV6_STRING];
#endif
    if(!pico_tree_empty(&listen->MCASTSources)) {
        pico_tree_foreach(index, &listen->MCASTSources)
        {
            if (src->ip4.addr == ((union pico_address *)index->keyValue)->ip4.addr) {
                so_mcast_dbg("MCAST: IP %08X in excluded socket source list\n", src->ip4.addr);
                return -1;
            }
        }
    }

#ifdef PICO_SUPPORT_IPV6
    if(!pico_tree_empty(&listen->MCASTSources_ipv6)) {
        pico_tree_foreach(index, &listen->MCASTSources_ipv6)
        {
            if (memcmp(&src->ip6, &((union pico_address *)index->keyValue)->ip6, sizeof(struct pico_ip6))) {
#ifdef PICO_DEBUG_MCAST
                pico_ipv6_to_string(tmp_string, src->ip6.addr);
                so_mcast_dbg("MCAST: IP %s in excluded socket source list\n", tmp_string);
#endif
                return 0;
            }
        }
    }

#endif
    /* XXX IPV6 ADDRESS  */
    so_mcast_dbg("MCAST: IP %08X NOT in excluded socket source list\n", src->ip4.addr);
    return 0;
}

static int pico_socket_mcast_source_filtering(struct pico_mcast_listen *listen, union pico_address *src)
{
    /* perform source filtering */
    if (listen->filter_mode == PICO_IP_MULTICAST_INCLUDE)
        return pico_socket_mcast_filter_include(listen, src);

    if (listen->filter_mode == PICO_IP_MULTICAST_EXCLUDE)
        return pico_socket_mcast_filter_exclude(listen, src);

    return -1;
}

static void *pico_socket_mcast_filter_link_get(struct pico_socket *s)
{
    /* check if no multicast enabled on socket */
    if (!s->MCASTListen)
        return NULL;

    if( IS_SOCK_IPV4(s)) {
        if (!s->local_addr.ip4.addr)
            return pico_ipv4_get_default_mcastlink();

        return pico_ipv4_link_get(&s->local_addr.ip4);
    }

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s)) {
        if (pico_ipv6_is_null_address(&s->local_addr.ip6))
            return pico_ipv6_get_default_mcastlink();

        return pico_ipv6_link_get(&s->local_addr.ip6);
    }
#endif
    return NULL;
}

int pico_socket_mcast_filter(struct pico_socket *s, union pico_address *mcast_group, union pico_address *src)
{
    void *mcast_link = NULL;
    struct pico_mcast_listen *listen = NULL;
    mcast_link = pico_socket_mcast_filter_link_get(s);
    if (!mcast_link)
        return -1;

    if(IS_SOCK_IPV4(s))
        listen = listen_find(s, (union pico_address *) &((struct pico_ipv4_link*)(mcast_link))->address, mcast_group);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s))
        listen = listen_find(s, (union pico_address *)&((struct pico_ipv6_link*)(mcast_link))->address, mcast_group);
#endif
    if (!listen)
        return -1;

    return pico_socket_mcast_source_filtering(listen, src);
}


static struct pico_ipv4_link *get_mcast_link(union pico_address *a)
{
    if (!a->ip4.addr)
        return pico_ipv4_get_default_mcastlink();

    return pico_ipv4_link_get(&a->ip4);
}
#ifdef PICO_SUPPORT_IPV6
static struct pico_ipv6_link *get_mcast_link_ipv6(union pico_address *a)
{

    if (pico_ipv6_is_null_address(&a->ip6)) {
        return pico_ipv6_get_default_mcastlink();
    }

    return pico_ipv6_link_get(&a->ip6);
}
#endif

static int pico_socket_setoption_pre_validation(struct pico_ip_mreq *mreq)
{
    if (!mreq)
        return -1;

    if (!mreq->mcast_group_addr.ip4.addr)
        return -1;

    return 0;
}
#ifdef PICO_SUPPORT_IPV6
static int pico_socket_setoption_pre_validation_ipv6(struct pico_ip_mreq *mreq)
{
    if (!mreq)
        return -1;

    if (pico_ipv6_is_null_address((struct pico_ip6*)&mreq->mcast_group_addr))
        return -1;

    return 0;
}
#endif

static struct pico_ipv4_link *pico_socket_setoption_validate_mreq(struct pico_ip_mreq *mreq)
{
    if (pico_socket_setoption_pre_validation(mreq) < 0)
        return NULL;

    if (pico_ipv4_is_unicast(mreq->mcast_group_addr.ip4.addr))
        return NULL;

    return get_mcast_link((union pico_address *)&mreq->mcast_link_addr);
}

#ifdef PICO_SUPPORT_IPV6
static struct pico_ipv6_link *pico_socket_setoption_validate_mreq_ipv6(struct pico_ip_mreq *mreq)
{
    if (pico_socket_setoption_pre_validation_ipv6(mreq) < 0)
        return NULL;

    if (pico_ipv6_is_unicast((struct pico_ip6 *)&mreq->mcast_group_addr))
        return NULL;

    return get_mcast_link_ipv6((union pico_address *)&mreq->mcast_link_addr);
}
#endif

static int pico_socket_setoption_pre_validation_s(struct pico_ip_mreq_source *mreq)
{
    if (!mreq)
        return -1;

    if (!mreq->mcast_group_addr.ip4.addr)
        return -1;

    return 0;
}
#ifdef PICO_SUPPORT_IPV6
static int pico_socket_setoption_pre_validation_s_ipv6(struct pico_ip_mreq_source *mreq)
{
    if (!mreq)
        return -1;

    if (pico_ipv6_is_null_address((struct pico_ip6 *)&mreq->mcast_group_addr))
        return -1;

    return 0;
}
#endif

static struct pico_ipv4_link *pico_socket_setoption_validate_s_mreq(struct pico_ip_mreq_source *mreq)
{
    if (pico_socket_setoption_pre_validation_s(mreq) < 0)
        return NULL;

    if (pico_ipv4_is_unicast(mreq->mcast_group_addr.ip4.addr))
        return NULL;

    if (!pico_ipv4_is_unicast(mreq->mcast_source_addr.ip4.addr))
        return NULL;

    return get_mcast_link((union pico_address *)&mreq->mcast_link_addr);
}
#ifdef PICO_SUPPORT_IPV6
static struct pico_ipv6_link *pico_socket_setoption_validate_s_mreq_ipv6(struct pico_ip_mreq_source *mreq)
{
    if (pico_socket_setoption_pre_validation_s_ipv6(mreq) < 0) {
        return NULL;
    }

    if (pico_ipv6_is_unicast((struct pico_ip6 *)&mreq->mcast_group_addr)) {
        return NULL;
    }

    if (!pico_ipv6_is_unicast((struct pico_ip6 *)&mreq->mcast_source_addr)) {
        return NULL;
    }

    return get_mcast_link_ipv6(&mreq->mcast_link_addr);
}
#endif

static struct pico_ipv4_link *setop_multicast_link_search(void *value, int bysource)
{

    struct pico_ip_mreq *mreq = NULL;
    struct pico_ipv4_link *mcast_link = NULL;
    struct pico_ip_mreq_source *mreq_src = NULL;
    if (!bysource) {
        mreq = (struct pico_ip_mreq *)value;
        mcast_link = pico_socket_setoption_validate_mreq(mreq);
        if (!mcast_link)
            return NULL;

        if (!mreq->mcast_link_addr.ip4.addr)
            mreq->mcast_link_addr.ip4.addr = mcast_link->address.addr;
    } else {
        mreq_src = (struct pico_ip_mreq_source *)value;
        if (!mreq_src) {
            return NULL;
        }

        mcast_link = pico_socket_setoption_validate_s_mreq(mreq_src);
        if (!mcast_link) {
            return NULL;
        }

        if (!mreq_src->mcast_link_addr.ip4.addr)
            mreq_src->mcast_link_addr.ip4 = mcast_link->address;
    }

    return mcast_link;
}
#ifdef PICO_SUPPORT_IPV6
static struct pico_ipv6_link *setop_multicast_link_search_ipv6(void *value, int bysource)
{
    struct pico_ip_mreq *mreq = NULL;
    struct pico_ipv6_link *mcast_link = NULL;
    struct pico_ip_mreq_source *mreq_src = NULL;
    if (!bysource) {
        mreq = (struct pico_ip_mreq *)value;
        mcast_link = pico_socket_setoption_validate_mreq_ipv6(mreq);
        if (!mcast_link) {
            return NULL;
        }

        if (pico_ipv6_is_null_address(&mreq->mcast_link_addr.ip6))
            mreq->mcast_link_addr.ip6 = mcast_link->address;
    } else {
        mreq_src = (struct pico_ip_mreq_source *)value;
        if (!mreq_src) {
            return NULL;
        }

        mcast_link = pico_socket_setoption_validate_s_mreq_ipv6(mreq_src);
        if (!mcast_link) {
            return NULL;
        }

        if (pico_ipv6_is_null_address(&mreq_src->mcast_link_addr.ip6))
            mreq_src->mcast_link_addr.ip6 = mcast_link->address;
    }

    return mcast_link;
}
#endif
static int setop_verify_listen_tree(struct pico_socket *s, int alloc)
{
    if(!alloc)
        return -1;

    if( IS_SOCK_IPV4(s)) {

        s->MCASTListen = PICO_ZALLOC(sizeof(struct pico_tree));
        if (!s->MCASTListen) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        s->MCASTListen->root = &LEAF;
        s->MCASTListen->compare = mcast_listen_cmp;
        return 0;
    }

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s)) {
        s->MCASTListen_ipv6 = PICO_ZALLOC(sizeof(struct pico_tree));
        if (!s->MCASTListen_ipv6) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        s->MCASTListen_ipv6->root = &LEAF;
        s->MCASTListen_ipv6->compare = mcast_listen_cmp_ipv6;
        return 0;

    }
#endif
    return -1;
}


static void *setopt_multicast_check(struct pico_socket *s, void *value, int alloc, int bysource)
{
    void *mcast_link = NULL;
    struct pico_tree *listen_tree = mcast_get_listen_tree(s);
    if (!value) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    if(IS_SOCK_IPV4(s))
        mcast_link = setop_multicast_link_search(value, bysource);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s))
        mcast_link = setop_multicast_link_search_ipv6(value, bysource);
#endif
    if (!mcast_link) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    if (!listen_tree) { /* No RBTree allocated yet */
        if (setop_verify_listen_tree(s, alloc) < 0) {
            return NULL;
        }
    }

    return mcast_link;
}

void pico_multicast_delete(struct pico_socket *s)
{
    int filter_mode;
    struct pico_tree_node *index = NULL, *_tmp = NULL, *index2 = NULL, *_tmp2 = NULL;
    struct pico_mcast_listen *listen = NULL;
    union pico_address *source = NULL;
    struct pico_tree *tree, *listen_tree;
    struct pico_mcast mcast;
    listen_tree = mcast_get_listen_tree(s);
    if(listen_tree) {
        pico_tree_delete(&MCASTSockets, s);
        pico_tree_foreach_safe(index, listen_tree, _tmp)
        {
            listen = index->keyValue;
            mcast.listen = listen;
            tree = mcast_get_src_tree(s, &mcast);
            if (tree) {
                pico_tree_foreach_safe(index2, tree, _tmp2)
                {
                    source = index2->keyValue;
                    pico_tree_delete(tree, source);
                    PICO_FREE(source);
                }
            }

            filter_mode = pico_socket_aggregate_mcastfilters((union pico_address *)&listen->mcast_link, (union pico_address *)&listen->mcast_group);
            if (filter_mode >= 0) {
                if(IS_SOCK_IPV4(s))
                    pico_ipv4_mcast_leave(&listen->mcast_link.ip4, &listen->mcast_group.ip4, 1, (uint8_t)filter_mode, &MCASTFilter);

#ifdef PICO_SUPPORT_IPV6
                else if(IS_SOCK_IPV6(s))
                    pico_ipv6_mcast_leave(&listen->mcast_link.ip6, &listen->mcast_group.ip6, 1, (uint8_t)filter_mode, &MCASTFilter_ipv6);
#endif
            }

            pico_tree_delete(listen_tree, listen);
            PICO_FREE(listen);
        }
        PICO_FREE(listen_tree);
        mcast_set_listen_tree_p_null(s);
    }
}


int pico_getsockopt_mcast(struct pico_socket *s, int option, void *value)
{
    switch(option) {
    case PICO_IP_MULTICAST_IF:
        pico_err = PICO_ERR_EOPNOTSUPP;
        return -1;

    case PICO_IP_MULTICAST_TTL:
        if (s->proto->proto_number == PICO_PROTO_UDP) {
            pico_udp_get_mc_ttl(s, (uint8_t *) value);
        } else {
            *(uint8_t *)value = 0;
            pico_err = PICO_ERR_EINVAL;
            return -1;
        }

        break;

    case PICO_IP_MULTICAST_LOOP:
        if (s->proto->proto_number == PICO_PROTO_UDP) {
            *(uint8_t *)value = (uint8_t)PICO_SOCKET_GETOPT(s, PICO_SOCKET_OPT_MULTICAST_LOOP);
        } else {
            *(uint8_t *)value = 0;
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

static int mcast_so_loop(struct pico_socket *s, void *value)
{
    uint8_t val = (*(uint8_t *)value);
    if (val == 0u) {
        PICO_SOCKET_SETOPT_DIS(s, PICO_SOCKET_OPT_MULTICAST_LOOP);
        return 0;
    } else if (val == 1u) {
        PICO_SOCKET_SETOPT_EN(s, PICO_SOCKET_OPT_MULTICAST_LOOP);
        return 0;
    }

    pico_err = PICO_ERR_EINVAL;
    return -1;
}
static int mcast_get_param(struct pico_mcast *mcast, struct pico_socket *s, void *value, int alloc, int by_source)
{
    if(by_source)
        mcast->mreq_s = (struct pico_ip_mreq_source *)value;
    else
        mcast->mreq = (struct pico_ip_mreq *)value;

    mcast->mcast_link = setopt_multicast_check(s, value, alloc, by_source);
    if (!mcast->mcast_link)
        return -1;

    mcast->address =  pico_mcast_get_link_address(s, mcast->mcast_link);
    if(by_source)
        mcast->listen = listen_find(s, &(mcast->mreq_s)->mcast_link_addr, &mcast->mreq_s->mcast_group_addr);
    else
        mcast->listen = listen_find(s, &(mcast->mreq)->mcast_link_addr, &mcast->mreq->mcast_group_addr);

    return 0;
}
static int mcast_so_addm(struct pico_socket *s, void *value)
{
    int filter_mode = 0;
    struct pico_mcast mcast;
    struct pico_tree *tree, *listen_tree;
    if(mcast_get_param(&mcast, s, value, 1, 0) < 0)
        return -1;

    if (mcast.listen) {
        if (mcast.listen->filter_mode != PICO_IP_MULTICAST_EXCLUDE) {
            so_mcast_dbg("pico_socket_setoption: ERROR any-source multicast (exclude) on source-specific multicast (include)\n");
        } else {
            so_mcast_dbg("pico_socket_setoption: ERROR duplicate PICO_IP_ADD_MEMBERSHIP\n");
        }

        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    mcast.listen = PICO_ZALLOC(sizeof(struct pico_mcast_listen));
    if (!mcast.listen) {
        pico_err = PICO_ERR_ENOMEM;
        return -1;
    }

    mcast.listen->filter_mode = PICO_IP_MULTICAST_EXCLUDE;
    mcast.listen->mcast_link = mcast.mreq->mcast_link_addr;
    mcast.listen->mcast_group = mcast.mreq->mcast_group_addr;
    mcast.listen->proto = s->net->proto_number;

    tree = mcast_get_src_tree(s, &mcast);
    listen_tree = mcast_get_listen_tree(s);
#ifdef PICO_SUPPORT_IPV6
    if( IS_SOCK_IPV6(s))
        mcast.listen->proto = PICO_PROTO_IPV6;

#endif
    tree->root = &LEAF;
    if (pico_tree_insert(listen_tree, mcast.listen)) {
		PICO_FREE(mcast.listen);
		return -1;
	}

    if (pico_tree_insert(&MCASTSockets, s) == &LEAF) {
        pico_tree_delete(listen_tree, mcast.listen);
        PICO_FREE(mcast.listen);
		return -1;
	}

    filter_mode = pico_socket_aggregate_mcastfilters(mcast.address, &mcast.mreq->mcast_group_addr);
    if (filter_mode < 0)
        return -1;

    so_mcast_dbg("PICO_IP_ADD_MEMBERSHIP - success, added %p\n", s);
    if(IS_SOCK_IPV4(s))
        return pico_ipv4_mcast_join((struct pico_ip4*)&mcast.mreq->mcast_link_addr, (struct pico_ip4*) &mcast.mreq->mcast_group_addr, 1, (uint8_t)filter_mode, &MCASTFilter);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s)) {
        return pico_ipv6_mcast_join((struct pico_ip6*)&mcast.mreq->mcast_link_addr, (struct pico_ip6*)&mcast.mreq->mcast_group_addr, 1, (uint8_t)filter_mode, &MCASTFilter_ipv6);
    }
#endif
    return -1;
}

static int mcast_so_dropm(struct pico_socket *s, void *value)
{
    int filter_mode = 0;
    union pico_address *source = NULL;
    struct pico_tree_node *_tmp, *index;
    struct pico_mcast mcast;
    struct pico_tree *listen_tree, *tree;
    if(mcast_get_param(&mcast, s, value, 0, 0) < 0)
        return -1;

    if (!mcast.listen) {
        so_mcast_dbg("pico_socket_setoption: ERROR PICO_IP_DROP_MEMBERSHIP before PICO_IP_ADD_MEMBERSHIP/SOURCE_MEMBERSHIP\n");
        pico_err = PICO_ERR_EADDRNOTAVAIL;
        return -1;
    }

    tree = mcast_get_src_tree(s, &mcast);
    listen_tree = mcast_get_listen_tree(s);

    pico_tree_foreach_safe(index, tree, _tmp)
    {
        source = index->keyValue;
        pico_tree_delete(tree, source);
    }
    pico_tree_delete(listen_tree, mcast.listen);
    PICO_FREE(mcast.listen);
    if (pico_tree_empty(listen_tree)) {
        PICO_FREE(listen_tree);
        mcast_set_listen_tree_p_null(s);
        pico_tree_delete(&MCASTSockets, s);
    }

    filter_mode = pico_socket_aggregate_mcastfilters(mcast.address, &mcast.mreq->mcast_group_addr);
    if (filter_mode < 0)
        return -1;

    if(IS_SOCK_IPV4(s))
        return pico_ipv4_mcast_leave((struct pico_ip4*) &mcast.mreq->mcast_link_addr, (struct pico_ip4 *) &mcast.mreq->mcast_group_addr, 1, (uint8_t)filter_mode, &MCASTFilter);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s)) { }
    return pico_ipv6_mcast_leave((struct pico_ip6*)&mcast.mreq->mcast_link_addr, (struct pico_ip6*)&mcast.mreq->mcast_group_addr, 1, (uint8_t)filter_mode, &MCASTFilter_ipv6);
#endif
    return -1;
}

static int mcast_so_unblock_src(struct pico_socket *s, void *value)
{
    int filter_mode = 0;
    union pico_address stest, *source = NULL;
    struct pico_mcast mcast;
    if(mcast_get_param(&mcast, s, value, 0, 1) < 0)
        return -1;

    memset(&stest, 0, sizeof(union pico_address));
    if (!mcast.listen) {
        so_mcast_dbg("pico_socket_setoption: ERROR PICO_IP_UNBLOCK_SOURCE before PICO_IP_ADD_MEMBERSHIP\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if (mcast.listen->filter_mode != PICO_IP_MULTICAST_EXCLUDE) {
        so_mcast_dbg("pico_socket_setoption: ERROR any-source multicast (exclude) on source-specific multicast (include)\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    stest = mcast.mreq_s->mcast_source_addr;
    if( IS_SOCK_IPV4(s))
        source = pico_tree_findKey(&mcast.listen->MCASTSources, &stest);

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s))
        source = pico_tree_findKey(&mcast.listen->MCASTSources_ipv6, &stest);
#endif
    if (!source) {
        so_mcast_dbg("pico_socket_setoption: ERROR address to unblock not in source list\n");
        pico_err = PICO_ERR_EADDRNOTAVAIL;
        return -1;
    }

    if( IS_SOCK_IPV4(s))
        pico_tree_delete(&mcast.listen->MCASTSources, source);

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s))
        pico_tree_delete(&mcast.listen->MCASTSources_ipv6, source);
#endif

    filter_mode = pico_socket_aggregate_mcastfilters(mcast.address, &mcast.mreq_s->mcast_group_addr);
    if (filter_mode < 0)
        return -1;

    if(IS_SOCK_IPV4(s))
        return pico_ipv4_mcast_leave((struct pico_ip4 *)&mcast.mreq_s->mcast_link_addr, (struct pico_ip4*) &mcast.mreq_s->mcast_group_addr, 0, (uint8_t)filter_mode, &MCASTFilter);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s)) { }
    return pico_ipv6_mcast_leave((struct pico_ip6*)&mcast.mreq_s->mcast_link_addr, (struct pico_ip6*)&mcast.mreq_s->mcast_group_addr, 0, (uint8_t)filter_mode, &MCASTFilter_ipv6);
#endif
    return -1;
}

static int mcast_so_block_src(struct pico_socket *s, void *value)
{
    int filter_mode = 0;
    union pico_address stest, *source = NULL;
    struct pico_mcast mcast;
    if(mcast_get_param(&mcast, s, value, 0, 1) < 0)
        return -1;

    memset(&stest, 0, sizeof(union pico_address));
    if (!mcast.listen) {
        dbg("pico_socket_setoption: ERROR PICO_IP_BLOCK_SOURCE before PICO_IP_ADD_MEMBERSHIP\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if (mcast.listen->filter_mode != PICO_IP_MULTICAST_EXCLUDE) {
        so_mcast_dbg("pico_socket_setoption: ERROR any-source multicast (exclude) on source-specific multicast (include)\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    stest = mcast.mreq_s->mcast_source_addr;
    if( IS_SOCK_IPV4(s))
        source = pico_tree_findKey(&mcast.listen->MCASTSources, &stest);

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s))
        source = pico_tree_findKey(&mcast.listen->MCASTSources_ipv6, &stest);
#endif
    if (source) {
        so_mcast_dbg("pico_socket_setoption: ERROR address to block already in source list\n");
        pico_err = PICO_ERR_ENOMEM;
        return -1;
    }

    source = PICO_ZALLOC(sizeof(union pico_address));
    if (!source) {
        pico_err = PICO_ERR_ENOMEM;
        return -1;
    }

    *source = mcast.mreq_s->mcast_source_addr;
    if( IS_SOCK_IPV4(s)) {
    	if (pico_tree_insert(&mcast.listen->MCASTSources, source)) {
    		PICO_FREE(source);
    		return -1;
    	}
    }

#ifdef PICO_SUPPORT_IPV6
    else if( IS_SOCK_IPV6(s))
    	if (pico_tree_insert(&mcast.listen->MCASTSources_ipv6, source)) {
			PICO_FREE(source);
			return -1;
		}
#endif

    filter_mode = pico_socket_aggregate_mcastfilters(mcast.address, &mcast.mreq_s->mcast_group_addr);
    if (filter_mode < 0)
        return -1;

    if(IS_SOCK_IPV4(s))
        return pico_ipv4_mcast_join((struct pico_ip4 *) &mcast.mreq_s->mcast_link_addr, (struct pico_ip4*)&mcast.mreq_s->mcast_group_addr, 0, (uint8_t)filter_mode, &MCASTFilter);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s)) { }
    return pico_ipv6_mcast_join((struct pico_ip6 *)&mcast.mreq_s->mcast_link_addr, (struct pico_ip6*)&mcast.mreq_s->mcast_group_addr, 0, (uint8_t)filter_mode, &MCASTFilter_ipv6);
#endif
    return -1;
}

static int mcast_so_addsrcm(struct pico_socket *s, void *value)
{
    int filter_mode = 0, reference_count = 0;
    union pico_address stest, *source = NULL;
    struct pico_mcast mcast;
    struct pico_tree *tree, *listen_tree;
    if(mcast_get_param(&mcast, s, value, 1, 1) < 0)
        return -1;

    memset(&stest, 0, sizeof(union pico_address));
    listen_tree = mcast_get_listen_tree(s);
    if (mcast.listen) {
        tree = mcast_get_src_tree(s, &mcast);
        if (mcast.listen->filter_mode != PICO_IP_MULTICAST_INCLUDE) {
            so_mcast_dbg("pico_socket_setoption: ERROR source-specific multicast (include) on any-source multicast (exclude)\n");
            pico_err = PICO_ERR_EINVAL;
            return -1;
        }

        stest = mcast.mreq_s->mcast_source_addr;
        source = pico_tree_findKey(tree, &stest);
        if (source) {
            so_mcast_dbg("pico_socket_setoption: ERROR source address to allow already in source list\n");
            pico_err = PICO_ERR_EADDRNOTAVAIL;
            return -1;
        }

        source = PICO_ZALLOC(sizeof(union pico_address));
        if (!source) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        *source = mcast.mreq_s->mcast_source_addr;
        if (pico_tree_insert(tree, source)) {
			PICO_FREE(source);
			return -1;
		}

    } else {
        mcast.listen = PICO_ZALLOC(sizeof(struct pico_mcast_listen));
        if (!mcast.listen) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        tree = mcast_get_src_tree(s, &mcast);
        mcast.listen->filter_mode = PICO_IP_MULTICAST_INCLUDE;
        mcast.listen->mcast_link = mcast.mreq_s->mcast_link_addr;
        mcast.listen->mcast_group = mcast.mreq_s->mcast_group_addr;
        tree->root = &LEAF;
        source = PICO_ZALLOC(sizeof(union pico_address));
        if (!source) {
            PICO_FREE(mcast.listen);
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

#ifdef PICO_SUPPORT_IPV6
        if( IS_SOCK_IPV6(s))
            mcast.listen->proto = PICO_PROTO_IPV6;

#endif
        *source = mcast.mreq_s->mcast_source_addr;
        if (pico_tree_insert(tree, source)) {
			PICO_FREE(mcast.listen);
			PICO_FREE(source);
			return -1;
		}

        if (pico_tree_insert(listen_tree, mcast.listen)) {
            pico_tree_delete(tree, source);
            PICO_FREE(source);
            PICO_FREE(mcast.listen);
			return -1;
		}
        reference_count = 1;
    }

    if (pico_tree_insert(&MCASTSockets, s) == &LEAF) {
		return -1;
	}

    filter_mode = pico_socket_aggregate_mcastfilters(mcast.address, &mcast.mreq_s->mcast_group_addr);
    if (filter_mode < 0)
        return -1;

    if(IS_SOCK_IPV4(s))
        return pico_ipv4_mcast_join((struct pico_ip4 *)&mcast.mreq_s->mcast_link_addr, (struct pico_ip4*)&mcast.mreq_s->mcast_group_addr,  (uint8_t)reference_count, (uint8_t)filter_mode, &MCASTFilter);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s)) { }
    return pico_ipv6_mcast_join((struct pico_ip6 *) &mcast.mreq_s->mcast_link_addr, (struct pico_ip6*)&mcast.mreq_s->mcast_group_addr, (uint8_t)reference_count, (uint8_t)filter_mode, &MCASTFilter_ipv6);
#endif
    return -1;
}

static int mcast_so_dropsrcm(struct pico_socket *s, void *value)
{
    int filter_mode = 0, reference_count = 0;
    union pico_address stest, *source = NULL;
    struct pico_mcast mcast;
    struct pico_tree *tree, *listen_tree;
    if(mcast_get_param(&mcast, s, value, 0, 1) < 0)
        return -1;

    memset(&stest, 0, sizeof(union pico_address));
    listen_tree = mcast_get_listen_tree(s);
    if (!mcast.listen) {
        so_mcast_dbg("pico_socket_setoption: ERROR PICO_IP_DROP_SOURCE_MEMBERSHIP before PICO_IP_ADD_SOURCE_MEMBERSHIP\n");
        pico_err = PICO_ERR_EADDRNOTAVAIL;
        return -1;
    }

    if (mcast.listen->filter_mode != PICO_IP_MULTICAST_INCLUDE) {
        so_mcast_dbg("pico_socket_setoption: ERROR source-specific multicast (include) on any-source multicast (exclude)\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    tree = mcast_get_src_tree(s, &mcast);
    stest = mcast.mreq_s->mcast_source_addr;
    source = pico_tree_findKey(tree, &stest);
    if (!source) {
        so_mcast_dbg("pico_socket_setoption: ERROR address to drop not in source list\n");
        pico_err = PICO_ERR_EADDRNOTAVAIL;
        return -1;
    }

    pico_tree_delete(tree, source);
    if (pico_tree_empty(tree)) { /* 1 if empty, 0 otherwise */
        reference_count = 1;
        pico_tree_delete(listen_tree, mcast.listen);
        PICO_FREE(mcast.listen);
        if (pico_tree_empty(listen_tree)) {
            PICO_FREE(listen_tree);
            mcast_set_listen_tree_p_null(s);
            pico_tree_delete(&MCASTSockets, s);
        }
    }

    filter_mode = pico_socket_aggregate_mcastfilters(mcast.address, &mcast.mreq_s->mcast_group_addr);
    if (filter_mode < 0)
        return -1;

    if(IS_SOCK_IPV4(s))
        return pico_ipv4_mcast_leave((struct pico_ip4 *) &mcast.mreq_s->mcast_link_addr, (struct pico_ip4*)&mcast.mreq_s->mcast_group_addr,  (uint8_t)reference_count, (uint8_t)filter_mode, &MCASTFilter);

#ifdef PICO_SUPPORT_IPV6
    else if(IS_SOCK_IPV6(s)) { }
    return pico_ipv6_mcast_leave((struct pico_ip6 *)&mcast.mreq_s->mcast_link_addr, (struct pico_ip6*)&mcast.mreq_s->mcast_group_addr, (uint8_t)reference_count, (uint8_t)filter_mode, &MCASTFilter_ipv6);
#endif
    return -1;
}


struct pico_setsockopt_mcast_call
{
    int option;
    int (*call)(struct pico_socket *, void *);
};

static const struct pico_setsockopt_mcast_call mcast_so_calls[1 + PICO_IP_DROP_SOURCE_MEMBERSHIP - PICO_IP_MULTICAST_IF] =
{
    { PICO_IP_MULTICAST_IF,             NULL },
    { PICO_IP_MULTICAST_TTL,            pico_udp_set_mc_ttl },
    { PICO_IP_MULTICAST_LOOP,           mcast_so_loop },
    { PICO_IP_ADD_MEMBERSHIP,           mcast_so_addm },
    { PICO_IP_DROP_MEMBERSHIP,          mcast_so_dropm },
    { PICO_IP_UNBLOCK_SOURCE,           mcast_so_unblock_src },
    { PICO_IP_BLOCK_SOURCE,             mcast_so_block_src },
    { PICO_IP_ADD_SOURCE_MEMBERSHIP,    mcast_so_addsrcm },
    { PICO_IP_DROP_SOURCE_MEMBERSHIP,   mcast_so_dropsrcm }
};


static int mcast_so_check_socket(struct pico_socket *s)
{
    pico_err = PICO_ERR_EINVAL;
    if (!s)
        return -1;

    if (!s->proto)
        return -1;

    if (s->proto->proto_number != PICO_PROTO_UDP)
        return -1;

    pico_err = PICO_ERR_NOERR;
    return 0;
}

int pico_setsockopt_mcast(struct pico_socket *s, int option, void *value)
{
    int arrayn = option - PICO_IP_MULTICAST_IF;
    if (option < PICO_IP_MULTICAST_IF || option > PICO_IP_DROP_SOURCE_MEMBERSHIP) {
        pico_err = PICO_ERR_EOPNOTSUPP;
        return -1;
    }

    if (mcast_so_check_socket(s) < 0)
        return -1;

    if (!mcast_so_calls[arrayn].call) {
        pico_err = PICO_ERR_EOPNOTSUPP;
        return -1;
    }

    return (mcast_so_calls[arrayn].call(s, value));
}

int pico_udp_set_mc_ttl(struct pico_socket *s, void  *_ttl)
{
    struct pico_socket_udp *u;
    uint8_t ttl = *(uint8_t *)_ttl;
    if(!s) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    u = (struct pico_socket_udp *) s;
    u->mc_ttl = ttl;
    return 0;
}

int pico_udp_get_mc_ttl(struct pico_socket *s, uint8_t *ttl)
{
    struct pico_socket_udp *u;
    if(!s)
        return -1;

    u = (struct pico_socket_udp *) s;
    *ttl = u->mc_ttl;
    return 0;
}
#else
int pico_udp_set_mc_ttl(struct pico_socket *s, void  *_ttl)
{
    IGNORE_PARAMETER(s);
    IGNORE_PARAMETER(_ttl);
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

int pico_udp_get_mc_ttl(struct pico_socket *s, uint8_t *ttl)
{
    IGNORE_PARAMETER(s);
    IGNORE_PARAMETER(ttl);
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

int pico_socket_mcast_filter(struct pico_socket *s, union pico_address *mcast_group, union pico_address *src)
{
    IGNORE_PARAMETER(s);
    IGNORE_PARAMETER(mcast_group);
    IGNORE_PARAMETER(src);
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

void pico_multicast_delete(struct pico_socket *s)
{
    (void)s;
}

int pico_getsockopt_mcast(struct pico_socket *s, int option, void *value)
{
    (void)s;
    (void)option;
    (void)value;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;
}

int pico_setsockopt_mcast(struct pico_socket *s, int option, void *value)
{
    (void)s;
    (void)option;
    (void)value;
    pico_err = PICO_ERR_EPROTONOSUPPORT;
    return -1;

}
#endif /* PICO_SUPPORT_MCAST */

