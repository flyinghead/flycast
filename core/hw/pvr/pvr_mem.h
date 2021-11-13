#pragma once
#include "types.h"
#include "stdclass.h"
#include "hw/sh4/sh4_if.h"

//vram 32-64b
extern VArray2 vram;

//regs
u32 pvr_ReadReg(u32 addr);
void pvr_WriteReg(u32 paddr, u32 data);

void DYNACALL TAWrite(u32 address, const SQBuffer *data, u32 count);
void DYNACALL TAWriteSQ(u32 address, const SQBuffer *sqb);

void YUV_init();
void YUV_serialize(Serializer& ser);
void YUV_deserialize(Deserializer& deser);

// 32-bit vram path handlers
template<typename T> T DYNACALL pvr_read32p(u32 addr);
template<typename T> void DYNACALL pvr_write32p(u32 addr, T data);
// Area 4 handlers
template<typename T, bool upper> T DYNACALL pvr_read_area4(u32 addr);
template<typename T, bool upper> void DYNACALL pvr_write_area4(u32 addr, T data);
