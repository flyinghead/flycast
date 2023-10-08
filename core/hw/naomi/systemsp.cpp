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
// based on mame code:
// license:BSD-3-Clause
// copyright-holders:David Haywood, MetalliC
#include "systemsp.h"
#include "naomi_cart.h"
#include "hw/flashrom/nvmem.h"
#include "network/ggpo.h"
#include "hw/maple/maple_cfg.h"
#include "input/gamepad.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/mem/addrspace.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "oslib/storage.h"
#include "oslib/oslib.h"
#include "cfg/option.h"
#include "card_reader.h"
#include "naomi_roms.h"
#include "stdclass.h"
#include <errno.h>

#ifdef DEBUG_SERIAL
#define SERIAL_LOG(...) DEBUG_LOG(NAOMI, __VA_ARGS__)
#else
#define SERIAL_LOG(...)
#endif
#ifdef DEBUG_FLASH
#define FLASH_LOG(...) DEBUG_LOG(FLASHROM, __VA_ARGS__)
#else
#define FLASH_LOG(...)
#endif
#ifdef DEBUG_ATA
#define ATA_LOG(...) DEBUG_LOG(NAOMI, __VA_ARGS__)
#else
#define ATA_LOG(...)
#endif
#ifdef DEBUG_IO
#define IO_LOG(...) DEBUG_LOG(NAOMI, __VA_ARGS__)
#else
#define IO_LOG(...)
#endif

namespace systemsp
{

SystemSpCart *SystemSpCart::Instance;

constexpr u8 DINOKING_CHIP_DATA[128] = {
    0x12, 0x34, 0x56, 0x78, // Serial No.0
    0x31, 0x00, 0x86, 0x07, // Serial No.1
    0x00, 0x00, 0x00, 0x00, // Key
    0x05, 0x40, 0x00, 0xAA, // Extend  Extend  Access  Mode
    0x23, 0xFF, 0xFF, 0xFF, // Counter4  Counter3  Counter2  Counter1
    0x00, 0x00, 0x00, 0x00, // User Data (first set date: day bits 0-4, month bits 5-8, year bits 9-... + 2000)
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x23, 0xFF, 0xFF, 0xFF, // User Data (max counters)
};
constexpr u8 LOVEBERRY_CHIP_DATA[128] = {
    0x10, 0xc5, 0x10, 0x96, // Serial No.0
    0xab, 0x2f, 0xf1, 0x74, // Serial No.1
    0x99, 0x33, 0x78, 0xa7, // Key
    0x05, 0x77, 0x00, 0xAA, // Extend  Extend  Access  Mode
    0x23, 0xFF, 0xFF, 0xFF, // Counter4  Counter3  Counter2  Counter1
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x00, 0x00, 0x00, 0x00, // User Data
    0x23, 0xFF, 0xFF, 0xFF, // User Data
};

//
// RS232C I/F board (838-14244) connected to RFID Chip R/W board (838-14243 aka Maxell Picochet ME-MR23)
//
class RfidReaderWriter : public SerialPort::Pipe
{
	enum Response : u8 {
		OK = 0xa,
		RETRY = 0xca,
		NG = 0x3a,
		NOT = 0x6a,
	};

	enum Command : u8 {
		REQ = 0x4e,
		SEL = 0x2e,
		READ = 0xe,
		WRITE = 0xce,
		WRITE_NOP = 0x8e,
		WRITE_ST = 0xee,
		HALT = 0xae,
		COUNT = 0x6e,
		RESET = 0x7e,
		TEST = 0x5e,
		CHANGE = 0x3e,
	};
	static constexpr u8 TEST_CODE = 0;
	static constexpr u8 RF_OFF = 0x80;

public:
	RfidReaderWriter(SerialPort *port, int index, const std::string& gameName) : port(port), index(index), gameName(gameName)
	{
		port->setPipe(this);

		FILE *f = nowide::fopen(getCardDataPath().c_str(), "rb");
		if (f != nullptr)
		{
			if (fread(cardData.data(), 1, cardData.size(), f) != cardData.size())
				WARN_LOG(NAOMI, "Rfid card %d: truncated read", index);
			fclose(f);
		}
		else {
			makeNewCard();
		}
	}

	void write(u8 v) override
	{
		if (expectedBytes > 0)
		{
			SERIAL_LOG("UART%d write data out: %02x", index, v);
			recvBuffer.push_back(v);
			if (recvBuffer.size() < expectedBytes)
				return;

			switch (recvBuffer[0])
			{
			case TEST:
				if (recvBuffer[1] == RF_OFF)
				{
					state = Off;
					toSend.push_back(OK);
					SERIAL_LOG("UART%d state Off", index);
				}
				else if (state == Off)
				{
					state = Connecting;
					toSend.push_back(NOT);
					SERIAL_LOG("UART%d state Connecting", index);
				}
				else
					toSend.push_back(OK);
				break;
			case SEL:
				if ((state == Connected || state == FullRead)
						&& (recvBuffer[5] != 0 || recvBuffer[6] != 0 || recvBuffer[7] != 0 || recvBuffer[8] != 0))
					state = FullRead;
				else
					state = Connected;
				toSend.push_back(OK);
				break;
			case COUNT:
				{
					u8 newVal = 0;
					switch (recvBuffer[1])
					{
					case 0xc0:
						newVal = --cardData[16];
						break;
					case 0x30:
						newVal = --cardData[17];
						break;
					case 0x0c:
						newVal = --cardData[18];
						break;
					case 0x03:
						newVal = --cardData[19];
						if (newVal == 0)
							makeNewCard();
						break;
					}
					toSend.push_back(OK);
					toSend.push_back(newVal);
					saveData();
					SERIAL_LOG("UART%d COUNT %x -> %x", index, recvBuffer[1], newVal);
					break;
				}
			case WRITE:
				memcpy(&cardData[4 * rowCounter], &recvBuffer[1], 4);
				saveData();
				SERIAL_LOG("UART%d card written @ row %d", index, rowCounter);
				toSend.push_back(OK);
				break;
			case WRITE_NOP:
			case WRITE_ST:
			case CHANGE:
			case HALT:
				toSend.push_back(OK);
				break;
			default:
				SERIAL_LOG("UART%d unhandled cmd %x (len %d)", index, recvBuffer[0], expectedBytes);
				break;
			}
			expectedBytes = 0;
		}
		else
		{
			switch (v)
			{
			case TEST:
				SERIAL_LOG("UART%d cmd TEST", index);
				expect(v, 2);
				break;
			case CHANGE: // change baud rate
				SERIAL_LOG("UART%d cmd CHANGE", index);
				expect(v, 1);
				break;
			case RESET:
				SERIAL_LOG("UART%d cmd RESET", index);
				state = Off;
				toSend.push_back(OK);
				break;
			case HALT:
				SERIAL_LOG("UART%d cmd HALT", index);
				expect(v, 4); // ser0
				break;
			case READ:
				SERIAL_LOG("UART%d cmd READ", index);
				toSend.push_back(OK);
				if (state == FullRead) {
					toSend.insert(toSend.end(), cardData.begin(), cardData.end());
				}
				else
				{
					toSend.insert(toSend.end(), cardData.begin(), cardData.begin() + 8);
					static const u8 maskedData[120] = { 0, 0, 0, 0, 0, 0, 0, 0xaa };
					toSend.insert(toSend.end(), std::begin(maskedData), std::end(maskedData));
				}
				break;
			case SEL:
				SERIAL_LOG("UART%d cmd SEL", index);
				expect(v, 8);	// ser0 key (key is calc'ed from ser1)
				break;
			case REQ:
				SERIAL_LOG("UART%d cmd REQ", index);
				if (state == Connecting)
				{
					state = Connected;
					toSend.push_back(RETRY);
					SERIAL_LOG("UART%d state Connected", index);
				}
				else if (state == Connected || state == FullRead)
				{
					toSend.push_back(OK);
					toSend.insert(toSend.end(), &cardData[0], &cardData[4]);	// ser0
					SERIAL_LOG("UART%d exec REQ", index);
				}
				else
					toSend.push_back(RETRY);
				break;
			case COUNT:
				SERIAL_LOG("UART%d cmd COUNT", index);
				expect(v, 1);
				break;
			case WRITE:
				SERIAL_LOG("UART%d cmd WRITE", index);
				rowCounter++;
				expect(v, 4);
				break;
			case WRITE_ST:
			case WRITE_NOP:
				SERIAL_LOG("UART%d cmd %s", index, v == WRITE_ST ? "WRITE_ST" : "WRITE_NOP");
				if (v == WRITE_ST)
					rowCounter = 0;
				else
					rowCounter++;
				expect(v, 4);
				break;
			default:
				INFO_LOG(NAOMI, "UART%d write data out: unknown cmd %x", index, v);
				toSend.push_back(NG);
				break;
			}
		}
		port->updateStatus();
	}

	int available() override {
		return toSend.size();
	}

	u8 read() override
	{
		u8 b = 0;
		if (!toSend.empty())
		{
			b = toSend.front();
			toSend.pop_front();
		}
		if (toSend.empty())
			port->updateStatus();
		return b;
	}

	void serialize(Serializer& ser) const override
	{
		ser << (u32)toSend.size();
		for (u8 b : toSend)
			ser << b;
		ser << expectedBytes;
		ser << (u32)recvBuffer.size();
		ser.serialize(recvBuffer.data(), recvBuffer.size());
		ser << state;
		ser << rowCounter;
		ser.serialize(cardData.data(), cardData.size());
	}

	void deserialize(Deserializer& deser) override
	{
		u32 size;
		deser >> size;
		toSend.resize(size);
		for (u32 i = 0; i < size; i++)
			deser >> toSend[i];
		deser >> expectedBytes;
		deser >> size;
		recvBuffer.resize(size);
		deser.deserialize(recvBuffer.data(), recvBuffer.size());
		if (deser.version() >= Deserializer::V41)
		{
			deser >> state;
			deser >> rowCounter;
			deser.deserialize(cardData.data(), cardData.size());
		}
		else
		{
			state = Off;
			rowCounter = 0;
		}
	}

private:
	void expect(u8 cmd, int bytes)
	{
		expectedBytes = bytes + 1;
		recvBuffer.clear();
		recvBuffer.push_back(cmd);
	}

	void makeNewCard()
	{
		INFO_LOG(NAOMI, "Creating new RFID card");
		if (gameName.substr(0, 6) == "dinoki")
		{
			memcpy(&cardData[0], &DINOKING_CHIP_DATA[0], sizeof(DINOKING_CHIP_DATA));
			for (int i = 0; i < 4; i++)
				cardData[i] = rand() & 0xff;

			u32 ser1 = ((rand() & 0xffff) << 16) | (rand() & 0xffff);
			ser1 &= ~0xc0000000;
			if (config::Region == 1) // USA (dinoking, dinokior)
				ser1 |= 0x40000000;
			ser1 = dinoShuffle(ser1);
			cardData[4] = ser1 >> 24;
			cardData[5] = ser1 >> 16;
			cardData[6] = ser1 >> 8;
			cardData[7] = ser1;
		}
		else
		{
			memcpy(&cardData[0], &LOVEBERRY_CHIP_DATA[0], sizeof(LOVEBERRY_CHIP_DATA));
			for (int i = 0; i < 4; i++)
				cardData[i] = rand() & 0xff;

			cardData[5] = rand() & 0xff;
			cardData[7] = rand() & 0xff;
			u32 ser1 = (cardData[4] << 24) | (cardData[5] << 16) | (cardData[6] << 8) | cardData[7];
			if (config::Region == 2) // Export
			{
				ser1 = loveberyDeobfuscate(ser1);
				ser1 &= ~0xc0000000;
				ser1 = loveberyObfuscate(ser1);
				cardData[4] = ser1 >> 24;
				cardData[5] = ser1 >> 16;
				cardData[6] = ser1 >> 8;
				cardData[7] = ser1;
			}

			ser1 ^= 0x321c89d3;
			cardData[8] = ser1 >> 24;
			cardData[9] = ser1 >> 16;
			cardData[10] = ser1 >> 8;
			cardData[11] = ser1;
		}
	}

	std::string getCardDataPath()
	{
		std::string path = hostfs::getArcadeFlashPath();
		switch (config::Region)
		{
		case 0:
			path += "-jp";
			break;
		case 1:
			path += "-us";
			break;
		default:
			path += "-exp";
			break;
		}
		path += ".rfid" + std::to_string(index);

		return path;
	}

	void saveData()
	{
		FILE *f = nowide::fopen(getCardDataPath().c_str(), "wb");
		if (f == nullptr) {
			WARN_LOG(NAOMI, "Can't save RFID card: error %x", errno);
			return;
		}
		fwrite(cardData.data(), 1, cardData.size(), f);
		fclose(f);
	}

	u32 dinoShuffle(u32 v) {
		return ((v & 1) << 22) | ((v & 2) << 28) | ((v & 4) << 1) | ((v & 8) << 3)
				| ((v & 0x10) << 21) | ((v & 0x20) << 10) | ((v & 0x40) << 1) | ((v & 0x80) >> 7)
				| ((v & 0x100) << 22) | ((v & 0x200) >> 1) | ((v & 0x400) >> 1) | ((v & 0x800) << 17)
				| ((v & 0x1000) << 4) | ((v & 0x2000) << 18) | ((v & 0x4000) >> 4) | ((v & 0x8000) >> 13)
				| ((v & 0x10000) >> 15) | ((v & 0x20000) >> 4) | ((v & 0x40000) << 1) | ((v & 0x80000) >> 8)
				| ((v & 0x100000) >> 6) | ((v & 0x200000) << 2) | ((v & 0x400000) >> 18) | ((v & 0x800000) >> 2)
				| ((v & 0x1000000) >> 12) | ((v & 0x2000000) >> 7) | ((v & 0x4000000) << 1) | ((v & 0x8000000) >> 3)
				| ((v & 0x10000000) >> 11) | ((v & 0x20000000) >> 9) | ((v & 0x40000000) >> 25) | ((v & 0x80000000) >> 5);
	}

	u32 dinoUnshuffle(u32 v) {
		  return ((v & 1) << 7) | ((v & 2) << 15) | ((v & 4) << 13) | ((v & 8) >> 1)
		         | ((v & 0x10) << 18) | ((v & 0x20) << 25) | ((v & 0x40) >> 3) | ((v & 0x80) >> 1)
				 | ((v & 0x100) << 1) | ((v & 0x200) << 1) | ((v & 0x400) << 4) | ((v & 0x800) << 8)
				 | ((v & 0x1000) << 12) | ((v & 0x2000) << 4) | ((v & 0x4000) << 6) | ((v & 0x8000) >> 10)
				 | ((v & 0x10000) >> 4) | ((v & 0x20000) << 11) | ((v & 0x40000) << 7) | ((v & 0x80000) >> 1)
				 | ((v & 0x100000) << 9) | ((v & 0x200000) << 2) | ((v & 0x400000) >> 22) | ((v & 0x800000) >> 2)
				 | ((v & 0x1000000) << 3) | ((v & 0x2000000) >> 21) | ((v & 0x4000000) << 5) | ((v & 0x8000000) >> 1)
				 | ((v & 0x10000000) >> 17) | ((v & 0x20000000) >> 28) | ((v & 0x40000000) >> 22) | ((v & 0x80000000) >> 18);
	}

	u32 loveberyDeobfuscate(u32 v)
	{
		v ^= 0xa35ec5e3;
		// 87654321 -> 34781256
		return ((v & 0xf00000) >> 20) | ((v & 0xf0000) >> 12)
				| ((v & 0xf0) << 4) | ((v & 0xf) << 12)
				| ((v & 0xf0000000) >> 12) | ((v & 0xf000000) >> 4)
				| ((v & 0xf000) << 12) | ((v & 0xf00) << 20);
	}
	u32 loveberyObfuscate(u32 v)
	{
		v = ((v & 0xf) << 20) | ((v & 0xf0) << 12)
				| ((v & 0xf00) >> 4) | ((v & 0xf000) >> 12)
				| ((v & 0xf0000) << 12) | ((v & 0xf00000) << 4)
				| ((v & 0xf000000) >> 12) | ((v & 0xf0000000) >> 20);
		return v ^ 0xa35ec5e3;
	}

	SerialPort *port;
	const int index;
	const std::string gameName;
	std::deque<u8> toSend;
	std::array<u8, 128> cardData;
	u8 expectedBytes = 0;
	std::vector<u8> recvBuffer;
	enum State {
		Off,
		Connecting,
		Connected,
		FullRead
	} state = Off;
	int rowCounter = 0;
};

//
// Isshoni Wanwan Waiwai Puppy touchscreen
//
class Touchscreen : public SerialPort::Pipe
{
public:
	Touchscreen(SerialPort *port) : port(port)
	{
		port->setPipe(this);
		schedId = sh4_sched_register(0, schedCallback, this);
	}

	~Touchscreen()
	{
		sh4_sched_unregister(schedId);
	}

	void write(u8 v) override
	{
		if (v == '\r')
		{
			if (recvBuffer.size() >= 2 && recvBuffer[0] == 1)
			{
				toSend.push_back(1);
				if (recvBuffer.size() == 3 && recvBuffer[1] == 'O' && recvBuffer[2] == 'I')
				{
					SERIAL_LOG("Received cmd OI: get name");
					toSend.push_back('A');
					toSend.push_back('3');
					toSend.push_back('0');
					toSend.push_back('9');
					toSend.push_back('9');
					toSend.push_back('9');
				}
				else if (recvBuffer.size() == 3 && recvBuffer[1] == 'N' && recvBuffer[2] == 'M')
				{
					SERIAL_LOG("Received cmd NM: unit verify");
					const std::array<u8, 19> id { 'E','X','I','I','-','7','7','2','0','S','C',' ','R','e','v',' ','3','.','0' };
					toSend.insert(toSend.end(), id.begin(), id.end());
				}
				else if (recvBuffer.size() == 3 && recvBuffer[1] == 'U' && recvBuffer[2] == 'V')
				{
					SERIAL_LOG("Received cmd UV: reset");
					const std::array<u8, 8> resp { 'Q','M','V','*','*','*','0','0' };
					toSend.insert(toSend.end(), resp.begin(), resp.end());
				}
				else if (recvBuffer.size() == 2 && recvBuffer[1] == 'R')
				{
					SERIAL_LOG("Received cmd R");
					toSend.push_back('0');
					sh4_sched_request(schedId, SCHED_CYCLES);
				}
				else
				{
					SERIAL_LOG("Received cmd %c", recvBuffer[1]);
					toSend.push_back('0');
				}
				toSend.push_back('\r');
				port->updateStatus();

				// FIXME
				if (recvBuffer.size() == 2 && recvBuffer[1] == 'Z')
					sendPosition(0);
			}
			else
			{
				WARN_LOG(NAOMI, "\\r ignored. buf size %d", (int)recvBuffer.size());
			}
			recvBuffer.clear();
		}
		else
		{
			if (recvBuffer.size() == 9)
			{
				if (!memcmp(&recvBuffer[0], "Ua0000000", 9))
				{
					SERIAL_LOG("UART receive Ua...%c", v);
					sendPosition(1);
				}
				else
					WARN_LOG(NAOMI, "Unknown command %.9s", &recvBuffer[0]);
				recvBuffer.clear();
			}
			else
			{
				recvBuffer.push_back(v);
			}
		}
	}

	int available() override {
		return toSend.size();
	}

	u8 read() override
	{
		u8 data = 0;
		if (!toSend.empty())
		{
			data = toSend.front();
			toSend.pop_front();
		}
		if (toSend.empty())
			port->updateStatus();
		SERIAL_LOG("UART read data %x", data);
		return data;
	}

	void serialize(Serializer& ser) const override
	{
		ser << (u32)toSend.size();
		for (u8 b : toSend)
			ser << b;
		ser << (u32)recvBuffer.size();
		ser.serialize(recvBuffer.data(), recvBuffer.size());
	}

	void deserialize(Deserializer& deser) override
	{
		u32 size;
		deser >> size;
		toSend.resize(size);
		for (u32 i = 0; i < size; i++)
			deser >> toSend[i];
		deser >> size;
		recvBuffer.resize(size);
		deser.deserialize(recvBuffer.data(), recvBuffer.size());
	}

private:
	void sendPosition(int type)
	{
		MapleInputState input[4];
		ggpo::getInput(input);
		// 0-1023 ?
		const u32 x = (640 - input[0].absPos.x) * 1023 / 639;
		const u32 y = input[0].absPos.y * 1023 / 479;

		size_t start = toSend.size();
		if (type == 1)
		{
			toSend.push_back('U');
			toSend.push_back('T');
			toSend.push_back(0x20);	// bit 0 and 1 are checked
			toSend.push_back(x & 0xff);
			toSend.push_back((x >> 8) & 0x1f);
			toSend.push_back(y & 0xff);
			toSend.push_back((y >> 8) & 0x1f);
			toSend.push_back(0);	// z pos
			u8 crc = 0xaa;
			for (; start < toSend.size(); start++)
				crc += toSend[start];
			toSend.push_back(crc);
			port->updateStatus();
		}
		else
		{
			bool button = (input[0].kcode & DC_BTN_A) == 0;
			if (button != lastButton || x != lastPosX || y != lastPosY)
			{
				// bit 6 is touch down
				if (button)
					toSend.push_back(0xc0);
				else
					toSend.push_back(0x80);
				toSend.push_back((x & 7) << 4);
				toSend.push_back((x >> 3) & 0x7f);
				toSend.push_back((y & 7) << 4);
				toSend.push_back((y >> 3) & 0x7f);
				lastButton = button;
				lastPosX = x;
				lastPosY = y;
				port->updateStatus();
			}
		}
	}

	static int schedCallback(int tag, int cycles, int jitter, void *p)
	{
		((Touchscreen *)p)->sendPosition(0);
		return SCHED_CYCLES;
	}

	SerialPort *port;
	std::deque<u8> toSend;
	std::vector<u8> recvBuffer;
	int schedId = 0;
	u32 lastPosX = ~0;
	u32 lastPosY = ~0;
	bool lastButton = false;

	static constexpr u32 SCHED_CYCLES = SH4_MAIN_CLOCK / 60;
};

u8 SerialPort::readReg(u32 addr)
{
	switch ((addr & 0x3f) / 4)
	{
	case 0: // data in
		if (pipe != nullptr)
			return pipe->read();
		else
			return 0;
	case 1: // out buffer len
		//SERIAL_LOG("UART%d read out buf len", index);
		return 0;
	case 2: // in buffer len
		//SERIAL_LOG("UART%d read in buf len %d", index, (int)toSend.size());
		if (pipe != nullptr)
			return pipe->available();
		else
			return 0;
	case 3: // errors?
		SERIAL_LOG("UART%d read errors", index);
		return 0;
	case 4: // unknown
		SERIAL_LOG("UART%d read reg4", index);
		return 0;
	case 5: // flow control
		SERIAL_LOG("UART%d read flow control", index);
		return 0;
	case 6: // status. bit 3: receive buffer not empty
		SERIAL_LOG("UART%d read status", index);
		if (pipe != nullptr && pipe->available() > 0)
			return 8;
		else
			return 0;
	case 7: // interrupt status/mask?
		SERIAL_LOG("UART%d read interrupt mask/status?", index);
		return 0;
	case 8: // unknown
		SERIAL_LOG("UART%d read reg8", index);
		return 0;
	case 9: // control?
		SERIAL_LOG("UART%d read control?", index);
		return 0;
	case 10: // baudrate (lsb)
		SERIAL_LOG("UART%d read baudrate(lsb)", index);
		return 0;
	case 11: // baudrate (msb)
		SERIAL_LOG("UART%d read baudrate(msb)", index);
		return 0;
	default:
		INFO_LOG(NAOMI, "Unknown UART%d port %x\n", index, addr);
		return 0;
	}
}

void SerialPort::writeReg(u32 addr, u8 v)
{
	switch ((addr & 0x3f) / 4)
	{
	case 0: // data out
		if (pipe != nullptr)
			pipe->write(v);
		else
			INFO_LOG(NAOMI, "UART%d out: %02x %c", index, v, v);
		break;

	case 1: // out buffer len
		SERIAL_LOG("UART%d write out buffer len: %x", index, v);
		break;

	case 2: // in buffer len
		SERIAL_LOG("UART%d write in buffer len: %x", index, v);
		break;

	case 3: // errors?
		SERIAL_LOG("UART%d write errors: %x", index, v);
		break;

	case 4: // unknown
		SERIAL_LOG("UART%d write reg4: %x", index, v);
		break;

	case 5: // flow control
		SERIAL_LOG("UART%d write flow control: %x", index, v);
		break;

	case 6: // status. bit 3: receive buffer not empty
		SERIAL_LOG("UART%d write status: %x", index, v);
		break;

	case 7: // interrupt status/mask?
		SERIAL_LOG("UART%d write interrupt status/mask?: %x", index, v);
		break;

	case 8: // unknown
		SERIAL_LOG("UART%d write reg8: %x", index, v);
		break;

	case 9: // control?
		SERIAL_LOG("UART%d write control: %x", index, v);
		break;

	case 10: // baudrate (lsb)
		SERIAL_LOG("UART%d write baudrate(lsb): %x", index, v);
		flush();
		break;

	case 11: // baudrate (msb)
		SERIAL_LOG("UART%d write baudrate(msb): %x", index, v);
		flush();
		break;

	default:
		INFO_LOG(NAOMI, "Unknown UART%d port %x\n", index, addr);
		break;
	}
}

void SerialPort::updateStatus()
{
	cart->updateInterrupt(index == 1 ? SystemSpCart::INT_UART1 : SystemSpCart::INT_UART2);
}

class DefaultInPortManager : public InPortManager
{
public:
	// IN_PORT0
	u8 getCN9_17_24() override
	{
		MapleInputState mapleInputState[4];
		ggpo::getInput(mapleInputState);

		u8 v = 0xff;
		// 0: P1 start
		// 1: P2 start
		// 2: P1 right
		// 3: P2 right
		// 4: P1 left
		// 5: P2 left
		// 6: P1 up
		// 7: P2 up
		if (!(mapleInputState[0].kcode & DC_BTN_START))
			v &= ~0x01;
		if (!(mapleInputState[1].kcode & DC_BTN_START))
			v &= ~0x02;
		if (!(mapleInputState[0].kcode & DC_DPAD_RIGHT))
			v &= ~0x04;
		if (!(mapleInputState[1].kcode & DC_DPAD_RIGHT))
			v &= ~0x08;
		if (!(mapleInputState[0].kcode & DC_DPAD_LEFT))
			v &= ~0x10;
		if (!(mapleInputState[1].kcode & DC_DPAD_LEFT))
			v &= ~0x20;
		if (!(mapleInputState[0].kcode & DC_DPAD_UP))
			v &= ~0x40;
		if (!(mapleInputState[1].kcode & DC_DPAD_UP))
			v &= ~0x80;
		IO_LOG("systemsp::read IN_PORT0 %x", v);
		return v;
	}

	// IN_PORT3
	u8 getCN9_25_32() override
	{
		u8 v = 0xff;
		// 0: P1 down
		// 1: P2 down
		// 2: P1 button 1
		// 3: P2 button 1
		// 4: P1 button 2
		// 5: P2 button 2
		// 6: P1 button 3
		// 7: P2 button 3
		MapleInputState mapleInputState[4];
		ggpo::getInput(mapleInputState);
		if (!(mapleInputState[0].kcode & DC_DPAD_DOWN))
			v &= ~0x01;
		if (!(mapleInputState[1].kcode & DC_DPAD_DOWN))
			v &= ~0x02;
		if (!(mapleInputState[0].kcode & DC_BTN_A))
			v &= ~0x04;
		if (!(mapleInputState[1].kcode & DC_BTN_A))
			v &= ~0x08;
		if (!(mapleInputState[0].kcode & DC_BTN_B))
			v &= ~0x10;
		if (!(mapleInputState[1].kcode & DC_BTN_B))
			v &= ~0x20;
		if (!(mapleInputState[0].kcode & DC_BTN_C))
			v &= ~0x40;
		if (!(mapleInputState[1].kcode & DC_BTN_C))
			v &= ~0x80;
		IO_LOG("systemsp::read IN_PORT3 %x", v);
		return v;
	}

	u8 getCN9_33_40() override
	{
		IO_LOG("systemsp::read IN CN9 33-40");
		return 0xff;
	}

	// IN_PORT1
	u8 getCN9_41_48() override
	{
		u8 v = 0xff;
		// 0: P1 service
		// 2: P1 test
		// 4: P1 coin
		// 5: P2 coin
		MapleInputState mapleInputState[4];
		ggpo::getInput(mapleInputState);
		if (!(mapleInputState[0].kcode & DC_DPAD2_UP)) // service
			v &= ~0x01;
		if (!(mapleInputState[0].kcode & DC_DPAD2_DOWN)) // test
			v &= ~0x04;
		if (!(mapleInputState[0].kcode & DC_BTN_D)) // coin
			v &= ~0x10;
		if (!(mapleInputState[1].kcode & DC_BTN_D))
			v &= ~0x20;
		IO_LOG("systemsp::read IN_PORT1 %x", v);
		return v;
	}

	// IN_PORT4
	u8 getCN9_49_56() override
	{
		u8 v = 0;
		// FIXME these are outputs??
		// 0: P1 coin meter
		// 1: P2 coin meter
		MapleInputState mapleInputState[4];
		ggpo::getInput(mapleInputState);
		if (!(mapleInputState[0].kcode & DC_BTN_D)) // coin
			v |= 1;
		if (!(mapleInputState[1].kcode & DC_BTN_D))
			v |= 2;
		IO_LOG("systemsp::read IN_PORT4 %x", v);
		return v;
	}
};

class CardReaderInPortManager : public DefaultInPortManager
{
public:
	u8 getCN9_17_24() override
	{
		MapleInputState mapleInputState[4];
		ggpo::getInput(mapleInputState);
		for (size_t i = 0; i < 2; i++)
		{
			if ((mapleInputState[i].kcode & DC_BTN_INSERT_CARD) == 0
					&& (last_kcode[i] & DC_BTN_INSERT_CARD) != 0)
				card_reader::insertCard(i);
			last_kcode[i] = mapleInputState[i].kcode;
		}
		return DefaultInPortManager::getCN9_17_24();
	}

	u8 getCN9_33_40() override
	{
		IO_LOG("systemsp::read IN CN9 33-40");
		// dinosaur king, love & berry:
		// 0: P1 card set (not used)
		// 2: CD1 input ok (active low)
		// 4: CD1 card jam (active low)
		// 6: CD1 empty (active low)
		return 0xfb;
	}

private:
	u32 last_kcode[2] = {};
};

class IsshoniInPortManager : public CardReaderInPortManager
{
public:
	u8 getCN9_17_24() override {
		CardReaderInPortManager::getCN9_17_24();
		return 0xff;
	}

	u8 getCN9_25_32() override {
		return 0xff;
	}
};

template<typename T>
T readMemArea0(u32 addr)
{
	verify(SystemSpCart::Instance != nullptr);
	return SystemSpCart::Instance->readMemArea0<T>(addr);
}
template u32 readMemArea0<>(u32 addr);
template u16 readMemArea0<>(u32 addr);
template u8 readMemArea0<>(u32 addr);

template<typename T>
T SystemSpCart::readMemArea0(u32 addr)
{
	addr &= 0x1fffff;
	if (addr < 0x10000)
	{
		// banked access to ROM/NET board address space, mainly backup SRAM and ATA
		u32 offset = (addr & 0xffff) | ((bank & 0x3fff) << 16);
		if ((bank & 0x3f00) == 0x3900)
		{
			// SRAM
			FLASH_LOG("systemsp::read(%x) SRAM. offset %x", addr, offset);
			verify(!(bank & 0x4000));
			// 8-bit device on 16-bit bus
			if constexpr (sizeof(T) == 1)
			{
				if (offset & 1)
					return 0xff;
				else
					return nvmem::readFlash(offset / 2, 1);
			}
			else if constexpr (sizeof(T) == 2)
				return 0xff00 | nvmem::readFlash(offset / 2, 1);
			else
				return 0xff00ff00 | nvmem::readFlash(offset / 2, 1) | (nvmem::readFlash(offset / 2 + 1, 1) << 16);
		}
		else if ((bank & 0x3f00) == 0x3a00)
		{
			// CF IDE registers
			switch (addr & 0xffff)
			{
			case 0x00: // RD data
				if constexpr (sizeof(T) == 2)
				{
					addr &= ~1;
					T ret = readMemArea0<u8>(addr);
					ret |= readMemArea0<u8>(addr) << 8;
					if (bank & 0x4000)
						// decrypt
						ret = decrypt(ret);
					return ret;
				}
				else
				{
					u8 data = 0;
					if (ata.bufferIndex < SECTOR_SIZE)
					{
						data = ata.buffer[ata.bufferIndex++];
						if (ata.bufferIndex == SECTOR_SIZE)
						{
							if (ata.sectorCount > 1)
							{
								// read next sector
								ata.sectorCount--;
								ata.sectorNumber++;
								if (ata.sectorNumber == 0)
									ata.cylinder++;
								if (ata.cylinder == 0)
									ata.driveHead.head++;
								readSectors();
								updateInterrupt(INT_ATA);
							}
							else
							{
								// no more data
								ata.status.drq = 0;
							}
						}
					}
					ATA_LOG("systemsp::read(%x) CF ATA data %02x %c", addr, data, data >= 32 && data < 127 ? (char)data : ' ');
					return data;
				}
			case 0x04: // error
				ATA_LOG("systemsp::read(%x) CF ATA error", addr);
				return 0;
			case 0x08: // sector count
				ATA_LOG("systemsp::read(%x) CF ATA sector count %d", addr, ata.sectorCount);
				return ata.sectorCount;
			case 0x0c: // sector no
				ATA_LOG("systemsp::read(%x) CF ATA sector# %x", addr, ata.sectorNumber);
				return ata.sectorNumber;
			case 0x10: // cylinder (lsb)
				ATA_LOG("systemsp::read(%x) CF ATA cylinder(lsb) %x", addr, ata.cylinder & 0xff);
				return ata.cylinder & 0xff;
			case 0x14: // cylinder (msb)
				ATA_LOG("systemsp::read(%x) CF ATA cylinder(msb) %x", addr, ata.cylinder >> 8);
				return ata.cylinder >> 8;
			case 0x18: // select card/head
				ATA_LOG("systemsp::read(%x) CF ATA card/head %x", addr, ata.driveHead.full);
				return ata.driveHead.full;
			case 0x1c: // status
				{
					// BUSY RDY  DWF  DSC  DRQ  CORR   0  ERR
					ATA_LOG("systemsp::read(%x) CF ATA status %x", addr, ata.status.full);
					u8 status = ata.status.full;
					// TODO correct?
					ata.status.dsc = 0;
					return status;
				}
			default:
				INFO_LOG(NAOMI, "systemsp::read(%x) CF IDE unknown reg", addr);
				return -1;
			}
		}
		else if ((bank & 0x3f00) == 0x3b00)
		{
			// CF IDE AltStatus/Device Ctrl register
			if ((addr & 0xffff) == 0x18) {
				ATA_LOG("systemsp::read(%x) CF IDE AltStatus %x", addr, ata.status.full);
				return ata.status.full;
			}
			INFO_LOG(NAOMI, "systemsp::read(%x) CF IDE AltStatus unknown addr", addr);
			return 0;
		}
		else if ((bank & 0x3f00) == 0x3d00)
		{
			// Network aka Media board shared buffer/RAM
			verify(!(bank & 0x4000));
			DEBUG_LOG(NAOMI, "systemsp::read(%x) Network shared RAM. offset %x", addr, offset);
			return -1;
		}
		else if ((bank & 0x3f00) == 0x3f00)
		{
			// Network board present flag (0x01)
			DEBUG_LOG(NAOMI, "systemsp::read(%x) Network board present. offset %x", addr, offset);
			return 0;
		}
		T v;
		if (!CurrentCartridge->Read(offset, sizeof(T), &v))
			return -1;
		else
			return v;
	}
	else if (addr == 0x10000)
	{
		// bank register
		return (T)bank;
	}
	else if (addr < 0x10100)
	{
		// IRQ pending/reset, ATA control
		DEBUG_LOG(NAOMI, "systemsp::read(%x) IRQ pending/reset, ATA control", addr);
		switch (addr - 0x10000)
		{
		case 0x30:
			return 0;
		case 0x80:
			{
				// interrupt status
				const u8 intPending = ata.interruptPending;
				ata.interruptPending = 0;
				updateInterrupt();
				// b0: UART1
				// b1: UART2
				// b3: DIMM
				// b4: ATA controller
				return intPending;
			}
		case 0x84:
			// interrupt mask?
			// 10084: (dinoking) bit0,1 reset, sometimes set
			return ata.reg84;
		default:
			return 0;
		}
	}
	else if (addr < 0x10128)
	{
		// I/O chip for inputs
		switch (addr - 0x10100)
		{
		case 0x0: // IN_PORT0 (CN9 17-24)
			return inPortManager->getCN9_17_24();

		case 0x4: // IN_PORT1 (CN9 41-48)
			return inPortManager->getCN9_41_48();

		case 0x8: // IN_PORT3 (CN9 25-32)
			return inPortManager->getCN9_25_32();

		case 0xc: // IN CN9 33-40
			return inPortManager->getCN9_33_40();

		case 0x10: // IN_PORT4 (CN9 49-56)
			return inPortManager->getCN9_49_56();

		case 0x18: // IN_PORT2 (DIP switches and jumpers, and P1 service for older pcb rev)
			{
				// DIP switches
				// 0: unknown, active low
				// 1: unknown, active low
				// 2: monitor (1: 31 kHz, 0: 15 kHz)
				// 3: unknown, must be on (active low)
				// 4: JP5
				// 5: JP6
				// 6: JP7
				// 7: JP8
				IO_LOG("systemsp::read(%x) IN_PORT2 %x", addr, 7);
				return 0xf7;
			}
		case 0x20: // IN G_PORT CN10 9-16
			IO_LOG("systemsp::read(%x) IN CN10 9-16", addr);
 			// dinosaur king, love & berry:
 			// 0: 232c sel status 1 (not used)
 			// 1: 232c sel status 2 (not used)
 			// 2: card status1 (not used)
 			// 3: card status2 (not used)
 			// 4: card status3 (not used)
			// FIXME read sequentially after reading/writing reg24 (c0?), gives 4 shorts (8 reads)
			return 0;

		case 0x24: // bios, write too
			IO_LOG("systemsp::read(24) ??");
			return 0;
		default:
			IO_LOG("systemsp::read(%x) inputs??", addr);
			return 0;
		}
	}
	else if (addr == 0x10128)
	{
		// eeprom
		return eeprom.readDO() << 4;
	}
	else if (addr == 0x10150)
	{
		// CFG rom board debug flags
        // bit 0 - romboard type, 1 = M4
        // bit 1 - debug mode (enable easter eggs in BIOS, can boot game without proper eeproms/settings)
		IO_LOG("systemsp::read(%x) CFG DIP switches", addr);
		return 3; // M4 board type, debug on;
	}
	else if (addr < 0x10180) {
		// unknown
	}
	else if (addr < 0x101c0)
	{
		// custom UART 1
		return uart1.readReg(addr);
	}
	else if (addr < 0x101f0)
	{
		// custom UART 2
		return uart2.readReg(addr);
	}
	INFO_LOG(NAOMI, "systemsp::readMemArea0<%d>: Unknown addr %x", (int)sizeof(T), addr);
	return -1;
}

template<typename T>
void writeMemArea0(u32 addr, T v)
{
	verify(SystemSpCart::Instance != nullptr);
	SystemSpCart::Instance->writeMemArea0(addr, v);
}
template void writeMemArea0<>(u32 addr, u32 v);
template void writeMemArea0<>(u32 addr, u16 v);
template void writeMemArea0<>(u32 addr, u8 v);

template<typename T>
void SystemSpCart::writeMemArea0(u32 addr, T v)
{
	//DEBUG_LOG(NAOMI, "SystemSpCart::writeMemArea0(%x, %x)", addr, (u32)v);
	addr &= 0x1fffff;
	if (addr < 0x10000)
	{
		// banked access to ROM/NET board address space, mainly backup SRAM and ATA
		u32 offset = (addr & 0xffff) | ((bank & 0x3fff) << 16);
		if ((bank & 0x3f00) == 0x3900)
		{
			FLASH_LOG("systemsp::write(%x) SRAM. offset %x data %x", addr, offset, (u32)v);
			// 8-bit device on 16-bit bus
			if constexpr (sizeof(T) == 1) {
				if (offset & 1)
					return;
			}
			if constexpr (sizeof(T) == 4)
				nvmem::writeFlash(offset / 2 + 1, (u8)(v >> 16), 1);
			nvmem::writeFlash(offset / 2, (u8)v, 1);
			return;
		}
		else if ((bank & 0x3f00) == 0x3a00)
		{
			// CF IDE registers
			switch (addr & 0xffff)
			{
			case 0x00: // WR data
				ATA_LOG("systemsp::write(%x) CF ATA data = %x", addr, (u32)v);
				break;
			case 0x04: // features
				ATA_LOG("systemsp::write(%x) CF ATA features = %x", addr, (u32)v);
				ata.features = v;
				break;
			case 0x08: // sector count
				ATA_LOG("systemsp::write(%x) CF ATA sector count = %x", addr, (u32)v);
				ata.sectorCount = v;
				break;
			case 0x0c: // sector no
				ATA_LOG("systemsp::write(%x) CF ATA sector# = %x", addr, (u32)v);
				ata.sectorNumber = v;
				break;
			case 0x10: // cylinder (lsb)
				ATA_LOG("systemsp::write(%x) CF ATA cylinder(lsb) = %x", addr, (u32)v);
				ata.cylinder = (ata.cylinder & 0xff00) | (v & 0xff);
				break;
			case 0x14: // cylinder (msb)
				ATA_LOG("systemsp::write(%x) CF ATA cylinder(msb) = %x", addr, (u32)v);
				ata.cylinder = (ata.cylinder & 0xff) | ((v & 0xff) << 8);
				break;
			case 0x18: // select card/head
				ATA_LOG("systemsp::write(%x) CF ATA card/head = %x", addr, (u32)v);
				ata.driveHead.full = v | 0xa0;
				break;
			case 0x1c: // command
				switch (v)
				{
				case 0x20:
					ATA_LOG("systemsp::write(%x) CF ATA cmd: read %d sector(s): c %x h %x s %x", addr, ata.sectorCount, ata.cylinder, ata.driveHead.head, ata.sectorNumber);
					ata.status.rdy = 0;
					ata.status.bsy = 1;
					ata.status.drq = 1; // FIXME should be done in the callback
					sh4_sched_request(schedId, 2000); // 10 us
					readSectors();
					break;
				case 0xe1:
					ATA_LOG("systemsp::write(%x) CF ATA cmd: idle immediate", addr);
					ata.status.bsy = 1;
					ata.status.rdy = 0;
					sh4_sched_request(schedId, 2000); // 10 us
					break;
				default:
					INFO_LOG(NAOMI, "systemsp::write(%x) CF ATA command unknown: %x", addr, (u32)v);
					break;
				}
				break;
			default:
				INFO_LOG(NAOMI, "systemsp::write(%x) CF ATA unknown reg = %x", addr, (u32)v);
				break;
			}
			return;
		}
		else if ((bank & 0x3f00) == 0x3b00)
		{
			if ((addr & 0xffff) == 0x18)
			{
				// CF IDE AltStatus/Device Ctrl register
				if (ata.devCtrl.srst && !(v & 4))
				{
					// software reset
					ata.bufferIndex = ~0;
					ata.error = 0;
					ata.status.drq = 0;
					ata.status.err = 0;
				}
				ATA_LOG("systemsp::write(%x) CF IDE Device Ctrl = %x", addr, (u32)v);
				ata.devCtrl.full = v & 0x86;
			}
			else
				INFO_LOG(NAOMI, "systemsp::write(%x) CF IDE unknown reg %x data %x", addr, offset, (u32)v);
			return;
		}
		else if ((bank & 0x3f00) == 0x3d00) {
			// Network aka Media board shared buffer/RAM
			DEBUG_LOG(NAOMI, "systemsp::write(%x) Network shared RAM. offset %x data %x", addr, offset, (u32)v);
			return;
		}
		else if ((bank & 0x3f00) == 0x3f00) {
			// Network board present flag (0x01)
			DEBUG_LOG(NAOMI, "systemsp::write(%x) Network board present. offset %x data %x", addr, offset, (u32)v);
			return;
		}
	}
	else if (addr == 0x10000)
	{
		// bank register
		if (bank != (u16)v)
			DEBUG_LOG(NAOMI, "systemsp: G2 Bank set to %08X%s", (v & 0x3fff) << 16, (v & 0x4000) ? " decrypt ON" : "");
		bank = (u16)v;
		return;
	}
	else if (addr < 0x10100)
	{
		// IRQ pending/reset, ATA control
		DEBUG_LOG(NAOMI, "systemsp::write(%x) IRQ pending/reset, ATA control. data %x", addr, (u32)v);
		switch (addr - 10000)
		{
		case 0x84:
			ata.reg84 = v;
			break;
		default:
			break;
		}
		return;
	}
	else if (addr < 0x10128)
	{
		// I/O chip for outputs
		switch (addr - 0x10100)
		{
		case 0x8: // OUT_PORT3 (CN9 25-32)?
			IO_LOG("systemsp::write(%x) OUT CN9 25-32? %x", addr, v);
			break;
		case 0xc: // OUT CN9 33-40
			IO_LOG("systemsp::write(%x) OUT CN10 9-16? %x", addr, v);
			break;
		case 0x10: // OUT_PORT4 (CN9 49-56)
			// 0: (P1 coin meter)
			// 1: (P2 coin meter)
			// giant tetris:
			// 2: P1 start lamp
			// 3: P2 start lamp
			// 4: P1 button 1 lamp
			// 5: P2 button 1 lamp
			// 6: P1 button 2 lamp
			// 7: P2 button 2 lamp
			// dinosaur king, love & berry:
			// 2: coin blocker 1 (not used)
			// 3: coin blocker 2 (not used)
			// 4: cd1 card pickout
			// 5: cd2 card pickout (not used)
			// 6: cd1 jam reset
			// 7: cd2 jam reset (not used)
			IO_LOG("systemsp::write(%x) OUT_PORT4 %x", addr, v & 0xfc);
			break;
		case 0x14: // OUT CN10 17-24
			// dinosaur king, love & berry:
			// 0: 232c select1 (not used)
			// 1: 232c select2 (not used)
			// 2: 232c reset (not used)
			// 3: card trigger (not used)
			// 4: rfid chip1 reset
			// 5: rfid chip2 reset
			// 6: rfid chip1 empty lamp
			// 7: rfid chip2 empty lamp
			IO_LOG("systemsp::write(%x) OUT CN10 17-24 %x", addr, v);
			break;
		case 0x24: // read too
		default:
			IO_LOG("systemsp::write(%x) outputs? data %x", addr, (u32)v);
			break;
		}
		return;
	}
	else if (addr == 0x10128)
	{
		// eeprom
		eeprom.writeDI((v & 1) != 0);
		eeprom.writeCS((v & 2) != 0);
		eeprom.writeCLK((v & 4) != 0);
		return;
	}
	else if (addr < 0x10180)
	{
		// rom board dip switches?
		IO_LOG("systemsp::write(%x) DIP switches? data %x", addr, (u32)v);
		return;
	}
	else if (addr < 0x101c0)
	{
		// custom UART 1
		uart1.writeReg(addr, v);
		return;
	}
	else if (addr < 0x101f0)
	{
		// custom UART 2
		uart2.writeReg(addr, v);
		return;
	}
	INFO_LOG(NAOMI, "systemsp::writeMemArea0<%d>: Unknown addr %x = %x", (int)sizeof(T), addr, (int)v);
}

void SystemSpCart::updateInterrupt(u32 mask)
{
	ata.interruptPending |= mask;
	if ((ata.interruptPending & (INT_UART1 | INT_UART2 | INT_DIMM))
			|| ((ata.interruptPending & INT_ATA) && ata.devCtrl.nien == 0))
		asic_RaiseInterrupt(holly_EXP_PCI);
	else
		asic_CancelInterrupt(holly_EXP_PCI);
}

SystemSpCart::SystemSpCart(u32 size) : M4Cartridge(size), uart1(this, 1), uart2(this, 2)
{
	schedId = sh4_sched_register(0, schedCallback, this);
	Instance = this;
	// mb_serial.ic57
	static const u8 eepromData[0x80] = {
		0xf5, 0x90, 0x53, 0x45, 0x47, 0x41, 0x20, 0x45, 0x4e, 0x54, 0x45, 0x52,
		0x50, 0x52, 0x49, 0x53, 0x45, 0x53, 0x2c, 0x4c, 0x54, 0x44, 0x2e, 0x00,
		0x4e, 0x41, 0x4f, 0x4d, 0x49, 0x00, 0x00, 0x00, 0x41, 0x41, 0x46, 0x45,
		0x30, 0x31, 0x44, 0x31, 0x35, 0x39, 0x32, 0x34, 0x38, 0x31, 0x36, 0x00,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};
	eeprom.Load(eepromData, sizeof(eepromData));
}

SystemSpCart::~SystemSpCart()
{
	EventManager::unlisten(Event::Pause, handleEvent, this);
	if (chd != nullptr)
		chd_close(chd);
	if (chdFile != nullptr)
		fclose(chdFile);
	sh4_sched_unregister(schedId);
	Instance = nullptr;
}

chd_file *SystemSpCart::openChd(const std::string path)
{
	chdFile = hostfs::storage().openFile(path, "rb");
	if (chdFile == nullptr)
	{
		WARN_LOG(NAOMI, "Cannot open file '%s' errno %d", path.c_str(), errno);
		return nullptr;
	}
	chd_file *chd;
	chd_error err = chd_open_file(chdFile, CHD_OPEN_READ, 0, &chd);

	if (err != CHDERR_NONE)
	{
		WARN_LOG(NAOMI, "Invalid CHD file %s", path.c_str());
		fclose(chdFile);
		chdFile = nullptr;
		return nullptr;
	}
	INFO_LOG(NAOMI, "compact flash: parsing file %s", path.c_str());

	const chd_header* head = chd_get_header(chd);

	hunkbytes = head->hunkbytes;
	hunkmem = std::make_unique<u8[]>(hunkbytes);

	return chd;
}

void SystemSpCart::readSectors()
{
	verify(ata.driveHead.lba == 1);
	u32 lba = (ata.driveHead.head << 24) | (ata.cylinder << 8) | ata.sectorNumber;
	u32 newHunk = lba * SECTOR_SIZE / hunkbytes;
	u32 offset = (lba * SECTOR_SIZE) % hunkbytes;
	if (hunknum != newHunk)
	{
		hunknum = newHunk;
		if (chd_read(chd, hunknum, &hunkmem[0]) != CHDERR_NONE)
			WARN_LOG(NAOMI, "CHD read failed");
	}
	memcpy(ata.buffer, &hunkmem[offset], SECTOR_SIZE);
	ata.bufferIndex = 0;
}

std::string SystemSpCart::getEepromPath() const
{
	std::string path = hostfs::getArcadeFlashPath();
	switch (region)
	{
	case 0:
		path += "-jp";
		break;
	case 1:
		path += "-us";
		break;
	default:
		path += "-exp";
		break;
	}
	path += ".eeprom";
	return path;
}

class BootIdLoader
{
	static constexpr u32 SECSIZE = 512;

public:
	BootIdLoader(SystemSpCart& cart) : cart(cart)
	{
		const chd_header* head = chd_get_header(cart.chd);

		hunkbytes = head->hunkbytes;
		hunkmem = std::make_unique<u8[]>(hunkbytes);
	}

	RomBootID *load()
	{
		// Read master boot record
		if (chd_read(cart.chd, 0, &hunkmem[0]) != CHDERR_NONE)
			return nullptr;
		// Check boot signature
		if (hunkmem[510] != 0x55 || hunkmem[511] != 0xaa)
			return nullptr;
		// Get partition 1 info
		if (hunkmem[446 + 4] != 6)
			// Not a FAT16B partition
			return nullptr;
		u32 start = *(u32 *)&hunkmem[446 + 8];

		// Read partition Bios Parameter Block
		if (chd_read(cart.chd, start * SECSIZE / hunkbytes, &hunkmem[0]) != CHDERR_NONE)
			return nullptr;
		int offset = (start * SECSIZE) % hunkbytes;
		// Partition attributes
		sectorsPerCluster = hunkmem[offset + 13];
		u16 resSectors = *(u16 *)&hunkmem[offset + 14];
		u8 fatCount = hunkmem[offset + 16];
		rootFolderSize = *(u16 *)&hunkmem[offset + 17]; // in entries
		u16 fatSize = *(u16 *)&hunkmem[offset + 22]; // in sectors
		// FATs start after reserved sectors
		fatOffset = start + resSectors;
		rootOffset = fatOffset + fatSize * fatCount;
		dataStartOffset = rootOffset + rootFolderSize * sizeof(DirEntry) / SECSIZE;

		if (!openFile("1STREAD.BIN"))
			return nullptr;
		std::unique_ptr<u8[]> buffer = std::make_unique<u8[]>(file.size);
		cart.enc_reset();
		for (u32 i = 0; i < file.size; i += 2)
			*(u16 *)&buffer[i] = cart.decrypt(*(u16 *)&hunkmem[file.offset + i]);
		if (memcmp(&buffer[0], "SPCF", 4))
			// wrong signature
			return nullptr;
		std::string imageName((const char *)&buffer[0x80]);
		if (imageName.length() > 12)
			// make short 8.3 name
			imageName = imageName.substr(0, 6) + "~1" + imageName.substr(imageName.find('.'));
		string_toupper(imageName);

		if (!openFile(imageName))
			return nullptr;
		buffer = std::make_unique<u8[]>(sizeof(RomBootID));
		cart.enc_reset();
		for (size_t i = 0; i < sizeof(RomBootID); i += 2)
		{
			if (file.offset + i >= hunkbytes)
			{
				u32 fileOffset = dataStartOffset + (file.cluster - 2) * sectorsPerCluster + i / SECSIZE;
				if (chd_read(cart.chd, fileOffset * SECSIZE / hunkbytes, &hunkmem[0]) != CHDERR_NONE)
					return nullptr;
				file.offset = (fileOffset * SECSIZE) % hunkbytes - i;
				file.size -= i;
			}
			*(u16 *)&buffer[i] = cart.decrypt(*(u16 *)&hunkmem[file.offset + i]);
		}
		if (memcmp(&buffer[0], "SystemSP", 8))
		{
			// Yes, it is encrypted twice (except tetgiano)
			cart.enc_reset();
			for (size_t i = 0; i < sizeof(RomBootID); i += 2)
				*(u16 *)&buffer[i] = cart.decrypt(*(u16 *)&buffer[i]);
		}
		RomBootID *bootId = new RomBootID();
		memcpy(bootId, &buffer[0], sizeof(RomBootID));

		return bootId;
	}

private:
	struct DirEntry
	{
		char name[8];
		char ext[3];
		enum Attribute : u8 {
			ReadOnly = 0x01,
			Hidden = 0x02,
			System = 0x04,
			VolumeName = 0x8,
			Directory = 0x10,
			Archive = 0x20,
			LongName = 0xf,
		};
		Attribute attributes;
		u8 _reserved;
		u8 creationTimeMs;
		u16 creationTime;
		u16 creationDate;
		u16 accessDate;
		u16 clusterHigh;
		u16 updateTime;
		u16 updateDate;
		u16 cluster;
		u32 size;
	};

	bool openFile(const std::string& fileName)
	{
		u32 curRootOffset = rootOffset;
		if (chd_read(cart.chd, curRootOffset * SECSIZE / hunkbytes, &hunkmem[0]) != CHDERR_NONE)
			return false;
		int offset = (curRootOffset * SECSIZE) % hunkbytes;
		for (int i = 0; i < rootFolderSize; i++)
		{
			if (offset + i * sizeof(DirEntry) >= hunkbytes)
			{
				++curRootOffset;
				if (chd_read(cart.chd, curRootOffset * SECSIZE / hunkbytes, &hunkmem[0]) != CHDERR_NONE)
					return false;
				offset = (curRootOffset * SECSIZE) % hunkbytes - i * sizeof(DirEntry);
			}
			const DirEntry *entry = (const DirEntry *)&hunkmem[offset + i * sizeof(DirEntry)];
			u8 b = (u8)entry->name[0];
			if (b == 0)
				// end of directory
				return false;
			if (b == 0xe5 || b == 0x2e)
				// unused or ./.. entry
				continue;
			if (entry->attributes == DirEntry::LongName || (entry->attributes & (DirEntry::VolumeName | DirEntry::System)))
				// long name or volume name/system
				continue;
			std::string name(std::begin(entry->name), std::end(entry->name));
			name = trim_trailing_ws(name);
			std::string ext(std::begin(entry->ext), std::end(entry->ext));
			ext = trim_trailing_ws(ext);
			if (!ext.empty())
				name += "." + ext;
			//printf("%s%c\t%d\n", name.c_str(), entry->attributes & DirEntry::Directory ? '/' : ' ', entry->size);
			if (name != fileName)
				continue;
			file.cluster = entry->cluster;
			file.size = entry->size;
			u32 fileOffset = dataStartOffset + (file.cluster - 2) * sectorsPerCluster;
			if (chd_read(cart.chd, fileOffset * SECSIZE / hunkbytes, &hunkmem[0]) != CHDERR_NONE)
				return false;
			file.offset = (fileOffset * SECSIZE) % hunkbytes;

			return true;
		}
		return false;
	}

	SystemSpCart& cart;
	u32 hunkbytes;
	std::unique_ptr<u8[]> hunkmem;
	u8 sectorsPerCluster = 0;
	u32 fatOffset = 0;
	u16 rootFolderSize = 0;
	u32 rootOffset = 0;
	u32 dataStartOffset = 0;
	struct {
		u32 cluster;
		u32 size;
		int offset;
	} file;
};

void SystemSpCart::Init(LoadProgress *progress, std::vector<u8> *digest)
{
	M4Cartridge::Init(progress, digest);

	if (mediaName != nullptr)
	{
		std::string parent = hostfs::storage().getParentPath(settings.content.path);
		std::string gdrom_path = get_file_basename(settings.content.fileName) + "/" + std::string(mediaName) + ".chd";
		gdrom_path = hostfs::storage().getSubPath(parent, gdrom_path);
		chd = openChd(gdrom_path);
		if (parentName != nullptr && chd == nullptr)
		{
			std::string gdrom_parent_path = hostfs::storage().getSubPath(parent, std::string(parentName) + "/" + std::string(mediaName) + ".chd");
			chd = openChd(gdrom_parent_path);
		}
		if (chd == nullptr)
			throw NaomiCartException("SystemSP: Cannot open CompactFlash file " + gdrom_path);

		BootIdLoader loader(*this);
		romBootId.reset(loader.load());
	}
	else
	{
		ata.status.rdy = 0;
		ata.status.df = 1;
	}

	RomBootID bootId;
	if (GetBootId(&bootId)
			&& bootId.country != 0 && (bootId.country & (2 << config::Region)) == 0)
	{
		if (bootId.country & 4)
		{
			NOTICE_LOG(NAOMI, "Forcing region USA");
			config::Region.override(1);
		}
		else if (bootId.country & 8)
		{
			NOTICE_LOG(NAOMI, "Forcing region Export");
			config::Region.override(2);
		}
		else if (bootId.country & 2)
		{
			NOTICE_LOG(NAOMI, "Forcing region Japan");
			config::Region.override(0);
		}
		// Force BIOS reload now to get the default eeprom for the correct region
		naomi_default_eeprom = nullptr;
		naomi_cart_LoadBios(settings.content.fileName.c_str());
	}
	region = config::Region;
	// Region must be set before loading the eeprom
	if (!eeprom.Load(getEepromPath()) && naomi_default_eeprom != nullptr)
		memcpy(eeprom.data, naomi_default_eeprom, 128);

	// dinoki4 doesn't use rfid chips. dinokich uses a different reader/writer protocol
	if ((!strncmp(game->name, "dinoki", 6) && strcmp(game->name, "dinoki4") != 0 && strcmp(game->name, "dinokich") != 0)
			|| !strncmp(game->name, "loveber", 7))
	{
		new RfidReaderWriter(&uart1, 1, game->name);
		new RfidReaderWriter(&uart2, 2, game->name);
	}
	else if (!strcmp(game->name, "isshoni"))
	{
		new Touchscreen(&uart1);
		inPortManager = std::make_unique<IsshoniInPortManager>();
	}
	if (!strncmp(game->name, "dinoki", 6) || !strncmp(game->name, "loveber", 7))
		inPortManager = std::make_unique<CardReaderInPortManager>();
	if (!inPortManager)
		inPortManager = std::make_unique<DefaultInPortManager>();

	EventManager::listen(Event::Pause, handleEvent, this);
}

u32 SystemSpCart::ReadMem(u32 address, u32 size)
{
	return M4Cartridge::ReadMem(address, size);
}

void SystemSpCart::WriteMem(u32 address, u32 data, u32 size)
{
	M4Cartridge::WriteMem(address, data, size);
}

bool SystemSpCart::Read(u32 offset, u32 size, void *dst)
{
	// TODO sram? if ((offset & 0x3f000000) == 0x39000000)

	if ((offset & 0x3f000000) == 0x3f000000)
	{
		// network card present
		DEBUG_LOG(NAOMI, "SystemSpCart::Read<%d>%x: net card present -> 0", size, offset);
		int rc = 0;
		memcpy(dst, &rc, size);
		return true;
	}
	return M4Cartridge::Read(offset, size, dst);
}

bool SystemSpCart::Write(u32 offset, u32 size, u32 data)
{
	switch (flash.cmdState)
	{
	case CmdState::INIT:
		if ((offset & 0xfff) == 0xaaa && data == 0xaa) {
			flash.cmdState = CmdState::AAA_AA_1;
			return true;
		}
		else if (offset == flash.progAddress && data == 0x29)
		{
			// write buffer to flash
			FLASH_LOG("Flash cmd PROGRAM BUF TO FLASH %x", offset);
			flash.progAddress = ~0;
			return true;
		}
		break;
	case CmdState::AAA_AA_1:
		if (((offset & 0xfff) == 0x555 || (offset & 0xfff) == 0x554) && data == 0x55) {
			flash.cmdState = CmdState::_555_55_1;
			return true;
		}
		FLASH_LOG("Unexpected command %x %x in state aaa_aa_1", offset, data);
		flash.cmdState = CmdState::INIT;
		break;
	case CmdState::_555_55_1:
		if ((offset & 0xfff) == 0xaaa)
		{
			if (data == 0xa0) {
				flash.cmdState = CmdState::PROGRAM;
				return true;
			}
			else if (data == 0x80) {
				flash.cmdState = CmdState::AAA_80;
				return true;
			}
		}
		else if (data == 0x25)
		{
			flash.cmdState = CmdState::WRITE_BUF_1;
			flash.progAddress = offset;
			return true;
		}
		FLASH_LOG("Unexpected command %x %x in state 555_55_1", offset, data);
		flash.cmdState = CmdState::INIT;
		break;
	case CmdState::PROGRAM:
		FLASH_LOG("Flash cmd PROGRAM %x %x", offset, data);
		*(u16 *)&RomPtr[offset & (RomSize - 1)] = data;
		flash.cmdState = CmdState::INIT;
		return true;
	case CmdState::WRITE_BUF_1:
		flash.wordCount = data + 1;
		flash.cmdState = CmdState::WRITE_BUF_2;
		FLASH_LOG("Flash cmd WRITE BUFFFER addr %x count %x", flash.progAddress, flash.wordCount);
		return true;
	case CmdState::WRITE_BUF_2:
		*(u16 *)&RomPtr[offset & (RomSize - 1)] = data;
		if (--flash.wordCount == 0)
			flash.cmdState = CmdState::INIT;
		return true;
	case CmdState::AAA_80:
		if ((offset & 0xfff) == 0xaaa && data == 0xaa) {
			flash.cmdState = CmdState::AAA_AA_2;
			return true;
		}
		INFO_LOG(NAOMI, "Unexpected command %x %x in state aaa_80", offset, data);
		flash.cmdState = CmdState::INIT;
		break;
	case CmdState::AAA_AA_2:
		if (((offset & 0xfff) == 0x555 || (offset & 0xfff) == 0x554) && data == 0x55) {
			flash.cmdState = CmdState::_555_55_2;
			return true;
		}
		INFO_LOG(NAOMI, "Unexpected command %x %x in state aaa_aa_2", offset, data);
		flash.cmdState = CmdState::INIT;
		break;
	case CmdState::_555_55_2:
		if ((offset & 0xfff) == 0xaaa && data == 0x10)
		{
			// Erase chip
			FLASH_LOG("Flash cmd CHIP ERASE");
			if ((offset & 0x1fffffff) < RomSize)
				memset(&RomPtr[offset & (0x1fffffff & ~(64_MB - 1))], 0xff, 64_MB);
			flash.cmdState = CmdState::INIT;
			return true;
		}
		else if (data == 0x30)
		{
			// Erase sector
			FLASH_LOG("Flash cmd SECTOR ERASE %x", offset);
			if ((offset & 0x1fffffff) < RomSize)
				memset(&RomPtr[offset & (RomSize - 1) & 0xffff0000], 0xff, 0x1000); // 64k sector size?
			flash.cmdState = CmdState::INIT;
			return true;
		}
		INFO_LOG(NAOMI, "Unexpected command %x %x in state aaa_aa_2", offset, data);
		flash.cmdState = CmdState::INIT;
		break;
	}
	FLASH_LOG("SystemSpCart::Write<%d>%x: %x", size, offset, data);

	return M4Cartridge::Write(offset, size, data);
}

int SystemSpCart::schedCallback(int tag, int sch_cycl, int jitter, void *arg)
{
	return ((SystemSpCart *)arg)->schedCallback();
}

int SystemSpCart::schedCallback()
{
	ata.status.rdy = 1;
	ata.status.bsy = 0;
	updateInterrupt(INT_ATA);

	return 0;
}

void SystemSpCart::Serialize(Serializer& ser) const
{
	M4Cartridge::Serialize(ser);
	sh4_sched_serialize(ser, schedId);
	uart1.serialize(ser);
	uart2.serialize(ser);
	eeprom.Serialize(ser);
	ser << bank;
	ser << ata.features;
	ser << ata.cylinder;
	ser << ata.sectorCount;
	ser << ata.sectorNumber;
	ser << ata.status.full;
	ser << ata.error;
	ser << ata.driveHead.full;
	ser << ata.devCtrl.full;
	ser << ata.interruptPending;
	ser << ata.reg84;
	ser << ata.buffer;
	ser << ata.bufferIndex;
	ser << flash.cmdState;
	ser << flash.progAddress;
	ser << flash.wordCount;
	if (mediaName != nullptr)
		ser.serialize(RomPtr, RomSize);
}

void SystemSpCart::Deserialize(Deserializer& deser)
{
	M4Cartridge::Deserialize(deser);
	sh4_sched_deserialize(deser, schedId);
	uart1.deserialize(deser);
	uart2.deserialize(deser);
	eeprom.Deserialize(deser);
	deser >> bank;
	deser >> ata.features;
	deser >> ata.cylinder;
	deser >> ata.sectorCount;
	deser >> ata.sectorNumber;
	deser >> ata.status.full;
	deser >> ata.error;
	deser >> ata.driveHead.full;
	deser >> ata.devCtrl.full;
	deser >> ata.interruptPending;
	deser >> ata.reg84;
	deser >> ata.buffer;
	deser >> ata.bufferIndex;
	deser >> flash.cmdState;
	deser >> flash.progAddress;
	deser >> flash.wordCount;
	if (mediaName != nullptr)
		deser.deserialize(RomPtr, RomSize);
}

void SystemSpCart::saveFiles()
{
	eeprom.Save(getEepromPath());
}

}
