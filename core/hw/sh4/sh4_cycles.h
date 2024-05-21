/*
	Copyright 2023 flyinghead

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
#include "sh4_opcode_list.h"
#include "sh4_if.h"
#include "sh4_sched.h"
#include "modules/mmu.h"

class Sh4Cycles
{
public:
	Sh4Cycles(int cpuRatio = 1) : cpuRatio(cpuRatio) {}

	void executeCycles(u16 op)
	{
		Sh4cntx.cycle_counter -= countCycles(op);
	}

	void addCycles(int cycles) const
	{
		Sh4cntx.cycle_counter -= cycles;
	}

	void addReadAccessCycles(u32 addr, u32 size) const
	{
		Sh4cntx.cycle_counter -= readAccessCycles(addr, size);
	}

	void addWriteAccessCycles(u32 addr, u32 size) const
	{
		Sh4cntx.cycle_counter -= writeAccessCycles(addr, size);
	}

	int countCycles(u16 op)
	{
		sh4_opcodelistentry *opcode = OpDesc[op];
		int cycles = 0;
#ifndef STRICT_MODE
		static const bool isMemOp[45] {
			false,
			false,
			true,	// all mem moves, ldtlb, sts.l FPUL/FPSCR, @-Rn, lds.l @Rn+,FPUL
			true,	// gbr-based load/store
			false,
			true,	// tst.b #<imm8>, @(R0,GBR)
			true,	// and/or/xor.b #<imm8>, @(R0,GBR)
			true,	// tas.b @Rn
			false,
			false,
			false,
			false,
			true,	// movca.l R0, @Rn
			false,
			false,
			false,
			false,
			true,	// ldc.l @Rn+, VBR/SPC/SSR/Rn_Bank/DBR
			true,	// ldc.l @Rn+, GBR/SGR
			true,	// ldc.l @Rn+, SR
			false,
			false,
			true,	// stc.l DBR/SR/GBR/VBR/SSR/SPC/Rn_Bank, @-Rn
			true,	// stc.l SGR, @-Rn
			false,
			true,	// lds.l @Rn+, PR
			false,
			true,	// sts.l PR, @-Rn
			false,
			true,	// lds.l @Rn+, MACH/MACL
			false,
			true,	// sts.l MACH/MACL, @-Rn
			false,
			true,	// lds.l @Rn+,FPSCR
			false,
			true,	// mac.wl @Rm+,@Rn+
		};
		if (isMemOp[opcode->ex_type])
		{
			if (++memOps < 4)
				cycles = mmu_enabled() ? 5 : 2;
		}
		// TODO only for mem read?
#endif

		if (lastUnit == CO
				|| opcode->unit == CO
				|| (lastUnit == opcode->unit && lastUnit != MT))
		{
			// cannot run in parallel
			lastUnit = opcode->unit;
			cycles += opcode->IssueCycles;
		}
		else
		{
			// can run in parallel
			lastUnit = CO;
		}
		return cycles * cpuRatio;
	}

	void reset()
	{
		lastUnit = CO;
		memOps = 0;
	}

	static u64 now() {
		return sh4_sched_now64() + SH4_TIMESLICE - Sh4cntx.cycle_counter;
	}

	int readAccessCycles(u32 addr, u32 size) const {
		return readExternalAccessCycles(addr, size) * 2 * cpuRatio;
	}

	int writeAccessCycles(u32 addr, u32 size) const {
		return writeExternalAccessCycles(addr, size) * 2 * cpuRatio;
	}

private:
	// Returns the number of external cycles (100 MHz) needed for a sized read at the given address
	static int readExternalAccessCycles(u32 addr, u32 size);
	// Returns the number of external cycles (100 MHz) needed for a sized write at the given address
	static int writeExternalAccessCycles(u32 addr, u32 size);

	sh4_eu lastUnit = CO;
	const int cpuRatio;
	int memOps = 0;
};

extern Sh4Cycles sh4cycles;
