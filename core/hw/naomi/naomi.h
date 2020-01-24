/*
**	naomi.h
*/

#pragma once

void naomi_reg_Init();
void naomi_reg_Term();
void naomi_reg_Reset(bool Manual);

u32  ReadMem_naomi(u32 Addr, u32 sz);
void WriteMem_naomi(u32 Addr, u32 data, u32 sz);

void NaomiBoardIDWrite(const u16 Data);
void NaomiBoardIDWriteControl(const u16 Data);
u16 NaomiBoardIDRead();
u16 NaomiGameIDRead();
void NaomiGameIDWrite(const u16 Data);
void naomi_process(u32 r3c,u32 r40,u32 r44, u32 r48);

extern u32 reg_dimm_command;	// command, written, 0x1E03 some flag ?
extern u32 reg_dimm_offsetl;
extern u32 reg_dimm_parameterl;
extern u32 reg_dimm_parameterh;
extern u32 reg_dimm_status;
