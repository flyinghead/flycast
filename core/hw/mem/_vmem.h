#pragma once
#include "types.h"

enum VMemType {
	MemType4GB,
	MemType512MB,
	MemTypeError
};

struct vmem_mapping {
	u64 start_address, end_address;
	u64 memoffset, memsize;
	bool allow_writes;
};

// Platform specific vmemory API
// To initialize (maybe) the vmem subsystem
VMemType vmem_platform_init(void **vmem_base_addr, void **sh4rcb_addr);
// To reset the on-demand allocated pages.
void vmem_platform_reset_mem(void *ptr, unsigned size_bytes);
// To handle a fault&allocate an ondemand page.
void vmem_platform_ondemand_page(void *address, unsigned size_bytes);
// To create the mappings in the address space.
void vmem_platform_create_mappings(const vmem_mapping *vmem_maps, unsigned nummaps);
// Just tries to wipe as much as possible in the relevant area.
void vmem_platform_destroy();
// Given a block of data in the .text section, prepares it for JIT action.
// both code_area and size are page aligned. Returns success.
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rwx);
// Same as above but uses two address spaces one with RX and RW protections.
// Note: this function doesnt have to be implemented, it's a fallback for the above one.
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rw, ptrdiff_t *rx_offset);
// This might not need an implementation (ie x86/64 cpus).
void vmem_platform_flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end);
// Change a code buffer permissions from r-x to/from rw-
void vmem_platform_jit_set_exec(void* code, size_t size, bool enable);

// Note: if you want to disable vmem magic in any given platform, implement the
// above functions as empty functions and make vmem_platform_init return MemTypeError.

//Typedef's
//ReadMem 
typedef u8 DYNACALL _vmem_ReadMem8FP(u32 Address);
typedef u16 DYNACALL _vmem_ReadMem16FP(u32 Address);
typedef u32 DYNACALL _vmem_ReadMem32FP(u32 Address);
//WriteMem
typedef void DYNACALL _vmem_WriteMem8FP(u32 Address,u8 data);
typedef void DYNACALL _vmem_WriteMem16FP(u32 Address,u16 data);
typedef void DYNACALL _vmem_WriteMem32FP(u32 Address,u32 data);

//our own handle type :)
typedef u32 _vmem_handler;

//Functions

//init/reset/term
void _vmem_init();
void _vmem_term();
void _vmem_init_mappings();

//functions to register and map handlers/memory
_vmem_handler _vmem_register_handler(_vmem_ReadMem8FP* read8,_vmem_ReadMem16FP* read16,_vmem_ReadMem32FP* read32, _vmem_WriteMem8FP* write8,_vmem_WriteMem16FP* write16,_vmem_WriteMem32FP* write32);

#define  _vmem_register_handler_Template(read,write) _vmem_register_handler \
									(read<u8>,read<u16>,read<u32>,	\
									write<u8>,write<u16>,write<u32>)

void _vmem_map_handler(_vmem_handler Handler,u32 start,u32 end);
void _vmem_map_block(void* base,u32 start,u32 end,u32 mask);
void _vmem_mirror_mapping(u32 new_region,u32 start,u32 size);

#define _vmem_map_block_mirror(base, start, end, blck_size) { \
	u32 block_size = (blck_size) >> 24; \
	for (u32 _maip = (start); _maip <= (end); _maip += block_size) \
		_vmem_map_block((base), _maip, _maip + block_size - 1, blck_size - 1); \
}

//ReadMem(s)
u32 DYNACALL _vmem_ReadMem8SX32(u32 Address);
u32 DYNACALL _vmem_ReadMem16SX32(u32 Address);
u8 DYNACALL _vmem_ReadMem8(u32 Address);
u16 DYNACALL _vmem_ReadMem16(u32 Address);
u32 DYNACALL _vmem_ReadMem32(u32 Address);
u64 DYNACALL _vmem_ReadMem64(u32 Address);
template<typename T, typename Trv> Trv DYNACALL _vmem_readt(u32 addr);
//WriteMem(s)
void DYNACALL _vmem_WriteMem8(u32 Address,u8 data);
void DYNACALL _vmem_WriteMem16(u32 Address,u16 data);
void DYNACALL _vmem_WriteMem32(u32 Address,u32 data);
void DYNACALL _vmem_WriteMem64(u32 Address,u64 data);
template<typename T> void DYNACALL _vmem_writet(u32 addr, T data);

//should be called at start up to ensure it will succeed :)
bool _vmem_reserve();
void _vmem_release();

//dynarec helpers
void* _vmem_read_const(u32 addr,bool& ismem,u32 sz);
void* _vmem_write_const(u32 addr,bool& ismem,u32 sz);

extern u8* virt_ram_base;
extern bool vmem_4gb_space;

static inline bool _nvmem_enabled() {
	return virt_ram_base != 0;
}
static inline bool _nvmem_4gb_space() {
	return vmem_4gb_space;
}
void _vmem_bm_reset();

#define MAP_RAM_START_OFFSET  0
#define MAP_VRAM_START_OFFSET (MAP_RAM_START_OFFSET+RAM_SIZE)
#define MAP_ARAM_START_OFFSET (MAP_VRAM_START_OFFSET+VRAM_SIZE)

void _vmem_protect_vram(u32 addr, u32 size);
void _vmem_unprotect_vram(u32 addr, u32 size);
u32 _vmem_get_vram_offset(void *addr);
bool BM_LockedWrite(u8* address);

