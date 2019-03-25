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
u16 NaomiGameIDRead();
void NaomiGameIDWrite(const u16 Data);
void naomi_process(u32 r3c,u32 r40,u32 r44, u32 r48);

typedef u16 (*getNaomiAxisFP)();

struct NaomiInputMapping {
	getNaomiAxisFP axis[8];
	u8 button_mapping_byte[16];
	u8 button_mapping_mask[16];
};

extern NaomiInputMapping Naomi_Mapping;

extern u32 reg_dimm_3c;			//IO window ! written, 0x1E03 some flag ?
extern u32 reg_dimm_40;			//parameters
extern u32 reg_dimm_44;			//parameters
extern u32 reg_dimm_48;			//parameters
extern u32 reg_dimm_4c;			//status/control reg ?

extern bool NaomiDataRead;
extern u32 naomi_updates;
