#pragma once
#include "types.h"

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
//#define ReadMem64(addr,reg) {  ((u32*)reg)[0]=_vmem_ReadMem32(addr);((u32*)reg)[1]=_vmem_ReadMem32((addr)+4); }

#define WriteMem8 _vmem_WriteMem8
#define WriteMem16 _vmem_WriteMem16
#define WriteMem32 _vmem_WriteMem32
#define WriteMem64 _vmem_WriteMem64
//#define WriteMem64(addr,reg) {  _vmem_WriteMem32(addr,((u32*)reg)[0]);_vmem_WriteMem32((addr)+4, ((u32*)reg)[1]); }
#else

typedef u8 (*ReadMem8Func)(u32 addr);
typedef u16 (*ReadMem16Func)(u32 addr);
typedef u32 (*ReadMem32Func)(u32 addr);
typedef u64 (*ReadMem64Func)(u32 addr);

typedef void (*WriteMem8Func)(u32 addr, u8 data);
typedef void (*WriteMem16Func)(u32 addr, u16 data);
typedef void (*WriteMem32Func)(u32 addr, u32 data);
typedef void (*WriteMem64Func)(u32 addr, u64 data);

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

void WriteMemBlock_ptr(u32 dst,u32* src,u32 size);
void WriteMemBlock_nommu_ptr(u32 dst,u32* src,u32 size);
void WriteMemBlock_nommu_sq(u32 dst,u32* src);
void WriteMemBlock_nommu_dma(u32 dst,u32 src,u32 size);
//Init/Res/Term
void mem_Init();
void mem_Term();
void mem_Reset(bool Manual);
void mem_map_default();

//Generic read/write functions for debugger
bool ReadMem_DB(u32 addr,u32& data,u32 size );
bool WriteMem_DB(u32 addr,u32 data,u32 size );

//Get pointer to ram area , 0 if error
//For debugger(gdb) - dynarec
u8* GetMemPtr(u32 Addr,u32 size);

//Get infomation about an area , eg ram /size /anything
//For dynarec - needs to be done
struct MemInfo
{
	//MemType:
	//Direct ptr   , just read/write to the ptr
	//Direct call  , just call for read , ecx=data on write (no address)
	//Generic call , ecx=addr , call for read , edx=data for write
	u32 MemType;
	
	//todo
	u32 Flags;

	void* read_ptr;
	void* write_ptr;
};

void GetMemInfo(u32 addr,u32 size,MemInfo* meminfo);

bool IsOnRam(u32 addr);


u32 GetRamPageFromAddress(u32 RamAddress);


bool LoadRomFiles(const string& root);
void SaveRomFiles(const string& root);
bool LoadHle(const string& root);
