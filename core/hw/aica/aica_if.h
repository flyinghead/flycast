#pragma once
#include "types.h"
#include "stdclass.h"

extern u32 VREG;
extern VArray2 aica_ram;
u32 GetRTC_now();
template<typename T> T ReadMem_aica_rtc(u32 addr);
template<typename T> void WriteMem_aica_rtc(u32 addr, T data);
template<typename T> T ReadMem_aica_reg(u32 addr);
template<typename T> void WriteMem_aica_reg(u32 addr, T data);

void aica_Init();
void aica_Reset(bool hard);
void aica_Term();
void aica_setMidiReceiver(void (*handler)(u8 data));
void aica_midiSend(u8 data);

void aica_sb_Init();
void aica_sb_Reset(bool hard);
void aica_sb_Term();

s32 libAICA_Init();
void libAICA_Reset(bool hard);
void libAICA_Term();
void libAICA_TimeStep();
