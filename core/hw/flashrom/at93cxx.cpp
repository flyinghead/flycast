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
#include "at93cxx.h"

//#define DEBUG_EEPROM 1

#ifdef DEBUG_EEPROM
#define EEPROM_LOG(...) DEBUG_LOG(FLASHROM, __VA_ARGS__)
#else
#define EEPROM_LOG(...)
#endif

void AT93CxxSerialEeprom::writeCLK(bool state)
{
	// CS asserted and CLK rising
	if (!clk && state && cs)
	{
		if (dataOutBits > 0)
			dataOutBits--;
		if (dataOutBits == 0)
		{
			if (command.empty() && !di)
				INFO_LOG(NAOMI, "serial eeprom: Ignoring bit 0 (start bit must be 1)");
			else
			{
				command.push_back(di);
				if ((int)command.size() == 3 + addrBits())
				{
					int opCode = (int)command[1] * 2 + (int)command[2];
					switch (opCode)
					{
					case 0: // write enable/disable, erase all, write all
						{
							int subCode = (int)command[3] * 2 + (int)command[4];
							switch (subCode)
							{
							case 0: // disable write
								EEPROM_LOG("serial eeprom: EWDS");
								writeEnable = false;
								command.clear();
								break;
							case 1: // write all
								expected = 3 + addrBits() + dataBits();
								break;
							case 2: // erase all
								EEPROM_LOG("serial eeprom: ERAL");
								if (writeEnable)
									memset(data, 0xff, size);
								command.clear();
								break;
							case 3: // enable write
								EEPROM_LOG("serial eeprom: EWEN");
								writeEnable = true;
								command.clear();
								break;
							}
						}
						break;
					case 1: // write
						expected = 3 + addrBits() + dataBits();
						break;
					case 2: // read
						if (x8)
							dataOut = Read(getCommandAddress(), 1);
						else
							dataOut = Read(getCommandAddress() * 2, 2);
						dataOutBits = dataBits() + 1; // leading dummy 0 bit (also, 0 means no data)
						EEPROM_LOG("serial eeprom: READ %x: %x", getCommandAddress(), dataOut);
						command.clear();
						break;
					case 3: // erase
						EEPROM_LOG("serial eeprom: ERASE %x", getCommandAddress());
						if (writeEnable)
							writeWord(getCommandAddress(), 0xffff);
						command.clear();
						break;
					}
				}
				else if (expected > 0 && (int)command.size() == expected)
				{
					int opCode = (int)command[1] * 2 + (int)command[2];
					switch (opCode)
					{
					case 0: // write all
						{
							u16 v = getCommandData();
							EEPROM_LOG("serial eeprom: WRAL = %x", v);
							if (writeEnable)
								for (u32 i = 0; i < size >> (x8 ? 0 : 1); i++)
									writeWord(i, v);
						}
						break;
					case 1: // write
						EEPROM_LOG("serial eeprom: WRITE %x = %x", getCommandAddress(), getCommandData());
						if (writeEnable)
							writeWord(getCommandAddress(), getCommandData());
						break;
					}
					command.clear();
					expected = 0;
				}
			}
		}
	}
	clk = state;
}

void AT93CxxSerialEeprom::Serialize(Serializer& ser) const
{
	ser << cs;
	ser << clk;
	ser << di;
	ser << (u32)command.size();
	for (bool b : command)
		ser << b;
	ser << expected;
	ser << writeEnable;
	ser << dataOut;
	ser << dataOutBits;
}

void AT93CxxSerialEeprom::Deserialize(Deserializer& deser)
{
	deser >> cs;
	deser >> clk;
	deser >> di;
	u32 size;
	deser >> size;
	command.resize(size);
	for (u32 i = 0; i < size; i++) {
		bool b;
		deser >> b;
		command[i] = b;
	}
	deser >> expected;
	deser >> writeEnable;
	deser >> dataOut;
	deser >> dataOutBits;
}
