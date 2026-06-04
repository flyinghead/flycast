/*
**	naomi.h
*/
#pragma once
#include "types.h"

void naomi_reg_Init();
void naomi_reg_Term();
void naomi_reg_Reset(bool hard);
void naomi_Serialize(Serializer& ser);
void naomi_Deserialize(Deserializer& deser);

u32  ReadMem_naomi(u32 Addr, u32 size);
void WriteMem_naomi(u32 Addr, u32 data, u32 size);

void NaomiBoardIDWrite(u16 data);
u16 NaomiBoardIDRead();
u16 NaomiGameIDRead();
void NaomiGameIDWrite(u16 data);
void setGameSerialId(const u8 *data);
const u8 *getGameSerialId();

void initDriveSimSerialPipe();
void Naomi_setDmaDelay();

//Area 0 , 0x01000000- 0x01FFFFFF      [G2 Ext. Device]
u32 g2ext_readMem(u32 addr, u32 size);
void g2ext_writeMem(u32 addr, u32 data, u32 size);
