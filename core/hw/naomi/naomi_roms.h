/*
	Created on: Nov 2, 2018

	Copyright 2018 flyinghead

	Rom information from mame (https://github.com/mamedev/mame)
	license:LGPL-2.1+
	copyright-holders: Samuele Zannoli, R. Belmont, ElSemi, David Haywood, Angelo Salese, Olivier Galibert, MetalliC

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

#define MAX_GAME_FILES 40

enum BlobType {
	Normal = 0,
	SwapWordBytes = 0,
	InterleavedWord,
	Copy,
	Key,
	Eeprom
};

enum CartridgeType {
	M1,
	M2,
	M4,
	AW,
	GD
};

enum RotationType {
    ROT0 = 0,
    ROT270 = 3,
};

struct BIOS_t
{
	const char* name;
	struct
	{
		u32 region;		// 0: Japan, 1: USA, 2: Export, 3: Other
		const char* filename;
		u32 offset;
		u32 length;
		u32 crc;
	} blobs[MAX_GAME_FILES];
};
extern const BIOS_t BIOS[];

struct InputDescriptors;

struct Game
{
	const char* name;
	const char* parent_name;
	const char* description;
	u32 size;
	u32 key;
	const char *bios;
	CartridgeType cart_type;
    RotationType rotation_flag;
	struct
	{
		const char* filename;
		u32 offset;
		u32 length;
		u32 crc;
		BlobType blob_type;
		u32 src_offset;		// For copy
	} blobs[MAX_GAME_FILES];
	const char *gdrom_name;
	InputDescriptors *inputs;
	u8 *eeprom_dump;
	int multiboard;
};
extern const Game Games[];
