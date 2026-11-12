/*
	Sega Hikaru board emulation.

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
#include "hw/naomi/naomi_cart.h"

namespace hikaru
{

// Physical memory sizes
constexpr u32 TEXRAM_SIZE = 16_MB;	// GPU texture/frame RAM (SH-4 sees the low 8 MB at 0x1B000000)
constexpr u32 CMDRAM_SIZE = 4_MB;	// GPU command RAM, shared by both CPUs
constexpr u32 SLAVE_RAM_SIZE = 32_MB;
constexpr u32 WAVE_RAM_SIZE = 8_MB;	// per AICA
constexpr u32 SRAM_SIZE = 64_KB;	// battery-backed backup RAM

// Cartridge ROM layout (see naomi_roms.cpp Hikaru entries):
//   0x0000000-0x1FFFFFF: EPROMs, one 8 MB slot per IC pair (bus banks 0x10-0x13)
//   0x2000000-:          mask ROMs (bus banks 0x20-0x2F)
constexpr u32 ROM_MASK_OFFSET = 0x2000000;

void init();
void term();
void reset(bool hard);
// Map the Hikaru master SH-4 address space for the given mirror base (0x00, 0x20, ... 0xC0)
void mapArea(u32 base);
void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

u8 *getTexRam();
u8 *getCmdRam();
u8 *getSlaveRam();

// Raw 32-bit access to the external bus (bank = bits 24-30 of a bus address)
u32 busRead32(u32 busAddr);
void busWrite32(u32 busAddr, u32 data);
// Host pointer to bus memory, or nullptr if the bus address isn't plain memory
u8 *busPtr(u32 busAddr);

// Slave CPU access to its own PCI bridge and to registers shared by both CPUs
template<typename T> T slaveBridgeRead(u32 addr);
template<typename T> void slaveBridgeWrite(u32 addr, T data);
bool sharedRegRead(u32 addr, u16& value);
bool sharedRegWrite(u32 addr, u16 value);

// Master SH-4 port A (PDTRA): security ID chip and slave NMI
void portAWrite(u16 data);
void configEepromWrite(u8 data);
bool configEepromRead();
u16 portARead();

class HikaruCartridge : public Cartridge
{
public:
	HikaruCartridge(u32 size) : Cartridge(size) {}

	u32 ReadMem(u32 address, u32 size) override { return 0xffffffff; }
	void WriteMem(u32 address, u32 data, u32 size) override {}
	void* GetDmaPtr(u32 &size) override { size = 0; return nullptr; }
	void AdvancePtr(u32 size) override {}
	bool GetBootId(RomBootID *bootId) override { return false; }

	void SetKey(u32 key) override { this->key = key; }

	u8 *data() const { return RomPtr; }
	u32 getKey() const { return key; }
	u32 size() const { return RomSize; }

private:
	u32 key = 0;		// 315-5881 game key, 0 if the ROM board has none
};

}
