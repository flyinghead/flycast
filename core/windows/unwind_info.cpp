/*
	Copyright (c) 2018, Magnus Norddahl
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
// Based on Asmjit unwind info registration and stack walking code for Windows, Linux and macOS
// https://gist.github.com/dpjudas/925d5c4ffef90bd8114be3b465069fff
#include "oslib/unwind_info.h"
#ifdef _M_X64
#include <windows.h>
#include <dbghelp.h>
#include <algorithm>

#define UWOP_PUSH_NONVOL 0
#define UWOP_ALLOC_LARGE 1
#define UWOP_ALLOC_SMALL 2
#define UWOP_SET_FPREG 3
#define UWOP_SAVE_NONVOL 4
#define UWOP_SAVE_NONVOL_FAR 5
#define UWOP_SAVE_XMM128 8
#define UWOP_SAVE_XMM128_FAR 9
#define UWOP_PUSH_MACHFRAME 10

void UnwindInfo::start(void *address)
{
	startAddr = (u8 *)address;
	codes.clear();
}

void UnwindInfo::pushReg(u32 offset, int reg)
{
	codes.push_back(offset | (UWOP_PUSH_NONVOL << 8) | (reg << 12));
}

void UnwindInfo::allocStack(u32 offset, int size)
{
	verify(size <= 128);
	verify((size & 7) == 0);
	codes.push_back(offset | (UWOP_ALLOC_SMALL << 8) | ((size / 8 - 1) << 12));
}

void UnwindInfo::endProlog(u32 offset)
{
	codes.push_back(0);
	codes.push_back(0);
	std::reverse(codes.begin(), codes.end());
	codes[0] = 1 | (offset  << 8);		// version (1), flags (0) and prolog size (offset)
	codes[1] = (u8)codes.size() - 2;	// unwind codes count
	if (codes.size() & 1)				// table size must be even
		codes.push_back(0);
}

size_t UnwindInfo::end(u32 offset, ptrdiff_t rwRxOffset)
{
	u8 *endAddr = startAddr + offset;
	if ((uintptr_t)endAddr & 3)
		offset += 4 - ((uintptr_t)endAddr & 3);
	u8 *unwindInfo = startAddr + offset;
	memcpy(unwindInfo, &codes[0], codes.size() * sizeof(u16));

	RUNTIME_FUNCTION *table = (RUNTIME_FUNCTION *)(unwindInfo + codes.size() * sizeof(u16));
	table[0].BeginAddress = 0;
	table[0].EndAddress = (DWORD)(endAddr - startAddr);
#ifndef __MINGW64__
	table[0].UnwindInfoAddress = (DWORD)(unwindInfo - startAddr);
#else
	table[0].UnwindData = (DWORD)(unwindInfo - startAddr);
#endif
	bool result = RtlAddFunctionTable(table, 1, (DWORD64)startAddr);
	tables.push_back(table);
	DEBUG_LOG(DYNAREC, "RtlAddFunctionTable %p sz %d rc %d tables: %d", startAddr, table[0].EndAddress, result, (u32)tables.size());

	return (unwindInfo + codes.size() * sizeof(u16) + sizeof(RUNTIME_FUNCTION)) - endAddr;
}

void UnwindInfo::clear()
{
	DEBUG_LOG(DYNAREC, "UnwindInfo::clear");
	for (RUNTIME_FUNCTION *table : tables)
		RtlDeleteFunctionTable(table);
	tables.clear();
}

void UnwindInfo::registerFrame(void *frame)
{
}

void UnwindInfo::deregisterFrame(void *frame)
{
}

#endif
