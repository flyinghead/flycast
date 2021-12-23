/*
	Mostly buggy, old, glue code that somehow still works
	Most of the work is now delegated on vtlb and only helpers are here
*/
#include "types.h"

#include "sh4_mem.h"
#include "hw/holly/sb_mem.h"
#include "sh4_mmr.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/sh4/sh4_core.h"
#include "hw/mem/_vmem.h"
#include "sh4_cache.h"

//main system mem
VArray2 mem_b;

// Memory handlers
ReadMem8Func ReadMem8;
ReadMem16Func ReadMem16;
ReadMem16Func IReadMem16;
ReadMem32Func ReadMem32;
ReadMem64Func ReadMem64;

WriteMem8Func WriteMem8;
WriteMem16Func WriteMem16;
WriteMem32Func WriteMem32;
WriteMem64Func WriteMem64;

//AREA 1
static _vmem_handler area1_32b;

static void map_area1_init()
{
	area1_32b = _vmem_register_handler_Template(pvr_read32p, pvr_write32p);
}

static void map_area1(u32 base)
{
	// VRAM
	
	//Lower 32 mb map
	//64b interface
	_vmem_map_block(vram.data, 0x04 | base, 0x04 | base, VRAM_MASK);
	//32b interface
	_vmem_map_handler(area1_32b, 0x05 | base, 0x05 | base);
	
	//Upper 32 mb mirror
	//0x0600 to 0x07FF
	_vmem_mirror_mapping(0x06 | base, 0x04 | base, 0x02);
}

//AREA 2
static void map_area2_init()
{
}

static void map_area2(u32 base)
{
}

//AREA 3
static void map_area3_init()
{
}

static void map_area3(u32 base)
{
	// System RAM
	_vmem_map_block_mirror(mem_b.data, 0x0C | base,0x0F | base, RAM_SIZE);
}

//AREA 4
static _vmem_handler area4_handler_lower;
static _vmem_handler area4_handler_upper;

static void map_area4_init()
{
	area4_handler_lower = _vmem_register_handler(pvr_read_area4<u8, false>, pvr_read_area4<u16, false>, pvr_read_area4<u32, false>,
									pvr_write_area4<u8, false>, pvr_write_area4<u16, false>, pvr_write_area4<u32, false>);
	area4_handler_upper = _vmem_register_handler(pvr_read_area4<u8, true>, pvr_read_area4<u16, true>, pvr_read_area4<u32, true>,
									pvr_write_area4<u8, true>, pvr_write_area4<u16, true>, pvr_write_area4<u32, true>);
}

static void map_area4(u32 base)
{
	// VRAM 64b/32b interface
	_vmem_map_handler(area4_handler_lower, 0x11 | base, 0x11 | base);
	// upper mirror
	_vmem_map_handler(area4_handler_upper, 0x13 | base, 0x13 | base);
}


//AREA 5	--	Ext. Device
//Read Ext.Device
template <class T>
T DYNACALL ReadMem_extdev_T(u32 addr)
{
	return (T)libExtDevice_ReadMem_A5(addr, sizeof(T));
}

//Write Ext.Device
template <class T>
void DYNACALL WriteMem_extdev_T(u32 addr,T data)
{
	libExtDevice_WriteMem_A5(addr, data, sizeof(T));
}

_vmem_handler area5_handler;
static void map_area5_init()
{
	area5_handler = _vmem_register_handler_Template(ReadMem_extdev_T,WriteMem_extdev_T);
}

static void map_area5(u32 base)
{
	//map whole region to plugin handler
	_vmem_map_handler(area5_handler,base|0x14,base|0x17);
}

//AREA 6	--	Unassigned 
static void map_area6_init()
{
}
static void map_area6(u32 base)
{
}

//set vmem to default values
void mem_map_default()
{
	_vmem_init();

	//U0/P0
	//0x0xxx xxxx	-> normal memmap
	//0x2xxx xxxx	-> normal memmap
	//0x4xxx xxxx	-> normal memmap
	//0x6xxx xxxx	-> normal memmap
	//-----------
	//P1
	//0x8xxx xxxx	-> normal memmap
	//-----------
	//P2
	//0xAxxx xxxx	-> normal memmap
	//-----------
	//P3
	//0xCxxx xxxx	-> normal memmap
	//-----------
	//P4
	//0xExxx xxxx	-> internal area

	//Init Memmaps (register handlers)
	map_area0_init();
	map_area1_init();
	map_area2_init();
	map_area3_init();
	map_area4_init();
	map_area5_init();
	map_area6_init();
	map_area7_init();

	// 00-0C: 7 times the normal memmap mirrors
	for (int i = 0; i < 7; i++)
	{
		map_area0(i << 5); //Bios,Flahsrom,i/f regs,Ext. Device,Sound Ram
		map_area1(i << 5); //VRAM
		map_area2(i << 5); //Unassigned
		map_area3(i << 5); //RAM
		map_area4(i << 5); //TA
		map_area5(i << 5); //Ext. Device
		map_area6(i << 5); //Unassigned
		map_area7(i << 5); //Sh4 Regs
	}

	// E0: p4 region
	map_p4();
}
void mem_Init()
{
	//Allocate mem for memory/bios/flash

	sh4_area0_Init();
	sh4_mmr_init();
}

//Reset Sysmem/Regs -- Pvr is not changed , bios/flash are not zeroed out
void mem_Reset(bool hard)
{
	//mem is reset on hard restart (power on), not soft reset
	if (hard)
	{
		//fill mem w/ 0's
		mem_b.Zero();
	}

	//Reset registers
	sh4_area0_Reset(hard);
	sh4_mmr_reset(true);
}

void mem_Term()
{
	sh4_mmr_term();
	sh4_area0_Term();

	_vmem_term();
}

void WriteMemBlock_nommu_dma(u32 dst, u32 src, u32 size)
{
	bool dst_ismem, src_ismem;
	void* dst_ptr = _vmem_write_const(dst, dst_ismem, 4);
	void* src_ptr = _vmem_read_const(src, src_ismem, 4);

	if (dst_ismem && src_ismem)
	{
		memcpy(dst_ptr, src_ptr, size);
	}
	else if (src_ismem)
	{
		WriteMemBlock_nommu_ptr(dst, (u32*)src_ptr, size);
	}
	else
	{
		verify(size % 4 == 0);
		for (u32 i = 0; i < size; i += 4)
			WriteMem32_nommu(dst + i, ReadMem32_nommu(src + i));
	}
}

void WriteMemBlock_nommu_ptr(u32 dst, const u32 *src, u32 size)
{
	bool dst_ismem;

	void* dst_ptr = _vmem_write_const(dst, dst_ismem, 4);

	if (dst_ismem)
	{
		memcpy(dst_ptr, src, size);
	}
	else
	{
		for (u32 i = 0; i < size;)
		{
			u32 left = size - i;
			if (left >= 4)
			{
				WriteMem32_nommu(dst + i, src[i >> 2]);
				i += 4;
			}
			else if (left >= 2)
			{
				WriteMem16_nommu(dst + i, ((u16 *)src)[i >> 1]);
				i += 2;
			}
			else
			{
				WriteMem8_nommu(dst + i, ((u8 *)src)[i]);
				i++;
			}
		}
	}
}

void WriteMemBlock_nommu_sq(u32 dst, const SQBuffer *src)
{
	// destination address is 32-byte aligned
	bool dst_ismem;
	SQBuffer *dst_ptr = (SQBuffer *)_vmem_write_const(dst, dst_ismem, 4);

	if (dst_ismem)
	{
		*dst_ptr = *src;
	}
	else
	{
		for (u32 i = 0; i < sizeof(SQBuffer); i += 4)
			WriteMem32_nommu(dst + i, *(const u32 *)&src->data[i]);
	}
}

//Get pointer to ram area , 0 if error
//For debugger(gdb) - dynarec
u8* GetMemPtr(u32 Addr, u32 size)
{
	if (((Addr >> 29) & 7) == 7)
		// P4
		return nullptr;
	if (((Addr >> 26) & 7) == 3)
		// Area 3
		return &mem_b[Addr & RAM_MASK];
	return nullptr;
}

static bool interpreterRunning = false;

void SetMemoryHandlers()
{
#ifdef STRICT_MODE
	if (config::DynarecEnabled && interpreterRunning)
	{
		// Flush caches when interp -> dynarec
		ocache.WriteBackAll();
		icache.Invalidate();
	}

	if (!config::DynarecEnabled)
	{
		interpreterRunning = true;
		IReadMem16 = &IReadCachedMem;
		ReadMem8 = &ReadCachedMem<u8>;
		ReadMem16 = &ReadCachedMem<u16>;
		ReadMem32 = &ReadCachedMem<u32>;
		ReadMem64 = &ReadCachedMem<u64>;

		WriteMem8 = &WriteCachedMem<u8>;
		WriteMem16 = &WriteCachedMem<u16>;
		WriteMem32 = &WriteCachedMem<u32>;
		WriteMem64 = &WriteCachedMem<u64>;

		return;
	}
	interpreterRunning = false;
#else
	(void)interpreterRunning;
#endif
	if (CCN_MMUCR.AT == 1 && config::FullMMU)
	{
		IReadMem16 = &mmu_IReadMem16;
		ReadMem8 = &mmu_ReadMem<u8>;
		ReadMem16 = &mmu_ReadMem<u16>;
		ReadMem32 = &mmu_ReadMem<u32>;
		ReadMem64 = &mmu_ReadMem<u64>;

		WriteMem8 = &mmu_WriteMem<u8>;
		WriteMem16 = &mmu_WriteMem<u16>;
		WriteMem32 = &mmu_WriteMem<u32>;
		WriteMem64 = &mmu_WriteMem<u64>;
	}
	else
	{
		ReadMem8 = &_vmem_ReadMem8;
		ReadMem16 = &_vmem_ReadMem16;
		IReadMem16 = &_vmem_ReadMem16;
		ReadMem32 = &_vmem_ReadMem32;
		ReadMem64 = &_vmem_ReadMem64;

		WriteMem8 = &_vmem_WriteMem8;
		WriteMem16 = &_vmem_WriteMem16;
		WriteMem32 = &_vmem_WriteMem32;
		WriteMem64 = &_vmem_WriteMem64;
	}
}
