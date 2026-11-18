#pragma once
#include "types.h"
#include "stdclass.h"

namespace aica
{

extern RamRegion aica_ram;
extern int aica_schid;
extern int rtc_schid;
extern int dma_sched_id;
u32 GetRTC_now();
template<typename T> T readRtcReg(u32 addr);
template<typename T> void writeRtcReg(u32 addr, T data);
template<typename T> T readAicaReg(u32 addr);
template<typename T> void writeAicaReg(u32 addr, T data);

void init();
void reset();
void term();
void timeStep();
void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

void initRtc();
void resetRtc();
void termRtc();

void setMidiReceiver(void (*handler)(u8 data));
void midiSend(u8 data);

void sbInit();
void sbReset();
void sbTerm();

}
