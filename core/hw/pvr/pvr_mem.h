#pragma once
#include "types.h"
#include "stdclass.h"

f32 vrf(u32 addr);
u32 vri(u32 addr);

//vram 32-64b
extern VArray2 vram;

//regs
u32 pvr_ReadReg(u32 addr);
void pvr_WriteReg(u32 paddr,u32 data);

void TAWrite(u32 address,u32* data,u32 count);
extern "C" void DYNACALL TAWriteSQ(u32 address,u8* sqb);

void YUV_init();

template<typename T> T DYNACALL pvr_read_area1(u32 addr);
template<typename T> void DYNACALL pvr_write_area1(u32 addr, T data);
template<typename T, bool upper> T DYNACALL pvr_read_area4(u32 addr);
template<typename T, bool upper> void DYNACALL pvr_write_area4(u32 addr, T data);
