/*
	Copyright 2025 flyinghead

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
#include "atomiswave.h"
#include "naomi_cart.h"
#include "hw/sh4/sh4_sched.h"
#include "input/haptic.h"
#include "network/output.h"
#include "serialize.h"

namespace atomiswave
{

static bool aw_ram_test_skipped;
static u8 aw_maple_devs;
static u64 coin_chute_time[4];
static u8 awDigitalOuput;

void reset() {
	aw_ram_test_skipped = false;
	aw_maple_devs = 0;
}

u32 readMem_A0_006(u32 addr, u32 size)
{
	addr &= 0x7ff;
	//printf("atomiswave::readMem %d@%08x: %x\n", size, addr, mem600[addr]);
	switch (addr)
	{
//	case 0:
//		return 0;
//	case 4:
//		return 1;
	case 0x280:
		// 0x00600280 r  0000dcba
		//	a/b - 1P/2P coin inputs (JAMMA), active low
		//	c/d - 3P/4P coin inputs (EX. IO board), active low
		//
		//	(ab == 0) -> BIOS skip RAM test
		if (!aw_ram_test_skipped)
		{
			// Skip RAM test at startup
			aw_ram_test_skipped = true;
			return 0;
		}
		{
			u8 coin_input = 0xF;
			u64 now = sh4_sched_now64();
			for (int slot = 0; slot < 4; slot++)
			{
				if (maple_atomiswave_coin_chute(slot))
				{
					// ggx15 needs 4 or 5 reads to register the coin but it needs to be limited to avoid coin errors
					// 1 s of cpu time is too much, 1/2 s seems to work, let's use 100 ms
					if (coin_chute_time[slot] == 0 || now - coin_chute_time[slot] < SH4_MAIN_CLOCK / 10)
					{
						if (coin_chute_time[slot] == 0)
							coin_chute_time[slot] = now;
						coin_input &= ~(1 << slot);
					}
				}
				else
				{
					coin_chute_time[slot] = 0;
				}
			}
			return coin_input;
		}

	case 0x284:		// Atomiswave maple devices
		// ddcc0000 where cc/dd are the types of devices on maple bus 2 and 3:
		// 0: regular AtomisWave controller
		// 1: light gun
		// 2,3: mouse/trackball
		//printf("NAOMI 600284 read %x\n", aw_maple_devs);
		return aw_maple_devs;
	case 0x288:
		// ??? Dolphin Blue
		return 0;
	case 0x28c:
		return awDigitalOuput;
	}
	INFO_LOG(NAOMI, "Unhandled read @ %x sz %d", addr, size);
	return 0xFF;
}

void writeMem_A0_006(u32 addr, u32 data, u32 size)
{
	addr &= 0x7ff;
	//printf("atomiswave::writeMem %d@%08x: %x\n", size, addr, data);
	switch (addr)
	{
	case 0x284:		// Atomiswave maple devices
		DEBUG_LOG(NAOMI, "NAOMI 600284 write %x", data);
		aw_maple_devs = data & 0xF0;
		return;
	case 0x288:
		// ??? Dolphin Blue
		return;
	case 0x28C:		// Digital output
		if ((u8)data != awDigitalOuput)
		{
			if (atomiswaveForceFeedback)
			{
				// Wheel force feedback:
				// bit 0    direction (0 pos, 1 neg)
				// bit 1-4  strength
				networkOutput.output("awffb", (u8)data);
				// This really needs to be soften
				haptic::setTorque(0, (data & 1 ? -1.f : 1.f) * ((data >> 1) & 0xf) / 15.f * 0.5f);
			}
			else
			{
				u8 changes = data ^ awDigitalOuput;
				for (int i = 0; i < 8; i++)
					if (changes & (1 << i))
					{
						std::string name = "lamp" + std::to_string(i);
						networkOutput.output(name.c_str(), (data >> i) & 1);
					}
			}
			awDigitalOuput = data;
			DEBUG_LOG(NAOMI, "AW output %02x", data);
		}
		return;
	default:
		break;
	}
	INFO_LOG(NAOMI, "Unhandled write @ %x (%d): %x", addr, size, data);
}

void serialize(Serializer& ser)
{
	ser << aw_maple_devs;
	ser << coin_chute_time;
	ser << aw_ram_test_skipped;
}

void deserialize(Deserializer& deser)
{
	deser >> aw_maple_devs;
	if (deser.version() >= Deserializer::V20) {
		deser >> coin_chute_time;
		deser >> aw_ram_test_skipped;
	}
}

}	// namespace atomiswave
