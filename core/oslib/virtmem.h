#include "types.h"

namespace virtmem
{

struct Mapping {
	u64 start_address, end_address;
	u64 memoffset, memsize;
	bool allow_writes;
};

// Platform specific vmemory API
// To initialize (maybe) the vmem subsystem
bool init(void **vmem_base_addr, void **sh4rcb_addr, size_t ramSize);
// To reset the on-demand allocated pages.
void reset_mem(void *ptr, unsigned size_bytes);
// To handle a fault&allocate an ondemand page.
void ondemand_page(void *address, unsigned size_bytes);
// To create the mappings in the address space.
void create_mappings(const Mapping *vmem_maps, unsigned nummaps);
// Just tries to wipe as much as possible in the relevant area.
void destroy();
// Given a block of data in the .text section, prepares it for JIT action.
// both code_area and size are page aligned. Returns success.
bool prepare_jit_block(void *code_area, size_t size, void **code_area_rwx);
// Same as above but uses two address spaces one with RX and RW protections.
// Note: this function doesnt have to be implemented, it's a fallback for the above one.
bool prepare_jit_block(void *code_area, size_t size, void **code_area_rw, ptrdiff_t *rx_offset);
// This might not need an implementation (ie x86/64 cpus).
void flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end);
// Change a code buffer permissions from r-x to/from rw-
void jit_set_exec(void* code, size_t size, bool enable);
// Release a jit block previously allocated by prepare_jit_block
void release_jit_block(void *code_area, size_t size);
// Release a jit block previously allocated by prepare_jit_block (with dual RW and RX areas)
void release_jit_block(void *code_area1, void *code_area2, size_t size);

bool region_lock(void *start, std::size_t len);
bool region_unlock(void *start, std::size_t len);
bool region_set_exec(void *start, std::size_t len);

} // namespace vmem
