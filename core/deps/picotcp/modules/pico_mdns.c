/*********************************************************************
   PicoTCP. Copyright (c) 2014-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
   .
   Author: Toon Stegen, Jelle De Vleeschouwer
 *********************************************************************/
#include "pico_config.h"
#include "pico_stack.h"
#include "pico_addressing.h"
#include "pico_socket.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_tree.h"
#include "pico_mdns.h"

#ifdef PICO_SUPPORT_MDNS

/* --- Debugging --- */
#ifdef DEBUG_MDNS
#define mdns_dbg dbg
#else
#define mdns_dbg(...) do {} while(0)
#endif

#define PICO_MDNS_QUERY_TIMEOUT (10000) /* Ten seconds */
#define PICO_MDNS_RR_TTL_TICK (1000)    /* One second */

/* mDNS MTU size */
#define PICO_MDNS_MAXBUF (1400u)

/* --- Cookie flags --- */
#define PICO_MDNS_PACKET_TYPE_ANNOUNCEMENT (0x01u)
#define PICO_MDNS_PACKET_TYPE_ANSWER (0x02u)
#define PICO_MDNS_PACKET_TYPE_QUERY (0x04u)
#define PICO_MDNS_PACKET_TYPE_PROBE (0x08u)
#define PICO_MDNS_PACKET_TYPE_QUERY_ANY (0x00u)
/* --- Cookie status ---  */
#define PICO_MDNS_COOKIE_STATUS_ACTIVE (0xffu)
#define PICO_MDNS_COOKIE_STATUS_INACTIVE (0x00u)
#define PICO_MDNS_COOKIE_STATUS_CANCELLED (0x77u)
#define PICO_MDNS_COOKIE_TIMEOUT (10u)

#define PICO_MDNS_SECTION_ANSWERS (0)
#define PICO_MDNS_SECTION_AUTHORITIES (1)
#define PICO_MDNS_SETCTIO_ADDITIONALS (2)

#define PICO_MDNS_CTREE_DESTROY(rtree) \
    pico_tree_destroy((rtree), pico_mdns_cookie_delete);

/* --- Question flags --- */
#define PICO_MDNS_QUESTION_FLAG_PROBE (0x01u)
#define PICO_MDNS_QUESTION_FLAG_NO_PROBE (0x00u)
#define PICO_MDNS_QUESTION_FLAG_UNICAST_RES (0x02u)
#define PICO_MDNS_QUESTION_FLAG_MULTICAST_RES (0x00u)

#define IS_QUESTION_PROBE_FLAG_SET(x) \
    (((x) & PICO_MDNS_QUESTION_FLAG_PROBE) ? (1) : (0))
#define IS_QUESTION_UNICAST_FLAG_SET(x) \
    (((x) & PICO_MDNS_QUESTION_FLAG_UNICAST_RES) ? (1) : (0))
#define IS_QUESTION_MULTICAST_FLAG_SET(x) \
    (((x) & PICO_MDNS_QUESTION_FLAG_UNICAST_RES) ? (0) : (1))

/* Resource Record flags */
#define PICO_MDNS_RECORD_ADDITIONAL (0x08u)
#define PICO_MDNS_RECORD_SEND_UNICAST (0x10u)
#define PICO_MDNS_RECORD_CURRENTLY_PROBING (0x20u)
#define PICO_MDNS_RECORD_PROBED (0x40u)
#define PICO_MDNS_RECORD_CLAIMED (0x80u)

#define IS_SHARED_RECORD(x) \
    ((x)->flags & PICO_MDNS_RECORD_SHARED)
#define IS_UNIQUE_RECORD(x) \
    (!((x)->flags & PICO_MDNS_RECORD_SHARED))
#define IS_RECORD_PROBING(x) \
    ((x)->flags & PICO_MDNS_RECORD_CURRENTLY_PROBING)
#define IS_UNICAST_REQUESTED(x) \
    ((x)->flags & PICO_MDNS_RECORD_SEND_UNICAST)
#define IS_RECORD_VERIFIED(x) \
    ((x)->flags & PICO_MDNS_RECORD_PROBED)
#define IS_RECORD_CLAIMED(x) \
    ((x)->flags & PICO_MDNS_RECORD_CLAIMED)

/* Set and clear flags */
#define PICO_MDNS_SET_FLAG(x, b) (x = ((x) | (uint8_t)(b)))
#define PICO_MDNS_CLR_FLAG(x, b) (x = (uint8_t)(((x) & (~((uint8_t)(b))))))

/* Set and clear MSB of BE short */
#define PICO_MDNS_SET_MSB(x) (x = x | (uint16_t)(0x8000u))
#define PICO_MDNS_CLR_MSB(x) (x = x & (uint16_t)(0x7fffu))
#define PICO_MDNS_SET_MSB_BE(x) (x = x | (uint16_t)(short_be(0x8000u)))
#define PICO_MDNS_CLR_MSB_BE(x) (x = x & (uint16_t)(short_be(0x7fffu)))
#define PICO_MDNS_IS_MSB_SET(x) ((x & 0x8000u) ? 1 : 0)

/* ****************************************************************************
 *  mDNS cookie
 * *****************************************************************************/
struct pico_mdns_cookie
{
    pico_dns_qtree qtree;               /* Question tree */
    pico_mdns_rtree antree;             /* Answer tree */
    pico_mdns_rtree artree;             /* Additional record tree */
    uint8_t count;                      /* Times to send the query */
    uint8_t type;                       /* QUERY/ANNOUNCE/PROBE/ANSWER */
    uint8_t status;                     /* Active status */
    uint8_t timeout;                    /* Timeout counter */
    uint32_t send_timer;      /* For sending events */
    void (*callback)(pico_mdns_rtree *,
                     char *,
                     void *);           /* Callback */
    void *arg;                          /* Argument to pass to callback */
};

/* MARK: TREES & GLOBAL VARIABLES */

/* MDNS Communication variables */
static struct pico_socket *mdns_sock_ipv4 = NULL;
static uint16_t mdns_port = 5353u;
static struct pico_ip4 inaddr_any = {
    0
};

/* ****************************************************************************
 *  Hostname for this machine, only 1 hostname can be set.
 *  Following RFC6267: 15.4 Recommendation
 * *****************************************************************************/
static char *_hostname = NULL;

static void (*init_callback)(pico_mdns_rtree *, char *, void *) = 0;

/* ****************************************************************************
 *  Compares 2 mDNS records by name and type only
 *
 *  @param a mDNS record A
 *  @param b mDNS record B
 *  @return 0 when name and type of records are equal, returns difference when
 *			they're not.
 * ****************************************************************************/
static int
pico_mdns_record_cmp_name_type( void *a, void *b )
{
    struct pico_mdns_record *_a = NULL, *_b = NULL;

    /* Check params */
    if (!(_a = (struct pico_mdns_record *)a) ||
        !(_b = (struct pico_mdns_record *)b)) {
        pico_err = PICO_ERR_EINVAL;
        return -1; /* Don't want a wrong result when NULL-pointers are passed */
    }

    return pico_dns_record_cmp_name_type(_a->record, _b->record);
}

/* ****************************************************************************
 *  Compares 2 mDNS records by type, name AND rdata for a truly unique result
 *
 *  @param ra mDNS record A
 *  @param rb mDNS record B
 *  @return 0 when records are equal, returns difference when they're not.
 * ****************************************************************************/
int
pico_mdns_record_cmp( void *a, void *b )
{
    /* Check params */
    if (!a || !b) {
        if (!a && !b)
            return 0;

        pico_err = PICO_ERR_EINVAL;
        return -1; /* Don't want a wrong result when NULL-pointers are passed */
    }

    return pico_dns_record_cmp((void*)(((struct pico_mdns_record *)a)->record),
                               (void*)(((struct pico_mdns_record *)b)->record));
}

/* ****************************************************************************
 *  Compares 2 mDNS cookies again each other. Only compares questions since a
 *  only a cookie query will be added to the tree. And there shouldn't be 2
 *  different cookies with the same questions in the tree.
 *
 *  @param ka mDNS cookie A
 *  @param kb mDNS cookie B
 *  @return 0 when cookies are equal, returns difference when they're not.
 * ****************************************************************************/
static int
pico_mdns_cookie_cmp( void *ka, void *kb )
{
    struct pico_mdns_cookie *a = (struct pico_mdns_cookie *)ka;
    struct pico_mdns_cookie *b = (struct pico_mdns_cookie *)kb;
    struct pico_dns_question *qa = NULL, *qb = 0;
    struct pico_tree_node *na = NULL, *nb = 0;
    uint16_t ca = 0, cb = 0;
    int ret = 0;

    /* Check params */
    if (!a || !b) {
        pico_err = PICO_ERR_EINVAL;
        return -1; /* Don't want a wrong result when NULL-pointers are passed */
    }

    /* Start comparing the questions */
    for (na = pico_tree_firstNode(a->qtree.root),
         nb = pico_tree_firstNode(b->qtree.root);
         (na != &LEAF) && (nb != &LEAF);
         na = pico_tree_next(na),
         nb = pico_tree_next(nb)) {
        qa = na->keyValue;
        qb = nb->keyValue;
        if ((qa) && (qb) && (ret = pico_dns_question_cmp(qa, qb)))
            return ret;
    }
    /* Check for lengths difference */
    ca = pico_tree_count(&(a->qtree));
    cb = pico_tree_count(&(b->qtree));
    if (ca != cb)
        return (int)((int)ca - (int)cb);

    /* Cookies contain same questions, shouldn't happen */
    return 0;
}

/*
 *  Hash to identify mDNS timers with
 */
static uint32_t mdns_hash = 0;

/*
 *  mDNS specific timer creation, to identify if timers are
 *  created by mDNS module
 */
static uint32_t
pico_mdns_timer_add(pico_time expire,
                    void (*timer)(pico_time, void *),
                    void *arg)
{
    return pico_timer_add_hashed(expire, timer, arg, mdns_hash);
}

#if PICO_MDNS_ALLOW_CACHING == 1
/* Cache records from mDNS peers on the network */
static PICO_TREE_DECLARE(Cache, &pico_mdns_record_cmp);
#endif

/* My records for which I want to have the authority */
static PICO_TREE_DECLARE(MyRecords, &pico_mdns_record_cmp_name_type);

/* Cookie-tree */
static PICO_TREE_DECLARE(Cookies, &pico_mdns_cookie_cmp);

/* ****************************************************************************
 *  MARK: PROTOTYPES                                                          */
static int
pico_mdns_getrecord_generic( const char *url, uint16_t type,
                             void (*callback)(pico_mdns_rtree *,
                                              char *,
                                              void *),
                             void *arg);

static void
pico_mdns_send_probe_packet( pico_time now, void *arg );

static int
pico_mdns_reclaim( pico_mdns_rtree record_tree,
                   void (*callback)(pico_mdns_rtree *,
                                    char *,
                                    void *),
                   void *arg );
/*  EOF PROTOTYPES
 * ****************************************************************************/

/* MARK: v MDNS NAMES */

#define IS_NUM(c) (((c) >= '0') && ((c) <= '9'))
/* ****************************************************************************
 *  Tries to convert the characters after '-' to a numeric value.
 *
 *  @param opening Pointer to dash index.
 *  @param closing Pointer to end of label.
 *  @return Numeric value of suffix on success
 * ****************************************************************************/
static inline uint16_t
pico_mdns_suffix_to_uint16( char *opening, char *closing)
{
    uint16_t n = 0;
    char *i = 0;

    /* Check params */
    if (!opening || !closing ||
        ((closing - opening) > 5) ||
        ((closing - opening) < 0))
        return 0;

    for (i = (char *)(opening + 1); i < closing; i++) {
        if (!IS_NUM(*i))
            return 0;

        n = (uint16_t)((n * 10) + (*i - '0'));
    }
    return n;
}

#define iterate_first_label_name_reverse(iterator, name) \
    for ((iterator) = \
             (*name < (char)63) ? ((char *)(name + *name)) : (name); \
         (iterator) > (name); \
         (iterator)--)

/* ****************************************************************************
 *  Checks whether there is already a conflict-suffix already present in the
 *  first label of a name or not.
 *
 *  @param name   Name in DNS name notation you want to check for a suffix.
 *  @param o_i    Pointer-pointer, will get filled with location to '-'-char.
 *  @param c_i    Pointer-pointer, will get filled with end of label.
 *  @return Returns value of the suffix, when it's present, 0 when no correct
 *          suffix is present.
 * ****************************************************************************/
static uint16_t
pico_mdns_is_suffix_present( char name[],
                             char **o_i,
                             char **c_i )
{
    char *i = NULL;
    uint16_t n = 0;

    *o_i = NULL; /* Clear out indexes */
    *c_i = NULL;

    /* Find the end of label. */
    *c_i = (name + *name + 1);

    iterate_first_label_name_reverse(i, name) {
        /* Find the last dash */
        if ((*c_i) && (i < *c_i) && *i == '-') {
            *o_i = i;
            break;
        }
    }

    /* Convert the string suffix to a number */
    if (!(n = pico_mdns_suffix_to_uint16(*o_i, *c_i))) {
        *o_i = NULL;
        *c_i = NULL;
    }

    return n;
}

/* ****************************************************************************
 *  Manual string to uint16_t conversion.
 *
 *  @param n Numeric value you want to convert.
 *  @param s String to convert to
 *  @return void
 * ****************************************************************************/
static void pico_itoa( uint16_t n, char s[] )
{
    int i = 0, j = 0;
    char c = 0;

    /* Get char values */
    do {
        s[i++] = (char)(n % 10 + '0');
    } while ((n /= 10) > 0);

    /* Reverse the string */
    for (i = 0, j = (int)(pico_dns_strlen(s) - 1); i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* ****************************************************************************
 *  Generates a new name by appending a conflict resolution suffix to the first
 *  label of an FQDN.
 *
 *  @param rname Name you want to append the suffix to
 *  @return Newly created FQDN with suffix appended to first label.
 * ****************************************************************************/
static char *
pico_mdns_resolve_name_conflict( char rname[] )
{
    char *new_rname = NULL;
    char suffix[5] = {
        0
    }, nsuffix[5] = {
        0
    }, copy_offset = 0;
    char *o_i = NULL, *c_i = NULL;
    uint16_t new_len = (uint16_t)(pico_dns_strlen(rname) + 1);
    uint8_t nslen = 0, slen = 0, ns = 0;

    /* Check params */
    if (pico_dns_check_namelen(new_len)) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Check whether a conflict-suffix is already present in the name */
    if ((ns = (uint8_t)pico_mdns_is_suffix_present(rname, &o_i, &c_i))) {
        pico_itoa(ns, suffix);
        pico_itoa(++ns, nsuffix);
        slen = (uint8_t)pico_dns_strlen(suffix);
        nslen = (uint8_t)pico_dns_strlen(nsuffix);
        new_len = (uint16_t)(new_len + nslen - slen);
    } else {
        /* If no suffix is present */
        c_i = (o_i = rname + *rname) + 1;
        new_len = (uint16_t)(new_len + 2u);
        memcpy((void *)nsuffix, "-2\0", (size_t)3);
    }

    /* Provide space for the new name */
    if (!(new_rname = PICO_ZALLOC(new_len))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Assemble the new name again */
    copy_offset = (char)((o_i - rname + 1));
    memcpy(new_rname, rname, (size_t)(copy_offset));
    strcpy(new_rname + copy_offset, nsuffix);
    strcpy(new_rname + copy_offset + pico_dns_strlen(nsuffix), c_i);
    /* Set the first length-byte */
    new_rname[0] = (char)(new_rname[0] + new_len - pico_dns_strlen(rname) - 1);
    return new_rname;
}

/* MARK: ^ MDNS NAMES */
/* MARK: v MDNS QUESTIONS */

/* ****************************************************************************
 *  Creates a standalone mDNS Question with a given name and type.
 *
 *  @param url     DNS question name in URL format. Will be converted to DNS
 *				   name notation format.
 *  @param len     Will be filled with the total length of the DNS question.
 *  @param proto   Protocol for which you want to create a question. Can be
 *				   either PICO_PROTO_IPV4 or PICO_PROTO_IPV6.
 *  @param qtype   DNS type of the question to be.
 *  @param flags   With the flags you can specify if the question should be
 *				   a QU-question rather than a QM-question
 *  @param reverse When this is true, a reverse resolution name will be gene-
 *				   from the URL
 *  @return Returns pointer to the created mDNS Question on success, NULL on
 *			failure.
 * ****************************************************************************/
static struct pico_dns_question *
pico_mdns_question_create( const char *url,
                           uint16_t *len,
                           uint8_t proto,
                           uint16_t qtype,
                           uint8_t flags,
                           uint8_t reverse )
{
    uint16_t qclass = PICO_DNS_CLASS_IN;

    /* Set the MSB of the qclass field according to the mDNS format */
    if (IS_QUESTION_UNICAST_FLAG_SET(flags))
        PICO_MDNS_SET_MSB(qclass);

    /* Fill in the question suffix */
    if (IS_QUESTION_PROBE_FLAG_SET(flags))
        qtype = PICO_DNS_TYPE_ANY;

    /* Create a question as you would with plain DNS */
    return pico_dns_question_create(url, len, proto, qtype, qclass, reverse);
}

/* MARK: ^ MDNS QUESTIONS */
/* MARK: v MDNS RECORDS */

/* ****************************************************************************
 *  Just makes a hardcopy from a single mDNS resource record.
 *
 *  @param record mDNS record you want to create a copy from
 *  @return Pointer to copied mDNS resource record
 * ****************************************************************************/
static struct pico_mdns_record *
pico_mdns_record_copy( struct pico_mdns_record *record )
{
    struct pico_mdns_record *copy = NULL;

    /* Check params */
    if (!record) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Provide space for the copy */
    if (!(copy = PICO_ZALLOC(sizeof(struct pico_mdns_record)))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Copy the DNS record */
    if (!(copy->record = pico_dns_record_copy(record->record))) {
        PICO_FREE(copy);
        return NULL;
    }

    /* Copy the fields */
    copy->current_ttl = record->current_ttl;
    copy->flags = record->flags;
    copy->claim_id = record->claim_id;

    return copy;
}

/* ****************************************************************************
 *  Looks for multiple mDNS records in a tree with the same name.
 *
 *  @param tree Tree in which you want to search.
 *  @param name Name you want to search for.
 *  @return Tree with found hits, can possibly be empty
 * ****************************************************************************/
static pico_mdns_rtree
pico_mdns_rtree_find_name( pico_mdns_rtree *tree,
                           const char *name,
                           uint8_t copy )
{
    PICO_MDNS_RTREE_DECLARE(hits);
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL;

    /* Check params */
    if (!name || !tree) {
        pico_err = PICO_ERR_EINVAL;
        return hits;
    }

    /* Iterate over tree */
    pico_tree_foreach(node, tree) {
        record = node->keyValue;
        if (record && strcasecmp(record->record->rname, name) == 0) {
            if (copy)
                record = pico_mdns_record_copy(record);

            if (record)
                if (pico_tree_insert(&hits, record) != NULL)
                    /* either key was already in there, or couldn't be inserted. */
                    /* Only delete record if it was copied */
                    if (copy)
                        pico_mdns_record_delete((void **)&record);
        }
    }

    return hits;
}

/* ****************************************************************************
 *  Looks for (possibly) multiple mDNS records in a tree with the same name and
 *  type.
 *
 *  @param tree  Tree in which you want to search.
 *  @param name  Name you want to search for.
 *  @param rtype DNS type you want to search for.
 *  @return Tree with found hits, can possibly be empty.
 * ****************************************************************************/
static pico_mdns_rtree
pico_mdns_rtree_find_name_type( pico_mdns_rtree *tree,
                                char *name,
                                uint16_t rtype,
                                uint8_t copy )
{
    PICO_MDNS_RTREE_DECLARE(hits);

    struct pico_dns_record_suffix test_dns_suffix = {
        0, 1, 0, 0
    };
    struct pico_dns_record test_dns_record = {
        0
    };
    struct pico_mdns_record test = {
        0
    };
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL;
    test_dns_record.rsuffix = &test_dns_suffix;
    test.record = &test_dns_record;

    /* Check params */
    if (!name || !tree) {
        pico_err = PICO_ERR_EINVAL;
        return hits;
    }

    test.record->rname = name;
    test.record->rsuffix->rtype = short_be(rtype);

    /* Iterate over the tree */
    pico_tree_foreach(node, tree) {
        record = node->keyValue;
        if ((record) && (0 == pico_mdns_record_cmp_name_type(record, &test))) {
            if (copy)
                record = pico_mdns_record_copy(record);

            if (record){
                if (pico_tree_insert(&hits, record) != NULL) {
                    /* either key was already in there, or couldn't be inserted. */
                    /* Only delete record if it was copied */
                    if (copy)
                        pico_mdns_record_delete((void **)&record);
            	}
            }
        }
    }

    return hits;
}

/* ****************************************************************************
 *  Deletes multiple mDNS records in a tree with the same name.
 *
 *  @param tree Tree from which you want to delete records by name.
 *  @param name Name of records you want to delete from the tree.
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_mdns_rtree_del_name( pico_mdns_rtree *tree,
                          const char *name )
{
    struct pico_tree_node *node = NULL, *safe = NULL;
    struct pico_mdns_record *record = NULL;

    /* Check params */
    if (!name || !tree) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Iterate over tree */
    pico_tree_foreach_safe(node, tree, safe) {
        record = node->keyValue;
        if (record && strcasecmp(record->record->rname, name) == 0) {
            record = pico_tree_delete(tree, record);
            pico_mdns_record_delete((void **)&record);
        }
    }

    return 0;
}

/* ****************************************************************************
 *  Deletes (possibly) multiple mDNS records from a tree with same name and
 *  type.
 *
 *  @param tree Tree from which you want to delete records by name and type.
 *  @param name Name of records you want to delete.
 *  @param type DNS type of records you want to delete.
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
#if PICO_MDNS_ALLOW_CACHING == 1
static int
pico_mdns_rtree_del_name_type( pico_mdns_rtree *tree,
                               char *name,
                               uint16_t type )
{
    struct pico_tree_node *node = NULL, *next = NULL;
    struct pico_mdns_record *record = NULL;
    struct pico_dns_record_suffix test_dns_suffix = {
        0, 1, 0, 0
    };
    struct pico_dns_record test_dns_record = {
        0
    };
    struct pico_mdns_record test = {
        0
    };

    test_dns_record.rsuffix = &test_dns_suffix;
    test.record = &test_dns_record;

    /* Check params */
    if (!name || !tree) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    test.record->rname = name;
    test.record->rsuffix->rtype = short_be(type);

    /* Iterate over the tree */
    pico_tree_foreach_safe(node, tree, next) {
        record = node->keyValue;
        if ((record) && (0 == pico_mdns_record_cmp_name_type(record, &test))) {
            record = pico_tree_delete(tree, record);
            pico_mdns_record_delete((void **)&record);
        }
    }

    return 0;
}
#endif

/* ****************************************************************************
 *  Makes a hardcopy from a single mDNS resource record, but sets a new name
 *  for the copy.
 *
 *  @param record    mDNS record you want to copy.
 *  @param new_rname New name you want to set the name of the record to.
 *  @return Pointer to the copy on success, NULL-pointer on failure.
 * ****************************************************************************/
static struct pico_mdns_record *
pico_mdns_record_copy_with_new_name( struct pico_mdns_record *record,
                                     const char *new_rname )
{
    struct pico_mdns_record *copy = NULL;
    uint16_t slen = (uint16_t)(pico_dns_strlen(new_rname) + 1u);

    /* Check params */
    if (!new_rname || pico_dns_check_namelen(slen)) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Copy the record */
    if (!(copy = pico_mdns_record_copy(record)))
        return NULL;

    /* Provide a new string */
    PICO_FREE(copy->record->rname);
    if (!(copy->record->rname = PICO_ZALLOC(slen))) {
        pico_err = PICO_ERR_ENOMEM;
        pico_mdns_record_delete((void **)&copy);
        return NULL;
    }

    memcpy((void *)(copy->record->rname), new_rname, slen);
    copy->record->rname_length = slen;

    return copy;
}

/* ****************************************************************************
 *  Generates (copies) new records from conflicting ones with another name.
 *  deletes
 *
 *  @param conflict_records mDNS record tree that contains conflicting records
 *  @param conflict_name    Name for which the conflict occurred. This is to be
 *                          able to delete the conflicting records from the tree
 *  @param new_name         To generate new records from the conflicting ones,
 *                          with this new name.
 *  @return A mDNS record tree that contains all the newly generated records.
 * ****************************************************************************/
static pico_mdns_rtree
pico_mdns_generate_new_records( pico_mdns_rtree *conflict_records,
                                char *conflict_name,
                                char *new_name )
{
    PICO_MDNS_RTREE_DECLARE(new_records);
    struct pico_tree_node *node = NULL, *next = NULL;
    struct pico_mdns_record *record = NULL, *new_record = NULL;

    /* Delete all the conflicting records from MyRecords */
    if (pico_mdns_rtree_del_name(&MyRecords, conflict_name))
        return new_records;

    pico_tree_foreach_safe(node, conflict_records, next) {
        record = node->keyValue;
        if (record && strcasecmp(record->record->rname, conflict_name) == 0) {
            /* Create a new record */
            new_record = pico_mdns_record_copy_with_new_name(record, new_name);
            if (!new_record) {
                mdns_dbg("Could not create new non-conflicting record!\n");
                return new_records;
            }

            new_record->flags &= (uint8_t)(~(PICO_MDNS_RECORD_PROBED |
                                             PICO_MDNS_RECORD_SHARED |
                                             PICO_MDNS_RECORD_CURRENTLY_PROBING));

            /* Add the record to the new tree */
            if (pico_tree_insert(&new_records, new_record)) {
            	mdns_dbg("Could not add new non-conflicting record to the tree!\n");
                pico_mdns_record_delete((void **)&new_record);
				return new_records;
			}

            /* Delete the old conflicting record */
            record = pico_tree_delete(conflict_records, record);
            if (pico_mdns_record_delete((void **)&record)) {
                mdns_dbg("Could not delete old conflict record from tree!\n");
                return new_records;
            }
        }
    }

    return new_records;
}

/* ****************************************************************************
 *  When hosts observe an unsolicited record, no cookie is currently active
 *  for that, so it has to check in MyRecords if no conflict occurred for a
 *  record it has already registered. When this occurs the conflict should be
 *  resolved as with a normal cookie, just without the cookie.
 *
 *  @param record mDNS record for which the conflict occurred.
 *  @param rname  DNS name for which the conflict occurred in DNS name notation.
 *  @return 0 when the resolving is applied successfully, 1 otherwise.
 * ****************************************************************************/
static int
pico_mdns_record_resolve_conflict( struct pico_mdns_record *record,
                                   char *rname )
{
    int retval;
    PICO_MDNS_RTREE_DECLARE(new_records);
    struct pico_mdns_record *copy = NULL;
    char *new_name = NULL;

    /* Check params */
    if (!record || !rname || IS_SHARED_RECORD(record)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Step 2: Create a new name depending on current name */
    if (!(new_name = pico_mdns_resolve_name_conflict(rname)))
        return -1;

    copy = pico_mdns_record_copy_with_new_name(record, new_name);
    PICO_FREE(new_name);
    if (copy){
    	if (pico_tree_insert(&new_records, copy)) {
            mdns_dbg("MDNS: Failed to insert copy in tree\n");
            pico_mdns_record_delete((void **)&copy);
            return -1;
		}
    }

    /* Step 3: delete conflicting record from my records */
    pico_tree_delete(&MyRecords, record);
    pico_mdns_record_delete((void **)&record);

    /* Step 4: Try to reclaim the newly created records */
    retval = pico_mdns_reclaim(new_records, init_callback, NULL);
    pico_tree_destroy(&new_records, NULL);
    return retval;
}

/* ****************************************************************************
 *  Determines if my_record is lexicographically later than peer_record, returns
 *  positive value when this is the case. Check happens by comparing rtype first
 *  and then rdata as prescribed by RFC6762.
 *
 *  @param my_record   Record this hosts want to claim.
 *  @param peer_record Record the peer host wants to claim (the enemy!)
 *  @return positive value when my record is lexicographically later
 * ****************************************************************************/
static int
pico_mdns_record_am_i_lexi_later( struct pico_mdns_record *my_record,
                                  struct pico_mdns_record *peer_record)
{
    struct pico_dns_record *my = NULL, *peer = NULL;
    uint16_t mclass = 0, pclass = 0, mtype = 0, ptype = 0;
    int dif = 0;

    /* Check params */
    if (!my_record || !peer_record ||
        !(my = my_record->record) || !(peer = peer_record->record)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /*
     * First compare the record class (excluding cache-flush bit described in
     * section 10.2)
     * The numerically greater class wins
     */
    mclass = PICO_MDNS_CLR_MSB_BE(my->rsuffix->rclass);
    pclass = PICO_MDNS_CLR_MSB_BE(peer->rsuffix->rclass);
    if ((dif = (int)((int)mclass - (int)pclass))) {
        return dif;
    }

    /* Second, compare the rrtypes */
    mtype = (my->rsuffix->rtype);
    ptype = (peer->rsuffix->rtype);
    if ((dif = (int)((int)mtype - (int)ptype))) {
        return dif;
    }

    /* Third compare binary content of rdata (no regard for meaning or structure) */

    /* When using name compression, names MUST be uncompressed before comparison. See secion 8.2 in RFC6762
       This is already the case, but we won't check for it here.
       The current execution stack to get here is:
       > pico_mdns_handle_data_as_answers_generic
       >  > pico_dns_record_decompress
       >  > pico_mdns_handle_single_authority
       >  >  > pico_mdns_cookie_apply_spt
       >  >  >  > pico_mdns_record_am_i_lexi_later

       Make sure pico_dns_record_decompress is executed before pico_mdns_record_am_i_lexi_later gets called, if problems ever arise with this function.
     */

    /* Then compare rdata */
    return pico_dns_rdata_cmp(my->rdata, peer->rdata,
                              short_be(my->rsuffix->rdlength),
                              short_be(peer->rsuffix->rdlength), PICO_DNS_CASE_SENSITIVE);
}

/* ****************************************************************************
 *  Deletes a single mDNS resource record.
 *
 *  @param record Void-pointer to mDNS Resource Record. Can be used with pico_-
 *         tree-destroy.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_mdns_record_delete( void **record )
{
    struct pico_mdns_record **rr = (struct pico_mdns_record **)record;

    /* Check params */
    if (!rr || !(*rr)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Delete DNS record contained */
    if (((*rr)->record)) {
        pico_dns_record_delete((void **)&((*rr)->record));
    }

    /* Delete the record itself */
    PICO_FREE(*rr);
    *record = NULL;

    return 0;
}

/* ****************************************************************************
 *  Creates a single standalone mDNS resource record with given name, type and
 *  data.
 *
 *  @param url     DNS rrecord name in URL format. Will be converted to DNS
 *                 name notation format.
 *  @param _rdata  Memory buffer with data to insert in the resource record. If
 *				   data of record should contain a DNS name, the name in the
 *				   data buffer needs to be in URL-format.
 *  @param datalen The exact length in bytes of the _rdata-buffer. If data of
 *				   record should contain a DNS name, datalen needs to be
 *				   pico_dns_strlen(_rdata).
 *  @param len     Will be filled with the total length of the DNS rrecord.
 *  @param rtype   DNS type of the resource record to be.
 *  @param rclass  DNS class of the resource record to be.
 *  @param rttl    DNS ttl of the resource record to be.
 *  @param flags   You can specify if the mDNS record should be a shared record
 *                 rather than a unique record.
 *  @return Pointer to newly created mDNS resource record.
 * ****************************************************************************/
struct pico_mdns_record *
pico_mdns_record_create( const char *url,
                         void *_rdata,
                         uint16_t datalen,
                         uint16_t rtype,
                         uint32_t rttl,
                         uint8_t flags )
{
    struct pico_mdns_record *record = NULL;
    uint16_t len = 0;
    uint16_t cl = 0;

    /* Check params */
    if (!url || !_rdata) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    } /* Block 1, 2 paths */

    /* Provide space for the new mDNS resource record */
    if (!(record = PICO_ZALLOC(sizeof(struct pico_mdns_record)))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    } /* Block 2, 1 path */
    else {
        /* Try to create the actual DNS record */
        if (!(record->record = pico_dns_record_create(url, _rdata, datalen,
                                                    &len, rtype,
                                                    PICO_DNS_CLASS_IN, rttl))) {
            mdns_dbg("Could not create DNS record for mDNS!\n");
            PICO_FREE(record);
            return NULL;
        } /* Block 3, 2 paths */
    } /* Block 4, Block 3 = 2 paths */
    /* Block 5, (Block 4 + Block 2) * Block 1 = 6 paths */

    /* Initialise fields */
    record->current_ttl = rttl;

    /* Set the MSB of the DNS class if it's a unique record */
    if (!((flags) & PICO_MDNS_RECORD_SHARED)) {
        cl = record->record->rsuffix->rclass;
        record->record->rsuffix->rclass = PICO_MDNS_SET_MSB_BE(cl);
    } /* Block 6, 2 paths */
    /* Block 7, Block 6 * Block 5 * Block 1 = 12 paths */

    record->flags = flags;
    record->claim_id = 0;

    return record;
}

/* MARK: ^ MDNS RECORDS */
/* MARK: v MDNS COOKIES */

/* ****************************************************************************
 *  Deletes a single mDNS packet cookie and frees memory.
 *
 *  @param cookie Void-pointer to mDNS cookie, allow to be used with pico_tree-
 *         destroy.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_mdns_cookie_delete( void **ptr )
{
    struct pico_mdns_cookie **c = (struct pico_mdns_cookie **)ptr;

    /* Check params */
    if (!c || !(*c)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Destroy the vectors contained */
    PICO_DNS_QTREE_DESTROY(&((*c)->qtree));
    PICO_MDNS_RTREE_DESTROY(&((*c)->antree));
    PICO_MDNS_RTREE_DESTROY(&((*c)->artree));

    /* Delete the cookie itself */
    PICO_FREE(*c);
    *c = NULL;

    return 0;
}

/* ****************************************************************************
 *  Creates a single standalone mDNS cookie
 *
 *  @param qtree    DNS questions you want to insert in the cookie.
 *  @param antree   mDNS answers/authority records you want to add to cookie.
 *  @param artree   mDNS additional records you want to add to cookie.
 *  @param count    Times you want to send the cookie as a packet on the wire.
 *  @param type     Type of packet you want to create from the cookie.
 *  @param callback Callback when the host receives responses for the cookie.
 *  @return Pointer to newly create cookie, NULL on failure.
 * ****************************************************************************/
static struct pico_mdns_cookie *
pico_mdns_cookie_create( pico_dns_qtree qtree,
                         pico_mdns_rtree antree,
                         pico_mdns_rtree artree,
                         uint8_t count,
                         uint8_t type,
                         void (*callback)(pico_mdns_rtree *,
                                          char *,
                                          void *),
                         void *arg )
{
    struct pico_mdns_cookie *cookie = NULL; /* Packet cookie to send */

    /* Provide space for the mDNS packet cookie */
    cookie = PICO_ZALLOC(sizeof(struct pico_mdns_cookie));
    if (!cookie) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Fill in the fields */
    cookie->qtree = qtree;
    cookie->antree = antree;
    cookie->artree = artree;
    cookie->count = count;
    cookie->type = type;
    cookie->status = PICO_MDNS_COOKIE_STATUS_INACTIVE;
    cookie->timeout = PICO_MDNS_COOKIE_TIMEOUT;
    cookie->callback = callback;
    cookie->arg = arg;
    return cookie;
}

/* ****************************************************************************
 *  Apply Simultaneous Probe Tiebreakin (S.P.T.) on a probe-cookie.
 *  See RFC6762: 8.2. Simultaneous Probe Tiebreaking
 *
 *  @param cookie Cookie which contains the record which is simult. probed.
 *  @param answer Authority record received from peer which is simult. probed.
 *  @return 0 when SPT is applied correctly, -1 otherwise.
 * ****************************************************************************/
static int
pico_mdns_cookie_apply_spt( struct pico_mdns_cookie *cookie,
                            struct pico_dns_record *answer)
{
    struct pico_mdns_record *my_record = NULL;
    struct pico_mdns_record peer_record;

    /* Check params */
    if ((!cookie) || !answer || (cookie->type != PICO_MDNS_PACKET_TYPE_PROBE)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    cookie->status = PICO_MDNS_COOKIE_STATUS_INACTIVE;

    /* Implement Simultaneous Probe Tiebreaking */
    peer_record.record = answer;
    my_record = pico_tree_findKey(&MyRecords, &peer_record);
    if (!my_record || !IS_RECORD_PROBING(my_record)) {
        mdns_dbg("This is weird! My record magically removed...\n");
        return -1;
    }

    if (pico_mdns_record_am_i_lexi_later(my_record, &peer_record) > 0) {
        mdns_dbg("My record is lexicographically later! Yay!\n");
        cookie->status = PICO_MDNS_COOKIE_STATUS_ACTIVE;
    } else {
        pico_timer_cancel(cookie->send_timer);
        cookie->timeout = PICO_MDNS_COOKIE_TIMEOUT;
        cookie->count = PICO_MDNS_PROBE_COUNT;
        cookie->send_timer = pico_mdns_timer_add(1000, pico_mdns_send_probe_packet,
                                            cookie);
        if (!cookie->send_timer) {
            mdns_dbg("cookie_apply_spt: failed to start timer\n");
            return -1;
        }
        mdns_dbg("Probing postponed by one second because of S.P.T.\n");
    }

    return 0;
}

static int
pico_mdns_cookie_del_questions( struct pico_mdns_cookie *cookie,
                                char *rname )
{
    uint16_t qc = 0;

    /* Step 1: Remove question with that name from cookie */
    pico_dns_qtree_del_name(&(cookie->qtree), rname);
    cookie->antree.root = &LEAF;

    /* Check if there are no questions left, cancel events if so and delete */
    if (!(qc = pico_tree_count(&(cookie->qtree)))) {
        pico_timer_cancel(cookie->send_timer);
        cookie = pico_tree_delete(&Cookies, cookie);
        pico_mdns_cookie_delete((void **)&cookie);
    }

    return 0;
}

/* ****************************************************************************
 *  Applies conflict resolution mechanism to a cookie, when a conflict occurs
 *  for a name which is present in the cookie.
 *
 *  @param cookie Cookie on which you want to apply the conflict resolution-
 *                mechanism.
 *  @param rname  Name for which the conflict occurred. A new non-conflicting
 *                name will be generated from this string.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_mdns_cookie_resolve_conflict( struct pico_mdns_cookie *cookie,
                                   char *rname )
{
    struct pico_tree_node *node = NULL;
    struct pico_dns_question *question = NULL;
    PICO_MDNS_RTREE_DECLARE(new_records);
    PICO_MDNS_RTREE_DECLARE(antree);
    char *new_name = NULL;
    void (*callback)(pico_mdns_rtree *, char *, void *);
    void *arg = NULL;
    int retval;

    /* Check params */
    if ((!cookie) || !rname || (cookie->type != PICO_MDNS_PACKET_TYPE_PROBE)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Convert rname to url */
    mdns_dbg("CONFLICT for probe query with name '%s' occurred!\n", rname);

    /* Store some information about a cookie for later on */
    antree = cookie->antree;
    callback = cookie->callback;
    arg = cookie->arg;

    /* Find the first question in the cookie with the name for which
     * the conflict occured. When found, generate a new name.
     *
     * DNS conflict is case-insensitive. However, we want to keep the original
     * capitalisation for the new probe. */
    pico_tree_foreach(node, &(cookie->qtree)) {
        question = (struct pico_dns_question *)node->keyValue;
        if ((question) && (strcasecmp(question->qname, rname) == 0)) {
            /* Create a new name depending on current name */
            new_name = pico_mdns_resolve_name_conflict(question->qname);

            /* Step 1: Check if the new name succeeded, if not: error. */
            if (!new_name) {
                /* Delete questions from cookie even if generating a new name failed */
                pico_mdns_cookie_del_questions(cookie, rname);
                return -1;
            }

            break;
        }
    }

    /* Step 2: Remove questions with this name from the cookie */
    pico_mdns_cookie_del_questions(cookie, rname);

    /* Step 3: Create records with new name for the records with that name */
    new_records = pico_mdns_generate_new_records(&antree, rname, new_name);
    PICO_FREE(new_name);

    /* Step 4: Try to reclaim the newly created records */
    retval = pico_mdns_reclaim(new_records, callback, arg);
    pico_tree_destroy(&new_records, NULL);
    return retval;
}

/* ****************************************************************************
 *  Find a query cookie that contains a question for a specific name.
 *
 *  @param name Name of question you want to look for.
 *  @return Pointer to cookie in tree when one is found, NULL on failure.
 * ****************************************************************************/
static struct pico_mdns_cookie *
pico_mdns_ctree_find_cookie( const char *name, uint8_t type )
{
    struct pico_mdns_cookie *cookie = NULL;
    struct pico_tree_node *node = NULL;

    /* Check params */
    if (!name) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Find the cookie in the tree wherein the question is present */
    pico_tree_foreach(node, &Cookies) {
        if ((cookie = node->keyValue) &&
            pico_dns_qtree_find_name(&(cookie->qtree), name)) {
            if (type == PICO_MDNS_PACKET_TYPE_QUERY_ANY)
                return cookie;
            else if (cookie->type == type)
                return cookie;
        }
    }

    return NULL;
}

/* MARK: ^ MDNS COOKIES */
/* MARK: v MY RECORDS */

/* ****************************************************************************
 *  Adds records contained in records-tree to MyRecords. Suppresses adding of
 *  duplicates.
 *
 *  @param records Tree with records to add to 'MyRecords'.
 *  @param reclaim If the records contained in records are claimed again.
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_mdns_my_records_add( pico_mdns_rtree *records, uint8_t reclaim )
{
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL;
    static uint8_t claim_id_count = 0;

    if (!reclaim) {
        ++claim_id_count;
    }

    /* Iterate over record vector */
    pico_tree_foreach(node, records) {
        record = node->keyValue;
        if (record) {
            /* Set probed flag if record is a shared record */
            if (IS_SHARED_RECORD(record)) {
                PICO_MDNS_SET_FLAG(record->flags, PICO_MDNS_RECORD_PROBED);
            }

            /* If record is not claimed again, set new claim-ID */
            if (!reclaim) {
                record->claim_id = claim_id_count;
            }

            if (pico_tree_insert(&MyRecords, record) == &LEAF) {
                mdns_dbg("MDNS: Failed to insert record in tree\n");
                return -1;
			}
        }
    }
    return 0;
}

/* ****************************************************************************
 *  Generates a tree of all My Records for which the probe flag has already
 *  been set, and for which the CLAIMED flag has NOT been set.
 *  Copies the records from MyRecords into a new tree.
 *
 *  @return Tree with all records in MyRecords with the PROBED-flag set.
 * ****************************************************************************/
static pico_mdns_rtree
pico_mdns_my_records_find_probed( void )
{
    PICO_MDNS_RTREE_DECLARE(probed);
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL, *copy = NULL;

    /* Iterate over MyRecords */
    pico_tree_foreach(node, &MyRecords) {
        record = node->keyValue;

        /* IS_RECORD_VERIFIED() checks the PICO_MDNS_RECORD_PROBED flag */
        if (record && IS_RECORD_VERIFIED(record) && !IS_RECORD_CLAIMED(record)) {
            copy = pico_mdns_record_copy(record);
            if (copy && pico_tree_insert(&probed, copy)) {
                pico_mdns_record_delete((void **)&copy);
            }
        }
    }

    return probed;
}

/* ****************************************************************************
 *  Generates a tree of all My Records for which the PROBED-flag has not yet
 *  been set. Copies the record from MyRecords into a new tree.
 *
 *  @return Tree with all records in MyRecords with the PROBED-flag not set.
 * ****************************************************************************/
static pico_mdns_rtree
pico_mdns_my_records_find_to_probe( void )
{
    PICO_MDNS_RTREE_DECLARE(to_probe);
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL, *copy = NULL;

    pico_tree_foreach(node, &MyRecords) {
        record = node->keyValue;
        /* Check if probed flag is not set of a record */
        if (record &&
            IS_UNIQUE_RECORD(record) &&
            !IS_RECORD_VERIFIED(record) &&
            !IS_RECORD_PROBING(record)) {
            /* Set record to currently being probed status */
            record->flags |= PICO_MDNS_RECORD_CURRENTLY_PROBING;
            copy = pico_mdns_record_copy(record);
            if (copy && pico_tree_insert(&to_probe, copy))
                pico_mdns_record_delete((void **)&copy);
        }
    }
    return to_probe;
}

/* ****************************************************************************
 *  Checks whether all MyRecords with a certain claim ID are claimed or not.
 *
 *  @param claim_id    Claim ID of the records to check for already been probed.
 *  @param reg_records Tree in which all MyRecords with claim ID are inserted.
 *  @return 1 when all MyRecords with claim ID are probed, 0 when they're not.
 * ****************************************************************************/
static uint8_t
pico_mdns_my_records_claimed_id( uint8_t claim_id,
                                 pico_mdns_rtree *reg_records )
{
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL;

    /* Initialise the iterator for iterating over my records */
    pico_tree_foreach(node, &MyRecords) {
        record = node->keyValue;
        if (record && record->claim_id == claim_id) {
            if (IS_RECORD_VERIFIED(record)) {
                if (pico_tree_insert(reg_records, record) == &LEAF) {
                    mdns_dbg("MDNS: Failed to insert record in tree\n");
                    return 0;
				}
            } else {
                return 0;
            }
        }
    }

    return 1;
}

/* ****************************************************************************
 *  Marks mDNS resource records in the tree as registered. Checks MyRecords for
 *  for other records with the same claim ID. If all records with the same
 *  claim ID as the records in the tree are claimed,
 *  the callback will get called.
 *
 *  @param rtree    Tree with mDNS records that are registered.
 *  @param callback Callback will get called when all records are registered.
 *  @return Returns 0 when everything went smooth, something else otherwise.
 * ****************************************************************************/
static int
pico_mdns_my_records_claimed( pico_mdns_rtree rtree,
                              void (*callback)(pico_mdns_rtree *,
                                               char *,
                                               void *),
                              void *arg )
{
    PICO_MDNS_RTREE_DECLARE(claimed_records);
    struct pico_mdns_record *record = NULL, *myrecord = NULL;
    struct pico_tree_node *node = NULL;
    uint8_t claim_id = 0;

    /* Iterate over records and set the PROBED flag */
    pico_tree_foreach(node, &rtree) {
        if ((record = node->keyValue)) {
            if (!claim_id) {
                claim_id = record->claim_id;
            }
        }

        if ((myrecord = pico_tree_findKey(&MyRecords, record))) {
            PICO_MDNS_SET_FLAG(myrecord->flags, PICO_MDNS_RECORD_CLAIMED);
        }
    }

    /* If all_claimed is still true */
    if (pico_mdns_my_records_claimed_id(claim_id, &claimed_records)) {
        callback(&claimed_records, _hostname, arg);
    }

    pico_tree_destroy(&claimed_records, NULL);

    mdns_dbg(">>>>>> DONE - CLAIM SESSION: %d\n", claim_id);

    return 0;
}

/* ****************************************************************************
 *  Makes sure the cache flush bit is set of the records which are probed, and
 *  set the corresponding MyRecords from 'being probed' to
 *  'has been probed'-state.
 *
 *  @param records mDNS records which are probed.
 * ****************************************************************************/
static void
pico_mdns_my_records_probed( pico_mdns_rtree *records )
{
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL, *found = NULL;

    pico_tree_foreach(node, records) {
        if ((record = node->keyValue)) {
            /* Set the cache flush bit again */
            PICO_MDNS_SET_MSB_BE(record->record->rsuffix->rclass);
            if ((found = pico_tree_findKey(&MyRecords, record))) {
                if (IS_HOSTNAME_RECORD(found)) {
                    if (_hostname) {
                        PICO_FREE(_hostname);
                    }

                    _hostname = pico_dns_qname_to_url(found->record->rname);
                }

                PICO_MDNS_CLR_FLAG(found->flags, PICO_MDNS_RECORD_CURRENTLY_PROBING);
                PICO_MDNS_SET_FLAG(found->flags, PICO_MDNS_RECORD_PROBED);
            } else{
                mdns_dbg("Could not find my corresponding record...\n");
            }
        }
    }
}

/* MARK: ^ MY RECORDS */
/* MARK: v CACHE COHERENCY */
#if PICO_MDNS_ALLOW_CACHING == 1
/* ****************************************************************************
 *  Updates TTL of a cache entry.
 *
 *  @param record Record of which you want to update the TTL of
 *  @param ttl    TTL you want to update the TTL of the record to.
 *  @return void
 * ****************************************************************************/
static inline void
pico_mdns_cache_update_ttl( struct pico_mdns_record *record,
                            uint32_t ttl )
{
    if(ttl > 0) {
        /* Update the TTL's */
        record->record->rsuffix->rttl = long_be(ttl);
        record->current_ttl = ttl;
    } else {
        /* TTL 0 means delete from cache but we need to wait one second */
        record->record->rsuffix->rttl = long_be(1u);
        record->current_ttl = 1u;
    }
}

static int
pico_mdns_cache_flush_name( char *name, struct pico_dns_record_suffix *suffix )
{
    /* Check if cache flush bit is set */
    if (PICO_MDNS_IS_MSB_SET(short_be(suffix->rclass))) {
        mdns_dbg("FLUSH - Cache flush bit was set, triggered flush.\n");
        if (pico_mdns_rtree_del_name_type(&Cache, name, short_be(suffix->rtype))) {
            mdns_dbg("Could not flush records from cache!\n");
            return -1;
        }
    }

    return 0;
}

/* ****************************************************************************
 *  Adds a mDNS record to the cache.
 *
 *  @param record mDNS record to add to the Cache.
 *  @return 0 when entry successfully added, something else when it all went ho-
 *			rribly wrong...
 * ****************************************************************************/
static int
pico_mdns_cache_add( struct pico_mdns_record *record )
{
    struct pico_dns_record_suffix *suffix = NULL;
    char *name = NULL;
    uint32_t rttl = 0;

    /* Check params */
    if (!record) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }
    /* 2 paths */

    suffix = record->record->rsuffix;
    name = record->record->rname;
    rttl = long_be(suffix->rttl);

    if (pico_mdns_cache_flush_name(name, suffix)) {
        return -1;
    }
    /* 4 paths */

    /* Check if the TTL is not 0*/
    if (!rttl) {
        return -1;
    } else {
        /* Set current TTL to the original TTL before inserting */
        record->current_ttl = rttl;

        if (pico_tree_insert(&Cache, record) != NULL)
            return -1;

        mdns_dbg("RR cached. TICK TACK TICK TACK...\n");

        return 0;
    }
    /* 12 paths */
}

/* ****************************************************************************
 *  Add a copy of an mDNS resource record to the cache tree. Checks whether the
 *  entry is already present in the Cache or not.
 *
 *  @param record Record to add to the Cache-tree
 *  @return 0 on grrrreat success, something else on awkward failure.
 * ****************************************************************************/
static int
pico_mdns_cache_add_record( struct pico_mdns_record *record )
{
    struct pico_mdns_record *found = NULL, *copy = NULL;
    uint32_t rttl = 0;

    /* Check params */
    if (!record) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* See if the record is already contained in the cache */
    if ((found = pico_tree_findKey(&Cache, record))) {
        rttl = long_be(record->record->rsuffix->rttl);
        pico_mdns_cache_update_ttl(found, rttl);
    } else if ((copy = pico_mdns_record_copy(record))) {
        if (pico_mdns_cache_add(copy)) {
            pico_mdns_record_delete((void **)&copy);
            return -1;
        }
    } else
        return -1;

    return 0;
}

#if PICO_MDNS_CONTINUOUS_REFRESH == 1
/* ****************************************************************************
 *  Determine if the current TTL is at a refreshing point.
 *
 *  @param original Original TTL to calculate refreshing points
 *  @param current  Current TTL to check.
 *  @return 1 when Current TTL is at refresh point. 0 when it's not.
 * ****************************************************************************/
static int
pico_mdns_ttl_at_refresh_time( uint32_t original,
                               uint32_t current )
{
    uint32_t rnd = 0;
    rnd = pico_rand() % 3;

    if (((original - current ==
          ((original * (80 + rnd)) / 100)) ? 1 : 0) ||
        ((original - current ==
          ((original * (85 + rnd)) / 100)) ? 1 : 0) ||
        ((original - current ==
          ((original * (90 + rnd)) / 100)) ? 1 : 0) ||
        ((original - current ==
          ((original * (95 + rnd)) / 100)) ? 1 : 0))
        return 1;
    else
        return 0;
}
#endif

/* ****************************************************************************
 *  Utility function to update the TTL of cache entries and check for expired
 *  ones. When continuous refreshing is enabled the records will be reconfirmed
 *	@ 80%, 85%, 90% and 95% of their original TTL.
 * ****************************************************************************/
static void
pico_mdns_cache_check_expiries( void )
{
    struct pico_tree_node *node = NULL, *next = NULL;
    struct pico_mdns_record *record = NULL;
#if PICO_MDNS_CONTINUOUS_REFRESH == 1
    uint32_t current = 0, original = 0;
    uint16_t type 0;
    char *url = NULL;
#endif

    /* Check for expired cache records */
    pico_tree_foreach_safe(node, &Cache, next) {
        if ((record = node->keyValue)) {
            /* Update current ttl and delete when TTL is 0*/
            if ((--(record->current_ttl)) == 0) {
                record = pico_tree_delete(&Cache, record);
                pico_mdns_record_delete((void **)&record);
            }

#if PICO_MDNS_CONTINUOUS_REFRESH == 1
            /* Determine original and current ttl */
            original = long_be(record->record->rsuffix->rttl);
            current = record->current_ttl;

            /* Cache refresh at 80 or 85/90/95% of TTL + 2% rnd */
            if (pico_mdns_ttl_at_refresh_time(original, current)) {
                url = pico_dns_qname_to_url(record->record->rname);
                type = short_be(record->record->rsuffix->rtype)
                       pico_mdns_getrecord_generic(url, type, NULL, NULL);
                PICO_FREE(url);
            }

#endif
        }
    }
}
#endif /* PICO_MDNS_ALLOW_CACHING */

/* ****************************************************************************
 *  Utility function to update the TTL of cookies and check for expired
 *  ones. Deletes the expired ones as well.
 * ****************************************************************************/
static void
pico_mdns_cookies_check_timeouts( void )
{
    struct pico_tree_node *node = NULL, *next = NULL;
    struct pico_mdns_cookie *cookie = NULL;

    pico_tree_foreach_safe(node, &Cookies, next) {
        if ((cookie = node->keyValue) && --(cookie->timeout) == 0) {
            /* Call callback to allow error checking */
            if (cookie->callback) {
                cookie->callback(NULL, NULL, cookie->arg);
            }

            /* Delete cookie */
            cookie = pico_tree_delete(&Cookies, cookie);
            pico_mdns_cookie_delete((void **)&cookie);

            /* If the request was for a reconfirmation of a record,
                flush the corresponding record after the timeout */
        }
    }
}

/* ****************************************************************************
 *  Global mDNS module tick-function, central point where all the timing is
 *  handled.
 *
 *  @param now  Ignore
 *  @param _arg Ignore
 * ****************************************************************************/
static void
pico_mdns_tick( pico_time now, void *_arg )
{
    IGNORE_PARAMETER(now);
    IGNORE_PARAMETER(_arg);

#if PICO_MDNS_ALLOW_CACHING == 1
    /* Update the cache */
    pico_mdns_cache_check_expiries();
#endif

    /* Update the cookies */
    pico_mdns_cookies_check_timeouts();

    /* Schedule new tick */
    if (!pico_mdns_timer_add(PICO_MDNS_RR_TTL_TICK, pico_mdns_tick, NULL)) {
        mdns_dbg("MDNS: Failed to start tick timer\n");
        /* TODO Not ticking anymore, what to do? */
    }
}

/* MARK: v MDNS PACKET UTILITIES */

/* ****************************************************************************
 *  Sends a Multicast packet on the wire to the mDNS destination port.
 *
 *  @param packet Packet buffer in memory
 *  @param len    Size of the packet in bytes
 *  @return 0 When the packet is passed successfully on to the lower layers of
 *			picoTCP. Doesn't mean the packet is successfully sent on the wire.
 * ****************************************************************************/
static int
pico_mdns_send_packet( pico_dns_packet *packet, uint16_t len )
{
    /* TODO: why only ipv4 support? */
    struct pico_ip4 dst4;

    /* Set the destination address to the mDNS multicast-address */
    pico_string_to_ipv4(PICO_MDNS_DEST_ADDR4, &dst4.addr);

    /* Send packet to IPv4 socket */
    return pico_socket_sendto(mdns_sock_ipv4, packet, (int)len, &dst4,
                              short_be(mdns_port));
}

/* ****************************************************************************
 *  Sends a Unicast packet on the wire to the mDNS destination port of specific
 *  peer in the network
 *
 *  @param packet Packet buffer in memory
 *  @param len    Size of the packet in bytes
 *  @param peer   Peer in the network you want to send the packet to.
 *  @return 0 When the packet is passed successfully on to the lower layers of
 *			picoTCP. Doesn't mean the packet is successfully send on the wire.
 * ****************************************************************************/
static int
pico_mdns_send_packet_unicast( pico_dns_packet *packet,
                               uint16_t len,
                               struct pico_ip4 peer )
{
    /* Send packet to IPv4 socket */
    return pico_socket_sendto(mdns_sock_ipv4, packet, (int)len, &peer,
                              short_be(mdns_port));
}


/* ****************************************************************************
 *  Send DNS records as answers to a peer via unicast
 *
 *  @param unicast_tree Tree with DNS records to send as answers.
 *  @param peer         Peer IPv4-address
 *  @return 0 when the packet is properly send, something else otherwise.
 * ****************************************************************************/
static int
pico_mdns_unicast_reply( pico_dns_rtree *unicast_tree,
                         pico_dns_rtree *artree,
                         struct pico_ip4 peer )
{
    union pico_address *local_addr = NULL;
    pico_dns_packet *packet = NULL;
    uint16_t len = 0;

    if (pico_tree_count(unicast_tree) > 0) {
        /* Create response DNS packet */
        packet = pico_dns_answer_create(unicast_tree, NULL, artree, &len);
        if (!packet || !len) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        packet->id = 0;

        /* Check if source address is on the local link */
        local_addr = (union pico_address *) pico_ipv4_source_find(&peer);
        if (!local_addr) {
            mdns_dbg("Peer not on same link!\n");
            /* Forced response via multicast */

            /* RFC6762: 18.6: In both multicast query and response messages,
               the RD bit SHOULD be zero on transmission. In
               pico_dns_fill_packet_header, the RD bit is set to
               PICO_DNS_RD_IS_DESIRED, which is defined to be 1 */
            packet->rd = PICO_DNS_RD_NO_DESIRE;


            if (pico_mdns_send_packet(packet, len) != (int)len) {
                mdns_dbg("Could not send multicast response!\n");
                return -1;
            }
        } else {
            /* Send the packet via unicast */
            if (pico_mdns_send_packet_unicast(packet, len, peer) != (int)len) {
                mdns_dbg("Could not send unicast response!\n");
                return -1;
            }

            mdns_dbg("Unicast response sent successfully!\n");
        }

        PICO_FREE(packet);
    }

    return 0;
}

/* ****************************************************************************
 *  Send DNS records as answers to mDNS peers via multicast
 *
 *  @param multicast_tree Tree with DNS records to send as answers.
 *  @return 0 when the packet is properly send, something else otherwise.
 * ****************************************************************************/
static int
pico_mdns_multicast_reply( pico_dns_rtree *multicast_tree,
                           pico_dns_rtree *artree )
{
    pico_dns_packet *packet = NULL;
    uint16_t len = 0;

    /* If there are any multicast records */
    if (pico_tree_count(multicast_tree) > 0) {
        /* Create response DNS packet */
        packet = pico_dns_answer_create(multicast_tree, NULL, artree, &len);
        if (!packet || len == 0) {
            pico_err = PICO_ERR_ENOMEM;
            return -1;
        }

        packet->id = 0;

        /* RFC6762: 18.6: In both multicast query and response messages,
           the RD bit SHOULD be zero on transmission.
           In pico_dns_fill_packet_header, the RD bit is set to
           PICO_DNS_RD_IS_DESIRED, which is defined to be 1 */
        packet->rd = PICO_DNS_RD_NO_DESIRE;

        /* Send the packet via multicast */
        if (pico_mdns_send_packet(packet, len) != (int)len) {
            mdns_dbg("Could not send multicast response!\n");
            return -1;
        }

        mdns_dbg("Multicast response sent successfully!\n");

        PICO_FREE(packet);
    }

    return 0;
}

/* MARK: ^ MDNS PACKET UTILITIES */
/* MARK: ASYNCHRONOUS MDNS RECEPTION */

/* ****************************************************************************
 *  Merges 2 pico_trees with each other.
 *
 *  @param dest Destination tree to merge the other tree in.
 *  @param src  Source tree to get the node from to insert into the dest-tree.
 *  @return Returns 0 when properly merged, or not..
 * ****************************************************************************/
static int
pico_tree_merge( struct pico_tree *dest, struct pico_tree *src )
{
    struct pico_tree_node *node = NULL;

    /* Check params */
    if (!dest || !src) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Insert source nodes */
    pico_tree_foreach(node, src) {
        if (node->keyValue) {
            if (pico_tree_insert(dest, node->keyValue) == &LEAF) {
                mdns_dbg("MDNS: Failed to insert record in tree\n");
                return -1;
			}
        }
    }

    return 0;
}

/* ****************************************************************************
 *  Populates an mDNS record tree with answers from MyRecords depending on name
 *  , qtype and qclass.
 *
 *  @param name   Name of records to look for in MyRecords
 *  @param qtype  Type of records to look for in MyRecords
 *  @param qclass Whether the answer should be sent via unicast or not.
 *  @return mDNS record tree with possible answers from MyRecords
 * ****************************************************************************/
static pico_mdns_rtree
pico_mdns_populate_antree( char *name, uint16_t qtype, uint16_t qclass )
{
    PICO_MDNS_RTREE_DECLARE(antree);
    struct pico_tree_node *node = NULL, *next;
    struct pico_mdns_record *record = NULL;

    /* Create an answer record vector */
    if (PICO_DNS_TYPE_ANY == qtype)
        antree = pico_mdns_rtree_find_name(&MyRecords, name, 1);
    else
        antree = pico_mdns_rtree_find_name_type(&MyRecords, name, qtype, 1);

    /* Remove answers which aren't successfully registered yet */
    pico_tree_foreach_safe(node, &antree, next) {
        if ((record = node->keyValue) && !IS_RECORD_VERIFIED(record)) {
            pico_tree_delete(&antree, record);
        }
    }

    /* Check if question is a QU-question */
    if (PICO_MDNS_IS_MSB_SET(qclass)) {
        /* Set all the flags of the answer accordingly */
        pico_tree_foreach(node, &antree) {
            if ((record = node->keyValue))
                PICO_MDNS_SET_FLAG(record->flags,
                                   PICO_MDNS_RECORD_SEND_UNICAST);
        }
    }

    return antree;
}

/* ****************************************************************************
 *  Handles a single received question.
 *
 *  @param question DNS question to parse and handle.
 *  @param packet   Received packet in which the DNS question was present.
 *  @return mDNS record tree with possible answer to the question. Can possibly
 *			be empty.
 * ****************************************************************************/
static pico_mdns_rtree
pico_mdns_handle_single_question( struct pico_dns_question *question,
                                  pico_dns_packet *packet )
{
    struct pico_mdns_cookie *cookie = NULL;
    PICO_MDNS_RTREE_DECLARE(antree);
    char *qname_original = NULL;
    uint16_t qtype = 0, qclass = 0;

    /* Check params */
    if (!question || !packet) {
        pico_err = PICO_ERR_EINVAL;
        return antree;
    }

    /* Decompress single DNS question */
    qname_original = pico_dns_question_decompress(question, packet);
    mdns_dbg("Question RCVD for '%s'\n", question->qname);

    /* Find currently active query cookie */
    if ((cookie = pico_mdns_ctree_find_cookie(question->qname,
                                              PICO_MDNS_PACKET_TYPE_QUERY))) {
        mdns_dbg("Query cookie found for question, suppress duplicate.\n");
        cookie->status = PICO_MDNS_COOKIE_STATUS_CANCELLED;
    } else {
        qtype = short_be(question->qsuffix->qtype);
        qclass = short_be(question->qsuffix->qclass);
        antree = pico_mdns_populate_antree(question->qname, qtype, qclass);
    }

    PICO_FREE(question->qname);
    question->qname = qname_original;
    return antree;
}

/* ****************************************************************************
 *  When a query-cookie is found for a RCVD answer, the cookie should be
 *  handled accordingly. This function does that.
 *
 *  @param cookie Cookie that contains the question for the RCVD answer.
 *  @param answer RCVD answer to handle cookie with
 *  @return Returns 0 when handling went OK, something else when it didn't.
 * ****************************************************************************/
static int
pico_mdns_handle_cookie_with_answer( struct pico_mdns_cookie *cookie,
                                     struct pico_mdns_record *answer )
{
    PICO_MDNS_RTREE_DECLARE(antree);
    uint8_t type = 0, status = 0;

    /* Check params */
    if (!cookie || !answer) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    type = cookie->type;
    status = cookie->status;
    if (PICO_MDNS_COOKIE_STATUS_ACTIVE == status) {
        if (PICO_MDNS_PACKET_TYPE_PROBE == type) {
            /* Conflict occurred, resolve it! */
            pico_mdns_cookie_resolve_conflict(cookie, answer->record->rname);
        } else if (PICO_MDNS_PACKET_TYPE_QUERY == type) {
            if (cookie->callback) {
                /* RCVD Answer on query, callback with answer. Callback is
                 * responsible for aggregating all the received answers. */
                if (pico_tree_insert(&antree, answer) == &LEAF) {
                    mdns_dbg("MDNS: Failed to insert answer in tree\n");
                    return -1;
				}
                cookie->callback(&antree, NULL, cookie->arg);
            }
        } else { /* Don't handle answer cookies with answer */
        }
    }

    return 0;
}

/* ****************************************************************************
 *  Handles a single received answer record.
 *
 *  @param answer Answer mDNS record.
 *  @return 0 when answer is properly handled, something else when it's not.
 * ****************************************************************************/
static int
pico_mdns_handle_single_answer( struct pico_mdns_record *answer )
{
    struct pico_mdns_cookie *found = NULL;
    struct pico_mdns_record *record = NULL;

    mdns_dbg("Answer RCVD for '%s'\n", answer->record->rname);

    /* Find currently active query cookie */
    found = pico_mdns_ctree_find_cookie(answer->record->rname,
                                        PICO_MDNS_PACKET_TYPE_QUERY_ANY);
    if (found && pico_mdns_handle_cookie_with_answer(found, answer)) {
        mdns_dbg("Could not handle found cookie correctly!\n");
        return -1;
    } else {
        mdns_dbg("RCVD an unsolicited record!\n");
        if ((record = pico_tree_findKey(&MyRecords, answer)) &&
            !IS_RECORD_PROBING(record))
            return pico_mdns_record_resolve_conflict(record,
                                                     answer->record->rname);
    }

    return 0;
}

/* ****************************************************************************
 *  Handles a single received authority record.
 *
 *  @param answer Authority mDNS record.
 *  @return 0 when authority is properly handled. -1 when it's not.
 * ****************************************************************************/
static int
pico_mdns_handle_single_authority( struct pico_mdns_record *answer )
{
    struct pico_mdns_cookie *found = NULL;
    char *name = NULL;

    name = answer->record->rname;
    mdns_dbg("Authority RCVD for '%s'\n", name);

    /* Find currently active probe cookie */
    if ((found = pico_mdns_ctree_find_cookie(name, PICO_MDNS_PACKET_TYPE_PROBE))
        && PICO_MDNS_COOKIE_STATUS_ACTIVE == found->status) {
        mdns_dbg("Simultaneous Probing occurred, went tiebreaking...\n");
        if (pico_mdns_cookie_apply_spt(found, answer->record) < 0) {
            mdns_dbg("Could not apply S.P.T. to cookie!\n");
            return -1;
        }
    }

    return 0;
}

/* ****************************************************************************
 *  Handles a single received additional [Temporarily unused]
 *
 *  @param answer Additional mDNS record.
 *  @return 0
 * ****************************************************************************/
static int
pico_mdns_handle_single_additional( struct pico_mdns_record *answer )
{
    /* Don't need this for now ... */
    IGNORE_PARAMETER(answer);
    return 0;
}

/* ****************************************************************************
 *  Handles a flat chunk of memory as if it were all questions in it.
 *  Generates a tree with responses if there are any questions for records for
 *  which host has the authority to answer.
 *
 *  @param ptr     Pointer-Pointer to location of question section of packet.
 *                 Will point to right after the question section on return.
 *  @param qdcount Amount of questions contained in the packet
 *  @param packet  DNS packet where the questions are present.
 *  @return Tree with possible responses on the questions.
 * ****************************************************************************/
static pico_mdns_rtree
pico_mdns_handle_data_as_questions ( uint8_t **ptr,
                                     uint16_t qdcount,
                                     pico_dns_packet *packet )
{
    PICO_MDNS_RTREE_DECLARE(antree);
    PICO_MDNS_RTREE_DECLARE(rtree);
    struct pico_dns_question question;
    uint16_t i = 0;

    /* Check params */
    if ((!ptr) || !packet || !(*ptr)) {
        pico_err = PICO_ERR_EINVAL;
        return antree;
    }

    for (i = 0; i < qdcount; i++) {
        /* Set qname of the question to the correct location */
        question.qname = (char *)(*ptr);

        /* Set qsuffix of the question to the correct location */
        question.qsuffix = (struct pico_dns_question_suffix *)
                           (question.qname + pico_dns_namelen_comp(question.qname) + 1);

        /* Handle a single question and merge the returned tree */
        rtree = pico_mdns_handle_single_question(&question, packet);
        pico_tree_merge(&antree, &rtree);
        pico_tree_destroy(&rtree, NULL);

        /* Move to next question */
        *ptr = (uint8_t *)question.qsuffix +
               sizeof(struct pico_dns_question_suffix);
    }
    if (pico_tree_count(&antree) == 0) {
        mdns_dbg("No 'MyRecords' found that corresponds with this query.\n");
    }

    return antree;
}

static int
pico_mdns_handle_data_as_answers_generic( uint8_t **ptr,
                                          uint16_t count,
                                          pico_dns_packet *packet,
                                          uint8_t type )
{
    struct pico_mdns_record mdns_answer = {
        .record = NULL, .current_ttl = 0,
        .flags = 0, .claim_id = 0
    };
    struct pico_dns_record answer;
    char *orname = NULL;
    uint16_t i = 0;

    /* Check params */
    if ((!ptr) || !packet || !(*ptr)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* TODO: When receiving multiple authoritative answers, */
    /* they should be sorted in lexicographical order */
    /* (just like in pico_mdns_record_am_i_lexi_later) */

    for (i = 0; i < count; i++) {
        /* Set rname of the record to the correct location */
        answer.rname = (char *)(*ptr);

        /* Set rsuffix of the record to the correct location */
        answer.rsuffix = (struct pico_dns_record_suffix *)
                         (answer.rname +
                          pico_dns_namelen_comp(answer.rname) + 1u);

        /* Set rdata of the record to the correct location */
        answer.rdata = (uint8_t *) answer.rsuffix +
                       sizeof(struct pico_dns_record_suffix);

        /* Make an mDNS record from the DNS answer */
        orname = pico_dns_record_decompress(&answer, packet);
        mdns_answer.record = &answer;
        mdns_answer.record->rname_length = (uint16_t)(pico_dns_strlen(answer.rname) + 1u);

        /* Handle a single aswer */
        switch (type) {
        case 1:
            pico_mdns_handle_single_authority(&mdns_answer);
            break;
        case 2:
            pico_mdns_handle_single_additional(&mdns_answer);
            break;
        default:
            pico_mdns_handle_single_answer(&mdns_answer);
#if PICO_MDNS_ALLOW_CACHING == 1
            pico_mdns_cache_add_record(&mdns_answer);
#endif
            break;
        }

        /* Free decompressed name and mDNS record */
        PICO_FREE(mdns_answer.record->rname);
        answer.rname = orname;

        /* Move to next record */
        *ptr = (uint8_t *) answer.rdata + short_be(answer.rsuffix->rdlength);
    }
    return 0;
}

/* ****************************************************************************
 *  Splits an mDNS record tree into two DNS record tree, one to send via
 *  unicast, one to send via multicast.
 *
 *  @param answers        mDNS record tree to split up
 *  @param unicast_tree   DNS record tree with unicast answers.
 *  @param multicast_tree DNS record tee with multicast answers.
 *  @return 0 when the tree is properly split up.
 * ****************************************************************************/
static int
pico_mdns_sort_unicast_multicast( pico_mdns_rtree *answers,
                                  pico_dns_rtree *unicast_tree,
                                  pico_dns_rtree *multicast_tree )
{
    struct pico_mdns_record *record = NULL;
    struct pico_tree_node *node = NULL;

    /* Check params */
    if (!answers || !unicast_tree || !multicast_tree) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    pico_tree_foreach(node, answers) {
        record = node->keyValue;
        if ((record = node->keyValue)) {
            if (IS_UNICAST_REQUESTED(record)) {
                if (record->record){
                	if (pico_tree_insert(unicast_tree, record->record) == &LEAF) {
                        mdns_dbg("MDNS: Failed to instert unicast record in tree\n");
                        return -1;
					}
                }
            } else {
                if (record->record){
                	if (pico_tree_insert(multicast_tree, record->record) == &LEAF) {
                        mdns_dbg("MDNS: Failed to instert multicast record in tree\n");
                        return -1;
					}
                }
            }
        }
    }

    return 0;
}

static uint16_t
pico_mdns_nsec_highest_type( pico_mdns_rtree *rtree )
{
    struct pico_tree_node *node = NULL, *next = NULL;
    struct pico_mdns_record *record = NULL;
    uint16_t highest_type = 0, type = 0;

    pico_tree_foreach_safe(node, rtree, next) {
        if ((record = node->keyValue)) {
            if (IS_SHARED_RECORD(record))
                pico_tree_delete(rtree, record);

            type = short_be(record->record->rsuffix->rtype);
            highest_type = (type > highest_type) ? (type) : (highest_type);
        }
    }

    return highest_type;
}

static void
pico_mdns_nsec_gen_bitmap( uint8_t *ptr, pico_mdns_rtree *rtree )
{
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL;
    uint16_t type = 0;

    pico_tree_foreach(node, rtree) {
        if ((record = node->keyValue)) {
            type = short_be(record->record->rsuffix->rtype);
            *(ptr + 1 + (type / 8)) = (uint8_t)(0x80 >> (type % 8));
        }
    }
}

/* ****************************************************************************
 *  Generates an NSEC record for a specific name. Looks in MyRecords for unique
 *  records with given name and generates the NSEC bitmap from them.
 *
 *  @param name Name of the records you want to generate a bitmap for.
 *  @return Pointer to newly created NSEC record on success, NULL on failure.
 * ****************************************************************************/
static struct pico_mdns_record *
pico_mdns_gen_nsec_record( char *name )
{
    PICO_MDNS_RTREE_DECLARE(rtree);
    struct pico_mdns_record *record = NULL;
    uint16_t highest_type = 0, rdlen = 0;
    uint8_t bitmap_len = 0, *rdata = NULL, *ptr = NULL;
    char *url = NULL;

    if (!name) { /* Check params */
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Determine the highest type of my unique records with this name */
    rtree = pico_mdns_rtree_find_name(&MyRecords, name, 0);
    highest_type = pico_mdns_nsec_highest_type(&rtree);

    /* Determine the bimap_len */
    bitmap_len = (uint8_t)(highest_type / 8);
    bitmap_len = (uint8_t)(bitmap_len + ((highest_type % 8) ? (1) : (0)));

    /* Provide rdata */
    rdlen = (uint16_t)(pico_dns_strlen(name) + 3u + bitmap_len);
    if (!(rdata = PICO_ZALLOC((size_t)rdlen))) {
        pico_err = PICO_ERR_ENOMEM;
        pico_tree_destroy(&rtree, NULL);
        return NULL;
    }

    /* Set the next domain name */
    strcpy((char *)rdata, name);
    /* Set the bitmap length */
    *(ptr = (uint8_t *)(rdata + pico_dns_strlen(name) + 2)) = bitmap_len;
    /* Generate the bitmap */
    pico_mdns_nsec_gen_bitmap(ptr, &rtree);
    pico_tree_destroy(&rtree, NULL);

    /* Generate the actual mDNS NSEC record */
    if (!(url = pico_dns_qname_to_url(name))) {
        PICO_FREE(rdata);
        return NULL;
    }

    record = pico_mdns_record_create(url, (void *)rdata, rdlen,
                                     PICO_DNS_TYPE_NSEC,
                                     PICO_MDNS_SERVICE_TTL,
                                     PICO_MDNS_RECORD_UNIQUE);
    PICO_FREE(rdata);
    PICO_FREE(url);
    return record;
}

/* ****************************************************************************
 *  Checks in additionals if there is an NSEC record already present with given
 *  name. If there's not, a new NSEC records will be generated and added to the
 *  additional tree.
 *
 *  @param artree mDNS record-tree containing additional records.
 *  @param name   Name to check for.
 *  @return 0 when NSEC is present in additional, whether it was already present
 *			or a new one is generated doesn't matter.
 * ****************************************************************************/
static int
pico_mdns_additionals_add_nsec( pico_mdns_rtree *artree,
                                char *name )
{
    struct pico_mdns_record *record = NULL, *nsec = NULL;
    struct pico_tree_node *node = NULL;
    uint16_t type = 0;

    /* Check params */
    if (!artree || !name) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Check if there is a NSEC already for this name */
    pico_tree_foreach(node, artree) {
        if (node != &LEAF && (record = node->keyValue)) {
            type = short_be(record->record->rsuffix->rtype);
            if ((PICO_DNS_TYPE_NSEC == type) && 0 == strcasecmp(record->record->rname, name)) {
                return 0;
            }
        }
    }

    /* If there is none present generate one for given name */
    if ((nsec = pico_mdns_gen_nsec_record(name))) {
        if (pico_tree_insert(artree, nsec)) {
            pico_mdns_record_delete((void **)nsec);
            return -1;
        }
    }

    return 0;
}

/* ****************************************************************************
 *  Adds hostname records to the additional records
 *
 *  @param artree mDNS record-tree containing additional records.
 *  @return 0 when hostname records are added successfully to additionals. Rets
 *			something else on failure.
 * ****************************************************************************/
static int
pico_mdns_additionals_add_host( pico_mdns_rtree *artree )
{
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL, *copy = NULL;

    pico_tree_foreach(node, &MyRecords) {
        record = node->keyValue;
        if (record) {
            if (IS_HOSTNAME_RECORD(record) && IS_RECORD_VERIFIED(record)) {
                copy = pico_mdns_record_copy(record);
                if (copy && pico_tree_insert(artree, copy))
                    pico_mdns_record_delete((void **)&copy);
            }
        }
    }

    return 0;
} /* Satic path count: 4 */

static void
pico_rtree_add_copy( pico_mdns_rtree *tree, struct pico_mdns_record *record )
{
    struct pico_mdns_record *copy = NULL;

    if (!tree || !record) {
        pico_err = PICO_ERR_EINVAL;
        return;
    }

    if ((copy = pico_mdns_record_copy(record))) {
        if (pico_tree_insert(tree, copy))
            pico_mdns_record_delete((void **)&copy);
    }
}

/* ****************************************************************************
 *  When a service is found, additional records should be generated and
 *  added to either the answer section or the additional sections.
 *  This happens here
 *
 *  @param antree     mDNS record tree with answers to send
 *  @param artree     mDNS record tree with additionals to send
 *  @param srv_record Found SRV record in the answers
 *  @return 0 When additional records are properly generated
 * ****************************************************************************/
static int
pico_mdns_gather_service_meta( pico_mdns_rtree *antree,
                               pico_mdns_rtree *artree,
                               struct pico_mdns_record *srv_record )
{
    struct pico_mdns_record *ptr_record = NULL, *meta_record = NULL;
    char *sin = NULL, *service = NULL;
    uint32_t ttl = 0;

    /* Generate proper service instance name and service */
    sin = pico_dns_qname_to_url(srv_record->record->rname); // May be leaking

    if (!antree || !artree || !sin) {
        pico_err = PICO_ERR_EINVAL;
        PICO_FREE(sin);
        return -1;
    } else {
        /* Add hostname records */
        pico_mdns_additionals_add_host(artree);

        service = sin + pico_dns_first_label_length(sin) + 1u;
        ttl = long_be(srv_record->record->rsuffix->rttl);

        /* Generate PTR records */
        ptr_record = pico_mdns_record_create(service, (void *)sin,
                                            (uint16_t)strlen(sin),
                                            PICO_DNS_TYPE_PTR,
                                            ttl, PICO_MDNS_RECORD_SHARED);
        /* Meta DNS-SD record */
        meta_record = pico_mdns_record_create("_services._dns-sd._udp.local",
                                            (void *)service,
                                            (uint16_t)strlen(service),
                                            PICO_DNS_TYPE_PTR,
                                            ttl, PICO_MDNS_RECORD_SHARED);
        PICO_FREE(sin); // Free allocated memory
        if (!meta_record || !ptr_record) {
            mdns_dbg("Could not generate META or PTR records!\n");
            pico_mdns_record_delete((void **)&ptr_record);
            pico_mdns_record_delete((void **)&meta_record);
            return -1;
        }

        ptr_record->flags |= (PICO_MDNS_RECORD_PROBED |
                              PICO_MDNS_RECORD_CLAIMED);
        meta_record->flags |= (PICO_MDNS_RECORD_PROBED |
                               PICO_MDNS_RECORD_CLAIMED);

        /* Add copies to the answer tree */
        pico_rtree_add_copy(antree, meta_record);
        pico_rtree_add_copy(antree, ptr_record);

        /* Insert the created service record in MyRecords, alread in, destroy */
        if (pico_tree_insert(&MyRecords, meta_record)) {
            mdns_dbg("MDNS: Failed to insert meta record in tree\n");
            pico_mdns_record_delete((void **)&meta_record);
            pico_mdns_record_delete((void **)&ptr_record);
            return -1;
        }

        if (pico_tree_insert(&MyRecords, ptr_record)) {
            mdns_dbg("MDNS: Failed to insert ptr record in tree\n");
            pico_mdns_record_delete((void **)&ptr_record);
            pico_tree_delete(&MyRecords, meta_record);
            pico_mdns_record_delete((void **)&meta_record);
        }
    }
    return 0;
} /* Static path count: 9 */

/* ****************************************************************************
 *  Gathers additional records for a to send response. Checks for services and
 *  whether or not there should be NSEC records added to the additional section
 *
 *  @param antree mDNS record tree with answers to send
 *  @param artree mDNS record tree with additionals to send
 *  @return Returns 0 when additionals are properly generated and added
 * ****************************************************************************/
static int
pico_mdns_gather_additionals( pico_mdns_rtree *antree,
                              pico_mdns_rtree *artree )
{
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL;
    int ret = 0;

    /* Check params */
    if (!antree || !artree) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    } else {
        /* Look for SRV records in the tree */
        pico_tree_foreach(node, antree) {
            if ((record = node->keyValue) &&
                short_be(record->record->rsuffix->rtype) == PICO_DNS_TYPE_SRV &&
                (ret = pico_mdns_gather_service_meta(antree, artree, record)))
                return ret;
        }

        /* Look for unique records in the tree to generate NSEC records */
        pico_tree_foreach(node, antree) {
            if ((record = node->keyValue) && IS_UNIQUE_RECORD(record) &&
                (ret = pico_mdns_additionals_add_nsec(artree,
                                                    record->record->rname)))
                return ret;
        }

        /* Look for unique records in the additionals to generate NSEC records*/
        pico_tree_foreach(node, artree) {
            if ((record = node->keyValue) && IS_UNIQUE_RECORD(record) &&
                (ret = pico_mdns_additionals_add_nsec(artree,
                                                    record->record->rname)))
                return ret;
        }
    }

    return 0;
} /* Static path count: 9 */

/* ****************************************************************************
 * Sends mDNS records to either multicast peer via unicast to a single peer.
 *
 *  @param antree Tree with mDNS records to send as answers
 *  @param peer   IPv4-address of peer who this host has RCVD a packet.
 *  @return 0 when answers are properly handled, something else otherwise.
 * ****************************************************************************/
static int
pico_mdns_reply( pico_mdns_rtree *antree, struct pico_ip4 peer )
{
    PICO_DNS_RTREE_DECLARE(antree_m);
    PICO_DNS_RTREE_DECLARE(antree_u);
    PICO_MDNS_RTREE_DECLARE(artree);
    PICO_DNS_RTREE_DECLARE(artree_dummy);
    PICO_DNS_RTREE_DECLARE(artree_dns);

    /* Try to gather additionals for the to send response */
    if (pico_mdns_gather_additionals(antree, &artree)) {
        mdns_dbg("Could not gather additionals properly!\n");
        return -1;
    }

    /* Sort the answers into multicast and unicast answers */
    pico_mdns_sort_unicast_multicast(antree, &antree_u, &antree_m);

    /* Convert the mDNS additional tree to a DNS additional tree to send with
     * the the unicast AND the multicast response */
    pico_mdns_sort_unicast_multicast(&artree, &artree_dummy, &artree_dns);

    /* Send response via unicast */
    if (pico_mdns_unicast_reply(&antree_u, &artree_dns, peer)) {
        mdns_dbg("Could not sent reply via unicast!\n");
        return -1;
    }

    /* Send response via multicast */
    if (pico_mdns_multicast_reply(&antree_m, &artree_dns)) {
        mdns_dbg("Could not sent reply via multicast!\n");
        return -1;
    }

    pico_tree_destroy(&antree_m, NULL);
    pico_tree_destroy(&antree_u, NULL);
    pico_tree_destroy(&artree_dummy, NULL);
    pico_tree_destroy(&artree_dns, NULL);
    PICO_MDNS_RTREE_DESTROY(&artree);

    return 0;
}

/* ****************************************************************************
 *  Parses DNS records from a plain chunk of data and looks for them in the
 *  answer tree. If they're found, they will be removed from the tree.
 *
 *  @param rtree   Tree to look in for known answers
 *  @param packet  DNS packet in which to look for known answers
 *  @param ancount Amount of answers in the DNS packet
 *  @param data    Answer section of the DNS packet as a flat chunk of memory.
 *  @return 0 K.A.S. could be properly applied, something else when not.
 * ****************************************************************************/
static int
pico_mdns_apply_k_a_s( pico_mdns_rtree *rtree,
                       pico_dns_packet *packet,
                       uint16_t ancount,
                       uint8_t **data )
{
    struct pico_tree_node *node = NULL, *next = NULL;
    struct pico_mdns_record *record = NULL, ka = {
        0
    };
    struct pico_dns_record answer = {
        0
    };
    uint16_t i = 0;

    /* Check params */
    if ((!data) || !rtree || !packet || !(*data)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    for (i = 0; i < ancount; i++) {
        /* Set rname of the record to the correct location */
        answer.rname = (char *)(*data);

        /* Set rsuffix of the record to the correct location */
        answer.rsuffix = (struct pico_dns_record_suffix *)
                         (answer.rname + pico_dns_namelen_comp(answer.rname) + 1u);

        /* Set rdata of the record to the correct location */
        answer.rdata = (uint8_t *) answer.rsuffix +
                       sizeof(struct pico_dns_record_suffix);

        pico_dns_record_decompress(&answer, packet);
        ka.record = &answer;

        /* If the answer is in the record vector */
        pico_tree_foreach_safe(node, rtree, next) {
            if ((record = node->keyValue)) {
                if (pico_mdns_record_cmp(record, &ka) == 0)
                    record = pico_tree_delete(rtree, record);
            }
        }
        PICO_FREE(ka.record->rname);
        ka.record = NULL;

        /* Move to next record */
        *data = (uint8_t *) answer.rdata + short_be(answer.rsuffix->rdlength);
    }
    return 0;
}

/* ****************************************************************************
 *  Handles a single incoming query packet. Applies Known Answer Suppression
 *  after handling as well.
 *
 *  @param packet Received packet
 *  @param peer   IPv4 address of the peer who sent the received packet.
 *  @return Returns 0 when the query packet is properly handled.
 * ****************************************************************************/
static int
pico_mdns_handle_query_packet( pico_dns_packet *packet, struct pico_ip4 peer )
{
    PICO_MDNS_RTREE_DECLARE(antree);
    uint16_t qdcount = 0, ancount = 0;
    uint8_t *data = NULL;

    /* Move to the data section of the packet */
    data = (uint8_t *)packet + sizeof(struct pico_dns_header);

    /* Generate a list of answers */
    qdcount = short_be(packet->qdcount);
    antree = pico_mdns_handle_data_as_questions(&data, qdcount, packet);
    if (pico_tree_count(&antree) == 0) {
        mdns_dbg("No records found that correspond with this query!\n");
        return 0;
    }

    /* Apply Known Answer Suppression */
    ancount = short_be(packet->ancount);
    if (pico_mdns_apply_k_a_s(&antree, packet, ancount, &data)) {
        mdns_dbg("Could not apply known answer suppression!\n");
        return -1;
    }

    /* Try to reply with the left-over answers */
    pico_mdns_reply(&antree, peer);
    PICO_MDNS_RTREE_DESTROY(&antree);

    return 0;
}

/* ****************************************************************************
 *  Handles a single incoming probe packet. Checks for Simultaneous Probe
 *  Tiebreaking as well.
 *
 *  @param packet Received probe packet.
 *  @param peer   IPv4 address of the peer who sent the probe packet.
 *  @return Returns 0 when the probe packet is properly handled.
 * ****************************************************************************/
static int
pico_mdns_handle_probe_packet( pico_dns_packet *packet, struct pico_ip4 peer )
{
    PICO_MDNS_RTREE_DECLARE(antree);
    uint16_t qdcount = 0, nscount = 0;
    uint8_t *data = NULL;

    /* Move to the data section of the packet */
    data = (uint8_t *)packet + sizeof(struct pico_dns_header);

    /* Generate a list of answers */
    qdcount = short_be(packet->qdcount);
    antree = pico_mdns_handle_data_as_questions(&data, qdcount, packet);

    /* Check for Simultaneous Probe Tiebreaking */
    nscount = short_be(packet->nscount);
    pico_mdns_handle_data_as_answers_generic(&data, nscount, packet, 1);

    /* Try to reply with the answers */
    if (pico_tree_count(&antree) != 0) {
        int retval = pico_mdns_reply(&antree, peer);
        PICO_MDNS_RTREE_DESTROY(&antree);
        return retval;
    }

    return 0;
}

/* ****************************************************************************
 *  Handles a single incoming answer packet.
 *
 *  @param packet Received answer packet.
 *  @return Returns 0 when the response packet is properly handled.
 * ****************************************************************************/
static int
pico_mdns_handle_response_packet( pico_dns_packet *packet )
{
    uint8_t *data = NULL;
    uint16_t ancount = 0;

    /* Move to the data section of the packet */
    data = (uint8_t *)packet + sizeof(struct pico_dns_header);

    /* Generate a list of answers */
    ancount = short_be(packet->ancount);
    if (pico_mdns_handle_data_as_answers_generic(&data, ancount, packet, 0)) {
        mdns_dbg("Could not handle data as answers\n");
        return -1;
    }

    return 0;
}

/* ****************************************************************************
 *  Parses an incoming packet and handles it according to the type of the
 *  packet. Packet type determination happens in this function.
 *
 *  @param buf    Memory buffer containing the received packet
 *  @param buflen Length in bytes of the memory buffer
 *  @param peer   IPv4 address of the peer who sent the received packet.
 *  @return 0 when the packet is properly handled. Something else when it's not
 * ****************************************************************************/
static int
pico_mdns_recv( void *buf, int buflen, struct pico_ip4 peer )
{
    pico_dns_packet *packet = (pico_dns_packet *) buf;
    uint16_t qdcount = short_be(packet->qdcount);
    uint16_t ancount = short_be(packet->ancount);
    uint16_t authcount = short_be(packet->nscount);
    uint16_t addcount = short_be(packet->arcount);

    /* RFC6762: */
    /* 18.3: Messages received with an opcode other than zero MUST be silently */
    /* ignored. */
    /* 18.11: messages received with non-zero Response Codes MUST be silently */
    /* ignored */
    if(packet->opcode == 0 && packet->rcode == 0) {
        mdns_dbg(">>>>>>> QDcount: %u, ANcount: %u, NScount: %u, ARcount: %u\n",
                 qdcount, ancount, authcount, addcount);

        IGNORE_PARAMETER(buflen);
        IGNORE_PARAMETER(addcount);

        /* DNS PACKET TYPE DETERMINATION */
        if ((qdcount > 0)) {
            if (authcount > 0) {
                mdns_dbg(">>>>>>> RCVD a mDNS probe query:\n");
                /* Packet is probe query */
                if (pico_mdns_handle_probe_packet(packet, peer) < 0) {
                    mdns_dbg("Could not handle mDNS probe query!\n");
                    return -1;
                }
            } else {
                mdns_dbg(">>>>>>> RCVD a plain mDNS query:\n");
                /* Packet is a plain query */
                if (pico_mdns_handle_query_packet(packet, peer) < 0) {
                    mdns_dbg("Could not handle plain DNS query!\n");
                    return -1;
                }
            }
        } else {
            if (ancount > 0) {
                mdns_dbg(">>>>>>> RCVD a mDNS response:\n");
                /* Packet is a response */
                if (pico_mdns_handle_response_packet(packet) < 0) {
                    mdns_dbg("Could not handle DNS response!\n");
                    return -1;
                }
            } else {
                /* Something went wrong here... */
                mdns_dbg("RCVD Packet contains no questions or answers...\n");
                return -1;
            }
        }
    }

    return 0;
}

/* ****************************************************************************
 *  picoTCP callback for UDP IPv4 Socket events
 *
 *  @param ev Determination of the occurred event
 *  @param s  Socket on which the event occurred
 * ****************************************************************************/
static void
pico_mdns_event4( uint16_t ev, struct pico_socket *s )
{
    char *recvbuf = NULL;
    struct pico_ip4 peer = {
        0
    };
    int pico_read = 0;
    uint16_t port = 0;

    /* process read event, data available */
    if (ev == PICO_SOCK_EV_RD) {
        mdns_dbg("\n>>>>>>> READ EVENT! <<<<<<<\n");
        recvbuf = PICO_ZALLOC(PICO_MDNS_MAXBUF);
        if (!recvbuf) {
            pico_err = PICO_ERR_ENOMEM;
            return;
        }

        /* Receive while data is available in socket buffer */
        while((pico_read = pico_socket_recvfrom(s, recvbuf, PICO_MDNS_MAXBUF,
                                                &peer, &port)) > 0) {
            /* Handle the MDNS data received */
            pico_mdns_recv(recvbuf, pico_read, peer);
        }
        PICO_FREE(recvbuf);
        mdns_dbg(">>>>>>>>>>>>>><<<<<<<<<<<<<\n\n");
    } else
        mdns_dbg("Socket Error received. Bailing out.\n");
}

/* MARK: ADDRESS RESOLUTION */

/* ****************************************************************************
 *  Send a mDNS query packet on the wire. This is scheduled with a pico_timer-
 *  event.
 *
 *  @param now Ignore
 *  @param arg Void-pointer to query-cookie
 * ****************************************************************************/
static void
pico_mdns_send_query_packet( pico_time now, void *arg )
{
    struct pico_mdns_cookie *cookie = (struct pico_mdns_cookie *)arg;
    pico_dns_qtree *questions = NULL;
    pico_dns_packet *packet = NULL;
    uint16_t len = 0;

    IGNORE_PARAMETER(now);

    /* Parse in the cookie */
    if (!cookie || cookie->type != PICO_MDNS_PACKET_TYPE_QUERY)
        return;

    /* Create DNS query packet */
    questions = &(cookie->qtree);
    if (!(packet = pico_dns_query_create(questions, NULL, NULL, NULL, &len))) {
        mdns_dbg("Could not create query packet!\n");
        return;
    }

    packet->id = 0;

    /* RFC6762: 18.6: In both multicast query and response messages,
       the RD bit SHOULD be zero on transmission. In pico_dns_fill_packet_header,
       the RD bit is set to PICO_DNS_RD_IS_DESIRED, which is defined to be 1 */
    packet->rd = PICO_DNS_RD_NO_DESIRE;

    if (cookie->status != PICO_MDNS_COOKIE_STATUS_CANCELLED) {
        cookie->status = PICO_MDNS_COOKIE_STATUS_ACTIVE;
        if(pico_mdns_send_packet(packet, len) != (int)len) {
            mdns_dbg("Send error occurred!\n");
            return;
        }

        mdns_dbg("DONE - Sent query.\n");
    } else {
        mdns_dbg("DONE - Duplicate query suppressed.\n");
        pico_timer_cancel(cookie->send_timer);
        /* Remove cookie from Cookies */
        cookie = pico_tree_delete(&Cookies, cookie);
        pico_mdns_cookie_delete((void **)&cookie);
    }

    PICO_FREE(packet);
}

/* ****************************************************************************
 *  Generates a mDNS query packet and schedules a sending on the wire.
 *
 *  @param url      URL for the name of the question contained in the query
 *  @param type     DNS type of the question contained in the query
 *  @param callback Callback to call when a response on this query is RCVD.
 *  @return 0 When the query is successfully generated and scheduled for sending
 * ****************************************************************************/
static int
pico_mdns_getrecord_generic( const char *url, uint16_t type,
                             void (*callback)(pico_mdns_rtree *,
                                              char *,
                                              void *),
                             void *arg)
{
    struct pico_mdns_cookie *cookie = NULL;
    PICO_DNS_QTREE_DECLARE(qtree);
    PICO_MDNS_RTREE_DECLARE(antree);
    PICO_MDNS_RTREE_DECLARE(artree);
    struct pico_dns_question *q = NULL;
    uint16_t l = 0;

    /* Create a single question and add it to the tree */
    q = pico_mdns_question_create(url, &l, PICO_PROTO_IPV4, type, 0, 0);
    if (!q) {
        mdns_dbg("question_create returned NULL!\n");
        return -1;
    }

    if (pico_tree_insert(&qtree, q)) {
    	mdns_dbg("inserting query into tree failed!\n");
        pico_dns_question_delete((void **)&q);
		return -1;
	}


    /* Create a mDNS cookie to send */
    if (!(cookie = pico_mdns_cookie_create(qtree, antree, artree, 1,
                                           PICO_MDNS_PACKET_TYPE_QUERY,
                                           callback, arg))) {
        PICO_DNS_QTREE_DESTROY(&qtree);
        mdns_dbg("cookie_create returned NULL!\n");
        return -1;
    }

    /* Add cookie to Cookies to be able to find it afterwards */
    if(pico_tree_insert(&Cookies, cookie) ){
		mdns_dbg("inserting cookie into tree failed!\n");
        PICO_DNS_QTREE_DESTROY(&qtree);
        pico_mdns_cookie_delete((void **)&cookie);
        return -1;
	}

    /* Create new pico_timer-event to send packet */
    if (!pico_mdns_timer_add((pico_rand() % 120) + 20, pico_mdns_send_query_packet,
                   (void *)cookie)) {
        mdns_dbg("MDNS: Failed to start send_query_packet timer\n");
        pico_tree_delete(&Cookies, cookie);
        pico_mdns_cookie_delete((void**)&cookie);
        pico_dns_question_delete((void**)&q);
        return -1;
    }

    return 0;
}

/* ****************************************************************************
 *  API-call to query a record with a certain URL and type. First checks the
 *  Cache for this record. If no cache-entry is found, a query will be sent on
 *  the wire for this record.
 *
 *  @param url      URL to query for.
 *  @param type     DNS type to query for.
 *  @param callback Callback to call when records are found for the query.
 *  @return 0 when query is correctly parsed, something else on failure.
 * ****************************************************************************/
int
pico_mdns_getrecord( const char *url, uint16_t type,
                     void (*callback)(pico_mdns_rtree *,
                                      char *,
                                      void *),
                     void *arg )
{
#if PICO_MDNS_ALLOW_CACHING == 1
    PICO_MDNS_RTREE_DECLARE(cache_hits);
    char *name = NULL;
#endif

    /* Check params */
    if (!url) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* First, try to find records in the cache */
#if PICO_MDNS_ALLOW_CACHING == 1
    name = pico_dns_url_to_qname(url);
    cache_hits = pico_mdns_rtree_find_name_type(&Cache, name, type, 0);
    PICO_FREE(name);
    if (pico_tree_count(&cache_hits) > 0) {
        mdns_dbg("CACHE HIT! Passed cache records to callback.\n");
        callback(&cache_hits, NULL, arg);
    } else {
#endif
    mdns_dbg("CACHE MISS! Trying to resolve URL '%s'...\n", url);
    return pico_mdns_getrecord_generic(url, type, callback, arg);
#if PICO_MDNS_ALLOW_CACHING == 1
}
return 0;
#endif
}

/* MARK: PROBING & ANNOUNCING */

/* ****************************************************************************
 *  Function to create an announcement from an mDNS cookie and send it on the
 *  wire.
 *
 *  @param now Ignore
 *  @param arg Void-pointer to mDNS announcement cookie
 * ***************************************************************************/
static void
pico_mdns_send_announcement_packet( pico_time now, void *arg )
{
    struct pico_mdns_cookie *cookie = (struct pico_mdns_cookie *)arg;

    /* Check params */
    IGNORE_PARAMETER(now);
    if (!cookie) {
        return;
    }

    cookie->status = PICO_MDNS_COOKIE_STATUS_ACTIVE;
    if (cookie->count > 0) {
        /* Send the announcement on the wire */
        pico_mdns_reply(&(cookie->antree), inaddr_any);
        mdns_dbg("DONE - Sent announcement!\n");

        /* The Multicast DNS responder MUST send at least two unsolicited
           responses, one second apart.  To provide increased robustness
           against packet loss, a responder MAY send up to eight unsolicited
           responses, provided that the interval between unsolicited
           responses increases by at least a factor of two with
           every response sent.
         */
        --(cookie->count);
        if (cookie->count == 0) {
            cookie->status = PICO_MDNS_COOKIE_STATUS_INACTIVE;

            /* Update the states of the records */
            pico_mdns_my_records_claimed(cookie->antree,
                                         cookie->callback,
                                         cookie->arg);

            /* Try to delete the cookie */
            pico_tree_delete(&Cookies, cookie);
            pico_mdns_cookie_delete((void **)&cookie);
        }
        else{
            /*
               A responder MAY send up to eight unsolicited responses,
               provided that the interval between unsolicited responses increases
               by at least a factor of two with every response sent.
               Starting at 1 second.
               So we bithsift to get our powers of two and we multiply by 1000 to
               get our miliseconds.
             */
            if (!pico_mdns_timer_add((pico_time)((1 << (PICO_MDNS_ANNOUNCEMENT_COUNT - cookie->count - 1))
                                       * 1000), pico_mdns_send_announcement_packet, cookie)) {
                mdns_dbg("MDNS: Failed to start send_announcement_packet timer\n");
                /* TODO no idea what the consequences of this are */

            }
        }
    }
}

/* ****************************************************************************
 *  Announces all 'my records' which passed the probing-step or just shared
 *  records.
 *
 *  @param callback Gets called when all records in the cookie are announced.
 *  @return 0 When the host successfully started announcing.
 * ****************************************************************************/
static int
pico_mdns_announce( void (*callback)(pico_mdns_rtree *,
                                     char *,
                                     void *),
                    void *arg )
{
    struct pico_mdns_cookie *announcement_cookie = NULL;
    PICO_DNS_QTREE_DECLARE(qtree);
    PICO_MDNS_RTREE_DECLARE(antree);
    PICO_MDNS_RTREE_DECLARE(artree);

    /* Check params */
    if (!callback) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    IGNORE_PARAMETER(arg);

    /* Find out which resource records can be announced */
    antree = pico_mdns_my_records_find_probed();
    if (pico_tree_count(&antree) == 0) {
        return 0;
    }

    /* Create a mDNS packet cookie */
    if (!(announcement_cookie = pico_mdns_cookie_create(qtree, antree, artree,
                                                        PICO_MDNS_ANNOUNCEMENT_COUNT,
                                                        PICO_MDNS_PACKET_TYPE_ANNOUNCEMENT,
                                                        callback, arg))) {
        mdns_dbg("cookie_create returned NULL!\n");
        PICO_MDNS_RTREE_DESTROY(&antree);
        return -1;
    }

    /* Send a first unsolicited announcement */
    pico_mdns_send_announcement_packet(0, announcement_cookie);
    mdns_dbg("DONE - Started announcing.\n");

    return 0;
}

/* ****************************************************************************
 *  Makes sure the cache flush bit of the to probe records is cleared, and
 *  generates a DNS record tree to insert in the Authority Section of the DNS
 *  packet
 *
 *  @param records mDNS records to probe.
 *  @return DNS record tree to with actual DNS records to insert in Authority
 *			Section of probe packet.
 * ****************************************************************************/
static pico_dns_rtree
pico_mdns_gen_probe_auths( pico_mdns_rtree *records )
{
    PICO_DNS_RTREE_DECLARE(nstree);
    struct pico_tree_node *node = NULL;
    struct pico_mdns_record *record = NULL;

    pico_tree_foreach(node, records) {
        if ((record = node->keyValue) && record->record) {
            /* Clear the cache flush bit for authority records in probes */
            PICO_MDNS_CLR_MSB_BE(record->record->rsuffix->rclass);
            /* Only the actual DNS records is required */
            if (pico_tree_insert(&nstree, record->record) == &LEAF) {
                mdns_dbg("MDNS: Failed to insert record in tree\n");
                break;
			}
        }
    }

    return nstree;
}

/* ****************************************************************************
 *  Function to create a probe from an mDNS cookie and send it on the wire.
 *
 *  @param now Ignore
 *  @param arg Void-pointer to mDNS probe cookie
 * ****************************************************************************/
static void
pico_mdns_send_probe_packet( pico_time now, void *arg )
{
    struct pico_mdns_cookie *cookie = (struct pico_mdns_cookie *)arg;
    pico_dns_packet *packet = NULL;
    PICO_DNS_RTREE_DECLARE(nstree);
    uint16_t len = 0;

    /* Check params */
    IGNORE_PARAMETER(now);
    /* if (!cookie || (cookie->type == PICO_MDNS_COOKIE_STATUS_INACTIVE)) { */
    if (!cookie || (cookie->type != PICO_MDNS_PACKET_TYPE_PROBE)) {
        pico_err = PICO_ERR_EINVAL;
        return;
    } else {
        /* Set the cookie to the active state */
        cookie->status = PICO_MDNS_COOKIE_STATUS_ACTIVE;
        if (cookie->count > 0) {
            --(cookie->count);

            /* Generate authority records */
            nstree = pico_mdns_gen_probe_auths(&(cookie->antree));

            /* Create an mDNS answer */
            if (!(packet = pico_dns_query_create(&(cookie->qtree), NULL,
                                                &nstree, NULL, &len))) {
                PICO_DNS_RTREE_DESTROY(&nstree);
                mdns_dbg("Could not create probe packet!\n");
                return;
            }

            pico_tree_destroy(&nstree, NULL);

            /* RFC6762: 18.1 */
            packet->id = 0;

            /* RFC6762: 18.6: In both multicast query and response messages,
            the RD bit SHOULD be zero on transmission.
            In pico_dns_fill_packet_header, the RD bit is set to
            PICO_DNS_RD_IS_DESIRED, which is defined to be 1 */
            packet->rd = PICO_DNS_RD_NO_DESIRE;

            /* Send the mDNS answer unsolicited via multicast */
            if(pico_mdns_send_packet(packet, len) != (int)len) {
                mdns_dbg("Send error occurred!\n");
                return;
            }

            PICO_FREE(packet);

            mdns_dbg("DONE - Sent probe!\n");

            /* Probes should be sent with a delay in between of 250 ms */
            if (PICO_MDNS_COOKIE_STATUS_ACTIVE == cookie->status ) {
                cookie->send_timer = pico_mdns_timer_add(250,
                                                    pico_mdns_send_probe_packet,
                                                    (void *)cookie);
                if (!cookie->send_timer) {
                    mdns_dbg("MDNS: Failed to start send_probe_packet timer\n");
                    /* TODO no idea what the consequences of this are */
                    return;
                }
            }
        } else {
            mdns_dbg("DONE - Probing.\n");

            pico_mdns_my_records_probed(&(cookie->antree));

            /* Start announcing */
            cookie->count = PICO_MDNS_ANNOUNCEMENT_COUNT;
            cookie->type = PICO_MDNS_PACKET_TYPE_ANNOUNCEMENT;
            pico_mdns_send_announcement_packet(0, (void*) cookie);
        }
    }
} /* Static path count: 10 */

/* ****************************************************************************
 *  Adds a new probe question to the probe cookie questions, if a probe question
 *  for a new is already present in the question-tree, it will not be generated
 *  and inserted again
 *
 *  @param qtree Probe question tree
 *  @param name  Name for which the function has to create a probe question
 *  @return 0 when the probe question is already present or added successfully.
 * ****************************************************************************/
static int
pico_mdns_add_probe_question( pico_dns_qtree *qtree,
                              char *name )
{
    struct pico_dns_question *new = NULL;
    char *url = NULL;
    uint16_t qlen = 0;
    uint8_t flags = PICO_MDNS_QUESTION_FLAG_PROBE;

#if PICO_MDNS_PROBE_UNICAST == 1
    flags |= PICO_MDNS_QUESTION_FLAG_UNICAST_RES;
#endif

    /* Convert name to URL and try to create a new probe question */
    if (!(url = pico_dns_qname_to_url(name)))
        return -1;

    mdns_dbg("Probe question for URL: %s\n", url);
    if (!(new = pico_mdns_question_create(url, &qlen, PICO_PROTO_IPV4,
                                          PICO_DNS_TYPE_ANY, flags, 0))) {
        PICO_FREE(url);
        return -1;
    }

    PICO_FREE(url);

    /* Try to find an existing question in the vector */
    if (pico_tree_insert(qtree, new))
        pico_dns_question_delete((void **)&new);

    return 0;
}

/* ****************************************************************************
 *  Find any of my record that need to be probed and try to probe them.
 *
 *  @param callback Callback to call when all records are properly registered
 *  @return When host successfully started probing.
 * ****************************************************************************/
static int pico_mdns_probe( void (*callback)(pico_mdns_rtree *,
                                             char *,
                                             void *),
                            void *arg )
{
    struct pico_mdns_cookie *cookie = NULL;
    struct pico_mdns_record *record = NULL;
    struct pico_tree_node *node = NULL;
    PICO_DNS_QTREE_DECLARE(qtree);
    PICO_MDNS_RTREE_DECLARE(antree);
    PICO_MDNS_RTREE_DECLARE(artree);

    /* Check params */
    if (!callback) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    } else {
        /* Find my records that need to pass the probing step first
        * All records that don't have their PROBED flag set and
        * are not being probed at hte moment are added to the tree
        */
        antree = pico_mdns_my_records_find_to_probe();

        /* Create probe questions for the records to be probed  */
        pico_tree_foreach(node, &antree) {
            if ((record = node->keyValue)) {
                pico_mdns_add_probe_question(&qtree, record->record->rname);
            }
        }

        /* Create a mDNS packet to send */
        cookie = pico_mdns_cookie_create(qtree, antree, artree,
                                        PICO_MDNS_PROBE_COUNT,
                                        PICO_MDNS_PACKET_TYPE_PROBE,
                                        callback, arg);
        if (!cookie) {
            mdns_dbg("Cookie_create returned NULL @ probe()!\n");
            PICO_DNS_QTREE_DESTROY(&qtree);
            PICO_MDNS_RTREE_DESTROY(&antree);
            return -1;
        }

        /* Add the probe cookie to the cookie tree */
        if (pico_tree_insert(&Cookies, cookie)) {
            pico_mdns_cookie_delete((void **)&cookie);
            return -1;
        }

        /* RFC6762: 8.1. Probing */
        /* When ready to send its Multicast DNS probe packet(s) the host should */
        /* first wait for a short random delay time, uniformly distributed in */
        /* the range 0-250 ms. */
        cookie->send_timer = pico_mdns_timer_add(pico_rand() % 250,
                                            pico_mdns_send_probe_packet,
                                            (void *)cookie);
        if (!cookie->send_timer) {
            mdns_dbg("MDNS: Failed to start send_probe_packet timer\n");
            pico_tree_delete(&Cookies, cookie);
            pico_mdns_cookie_delete((void**)&cookie);
            return -1;
        }

        mdns_dbg("DONE - Started probing.\n");
    }
    return 0;
} /* Static path count: 9 */

/* MARK: API functions */

/* ****************************************************************************
 *  Claim or reclaim all the mDNS records contain in a tree in one single call
 *
 *  @param rtree    mDNS record tree with records to claim
 *  @param reclaim  Whether or not the records in tree should be reclaimed.
 *  @param callback Callback to call when all records are properly registered
 *  @return 0 When claiming didn't horribly fail.
 * ****************************************************************************/
static int
pico_mdns_claim_generic( pico_mdns_rtree rtree,
                         uint8_t reclaim,
                         void (*callback)(pico_mdns_rtree *,
                                          char *,
                                          void *),
                         void *arg )
{
    /* Check if arguments are passed correctly */
    if (!callback) {
        mdns_dbg("NULL pointers passed to 'pico_mdns_claim()'!\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Check if module is initialised */
    if (!mdns_sock_ipv4) {
        mdns_dbg("Socket not initialised, did you call 'pico_mdns_init()'?\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* 1.) Appending records to 'my records' */
    pico_mdns_my_records_add(&rtree, reclaim);

    /* 2a.) Try to probe any records */
    pico_mdns_probe(callback, arg);

    /* 2b.) Try to announce any records */
    pico_mdns_announce(callback, arg);

    return 0;
}

/* ****************************************************************************
 *  Claim all different mDNS records in a tree in a single API-call. All records
 *  in tree are called in a single new claim-session.
 *
 *  @param rtree    mDNS record tree with records to claim
 *  @param callback Callback to call when all record are properly claimed.
 *  @return 0 When claiming didn't horribly fail.
 * ****************************************************************************/
int
pico_mdns_claim( pico_mdns_rtree rtree,
                 void (*callback)(pico_mdns_rtree *,
                                  char *,
                                  void *),
                 void *arg )
{
    return pico_mdns_claim_generic(rtree, PICO_MDNS_NO_RECLAIM, callback, arg);
}

/* ****************************************************************************
 *  Reclaim records when a conflict occurred, claim-session will stay the same
 *  as the session in which the conflict occurred.
 *
 *  @param rtree    mDNS record tree with records to claim
 *  @param callback Callback to call when all record are properly claimed.
 *  @return 0 When claiming didn't horribly fail.
 * ****************************************************************************/
static int
pico_mdns_reclaim( pico_mdns_rtree rtree,
                   void (*callback)(pico_mdns_rtree *,
                                    char *,
                                    void *),
                   void *arg )
{
    return pico_mdns_claim_generic(rtree, PICO_MDNS_RECLAIM, callback, arg);
}

/* ****************************************************************************
 *  Tries to claim a hostname for this machine. Claims automatically a
 *  unique A record with the IPv4-address of this host.
 *  The hostname won't be set directly when this functions returns,
 *  but only if the claiming of the unique record succeeded.
 *  Init-callback will be called when the hostname-record is successfully
 *  registered.
 *
 *  @param url URL to set the hostname to.
 *  @param arg Argument to pass to the init-callback.
 *  @return 0 when the host started registering the hostname-record successfully,
 *          Returns something else when it didn't succeeded.
 * ****************************************************************************/
int
pico_mdns_tryclaim_hostname( const char *url, void *arg )
{
    PICO_MDNS_RTREE_DECLARE(rtree);
    struct pico_mdns_record *record = NULL;

    /* Check if module is initialised */
    if (!mdns_sock_ipv4) {
        mdns_dbg("mDNS socket not initialised, did you call 'pico_mdns_init()'?\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    } else {
        /* Create an A record for hostname */
        record = pico_mdns_record_create(url,
                                        &(mdns_sock_ipv4->local_addr.ip4.addr),
                                        PICO_SIZE_IP4, PICO_DNS_TYPE_A,
                                        PICO_MDNS_DEFAULT_TTL,
                                        (PICO_MDNS_RECORD_UNIQUE |
                                        PICO_MDNS_RECORD_HOSTNAME));
        if (!record) {
            mdns_dbg("Could not create A record for hostname %s!\n",
                    strerror(pico_err));
            return -1;
        }

        /* TODO: Create IPv6 record */
        /* TODO: Create a reverse resolution record */

        /* Try to claim the record */
        if (pico_tree_insert(&rtree, record)) {
            pico_mdns_record_delete((void **)&record);
            return -1;
        }

        if (pico_mdns_claim(rtree, init_callback, arg)) {
            mdns_dbg("Could not claim record for hostname %s!\n", url);
            PICO_MDNS_RTREE_DESTROY(&rtree);
            return -1;
        }

        pico_tree_destroy(&rtree, NULL);
    }
    return 0;
} /* Static path count: 9 */

/* ****************************************************************************
 *  Get the hostname for this machine.
 *
 *  @return Returns the hostname for this machine when the module is initialised
 *			Returns NULL when the module is not initialised.
 * ****************************************************************************/
const char *
pico_mdns_get_hostname( void )
{
    /* Check if module is initialised */
    if (!mdns_sock_ipv4) {
        mdns_dbg("mDNS socket not initialised, did you call 'pico_mdns_init()'?\n");
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    return (const char *)_hostname;
}

static void
pico_mdns_cleanup( void )
{
    /* Delete socket if it was previously opened */
    if (mdns_sock_ipv4) {
        pico_socket_del(mdns_sock_ipv4);
    }

    /* Clear out every memory structure used by mDNS */
#if PICO_MDNS_ALLOW_CACHING == 1
    PICO_MDNS_RTREE_DESTROY(&Cache);
#endif /* PICO_MDNS_ALLOW_CACHING */
    PICO_MDNS_RTREE_DESTROY(&MyRecords);
    PICO_MDNS_CTREE_DESTROY(&Cookies);

    /* Cancel every timer */
    pico_timer_cancel_hashed(mdns_hash);
}

/* ****************************************************************************
 *  Initialises the entire mDNS-module and sets the hostname for this machine.
 *  Sets up the global mDNS socket properly and calls callback when succeeded.
 *	Only when the module is properly initialised records can be registered on
 *  the module.
 *
 *  @param hostname_url URL to set the hostname to.
 *  @param address      IPv4-address of this host to bind to.
 *  @param callback     Callback to call when the hostname is registered and
 *						also the global mDNS module callback. Gets called when
 *						Passive conflicts occur, so changes in records can be
 *						tracked in this callback.
 *	@param arg			Argument to pass to the init-callback.
 *  @return 0 when the module is properly initialised and the host started regis-
 *			tering the hostname. Returns something else went the host failed
 *			initialising the module or registering the hostname.
 * ****************************************************************************/
int
pico_mdns_init( const char *hostname,
                struct pico_ip4 address,
                void (*callback)(pico_mdns_rtree *,
                                 char *,
                                 void *),
                void *arg )
{
    struct pico_ip_mreq mreq4;
    uint16_t proto4 = PICO_PROTO_IPV4, port = 0, loop = 0, ttl = 255;

    /* Initialise port */
    port = short_be(mdns_port);

    /* Check callback parameter */
    if(!callback || !hostname) {
        mdns_dbg("No callback function supplied!\n");
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Clear out all the memory structure's and delete socket if it was
     * already opened before */
    pico_mdns_cleanup();

    /* Create a hash to identify mDNS timers with */
    mdns_hash = pico_hash(hostname, (uint32_t)strlen(hostname));

    /* Open global IPv4 mDNS socket */
    mdns_sock_ipv4 = pico_socket_open(proto4, PICO_PROTO_UDP, &pico_mdns_event4);
    if(!mdns_sock_ipv4) {
        mdns_dbg("pico_socket_open returned NULL-ptr...\n");
        return -1;
    }

    /* Convert the mDNS IPv4 destination address to struct */
    if(pico_string_to_ipv4(PICO_MDNS_DEST_ADDR4, &mreq4.mcast_group_addr.ip4.addr)) {
        mdns_dbg("String to IPv4 error\n");
        return -1;
    }

    /* Receive data on any network interface */
    mreq4.mcast_link_addr.ip4 = inaddr_any;

    /* Don't want the multicast data to be looped back to the host */
    if(pico_socket_setoption(mdns_sock_ipv4, PICO_IP_MULTICAST_LOOP, &loop)) {
        mdns_dbg("socket_setoption PICO_IP_MULTICAST_LOOP failed\n");
        return -1;
    }

    /* Tell the stack we're interested in this particular multicast group */
    if(pico_socket_setoption(mdns_sock_ipv4, PICO_IP_ADD_MEMBERSHIP, &mreq4)) {
        mdns_dbg("socket_setoption PICO_IP_ADD_MEMBERSHIP failed\n");
        return -1;
    }

    /* RFC6762:
     * 11.  Source Address Check
     *  All Multicast DNS responses (including responses sent via unicast)
     *  SHOULD be sent with IP TTL set to 255.
     */
    if(pico_socket_setoption(mdns_sock_ipv4, PICO_IP_MULTICAST_TTL, &ttl)) {
        mdns_dbg("socket_setoption PICO_IP_MULTICAST_TTL failed\n");
        return -1;
    }

    /* Bind to mDNS port */
    if (pico_socket_bind(mdns_sock_ipv4, (void *)&address, &port)) {
        mdns_dbg("Bind error!\n");
        return -1;
    }

    /* Set the global init callback variable */
    init_callback = callback;
    if (!pico_mdns_timer_add(PICO_MDNS_RR_TTL_TICK, pico_mdns_tick, NULL)) {
        mdns_dbg("MDNS: Failed to start tick timer\n");
        return -1;
    }

    /* Set the hostname eventually */
    return pico_mdns_tryclaim_hostname(hostname, arg);
}

#endif /* PICO_SUPPORT_MDNS */
