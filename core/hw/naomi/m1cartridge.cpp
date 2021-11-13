/*
 * m1cartridge.cpp
 *
 *  Created on: Nov 4, 2018
 *      Author: flyinghead
 *  Plagiarized from mame (mame/src/mame/machine/naomim1.cpp)
 *  // license:BSD-3-Clause
 *  // copyright-holders:Olivier Galibert
 */
#include "m1cartridge.h"
#include "serialize.h"

M1Cartridge::M1Cartridge(u32 size) : NaomiCartridge(size)
{
	rom_cur_address = 0;
	buffer_actual_size = 0;
	has_history = false;
	stream_ended = false;
	memset(dict, 0, sizeof(dict));
	memset(hist, 0, sizeof(hist));
	avail_val = 0;
	avail_bits = 0;
	encryption = false;
}

void M1Cartridge::AdvancePtr(u32 size)
{
	if (encryption)
	{
		if (size < buffer_actual_size)
		{
			memmove(buffer, buffer + size, buffer_actual_size - size);
			buffer_actual_size -= size;
		}
		else
		{
			hist[0] = buffer[buffer_actual_size - 2];
			hist[1] = buffer[buffer_actual_size - 1];
			has_history = true;
			buffer_actual_size = 0;
		}
		enc_fill();
	}
	else
		NaomiCartridge::AdvancePtr(size);
}

void M1Cartridge::wb(u8 byte)
{
	if (dict[0] & 64)
		if (buffer_actual_size < 2)
			if (has_history)
				buffer[buffer_actual_size] = hist[buffer_actual_size] - byte;
			else
				buffer[buffer_actual_size] = byte;
		else
			buffer[buffer_actual_size] = buffer[buffer_actual_size - 2] - byte;
	else
		buffer[buffer_actual_size] = byte;

	buffer_actual_size++;
}

void M1Cartridge::enc_fill()
{
	while (buffer_actual_size < sizeof(buffer) && !stream_ended)
	{
		switch (lookb(3)) {
		// 00+2 - 0000+esc
		case 0:
		case 1: {
			skipb(2);
			int addr = getb(2);
			if (addr)
				wb (dict[addr]);
			else
				wb(getb(8));

			break;
		}
			// 010+2
		case 2:
			skipb(3);
			wb (dict[getb(2) + 4]);
			break;
			// 011+3
		case 3:
			skipb(3);
			wb(dict[getb(3) + 8]);
			break;
			// 10+5
		case 4:
		case 5:
			skipb(2);
			wb(dict[getb(5) + 16]);
			break;
			// 11+6
		case 6:
		case 7:
			{
				skipb(2);
				int addr = getb(6) + 48;
				if (addr == 111)
					stream_ended = true;
				else
					wb(dict[addr]);

				break;
			}
		}
	}
	while (buffer_actual_size < sizeof(buffer))
		buffer[buffer_actual_size++] = 0;
}

u32 M1Cartridge::get_decrypted_32b()
{
	u8* base = RomPtr + rom_cur_address;
	u8 a = base[0];
	u8 b = base[1];
	u8 c = base[2];
	u8 d = base[3];
	rom_cur_address += 4;

	u32 swapped_key = (key >> 24) | ((key >> 8) & 0xFF00)
			| ((key << 8) & 0xFF0000) | (key << 24);
	u32 res = swapped_key ^ (((b ^ d) << 24) | ((a ^ c) << 16) | (b << 8) | a);
	return res;
}

void M1Cartridge::Serialize(Serializer& ser) const
{
	ser << buffer;
	ser << dict;
	ser << hist;
	ser << avail_val;
	ser << rom_cur_address;
	ser << buffer_actual_size;
	ser << avail_bits;
	ser << stream_ended;
	ser << has_history;
	ser << encryption;

	NaomiCartridge::Serialize(ser);
}

void M1Cartridge::Deserialize(Deserializer& deser)
{
	deser >> buffer;
	deser >> dict;
	deser >> hist;
	deser >> avail_val;
	deser >> rom_cur_address;
	deser >> buffer_actual_size;
	deser >> avail_bits;
	deser >> stream_ended;
	deser >> has_history;
	deser >> encryption;

	NaomiCartridge::Deserialize(deser);
}
