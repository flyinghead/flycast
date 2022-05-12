/*
	Copyright 2022 flyinghead

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

/*
	Initial D card reader protocol (from my good friend Metallic)

>>>	SEND PKT: [START 02][LEN][CMD][0][0][0]{optional data}[STOP 03][CRC]
<<<	RECV ACK: [OK 06]
>>>	SEND REQ: [RQ 05]
<<<	RECV PKT: [START 02][LEN][CMD][RESULT1][RESULT2][RESULT3]{optional data}[STOP 03][CRC]
<<< RECV ERR: [15]
	RESULT1 (SENSORS): binary value SSTCCCCC
		S - Shutter state:
			0 - "ERROR"
			1 - "CLOSED"
			2 - "OPEN"
			3 - "NOW" (both open and close sensors)
		T - Stocker (card dispenser) state:
			0 - "NO" (empty)
			1 - "OK" (full)
		C - Card sensors:
			0     - "NO CARD"
			1     - "CARD EXIST (ENTER)" (card inserted in front of shutter)
			18    - "CARD EXIST" (card loaded inside of reader)
			other - "CARD EXIST (OTHER)"
	RESULT2: char 01235QRSTUV`
		Error ENUM "OK", "READ ERR", "WRITE ERR", "STOP ERR", "PRINT ERR", "READERR T1", "READERR T2", "READERR T3",
		 "READERR T12", "READERR T13", "READERR T23", "SHUT ERR"
	RESULT3 (CMD_STATE): char 02345
		State ENUM "OK", "DISABLE", "BUSY", "CARD WAIT", "NO CARD IN BOX"

	Protocol dumps and more:
	https://www.arcade-projects.com/threads/naomi-2-chihiro-triforce-card-reader-emulator-initial-d3-wmmt-mario-kart-f-zero-ax.814/
 */
#pragma once
#include "types.h"
#include "hw/sh4/modules/modules.h"

namespace card_reader {

class CardReader
{
protected:
	u8 calcCrc(u8 *data, u32 len)
	{
		u32 crc = 0;
		for (u32 i = 0; i < len; i++)
			crc ^= data[i];
		return crc;
	}

	bool loadCard(u8 *cardData, u32 len);
	void saveCard(const u8 *cardData, u32 len);
};

class InitialDCardReader : public CardReader, SerialPipe
{
public:
	void write(u8 b) override;
	u8 read() override;

	int available() override {
		return outBufferLen - outBufferIdx;
	}

	void insertCard();
	void init() {
		serial_setPipe(this);
	}

private:
	enum Commands
	{
		CARD_INIT			= 0x10,
		CARD_GET_CARD_STATE	= 0x20,
		CARD_IS_PRESENT		= 0x40, // cancel?
		CARD_LOAD_CARD		= 0xB0,
		CARD_CLEAN_CARD		= 0xA0,
		CARD_READ			= 0x33,
		CARD_WRITE			= 0x53,
		CARD_WRITE_INFO		= 0x7C,
		CARD_PRINT			= 0x78,
		CARD_7A				= 0x7A,
		CARD_7D				= 0x7D,
		CARD_DOOR			= 0xD0,
		CARD_EJECT			= 0x80,
		CARD_NEW            = 0xB0,
	};

	u8 getStatus1()
	{
		return ((doorOpen ? 2 : 1) << 6) | 0x20 | (cardInserted ? 0x18 : 0);
	}

	void handleCommand()
	{
		if (rxCommandLen == 0)
			return;
		outBuffer[outBufferLen++] = 2;
		u32 crcIdx = outBufferLen;
		u8 len = 6;
		u8 status1 = getStatus1();
		u8 status2 = '0';
		u8 status3 = '0';
		switch (rxCommand[0])
		{
		case CARD_DOOR:
			doorOpen = rxCommand[4] == '1';
			status1 = getStatus1();
			break;

		case CARD_NEW:
			cardInserted = true;
			doorOpen = false;
			status1 = getStatus1();
			break;

		case CARD_WRITE:
			memcpy(cardData, &rxCommand[7], sizeof(cardData));
			saveCard(cardData, sizeof(cardData));
			break;

		case CARD_READ:
			if (cardInserted && !doorOpen)
				len = 6 + sizeof(cardData);
			else
				status3 = cardInserted ? '0' : '4';
			break;

		case CARD_EJECT:
			cardInserted = false;
			status1 = getStatus1();
			break;

		case CARD_IS_PRESENT:
		case CARD_GET_CARD_STATE:
		case CARD_INIT:
		case CARD_7A:
		case CARD_PRINT:
		case CARD_WRITE_INFO:
		case CARD_CLEAN_CARD:
			break;

		default:
			WARN_LOG(NAOMI, "Unknown command %x", rxCommand[0]);
			break;
		}
		outBuffer[outBufferLen++] = len;
		outBuffer[outBufferLen++] = rxCommand[0];
		outBuffer[outBufferLen++] = status1;
		outBuffer[outBufferLen++] = status2;
		outBuffer[outBufferLen++] = status3;
		if (rxCommand[0] == CARD_READ && cardInserted && !doorOpen)
		{
			memcpy(&outBuffer[outBufferLen], cardData, sizeof(cardData));
			outBufferLen += sizeof(cardData);
		}
		outBuffer[outBufferLen++] = 3;
		outBuffer[outBufferLen] = calcCrc(&outBuffer[crcIdx], outBufferLen - crcIdx);
		outBufferLen++;
	}

	u8 inBuffer[256];
	u32 inBufferIdx;

	u8 rxCommand[256];
	u32 rxCommandLen;

	u8 outBuffer[256];
	u32 outBufferIdx;
	u32 outBufferLen;

	u8 cardData[207];
	bool doorOpen = false;
	bool cardInserted = false;
};
extern InitialDCardReader initialDCardReader;

inline static void insertCard() {
	initialDCardReader.insertCard();
}

}
