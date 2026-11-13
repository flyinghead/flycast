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
#include "flashrom.h"
#include <vector>

//
// Three-wire Serial EEPROM
//
class AT93CxxSerialEeprom : public WritableChip
{
public:
	// x8: 8-bit organization (ORG pin low), otherwise 16-bit
	AT93CxxSerialEeprom(u32 size, bool x8 = false) : WritableChip(size), x8(x8) {
		memset(data, 0xff, size);
	}

	// combined DO + READY/BUSY
	bool readDO()
	{
		if (dataOutBits > 0)
			// DO
			return (dataOut >> (dataOutBits - 1)) & 1;
		else
			// Ready
			return true;
	}

	// chip select (active high)
	void writeCS(bool state)
	{
		// deselecting the chip aborts any partially received instruction
		if (cs && !state)
		{
			command.clear();
			expected = 0;
		}
		cs = state;
	}

	// clock
	void writeCLK(bool state);

	// data in
	void writeDI(bool state) {
		di = state;
	}

	void Write(u32 addr, u32 data, u32 size) override {
		die("Unsupported");
	}

	void Serialize(Serializer& ser) const override;
	void Deserialize(Deserializer& deser) override;

private:
	void writeWord(u32 addr, u16 v)
	{
		if (x8)
			data[addr & mask] = (u8)v;
		else
			*(u16 *)&data[(addr * 2) & mask] = v;
	}
	int addrBits() const {
		return x8 ? 7 : 6;
	}
	int dataBits() const {
		return x8 ? 8 : 16;
	}
	u8 getCommandAddress() const
	{
		verify((int)command.size() >= 3 + addrBits());
		u8 addr = 0;
		for (int i = 3; i < 3 + addrBits(); i++) {
			addr <<= 1;
			addr |= command[i];
		}
		return addr;
	}

	u16 getCommandData() const
	{
		verify((int)command.size() >= 3 + addrBits() + dataBits());
		u16 v = 0;
		for (int i = 3 + addrBits(); i < 3 + addrBits() + dataBits(); i++) {
			v <<= 1;
			v |= command[i];
		}
		return v;
	}

	const bool x8;
	bool cs = false;
	bool clk = false;
	bool di = false;
	std::vector<bool> command;
	int expected = 0;
	bool writeEnable = false;
	u16 dataOut = 0;
	u8 dataOutBits = 0;
};

//
// Three-wire Serial EEPROM
// 1 Kb (128 x 8)
//
class AT93C46SerialEeprom : public AT93CxxSerialEeprom
{
public:
	AT93C46SerialEeprom(bool x8 = false) : AT93CxxSerialEeprom(128, x8) {}
};

