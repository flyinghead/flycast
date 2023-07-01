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
#include "oslib/unwind_info.h"
#include <aarch64/macro-assembler-aarch64.h>
using namespace vixl::aarch64;

class Arm64UnwindInfo : public UnwindInfo
{
public:
	void saveReg(u32 offset, const Register& reg, int stackOffset) {
		UnwindInfo::saveReg(offset, dwarfRegisterId(reg), stackOffset);
	}
	void saveReg(u32 offset, const VRegister& reg, int stackOffset) {
		UnwindInfo::saveExtReg(offset, dwarfRegisterId(reg), stackOffset);
	}

private:
	int dwarfRegisterId(const CPURegister& reg)
	{
		verify(reg.Is64Bits());
		if (reg.IsFPRegister())
			return reg.GetCode() + 64;
		else
			return reg.GetCode();
	}
};
