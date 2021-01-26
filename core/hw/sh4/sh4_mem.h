#pragma once
#include "types.h"
#include "stdclass.h"

//main system mem
extern VArray2 mem_b;

#include "hw/mem/_vmem.h"
#include "modules/mmu.h"

#ifdef NO_MMU
#define ReadMem8 _vmem_ReadMem8
#define ReadMem16 _vmem_ReadMem16
#define IReadMem16 ReadMem16
#define ReadMem32 _vmem_ReadMem32
#define ReadMem64 _vmem_ReadMem64

#define WriteMem8 _vmem_WriteMem8
#define WriteMem16 _vmem_WriteMem16
#define WriteMem32 _vmem_WriteMem32
#define WriteMem64 _vmem_WriteMem64
#ifdef STRICT_MODE
#error Strict mode requires the MMU
#endif

#else

#ifdef _MSC_VER
	typedef u8 (DYNACALL *ReadMem8Func)(u32 addr);
	typedef u16 (DYNACALL *ReadMem16Func)(u32 addr);
	typedef u32 (DYNACALL *ReadMem32Func)(u32 addr);
	typedef u64 (DYNACALL *ReadMem64Func)(u32 addr);

	typedef void (DYNACALL *WriteMem8Func)(u32 addr, u8 data);
	typedef void (DYNACALL *WriteMem16Func)(u32 addr, u16 data);
	typedef void (DYNACALL *WriteMem32Func)(u32 addr, u32 data);
	typedef void (DYNACALL *WriteMem64Func)(u32 addr, u64 data);
#else
	typedef u8 DYNACALL (*ReadMem8Func)(u32 addr);
	typedef u16 DYNACALL (*ReadMem16Func)(u32 addr);
	typedef u32 DYNACALL (*ReadMem32Func)(u32 addr);
	typedef u64 DYNACALL (*ReadMem64Func)(u32 addr);

	typedef void DYNACALL (*WriteMem8Func)(u32 addr, u8 data);
	typedef void DYNACALL (*WriteMem16Func)(u32 addr, u16 data);
	typedef void DYNACALL (*WriteMem32Func)(u32 addr, u32 data);
	typedef void DYNACALL (*WriteMem64Func)(u32 addr, u64 data);
#endif

extern ReadMem8Func ReadMem8;
extern ReadMem16Func ReadMem16;
extern ReadMem16Func IReadMem16;
extern ReadMem32Func ReadMem32;
extern ReadMem64Func ReadMem64;

extern WriteMem8Func WriteMem8;
extern WriteMem16Func WriteMem16;
extern WriteMem32Func WriteMem32;
extern WriteMem64Func WriteMem64;

#endif

#define ReadMem8_nommu _vmem_ReadMem8
#define ReadMem16_nommu _vmem_ReadMem16
#define IReadMem16_nommu _vmem_IReadMem16
#define ReadMem32_nommu _vmem_ReadMem32

#define WriteMem8_nommu _vmem_WriteMem8
#define WriteMem16_nommu _vmem_WriteMem16
#define WriteMem32_nommu _vmem_WriteMem32

void WriteMemBlock_nommu_ptr(u32 dst,u32* src,u32 size);
void WriteMemBlock_nommu_sq(u32 dst,u32* src);
void WriteMemBlock_nommu_dma(u32 dst,u32 src,u32 size);

//Init/Res/Term
void mem_Init();
void mem_Term();
void mem_Reset(bool Manual);
void mem_map_default();

//Get pointer to ram area , 0 if error
//For debugger(gdb) - dynarec
u8* GetMemPtr(u32 Addr,u32 size);

static inline bool IsOnRam(u32 addr)
{
	// in area 3 but not in P4
	return ((addr >> 26) & 7) == 3 && ((addr >> 29) & 7) != 7;
}

void SetMemoryHandlers();
