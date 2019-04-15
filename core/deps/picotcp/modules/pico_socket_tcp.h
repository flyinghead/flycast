#ifndef PICO_SOCKET_TCP_H
#define PICO_SOCKET_TCP_H
#include "pico_socket.h"

#ifdef PICO_SUPPORT_TCP

/* Functions/macros: conditional! */

# define IS_NAGLE_ENABLED(s) (!(!(!(s->opt_flags & (1 << PICO_SOCKET_OPT_TCPNODELAY)))))
int pico_setsockopt_tcp(struct pico_socket *s, int option, void *value);
int pico_getsockopt_tcp(struct pico_socket *s, int option, void *value);
int pico_socket_tcp_deliver(struct pico_sockport *sp, struct pico_frame *f);
void pico_socket_tcp_delete(struct pico_socket *s);
void pico_socket_tcp_cleanup(struct pico_socket *sock);
struct pico_socket *pico_socket_tcp_open(uint16_t family);
int pico_socket_tcp_read(struct pico_socket *s, void *buf, uint32_t len);
void transport_flags_update(struct pico_frame *, struct pico_socket *);

#else
#   define pico_getsockopt_tcp(...) (-1)
#   define pico_setsockopt_tcp(...) (-1)
#   define pico_socket_tcp_deliver(...) (-1)
#   define IS_NAGLE_ENABLED(s) (0)
#   define pico_socket_tcp_delete(...) do {} while(0)
#   define pico_socket_tcp_cleanup(...) do {} while(0)
#   define pico_socket_tcp_open(f) (NULL)
#   define pico_socket_tcp_read(...) (-1)
#   define transport_flags_update(...) do {} while(0)

#endif


#endif
