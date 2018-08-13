/*
**	naomi.h
*/

#pragma once

void naomi_reg_Init();
void naomi_reg_Term();
void naomi_reg_Reset(bool Manual);

void Update_naomi();

u32  ReadMem_naomi(u32 Addr, u32 sz);
void WriteMem_naomi(u32 Addr, u32 data, u32 sz);

void NaomiBoardIDWrite(const u16 Data);
void NaomiBoardIDWriteControl(const u16 Data);
u16 NaomiBoardIDRead();

typedef u16 (*getNaomiAxisFP)();

struct NaomiInputMapping {
	getNaomiAxisFP axis[4];
	u8 button_mapping_byte[16];
	u8 button_mapping_mask[16];
};

extern NaomiInputMapping Naomi_Mapping;




















