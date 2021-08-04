#pragma once

#include "types.h"

#ifdef DC_PLATFORM_DREAMCAST
#include "hw/sh4/sh4_mem.h"
#endif
#define gdxsv_ReadMem32 ReadMem32_nommu
#define gdxsv_ReadMem16 ReadMem16_nommu
#define gdxsv_ReadMem8 ReadMem8_nommu
#define gdxsv_WriteMem32 WriteMem32_nommu
#define gdxsv_WriteMem16 WriteMem16_nommu
#define gdxsv_WriteMem8 WriteMem8_nommu
