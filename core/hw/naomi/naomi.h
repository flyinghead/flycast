/*
**	naomi.h
*/
#pragma once
#include "types.h"
#include "multiboard.h"

void naomi_reg_Init();
void naomi_reg_Term();
void naomi_reg_Reset(bool hard);
void naomi_Serialize(Serializer& ser);
void naomi_Deserialize(Deserializer& deser);

u32  ReadMem_naomi(u32 Addr, u32 size);
void WriteMem_naomi(u32 Addr, u32 data, u32 size);

void NaomiBoardIDWrite(u16 Data);
void NaomiBoardIDWriteControl(u16 Data);
u16 NaomiBoardIDRead();
u16 NaomiGameIDRead();
void NaomiGameIDWrite(u16 Data);
void naomi_process(u32 command,u32 offsetl,u32 parameterl, u32 parameterh);

extern u32 reg_dimm_command;	// command, written, 0x1E03 some flag ?
extern u32 reg_dimm_offsetl;
extern u32 reg_dimm_parameterl;
extern u32 reg_dimm_parameterh;
extern u32 reg_dimm_status;

void initMidiForceFeedback();
void initDriveSimSerialPipe();

u32  libExtDevice_ReadMem_A0_006(u32 addr, u32 size);
void libExtDevice_WriteMem_A0_006(u32 addr, u32 data, u32 size);

extern Multiboard *multiboard;
//Area 0 , 0x01000000- 0x01FFFFFF      [G2 Ext. Device]
static inline u32 g2ext_readMem(u32 addr, u32 size)
{
	if (multiboard != nullptr)
		return multiboard->readG2Ext(addr, size);

	INFO_LOG(NAOMI, "Unhandled G2 Ext read<%d> at %x", size, addr);
	return 0;
}
static inline void g2ext_writeMem(u32 addr, u32 data, u32 size)
{
	if (multiboard != nullptr)
		multiboard->writeG2Ext(addr, size, data);
	else
		INFO_LOG(NAOMI, "Unhandled G2 Ext write<%d> at %x: %x", size, addr, data);
}
