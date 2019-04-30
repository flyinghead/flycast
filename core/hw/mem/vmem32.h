#include "types.h"

bool vmem32_init();
void vmem32_term();
bool vmem32_handle_signal(void *fault_addr, bool write);
void vmem32_flush_mmu();
void vmem32_protect_vram(vram_block *block);
void vmem32_unprotect_vram(vram_block *block);
static inline bool vmem32_enabled() {
#if HOST_OS == OS_WINDOWS
	return false;
#else
	return !settings.dynarec.disable_vmem32;
#endif
}
