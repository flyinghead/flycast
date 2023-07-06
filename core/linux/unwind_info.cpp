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
#include "build.h"
#include "oslib/oslib.h"

extern "C"
{
#if HOST_CPU != CPU_ARM
	void __register_frame(const void*);
	void __deregister_frame(const void*);
#else
	void __register_frame(const void *) {
	}
	void __deregister_frame(const void *) {
	}
#endif
}

#if HOST_CPU == CPU_X64

constexpr int dwarfRegId[16] = {
	0,		// RAX
	2,		// RCX
	1,		// RDX
	3,		// RBX
	7,		// RSP
	6,		// RBP
	4,		// RSI
	5,		// RDI
	8,		// R8
	9,		// R9
	10,		// R10
	11,		// R11
	12,		// R12
	13,		// R13
	14,		// R14
	15,		// R15
};
inline static int registerId(int x64Id) {
	return dwarfRegId[x64Id];
}
constexpr int dwarfRegRAId = 16;
constexpr int dwarfRegXmmId = 17;
constexpr int dwarfRegSP = dwarfRegId[4];	// RSP

#elif HOST_CPU == CPU_X86

inline static int registerId(int x86Id) {
	return x86Id;
}
constexpr int dwarfRegRAId = 8;
constexpr int dwarfRegSP = 4;	// ESP

#elif HOST_CPU == CPU_ARM64
// https://developer.arm.com/documentation/ihi0057/latest
//
// register codes:
// x0-x30		0 - 30
// SP			31
// d0-d31		64 - 95

inline static int registerId(int armId) {
	return armId;
}
constexpr int dwarfRegRAId = 30;
constexpr int dwarfRegSP = 31;

#elif HOST_CPU == CPU_ARM

inline static int registerId(int armId) {
	return armId;
}
constexpr int dwarfRegRAId = 14;
constexpr int dwarfRegSP = 13;

#endif

#if HOST_CPU == CPU_X64 || HOST_CPU == CPU_ARM64 || HOST_CPU == CPU_X86 || (HOST_CPU == CPU_ARM && defined(__ANDROID__))

#include <cstring>
#include <vector>

using ByteStream = std::vector<u8>;

static void writeLength(ByteStream &stream, u32 pos, u32 v)
{
	*(u32*)(&stream[pos]) = v;
}

template<typename T>
void write(ByteStream &stream, T v)
{
	for (size_t i = 0; i < sizeof(T); i++)
		stream.push_back(v >> (i * 8));
}

static void writeULEB128(ByteStream &stream, u32 v)
{
	while (true)
	{
		if (v < 128)
		{
			write<u8>(stream, v);
			break;
		}
		else
		{
			write<u8>(stream, (v & 0x7f) | 0x80);
			v >>= 7;
		}
	}
}

static void writeSLEB128(ByteStream &stream, int32_t v)
{
	bool more;
	do {
		u8 byte = v & 0x7f;
		v >>= 7;
		more = !(((v == 0 && (byte & 0x40) == 0) ||
	               (v == -1 && (byte & 0x40) != 0)));
		if (more)
			byte |= 0x80; // Mark this byte to show that more bytes will follow.
		write<u8>(stream, byte);
	} while (more);
}

static void writePadding(ByteStream &stream)
{
	int padding = stream.size() % sizeof(uintptr_t);
	if (padding != 0)
	{
		padding = sizeof(uintptr_t) - padding;
		for (int i = 0; i < padding; i++)
			write<u8>(stream, 0);
	}
}

static void writeCIE(ByteStream &stream, const ByteStream &cieInstructions, u8 returnAddressReg)
{
	u32 lengthPos = stream.size();
	write<u32>(stream, 0);		// Length
	write<u32>(stream, 0);		// CIE ID

	write<u8>(stream, 1);		// CIE Version
	write<u8>(stream, 'z');
	write<u8>(stream, 'R');		// fde encoding
	write<u8>(stream, 0);
	writeULEB128(stream, 1);	// code alignment. Loc multiplier
	writeSLEB128(stream, -1);	// data alignment. Stack offset multiplier.
	writeULEB128(stream, returnAddressReg);

	writeULEB128(stream, 1);	// LEB128 augmentation size
	write<u8>(stream, 0);		// DW_EH_PE_absptr (FDE uses absolute pointers)

	stream.insert(stream.end(), cieInstructions.begin(), cieInstructions.end());

	writePadding(stream);
	writeLength(stream, lengthPos, stream.size() - lengthPos - 4);
}

static void writeFDE(ByteStream &stream, const ByteStream &fdeInstructions, u32 cieLocation, u32 &functionStart)
{
	u32 lengthPos = stream.size();
	write<u32>(stream, 0); // Length
	u32 offsetToCIE = stream.size() - cieLocation;
	write<u32>(stream, offsetToCIE);

	functionStart = stream.size();
	write<uintptr_t>(stream, 0); // func start
	write<uintptr_t>(stream, 0); // func size

	writeULEB128(stream, 0); // LEB128 augmentation size

	stream.insert(stream.end(), fdeInstructions.begin(), fdeInstructions.end());

	writePadding(stream);
	writeLength(stream, lengthPos, stream.size() - lengthPos - 4);
}

static void writeAdvanceLoc(ByteStream &fdeInstructions, uintptr_t offset, uintptr_t &lastOffset)
{
	uintptr_t delta = offset - lastOffset;
	if (delta == 0)
		return;
	if (delta < (1 << 6))
	{
		write<u8>(fdeInstructions, (1 << 6) | delta);	// DW_CFA_advance_loc
	}
	else if (delta < (1 << 8))
	{
		write<u8>(fdeInstructions, 2);					// DW_CFA_advance_loc1
		write<u8>(fdeInstructions, delta);
	}
	else if (delta < (1 << 16))
	{
		write<u8>(fdeInstructions, 3);					// DW_CFA_advance_loc2
		write<u16>(fdeInstructions, delta);
	}
	else
	{
		write<u8>(fdeInstructions, 4);					// DW_CFA_advance_loc3
		write<u32>(fdeInstructions, delta);
	}
	lastOffset = offset;
}

static void writeDefineCFA(ByteStream &cieInstructions, int dwarfRegId, int stackOffset)
{
	write<u8>(cieInstructions, 0x0c);					// DW_CFA_def_cfa
	writeULEB128(cieInstructions, dwarfRegId);
	writeULEB128(cieInstructions, stackOffset);
}

static void writeDefineStackOffset(ByteStream &fdeInstructions, int stackOffset)
{
	write<u8>(fdeInstructions, 0x0e);					// DW_CFA_def_cfa_offset
	writeULEB128(fdeInstructions, stackOffset);
}

static void writeRegisterStackLocation(ByteStream &instructions, int dwarfRegId, int stackLocation)
{
	write<u8>(instructions, (2 << 6) | dwarfRegId);		// DW_CFA_offset
	writeULEB128(instructions, stackLocation);
}

static void writeRegisterStackLocationExtended(ByteStream &instructions, int dwarfRegId, int stackLocation)
{
	write<u8>(instructions, 5);							// DW_CFA_offset_extended
	writeULEB128(instructions, dwarfRegId);
	writeULEB128(instructions, stackLocation);
}

void UnwindInfo::start(void *address)
{
	startAddr = (u8 *)address;
#if HOST_CPU == CPU_X64 || HOST_CPU == CPU_X86
	stackOffset = sizeof(uintptr_t);
#else
	stackOffset = 0;
#endif
	lastOffset = 0;
	cieInstructions.clear();
	fdeInstructions.clear();
	writeDefineCFA(cieInstructions, dwarfRegSP, stackOffset);
	if (stackOffset > 0)
		// Return address pushed on stack
		writeRegisterStackLocation(cieInstructions, dwarfRegRAId, stackOffset);
}

void UnwindInfo::pushReg(u32 offset, int reg)
{
	stackOffset += sizeof(uintptr_t);
	writeAdvanceLoc(fdeInstructions, offset, lastOffset);
	writeDefineStackOffset(fdeInstructions, stackOffset);
	writeRegisterStackLocation(fdeInstructions, registerId(reg), stackOffset);
}

void UnwindInfo::saveReg(u32 offset, int reg, int stackOffset)
{
	writeAdvanceLoc(fdeInstructions, offset, lastOffset);
	writeRegisterStackLocation(fdeInstructions, registerId(reg), stackOffset);
}

void UnwindInfo::saveExtReg(u32 offset, int reg, int stackOffset)
{
	writeAdvanceLoc(fdeInstructions, offset, lastOffset);
	writeRegisterStackLocationExtended(fdeInstructions, registerId(reg), stackOffset);
}

void UnwindInfo::allocStack(u32 offset, int size)
{
	stackOffset += size;
	writeAdvanceLoc(fdeInstructions, offset, lastOffset);
	writeDefineStackOffset(fdeInstructions, stackOffset);
}

void UnwindInfo::endProlog(u32 offset)
{
}

size_t UnwindInfo::end(u32 offset, ptrdiff_t rwRxOffset)
{
	ByteStream unwindInfo;
	writeCIE(unwindInfo, cieInstructions, dwarfRegRAId);
	u32 functionStart;
	writeFDE(unwindInfo, fdeInstructions, 0, functionStart);
	write<u32>(unwindInfo, 0);

	u8 *endAddr = startAddr + offset;
	// 16 bytes alignment
	if ((uintptr_t)endAddr & 0xf)
		offset += 16 - ((uintptr_t)endAddr & 0xf);
	u8 *unwindInfoDest = startAddr + offset;
	memcpy(unwindInfoDest, &unwindInfo[0], unwindInfo.size());

	if (!unwindInfo.empty())
	{
		uintptr_t *unwindfuncaddr = (uintptr_t *)(unwindInfoDest + functionStart);
		unwindfuncaddr[0] = (uintptr_t)startAddr + rwRxOffset;
		unwindfuncaddr[1] = (ptrdiff_t)(endAddr - startAddr);

// Android NDK 23 switched to using clang's unwind instead of gcc
#if defined(__llvm__) && (!defined(__NDK_MAJOR__) || __NDK_MAJOR__ >= 23 || HOST_CPU == CPU_ARM)
		// With clang __register_frame takes a single FDE as an argument
		u8 *entry = unwindInfoDest;
		while (true)
		{
			u32 length = *((u32 *)entry);
			if (length == 0)
				break;

			if (length == 0xffffffff)
			{
				u64 length64 = *((u64 *)(entry + 4));
				if (length64 == 0)
					break;

				u64 offset = *((u64 *)(entry + 12));
				if (offset != 0)
				{
					registerFrame(entry);
					registeredFrames.push_back(entry);
				}
				entry += length64 + 12;
			}
			else
			{
				u32 offset = *((u32 *)(entry + 4));
				if (offset != 0)
				{
					registerFrame(entry);
					registeredFrames.push_back(entry);
				}
				entry += length + 4;
			}
		}
#else
		// With gcc it takes a pointer to the entire .eh_frame
		registerFrame(unwindInfoDest);
		registeredFrames.push_back(unwindInfoDest);
#endif
	}
	DEBUG_LOG(DYNAREC, "RegisterFrame %p sz %d tables: %d", startAddr, (u32)(endAddr - startAddr), (u32)registeredFrames.size());

	return (unwindInfoDest + unwindInfo.size()) - endAddr;
}

void UnwindInfo::clear()
{
	DEBUG_LOG(DYNAREC, "UnwindInfo::clear");
	for (u8 *frame : registeredFrames)
		deregisterFrame(frame);
	registeredFrames.clear();
}

void UnwindInfo::registerFrame(void *frame)
{
	__register_frame(frame);
}

void UnwindInfo::deregisterFrame(void *frame)
{
	__deregister_frame(frame);
}

#if HOST_CPU == CPU_ARM
#include "rec-ARM/arm_unwind.h"

bool flycastFindDynarecFDE(uintptr_t targetAddr, uintptr_t &fde)
{
	return ArmUnwindInfo::findFDE(targetAddr, fde);
}
#endif

#endif
