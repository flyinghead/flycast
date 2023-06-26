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
		if (opcode->ex_type == 2 || opcode->ex_type == 3
				|| opcode->ex_type == 5 || opcode->ex_type == 6 || opcode->ex_type == 7
				// cache mgmt || opcode->ex_type == 10 || opcode->ex_type == 11
				|| opcode->ex_type == 12
				|| opcode->ex_type == 17 || opcode->ex_type == 18 || opcode->ex_type == 19
				|| opcode->ex_type == 22 || opcode->ex_type == 23 || opcode->ex_type == 25
				|| opcode->ex_type == 27 || opcode->ex_type == 29 || opcode->ex_type == 31
				|| opcode->ex_type == 33 || opcode->ex_type == 35)
		{
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
};

extern Sh4Cycles sh4cycles;
