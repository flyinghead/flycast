#pragma once

#include "hw/sh4/sh4_mem.h"
#include "types.h"

inline u32 gdxsv_ReadMem32(u32 addr) { return ReadMem32_nommu(addr); }
inline u16 gdxsv_ReadMem16(u32 addr) { return ReadMem16_nommu(addr); }
inline u8 gdxsv_ReadMem8(u32 addr) { return ReadMem8_nommu(addr); }

inline u32 gdxsv_ReadMem(int bits, u32 addr) {
	if (bits == 32) return gdxsv_ReadMem32(addr);
	if (bits == 16) return gdxsv_ReadMem16(addr);
	if (bits == 8) return gdxsv_ReadMem8(addr);
	verify(false);
}

inline void gdxsv_WriteMem32(u32 addr, u32 value) {
	if (ReadMem32_nommu(addr) != value) WriteMem32_nommu(addr, value);
}

inline void gdxsv_WriteMem16(u32 addr, u16 value) {
	if (ReadMem16_nommu(addr) != value) WriteMem16_nommu(addr, value);
}

inline void gdxsv_WriteMem8(u32 addr, u8 value) {
	if (ReadMem8_nommu(addr) != value) WriteMem8_nommu(addr, value);
}

inline void gdxsv_WriteMem(int bits, u32 addr, u32 value) {
	if (bits == 32)
		gdxsv_WriteMem32(addr, value);
	else if (bits == 16)
		gdxsv_WriteMem16(addr, value & 0xffffu);
	else if (bits == 8)
		gdxsv_WriteMem8(addr, value & 0xffu);
	else
		verify(false);
}

template <typename T>
static bool future_is_ready(const T& future) {
	return future.valid() && future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}
