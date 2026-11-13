/*
	Sega Hikaru GPU (315-6083A): registers, interrupts, texture upload engine, display timing.

	Hardware notes are in HIKARU_SPEC.md (sections 5, 6, 9 and 11-13).

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
#include "hikaru_gpu.h"
#include "hikaru.h"
#include "hikaru_cp.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "hikaru_render.h"
#include "emulator.h"
#include "hikaru_slave.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/mem/addrspace.h"
#include "stdclass.h"
#include "serialize.h"

#include <cstring>

namespace hikaru::gpu
{

// 0x15000000-0x150000FF
static u32 regs15[0x40];
// 0x1A000000-0x1A00013F
static u32 regs1A[0x50];
// 0x1A000180-0x1A0001BF and 0x1A000200-0x1A00023F (2D layer control)
static u32 regs1A180[0x10];
static u32 regs1A200[0x10];
// 0x1A040000-0x1A04000C: rectangle copy unit
static u32 copyRegs[4];

enum {
	// regs15 indices
	TEX_LIST_ADDR = 0x0c / 4,
	TEX_LIST_COUNT = 0x10 / 4,
	TEX_LIST_ENABLE = 0x14 / 4,
	CP_START = 0x58 / 4,
	IRQ_MASK = 0x84 / 4,
	IRQ_STATUS = 0x88 / 4,
	// regs1A indices
	IRQ_LATCH0 = 0x08 / 4,	// 4 latches: 0x08, 0x0C, 0x10, 0x14
	IRQ_SUMMARY = 0x18 / 4,
	DISPLAY_STATUS = 0x1c / 4,
	CP_STATUS = 0x24 / 4,
};

// IRQ_STATUS bits
constexpr u32 IRQ_TEX_UPLOAD = 1;
constexpr u32 IRQ_VBLANK = 2;
constexpr u32 IRQ_CP_DONE = 4;
constexpr u32 IRQ_LATCHES = 0x80;

// Display timing (from the board timers): one line every 6355 SH-4 cycles (31.47 kHz),
// 544 lines per frame
constexpr int LINE_CYCLES = 0x18d3;
constexpr u32 LINES_PER_FRAME = 0x220;
constexpr u32 MID_FRAME_LINE = 0xf0;

static u32 currentLine;
static int lineSchedId = -1;
static int cpSchedId = -1;

static void updateIrq()
{
	u32 summary = 0;
	for (int i = 0; i < 4; i++)
		summary |= (regs1A[IRQ_LATCH0 + i] & 1) << i;
	regs1A[IRQ_SUMMARY] = summary;
	if (summary != 0)
		regs15[IRQ_STATUS] |= IRQ_LATCHES;
	// The GPU drives the master's IRL2 pin (BIOS interrupt handler for INTEVT 0x300)
	bool irq = (regs15[IRQ_MASK] & regs15[IRQ_STATUS]) != 0;
	InterruptPend(sh4_IRL2_INDEP, irq);
}

u32 displayStatus() {
	return regs1A[DISPLAY_STATUS];
}

u32 layerReg(int index) {
	return regs1A180[index & 0xf];
}

void textureUploaded(u32 region)
{
	(void)region;
}

static u32 bandGen[TEXRAM_SIZE >> 16];
static u32 texGen = 1;

void texRamWritten(u32 offset, u32 size)
{
	if (size == 0)
		return;
	texGen++;
	u32 first = offset >> 16;
	u32 last = (offset + size - 1) >> 16;
	for (u32 b = first; b <= last && b < std::size(bandGen); b++)
		bandGen[b] = texGen;
}

u32 texRamGeneration(u32 offset, u32 size)
{
	u32 gen = 0;
	if (size == 0)
		return gen;
	u32 first = offset >> 16;
	u32 last = (offset + size - 1) >> 16;
	for (u32 b = first; b <= last && b < std::size(bandGen); b++)
		gen = std::max(gen, bandGen[b]);
	return gen;
}

u32 texRamCurrentGeneration() {
	return texGen;
}

// Texture upload engine: processes a list of 16-byte descriptors in command RAM,
// copying texel data (with its mip chain) into texture RAM pages A/B.
static void processTextureUploads()
{
	if ((regs15[TEX_LIST_ENABLE] & 1) == 0 || regs15[TEX_LIST_COUNT] == 0)
		return;

	u8 *texRam = getTexRam();
	// The length register reads back unchanged after the transfer: the game adds it to its
	// count of uploaded entries, which its loaders wait on (Odin, Demul)
	u32 count = regs15[TEX_LIST_COUNT] & 0xffff;
	while (count != 0)
	{
		u32 descAddr = regs15[TEX_LIST_ADDR] & (CMDRAM_SIZE - 1) & ~3;
		u32 desc[4];
		memcpy(desc, getCmdRam() + descAddr, sizeof(desc));
		u32 texelCount = desc[1];
		if (texelCount != 0)
		{
			u32 srcSel = desc[0] >> 24;
			const u8 *src;
			u32 srcMask;
			if (srcSel == 0x48 || srcSel == 0x4c) {
				src = getCmdRam();
				srcMask = CMDRAM_SIZE / 2 - 1;
			}
			else {
				src = getSlaveRam();
				srcMask = SLAVE_RAM_SIZE / 2 - 1;
			}
			u32 srcIdx = (desc[0] >> 1) & srcMask;	// in 16-bit units
			u32 x = (desc[2] & 0x7f) << 4;
			u32 y = ((desc[2] >> 8) & 0x3f) << 4;
			u32 width = 16 << ((desc[2] >> 16) & 7);
			u32 height = 16 << ((desc[2] >> 19) & 7);
			bool pageB = desc[3] & 1;
			u32 xOffset = 0, yOffset = 0;
			for (int level = 0; texelCount != 0 && width != 0 && height != 0; )
			{
				texelCount -= std::min(texelCount, width * height);
				u16 *dst = (u16 *)(texRam + (pageB ? 0xc00000 : 0x800000));
				texRamWritten((pageB ? 0xc00000 : 0x800000) + (y + yOffset) * 4096, height * 4096);
				for (u32 row = 0; row < height; row++)
				{
					u16 *line = dst + (y + yOffset + row) * 2048 + x + xOffset;
					for (u32 col = 0; col < width; col++)
					{
						memcpy(&line[col], src + (srcIdx & srcMask) * 2, 2);
						srcIdx++;
					}
				}
				level++;
				x >>= 1;
				y >>= 1;
				pageB = !pageB;
				xOffset += 0x800 >> level;
				yOffset += 0x400 >> level;
				width >>= 1;
				height >>= 1;
			}
			textureUploaded(desc[2] | (desc[3] << 31));
			regs15[TEX_LIST_ADDR] += 0x10;
		}
		count--;
	}
	regs15[IRQ_STATUS] |= IRQ_TEX_UPLOAD;
	regs15[TEX_LIST_ENABLE] = 0;
	updateIrq();
}

// Rectangle copy inside the framebuffer RAM (low 8 MB of texture RAM, 2048 16-bit pixels per row).
// Coordinates are in pixels. The two pixels of each 32-bit word are stored swapped, so a copy
// between columns of different parity follows the pixels, not the memory order.
// The copy is immediate: the busy bit (0x1A000024 bit 0) is never left set, since games poll it
// before queueing the next copy.
static void rectangleCopy()
{
	u16 *texRam = (u16 *)getTexRam();
	u32 w = copyRegs[2] & 0xffff;
	u32 h = copyRegs[2] >> 16;
	u32 srcX = copyRegs[0] & 0x7ff;
	u32 srcY = (copyRegs[0] >> 11) & 0x7ff;
	u32 dstX = copyRegs[1] & 0x7ff;
	u32 dstY = (copyRegs[1] >> 11) & 0x7ff;
	for (u32 row = 0; row < h; row++)
		texRamWritten((((dstY + row) * 4096) & 0x7fffff), 4096);
	for (u32 row = 0; row < h; row++)
		for (u32 col = 0; col < w; col++)
		{
			u32 s = ((srcY + row) * 2048 + ((srcX + col) ^ 1)) & 0x3fffff;
			u32 d = ((dstY + row) * 2048 + ((dstX + col) ^ 1)) & 0x3fffff;
			texRam[d] = texRam[s];
		}
	regs1A[CP_STATUS] &= ~1;
}

static int cpDoneCallback(int tag, int cycles, int jitter, void *arg)
{
	regs15[CP_START] &= ~3;
	regs1A[CP_STATUS] &= ~1;
	regs1A[IRQ_LATCH0 + 2] |= 1;
	regs15[IRQ_STATUS] |= IRQ_CP_DONE;
	updateIrq();
	return 0;
}

static int lineCallback(int tag, int cycles, int jitter, void *arg)
{
	currentLine++;
	// bits 11-21: current line (Demul 2025)
	regs1A[DISPLAY_STATUS] = (regs1A[DISPLAY_STATUS] & 0xffc007ff) | ((currentLine - 1) << 11);
	if (currentLine == MID_FRAME_LINE)
	{
		regs15[IRQ_STATUS] |= IRQ_VBLANK;
		updateIrq();
	}
	else if (currentLine == LINES_PER_FRAME)
	{
		regs1A[IRQ_LATCH0 + 1] |= 1;
		regs15[IRQ_STATUS] |= IRQ_VBLANK;
		currentLine = 0;
		updateIrq();
		cp::vblank();
		emu.vblank();
		static u32 frames;
		{	// Debug aid: set HIKARU_SAVE_FRAME=n to save a state in slot 9 at frame n.
			// Later runs can resume from it with -config config:Dreamcast.AutoLoadState=yes
			// -config config:Dreamcast.SavestateSlot=9
			static int saveFrame = getenv("HIKARU_SAVE_FRAME") ? atoi(getenv("HIKARU_SAVE_FRAME")) : -1;
			if ((int)frames == saveFrame) {
				NOTICE_LOG(PVR, "hikaru: saving state at frame %d", frames);
				dc_savestate(getenv("HIKARU_SAVE_SLOT") ? atoi(getenv("HIKARU_SAVE_SLOT")) : 9);
			}
		}
		++frames;
#ifdef _WIN32
		{	// Debug aid: F9 saves a state in slot 7 (no menu needed)
			static bool f9Down;
			bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
			if (down && !f9Down) {
				NOTICE_LOG(PVR, "hikaru: F9, saving state in slot 7");
				dc_savestate(7);
			}
			f9Down = down;
		}
#endif
	}
	return LINE_CYCLES;
}

template<typename T>
T DYNACALL readReg(u32 addr)
{
	addr &= 0x1fffffff;
	u32 v;
	if (addr < 0x15000100 && addr >= 0x15000000)
		v = regs15[(addr & 0xff) >> 2];
	else if (addr >= 0x15002000 && addr < 0x1500284c)
		v = addr == 0x15002000 ? 1 : 0xffffffff;
	else if (addr == 0x18001000)
		v = 0x17c711db;	// PCI vendor/device id
	else if (addr >= 0x1a000000 && addr < 0x1a000140)
		v = regs1A[(addr & 0x1ff) >> 2];
	else if (addr >= 0x1a000180 && addr < 0x1a0001c0)
		v = regs1A180[(addr & 0x3f) >> 2];
	else if (addr >= 0x1a000200 && addr < 0x1a000240)
		v = regs1A200[(addr & 0x3f) >> 2];
	else if (addr >= 0x1a040000 && addr < 0x1a040010)
		v = copyRegs[(addr & 0xf) >> 2];
	else
	{
		INFO_LOG(PVR, "hikaru: GPU read%d %08x", (int)sizeof(T) * 8, addr);
		return 0;
	}
	if (sizeof(T) == 2)
		return (T)(v >> ((addr & 2) * 8));
	if (sizeof(T) == 1)
		return (T)(v >> ((addr & 3) * 8));
	return (T)v;
}

template<typename T>
void DYNACALL writeReg(u32 addr, T data)
{
	addr &= 0x1fffffff;
	if (sizeof(T) != 4)
		INFO_LOG(PVR, "hikaru: GPU write%d %08x = %x", (int)sizeof(T) * 8, addr, (u32)data);
	u32 v = data;
	if (addr >= 0x15000000 && addr < 0x15000100)
	{
		u32 idx = (addr & 0xff) >> 2;
		switch (idx)
		{
		case TEX_LIST_COUNT:
		case TEX_LIST_ENABLE:
			regs15[idx] = v;
			processTextureUploads();
			break;
		case CP_START:
			regs15[idx] = v;
			if (v == 3)
			{
				regs1A[CP_STATUS] = 1;
				cp::run(regs15[0x70 / 4], regs15[0x74 / 4]);
				sh4_sched_request(cpSchedId, 2 * LINE_CYCLES);
			}
			break;
		case IRQ_MASK:
			regs15[idx] = v;
			updateIrq();
			break;
		case IRQ_STATUS:
			regs15[idx] &= v;
			updateIrq();
			break;
		default:
			regs15[idx] = v;
			break;
		}
	}
	else if (addr >= 0x1a000000 && addr < 0x1a000140)
	{
		u32 idx = (addr & 0x1ff) >> 2;
		switch (idx)
		{
		case 0:
		case 1:
			// A write to 1A000000 flips 1A00001C bit 23, the 3D buffer shown; 1A000004 flips bit 24
			// (Demul 2025). The game picks its 0x781 buffers from them.
			regs1A[DISPLAY_STATUS] ^= 1u << (23 + idx);
			break;
		case IRQ_LATCH0:
		case IRQ_LATCH0 + 1:
		case IRQ_LATCH0 + 2:
		case IRQ_LATCH0 + 3:
			regs1A[idx] &= ~v;
			updateIrq();
			break;
		case CP_STATUS:
			regs1A[idx] = v * 2;
			break;
		default:
			regs1A[idx] = v;
			break;
		}
	}
	else if (addr >= 0x1a000180 && addr < 0x1a0001c0)
		regs1A180[(addr & 0x3f) >> 2] = v;
	else if (addr >= 0x1a000200 && addr < 0x1a000240)
		regs1A200[(addr & 0x3f) >> 2] = v;
	else if (addr >= 0x1a040000 && addr < 0x1a040010)
	{
		u32 idx = (addr & 0xf) >> 2;
		if (idx < 3)
			copyRegs[idx] = v;
		else if (v & 1)
			rectangleCopy();
	}
	else
		INFO_LOG(PVR, "hikaru: GPU write%d %08x = %x", (int)sizeof(T) * 8, addr, v);
}

template u8 DYNACALL readReg<u8>(u32 addr);
template u16 DYNACALL readReg<u16>(u32 addr);
template u32 DYNACALL readReg<u32>(u32 addr);
template void DYNACALL writeReg<u8>(u32 addr, u8 data);
template void DYNACALL writeReg<u16>(u32 addr, u16 data);
template void DYNACALL writeReg<u32>(u32 addr, u32 data);

void init()
{
	if (lineSchedId == -1)
		lineSchedId = sh4_sched_register(0, lineCallback);
	if (cpSchedId == -1)
		cpSchedId = sh4_sched_register(0, cpDoneCallback);
	cp::init();
}

void term()
{
	cp::term();
	if (lineSchedId != -1)
		sh4_sched_unregister(lineSchedId);
	if (cpSchedId != -1)
		sh4_sched_unregister(cpSchedId);
	lineSchedId = cpSchedId = -1;
}

void reset(bool hard)
{
	memset(regs15, 0, sizeof(regs15));
	memset(regs1A, 0, sizeof(regs1A));
	memset(regs1A180, 0, sizeof(regs1A180));
	memset(regs1A200, 0, sizeof(regs1A200));
	memset(copyRegs, 0, sizeof(copyRegs));
	currentLine = 0;
	sh4_sched_request(lineSchedId, LINE_CYCLES);
	sh4_sched_request(cpSchedId, -1);
	cp::reset();
	render::reset();
}

void serialize(Serializer& ser)
{
	cp::serialize(ser);
	ser << regs15;
	ser << regs1A;
	ser << regs1A180;
	ser << regs1A200;
	ser << copyRegs;
	ser << currentLine;
	sh4_sched_serialize(ser, lineSchedId);
	sh4_sched_serialize(ser, cpSchedId);
}

void deserialize(Deserializer& deser)
{
	cp::deserialize(deser);
	deser >> regs15;
	deser >> regs1A;
	deser >> regs1A180;
	deser >> regs1A200;
	deser >> copyRegs;
	deser >> currentLine;
	sh4_sched_deserialize(deser, lineSchedId);
	sh4_sched_deserialize(deser, cpSchedId);
	// The line timer always runs. A state saved from inside its callback has it unscheduled.
	if (!sh4_sched_is_scheduled(lineSchedId))
		sh4_sched_request(lineSchedId, LINE_CYCLES);
	updateIrq();
}

}
