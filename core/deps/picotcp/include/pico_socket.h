/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#ifndef INCLUDE_PICO_SOCKET
#define INCLUDE_PICO_SOCKET
#include "pico_queue.h"
#include "pico_addressing.h"
#include "pico_config.h"
#include "pico_protocol.h"
#include "pico_tree.h"

#ifdef __linux__
    #define PICO_DEFAULT_SOCKETQ (16 * 1024) /* Linux host, so we want full throttle */
#else
    #define PICO_DEFAULT_SOCKETQ (6 * 1024) /* seems like an acceptable default for small embedded systems */
#endif

#define PICO_SHUT_RD   1
#define PICO_SHUT_WR   2
#define PICO_SHUT_RDWR 3

#ifdef PICO_SUPPORT_IPV4
# define IS_SOCK_IPV4(s) ((s->net == &pico_proto_ipv4))
#else
# define IS_SOCK_IPV4(s) (0)
#endif

#ifdef PICO_SUPPORT_IPV6
# define IS_SOCK_IPV6(s) ((s->net == &pico_proto_ipv6))
#else
# define IS_SOCK_IPV6(s) (0)
#endif


struct pico_sockport
{
    struct pico_tree socks; /* how you make the connection ? */
    uint16_t number;
    uint16_t proto;
};


struct pico_socket {
    struct pico_protocol *proto;
    struct pico_protocol *net;

    union pico_address local_addr;
    union pico_address remote_addr;

    uint16_t local_port;
    uint16_t remote_port;

    struct pico_queue q_in;
    struct pico_queue q_out;

    void (*wakeup)(uint16_t ev, struct pico_socket *s);


#ifdef PICO_SUPPORT_TCP
    /* For the TCP backlog queue */
    struct pico_socket *backlog;
    struct pico_socket *next;
    struct pico_socket *parent;
    uint16_t max_backlog;
    uint16_t number_of_pending_conn;
#endif
#ifdef PICO_SUPPORT_MCAST
    struct pico_tree *MCASTListen;
#ifdef PICO_SUPPORT_IPV6
    struct pico_tree *MCASTListen_ipv6;
#endif
#endif
    uint16_t ev_pending;

    struct pico_device *dev;

    /* Private field. */
    int id;
    uint16_t state;
    uint16_t opt_flags;
    pico_time timestamp;
    void *priv;
};

struct pico_remote_endpoint {
    union pico_address remote_addr;
    uint16_t remote_port;
};


struct pico_ip_mreq {
    union pico_address mcast_group_addr;
    union pico_address mcast_link_addr;
};
struct pico_ip_mreq_source {
    union pico_address mcast_group_addr;
    union pico_address mcast_source_addr;
    union pico_address mcast_link_addr;
};


#define PICO_SOCKET_STATE_UNDEFINED       0x0000u
#define PICO_SOCKET_STATE_SHUT_LOCAL      0x0001u
#define PICO_SOCKET_STATE_SHUT_REMOTE     0x0002u
#define PICO_SOCKET_STATE_BOUND           0x0004u
#define PICO_SOCKET_STATE_CONNECTED       0x0008u
#define PICO_SOCKET_STATE_CLOSING         0x0010u
#define PICO_SOCKET_STATE_CLOSED          0x0020u

# define PICO_SOCKET_STATE_TCP                0xFF00u
# define PICO_SOCKET_STATE_TCP_UNDEF          0x00FFu
# define PICO_SOCKET_STATE_TCP_CLOSED         0x0100u
# define PICO_SOCKET_STATE_TCP_LISTEN         0x0200u
# define PICO_SOCKET_STATE_TCP_SYN_SENT       0x0300u
# define PICO_SOCKET_STATE_TCP_SYN_RECV       0x0400u
# define PICO_SOCKET_STATE_TCP_ESTABLISHED    0x0500u
# define PICO_SOCKET_STATE_TCP_CLOSE_WAIT     0x0600u
# define PICO_SOCKET_STATE_TCP_LAST_ACK       0x0700u
# define PICO_SOCKET_STATE_TCP_FIN_WAIT1      0x0800u
# define PICO_SOCKET_STATE_TCP_FIN_WAIT2      0x0900u
# define PICO_SOCKET_STATE_TCP_CLOSING        0x0a00u
# define PICO_SOCKET_STATE_TCP_TIME_WAIT      0x0b00u
# define PICO_SOCKET_STATE_TCP_ARRAYSIZ       0x0cu


/* Socket options */
# define PICO_TCP_NODELAY                     1
# define PICO_SOCKET_OPT_TCPNODELAY           0x0000u

# define PICO_IP_MULTICAST_EXCLUDE            0
# define PICO_IP_MULTICAST_INCLUDE            1
# define PICO_IP_MULTICAST_IF                 32
# define PICO_IP_MULTICAST_TTL                33
# define PICO_IP_MULTICAST_LOOP               34
# define PICO_IP_ADD_MEMBERSHIP               35
# define PICO_IP_DROP_MEMBERSHIP              36
# define PICO_IP_UNBLOCK_SOURCE               37
# define PICO_IP_BLOCK_SOURCE                 38
# define PICO_IP_ADD_SOURCE_MEMBERSHIP        39
# define PICO_IP_DROP_SOURCE_MEMBERSHIP       40

# define PICO_SOCKET_OPT_MULTICAST_LOOP       1
# define PICO_SOCKET_OPT_KEEPIDLE              4
# define PICO_SOCKET_OPT_KEEPINTVL             5
# define PICO_SOCKET_OPT_KEEPCNT               6

#define PICO_SOCKET_OPT_LINGER                13

# define PICO_SOCKET_OPT_RCVBUF               52
# define PICO_SOCKET_OPT_SNDBUF               53


/* Constants */
# define PICO_IP_DEFAULT_MULTICAST_TTL        1
# define PICO_IP_DEFAULT_MULTICAST_LOOP       1

#define PICO_SOCKET_TIMEOUT                   5000u /* 5 seconds */
#define PICO_SOCKET_LINGER_TIMEOUT            3000u /* 3 seconds */
#define PICO_SOCKET_BOUND_TIMEOUT             30000u /* 30 seconds */

#define PICO_SOCKET_SHUTDOWN_WRITE 0x01u
#define PICO_SOCKET_SHUTDOWN_READ  0x02u
#define TCPSTATE(s) ((s)->state & PICO_SOCKET_STATE_TCP)

#define PICO_SOCK_EV_RD 1u
#define PICO_SOCK_EV_WR 2u
#define PICO_SOCK_EV_CONN 4u
#define PICO_SOCK_EV_CLOSE 8u
#define PICO_SOCK_EV_FIN 0x10u
#define PICO_SOCK_EV_ERR 0x80u

struct pico_msginfo {
    struct pico_device *dev;
    uint8_t ttl;
    uint8_t tos;
};

struct pico_socket *pico_socket_open(uint16_t net, uint16_t proto, void (*wakeup)(uint16_t ev, struct pico_socket *s));

int pico_socket_read(struct pico_socket *s, void *buf, int len);
int pico_socket_write(struct pico_socket *s, const void *buf, int len);

int pico_socket_sendto(struct pico_socket *s, const void *buf, int len, void *dst, uint16_t remote_port);
int pico_socket_sendto_extended(struct pico_socket *s, const void *buf, const int len,
                                void *dst, uint16_t remote_port, struct pico_msginfo *msginfo);

int pico_socket_recvfrom(struct pico_socket *s, void *buf, int len, void *orig, uint16_t *local_port);
int pico_socket_recvfrom_extended(struct pico_socket *s, void *buf, int len, void *orig,
                                  uint16_t *remote_port, struct pico_msginfo *msginfo);

int pico_socket_send(struct pico_socket *s, const void *buf, int len);
int pico_socket_recv(struct pico_socket *s, void *buf, int len);

int pico_socket_bind(struct pico_socket *s, void *local_addr, uint16_t *port);
int pico_socket_getname(struct pico_socket *s, void *local_addr, uint16_t *port, uint16_t *proto);
int pico_socket_getpeername(struct pico_socket *s, void *remote_addr, uint16_t *port, uint16_t *proto);

int pico_socket_connect(struct pico_socket *s, const void *srv_addr, uint16_t remote_port);
int pico_socket_listen(struct pico_socket *s, const int backlog);
struct pico_socket *pico_socket_accept(struct pico_socket *s, void *orig, uint16_t *port);
int8_t pico_socket_del(struct pico_socket *s);

int pico_socket_setoption(struct pico_socket *s, int option, void *value);
int pico_socket_getoption(struct pico_socket *s, int option, void *value);

int pico_socket_shutdown(struct pico_socket *s, int mode);
int pico_socket_close(struct pico_socket *s);

struct pico_frame *pico_socket_frame_alloc(struct pico_socket *s, struct pico_device *dev, uint16_t len);
struct pico_device *get_sock_dev(struct pico_socket *s);


#ifdef PICO_SUPPORT_IPV4
# define is_sock_ipv4(x) (x->net == &pico_proto_ipv4)
#else
# define is_sock_ipv4(x) (0)
#endif

#ifdef PICO_SUPPORT_IPV6
# define is_sock_ipv6(x) (x->net == &pico_proto_ipv6)
#else
# define is_sock_ipv6(x) (0)
#endif

#ifdef PICO_SUPPORT_UDP
# define is_sock_udp(x) (x->proto == &pico_proto_udp)
#else
# define is_sock_udp(x) (0)
#endif

#ifdef PICO_SUPPORT_TCP
# define is_sock_tcp(x) (x->proto == &pico_proto_tcp)
#else
# define is_sock_tcp(x) (0)
#endif

/* Interface towards transport protocol */
int pico_transport_process_in(struct pico_protocol *self, struct pico_frame *f);
struct pico_socket *pico_socket_clone(struct pico_socket *facsimile);
int8_t pico_socket_add(struct pico_socket *s);
int pico_transport_error(struct pico_frame *f, uint8_t proto, int code);

/* Socket loop */
int pico_sockets_loop(int loop_score);
struct pico_socket*pico_sockets_find(uint16_t local, uint16_t remote);
/* Port check */
int pico_is_port_free(uint16_t proto, uint16_t port, void *addr, void *net);

struct pico_sockport *pico_get_sockport(uint16_t proto, uint16_t port);

uint32_t pico_socket_get_mss(struct pico_socket *s);
int pico_socket_set_family(struct pico_socket *s, uint16_t family);

int pico_count_sockets(uint8_t proto);

#define PICO_SOCKET_SETOPT_EN(socket, index)  (socket->opt_flags |=  (1 << index))
#define PICO_SOCKET_SETOPT_DIS(socket, index) (socket->opt_flags &= (uint16_t) ~(1 << index))
#define PICO_SOCKET_GETOPT(socket, index) ((socket->opt_flags & (1u << index)) != 0)


#endif
