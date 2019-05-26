#include "_vmem.h"

bool vmem32_init();
void vmem32_term();
bool vmem32_handle_signal(void *fault_addr, bool write, u32 exception_pc);
void vmem32_flush_mmu();
void vmem32_protect_vram(vram_block *block);
void vmem32_unprotect_vram(vram_block *block);

extern bool vmem32_inited;
static inline bool vmem32_enabled() {
	return vmem32_inited;
}
