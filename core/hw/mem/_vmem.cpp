#include "_vmem.h"
#include "vmem32.h"
#include "hw/aica/aica_if.h"
#include "hw/sh4/dyna/blockmanager.h"

#define HANDLER_MAX 0x1F
#define HANDLER_COUNT (HANDLER_MAX+1)

//top registered handler
_vmem_handler       _vmem_lrp;

//handler tables
_vmem_ReadMem8FP*   _vmem_RF8[HANDLER_COUNT];
_vmem_WriteMem8FP*  _vmem_WF8[HANDLER_COUNT];

_vmem_ReadMem16FP*  _vmem_RF16[HANDLER_COUNT];
_vmem_WriteMem16FP* _vmem_WF16[HANDLER_COUNT];

_vmem_ReadMem32FP*  _vmem_RF32[HANDLER_COUNT];
_vmem_WriteMem32FP* _vmem_WF32[HANDLER_COUNT];

//upper 8b of the address
void* _vmem_MemInfo_ptr[0x100];

void _vmem_get_ptrs(u32 sz,bool write,void*** vmap,void*** func)
{
	*vmap=_vmem_MemInfo_ptr;
	switch(sz)
	{
	case 1:
		*func=write?(void**)_vmem_WF8:(void**)_vmem_RF8;
		return;

	case 2:
		*func=write?(void**)_vmem_WF16:(void**)_vmem_RF16;
		return;

	case 4:
	case 8:
		*func=write?(void**)_vmem_WF32:(void**)_vmem_RF32;
		return;

	default:
		die("invalid size");
	}
}

void* _vmem_get_ptr2(u32 addr,u32& mask)
{
	u32   page=addr>>24;
	unat  iirf=(unat)_vmem_MemInfo_ptr[page];
	void* ptr=(void*)(iirf&~HANDLER_MAX);

	if (ptr==0) return 0;

	mask=0xFFFFFFFF>>iirf;
	return ptr;
}

void* _vmem_read_const(u32 addr,bool& ismem,u32 sz)
{
	u32   page=addr>>24;
	unat  iirf=(unat)_vmem_MemInfo_ptr[page];
	void* ptr=(void*)(iirf&~HANDLER_MAX);

	if (ptr==0)
	{
		ismem=false;
		const unat id=iirf;
		if (sz==1)
		{
			return (void*)_vmem_RF8[id/4];
		}
		else if (sz==2)
		{
			return (void*)_vmem_RF16[id/4];
		}
		else if (sz==4)
		{
			return (void*)_vmem_RF32[id/4];
		}
		else
		{
			die("Invalid size");
		}
	}
	else
	{
		ismem=true;
		addr<<=iirf;
		addr>>=iirf;

		return &(((u8*)ptr)[addr]);
	}
	die("Invalid memory size");

	return 0;
}

void* _vmem_write_const(u32 addr,bool& ismem,u32 sz)
{
	u32   page=addr>>24;
	unat  iirf=(unat)_vmem_MemInfo_ptr[page];
	void* ptr=(void*)(iirf&~HANDLER_MAX);

	if (ptr==0)
	{
		ismem=false;
		const unat id=iirf;
		if (sz==1)
		{
			return (void*)_vmem_WF8[id/4];
		}
		else if (sz==2)
		{
			return (void*)_vmem_WF16[id/4];
		}
		else if (sz==4)
		{
			return (void*)_vmem_WF32[id/4];
		}
		else
		{
			die("Invalid size");
		}
	}
	else
	{
		ismem=true;
		addr<<=iirf;
		addr>>=iirf;

		return &(((u8*)ptr)[addr]);
	}
	die("Invalid memory size");

	return 0;
}

void* _vmem_page_info(u32 addr,bool& ismem,u32 sz,u32& page_sz,bool rw)
{
	u32   page=addr>>24;
	unat  iirf=(unat)_vmem_MemInfo_ptr[page];
	void* ptr=(void*)(iirf&~HANDLER_MAX);
	
	if (ptr==0)
	{
		ismem=false;
		const unat id=iirf;
		page_sz=24;
		if (sz==1)
		{
			return rw?(void*)_vmem_RF8[id/4]:(void*)_vmem_WF8[id/4];
		}
		else if (sz==2)
		{
			return rw?(void*)_vmem_RF16[id/4]:(void*)_vmem_WF16[id/4];
		}
		else if (sz==4)
		{
			return rw?(void*)_vmem_RF32[id/4]:(void*)_vmem_WF32[id/4];
		}
		else
		{
			die("Invalid size");
		}
	}
	else
	{
		ismem=true;

		page_sz=32-(iirf&0x1F);

		return ptr;
	}
	die("Invalid memory size");

	return 0;
}

template<typename T,typename Trv>
INLINE Trv DYNACALL _vmem_readt(u32 addr)
{
	const u32 sz=sizeof(T);

	u32   page=addr>>24;	//1 op, shift/extract
	unat  iirf=(unat)_vmem_MemInfo_ptr[page]; //2 ops, insert + read [vmem table will be on reg ]
	void* ptr=(void*)(iirf&~HANDLER_MAX);     //2 ops, and // 1 op insert

	if (likely(ptr!=0))
	{
		addr<<=iirf;
		addr>>=iirf;

		T data=(*((T*)&(((u8*)ptr)[addr])));
		return data;
	}
	else
	{
		const u32 id=iirf;
		if (sz==1)
		{
			return (T)_vmem_RF8[id/4](addr);
		}
		else if (sz==2)
		{
			return (T)_vmem_RF16[id/4](addr);
		}
		else if (sz==4)
		{
			return _vmem_RF32[id/4](addr);
		}
		else if (sz==8)
		{
			T rv=_vmem_RF32[id/4](addr);
			rv|=(T)((u64)_vmem_RF32[id/4](addr+4)<<32);
			
			return rv;
		}
		else
		{
			die("Invalid size");
		}
	}
}
template u8 DYNACALL _vmem_readt<u8, u8>(u32 addr);
template u16 DYNACALL _vmem_readt<u16, u16>(u32 addr);
template u32 DYNACALL _vmem_readt<u32, u32>(u32 addr);
template u64 DYNACALL _vmem_readt<u64, u64>(u32 addr);

template<typename T>
INLINE void DYNACALL _vmem_writet(u32 addr,T data)
{
	const u32 sz=sizeof(T);

	u32 page=addr>>24;
	unat  iirf=(unat)_vmem_MemInfo_ptr[page];
	void* ptr=(void*)(iirf&~HANDLER_MAX);

	if (likely(ptr!=0))
	{
		addr<<=iirf;
		addr>>=iirf;

		*((T*)&(((u8*)ptr)[addr]))=data;
	}
	else
	{
		const u32 id=iirf;
		if (sz==1)
		{
			 _vmem_WF8[id/4](addr,data);
		}
		else if (sz==2)
		{
			 _vmem_WF16[id/4](addr,data);
		}
		else if (sz==4)
		{
			 _vmem_WF32[id/4](addr,data);
		}
		else if (sz==8)
		{
			_vmem_WF32[id/4](addr,(u32)data);
			_vmem_WF32[id/4](addr+4,(u32)((u64)data>>32));
		}
		else
		{
			die("Invalid size");
		}
	}
}
template void DYNACALL _vmem_writet<u8>(u32 addr, u8 data);
template void DYNACALL _vmem_writet<u16>(u32 addr, u16 data);
template void DYNACALL _vmem_writet<u32>(u32 addr, u32 data);
template void DYNACALL _vmem_writet<u64>(u32 addr, u64 data);

//ReadMem/WriteMem functions
//ReadMem
u32 DYNACALL _vmem_ReadMem8SX32(u32 Address) { return _vmem_readt<s8,s32>(Address); }
u32 DYNACALL _vmem_ReadMem16SX32(u32 Address) { return _vmem_readt<s16,s32>(Address); }

u8 DYNACALL _vmem_ReadMem8(u32 Address) { return _vmem_readt<u8,u8>(Address); }
u16 DYNACALL _vmem_ReadMem16(u32 Address) { return _vmem_readt<u16,u16>(Address); }
u32 DYNACALL _vmem_ReadMem32(u32 Address) { return _vmem_readt<u32,u32>(Address); }
u64 DYNACALL _vmem_ReadMem64(u32 Address) { return _vmem_readt<u64,u64>(Address); }

//WriteMem
void DYNACALL _vmem_WriteMem8(u32 Address,u8 data) { _vmem_writet<u8>(Address,data); }
void DYNACALL _vmem_WriteMem16(u32 Address,u16 data) { _vmem_writet<u16>(Address,data); }
void DYNACALL _vmem_WriteMem32(u32 Address,u32 data) { _vmem_writet<u32>(Address,data); }
void DYNACALL _vmem_WriteMem64(u32 Address,u64 data) { _vmem_writet<u64>(Address,data); }

//0xDEADC0D3 or 0
#define MEM_ERROR_RETURN_VALUE 0xDEADC0D3

//phew .. that was lota asm code ;) lets go back to C :D
//default mem handlers ;)
//default read handlers
u8 DYNACALL _vmem_ReadMem8_not_mapped(u32 addresss)
{
	//printf("[sh4]Read8 from 0x%X, not mapped [_vmem default handler]\n",addresss);
	return (u8)MEM_ERROR_RETURN_VALUE;
}
u16 DYNACALL _vmem_ReadMem16_not_mapped(u32 addresss)
{
	//printf("[sh4]Read16 from 0x%X, not mapped [_vmem default handler]\n",addresss);
	return (u16)MEM_ERROR_RETURN_VALUE;
}
u32 DYNACALL _vmem_ReadMem32_not_mapped(u32 addresss)
{
	//printf("[sh4]Read32 from 0x%X, not mapped [_vmem default handler]\n",addresss);
	return (u32)MEM_ERROR_RETURN_VALUE;
}
//default write handers
void DYNACALL _vmem_WriteMem8_not_mapped(u32 addresss,u8 data)
{
	//printf("[sh4]Write8 to 0x%X=0x%X, not mapped [_vmem default handler]\n",addresss,data);
}
void DYNACALL _vmem_WriteMem16_not_mapped(u32 addresss,u16 data)
{
	//printf("[sh4]Write16 to 0x%X=0x%X, not mapped [_vmem default handler]\n",addresss,data);
}
void DYNACALL _vmem_WriteMem32_not_mapped(u32 addresss,u32 data)
{
	//printf("[sh4]Write32 to 0x%X=0x%X, not mapped [_vmem default handler]\n",addresss,data);
}
//code to register handlers
//0 is considered error :)
_vmem_handler _vmem_register_handler(
									 _vmem_ReadMem8FP* read8,
									 _vmem_ReadMem16FP* read16,
									 _vmem_ReadMem32FP* read32,

									 _vmem_WriteMem8FP* write8,
									 _vmem_WriteMem16FP* write16,
									 _vmem_WriteMem32FP* write32
									 )
{
	_vmem_handler rv=_vmem_lrp++;

	verify(rv<HANDLER_COUNT);

	_vmem_RF8[rv] =read8==0  ? _vmem_ReadMem8_not_mapped  : read8;
	_vmem_RF16[rv]=read16==0 ? _vmem_ReadMem16_not_mapped : read16;
	_vmem_RF32[rv]=read32==0 ? _vmem_ReadMem32_not_mapped : read32;

	_vmem_WF8[rv] =write8==0 ? _vmem_WriteMem8_not_mapped : write8;
	_vmem_WF16[rv]=write16==0? _vmem_WriteMem16_not_mapped: write16;
	_vmem_WF32[rv]=write32==0? _vmem_WriteMem32_not_mapped: write32;

	return rv;
}
u32 FindMask(u32 msk)
{
	u32 s=-1;
	u32 rv=0;

	while(msk!=s>>rv)
		rv++;

	return rv;
}

//map a registered handler to a mem region
void _vmem_map_handler(_vmem_handler Handler,u32 start,u32 end)
{
	verify(start<0x100);
	verify(end<0x100);
	verify(start<=end);
	for (u32 i=start;i<=end;i++)
	{
		_vmem_MemInfo_ptr[i]=((u8*)0)+(0x00000000 + Handler*4);
	}
}

//map a memory block to a mem region
void _vmem_map_block(void* base,u32 start,u32 end,u32 mask)
{
	verify(start<0x100);
	verify(end<0x100);
	verify(start<=end);
	verify((0xFF & (unat)base)==0);
	verify(base!=0);
	u32 j=0;
	for (u32 i=start;i<=end;i++)
	{
		_vmem_MemInfo_ptr[i]=&(((u8*)base)[j&mask]) + FindMask(mask) - (j & mask);
		j+=0x1000000;
	}
}

void _vmem_mirror_mapping(u32 new_region,u32 start,u32 size)
{
	u32 end=start+size-1;
	verify(start<0x100);
	verify(end<0x100);
	verify(start<=end);
	verify(!((start>=new_region) && (end<=new_region)));

	u32 j=new_region;
	for (u32 i=start;i<=end;i++)
	{
		_vmem_MemInfo_ptr[j&0xFF]=_vmem_MemInfo_ptr[i&0xFF];
		j++;
	}
}

//init/reset/term
void _vmem_init()
{
	_vmem_reset();
}

void _vmem_reset()
{
	//clear read tables
	memset(_vmem_RF8,0,sizeof(_vmem_RF8));
	memset(_vmem_RF16,0,sizeof(_vmem_RF16));
	memset(_vmem_RF32,0,sizeof(_vmem_RF32));
	
	//clear write tables
	memset(_vmem_WF8,0,sizeof(_vmem_WF8));
	memset(_vmem_WF16,0,sizeof(_vmem_WF16));
	memset(_vmem_WF32,0,sizeof(_vmem_WF32));
	
	//clear meminfo table
	memset(_vmem_MemInfo_ptr,0,sizeof(_vmem_MemInfo_ptr));

	//reset registration index
	_vmem_lrp=0;

	//register default functions (0) for slot 0
	verify(_vmem_register_handler(0,0,0,0,0,0)==0);
}

void _vmem_term() {}

#include "hw/pvr/pvr_mem.h"
#include "hw/sh4/sh4_mem.h"

u8* virt_ram_base;
bool vmem_4gb_space;

void* malloc_pages(size_t size) {
#if HOST_OS == OS_WINDOWS
	return _aligned_malloc(size, PAGE_SIZE);
#elif defined(_ISOC11_SOURCE)
	return aligned_alloc(PAGE_SIZE, size);
#else
	void *data;
	if (posix_memalign(&data, PAGE_SIZE, size) != 0)
		return NULL;
	else
		return data;
#endif
}

// Resets the FPCB table (by either clearing it to the default val
// or by flushing it and making it fault on access again.
void _vmem_bm_reset() {
	// If we allocated it via vmem:
	if (virt_ram_base)
		vmem_platform_reset_mem(p_sh4rcb->fpcb, sizeof(p_sh4rcb->fpcb));
	else
		// We allocated it via a regular malloc/new/whatever on the heap
		bm_vmem_pagefill((void**)p_sh4rcb->fpcb, sizeof(p_sh4rcb->fpcb));
}

// This gets called whenever there is a pagefault, it is possible that it lands
// on the fpcb memory range, which is allocated on miss. Returning true tells the
// fault handler this was us, and that the page is resolved and can continue the execution.
bool BM_LockedWrite(u8* address) {
	if (!virt_ram_base)
		return false;  // No vmem, therefore not us who caused this.

	uintptr_t ptrint = (uintptr_t)address;
	uintptr_t start  = (uintptr_t)p_sh4rcb->fpcb;
	uintptr_t end    = start + sizeof(p_sh4rcb->fpcb);

	if (ptrint >= start && ptrint < end) {
		// Alloc the page then and initialize it to default values
		void *aligned_addr = (void*)(ptrint & (~PAGE_MASK));
		vmem_platform_ondemand_page(aligned_addr, PAGE_SIZE);
		bm_vmem_pagefill((void**)aligned_addr, PAGE_SIZE);
		return true;
	}
	return false;
}

static void _vmem_set_p0_mappings()
{
	const vmem_mapping mem_mappings[] = {
		// P0/U0
		{0x00000000, 0x00800000,                               0,         0, false},  // Area 0 -> unused
		{0x00800000, 0x00800000 + ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE, true}, // Aica
		{0x00800000 + ARAM_SIZE, 0x02800000,                   0,         0, false},  // unused
		{0x02800000, 0x02800000 + ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE, true}, // Aica mirror
		{0x02800000 + ARAM_SIZE, 0x04000000,                   0,         0, false},  // unused
		{0x04000000, 0x05000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // Area 1 (vram, 16MB, wrapped on DC as 2x8MB)
		{0x05000000, 0x06000000,                               0,         0, false},  // 32 bit path (unused)
		{0x06000000, 0x07000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // VRAM mirror
		{0x07000000, 0x08000000,                               0,         0, false},  // 32 bit path (unused) mirror
		{0x08000000, 0x0C000000,                               0,         0, false},  // Area 2
		{0x0C000000, 0x10000000,            MAP_RAM_START_OFFSET,  RAM_SIZE,  true},  // Area 3 (main RAM + 3 mirrors)
		{0x10000000, 0x80000000,                               0,         0, false},  // Area 4-7 (unused)
	};
	vmem_platform_create_mappings(&mem_mappings[0], ARRAY_SIZE(mem_mappings));
}

bool _vmem_reserve() {
	// TODO: Static assert?
	verify((sizeof(Sh4RCB)%PAGE_SIZE)==0);

	VMemType vmemstatus = MemTypeError;

	// Use vmem only if settings mandate so, and if we have proper exception handlers.
	#ifndef TARGET_NO_EXCEPTIONS
	if (!settings.dynarec.disable_nvmem)
		vmemstatus = vmem_platform_init((void**)&virt_ram_base, (void**)&p_sh4rcb);
	#endif

	// Fallback to statically allocated buffers, this results in slow-ops being generated.
	if (vmemstatus == MemTypeError) {
		printf("Warning! nvmem is DISABLED (due to failure or not being built-in\n");
		virt_ram_base = 0;

		// Allocate it all and initialize it.
		p_sh4rcb = (Sh4RCB*)malloc_pages(sizeof(Sh4RCB));
		bm_vmem_pagefill((void**)p_sh4rcb->fpcb, sizeof(p_sh4rcb->fpcb));

		mem_b.size = RAM_SIZE;
		mem_b.data = (u8*)malloc_pages(RAM_SIZE);

		vram.size = VRAM_SIZE;
		vram.data = (u8*)malloc_pages(VRAM_SIZE);

		aica_ram.size = ARAM_SIZE;
		aica_ram.data = (u8*)malloc_pages(ARAM_SIZE);
	}
	else {
		printf("Info: nvmem is enabled, with addr space of size %s\n", vmemstatus == MemType4GB ? "4GB" : "512MB");
		printf("Info: p_sh4rcb: %p virt_ram_base: %p\n", p_sh4rcb, virt_ram_base);
		// Map the different parts of the memory file into the new memory range we got.
		if (vmemstatus == MemType512MB)
		{
			const vmem_mapping mem_mappings[] = {
				{0x00000000, 0x00800000,                               0,         0, false},  // Area 0 -> unused
				{0x00800000, 0x01000000,           MAP_ARAM_START_OFFSET, ARAM_SIZE, false},  // Aica
				{0x20000000, 0x20000000+ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE,  true},
				{0x01000000, 0x04000000,                               0,         0, false},  // More unused
				{0x04000000, 0x05000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // Area 1 (vram, 16MB, wrapped on DC as 2x8MB)
				{0x05000000, 0x06000000,                               0,         0, false},  // 32 bit path (unused)
				{0x06000000, 0x07000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // VRAM mirror
				{0x07000000, 0x08000000,                               0,         0, false},  // 32 bit path (unused) mirror
				{0x08000000, 0x0C000000,                               0,         0, false},  // Area 2
				{0x0C000000, 0x10000000,            MAP_RAM_START_OFFSET,  RAM_SIZE,  true},  // Area 3 (main RAM + 3 mirrors)
				{0x10000000, 0x20000000,                               0,         0, false},  // Area 4-7 (unused)
			};
			vmem_platform_create_mappings(&mem_mappings[0], ARRAY_SIZE(mem_mappings));

			// Point buffers to actual data pointers
			aica_ram.data = &virt_ram_base[0x20000000];  // Points to the writable AICA addrspace
			vram.data = &virt_ram_base[0x04000000];   // Points to first vram mirror (writable and lockable)
			mem_b.data = &virt_ram_base[0x0C000000];   // Main memory, first mirror
		}
		else
		{
			_vmem_set_p0_mappings();
			const vmem_mapping mem_mappings[] = {
				// P1
				{0x80000000, 0x80800000,                               0,         0, false},  // Area 0 -> unused
				{0x80800000, 0x80800000 + ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE, true}, // Aica
				{0x80800000 + ARAM_SIZE, 0x82800000,                   0,         0, false},  // unused
				{0x82800000, 0x82800000 + ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE, true}, // Aica mirror
				{0x82800000 + ARAM_SIZE, 0x84000000,                   0,         0, false},  // unused
				{0x84000000, 0x85000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // Area 1 (vram, 16MB, wrapped on DC as 2x8MB)
				{0x85000000, 0x86000000,                               0,         0, false},  // 32 bit path (unused)
				{0x86000000, 0x87000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // VRAM mirror
				{0x87000000, 0x88000000,                               0,         0, false},  // 32 bit path (unused) mirror
				{0x88000000, 0x8C000000,                               0,         0, false},  // Area 2
				{0x8C000000, 0x90000000,            MAP_RAM_START_OFFSET,  RAM_SIZE,  true},  // Area 3 (main RAM + 3 mirrors)
				{0x90000000, 0xA0000000,                               0,         0, false},  // Area 4-7 (unused)
				// P2
				{0xA0000000, 0xA0800000,                               0,         0, false},  // Area 0 -> unused
				{0xA0800000, 0xA0800000 + ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE, true}, // Aica
				{0xA0800000 + ARAM_SIZE, 0xA2800000,                   0,         0, false},  // unused
				{0xA2800000, 0xA2800000 + ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE, true}, // Aica mirror
				{0xA2800000 + ARAM_SIZE, 0xA4000000,                   0,         0, false},  // unused
				{0xA4000000, 0xA5000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // Area 1 (vram, 16MB, wrapped on DC as 2x8MB)
				{0xA5000000, 0xA6000000,                               0,         0, false},  // 32 bit path (unused)
				{0xA6000000, 0xA7000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // VRAM mirror
				{0xA7000000, 0xA8000000,                               0,         0, false},  // 32 bit path (unused) mirror
				{0xA8000000, 0xAC000000,                               0,         0, false},  // Area 2
				{0xAC000000, 0xB0000000,            MAP_RAM_START_OFFSET,  RAM_SIZE,  true},  // Area 3 (main RAM + 3 mirrors)
				{0xB0000000, 0xC0000000,                               0,         0, false},  // Area 4-7 (unused)
				// P3
				{0xC0000000, 0xC0800000,                               0,         0, false},  // Area 0 -> unused
				{0xC0800000, 0xC0800000 + ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE, true}, // Aica
				{0xC0800000 + ARAM_SIZE, 0xC2800000,                   0,         0, false},  // unused
				{0xC2800000, 0xC2800000 + ARAM_SIZE, MAP_ARAM_START_OFFSET, ARAM_SIZE, true}, // Aica mirror
				{0xC2800000 + ARAM_SIZE, 0xC4000000,                   0,         0, false},  // unused
				{0xC4000000, 0xC5000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // Area 1 (vram, 16MB, wrapped on DC as 2x8MB)
				{0xC5000000, 0xC6000000,                               0,         0, false},  // 32 bit path (unused)
				{0xC6000000, 0xC7000000,           MAP_VRAM_START_OFFSET, VRAM_SIZE,  true},  // VRAM mirror
				{0xC7000000, 0xC8000000,                               0,         0, false},  // 32 bit path (unused) mirror
				{0xC8000000, 0xCC000000,                               0,         0, false},  // Area 2
				{0xCC000000, 0xD0000000,            MAP_RAM_START_OFFSET,  RAM_SIZE,  true},  // Area 3 (main RAM + 3 mirrors)
				{0xD0000000, 0x100000000L,                             0,         0, false},  // Area 4-7 (unused)
			};
			vmem_platform_create_mappings(&mem_mappings[0], ARRAY_SIZE(mem_mappings));

			// Point buffers to actual data pointers
			aica_ram.data = &virt_ram_base[0x80800000];  // Points to the first AICA addrspace in P1
			vram.data = &virt_ram_base[0x84000000];   // Points to first vram mirror (writable and lockable) in P1
			mem_b.data = &virt_ram_base[0x8C000000];   // Main memory, first mirror in P1

			vmem_4gb_space = true;
		}

		aica_ram.size = ARAM_SIZE;
		vram.size = VRAM_SIZE;
		mem_b.size = RAM_SIZE;
	}

	// Clear out memory
	aica_ram.Zero();
	vram.Zero();
	mem_b.Zero();

	return true;
}

#define freedefptr(x) \
	if (x) { free(x); x = NULL; }

void _vmem_release() {
	if (virt_ram_base)
		vmem_platform_destroy();
	else {
		freedefptr(p_sh4rcb);
		freedefptr(vram.data);
		freedefptr(aica_ram.data);
		freedefptr(mem_b.data);
	}
}

void _vmem_enable_mmu(bool enable)
{
	if (enable)
	{
		vmem32_init();
	}
	else
	{
		// Restore P0/U0 mem mappings
		vmem32_term();
		_vmem_set_p0_mappings();
	}
}
