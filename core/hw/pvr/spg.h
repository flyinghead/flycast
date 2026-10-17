#pragma once
#include "ta_ctx.h"

extern bool SH4FastEnough;

bool spg_Init();
void spg_Term();
void spg_Reset(bool Manual);
void spg_Serialize(Serializer& ser);
void spg_Deserialize(Deserializer& deser);

void CalculateSync();
// Atomic snapshot of the emulated video field duration for presentation threads.
double spg_GetFrameDuration();
// Capture on the emulation thread, then carry with the queued video image.
double spg_GetVideoTime();
void read_lightgun_position(int x, int y);
void scheduleRenderDone(TA_context *cntx);
void rescheduleSPG();
