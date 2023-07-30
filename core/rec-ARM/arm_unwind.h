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
#include "oslib/unwind_info.h"

#ifdef _M_ARM
#pragma push_macro("MemoryBarrier")
#pragma push_macro("Yield")
#undef MemoryBarrier
#undef Yield
#endif

#include <aarch32/macro-assembler-aarch32.h>
using namespace vixl::aarch32;

#ifdef _M_ARM
#pragma pop_macro("MemoryBarrier")
#pragma pop_macro("Yield")
#endif

#include <map>

class ArmUnwindInfo : public UnwindInfo
{
public:
	void saveReg(u32 offset, const Register& reg, int stackOffset) {
		UnwindInfo::saveReg(offset, dwarfRegisterId(reg), stackOffset);
	}
	void saveReg(u32 offset, const SRegister& reg, int stackOffset) {
		UnwindInfo::saveExtReg(offset, dwarfRegisterId(reg), stackOffset);
	}

	static bool findFDE(uintptr_t targetAddr, uintptr_t &fde)
	{
		DEBUG_LOG(DYNAREC, "findFDE: addr %x entries %d", targetAddr, (int)fdes.size());
		if (fdes.empty())
			return false;
		auto it = fdes.upper_bound(targetAddr);
		if (it == fdes.begin())
			return false;
		--it;
		u32 start = it->second[2];
		u32 size = it->second[3];
		if (targetAddr >= start && targetAddr < start + size) {
			fde = (uintptr_t)it->second;
			return true;
		}
		return false;
	}

protected:
	void registerFrame(void *frame) override
	{
		u32 *fde = (u32 *)frame;
		u32 start = fde[2];
		fdes[start] = fde;
	}

	void deregisterFrame(void *frame) override
	{
		u32 *fde = (u32 *)frame;
		u32 start = fde[2];
		fdes.erase(start);
	}

private:
	int dwarfRegisterId(const CPURegister& reg)
	{
		if (reg.IsFPRegister())
			return reg.GetCode() + 64;
		else
			return reg.GetCode();
	}

	static std::map<u32, u32 *> fdes;
};
