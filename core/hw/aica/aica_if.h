#pragma once
#include "types.h"
#include "stdclass.h"

namespace aica
{

extern RamRegion aica_ram;
u32 GetRTC_now();
template<typename T> T readRtcReg(u32 addr);
template<typename T> void writeRtcReg(u32 addr, T data);
template<typename T> T readAicaReg(u32 addr);
template<typename T> void writeAicaReg(u32 addr, T data);

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
