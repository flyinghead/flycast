/* ****************************************************************************
 *  PicoTCP. Copyright (c) 2014 TASS Belgium NV. Some rights reserved.
 *  See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 *  .
 *  Author: Toon Stegen, Jelle De Vleeschouwer
 * ****************************************************************************/
#ifndef INCLUDE_PICO_MDNS
#define INCLUDE_PICO_MDNS

#include "pico_dns_common.h"
#include "pico_tree.h"
#include "pico_ipv4.h"

/* ********************************* CONFIG ***********************************/
#define PICO_MDNS_PROBE_UNICAST 1       /* Probe queries as QU-questions      */
#define PICO_MDNS_CONTINUOUS_REFRESH 0  /* Continuously update cache          */
#define PICO_MDNS_ALLOW_CACHING 1       /* Enable caching on this host		  */
#define PICO_MDNS_DEFAULT_TTL 120       /* Default TTL of mDNS records        */
#define PICO_MDNS_SERVICE_TTL 120       /* Default TTL of SRV/TXT/PTR/NSEC    */
#define PICO_MDNS_PROBE_COUNT 3
/* Amount of probes to send:
   RFC6762: 8.1. Probing:
   250 ms after the first query, the host should send a second; then,
   250 ms after that, a third.  If, by 250 ms after the third probe, no
   conflicting Multicast DNS responses have been received, the host may
   move to the next step, announcing.
 */

#define PICO_MDNS_ANNOUNCEMENT_COUNT 3
/* Amount of announcements to send: (we've opted for 1 extra for robustness)
   RFC6762: 8.3. Announcing:
   The Multicast DNS responder MUST send at least two unsolicited
   responses, one second apart.  To provide increased robustness against
   packet loss, a responder MAY send up to eight unsolicited responses,
   provided that the interval between unsolicited responses increases by
   at least a factor of two with every response sent.
 */
/* ****************************************************************************/

#define PICO_MDNS_DEST_ADDR4 "224.0.0.251"

/* To make mDNS records unique or shared records */
#define PICO_MDNS_RECORD_UNIQUE 0x00u
#define PICO_MDNS_RECORD_SHARED 0x01u

/* To indicate if we reclaim or not */
#define PICO_MDNS_RECLAIM 1
#define PICO_MDNS_NO_RECLAIM 0

/* Flag to check for when records are returned, to determine the hostname */
#define PICO_MDNS_RECORD_HOSTNAME 0x02u
#define IS_HOSTNAME_RECORD(x) \
    (((x)->flags) & PICO_MDNS_RECORD_HOSTNAME) ? (1) : (0)

/* --- MDNS resource record --- */
struct pico_mdns_record
{
    struct pico_dns_record *record; /* DNS Resource Record */
    uint32_t current_ttl;           /* Current TTL */
    uint8_t flags;                  /* Resource Record flags */
    uint8_t claim_id;               /* Claim ID number */
};

/* ****************************************************************************
 *  Compares 2 mDNS records by type, name AND rdata for a truly unique result
 *
 *  @param ra mDNS record A
 *  @param rb mDNS record B
 *  @return 0 when records are equal, returns difference when they're not.
 * ****************************************************************************/
int
pico_mdns_record_cmp( void *a, void *b );

/* ****************************************************************************
 *  Deletes a single mDNS resource record.
 *
 *  @param record Void-pointer to mDNS Resource Record. Can be used with pico_-
 *         tree-destroy.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_mdns_record_delete( void **record );

/* ****************************************************************************
 *  Creates a single standalone mDNS resource record with given name, type and
 *  data to register on the network.
 *
 *  @param url     DNS rrecord name in URL format. Will be converted to DNS
 *                 name notation format.
 *  @param _rdata  Memory buffer with data to insert in the resource record. If
 *				   data of record should contain a DNS name, the name in the
 *				   databuffer needs to be in URL-format.
 *  @param datalen The exact length in bytes of the _rdata-buffer. If data of
 *				   record should contain a DNS name, datalen needs to be
 *				   pico_dns_strlen(_rdata).
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
                         uint8_t flags );



/* ****************************************************************************
 *  Definition of DNS record tree
 * ****************************************************************************/
typedef struct pico_tree pico_mdns_rtree;
#define PICO_MDNS_RTREE_DECLARE(name) \
    pico_mdns_rtree (name) = {&LEAF, pico_mdns_record_cmp}
#define PICO_MDNS_RTREE_DESTROY(rtree) \
    pico_tree_destroy((rtree), pico_mdns_record_delete)
#define PICO_MDNS_RTREE_ADD(tree, record) \
    pico_tree_insert((tree), (record))

/* ****************************************************************************
 *  API-call to query a record with a certain URL and type. First checks the
 *  Cache for this record. If no cache-entry is found, a query will be sent on
 *  the wire for this record.
 *
 *  @param url      URL to query for.
 *  @param type     DNS type top query for.
 *  @param callback Callback to call when records are found for the query.
 *  @return 0 when query is correctly parsed, something else on failure.
 * ****************************************************************************/
int
pico_mdns_getrecord( const char *url, uint16_t type,
                     void (*callback)(pico_mdns_rtree *,
                                      char *,
                                      void *),
                     void *arg );

/* ****************************************************************************
 *  Claim all different mDNS records in a tree in a single API-call. All records
 *  in tree are called in a single new claim-session.
 *
 *  @param rtree    mDNS record tree with records to claim
 *  @param callback Callback to call when all record are properly claimed.
 *  @return 0 When claiming didn't horribly fail.
 * ****************************************************************************/
int
pico_mdns_claim( pico_mdns_rtree record_tree,
                 void (*callback)(pico_mdns_rtree *,
                                  char *,
                                  void *),
                 void *arg );

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
pico_mdns_tryclaim_hostname( const char *url, void *arg );

/* ****************************************************************************
 *  Get the current hostname for this machine.
 *
 *  @return Returns the hostname for this machine when the module is initialised
 *			Returns NULL when the module is not initialised.
 * ****************************************************************************/
const char *
pico_mdns_get_hostname( void );

/* ****************************************************************************
 *  Initialises the entire mDNS-module and sets the hostname for this machine.
 *  Sets up the global mDNS socket properly and calls callback when succeeded.
 *	Only when the module is properly initialised records can be registered on
 *  the module.
 *
 *  @param hostname URL to set the hostname to.
 *  @param address  IPv4-address of this host to bind to.
 *  @param callback Callback to call when the hostname is registered and
 *					also the global mDNS module callback. Gets called when
 *					Passive conflicts occur, so changes in records can be
 *					tracked in this callback.
 *	@param arg		Argument to pass to the init-callback.
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
                void *arg );

#endif /* _INCLUDE_PICO_MDNS */
