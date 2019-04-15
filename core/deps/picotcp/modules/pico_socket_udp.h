#ifndef PICO_SOCKET_UDP_H
#define PICO_SOCKET_UDP_H

struct pico_socket *pico_socket_udp_open(void);
int pico_socket_udp_deliver(struct pico_sockport *sp, struct pico_frame *f);


#ifdef PICO_SUPPORT_UDP
int pico_setsockopt_udp(struct pico_socket *s, int option, void *value);
int pico_getsockopt_udp(struct pico_socket *s, int option, void *value);
#   define pico_socket_udp_recv(s, buf, len, addr, port) pico_udp_recv(s, buf, len, addr, port, NULL)
#else
#   define pico_socket_udp_recv(...) (0)
#   define pico_getsockopt_udp(...) (-1)
#   define pico_setsockopt_udp(...) (-1)
#endif


#endif
