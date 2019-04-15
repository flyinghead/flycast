
/*********************************************************************
   PicoTCP. Copyright (c) 2012 TASS Belgium NV. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
   .
   Authors: Toon Stegen, Jelle De Vleeschouwer
 *********************************************************************/

#ifndef INCLUDE_PICO_DNS_COMMON
#define INCLUDE_PICO_DNS_COMMON

#include "pico_config.h"
#include "pico_tree.h"

/* TYPE values */
#define PICO_DNS_TYPE_A 1
#define PICO_DNS_TYPE_CNAME 5
#define PICO_DNS_TYPE_PTR 12
#define PICO_DNS_TYPE_TXT 16
#define PICO_DNS_TYPE_AAAA 28
#define PICO_DNS_TYPE_SRV 33
#define PICO_DNS_TYPE_NSEC 47
#define PICO_DNS_TYPE_ANY 255

/* CLASS values */
#define PICO_DNS_CLASS_IN 1

/* FLAG values */
#define PICO_DNS_QR_QUERY 0
#define PICO_DNS_QR_RESPONSE 1
#define PICO_DNS_OPCODE_QUERY 0
#define PICO_DNS_OPCODE_IQUERY 1
#define PICO_DNS_OPCODE_STATUS 2
#define PICO_DNS_AA_NO_AUTHORITY 0
#define PICO_DNS_AA_IS_AUTHORITY 1
#define PICO_DNS_TC_NO_TRUNCATION 0
#define PICO_DNS_TC_IS_TRUNCATED 1
#define PICO_DNS_RD_NO_DESIRE 0
#define PICO_DNS_RD_IS_DESIRED 1
#define PICO_DNS_RA_NO_SUPPORT 0
#define PICO_DNS_RA_IS_SUPPORTED 1
#define PICO_DNS_RCODE_NO_ERROR 0
#define PICO_DNS_RCODE_EFORMAT 1
#define PICO_DNS_RCODE_ESERVER 2
#define PICO_DNS_RCODE_ENAME 3
#define PICO_DNS_RCODE_ENOIMP 4
#define PICO_DNS_RCODE_EREFUSED 5

#define PICO_ARPA_IPV4_SUFFIX ".in-addr.arpa"

#ifdef PICO_SUPPORT_IPV6
#define STRLEN_PTR_IP6 63
#define PICO_ARPA_IPV6_SUFFIX ".IP6.ARPA"
#endif

/* Used in pico_dns_rdata_cmp  */
#define PICO_DNS_CASE_SENSITIVE 0x00u
#define PICO_DNS_CASE_INSENSITIVE 0x01u

#define PICO_DNS_NAMEBUF_SIZE (256)

enum pico_dns_arpa
{
    PICO_DNS_ARPA4,
    PICO_DNS_ARPA6,
    PICO_DNS_NO_ARPA,
};

/* flags split in 2x uint8 due to endianness */
PACKED_STRUCT_DEF pico_dns_header
{
    uint16_t id;        /* Packet id */
    uint8_t rd : 1;     /* Recursion Desired */
    uint8_t tc : 1;     /* TrunCation */
    uint8_t aa : 1;     /* Authoritative Answer */
    uint8_t opcode : 4; /* Opcode */
    uint8_t qr : 1;     /* Query/Response */
    uint8_t rcode : 4;  /* Response code */
    uint8_t z : 3;      /* Zero */
    uint8_t ra : 1;     /* Recursion Available */
    uint16_t qdcount;   /* Question count */
    uint16_t ancount;   /* Answer count */
    uint16_t nscount;   /* Authority count */
    uint16_t arcount;   /* Additional count */
};
typedef struct pico_dns_header pico_dns_packet;

/* Question fixed-sized fields */
PACKED_STRUCT_DEF pico_dns_question_suffix
{
    uint16_t qtype;
    uint16_t qclass;
};

/* Resource record fixed-sized fields */
PACKED_STRUCT_DEF pico_dns_record_suffix
{
    uint16_t rtype;
    uint16_t rclass;
    uint32_t rttl;
    uint16_t rdlength;
};

/* DNS QUESTION */
struct pico_dns_question
{
    char *qname;
    struct pico_dns_question_suffix *qsuffix;
    uint16_t qname_length;
    uint8_t proto;
};

/* DNS RECORD */
struct pico_dns_record
{
    char *rname;
    struct pico_dns_record_suffix *rsuffix;
    uint8_t *rdata;
    uint16_t rname_length;
};

/* MARK: v NAME & IP FUNCTIONS */

/* ****************************************************************************
 *  Checks if the DNS name doesn't exceed 256 bytes including zero-byte.
 *
 *  @param namelen Length of the DNS name-string including zero-byte
 *  @return 0 when the length is correct
 * ****************************************************************************/
int
pico_dns_check_namelen( uint16_t namelen );

/* ****************************************************************************
 *  Returns the length of a name in a DNS-packet as if DNS name compression
 *  would be applied to the packet. If there's no compression present this
 *	returns the strlen. If there's compression present this returns the length
 *	until the compression-pointer + 1.
 *
 *  @param name Compressed name you want the calculate the strlen from
 *  @return Returns strlen of a compressed name, takes the first byte of compr-
 *			ession pointer into account but not the second byte, which acts
 *			like a trailing zero-byte.
 * ****************************************************************************/
uint16_t
pico_dns_namelen_comp( char *name );

/* ****************************************************************************
 *  Returns the uncompressed name in DNS name format when DNS name compression
 *  is applied to the packet-buffer.
 *
 *  @param name   Compressed name, should be in the bounds of the actual packet
 *  @param packet Packet that contains the compressed name
 *  @return Returns the decompressed name, NULL on failure.
 * ****************************************************************************/
char *
pico_dns_decompress_name( char *name, pico_dns_packet *packet );

/* ****************************************************************************
 *  Converts a DNS name in DNS name format to a name in URL format. Provides
 *  space for the name in URL format as well. PICO_FREE() should be called on
 *  the returned string buffer that contains the name in URL format.
 *
 *  @param qname DNS name in DNS name format to convert
 *  @return Returns a pointer to a string-buffer with the URL name on success.
 * ****************************************************************************/
char *
pico_dns_qname_to_url( const char *qname );

/* ****************************************************************************
 *  Converts a DNS name in URL format to name in DNS name format. Provides
 *  space for the DNS name as well. PICO_FREE() should be called on the returned
 *  string buffer that contains the DNS name.
 *
 *  @param url DNS name in URL format to convert
 *  @return Returns a pointer to a string-buffer with the DNS name on success.
 * ****************************************************************************/
char *
pico_dns_url_to_qname( const char *url );

/* ****************************************************************************
 *  @param url String-buffer
 *  @return Length of string-buffer in an uint16_t
 * ****************************************************************************/
uint16_t
pico_dns_strlen( const char *url );

/* ****************************************************************************
 *  Replaces .'s in a DNS name in URL format by the label lengths. So it
 *  actually converts a name in URL format to a name in DNS name format.
 *  f.e. "*www.google.be" => "3www6google2be0"
 *
 *  @param url    Location to buffer with name in URL format. The URL needs to
 *                be +1 byte offset in the actual buffer. Size is should be
 *                strlen(url) + 2.
 *  @param maxlen Maximum length of buffer so it doesn't cause a buffer overflow
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
int pico_dns_name_to_dns_notation( char *url, uint16_t maxlen );

/* ****************************************************************************
 *  Replaces the label lengths in a DNS-name by .'s. So it actually converts a
 *  name in DNS format to a name in URL format.
 *  f.e. 3www6google2be0 => .www.google.be
 *
 *  @param ptr    Location to buffer with name in DNS name format
 *  @param maxlen Maximum length of buffer so it doesn't cause a buffer overflow
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
int pico_dns_notation_to_name( char *ptr, uint16_t maxlen );

/* ****************************************************************************
 *  Determines the length of the first label of a DNS name in URL-format
 *
 *  @param url DNS name in URL-format
 *  @return Length of the first label of DNS name in URL-format
 * ****************************************************************************/
uint16_t
pico_dns_first_label_length( const char *url );

/* ****************************************************************************
 *  Mirrors a dotted IPv4-address string.
 *	f.e. 192.168.0.1 => 1.0.168.192
 *
 *  @param ptr
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_mirror_addr( char *ptr );

/* ****************************************************************************
 *  Convert an IPv6-address in string-format to a IPv6-address in nibble-format.
 *	Doesn't add a IPv6 ARPA-suffix though.
 *
 *  @param ip  IPv6-address stored as a string
 *  @param dst Destination to store IPv6-address in nibble-format
 * ****************************************************************************/
void
pico_dns_ipv6_set_ptr( const char *ip, char *dst );

/* MARK: QUESTION FUNCTIONS */

/* ****************************************************************************
 *  Deletes a single DNS Question.
 *
 *  @param question Void-pointer to DNS Question. Can be used with pico_tree_-
 *					destroy.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_question_delete( void **question);

/* ****************************************************************************
 *  Fills in the DNS question suffix-fields with the correct values.
 *
 *  todo: Update pico_dns_client to make the same mechanism possible as with
 *        filling DNS Resource Record-suffixes. This function shouldn't be an
 *		  API-function.
 *
 *  @param suf    Pointer to the suffix member of the DNS question.
 *  @param qtype  DNS type of the DNS question to be.
 *  @param qclass DNS class of the DNS question to be.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_question_fill_suffix( struct pico_dns_question_suffix *suf,
                               uint16_t qtype,
                               uint16_t qclass );

/* ****************************************************************************
 *  Creates a standalone DNS Question with a given name and type.
 *
 *  @param url     DNS question name in URL format. Will be converted to DNS
 *				   name notation format.
 *  @param len     Will be filled with the total length of the DNS question.
 *  @param proto   Protocol for which you want to create a question. Can be
 *				   either PICO_PROTO_IPV4 or PICO_PROTO_IPV6.
 *  @param qtype   DNS type of the question to be.
 *  @param qclass  DNS class of the question to be.
 *  @param reverse When this is true, a reverse resolution name will be gene-
 *				   from the URL
 *  @return Returns pointer to the created DNS Question on success, NULL on
 *			failure.
 * ****************************************************************************/
struct pico_dns_question *
pico_dns_question_create( const char *url,
                          uint16_t *len,
                          uint8_t proto,
                          uint16_t qtype,
                          uint16_t qclass,
                          uint8_t reverse );

/* ****************************************************************************
 *  Decompresses the name of a single DNS question.
 *
 *  @param question Question you want to decompress the name of
 *  @param packet   Packet in which the DNS question is contained.
 *  @return Pointer to original name of the DNS question before decompressing.
 * ****************************************************************************/
char *
pico_dns_question_decompress( struct pico_dns_question *question,
                              pico_dns_packet *packet );

/* MARK: RESOURCE RECORD FUNCTIONS */

/* ****************************************************************************
 *  Deletes a single DNS resource record.
 *
 *  @param record Void-pointer to DNS record. Can be used with pico_tree_destroy
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_record_delete( void **record );

/* ****************************************************************************
 *  Just makes a hardcopy from a single DNS Resource Record
 *
 *  @param record DNS record you want to copy
 *  @return Pointer to copy of DNS record.
 * ****************************************************************************/
struct pico_dns_record *
pico_dns_record_copy( struct pico_dns_record *record );

/* ****************************************************************************
 *  Create a standalone DNS Resource Record with given name, type and data.
 *
 *  @param url     DNS rrecord name in URL format. Will be converted to DNS
 *                 name notation format.
 *  @param _rdata  Memory buffer with data to insert in the resource record. If
 *				   data of record should contain a DNS name, the name in the
 *				   databuffer needs to be in URL-format.
 *  @param datalen The exact length in bytes of the _rdata-buffer. If data of
 *				   record should contain a DNS name, datalen needs to be
 *				   pico_dns_strlen(_rdata).
 *  @param len     Will be filled with the total length of the DNS rrecord.
 *  @param rtype   DNS type of the resource record to be.
 *  @param rclass  DNS class of the resource record to be.
 *  @param rttl    DNS ttl of the resource record to be.
 *  @return Returns pointer to the created DNS Resource Record
 * ****************************************************************************/
struct pico_dns_record *
pico_dns_record_create( const char *url,
                        void *_rdata,
                        uint16_t datalen,
                        uint16_t *len,
                        uint16_t rtype,
                        uint16_t rclass,
                        uint32_t rttl );

/* ****************************************************************************
 *  Decompresses the name of single DNS record.
 *
 *  @param record DNS record to decompress the name of.
 *  @param packet Packet in which is DNS record is present
 *  @return Pointer to original name of the DNS record before decompressing.
 * ****************************************************************************/
char *
pico_dns_record_decompress( struct pico_dns_record *record,
                            pico_dns_packet *packet );

/* MARK: COMPARING */

/* ****************************************************************************
 *  Compares two databuffers against each other.
 *
 *  @param a          1st Memory buffer to compare
 *  @param b          2nd Memory buffer to compare
 *  @param rdlength_a Length of 1st memory buffer
 *  @param rdlength_b Length of 2nd memory buffer
 *  @param caseinsensitive Whether or not the bytes are compared
 *                         case-insensitive. Should be either
 *                         PICO_DNS_CASE_SENSITIVE or PICO_DNS_CASE_INSENSITIVE
 *  @return 0 when the buffers are equal, returns difference when they're not.
 * ****************************************************************************/
int
pico_dns_rdata_cmp( uint8_t *a, uint8_t *b,
                    uint16_t rdlength_a, uint16_t rdlength_b, uint8_t caseinsensitive );

/* ****************************************************************************
 *  Compares 2 DNS questions
 *
 *  @param qa DNS question A as a void-pointer (for pico_tree)
 *  @param qb DNS question A as a void-pointer (for pico_tree)
 *  @return 0 when questions are equal, returns difference when they're not.
 * ****************************************************************************/
int
pico_dns_question_cmp( void *qa,
                       void *qb );

/* ****************************************************************************
 *  Compares 2 DNS records by type and name only
 *
 *  @param ra DNS record A as a void-pointer (for pico_tree)
 *  @param rb DNS record B as a void-pointer (for pico_tree)
 *  @return 0 when name and type of records are equal, returns difference when
 *			they're not.
 * ****************************************************************************/
int
pico_dns_record_cmp_name_type( void *ra,
                               void *rb );

/* ****************************************************************************
 *  Compares 2 DNS records by type, name AND rdata for a truly unique result
 *
 *  @param ra DNS record A as a void-pointer (for pico_tree)
 *  @param rb DNS record B as a void-pointer (for pico_tree)
 *  @return 0 when records are equal, returns difference when they're not
 * ****************************************************************************/
int
pico_dns_record_cmp( void *ra,
                     void *rb );

/* MARK: PICO_TREE */

/* ****************************************************************************
 *  Erases a pico_tree entirely.
 *
 *  @param tree        Pointer to a pico_tree-instance
 *  @param node_delete Helper-function for type-specific deleting.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_tree_destroy( struct pico_tree *tree, int (*node_delete)(void **));

/* ****************************************************************************
 *  Determines the amount of nodes in a pico_tree
 *
 *  @param tree Pointer to pico_tree-instance
 *  @return Amount of items in the tree.
 * ****************************************************************************/
uint16_t
pico_tree_count( struct pico_tree *tree );

/* ****************************************************************************
 *  Definition of DNS question tree
 * ****************************************************************************/
typedef struct pico_tree pico_dns_qtree;
#define PICO_DNS_QTREE_DECLARE(name) \
    pico_dns_qtree (name) = {&LEAF, pico_dns_question_cmp}
#define PICO_DNS_QTREE_DESTROY(qtree) \
    pico_tree_destroy(qtree, pico_dns_question_delete)

/* ****************************************************************************
 *  Deletes all the questions with given DNS name from a pico_tree
 *
 *  @param qtree Pointer to pico_tree-instance which contains DNS questions
 *  @param name  Name of the questions you want to delete
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_qtree_del_name( struct pico_tree *qtree,
                         const char *name );

/* ****************************************************************************
 *  Checks whether a question with given name is in the tree or not.
 *
 *  @param qtree Pointer to pico_tree-instance which contains DNS questions
 *  @param name  Name you want to check for
 *  @return 1 when the name is present in the qtree, 0 when it's not.
 * ****************************************************************************/
int
pico_dns_qtree_find_name( struct pico_tree *qtree,
                          const char *name );

/* ****************************************************************************
 *  Definition of DNS record tree
 * ****************************************************************************/
typedef struct pico_tree pico_dns_rtree;
#define PICO_DNS_RTREE_DECLARE(name) \
    pico_dns_rtree (name) = {&LEAF, pico_dns_record_cmp}
#define PICO_DNS_RTREE_DESTROY(rtree) \
    pico_tree_destroy((rtree), pico_dns_record_delete)

/* MARK: DNS PACKET FUNCTIONS */

/* ****************************************************************************
 *  Fills the header section of a DNS packet with the correct flags and section
 *  -counts.
 *
 *  @param hdr     Header to fill in.
 *  @param qdcount Amount of questions added to the packet
 *  @param ancount Amount of answer records added to the packet
 *  @param nscount Amount of authority records added to the packet
 *  @param arcount Amount of additional records added to the packet
 * ****************************************************************************/
void
pico_dns_fill_packet_header( struct pico_dns_header *hdr,
                             uint16_t qdcount,
                             uint16_t ancount,
                             uint16_t authcount,
                             uint16_t addcount );

/* ****************************************************************************
 *  Creates a DNS Query packet with given question and resource records to put
 *  the Resource Record Sections. If a NULL-pointer is provided for a certain
 *  tree, no records will be added to that particular section of the packet.
 *
 *  @param qtree  DNS Questions to put in the Question Section
 *  @param antree DNS Records to put in the Answer Section
 *  @param nstree DNS Records to put in the Authority Section
 *  @param artree DNS Records to put in the Additional Section
 *  @param len    Will get filled with the entire size of the packet
 *  @return Pointer to created DNS packet
 * ****************************************************************************/
pico_dns_packet *
pico_dns_query_create( struct pico_tree *qtree,
                       struct pico_tree *antree,
                       struct pico_tree *nstree,
                       struct pico_tree *artree,
                       uint16_t *len );

/* ****************************************************************************
 *  Creates a DNS Answer packet with given resource records to put in the
 *  Resource Record Sections. If a NULL-pointer is provided for a certain tree,
 *  no records will be added to that particular section of the packet.
 *
 *  @param antree DNS Records to put in the Answer Section
 *  @param nstree DNS Records to put in the Authority Section
 *  @param artree DNS Records to put in the Additional Section
 *  @param len    Will get filled with the entire size of the packet
 *  @return Pointer to created DNS packet.
 * ****************************************************************************/
pico_dns_packet *
pico_dns_answer_create( struct pico_tree *antree,
                        struct pico_tree *nstree,
                        struct pico_tree *artree,
                        uint16_t *len );

#endif /* _INCLUDE_PICO_DNS_COMMON */
