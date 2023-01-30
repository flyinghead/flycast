#pragma once
#include "types.h"
#include "stdclass.h"

namespace aica
{

extern u32 VREG;
extern RamRegion aica_ram;
u32 GetRTC_now();
template<typename T> T ReadMem_aica_rtc(u32 addr);
template<typename T> void WriteMem_aica_rtc(u32 addr, T data);
template<typename T> T ReadMem_aica_reg(u32 addr);
template<typename T> void WriteMem_aica_reg(u32 addr, T data);

void init();
void reset(bool hard);
void term();
void timeStep();
void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

void initRtc();
void resetRtc(bool hard);
void termRtc();

void setMidiReceiver(void (*handler)(u8 data));
void midiSend(u8 data);

void sbInit();
void sbReset(bool hard);
void sbTerm();

}
