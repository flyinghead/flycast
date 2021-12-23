/*
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
#pragma once
#include "types.h"
#include "emulator.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_interpreter.h"
#include "cfg/option.h"
#include <array>
#include <signal.h>
#include <map>

#ifndef SIGTRAP
#define SIGTRAP 5
#endif
#ifndef SIGBUS
#define SIGBUS 7
#endif

const std::array<Sh4RegType, 59> Sh4RegList {
		reg_r0,
		reg_r1,
		reg_r2,
		reg_r3,
		reg_r4,
		reg_r5,
		reg_r6,
		reg_r7,
		reg_r8,
		reg_r9,
		reg_r10,
		reg_r11,
		reg_r12,
		reg_r13,
		reg_r14,
		reg_r15,
		reg_nextpc,
		reg_pr,
		reg_gbr,
		reg_vbr,
		reg_mach,
		reg_macl,
		reg_sr_status,

		reg_fpul,
		reg_fpscr,
		reg_fr_0, reg_fr_1, reg_fr_2, reg_fr_3, reg_fr_4, reg_fr_5, reg_fr_6, reg_fr_7,
		reg_fr_8, reg_fr_9, reg_fr_10, reg_fr_11, reg_fr_12, reg_fr_13, reg_fr_14, reg_fr_15,
		reg_ssr,
		reg_spc,
		// correct?
		reg_r0, reg_r1, reg_r2, reg_r3, reg_r4, reg_r5, reg_r6, reg_r7,
		reg_r0_Bank, reg_r1_Bank, reg_r2_Bank, reg_r3_Bank, reg_r4_Bank, reg_r5_Bank, reg_r6_Bank, reg_r7_Bank,
};

class DebugAgent
{
public:
	void doContinue(u32 pc = 1)
	{
		if (pc != 1)
			Sh4cntx.pc = pc;
		emu.start();
	}

	void step()
	{
		bool restoreBreakpoint = removeMatchpoint(0, Sh4cntx.pc, 2);
		u32 savedPc = Sh4cntx.pc;
		emu.step();
		if (restoreBreakpoint)
			insertMatchpoint(0, savedPc, 2);
	}

	int readAllRegs(u32 **regs)
	{
		static std::array<u32, Sh4RegList.size()> allregs;
		for (u32 i = 0; i < Sh4RegList.size(); i++)
		{
			if (Sh4RegList[i] == reg_sr_status)
				allregs[i] = sh4_sr_GetFull();
			else if (Sh4RegList[i] != NoReg)
				allregs[i] = *GetRegPtr(Sh4RegList[i]);
		}
		*regs = &allregs[0];
		return allregs.size();
	}

	void writeAllRegs(const std::vector<u32>& regs)
	{
		for (u32 i = 0; i < Sh4RegList.size(); i++)
			if (Sh4RegList[i] != NoReg)
				*GetRegPtr(Sh4RegList[i]) = regs[i];
	}

	u32 readReg(u32 regNum)
	{
		if (regNum >= Sh4RegList.size())
			return 0;
		Sh4RegType reg = Sh4RegList[regNum];
		if (reg == reg_sr_status)
			return sh4_sr_GetFull();
		if (reg != NoReg)
			return *GetRegPtr(reg);
		return 0;
	}
	void writeReg(u32 regNum, u32 value)
	{
		if (regNum >= Sh4RegList.size())
			return;
		Sh4RegType reg = Sh4RegList[regNum];
		if (reg == reg_sr_status)
			sh4_sr_SetFull(value);
		else if (reg != NoReg)
			*GetRegPtr(reg) = value;
	}

	const u8 *readMem(u32 addr, u32 len)
	{
		static std::vector<u8> data;
		data.resize(len);
		u8 *p = &data[0];
		while (len > 0)
		{
			if (len >= 4 && (addr & 3) == 0)
			{
				*(u32 *)p = ReadMem32_nommu(addr);
				addr += 4;
				len -= 4;
				p += 4;
			}
			else if (len >= 2 && (addr & 1) == 0)
			{
				*(u16 *)p = ReadMem16_nommu(addr);
				addr += 2;
				len -= 2;
				p += 2;
			}
			else
			{
				*p++ = ReadMem8_nommu(addr);
				addr++;
				len--;
			}
		}
		return &data[0];
	}
	void writeMem(u32 addr, const std::vector<u8>& data)
	{
		const u8 *p = &data[0];
		u32 len = data.size();
		while (len > 0)
		{
			if (len >= 4 && (addr & 3) == 0)
			{
				WriteMem32_nommu(addr, *(u32 *)p);
				addr += 4;
				len -= 4;
				p += 4;
			}
			else if (len >= 2 && (addr & 3) == 0)
			{
				WriteMem16_nommu(addr, *(u16 *)p);
				addr += 2;
				len -= 2;
				p += 2;
			}
			else
			{
				WriteMem8_nommu(addr, *p++);
				addr++;
				len--;
			}
		}
	}
	bool insertMatchpoint(char type, u32 addr, u32 len)
	{
		if (type == 0 && len != 2) {
			WARN_LOG(COMMON, "insertMatchpoint: length != 2: %d", len);
			return false;
		}
		// TODO other matchpoint types
		if (breakpoints.find(addr) != breakpoints.end())
			return true;
		breakpoints[addr] = Breakpoint(type, addr);
		breakpoints[addr].savedOp = ReadMem16_nommu(addr);
		WriteMem16_nommu(addr, 0xC308);	// trapa #0x20
		return true;
	}
	bool removeMatchpoint(char type, u32 addr, u32 len)
	{
		if (type == 0 && len != 2) {
			WARN_LOG(COMMON, "removeMatchpoint: length != 2: %d", len);
			return false;
		}
		auto it = breakpoints.find(addr);
		if (it == breakpoints.end())
			return false;
		WriteMem16_nommu(addr, it->second.savedOp);
		breakpoints.erase(it);
		return true;
	}

	u32 interrupt()
	{
		config::DynarecEnabled = false;
		exception = SIGINT;
		emu.stop();
		return exception;
	}

	// called on the emu thread
	void debugTrap(u32 event)
	{
		exception = findException(event);
		Sh4cntx.pc -= 2;	// FIXME delay slot
	}

	void postDebugTrap()
	{
		// needed to join the emu thread since debugTrap() is called by it
		emu.stop();
	}

	u32 currentException()
	{
		return exception;
	}

	void restart()
	{
		emu.unloadGame();
		emu.loadGame(settings.content.path);
		emu.start();
	}

	void detach()
	{
		emu.start();
	}

	void kill()
	{
		dc_exit();
	}

	void resetAgent()
	{
		stack.clear();
	}

	int findException(u32 event)
	{
		switch (event)
		{
		case 0x0A0:		// Instruction/data TLB protection violation exception (read)
		case 0x0C0:		// Data TLB protection violation exception (write)
			return SIGSEGV;
		case 0x0E0:		// Instruction/data address error (read)
		case 0x100:		// data address error (write)
			return SIGBUS;
		case 0x160:		// trapa
		case 0x1E0:		// User break before/after instruction execution
			return SIGTRAP;
		case 0x180:		// General illegal instruction exception
		case 0x1A0:		// Slot illegal instruction exception
			return SIGILL;
		case 0x120:		// FPU exception
		case 0x800:		// General FPU disable exception
		case 0x820:		// Slot FPU disable exception
			return SIGFPE;
		default:
			return SIGABRT;
		}
	}

	const u32 *getStack(u32& size)
	{
		verify(!Sh4cntx.CpuRunning);
		size = stack.size() * 8;
		return (const u32 *)&stack[0];
	}

	void subroutineCall()
	{
		subroutineReturn();
		stack.push_back(std::make_pair(Sh4cntx.pc, Sh4cntx.r[15]));
	}

	void subroutineReturn()
	{
		while (!stack.empty() && Sh4cntx.r[15] >= stack.back().second)
			stack.pop_back();
	}

	u32 exception = 0;

	struct Breakpoint {
		Breakpoint() = default;
		Breakpoint(u16 type, u32 addr) : addr(addr), type(type) { }
		u32 addr = 0;
		u16 type = 0;
		u16 savedOp = 0;
	};
	std::map<u32, Breakpoint> breakpoints;
	std::vector<std::pair<u32, u32>> stack;
};
