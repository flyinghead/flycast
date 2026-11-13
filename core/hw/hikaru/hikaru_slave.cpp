/*
	Sega Hikaru slave SH-4.

	Slave memory map (from the BIOS and Demul's slave address space):
		00000000-001fffff	BIOS (same image as the master)
		04000000-0400003f	slave PCI bridge (bridge register 0 reads all ones: CPU id)
		0c000000-0dffffff	slave work RAM (32 MB), mirrored at 0e000000
		10000000-103fffff	GPU command RAM (shared with the master's 14000000)
		1a800008, 1b000100	registers shared with the master
		e0000000-ffffffff	on-chip peripherals (only stored, not emulated yet)

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
#include "hikaru_slave.h"
#include "hikaru.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/flashrom/nvmem.h"
#include "serialize.h"
#include "stdclass.h"
#include "cfg/option.h"
#if FEAT_SHREC != DYNAREC_NONE
#include "hw/sh4/dyna/ngen.h"
#endif

#include <cstring>
#include <memory>
#include <unordered_map>

namespace hikaru::slave
{

// Run the slave in slices of this many cycles, interleaved with the master
constexpr int SLICE_CYCLES = 448;

static Sh4Context context;
static std::unique_ptr<Sh4Interpreter> interpreter;
// Run on the recompiler (as CPU 1) rather than the interpreter
static bool recompiled;
static int schedId = -1;
static bool bridgeIrq;
static bool nmiPending;
static std::unordered_map<u32, u32> onChipRegs;

//
// Slave memory map
//
// Wait loops: a slice that writes nothing, only reads a few words of RAM, ends near where it
// started and leaves the FPU registers alone is waiting for one of them to change. The following slices are skipped until one does (or an NMI comes),
// which has the same outcome as running the loop.
struct WaitLoop
{
	static constexpr int MAX_WORDS = 4;
	u32 addr[MAX_WORDS];
	u32 value[MAX_WORDS];
	int count = 0;
	bool polling = false;	// the running slice only polled RAM so far
	bool waiting = false;	// slices are skipped
};
static WaitLoop waitLoop;

static u8 *ramPtr(u32 paddr)
{
	if (paddr >= 0x0c000000 && paddr < 0x10000000)
		return getSlaveRam() + (paddr & (SLAVE_RAM_SIZE - 1));
	if (paddr >= 0x10000000 && paddr < 0x10000000 + CMDRAM_SIZE)
		return getCmdRam() + (paddr & (CMDRAM_SIZE - 1));
	return nullptr;
}

static void pollRam(u32 paddr)
{
	paddr &= ~3;
	for (int i = 0; i < waitLoop.count; i++)
		if (waitLoop.addr[i] == paddr)
			return;
	if (waitLoop.count == WaitLoop::MAX_WORDS)
		waitLoop.polling = false;
	else
		waitLoop.addr[waitLoop.count++] = paddr;
}

static u32 peekRam(u32 paddr)
{
	u32 v;
	memcpy(&v, ramPtr(paddr), sizeof(v));
	return v;
}

template<typename T>
static T read(u32 addr)
{
	if (addr >= 0xe0000000)
	{
		waitLoop.polling = false;
		auto it = onChipRegs.find(addr);
		T v = it == onChipRegs.end() ? 0 : (T)it->second;
		DEBUG_LOG(SH4, "hikaru slave: on-chip read%d %08x -> %x", (int)sizeof(T) * 8, addr, (u32)v);
		return v;
	}
	u32 paddr = addr & 0x1fffffff;
	T v;
	if (paddr < 0x200000)
		memcpy(&v, nvmem::getBiosData() + paddr, sizeof(T));
	else if (u8 *p = ramPtr(paddr))
	{
		if (waitLoop.polling)
			pollRam(paddr);
		memcpy(&v, p, sizeof(T));
		return v;
	}
	else if (paddr >= 0x04000000 && paddr < 0x04000040)
		v = slaveBridgeRead<T>(paddr);
	else if (paddr == 0x14000800)
		v = (T)0x17c311db;	// PCI vendor/device id
	else
	{
		u16 reg;
		if (sharedRegRead(paddr, reg))
		{
			waitLoop.polling = false;
			return (T)reg;
		}
		INFO_LOG(SH4, "hikaru slave: read%d from %08x (pc %08x)", (int)sizeof(T) * 8, addr, Sh4cntx.pc);
		v = 0;
	}
	if (paddr >= 0x200000)
		// Registers: not a wait on RAM
		waitLoop.polling = false;
	return v;
}

template<typename T>
static void write(u32 addr, T data)
{
	waitLoop.polling = false;
	if (addr >= 0xe0000000)
	{
		DEBUG_LOG(SH4, "hikaru slave: on-chip write%d %08x = %x", (int)sizeof(T) * 8, addr, (u32)data);
		onChipRegs[addr] = data;
		return;
	}
	u32 paddr = addr & 0x1fffffff;
	if (paddr >= 0x0c000000 && paddr < 0x10000000)
		memcpy(getSlaveRam() + (paddr & (SLAVE_RAM_SIZE - 1)), &data, sizeof(T));
	else if (paddr >= 0x10000000 && paddr < 0x10000000 + CMDRAM_SIZE)
		memcpy(getCmdRam() + (paddr & (CMDRAM_SIZE - 1)), &data, sizeof(T));
	else if (paddr >= 0x04000000 && paddr < 0x04000040)
		slaveBridgeWrite<T>(paddr, data);
	else if (!sharedRegWrite(paddr, (u16)data))
		INFO_LOG(SH4, "hikaru slave: write%d to %08x = %x (pc %08x)", (int)sizeof(T) * 8, addr, (u32)data, Sh4cntx.pc);
}

static u8 DYNACALL read8(u32 addr) { return read<u8>(addr); }
static u16 DYNACALL read16(u32 addr) { return read<u16>(addr); }
// Instruction fetch: not a data read of a wait loop
static u16 DYNACALL fetch16(u32 addr)
{
	if (addr < 0xe0000000)
	{
		u32 paddr = addr & 0x1fffffff;
		u16 v;
		if (paddr < 0x200000)
		{
			memcpy(&v, nvmem::getBiosData() + paddr, sizeof(v));
			return v;
		}
		if (u8 *p = ramPtr(paddr))
		{
			memcpy(&v, p, sizeof(v));
			return v;
		}
	}
	return read<u16>(addr);
}
static u32 DYNACALL read32(u32 addr) { return read<u32>(addr); }
static u64 DYNACALL read64(u32 addr) {
	return read<u32>(addr) | ((u64)read<u32>(addr + 4) << 32);
}
static void DYNACALL write8(u32 addr, u8 data) { write<u8>(addr, data); }
static void DYNACALL write16(u32 addr, u16 data) { write<u16>(addr, data); }
static void DYNACALL write32(u32 addr, u32 data) { write<u32>(addr, data); }
static void DYNACALL write64(u32 addr, u64 data) {
	write<u32>(addr, (u32)data);
	write<u32>(addr + 4, (u32)(data >> 32));
}

// Host pointer to slave memory, for the recompiler to check the code of its blocks.
// The BIOS is read-only so its code needs no check.
static u8 *getMemPtr(u32 addr, u32 size)
{
	if (addr >= 0xe0000000)
		return nullptr;
	u32 paddr = addr & 0x1fffffff;
	if (paddr >= 0x0c000000 && paddr < 0x10000000)
	{
		u32 offset = paddr & (SLAVE_RAM_SIZE - 1);
		return offset + size <= SLAVE_RAM_SIZE ? getSlaveRam() + offset : nullptr;
	}
	if (paddr >= 0x10000000 && paddr < 0x10000000 + CMDRAM_SIZE)
	{
		u32 offset = paddr & (CMDRAM_SIZE - 1);
		return offset + size <= CMDRAM_SIZE ? getCmdRam() + offset : nullptr;
	}
	return nullptr;
}

// Store queue flush (pref to e0000000-e3ffffff). Address bit 5 selects SQ0/SQ1 and the
// matching QACR0/QACR1 register gives address bits 28-26 of the destination.
static void DYNACALL sqWrite(u32 dest, Sh4Context *ctx)
{
	const int sq = (dest >> 5) & 1;
	auto it = onChipRegs.find(sq == 0 ? 0xff000038 : 0xff00003c);
	const u32 qacr = it == onChipRegs.end() ? 0 : it->second;
	const u32 address = ((qacr & 0x1c) << 24) | (dest & 0x03ffffe0);
	for (int i = 0; i < 8; i++)
	{
		u32 v;
		memcpy(&v, &ctx->sq_buffer[sq].data[i * 4], sizeof(v));
		write<u32>(address + i * 4, v);
	}
}

#if FEAT_SHREC != DYNAREC_NONE
static const Sh4CpuMemory slaveMemory {
	read8, read16, read32, read64,
	write8, write16, write32, write64,
	getMemPtr
};
#endif

//
// Context switching
//
struct MemAccessors
{
	ReadMem8Func read8;
	ReadMem16Func read16;
	ReadMem16Func iread16;
	ReadMem32Func read32;
	ReadMem64Func read64;
	WriteMem8Func write8;
	WriteMem16Func write16;
	WriteMem32Func write32;
	WriteMem64Func write64;

	void save() {
		read8 = ReadMem8; read16 = ReadMem16; iread16 = IReadMem16; read32 = ReadMem32; read64 = ReadMem64;
		write8 = WriteMem8; write16 = WriteMem16; write32 = WriteMem32; write64 = WriteMem64;
	}
	void restore() const {
		ReadMem8 = read8; ReadMem16 = read16; IReadMem16 = iread16; ReadMem32 = read32; ReadMem64 = read64;
		WriteMem8 = write8; WriteMem16 = write16; WriteMem32 = write32; WriteMem64 = write64;
	}
};

static void swapContext()
{
	Sh4Context tmp;
	memcpy(&tmp, &Sh4cntx, sizeof(tmp));
	memcpy(&Sh4cntx, &context, sizeof(tmp));
	memcpy(&context, &tmp, sizeof(tmp));
}

// Runs fn with the slave context and memory map active
template<typename F>
static void withSlave(F fn)
{
	MemAccessors master;
	master.save();
	Sh4Interpreter *masterInstance = Sh4Interpreter::Instance;
	interrupts_suspend();
	swapContext();
	// The scheduler time is kept in the context: carry it to the slave and back
	Sh4cntx.sh4_sched_next = context.sh4_sched_next;
	ReadMem8 = read8; ReadMem16 = read16; IReadMem16 = fetch16; ReadMem32 = read32; ReadMem64 = read64;
	WriteMem8 = write8; WriteMem16 = write16; WriteMem32 = write32; WriteMem64 = write64;

	fn();

	context.sh4_sched_next = Sh4cntx.sh4_sched_next;
	swapContext();
	interrupts_resume();
	master.restore();
	Sh4Interpreter::Instance = masterInstance;
}

// Interrupt entry on the current (slave) context
static void takeInterrupt(u32 intevt)
{
	Sh4cntx.spc = Sh4cntx.pc;
	Sh4cntx.ssr = Sh4cntx.sr.getFull();
	Sh4cntx.sgr = Sh4cntx.r[15];
	Sh4cntx.sr.MD = 1;
	Sh4cntx.sr.RB = 1;
	Sh4cntx.sr.BL = 1;
	UpdateSR();
	onChipRegs[0xff000028] = intevt;	// INTEVT
	Sh4cntx.pc = Sh4cntx.vbr + 0x600;
}

void nmi()
{
	DEBUG_LOG(SH4, "hikaru slave: NMI");
	nmiPending = true;
}

// A polling loop stays within a few instructions and leaves the FPU registers alone
struct CpuState
{
	u32 pc;
	float fr[16];
	float xf[16];

	bool sameLoop(const CpuState& o) const {
		return pc - o.pc + 0x40 < 0x80 && memcmp(fr, o.fr, sizeof(fr)) == 0 && memcmp(xf, o.xf, sizeof(xf)) == 0;
	}
};

static CpuState cpuState()
{
	CpuState st;
	st.pc = context.pc;
	memcpy(st.fr, context.fr, sizeof(st.fr));
	memcpy(st.xf, context.xf, sizeof(st.xf));
	return st;
}

static int schedCallback(int tag, int cycles, int jitter, void *arg)
{
	if (waitLoop.waiting && !nmiPending)
	{
		bool changed = false;
		for (int i = 0; i < waitLoop.count && !changed; i++)
			changed = peekRam(waitLoop.addr[i]) != waitLoop.value[i];
		if (!changed)
			return SLICE_CYCLES;
	}
	waitLoop.waiting = false;
	waitLoop.polling = true;
	waitLoop.count = 0;
	// A routine computing in FPU registers from constants in RAM also only reads a few words:
	// it isn't waiting
	const CpuState start = cpuState();
	withSlave([]() {
		if (nmiPending)
		{
			nmiPending = false;
			onChipRegs[0xffa00040] |= 2;	// DMAOR.NMIF
			takeInterrupt(0x1c0);
		}
		Sh4cntx.cycle_counter = SLICE_CYCLES;
#if FEAT_SHREC != DYNAREC_NONE
		if (recompiled)
		{
			rdv_SelectCpu(1);
			Sh4cntx.CpuRunning = 1;
			rdv_RunCpu();
			rdv_SelectCpu(0);
			return;
		}
#endif
		while (Sh4cntx.cycle_counter > 0)
			interpreter->Step();
	});
	if (waitLoop.polling && waitLoop.count > 0 && cpuState().sameLoop(start))
	{
		waitLoop.waiting = true;
		for (int i = 0; i < waitLoop.count; i++)
			waitLoop.value[i] = peekRam(waitLoop.addr[i]);
	}
	waitLoop.polling = false;
	return SLICE_CYCLES;
}

void setBridgeIrq(bool pending)
{
	if (pending != bridgeIrq)
		DEBUG_LOG(SH4, "hikaru slave: bridge IRQ %d", pending);
	// TODO deliver interrupts to the slave
	bridgeIrq = pending;
}

std::string debugState()
{
	std::string s = strprintf("slave PC %08x PR %08x SR %08x | ", context.pc, context.pr, context.sr.getFull());
	for (int i = 0; i < 16; i++)
		s += strprintf("r%d=%08x ", i, context.r[i]);
	s += strprintf("| slave code @%08x: ", (context.pc & ~0xf) - 0x80);
	for (u32 a = (context.pc & ~0xf) - 0x80; a < (context.pc & ~0xf) + 0x40; a += 2)
		s += strprintf("%04x ", read<u16>(a));
	s += strprintf("| VBR %08x GBR %08x | slave code @%08x: ", context.vbr, context.gbr, context.vbr + 0x600);
	for (u32 a = context.vbr + 0x600; a < context.vbr + 0x680; a += 2)
		s += strprintf("%04x ", read<u16>(a));
	return s;
}

void init()
{
	if (interpreter == nullptr)
		interpreter = std::make_unique<Sh4Interpreter>();
	if (schedId == -1)
		schedId = sh4_sched_register(0, schedCallback);
}

void term()
{
	if (schedId != -1)
		sh4_sched_unregister(schedId);
	schedId = -1;
	interpreter.reset();
#if FEAT_SHREC != DYNAREC_NONE
	if (recompiled)
		rdv_TermCpu(1);
#endif
	recompiled = false;
}

void reset()
{
	memcpy(&context, &Sh4cntx, sizeof(context));
	context.CpuRunning = 0;
	withSlave([]() {
		interpreter->Reset(true);
	});
	context.doSqWrite = sqWrite;
	onChipRegs.clear();
	bridgeIrq = false;
	nmiPending = false;
	waitLoop = {};
#if FEAT_SHREC != DYNAREC_NONE
	// The recompiler for the slave too if the master uses it. This also clears its code cache.
	if (config::DynarecEnabled)
		recompiled = rdv_InitCpu(1, slaveMemory);
	else if (recompiled)
	{
		rdv_TermCpu(1);
		recompiled = false;
	}
	INFO_LOG(SH4, "hikaru slave: %s", recompiled ? "recompiler" : "interpreter");
#endif
	sh4_sched_request(schedId, SLICE_CYCLES);
}

void serialize(Serializer& ser)
{
	ser << context;
	ser << bridgeIrq;
	ser << nmiPending;
	ser << (u32)onChipRegs.size();
	for (const auto& [addr, value] : onChipRegs)
	{
		ser << addr;
		ser << value;
	}
	sh4_sched_serialize(ser, schedId);
}

void deserialize(Deserializer& deser)
{
	deser >> context;
	context.doSqWrite = sqWrite;
	deser >> bridgeIrq;
	deser >> nmiPending;
	u32 count;
	deser >> count;
	onChipRegs.clear();
	for (u32 i = 0; i < count; i++)
	{
		u32 addr, value;
		deser >> addr;
		deser >> value;
		onChipRegs[addr] = value;
	}
	sh4_sched_deserialize(deser, schedId);
	waitLoop = {};
	// The slave always runs. A state saved while its slice was already due holds a time in
	// the past, which the scheduler would never reach, so schedule the next slice again.
	sh4_sched_request(schedId, SLICE_CYCLES);
}

}
