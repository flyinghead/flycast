// Derived from mame src/devices/machine/x76f100.{h,cpp}
// license:BSD-3-Clause
// copyright-holders:smf
/*
 * The X76F100 is a Password Access Security Supervisor, containing one 896-bit Secure SerialFlash array.
 * Access to the memory array can be controlled by two 64-bit passwords. These passwords protect read and
 * write operations of the memory array.
 */
#pragma once
#include "types.h"
#include <string.h>

class X76F100SerialFlash
{
public:
	void reset();

	// Clock
	void writeSCL(bool v);

	// Data I/O
	void writeSDA(bool v);

	bool readSDA() {
		return lastCS ? 1 : sda;
	}

	// Chip select
	void writeCS(bool v)
	{
		if (lastCS && !v)
			state = State::STOP;
		else if (!lastCS && v) {
			state = State::STOP;
			sda = false;
		}
		lastCS = v;
	}

	// Reset
	void writeRST(bool v)
	{
		if (v && !lastRST && !lastCS)
		{
			DEBUG_LOG(FLASHROM, "reset");
			state = State::RESPONSE_TO_RESET;
			bit = 0;
			byteOffset = 0;
		}
		lastRST = v;
	}

	void setData(const u8 *data)
	{
		size_t index = 0;
		memcpy(responseToReset, &data[index], sizeof(responseToReset));
		index += sizeof(responseToReset);
		memcpy(writePassword, &data[index], sizeof(writePassword));
		index += sizeof(writePassword);
		memcpy(readPassword, &data[index], sizeof(readPassword));
		index += sizeof(readPassword);
		memcpy(this->data, &data[index], sizeof(this->data));
	}

	void serialize(Serializer& ser) const;
	void deserialize(Deserializer& deser);

private:
	enum class Command
	{
		WRITE = 0x80,
		READ = 0x81,
		CHANGE_WRITE_PASSWORD = 0xfc,
		CHANGE_READ_PASSWORD = 0xfe,
		ACK_PASSWORD = 0x55
	};

	enum class State
	{
		STOP,
		RESPONSE_TO_RESET,
		LOAD_COMMAND,
		LOAD_PASSWORD,
		VERIFY_PASSWORD,
		READ_DATA,
		WRITE_DATA
    };

	int dataOffset();

	u8 data[112] {};
	u8 readPassword[8] {};
	u8 writePassword[8] {};
	u8 responseToReset[4] { 0x19, 0x00, 0xaa, 0x55 };
	u8 writeBuffer[8] {};
	bool lastSCL = false;
	bool lastSDA = false;
	bool sda = false;
	bool lastRST = false;
	bool lastCS = false;
	bool SCLChanged = false;
	State state = State::STOP;
	u8 command = 0;
	u8 byteOffset = 0;
	u8 bit = 0;
	u8 shift = 0;
};
