/*
**	naomi.h
*/

#pragma once

void naomi_reg_Init();
void naomi_reg_Term();
void naomi_reg_Reset(bool hard);
void naomi_Serialize(void **data, unsigned int *total_size);
void naomi_Unserialize(void **data, unsigned int *total_size, serialize_version_enum version);

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
