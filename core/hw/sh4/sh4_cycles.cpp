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
#include "sh4_cycles.h"
#include "modules/mmu.h"

int Sh4Cycles::countCycles(u16 op)
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

// TODO additional wait cycles depending on area?:
// Area       Wait cycles (not including external wait)
// 0          3
// 1 VRAM     3
// 2 reserved 3
// 3 SDRAM    0         CAS latency 3
// 4 TA,YUV   1
// 5 G2 ext   3
// 6 reserved 3

int Sh4Cycles::readExternalAccessCycles(u32 addr, u32 size)
{
	if ((addr & 0xfc000000) == 0xe0000000)
		// store queues
		return 0;

	addr &= 0x1fffffff;
	switch (addr >> 26)
	{
	case 0:
		if (!settings.platform.isAtomiswave())
		{
			// Dreamcast, Naomi
			if (addr < 0x00200000)
			{
				// system rom
				switch (size)
				{
				case 1:
					return 44;
				case 2:
					return 63;
				case 4:
					return 99;
				case 32:
				default:
					return 618;
				}
			}
			if (addr < 0x00200000 + settings.platform.flash_size)
			{
				// flash
				switch (size)
				{
				case 1:
					return 41;
				case 2:
					return 55;
				case 4:
					return 83;
				case 32:
				default:
					return 489;
				}
			}
		}
		else
		{
			// Atomiswave
			if (addr < 0x00020000 || (addr >= 0x00200000 && addr < 0x00200000 + settings.platform.flash_size))
			{
				// flash
				switch (size)
				{
				case 1:
					return 41;
				case 2:
					return 55;
				case 4:
					return 83;
				case 32:
				default:
					return 489;
				}
			}
		}
		addr &= 0x01ffffff;
		if (addr >= 0x005f6800 && addr <= 0x005f69ff)
		{
			// holly system control regs
			if (size != 4)
				INFO_LOG(SH4, "holly system reg: Invalid read size %d @ %07x", size, addr);
			return 5;
		}
		if (addr >= 0x005f6c00 && addr <= 0x005f6cff)
		{
			// maple regs
			if (size != 4)
				INFO_LOG(SH4, "maple reg: Invalid read size %d @ %07x", size, addr);
			return 22;
		}
		if (addr >= 0x005f7000 && addr <= 0x005f70ff)
		{
			if (settings.platform.isArcade())
				// naomi/aw cart
				return 20; // ???
			else
			{
				// gd-rom
				if (size > 2)
					INFO_LOG(SH4, "gd-rom: Invalid read size %d @ %07x", size, addr);
				return 39;
			}
		}
		if (addr >= 0x005f7400 && addr <= 0x005f74ff)
		{
			// G1 I/F control regs
			if (settings.platform.isConsole())
			{
				if (size != 4) // unknown for aw/naomi
					INFO_LOG(SH4, "G1 I/F: Invalid read size %d @ %07x", size, addr);
			}
			else
			{
				// unknown for aw/naomi. seeing size 1 and 4 at least
			}
			return 24;
		}
		if (addr >= 0x005f7800 && addr <= 0x005f78ff)
		{
			// G2 I/F control regs
			if (size != 4)
				INFO_LOG(SH4, "G2 I/F: Invalid read size %d @ %07x", size, addr);
			return 38;
		}
		if (addr >= 0x005f7c00 && addr <= 0x005f7cff)
		{
			// PVR I/F control regs
			if (size != 4)
				INFO_LOG(SH4, "PVR I/F: Invalid read size %d @ %07x", size, addr);
			return 24;
		}
		if (addr >= 0x005f8000 && addr <= 0x005f9fff)
		{
			// TA/PVR core control regs, Palette RAM, fog table
			if (size != 4)
				// TODO 32-byte access allowed for palette and fog tables?
				INFO_LOG(SH4, "PVR/TA core: Invalid read size %d @ %07x", size, addr);
			return 34;
		}
		if (addr >= 0x00600000 && addr <= 0x006007ff)
		{
			if (settings.platform.isConsole())
				// AW registers
				return 20; // ???
			else
			{
				// modem
				if (size != 1)
					INFO_LOG(SH4, "modem: Invalid read size %d @ %07x", size, addr);
				return 67;
			}
		}
		if (addr >= 0x00700000 && addr <= 0x00ffffff)
		{
			// aica regs and ram
			if (size < 4)
				INFO_LOG(SH4, "aica: Invalid read size %d @ %07x", size, addr);
			return 40 * size / 4;	// undocumented
		}
		if (addr >= 0x01000000 && addr <= 0x01ffffff)
		{
			// G2 external area
			switch (size)
			{
			case 1:
			case 2:
				return 56;
			case 4:
				return 60;
			case 32:
			default:
				return 84;
			}
		}
		break;

	case 1:
		// VRAM
		switch (size)
		{
		case 1:
			INFO_LOG(SH4, "vram: Invalid read size 1 @ %07x", addr);
			return 41;
		case 2:
		case 4:
			return 41;
		case 32:
		default:
			return 61;
		}

	case 2:
		// Area 2
		INFO_LOG(SH4, "Invalid read from area 2 @ %07x", addr);
		return 60;

	case 3:
		// System RAM
		return 7;	// or 12 if row miss TODO average?

	case 4:
		// TA FIFO
		if (size != 32)
			INFO_LOG(SH4, "Invalid read size %d from area 4 (TA FIFO) @ %07x", size, addr);
		if ((addr >= 0x11000000 && addr <= 0x11ffffff) || (addr >= 0x13000000 && addr <= 0x13ffffff))
			// VRAM (64 bits)
			return 61;	// undocumented
		break;

	case 5:
		// Ext device
		switch (size)
		{
		case 1:
		case 2:
			return 56;
		case 4:
			return 60;
		case 32:
		default:
			return 84;
		}

	case 6:
		// Area 6
		INFO_LOG(SH4, "Invalid read from area 6 @ %07x", addr);
		return 60;

	case 7:
		// SH4 registers
		return 0;
	}

	INFO_LOG(SH4, "Unmapped read @ %08x", addr);
	return 60;
}


int Sh4Cycles::writeExternalAccessCycles(u32 addr, u32 size)
{
	if ((addr & 0xfc000000) == 0xe0000000)
		// store queues
		return 0;

	addr &= 0x1fffffff;
	switch (addr >> 26)
	{
	case 0:
		if (!settings.platform.isAtomiswave())
		{
			if (addr < 0x00200000)
			{
				// system rom
				INFO_LOG(SH4, "Invalid write to rom @ %07x", addr);
				return 99;
			}
			if (addr < 0x00200000 + settings.platform.flash_size)
			{
				// flash
				if (size != 1)
					INFO_LOG(SH4, "flashrom: Invalid write size %d @ %07x", size, addr);
				return 28;
			}
		}
		else
		{
			if (addr < 0x00020000)
			{
				// flash
				if (size != 1)
					INFO_LOG(SH4, "flashrom: Invalid write size %d @ %07x", size, addr);
				return 28;
			}
			if (addr >= 0x00200000 && addr < 0x00200000 + settings.platform.flash_size)
			{
				// nvmem
				return 14; // ????
			}
		}
		addr &= 0x01ffffff;
		if (addr >= 0x005f6800 && addr <= 0x005f69ff)
		{
			// holly system control regs
			if (size != 4)
				INFO_LOG(SH4, "holly system reg: Invalid write size %d @ %07x", size, addr);
			return 5;
		}
		if (addr >= 0x005f6c00 && addr <= 0x005f6cff)
		{
			// maple regs
			if (size != 4)
				INFO_LOG(SH4, "maple reg: Invalid write size %d @ %07x", size, addr);
			return 12;
		}
		if (addr >= 0x005f7000 && addr <= 0x005f70ff)
		{
			if (settings.platform.isArcade())
				// naomi/aw cart
				return 14; // ???
			else
			{
				// gd-rom
				if (size > 2)
					INFO_LOG(SH4, "gd-rom: Invalid write size %d @ %07x", size, addr);
				return 28;
			}
		}
		if (addr >= 0x005f7400 && addr <= 0x005f74ff)
		{
			// G1 I/F control regs
			if (size != 4)
				INFO_LOG(SH4, "G1 I/F: Invalid write size %d @ %07x", size, addr);
			return 12;
		}
		if (addr >= 0x005f7800 && addr <= 0x005f78ff)
		{
			// G2 I/F control regs
			if (size != 4)
				INFO_LOG(SH4, "G2 I/F: Invalid write size %d @ %07x", size, addr);
			return 12;
		}
		if (addr >= 0x005f7c00 && addr <= 0x005f7cff)
		{
			// PVR I/F control regs
			if (size != 4)
				INFO_LOG(SH4, "PVR I/F: Invalid write size %d @ %07x", size, addr);
			return 12;
		}
		if (addr >= 0x005f8000 && addr <= 0x005f9fff)
		{
			// TA/PVR core control regs, Palette RAM, fog table
			if (size != 4)
				// TODO 32-byte access allowed for palette and fog tables?
				INFO_LOG(SH4, "PVR/TA core: Invalid write size %d @ %07x", size, addr);
			return 14;
		}
		if (addr >= 0x00600000 && addr <= 0x006007ff)
		{
			if (settings.platform.isAtomiswave())
				// AW registers
				return 14; // ???
			else
			{
				// modem
				if (size != 1)
					INFO_LOG(SH4, "modem: Invalid write size %d @ %07x", size, addr);
				return 44;
			}
		}
		if (addr >= 0x00700000 && addr <= 0x00ffffff)
		{
			// aica regs and ram
			if (size < 4)
				INFO_LOG(SH4, "aica: Invalid read size %d @ %07x", size, addr);
			return 12 * size / 4;	// undocumented
		}
		if (addr >= 0x01000000 && addr <= 0x01ffffff)
		{
			// G2 external area
			switch (size)
			{
			case 1:
			case 2:
			case 4:
				return 28;
			case 32:
			default:
				return 52;
			}
		}
		break;

	case 1:
		// VRAM
		switch (size)
		{
		case 1:
			INFO_LOG(SH4, "vram: Invalid write size 1 @ %07x", addr);
			return 12;
		case 2:
		case 4:
			return 12;
		case 32:
		default:
			return 38;
		}

	case 2:
		// Area 2
		INFO_LOG(SH4, "Invalid read to area 2 @ %07x", addr);
		return 12;

	case 3:
		// System RAM
		return 4;	// or 9 if row miss TODO average?

	case 4:
		// TA FIFO
		if (size != 32)
			INFO_LOG(SH4, "Invalid write size %d to area 4 (TA FIFO) @ %07x", size, addr);
		if ((addr >= 0x10000000 && addr <= 0x107fffff) || (addr >= 0x12000000 && addr <= 0x127fffff))
			// TA polygon data
			return 7;	// undocumented
		if ((addr >= 0x10800000 && addr <= 0x10ffffff) || (addr >= 0x12800000 && addr <= 0x12ffffff))
			// YUV converter
			return 9;	// 858 cycles for 3072 bytes (YUV420)
		if ((addr >= 0x11000000 && addr <= 0x11ffffff) || (addr >= 0x13000000 && addr <= 0x13ffffff))
			// VRAM (64 bits)
			return 5;	// 8 for 32-bit access (LMMODE0/1)
		break;

	case 5:
		// Ext device
		switch (size)
		{
		case 1:
		case 2:
		case 4:
			return 28;
		case 32:
		default:
			return 52;
		}

	case 6:
		// Area 6
		INFO_LOG(SH4, "Invalid write to area 6 @ %07x", addr);
		return 14;

	case 7:
		// SH4 registers
		return 0;
	}

	INFO_LOG(SH4, "Unmapped read @ %08x", addr);
	return 14;
}
