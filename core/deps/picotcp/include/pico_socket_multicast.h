#ifndef PICO_SOCKET_MULTICAST_H
#define PICO_SOCKET_MULTICAST_H
int pico_socket_mcast_filter(struct pico_socket *s, union pico_address *mcast_group, union pico_address *src);
void pico_multicast_delete(struct pico_socket *s);
int pico_setsockopt_mcast(struct pico_socket *s, int option, void *value);
int pico_getsockopt_mcast(struct pico_socket *s, int option, void *value);
int pico_udp_get_mc_ttl(struct pico_socket *s, uint8_t *ttl);
int pico_udp_set_mc_ttl(struct pico_socket *s, void *_ttl);

#endif
