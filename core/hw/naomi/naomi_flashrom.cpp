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
#include "hw/flashrom/nvmem.h"
#include "hw/maple/maple_devs.h"
#include "cfg/option.h"

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
	verify(addr >= 0x218);
	u32 block_size = nvmem::readFlash(0x200, 4);
	if (addr >= 0x218 + block_size || 0x218 + block_size * 2 > settings.platform.flash_size)
	{
		WARN_LOG(NAOMI, "NVMEM record doesn't exist or is too short");
		return;
	}
	u8 *flashData = nvmem::getFlashData();
	flashData[addr] = value;
	flashData[addr + block_size] = value;
	u16 crc = eeprom_crc(&flashData[0x218], block_size);
	*(u16 *)&flashData[0x1f8] = crc;
	*(u16 *)&flashData[0x208] = crc;
}

//
// eeprom layout:
// Offset    Size
// 0         2        CRC of bytes [2,17]
// 2         16       data
// 18        18       same record repeated
// 36        2        CRC of bytes [44,44+size[
// 38        1        record size
// 39        1        record padded size (crc is done on this)
// 40        4        Same header repeated
// 44     size*2      Record data, repeated twice
//
// The first record contains naomi bios settings, and the second one game-specific settings
// common settings:
// 2	b0 vertical screen
//		b4 attract mode sound
// 3    Game serial ID (4 chars)
// 7    unknown (9 or x18)
// 8	b0: coin chute type (0 common, 1 individual)
//      b4-5: cabinet type (0: 1P, 10: 2P, 20: 2P, 30: 4P)
// 9	coin setting (-1), 27 is manual
// 10   coin to credit
// 11	chute 1 multiplier
// 12	chute 2 multiplier
// 13	bonus adder (coins)
// 14	coin sequence: b0-3 seq 1, b4-7 seq 2
// 15	coin sequence: b0-3 seq 3, b4-7 seq 4
// 16	coin sequence: b0-3 seq 5, b4-7 seq 6
// 17	coin sequence: b0-3 seq 7, b4-7 seq 8
//

void write_naomi_eeprom(u32 offset, u8 value)
{
	if (offset >= 2 && offset < 18)
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

static u8 readEeprom(u32 offset)
{
	return EEPROM[offset & 127];
}

static bool initEeprom(const RomBootID *bootId)
{
	if (!memcmp(bootId->gameID, &EEPROM[3], sizeof(bootId->gameID)))
		return false;
	NOTICE_LOG(NAOMI, "Initializing Naomi EEPROM for game %.32s", bootId->gameTitle[0]);
	for (int i = 0; i < 4; i++)
		write_naomi_eeprom(3 + i, bootId->gameID[i]);
	write_naomi_eeprom(7, 9);	// FIXME 9 or 0x18?
	if (bootId->cabinet == 0
			&& (settings.input.JammaSetup == JVS::FourPlayers
					|| settings.input.JammaSetup == JVS::DualIOBoards4P
					|| settings.input.JammaSetup == JVS::WorldKicks
					|| settings.input.JammaSetup == JVS::WorldKicksPCB))
		write_naomi_eeprom(8, 0x30);
	else if (bootId->cabinet & 8)
		write_naomi_eeprom(8, 0x30);
	else if (bootId->cabinet & 4)
		write_naomi_eeprom(8, 0x20);
	else if (bootId->cabinet & 2)
		write_naomi_eeprom(8, 0x10);
	else
		write_naomi_eeprom(8, 0);
	if (bootId->coinFlag[0][0] == 1)
	{
		// ROM-specific defaults
		write_naomi_eeprom(2, bootId->coinFlag[0][1] | (((bootId->coinFlag[0][1] & 2) ^ 2) << 3));
		if (bootId->coinFlag[0][2] == 1) // individual coin chute
			write_naomi_eeprom(8, readEeprom(8) | 1);
		write_naomi_eeprom(9, bootId->coinFlag[0][3] - 1);
		write_naomi_eeprom(10, std::max(bootId->coinFlag[0][6], (u8)1));
		write_naomi_eeprom(11, std::max(bootId->coinFlag[0][4], (u8)1));
		write_naomi_eeprom(12, std::max(bootId->coinFlag[0][5], (u8)1));
		write_naomi_eeprom(13, bootId->coinFlag[0][7]);
		write_naomi_eeprom(14, bootId->coinFlag[0][8] | (bootId->coinFlag[0][9] << 4));
		write_naomi_eeprom(15, bootId->coinFlag[0][10] | (bootId->coinFlag[0][11] << 4));
		write_naomi_eeprom(16, bootId->coinFlag[0][12] | (bootId->coinFlag[0][13] << 4));
		write_naomi_eeprom(17, bootId->coinFlag[0][14] | (bootId->coinFlag[0][15] << 4));
	}
	else
	{
		// BIOS defaults
		write_naomi_eeprom(2, (bootId->vertical & 2) ? 0x11 : 0x10);
		write_naomi_eeprom(9, 0);
		write_naomi_eeprom(10, 1);
		write_naomi_eeprom(11, 1);
		write_naomi_eeprom(12, 1);
		write_naomi_eeprom(13, 0);
		write_naomi_eeprom(14, 0x11);
		write_naomi_eeprom(15, 0x11);
		write_naomi_eeprom(16, 0x11);
		write_naomi_eeprom(17, 0x11);
	}
	return true;
}

void configure_naomi_eeprom(const RomBootID *bootId)
{
	initEeprom(bootId);
	// Horizontal / vertical screen orientation
	if (bootId->vertical == 2)
	{
		NOTICE_LOG(NAOMI, "EEPROM: vertical monitor orientation");
		write_naomi_eeprom(2, readEeprom(2) | 1);
		config::Rotate90.override(true);
	}
	else if (bootId->vertical == 1)
	{
		NOTICE_LOG(NAOMI, "EEPROM: horizontal monitor orientation");
		write_naomi_eeprom(2, readEeprom(2) & ~1);
	}
	// Number of players
	if (bootId->cabinet != 0 && bootId->cabinet < 0x10)
	{
		int nPlayers = readEeprom(8) >> 4;	// 0 to 3
		if (!(bootId->cabinet & (1 << nPlayers)))
		{
			u8 coinChute = readEeprom(8) & 1;
			if (bootId->cabinet & 8)
			{
				NOTICE_LOG(NAOMI, "EEPROM: 4-player cabinet");
				write_naomi_eeprom(8, 0x30 | coinChute);
			}
			else if (bootId->cabinet & 4)
			{
				NOTICE_LOG(NAOMI, "EEPROM: 3-player cabinet");
				write_naomi_eeprom(8, 0x20 | coinChute);
			}
			else if (bootId->cabinet & 2)
			{
				NOTICE_LOG(NAOMI, "EEPROM: 2-player cabinet");
				write_naomi_eeprom(8, 0x10 | coinChute);
			}
			else if (bootId->cabinet & 1)
			{
				NOTICE_LOG(NAOMI, "EEPROM: 1-player cabinet");
				write_naomi_eeprom(8, 0x00 | coinChute);
			}
		}
	}
	// Region
	if (bootId->country != 0 && (bootId->country & (1 << config::Region)) == 0)
	{
		if (bootId->country & 2)
		{
			NOTICE_LOG(NAOMI, "Forcing region USA");
			config::Region.override(1);
		}
		else if (bootId->country & 4)
		{
			NOTICE_LOG(NAOMI, "Forcing region Export");
			config::Region.override(2);
		}
		else if (bootId->country & 1)
		{
			NOTICE_LOG(NAOMI, "Forcing region Japan");
			config::Region.override(0);
		}
		else if (bootId->country & 8)
		{
			NOTICE_LOG(NAOMI, "Forcing region Korea");
			config::Region.override(3);
		}
		naomi_cart_LoadBios(settings.content.fileName.c_str());
	}
	// Coin settings
	if (config::ForceFreePlay)
		write_naomi_eeprom(9, 27 - 1);
}

static u32 aw_crc32(const void *data, size_t len)
{
	constexpr u32 POLY = 0xEDB88320;
	const u8 *buffer = (const u8 *)data;
	u32 crc = -1;

	while (len--)
	{
		crc = crc ^ *buffer++;
		for (int bit = 0; bit < 8; bit++)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ POLY;
			else
				crc = crc >> 1;
		}
	}
	return ~crc;
}

void configure_maxspeed_flash(bool enableNetwork, bool master)
{
	u8 *flashData = nvmem::getFlashData();
	if (enableNetwork)
	{
		flashData[0x3358] = 0;
		flashData[0x46ac] = 0;
		flashData[0x335c] = !master;
		flashData[0x46b0] = !master;
	}
	else
	{
		flashData[0x3358] = 1;
		flashData[0x46ac] = 1;
	}
	u32 crc = aw_crc32(&flashData[0x2200], 0x1354);
	*(u32 *)&flashData[0x34] = crc;
	*(u32 *)&flashData[0x38] = crc;
	*(u32 *)&flashData[0x84] = crc;
	*(u32 *)&flashData[0x88] = crc;
	crc = aw_crc32(&flashData[0x20], 0x44);
	*(u32 *)&flashData[0x64] = crc;
	*(u32 *)&flashData[0xb4] = crc;
}
