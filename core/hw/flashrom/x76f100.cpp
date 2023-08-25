// Derived from mame src/devices/machine/x76f100.{h,cpp}
// license:BSD-3-Clause
// copyright-holders:smf
#include "x76f100.h"
#include "serialize.h"

void X76F100SerialFlash::writeSCL(bool v)
{
	if (!lastCS)
	{
		switch (state)
		{
		case State::STOP:
			break;

		case State::RESPONSE_TO_RESET:
			if (lastSCL && !v)
			{
				if (bit == 0)
					shift = responseToReset[byteOffset];

				sda = shift & 1;
				shift >>= 1;
				bit++;

				if (bit == 8)
				{
					bit = 0;
					byteOffset++;
					if (byteOffset == sizeof(responseToReset))
						byteOffset = 0;
				}
			}
			break;

		case State::LOAD_COMMAND:
		case State::LOAD_PASSWORD:
		case State::VERIFY_PASSWORD:
		case State::WRITE_DATA:
			if (!lastSCL && v)
			{
				if (bit < 8)
				{
					shift <<= 1;

					if (lastSDA)
						shift |= 1;
					bit++;
				}
				else
				{
					sda = false;

					switch (state)
					{
					case State::LOAD_COMMAND:
						command = shift;
						DEBUG_LOG(FLASHROM, "-> command: %02x", command);
						state = State::LOAD_PASSWORD;
						break;

					case State::LOAD_PASSWORD:
						DEBUG_LOG(FLASHROM, "-> password: %02x", shift);
						writeBuffer[byteOffset++] = shift;

						if (byteOffset == sizeof(writeBuffer))
						{
							state = State::VERIFY_PASSWORD;

							// We don't check the password
							u8 *password;
							if ((command & 0xe1) == (int)Command::READ)
								password = readPassword;
							else
								password = writePassword;
							DEBUG_LOG(FLASHROM, "Password accepted: %d", memcmp(password, writeBuffer, sizeof(writeBuffer)) == 0);
						}
						break;

					case State::VERIFY_PASSWORD:
						DEBUG_LOG(FLASHROM, "-> verify password: %02x", shift);

						if (shift == (int)Command::ACK_PASSWORD)
						{
							if (true) // password accepted
							{
								if ((command & 0x81) == (int)Command::READ)
									state = State::READ_DATA;
								else if ((command & 0x81) == (int)Command::WRITE)
									state = State::WRITE_DATA;
							}
							else
								sda = true;
						}
						break;

					case State::WRITE_DATA:
						DEBUG_LOG(FLASHROM, "-> data: %02x", shift );
						writeBuffer[byteOffset++] = shift;

						if (byteOffset == sizeof(writeBuffer))
						{
							if (command == (int)Command::CHANGE_WRITE_PASSWORD)
							{
								memcpy(writeBuffer, writePassword, sizeof(writePassword));
							}
							else if (command == (int)Command::CHANGE_READ_PASSWORD)
							{
								memcpy(writeBuffer, readPassword, sizeof(readPassword));
							}
							else
							{
								for (byteOffset = 0; byteOffset < sizeof(writeBuffer); byteOffset++)
								{
									int offset = dataOffset();

									if (offset != -1)
										data[offset] = writeBuffer[byteOffset];
									else
										break;
								}
							}
							byteOffset = 0;
						}
						break;

					default:
						break;
					}

					bit = 0;
					shift = 0;
				}
			}
			break;

		case State::READ_DATA:
			if (!lastSCL && v)
			{
				if (bit < 8)
				{
					if (bit == 0)
					{
						int offset = dataOffset();

						if (offset != -1)
							shift = data[offset];
						else
							shift = 0;
					}

					sda = (shift >> 7) & 1;
					shift <<= 1;
					bit++;
				}
				else
				{
					bit = 0;
					sda = false;

					if (!lastSDA)
					{
						DEBUG_LOG(FLASHROM, "ack <-");
						byteOffset++;
					}
					else
					{
						DEBUG_LOG(FLASHROM, "nak <-");
					}
				}
			}
			break;
		}
	}

	SCLChanged = (lastSCL != v);
	lastSCL = v;
}

void X76F100SerialFlash::writeSDA(bool v)
{
	if (lastSCL && !SCLChanged && !lastCS)
    {
		if (!lastSDA && v)
		{
			DEBUG_LOG(FLASHROM, "goto stop");
			state = State::STOP;
			sda = false;
		}

		if (lastSDA && !v)
		{
			switch (state)
			{
			case State::STOP:
				DEBUG_LOG(FLASHROM, "goto start");
				state = State::LOAD_COMMAND;
				break;

			case State::LOAD_PASSWORD:
				DEBUG_LOG(FLASHROM, "goto start");
				break;

			case State::READ_DATA:
				DEBUG_LOG(FLASHROM, "reading");
				break;

			default:
				DEBUG_LOG(FLASHROM, "skipped start (default)");
				break;
			}

			bit = 0;
			byteOffset = 0;
			shift = 0;
			sda = false;
		}
    }
    lastSDA = v;
}

void X76F100SerialFlash::reset()
{
	lastSCL = false;
	SCLChanged = false;
	lastSDA = false;
	sda = false;
	lastRST = false;
	lastCS = false;
	state = State::STOP;
	bit = 0;
	byteOffset = 0;
	command = 0;
	shift = 0;
	memset(writeBuffer, 0, sizeof(writeBuffer));
}

int X76F100SerialFlash::dataOffset()
{
	int block_offset = (command >> 1) & 0x0f;
	int offset = (block_offset * 8) + byteOffset;

	// Technically there are 4 bits assigned to sector values but since the data array is only 112 bytes,
	// it will try reading out of bounds when the sector is 14 (= starts at 112) or 15 (= starts at 120).
	if (offset >= (int)sizeof(data))
		return -1;

	return offset;
}

void X76F100SerialFlash::serialize(Serializer& ser) const
{
	ser << writeBuffer;
	ser << lastSCL;
	ser << SCLChanged;
	ser << lastSDA;
	ser << sda;
	ser << lastRST;
	ser << lastCS;
	ser << state;
	ser << command;
	ser << byteOffset;
	ser << bit;
	ser << shift;
}

void X76F100SerialFlash::deserialize(Deserializer& deser)
{
	deser >> writeBuffer;
	deser >> lastSCL;
	deser >> SCLChanged;
	deser >> lastSDA;
	deser >> sda;
	deser >> lastRST;
	deser >> lastCS;
	deser >> state;
	deser >> command;
	deser >> byteOffset;
	deser >> bit;
	deser >> shift;
}

