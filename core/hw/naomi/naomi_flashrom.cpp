/*
	Created on: Apr 19, 2020

	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "naomi_flashrom.h"
#include "hw/flashrom/flashrom.h"
#include "hw/holly/sb_mem.h"
#include "hw/maple/maple_devs.h"

extern WritableChip *sys_nvmem;

static u16 eeprom_crc(const u8 *buf, int size)
{
	int n = 0xdebdeb00;

	for (int i = 0; i < size; i++)
	{
		n &= 0xffffff00;
		n += buf[i];

		for (int c = 0; c < 8; c++)
		{
			if (n & 0x80000000)
				n = (n << 1) + 0x10210000;
			else
				n <<= 1;
		}
	}
	for (int c = 0; c < 8; c++)
	{
		if (n & 0x80000000)
			n = (n << 1) + 0x10210000;
		else
			n <<= 1;
	}

	return n >> 16;
}

//
// bbsram layout:
// not totally reveng'ed but there's one fixed-size record,
// followed by a variable-size record starting at 1F8
// where the interesting stuff is.
// Offset    Size
// 0x1f8       2        CRC of bytes [218,218+size[
// 0x1fa       2        0
// 0x1fc       4        record size
// 0x200       4        record padded size (crc is done on this)
// 0x204       4        0
// 0x208       16       Same header repeated
// 0x218    size*2      Record data, repeated twice
//
void write_naomi_flash(u32 addr, u8 value)
{
	addr &= sys_nvmem->mask;
	verify(addr >= 0x218);
	u32 block_size = sys_nvmem->Read(0x200, 4);
	if (addr >= 0x218 + block_size || 0x218 + block_size * 2 > sys_nvmem->size)
	{
		WARN_LOG(NAOMI, "NVMEM record doesn't exist or is too short");
		return;
	}
	sys_nvmem->data[addr] = value;
	sys_nvmem->data[addr + block_size] = value;
	u16 crc = eeprom_crc(&sys_nvmem->data[0x218], block_size);
	*(u16 *)&sys_nvmem->data[0x1f8] = crc;
	*(u16 *)&sys_nvmem->data[0x208] = crc;
}

//
// eeprom layout:
// Offset    Size
// 0         2        CRC of bytes [2,17]
// 2         1        size of record (16, sometimes 1, ignored)
// 3         15       data
// 18        18       same record repeated
// 36        2        CRC of bytes [44,44+size[
// 38        1        record size
// 39        1        record padded size (crc is done on this)
// 40        4        Same header repeated
// 44     size*2      Record data, repeated twice
//
// The first record contains naomi bios settings, and the second one game-specific settings
//
void write_naomi_eeprom(u32 offset, u8 value)
{
	if (offset >= 3 && offset < 20)
	{
		EEPROM[offset] = value;
		EEPROM[offset + 18] = value;

		u16 crc = eeprom_crc((u8 *)EEPROM + 2, 16);
		*(u16 *)&EEPROM[0] = crc;
		*(u16 *)&EEPROM[18] = crc;
	}
	else if (offset >= 44 && (int)offset - 44 < EEPROM[39])
	{
		EEPROM[offset] = value;
		EEPROM[offset + EEPROM[39]] = value;
		u16 crc = eeprom_crc((u8 *)EEPROM + 44, EEPROM[39]);
		*(u16 *)&EEPROM[36] = crc;
		*(u16 *)&EEPROM[40] = crc;
	}
	else
		WARN_LOG(NAOMI, "EEPROM record doesn't exist or is too short");
}
