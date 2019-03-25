#ifndef PICO_FRAGMENTS_H
#define PICO_FRAGMENTS_H
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_addressing.h"
#include "pico_frame.h"

void pico_ipv6_process_frag(struct pico_ipv6_exthdr *frag, struct pico_frame *f, uint8_t proto);
void pico_ipv4_process_frag(struct pico_ipv4_hdr *hdr, struct pico_frame *f, uint8_t proto);

#endif
