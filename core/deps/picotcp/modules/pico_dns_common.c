/* ****************************************************************************
 *  PicoTCP. Copyright (c) 2012 TASS Belgium NV. Some rights reserved.
 *  See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 *  .
 *  Authors: Toon Stegen, Jelle De Vleeschouwer
 * ****************************************************************************/
#include "pico_config.h"
#include "pico_protocol.h"
#include "pico_stack.h"
#include "pico_addressing.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_dns_common.h"
#include "pico_tree.h"

#ifdef DEBUG_DNS
    #define dns_dbg dbg
#else
    #define dns_dbg(...) do {} while(0)
#endif

/* MARK: v NAME & IP FUNCTIONS */
#define dns_name_foreach_label_safe(label, name, next, maxlen) \
    for ((label) = (name), (next) = (char *)((name) + *(unsigned char*)(name) + 1); \
         (*(label) != '\0') && ((uint16_t)((label) - (name)) < (maxlen)); \
         (label) = (next), (next) = (char *)((next) + *(unsigned char*)(next) + 1))

/* ****************************************************************************
 *  Checks if the DNS name doesn't exceed 256 bytes including zero-byte.
 *
 *  @param namelen Length of the DNS name-string including zero-byte
 *  @return 0 when the length is correct
 * ****************************************************************************/
int
pico_dns_check_namelen( uint16_t namelen )
{
    return ((namelen > 2u) && (namelen < 256u)) ? (0) : (-1);
}

/* ****************************************************************************
 *  Returns the length of a name in a DNS-packet as if DNS name compression
 *  would be applied to the packet. If there's no compression present
 *
 *  @param name Compressed name you want the calculate the strlen from
 *  @return Returns strlen of a compressed name, takes the first byte of compr-
 *			ession pointer into account but not the second byte, which acts
 *			like a trailing zero-byte
 * ****************************************************************************/
uint16_t
pico_dns_namelen_comp( char *name )
{
    uint16_t len = 0;
    char *label = NULL, *next = NULL;

    /* Check params */
    if (!name) {
        pico_err = PICO_ERR_EINVAL;
        return 0;
    }

    /* Just count until the zero-byte or a pointer */
    dns_name_foreach_label_safe(label, name, next, 255) {
        if ((0xC0 & *label))
            break;
    }

    /* Calculate the length */
    len = (uint16_t)(label - name);
    if(*label != '\0')
        len++;

    return len;
}

/* ****************************************************************************
 *  Returns the uncompressed name in DNS name format when DNS name compression
 *  is applied to the packet-buffer.
 *
 *  @param name   Compressed name, should be in the bounds of the actual packet
 *  @param packet Packet that contains the compressed name
 *  @return Returns the decompressed name, NULL on failure.
 * ****************************************************************************/
char *
pico_dns_decompress_name( char *name, pico_dns_packet *packet )
{
    char decompressed_name[PICO_DNS_NAMEBUF_SIZE] = {
        0
    };
    char *return_name = NULL;
    uint8_t *dest_iterator = NULL;
    uint8_t *iterator = NULL;
    uint16_t ptr = 0, nslen = 0;

    /* Initialise iterators */
    iterator = (uint8_t *) name;
    dest_iterator = (uint8_t *) decompressed_name;
    while (*iterator != '\0') {
        if ((*iterator) & 0xC0) {
            /* We have a pointer */
            ptr = (uint16_t)((((uint16_t) *iterator) & 0x003F) << 8);
            ptr = (uint16_t)(ptr | (uint16_t) *(iterator + 1));
            iterator = (uint8_t *)((uint8_t *)packet + ptr);
        } else {
            /* We want to keep the label lengths */
            *dest_iterator = (uint8_t) *iterator;
            /* Copy the label */
            memcpy(dest_iterator + 1, iterator + 1, *iterator);
            /* Move to next length label */
            dest_iterator += (*iterator) + 1;
            iterator += (*iterator) + 1;
        }
    }
    /* Append final zero-byte */
    *dest_iterator = (uint8_t) '\0';

    /* Provide storage for the name to return */
    nslen = (uint16_t)(pico_dns_strlen(decompressed_name) + 1);
    if(!(return_name = PICO_ZALLOC((size_t)nslen))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    memcpy((void *)return_name, (void *)decompressed_name, (size_t)nslen);

    return return_name;
}

/* ****************************************************************************
 *  Determines the length of a given url as if it where a DNS name in reverse
 *  resolution format.
 *
 *  @param url     URL wanted to create a reverse resolution name from.
 *  @param arpalen Will get filled with the length of the ARPA-suffix depending
 *                 on the proto-parameter.
 *  @param proto   The protocol to create a ARPA-suffix for. Can be either
 *				   'PICO_PROTO_IPV4' or 'PICO_PROTO_IPV6'
 *  @return Returns the length of the reverse name
 * ****************************************************************************/
static uint16_t
pico_dns_url_get_reverse_len( const char *url,
                              uint16_t *arpalen,
                              uint16_t proto )
{
    uint16_t slen = (uint16_t)(pico_dns_strlen(url) + 2u);

    /* Check if pointers given are not NULL */
    if (pico_dns_check_namelen(slen) && !arpalen) {
        pico_err = PICO_ERR_EINVAL;
        return 0;
    }

    /* Get the length of arpa-suffix if needed */
    if (proto == PICO_PROTO_IPV4)
        *arpalen = (uint16_t) pico_dns_strlen(PICO_ARPA_IPV4_SUFFIX);

#ifdef PICO_SUPPORT_IPV6
    else if (proto == PICO_PROTO_IPV6)
    {
        *arpalen = (uint16_t) pico_dns_strlen(PICO_ARPA_IPV6_SUFFIX);
        slen = STRLEN_PTR_IP6 + 2u;
    }
#endif
    return slen;
}

/* ****************************************************************************
 *  Converts a DNS name in URL format to a reverse name in DNS name format.
 *  Provides space for the DNS name as well. PICO_FREE() should be called on the
 *  returned string buffer that contains the reverse DNS name.
 *
 *  @param url   DNS name in URL format to convert to reverse name
 *  @param proto Depending on the protocol given the ARPA-suffix will be added.
 *  @return Returns a pointer to a string-buffer with the reverse DNS name.
 * ****************************************************************************/
static char *
pico_dns_url_to_reverse_qname( const char *url, uint8_t proto )
{
    char *reverse_qname = NULL;
    uint16_t arpalen = 0;
    uint16_t slen = pico_dns_url_get_reverse_len(url, &arpalen, proto);

    /* Check namelen */
    if (pico_dns_check_namelen(slen)) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Provide space for the reverse name */
    if (!(reverse_qname = PICO_ZALLOC((size_t)(slen + arpalen)))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* If reverse IPv4 address resolving, convert to IPv4 arpa-format */
    if (PICO_PROTO_IPV4 == proto) {
        memcpy(reverse_qname + 1u, url, slen - 1u);
        pico_dns_mirror_addr(reverse_qname + 1u);
        memcpy(reverse_qname + slen - 1, PICO_ARPA_IPV4_SUFFIX, arpalen);
    }

    /* If reverse IPv6 address resolving, convert to IPv6 arpa-format */
#ifdef PICO_SUPPORT_IPV6
    else if (proto == PICO_PROTO_IPV6) {
        pico_dns_ipv6_set_ptr(url, reverse_qname + 1u);
        memcpy(reverse_qname + 1u + STRLEN_PTR_IP6,
               PICO_ARPA_IPV6_SUFFIX, arpalen);
    }
#endif
    else { /* This shouldn't happen */
        PICO_FREE(reverse_qname);
        return NULL;
    }

    pico_dns_name_to_dns_notation(reverse_qname, (uint16_t)(slen + arpalen));
    return reverse_qname;
}

/* ****************************************************************************
 *  Converts a DNS name in DNS name format to a name in URL format. Provides
 *  space for the name in URL format as well. PICO_FREE() should be called on
 *  the returned string buffer that contains the name in URL format.
 *
 *  @param qname DNS name in DNS name format to convert
 *  @return Returns a pointer to a string-buffer with the URL name on success.
 * ****************************************************************************/
char *
pico_dns_qname_to_url( const char *qname )
{
    char *url = NULL;
    char temp[256] = {
        0
    };
    uint16_t namelen = pico_dns_strlen(qname);

    /* Check if qname is not a NULL-pointer and if the length is OK */
    if (pico_dns_check_namelen(namelen)) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Provide space for the URL */
    if (!(url = PICO_ZALLOC(namelen))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Convert qname to an URL */
    memcpy(temp, qname, namelen);
    pico_dns_notation_to_name(temp, namelen);
    memcpy((void *)url, (void *)(temp + 1), (size_t)(namelen - 1));

    return url;
}

/* ****************************************************************************
 *  Converts a DNS name in URL format to a name in DNS name format. Provides
 *  space for the DNS name as well. PICO_FREE() should be called on the returned
 *  string buffer that contains the DNS name.
 *
 *  @param url DNS name in URL format to convert
 *  @return Returns a pointer to a string-buffer with the DNS name on success.
 * ****************************************************************************/
char *
pico_dns_url_to_qname( const char *url )
{
    char *qname = NULL;
    uint16_t namelen = (uint16_t)(pico_dns_strlen(url) + 2u);

    /* Check if url or qname_addr is not a NULL-pointer */
    if (pico_dns_check_namelen(namelen)) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Provide space for the qname */
    if (!(qname = PICO_ZALLOC(namelen))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Copy in the URL (+1 to leave space for leading '.') */
    memcpy(qname + 1, url, (size_t)(namelen - 1));
    pico_dns_name_to_dns_notation(qname, namelen);
    return qname;
}

/* ****************************************************************************
 *  @param url String-buffer
 *  @return Length of string-buffer in an uint16_t
 * ****************************************************************************/
uint16_t
pico_dns_strlen( const char *url )
{
    if (!url)
        return 0;

    return (uint16_t) strlen(url);
}

/* ****************************************************************************
 *  Replaces .'s in a DNS name in URL format by the label lengths. So it
 *  actually converts a name in URL format to a name in DNS name format.
 *  f.e. "*www.google.be" => "3www6google2be0"
 *
 *  @param url    Location to buffer with name in URL format. The URL needs to
 *                be +1 byte offset in the actual buffer. Size is should be
 *                pico_dns_strlen(url) + 2.
 *  @param maxlen Maximum length of buffer so it doesn't cause a buffer overflow
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
int pico_dns_name_to_dns_notation( char *url, uint16_t maxlen )
{
    char c = '\0';
    char *lbl = url, *i = url;

    /* Check params */
    if (!url || pico_dns_check_namelen(maxlen)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Iterate over url */
    while ((c = *++i) != '\0') {
        if ('.' == c) {
            *lbl = (char)(i - lbl - 1);
            lbl = i;
        }

        if ((uint16_t)(i - url) > (uint16_t)maxlen) break;
    }
    *lbl = (char)(i - lbl - 1);

    return 0;
}

/* ****************************************************************************
 *  Replaces the label lengths in a DNS-name by .'s. So it actually converts a
 *  name in DNS format to a name in URL format.
 *  f.e. 3www6google2be0 => .www.google.be
 *
 *  @param ptr    Location to buffer with name in DNS name format
 *  @param maxlen Maximum length of buffer so it doesn't cause a buffer overflow
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
int pico_dns_notation_to_name( char *ptr, uint16_t maxlen )
{
    char *label = NULL, *next = NULL;

    /* Iterate safely over the labels and update each label */
    dns_name_foreach_label_safe(label, ptr, next, maxlen) {
        *label = '.';
    }

    return 0;
}

/* ****************************************************************************
 *  Determines the length of the first label of a DNS name in URL-format
 *
 *  @param url DNS name in URL-format
 *  @return Length of the first label of DNS name in URL-format
 * ****************************************************************************/
uint16_t
pico_dns_first_label_length( const char *url )
{
    const char *i = NULL;
    uint16_t len = 0;

    /* Check params */
    if (!url) return 0;

    /* Count */
    i = url;
    while (*i != '.' && *i != '\0') {
        ++i;
        ++len;
    }
    return len;
}

/* ****************************************************************************
 *  Mirrors a dotted IPv4-address string.
 *	f.e. 192.168.0.1 => 1.0.168.192
 *
 *  @param ptr
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_mirror_addr( char *ip )
{
    uint32_t addr = 0;

    /* Convert IPv4-string to network-order 32-bit number */
    if (pico_string_to_ipv4(ip, &addr) < 0)
        return -1;

    /* Mirror the 32-bit number */
    addr = (uint32_t)((uint32_t)((addr & (uint32_t)0xFF000000u) >> 24) |
                      (uint32_t)((addr & (uint32_t)0xFF0000u) >> 8) |
                      (uint32_t)((addr & (uint32_t)0xFF00u) << 8) |
                      (uint32_t)((addr & (uint32_t)0xFFu) << 24));

    return pico_ipv4_to_string(ip, addr);
}

#ifdef PICO_SUPPORT_IPV6
/* ****************************************************************************
 *  Get the ASCII value of the Most Significant Nibble of a byte
 *
 *  @param byte Byte you want to extract the MSN from.
 *  @return The ASCII value of the Most Significant Nibble of the byte
 * ****************************************************************************/
static inline char
dns_ptr_ip6_nibble_lo( uint8_t byte )
{
    uint8_t nibble = byte & 0x0f;
    if (nibble < 10)
        return (char)(nibble + '0');
    else
        return (char)(nibble - 0xa + 'a');
}

/* ****************************************************************************
 *  Get the ASCII value of the Least Significant Nibble of a byte
 *
 *  @param byte Byte you want to extract the LSN from.
 *  @return The ASCII value of the Least Significant Nibble of the byte
 * ****************************************************************************/
static inline char
dns_ptr_ip6_nibble_hi( uint8_t byte )
{
    uint8_t nibble = (byte & 0xf0u) >> 4u;
    if (nibble < 10u)
        return (char)(nibble + '0');
    else
        return (char)(nibble - 0xa + 'a');
}

/* ****************************************************************************
 *  Convert an IPv6-address in string-format to a IPv6-address in nibble-format.
 *	Doesn't add a IPv6 ARPA-suffix though.
 *
 *  @param ip  IPv6-address stored as a string
 *  @param dst Destination to store IPv6-address in nibble-format
 * ****************************************************************************/
void
pico_dns_ipv6_set_ptr( const char *ip, char *dst )
{
    int i = 0, j = 0;
    struct pico_ip6 ip6;
    memset(&ip6, 0, sizeof(struct pico_ip6));
    pico_string_to_ipv6(ip, ip6.addr);
    for (i = 15; i >= 0; i--) {
        if ((j + 3) > 64) return; /* Don't want j to go out of bounds */

        dst[j++] = dns_ptr_ip6_nibble_lo(ip6.addr[i]);
        dst[j++] = '.';
        dst[j++] = dns_ptr_ip6_nibble_hi(ip6.addr[i]);
        dst[j++] = '.';
    }
}
#endif

/* MARK: ^ NAME & IP FUNCTIONS */
/* MARK: v QUESTION FUNCTIONS */

/* ****************************************************************************
 *  Calculates the size of a single DNS Question. Void-pointer allows this
 *  function to be used with pico_tree_size.
 *
 *  @param question Void-point to DNS Question
 *  @return Size in bytes of single DNS Question if it was copied flat.
 * ****************************************************************************/
static uint16_t pico_dns_question_size( void *question )
{
    uint16_t size = 0;
    struct pico_dns_question *q = (struct pico_dns_question *)question;
    if (!q)
        return 0;

    size = q->qname_length;
    size = (uint16_t)(size + sizeof(struct pico_dns_question_suffix));
    return size;
}

/* ****************************************************************************
 *  Deletes a single DNS Question.
 *
 *  @param question Void-pointer to DNS Question. Can be used with pico_tree_-
 *					destroy.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_question_delete( void **question )
{
    struct pico_dns_question **q = (struct pico_dns_question **)question;

    /* Check params */
    if ((!q) || !(*q)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if ((*q)->qname)
        PICO_FREE(((*q)->qname));

    if ((*q)->qsuffix)
        PICO_FREE((*q)->qsuffix);

    PICO_FREE((*q));
    *question = NULL;

    return 0;
}

/* ****************************************************************************
 *  Fills in the DNS question suffix-fields with the correct values.
 *
 *  todo: Update pico_dns_client to make the same mechanism possible like with
 *        filling DNS Resource Record-suffixes.
 *
 *  @param suf    Pointer to the suffix member of the DNS question.
 *  @param qtype  DNS type of the DNS question to be.
 *  @param qclass DNS class of the DNS question to be.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_question_fill_suffix( struct pico_dns_question_suffix *suf,
                               uint16_t qtype,
                               uint16_t qclass )
{
    if (!suf)
        return -1;

    suf->qtype = short_be(qtype);
    suf->qclass = short_be(qclass);
    return 0;
}

/* ****************************************************************************
 *  Fills in the name of the DNS question.
 *
 *  @param qname   Pointer-pointer to the name-member of the DNS-question
 *  @param url     Name in URL format you want to convert to a name in DNS name
 *				   format. When reverse resolving, only the IP, either IPV4 or
 *				   IPV6, should be given in string format.
 *				   f.e. => for IPv4: "192.168.2.1"
 *						=> for IPv6: "2001:0db8:85a3:0042:1000:8a2e:0370:7334"
 *  @param qtype   DNS type type of the DNS question to be.
 *  @param proto   When reverse is true the reverse resolution name will be
 *				   generated depending on the protocol. Can be either
 *				   PICO_PROTO_IPV4 or PICO_PROTO_IPV6.
 *  @param reverse When this is true a reverse resolution name will be generated
 *				   from the URL.
 *  @return The eventual length of the generated name, 0 on failure.
 * ****************************************************************************/
static uint16_t
pico_dns_question_fill_name( char **qname,
                             const char *url,
                             uint16_t qtype,
                             uint8_t proto,
                             uint8_t reverse )
{
    uint16_t slen = 0;

    /* Try to convert the URL to an FQDN */
    if (reverse && qtype == PICO_DNS_TYPE_PTR)
        *qname = pico_dns_url_to_reverse_qname(url, proto);
    else {
        (*qname) = pico_dns_url_to_qname(url);
    }

    if (!(*qname)) {
        return 0;
    }

    slen = (uint16_t)(pico_dns_strlen(*qname) + 1u);
    return (pico_dns_check_namelen(slen)) ? ((uint16_t)0) : (slen);
}

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
 *  @param reverse When this is true, a reverse resolution name will be
 *				   generated from the URL
 *  @return Returns pointer to the created DNS Question on success, NULL on
 *			failure.
 * ****************************************************************************/
struct pico_dns_question *
pico_dns_question_create( const char *url,
                          uint16_t *len,
                          uint8_t proto,
                          uint16_t qtype,
                          uint16_t qclass,
                          uint8_t reverse )
{
    struct pico_dns_question *question = NULL;
    uint16_t slen = 0;
    int ret = 0;

    /* Check if valid arguments are provided */
    if (!url || !len) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Allocate space for the question and the subfields */
    if (!(question = PICO_ZALLOC(sizeof(struct pico_dns_question)))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Fill name field */
    slen = pico_dns_question_fill_name(&(question->qname), url,
                                       qtype, proto, reverse);
    question->qname_length = (uint8_t)(slen);
    question->proto = proto;

    /* Provide space for the question suffix & try to fill in */
    question->qsuffix = PICO_ZALLOC(sizeof(struct pico_dns_question_suffix));
    ret = pico_dns_question_fill_suffix(question->qsuffix, qtype, qclass);
    if (ret || pico_dns_check_namelen(slen)) {
        pico_dns_question_delete((void **)&question);
        return NULL;
    }

    /* Determine the entire length of the question */
    *len = (uint16_t)(slen + (uint16_t)sizeof(struct pico_dns_question_suffix));

    return question;
}

/* ****************************************************************************
 *  Decompresses the name of a single DNS question.
 *
 *  @param question Question you want to decompress the name of
 *  @param packet   Packet in which the DNS question is contained.
 *  @return Pointer to original name of the DNS question before decompressing.
 * ****************************************************************************/
char *
pico_dns_question_decompress( struct pico_dns_question *question,
                              pico_dns_packet *packet )
{
    char *qname_original = question->qname;

    /* Try to decompress the question name */
    if (!(question->qname = pico_dns_decompress_name(question->qname, packet))) {
        question->qname = qname_original;
    }

    return qname_original;
}


/* MARK: ^ QUESTION FUNCTIONS */
/* MARK: v RESOURCE RECORD FUNCTIONS */

/* ****************************************************************************
 *  Copies the contents of DNS Resource Record to a single flat memory-buffer.
 *
 *  @param record      Pointer to DNS record you want to copy flat.
 *  @param destination Pointer-pointer to flat memory buffer to copy DNS record
 *					   to. When function returns, this will point to location
 *					   right after the flat copied DNS Resource Record.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_dns_record_copy_flat( struct pico_dns_record *record,
                           uint8_t **destination )
{
    char *dest_rname = NULL; /* rname destination location */
    struct pico_dns_record_suffix *dest_rsuffix = NULL; /* rsuffix destin. */
    uint8_t *dest_rdata = NULL; /* rdata destination location */

    /* Check if there are no NULL-pointers given */
    if (!record || !destination || !(*destination)) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Initialise the destination pointers to the right locations */
    dest_rname = (char *) *destination;
    dest_rsuffix = (struct pico_dns_record_suffix *)
                   (dest_rname + record->rname_length);
    dest_rdata = ((uint8_t *)dest_rsuffix +
                  sizeof(struct pico_dns_record_suffix));

    /* Copy the rname of the resource record into the flat location */
    strcpy(dest_rname, record->rname);

    /* Copy the question suffix fields */
    dest_rsuffix->rtype = record->rsuffix->rtype;
    dest_rsuffix->rclass = record->rsuffix->rclass;
    dest_rsuffix->rttl = record->rsuffix->rttl;
    dest_rsuffix->rdlength = record->rsuffix->rdlength;

    /* Copy the rdata of the resource */
    memcpy(dest_rdata, record->rdata, short_be(dest_rsuffix->rdlength));

    /* Point to location right after flat resource record */
    *destination = (uint8_t *)(dest_rdata +
                               short_be(record->rsuffix->rdlength));
    return 0;
}

/* ****************************************************************************
 *  Calculates the size of a single DNS Resource Record. Void-pointer allows
 *	this function to be used with pico_tree_size.
 *
 *  @param record void-pointer to DNS record you want to know the size of.
 *  @return Size of single DNS record if it was copied flat.
 * ****************************************************************************/
static uint16_t
pico_dns_record_size( void *record )
{
    uint16_t size = 0;
    struct pico_dns_record *rr = (struct pico_dns_record *)record;

    if (!rr || !(rr->rsuffix))
        return 0;

    size = rr->rname_length;
    size = (uint16_t)(size + sizeof(struct pico_dns_record_suffix));
    size = (uint16_t)(size + short_be(rr->rsuffix->rdlength));
    return size;
}

/* ****************************************************************************
 *  Deletes a single DNS resource record. Void-pointer-pointer allows this
 *	function to be used with pico_tree_destroy.
 *
 *  @param record void-pointer-pointer to DNS record you want to delete.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_record_delete( void **record )
{
    struct pico_dns_record **rr = (struct pico_dns_record **)record;

    if ((!rr) || !(*rr))
        return 0;

    if ((*rr)->rname)
        PICO_FREE((*rr)->rname);

    if ((*rr)->rsuffix)
        PICO_FREE((*rr)->rsuffix);

    if ((*rr)->rdata)
        PICO_FREE((*rr)->rdata);

    PICO_FREE((*rr));
    *record = NULL;

    return 0;
}

/* ****************************************************************************
 *  Just copies a resource record hard.
 *
 *  @param record DNS record you want to copy
 *  @return Pointer to copy of DNS record.
 * ****************************************************************************/
struct pico_dns_record *
pico_dns_record_copy( struct pico_dns_record *record )
{
    struct pico_dns_record *copy = NULL;

    /* Check params */
    if (!record || !(record->rname) || !(record->rdata) || !(record->rsuffix)) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Provide space for the copy */
    if (!(copy = PICO_ZALLOC(sizeof(struct pico_dns_record)))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Provide space for the subfields */
    copy->rname = PICO_ZALLOC((size_t)record->rname_length);
    copy->rsuffix = PICO_ZALLOC(sizeof(struct pico_dns_record_suffix));
    copy->rdata = PICO_ZALLOC((size_t)short_be(record->rsuffix->rdlength));
    if (!(copy->rname) || !(copy->rsuffix) || !(copy->rdata)) {
        pico_dns_record_delete((void **)&copy);
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Fill in the rname field */
    memcpy((void *)(copy->rname), (void *)(record->rname),
           (size_t)(record->rname_length));
    copy->rname_length = record->rname_length;

    /* Fill in the rsuffix fields */
    copy->rsuffix->rtype = record->rsuffix->rtype;
    copy->rsuffix->rclass = record->rsuffix->rclass;
    copy->rsuffix->rttl = record->rsuffix->rttl;
    copy->rsuffix->rdlength = record->rsuffix->rdlength;

    /* Fill in the rdata field */
    memcpy(copy->rdata, record->rdata, short_be(record->rsuffix->rdlength));

    return copy;
}

/* ****************************************************************************
 *  Fills in the DNS resource record suffix-fields with the correct values.
 *
 *  @param suf      Pointer-pointer to rsuffix-member of struct pico_dns_record.
 *  @param rtype    DNS type of the resource record to be.
 *  @param rclass   DNS class of the resource record to be.
 *  @param rttl     DNS ttl of the resource record to be.
 *  @param rdlength DNS rdlength of the resource record to be.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_dns_record_fill_suffix( struct pico_dns_record_suffix **suf,
                             uint16_t rtype,
                             uint16_t rclass,
                             uint32_t rttl,
                             uint16_t rdlength )
{
    /* Try to provide space for the rsuffix */
    if (!(*suf = PICO_ZALLOC(sizeof(struct pico_dns_record_suffix)))) {
        pico_err = PICO_ERR_ENOMEM;
        return -1;
    }

    /* Fill in the fields */
    (*suf)->rtype = short_be(rtype);
    (*suf)->rclass = short_be(rclass);
    (*suf)->rttl = long_be(rttl);
    (*suf)->rdlength = short_be(rdlength);

    return 0;
}

/* ****************************************************************************
 *  Fills the data-buffer of a DNS resource record.
 *
 *  @param rdata   Pointer-pointer to rdata-member of struct pico_dns_record.
 *  @param _rdata  Memory buffer with data to insert in the resource record. If
 *				   data should contain a DNS name, the name in the databuffer
 *				   needs to be in URL-format.
 *  @param datalen The exact length in bytes of the _rdata-buffer. If data of
 *				   record should contain a DNS name, datalen needs to be
 *				   pico_dns_strlen(_rdata).
 *  @param rtype   DNS type of the resource record to be
 *  @return Returns 0 on failure, length of filled in rdata-member on success.
 *			Can differ from datalen-param because of URL to DNS Name conversion.
 * ****************************************************************************/
static uint16_t
pico_dns_record_fill_rdata( uint8_t **rdata,
                            void *_rdata,
                            uint16_t datalen,
                            uint16_t rtype )
{
    uint16_t _datalen = 0;

    /* If type is PTR, rdata will be a DNS name in URL format */
    if (rtype == PICO_DNS_TYPE_PTR) {
        _datalen = (uint16_t)(datalen + 2u);
        if (!(*rdata = (uint8_t *)pico_dns_url_to_qname(_rdata))) {
            pico_err = PICO_ERR_ENOMEM;
            return 0;
        }
    } else {
        /* Otherwise just copy in the databuffer */
        if (datalen == 0) {
            return datalen;
        }

        _datalen = datalen;
        if (!(*rdata = (uint8_t *)PICO_ZALLOC((size_t)datalen))) {
            pico_err = PICO_ERR_ENOMEM;
            return 0;
        }

        memcpy((void *)*rdata, (void *)_rdata, datalen);
    }

    return _datalen;
}

/* ****************************************************************************
 *  Create a standalone DNS Resource Record with a given name.
 *
 *  @param url     DNS rrecord name in URL format. Will be converted to DNS
 *                 name notation format.
 *  @param _rdata  Memory buffer with data to insert in the resource record. If
 *				   data should contain a DNS name, the name in the databuffer
 *				   needs to be in URL-format.
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
                        uint32_t rttl )
{
    struct pico_dns_record *record = NULL;
    uint16_t slen = (uint16_t)(pico_dns_strlen(url) + 2u);
    int ret = 0;

    /* Check params */
    if (pico_dns_check_namelen(slen) || !_rdata || !len) {
        pico_err = PICO_ERR_EINVAL;
        return NULL;
    }

    /* Allocate space for the record and subfields */
    if (!(record = PICO_ZALLOC(sizeof(struct pico_dns_record)))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Provide space and convert the URL to a DNS name */
    record->rname = pico_dns_url_to_qname(url);
    record->rname_length = slen;

    /* Provide space & fill in the rdata field */
    datalen = pico_dns_record_fill_rdata(&(record->rdata), _rdata,
                                         datalen, rtype);

    /* Provide space & fill in the rsuffix */
    ret = pico_dns_record_fill_suffix(&(record->rsuffix), rtype, rclass, rttl,
                                      datalen);

    /* Check if everything succeeded */
    if (!(record->rname) || ret) {
        pico_dns_record_delete((void **)&record);
        return NULL;
    }

    /* Determine the complete length of resource record */
    *len = (uint16_t)(slen + sizeof(struct pico_dns_record_suffix) + datalen);
    return record;
}

/* ****************************************************************************
 *  Decompresses the name of single DNS record.
 *
 *  @param record DNS record to decompress the name of.
 *  @param packet Packet in which is DNS record is present
 *  @return Pointer to original name of the DNS record before decompressing.
 * ****************************************************************************/
char *
pico_dns_record_decompress( struct pico_dns_record *record,
                            pico_dns_packet *packet )
{
    char *rname_original = record->rname;

    /* Try to decompress the record name */
    if (!(record->rname = pico_dns_decompress_name(record->rname, packet))) {
        record->rname = rname_original;
    }

    return rname_original;
}

static int pico_tolower(int c)
{
    if ((c >= 'A')  && (c <= 'Z'))
        c += 'a' - 'A';

    return c;
}

/* MARK: ^ RESOURCE RECORD FUNCTIONS */
/* MARK: v COMPARING */

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
                    uint16_t rdlength_a, uint16_t rdlength_b, uint8_t caseinsensitive )
{
    uint16_t i = 0;
    uint16_t slen = 0;
    int dif = 0;

    /* Check params */
    if (!a || !b) {
        if (!a && !b)
            return 0;

        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Determine the smallest length */
    slen = rdlength_a;
    if (rdlength_b < slen)
        slen = rdlength_b;

    /* loop over slen */
    if(caseinsensitive) {
        for (i = 0; i < slen; i++) {
            if ((dif = pico_tolower((int)a[i]) - pico_tolower((int)b[i]))) {
                return dif;
            }
        }
    }else{
        for (i = 0; i < slen; i++) {
            if ((dif = (int)a[i] - (int)b[i])) {
                return dif;
            }
        }
    }

    /* Return difference of buffer lengths */
    return (int)((int)rdlength_a - (int)rdlength_b);
}

/* ****************************************************************************
 *  Compares 2 DNS questions
 *
 *  @param qa DNS question A as a void-pointer (for pico_tree)
 *  @param qb DNS question A as a void-pointer (for pico_tree)
 *  @return 0 when questions are equal, returns difference when they're not.
 * ****************************************************************************/
int
pico_dns_question_cmp( void *qa,
                       void *qb )
{
    int dif = 0;
    uint16_t at = 0, bt = 0;
    struct pico_dns_question *a = (struct pico_dns_question *)qa;
    struct pico_dns_question *b = (struct pico_dns_question *)qb;

    /* Check params */
    if (!a || !b) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* First, compare the qtypes */
    at = short_be(a->qsuffix->qtype);
    bt = short_be(b->qsuffix->qtype);
    if ((dif = (int)((int)at - (int)bt)))
        return dif;

    /* Then compare qnames */
    return pico_dns_rdata_cmp((uint8_t *)a->qname, (uint8_t *)b->qname,
                              pico_dns_strlen(a->qname),
                              pico_dns_strlen(b->qname), PICO_DNS_CASE_INSENSITIVE);
}

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
                               void *rb )
{
    int dif;
    uint16_t at = 0, bt = 0;
    struct pico_dns_record *a = (struct pico_dns_record *)ra;
    struct pico_dns_record *b = (struct pico_dns_record *)rb;

    /* Check params */
    if (!a || !b) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* First, compare the rrtypes */
    at = short_be(a->rsuffix->rtype);
    bt = short_be(b->rsuffix->rtype);
    if ((dif = (int)((int)at - (int)bt)))
        return dif;

    /* Then compare names */
    return pico_dns_rdata_cmp((uint8_t *)(a->rname), (uint8_t *)(b->rname),
                              (uint16_t)strlen(a->rname),
                              (uint16_t)strlen(b->rname), PICO_DNS_CASE_INSENSITIVE);
}

/* ****************************************************************************
 *  Compares 2 DNS records by type, name AND rdata for a truly unique result
 *
 *  @param ra DNS record A as a void-pointer (for pico_tree)
 *  @param rb DNS record B as a void-pointer (for pico_tree)
 *  @return 0 when records are equal, returns difference when they're not
 * ****************************************************************************/
int
pico_dns_record_cmp( void *ra,
                     void *rb )
{
    int dif = 0;
    struct pico_dns_record *a = (struct pico_dns_record *)ra;
    struct pico_dns_record *b = (struct pico_dns_record *)rb;

    /* Check params */
    if (!a || !b) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Compare type and name */
    if ((dif = pico_dns_record_cmp_name_type(a, b)))
        return dif;

    /* Then compare rdata */
    return pico_dns_rdata_cmp(a->rdata, b->rdata,
                              short_be(a->rsuffix->rdlength),
                              short_be(b->rsuffix->rdlength), PICO_DNS_CASE_SENSITIVE);
}

/* MARK: ^ COMPARING */
/* MARK: v PICO_TREE */

/* ****************************************************************************
 *  Erases a pico_tree entirely.
 *
 *  @param tree        Pointer to a pico_tree-instance
 *  @param node_delete Helper-function for type-specific deleting.
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_tree_destroy( struct pico_tree *tree, int (*node_delete)(void **))
{
    struct pico_tree_node *node = NULL, *next = NULL;
    void *item = NULL;

    /* Check params */
    if (!tree) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    pico_tree_foreach_safe(node, tree, next) {
        item = node->keyValue;
        pico_tree_delete(tree, node->keyValue);
        if (item && node_delete) {
            node_delete((void **)&item);
        }
    }

    return 0;
}

/* ****************************************************************************
 *  Calculates the size in bytes of all the nodes contained in the tree summed
 *  up. And gets the amount of items in the tree as well.
 *
 *  @param tree      Pointer to pico_tree-instance
 *  @param size      Will get filled with the size of all the nodes summed up.
 *					 Make sure you clear out (set to 0) this param before you
 *					 call this function because it doesn't happen inside and
 *					 each size will be added to the initial value.
 *  @param node_size Helper-function for type-specific size-determination
 *  @return Amount of items in the tree.
 * ****************************************************************************/
static uint16_t
pico_tree_size( struct pico_tree *tree,
                uint16_t *size,
                uint16_t (*node_size)(void *))
{
    struct pico_tree_node *node = NULL;
    void *node_item = NULL;
    uint16_t count = 0;

    /* Check params */
    if (!tree || !size) {
        pico_err = PICO_ERR_EINVAL;
        return 0;
    }

    /* Add up the node sizes */
    pico_tree_foreach(node, tree) {
        if ((node_item = node->keyValue)) {
            *size = (uint16_t)((*size) + node_size(node_item));
            count++;
        }
    }

    return count;
}

/* ****************************************************************************
 *  Determines the amount of nodes in a pico_tere
 *
 *  @param tree Pointer to pico_tree-instance
 *  @return Amount of items in the tree.
 * ****************************************************************************/
uint16_t
pico_tree_count( struct pico_tree *tree )
{
    struct pico_tree_node *node = NULL;
    uint16_t count = 0;

    pico_tree_foreach(node, tree) {
        if (node->keyValue)
            count++;
    }

    return count;
}

/* ****************************************************************************
 *  Deletes all the questions with given DNS name from a pico_tree
 *
 *  @param qtree Pointer to pico_tree-instance which contains DNS questions
 *  @param name  Name of the questions you want to delete
 *  @return Returns 0 on success, something else on failure.
 * ****************************************************************************/
int
pico_dns_qtree_del_name( struct pico_tree *qtree,
                         const char *name )
{
    struct pico_tree_node *node = NULL, *next = NULL;
    struct pico_dns_question *question = NULL;

    /* Check params */
    if (!qtree || !name) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Iterate over tree and delete every node with given name */
    pico_tree_foreach_safe(node, qtree, next) {
        question = (struct pico_dns_question *)node->keyValue;
        if ((question) && (strcasecmp(question->qname, name) == 0)) {
            question = pico_tree_delete(qtree, (void *)question);
            pico_dns_question_delete((void **)&question);
        }
    }

    return 0;
}

/* ****************************************************************************
 *  Checks whether a question with given name is in the tree or not.
 *
 *  @param qtree Pointer to pico_tree-instance which contains DNS questions
 *  @param name  Name you want to check for
 *  @return 1 when the name is present in the qtree, 0 when it's not.
 * ****************************************************************************/
int
pico_dns_qtree_find_name( struct pico_tree *qtree,
                          const char *name )
{
    struct pico_tree_node *node = NULL;
    struct pico_dns_question *question = NULL;

    /* Check params */
    if (!qtree || !name) {
        pico_err = PICO_ERR_EINVAL;
        return 0;
    }

    /* Iterate over tree and compare names */
    pico_tree_foreach(node, qtree) {
        question = (struct pico_dns_question *)node->keyValue;
        if ((question) && (strcasecmp(question->qname, name) == 0))
            return 1;
    }

    return 0;
}

/* MARK: ^ PICO_TREE */
/* MARK: v DNS PACKET FUNCTIONS */

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
                             uint16_t nscount,
                             uint16_t arcount )
{
    /* ID should be filled by caller */

    if(qdcount > 0) { /* Questions present? Make it a query */
        hdr->qr = PICO_DNS_QR_QUERY;
        hdr->aa = PICO_DNS_AA_NO_AUTHORITY;
    } else { /* No questions present? Make it an answer*/
        hdr->qr = PICO_DNS_QR_RESPONSE;
        hdr->aa = PICO_DNS_AA_IS_AUTHORITY;
    }

    /* Fill in the flags and the fields */
    hdr->opcode = PICO_DNS_OPCODE_QUERY;
    hdr->tc = PICO_DNS_TC_NO_TRUNCATION;
    hdr->rd = PICO_DNS_RD_IS_DESIRED;
    hdr->ra = PICO_DNS_RA_NO_SUPPORT;
    hdr->z = 0; /* Z, AD, CD are 0 */
    hdr->rcode = PICO_DNS_RCODE_NO_ERROR;
    hdr->qdcount = short_be(qdcount);
    hdr->ancount = short_be(ancount);
    hdr->nscount = short_be(nscount);
    hdr->arcount = short_be(arcount);
}

/* ****************************************************************************
 *  Fills a single DNS resource record section of a DNS packet.
 *
 *  @param rtree Tree that contains the DNS resource records.
 *  @param dest  Pointer-pointer to location where you want to insert records.
 *				 Will point to location after current section on return.
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_dns_fill_packet_rr_section( struct pico_tree *rtree,
                                 uint8_t **dest )
{
    struct pico_tree_node *node = NULL;
    struct pico_dns_record *record = NULL;

    pico_tree_foreach(node, rtree) {
        record = node->keyValue;
        if ((record) && pico_dns_record_copy_flat(record, dest)) {
            dns_dbg("Could not copy record into Answer Section!\n");
            return -1;
        }
    }
    return 0;
}

/* ****************************************************************************
 *  Fills the resource record sections of a DNS packet with provided record-
 *  trees.
 *
 *  @param packet Packet you want to fill
 *  @param qtree  Question tree to determine where the rrsections begin.
 *  @param antree DNS records to put in Answer section
 *  @param nstree DNS records to put in Authority section
 *  @param artree DNS records to put in Additional section
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_dns_fill_packet_rr_sections( pico_dns_packet *packet,
                                  struct pico_tree *qtree,
                                  struct pico_tree *antree,
                                  struct pico_tree *nstree,
                                  struct pico_tree *artree )
{
    int anret = 0, nsret = 0, arret = 0;
    uint16_t temp = 0;
    uint8_t *destination = NULL;

    /* Check params */
    if (!packet || !qtree || !antree || !nstree || !artree) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Initialise the destination pointers before iterating */
    destination = (uint8_t *)packet + sizeof(struct pico_dns_header);
    pico_tree_size(qtree, &temp, &pico_dns_question_size);
    destination = destination + temp;

    /* Iterate over ANSWERS */
    anret = pico_dns_fill_packet_rr_section(antree, &destination);

    /* Iterate over AUTHORITIES */
    nsret = pico_dns_fill_packet_rr_section(nstree, &destination);

    /* Iterate over ADDITIONALS */
    arret = pico_dns_fill_packet_rr_section(artree, &destination);

    if (anret || nsret || arret)
        return -1;

    return 0;
}

/* ****************************************************************************
 *  Fills the question section of a DNS packet with provided questions in the
 *  tree.
 *
 *  @param packet Packet you want to fill
 *  @param qtree  Question tree with question you want to insert
 *  @return 0 on success, something else on failure.
 * ****************************************************************************/
static int
pico_dns_fill_packet_question_section( pico_dns_packet *packet,
                                       struct pico_tree *qtree )
{
    struct pico_tree_node *node = NULL;
    struct pico_dns_question *question = NULL;
    struct pico_dns_question_suffix *dest_qsuffix = NULL;
    char *dest_qname = NULL;

    /* Check params */
    if (!packet || !qtree) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Initialise pointer */
    dest_qname = (char *)((char *)packet + sizeof(struct pico_dns_header));

    pico_tree_foreach(node, qtree) {
        question = node->keyValue;
        if (question) {
            /* Copy the name */
            memcpy(dest_qname, question->qname, question->qname_length);

            /* Copy the suffix */
            dest_qsuffix = (struct pico_dns_question_suffix *)
                           (dest_qname + question->qname_length);
            dest_qsuffix->qtype = question->qsuffix->qtype;
            dest_qsuffix->qclass = question->qsuffix->qclass;

            /* Move to next question */
            dest_qname = (char *)((char *)dest_qsuffix +
                                  sizeof(struct pico_dns_question_suffix));
        }
    }
    return 0;
}

/* ****************************************************************************
 *  Looks for a name somewhere else in packet, more specifically between the
 *  beginning of the data buffer and the name itself.
 * ****************************************************************************/
static uint8_t *
pico_dns_packet_compress_find_ptr( uint8_t *name,
                                   uint8_t *data,
                                   uint16_t len )
{
    uint8_t *iterator = NULL;

    /* Check params */
    if (!name || !data || !len)
        return NULL;

    if ((name < data) || (name > (data + len)))
        return NULL;

    iterator = data;

    /* Iterate from the beginning of data up until the name-ptr */
    while (iterator < name) {
        /* Compare in each iteration of current name is equal to a section of
           the DNS packet and if so return the pointer to that section */
        if (memcmp((void *)iterator++, (void *)name,
                   pico_dns_strlen((char *)name) + 1u) == 0)
            return (iterator - 1);
    }
    return NULL;
}

/* ****************************************************************************
 *  Compresses a single name by looking for the same name somewhere else in the
 *  packet-buffer.
 * ****************************************************************************/
static int
pico_dns_packet_compress_name( uint8_t *name,
                               uint8_t *packet,
                               uint16_t *len)
{
    uint8_t *lbl_iterator = NULL;    /* To iterate over labels */
    uint8_t *compression_ptr = NULL; /* PTR to somewhere else in the packet */
    uint8_t *offset = NULL;          /* PTR after compression pointer */
    uint8_t *ptr_after_str = NULL;
    uint8_t *last_byte = NULL;
    uint8_t *i = NULL;
    uint16_t ptr = 0;
    uint16_t difference = 0;

    /* Check params */
    if (!name || !packet || !len) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    if ((name < packet) || (name > (packet + *len))) {
        dns_dbg("Name ptr OOB. name: %p max: %p\n", name, packet + *len);
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    /* Try to compress name */
    lbl_iterator = name;
    while (lbl_iterator != '\0') {
        /* Try to find a compression pointer with current name */
        compression_ptr = pico_dns_packet_compress_find_ptr(lbl_iterator,
                                                            packet + 12, *len);
        /* If name can be compressed */
        if (compression_ptr) {
            /* Point to place after current string */
            ptr_after_str = lbl_iterator + strlen((char *)lbl_iterator) + 1u;

            /* Calculate the compression pointer value */
            ptr = (uint16_t)(compression_ptr - packet);

            /* Set the compression pointer in the packet */
            *lbl_iterator = (uint8_t)(0xC0 | (uint8_t)(ptr >> 8));
            *(lbl_iterator + 1) = (uint8_t)(ptr & 0xFF);

            /* Move up the rest of the packet data to right after the pointer */
            offset = lbl_iterator + 2;

            /* Move up left over data */
            difference = (uint16_t)(ptr_after_str - offset);
            last_byte = packet + *len;
            for (i = ptr_after_str; i < last_byte; i++) {
                *((uint8_t *)(i - difference)) = *i;
            }
            /* Update length */
            *len = (uint16_t)(*len - difference);
            break;
        }

        /* Move to next length label */
        lbl_iterator = lbl_iterator + *(lbl_iterator) + 1;
    }
    return 0;
}

/* ****************************************************************************
 *  Utility function compress a record section
 * ****************************************************************************/
static int
pico_dns_compress_record_sections( uint16_t qdcount, uint16_t count,
                                   uint8_t *buf, uint8_t **iterator,
                                   uint16_t *len )
{
    struct pico_dns_record_suffix *rsuffix = NULL;
    uint8_t *_iterator = NULL;
    uint16_t i = 0;

    /* Check params */
    if (!iterator || !(*iterator) || !buf || !len) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    _iterator = *iterator;

    for (i = 0; i < count; i++) {
        if (qdcount || i)
            pico_dns_packet_compress_name(_iterator, buf, len);

        /* To get rdlength */
        rsuffix = (struct pico_dns_record_suffix *)
                  (_iterator + pico_dns_namelen_comp((char *)_iterator) + 1u);

        /* Move to next res record */
        _iterator = ((uint8_t *)rsuffix +
                     sizeof(struct pico_dns_record_suffix) +
                     short_be(rsuffix->rdlength));
    }
    *iterator = _iterator;
    return 0;
}

/* ****************************************************************************
 *  Applies DNS name compression to an entire DNS packet
 * ****************************************************************************/
static int
pico_dns_packet_compress( pico_dns_packet *packet, uint16_t *len )
{
    uint8_t *packet_buf = NULL;
    uint8_t *iterator = NULL;
    uint16_t qdcount = 0, rcount = 0, i = 0;

    /* Check params */
    if (!packet || !len) {
        pico_err = PICO_ERR_EINVAL;
        return -1;
    }

    packet_buf = (uint8_t *)packet;

    /* Temporarily store the question & record counts */
    qdcount = short_be(packet->qdcount);
    rcount = (uint16_t)(rcount + short_be(packet->ancount));
    rcount = (uint16_t)(rcount + short_be(packet->nscount));
    rcount = (uint16_t)(rcount + short_be(packet->arcount));

    /* Move past the DNS packet header */
    iterator = (uint8_t *)((uint8_t *) packet + 12u);

    /* Start with the questions */
    for (i = 0; i < qdcount; i++) {
        if(i) { /* First question can't be compressed */
            pico_dns_packet_compress_name(iterator, packet_buf, len);
        }

        /* Move to next question */
        iterator = (uint8_t *)(iterator +
                               pico_dns_namelen_comp((char *)iterator) +
                               sizeof(struct pico_dns_question_suffix) + 1u);
    }
    /* Then onto the answers */
    pico_dns_compress_record_sections(qdcount, rcount, packet_buf, &iterator,
                                      len);
    return 0;
}

/* ****************************************************************************
 *  Calculates how big a packet needs be in order to store all the questions &
 *  records in the tree. Also determines the amount of questions and records.
 *
 *  @param qtree   Tree with Questions.
 *  @param antree  Tree with Answer Records.
 *  @param nstree  Tree with Authority Records.
 *  @param artree  Tree with Additional Records.
 *  @param qdcount Pointer to var to store amount of questions
 *  @param ancount Pointer to var to store amount of answers.
 *  @param nscount Pointer to var to store amount of authorities.
 *  @param arcount Pointer to var to store amount of additionals.
 *  @return Returns the total length that the DNS packet needs to be.
 * ****************************************************************************/
static uint16_t
pico_dns_packet_len( struct pico_tree *qtree,
                     struct pico_tree *antree,
                     struct pico_tree *nstree,
                     struct pico_tree *artree,
                     uint8_t *qdcount, uint8_t *ancount,
                     uint8_t *nscount, uint8_t *arcount )
{
    uint16_t len = (uint16_t) sizeof(pico_dns_packet);

    /* Check params */
    if (!qtree || !antree || !nstree || !artree) {
        pico_err = PICO_ERR_EINVAL;
        return 0;
    }

    *qdcount = (uint8_t)pico_tree_size(qtree, &len, &pico_dns_question_size);
    *ancount = (uint8_t)pico_tree_size(antree, &len, &pico_dns_record_size);
    *nscount = (uint8_t)pico_tree_size(nstree, &len, &pico_dns_record_size);
    *arcount = (uint8_t)pico_tree_size(artree, &len, &pico_dns_record_size);
    return len;
}

/* ****************************************************************************
 *  Generic packet creation utility that just creates a DNS packet with given
 *  questions and resource records to put in the Resource Record Sections. If a
 *  NULL-pointer is provided for a certain tree, no records will be added to
 *  that particular section of the packet.
 *
 *  @param qtree  DNS Questions to put in the Question Section.
 *  @param antree DNS Records to put in the Answer Section.
 *  @param nstree DNS Records to put in the Authority Section.
 *  @param artree DNS Records to put in the Additional Section.
 *  @param len    Will get fill with the entire size of the packet
 *  @return Pointer to created DNS packet
 * ****************************************************************************/
static pico_dns_packet *
pico_dns_packet_create( struct pico_tree *qtree,
                        struct pico_tree *antree,
                        struct pico_tree *nstree,
                        struct pico_tree *artree,
                        uint16_t *len )
{
    PICO_DNS_QTREE_DECLARE(_qtree);
    PICO_DNS_RTREE_DECLARE(_antree);
    PICO_DNS_RTREE_DECLARE(_nstree);
    PICO_DNS_RTREE_DECLARE(_artree);
    pico_dns_packet *packet = NULL;
    uint8_t qdcount = 0, ancount = 0, nscount = 0, arcount = 0;

    /* Set default vector, if arguments are NULL-pointers */
    _qtree = (qtree) ? (*qtree) : (_qtree);
    _antree = (antree) ? (*antree) : (_antree);
    _nstree = (nstree) ? (*nstree) : (_nstree);
    _artree = (artree) ? (*artree) : (_artree);

    /* Get the size of the entire packet and determine the header counters */
    *len = pico_dns_packet_len(&_qtree, &_antree, &_nstree, &_artree,
                               &qdcount, &ancount, &nscount, &arcount);

    /* Provide space for the entire packet */
    if (!(packet = PICO_ZALLOC(*len))) {
        pico_err = PICO_ERR_ENOMEM;
        return NULL;
    }

    /* Fill the Question Section with questions */
    if (qtree && pico_tree_count(&_qtree) != 0) {
        if (pico_dns_fill_packet_question_section(packet, &_qtree)) {
            dns_dbg("Could not fill Question Section correctly!\n");
            PICO_FREE(packet);
            return NULL;
        }
    }

    /* Fill the Resource Record Sections with resource records */
    if (pico_dns_fill_packet_rr_sections(packet, &_qtree, &_antree,
                                         &_nstree, &_artree)) {
        dns_dbg("Could not fill Resource Record Sections correctly!\n");
        PICO_FREE(packet);
        return NULL;
    }

    /* Fill the DNS packet header and try to compress */
    pico_dns_fill_packet_header(packet, qdcount, ancount, nscount, arcount);
    pico_dns_packet_compress(packet, len);

    return packet;
}

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
                       uint16_t *len )
{
    return pico_dns_packet_create(qtree, antree, nstree, artree, len);
}

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
                        uint16_t *len )
{
    return pico_dns_packet_create(NULL, antree, nstree, artree, len);
}
/* MARK: ^ DNS PACKET FUNCTIONS */
