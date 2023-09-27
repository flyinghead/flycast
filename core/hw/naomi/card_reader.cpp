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
#include "card_reader.h"
#include "oslib/oslib.h"
#include "hw/sh4/modules/modules.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_devs.h"
#include <deque>
#include <memory>
#include <errno.h>

namespace card_reader {

/*
	Sanwa CRP-1231BR-10 card reader/writer protocol (from my good friend Metallic)
	used in InitialD and Derby Owners Club

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

	Bits from YACardEmu: https://github.com/GXTX/YACardEmu/
	Copyright (C) 2020-2023 wutno (https://github.com/GXTX)
    Copyright (C) 2022-2023 tugpoat (https://github.com/tugpoat)
*/

class SanwaCRP1231BR : public SerialPort::Pipe
{
public:
	void write(u8 b) override
	{
		if (inBufferIdx == 0 && b == 5)
		{
			DEBUG_LOG(NAOMI, "Received RQ(5)");
			handleCommand();
			return;
		}
		inBuffer[inBufferIdx++] = b;
		if (inBufferIdx >= 3)
		{
			if (inBuffer[0] != 2)
			{
				INFO_LOG(NAOMI, "Unexpected cmd start byte %x", inBuffer[0]);
				inBufferIdx = 0;
				return;
			}
			u32 len = inBuffer[1];
			if (inBufferIdx < len + 2)
			{
				if (inBufferIdx == sizeof(inBuffer))
				{
					WARN_LOG(NAOMI, "Card reader buffer overflow");
					inBufferIdx = 0;
				}
				return;
			}
			u32 crc = calcCrc(&inBuffer[1], inBufferIdx - 2);
			if (crc != inBuffer[inBufferIdx - 1])
			{
				INFO_LOG(NAOMI, "Wrong crc: expected %x got %x", crc, inBuffer[inBufferIdx - 1]);
				inBufferIdx = 0;
				return;
			}
			DEBUG_LOG(NAOMI, "Received cmd %x len %d", inBuffer[2], inBuffer[1]);
			outBuffer[outBufferLen++] = 6;	// ACK
			rxCommandLen = inBufferIdx - 3;
			memcpy(rxCommand, inBuffer + 2, rxCommandLen);
			inBufferIdx = 0;
		}
	}

	u8 read() override
	{
		verify(outBufferIdx < outBufferLen);
		u8 b = outBuffer[outBufferIdx++];
		DEBUG_LOG(NAOMI, "Sending %x", b);
		if (outBufferIdx == outBufferLen)
			outBufferIdx = outBufferLen = 0;
		return b;
	}

	int available() override {
		return outBufferLen - outBufferIdx;
	}

	void insertCard()
	{
		cardInserted = loadCard(cardData, sizeof(cardData));
		if (cardInserted)
			INFO_LOG(NAOMI, "Card inserted");
	}

protected:
	enum Commands
	{
		CARD_INIT			= 0x10,
		CARD_GET_CARD_STATE	= 0x20,
		CARD_CANCEL			= 0x40,
		CARD_LOAD_CARD		= 0xB0,
		CARD_CLEAN_CARD		= 0xA0,
		CARD_READ			= 0x33,
		CARD_WRITE			= 0x53,
		CARD_PRINT			= 0x7C,
		CARD_PRINT_SETTINGS	= 0x78,
		CARD_REGISTER_FONT	= 0x7A,
		CARD_ERASE_PRINT	= 0x7D,
		CARD_DOOR			= 0xD0,
		CARD_EJECT			= 0x80,
		CARD_NEW            = 0xB0,
	};

	virtual u8 getStatus1()
	{
		return ((doorOpen ? 2 : 1) << 6) | 0x20 | (cardInserted ? 0x18 : 0);
	}

	void handleCommand()
	{
		if (rxCommandLen == 0)
			return;
		outBuffer[outBufferLen++] = 2;
		u32 crcIdx = outBufferLen;
		u8 status1 = getStatus1();
		u8 status2 = '0';
		u8 status3 = '0';
		switch (rxCommand[0])
		{
		case CARD_DOOR:
			doorOpen = rxCommand[4] == '1';
			INFO_LOG(NAOMI, "Door %s", doorOpen ? "open" : "closed");
			status1 = getStatus1();
			break;

		case CARD_NEW:
			INFO_LOG(NAOMI, "New card");
			cardInserted = true;
			doorOpen = false;
			status1 = getStatus1();
			break;

		case CARD_WRITE:
			// 4: mode ('0': read 0x45 bytes, '1': variable length write 1-47 bytes)
			// 5: parity ('0': 7-bit parity, '1': 8-bit no parity)
			// 6: track (see below)
			INFO_LOG(NAOMI, "Card write mode %c parity %c track %c", rxCommand[4], rxCommand[5], rxCommand[6]);
			switch (rxCommand[6])
			{
			case '0': // track 1
				memcpy(cardData, &rxCommand[7], TRACK_SIZE);
				break;
			case '1': // track 2
				memcpy(cardData + TRACK_SIZE, &rxCommand[7], TRACK_SIZE);
				break;
			case '2': // track 3
				memcpy(cardData + TRACK_SIZE * 2, &rxCommand[7], TRACK_SIZE);
				break;
			case '3': // track 1 & 2
				memcpy(cardData, &rxCommand[7], TRACK_SIZE * 2);
				break;
			case '4': // track 1 & 3
				memcpy(cardData, &rxCommand[7], TRACK_SIZE);
				memcpy(cardData + TRACK_SIZE * 2, &rxCommand[7 + TRACK_SIZE], TRACK_SIZE);
				break;
			case '5': // track 2 & 3
				memcpy(cardData + TRACK_SIZE, &rxCommand[7], TRACK_SIZE * 2);
				break;
			case '6': // track 1 2 & 3
				memcpy(cardData, &rxCommand[7], TRACK_SIZE * 3);
				break;
			default:
				WARN_LOG(NAOMI, "Unknown track# %02x", rxCommand[6]);
				break;
			}
			saveCard(cardData, sizeof(cardData));
			break;

		case CARD_READ:
			// 4: mode ('0': read 0x45 bytes, '1': variable length read 1-47 bytes, '2': card capture, pull in card?)
			// 5: parity ('0': 7-bit parity, '1': 8-bit no parity)
			// 6: track (see below)
			INFO_LOG(NAOMI, "Card read mode %c parity %c track %c", rxCommand[4], rxCommand[5], rxCommand[6]);
			if (!cardInserted || doorOpen)
				status3 = cardInserted ? '0' : '4';
			break;

		case CARD_EJECT:
			INFO_LOG(NAOMI, "Card ejected");
			cardInserted = false;
			status1 = getStatus1();
			break;

		case CARD_CANCEL:
		case CARD_GET_CARD_STATE:
		case CARD_INIT:
		case CARD_REGISTER_FONT:
		case CARD_PRINT_SETTINGS:
		case CARD_PRINT:
		case CARD_CLEAN_CARD:
			break;

		default:
			WARN_LOG(NAOMI, "Unknown command %x", rxCommand[0]);
			break;
		}
		outBuffer[outBufferLen++] = 6;
		outBuffer[outBufferLen++] = rxCommand[0];
		outBuffer[outBufferLen++] = status1;
		outBuffer[outBufferLen++] = status2;
		outBuffer[outBufferLen++] = status3;
		if (rxCommand[0] == CARD_READ && cardInserted && !doorOpen && rxCommand[4] != '2')
		{
			u32 idx = 0;
			u32 size = TRACK_SIZE;
			switch (rxCommand[6])
			{
			case '0': // track 1
				break;
			case '1': // track 2
				idx = TRACK_SIZE;
				break;
			case '2': // track 3
				idx = TRACK_SIZE * 2;
				break;
			case '3': // track 1 & 2
				size = TRACK_SIZE * 2;
				break;
			case '4': // track 1 & 3
				memcpy(&outBuffer[outBufferLen], cardData, TRACK_SIZE);
				outBufferLen += TRACK_SIZE;
				outBuffer[crcIdx] += size;
				idx = TRACK_SIZE * 2;
				break;
			case '5': // track 2 & 3
				idx = TRACK_SIZE;
				size = TRACK_SIZE * 2;
				break;
			case '6': // track 1 2 & 3
				size = TRACK_SIZE * 3;
				break;
			default:
				WARN_LOG(NAOMI, "Unknown track# %02x", rxCommand[6]);
				size = 0;
				break;
			}
			memcpy(&outBuffer[outBufferLen], cardData + idx, size);
			outBufferLen += size;
			outBuffer[crcIdx] += size;
		}
		outBuffer[outBufferLen++] = 3;
		outBuffer[outBufferLen] = calcCrc(&outBuffer[crcIdx], outBufferLen - crcIdx);
		outBufferLen++;
	}

	u8 calcCrc(u8 *data, u32 len)
	{
		u32 crc = 0;
		for (u32 i = 0; i < len; i++)
			crc ^= data[i];
		return crc;
	}

	bool loadCard(u8 *cardData, u32 len)
	{
		std::string path = hostfs::getArcadeFlashPath() + ".card";
		FILE *fp = nowide::fopen(path.c_str(), "rb");
		if (fp == nullptr)
			return false;

		DEBUG_LOG(NAOMI, "Loading card file from %s", path.c_str());
		if (fread(cardData, 1, len, fp) != len)
			WARN_LOG(NAOMI, "Truncated or empty card file: %s" ,path.c_str());
		fclose(fp);

		return true;
	}

	void saveCard(const u8 *cardData, u32 len)
	{
		std::string path = hostfs::getArcadeFlashPath() + ".card";
		FILE *fp = nowide::fopen(path.c_str(), "wb");
		if (fp == nullptr)
		{
			WARN_LOG(NAOMI, "Can't create card file %s: errno %d", path.c_str(), errno);
			return;
		}
		DEBUG_LOG(NAOMI, "Saving card file to %s", path.c_str());
		if (fwrite(cardData, 1, len, fp) != len)
			WARN_LOG(NAOMI, "Truncated write to file: %s", path.c_str());
		fclose(fp);
	}

	u8 inBuffer[256];
	u32 inBufferIdx = 0;

	u8 rxCommand[256];
	u32 rxCommandLen = 0;

	u8 outBuffer[256];
	u32 outBufferIdx = 0;
	u32 outBufferLen = 0;

	static constexpr u32 TRACK_SIZE = 0x45;
	u8 cardData[TRACK_SIZE * 3];
	bool doorOpen = false;
	bool cardInserted = false;
};

class SanwaCRP1231LR : public SanwaCRP1231BR
{
	u8 getStatus1() override
	{
		// '0'	no card
		// '1'	pos magnetic read/write
		// '2'	pos thermal printer
		// '3'	pos thermal dispenser
		// '4'	ejected not removed
		return cardInserted ? '1' : '0';
	}
};

// Hooked to the SH4 SCIF serial port
class InitialDCardReader final : public SanwaCRP1231BR
{
public:
	InitialDCardReader() {
		SCIFSerialPort::Instance().setPipe(this);
	}

	~InitialDCardReader() {
		SCIFSerialPort::Instance().setPipe(nullptr);
	}
};

// Hooked to the MIE via a 838-13661 RS232/RS422 converter board
class DerbyBRCardReader final : public SanwaCRP1231BR
{
public:
	DerbyBRCardReader() {
		getMieDevice()->setPipe(this);
	}

	~DerbyBRCardReader() {
		getMieDevice()->setPipe(nullptr);
	}
};

class DerbyLRCardReader final : public SanwaCRP1231LR
{
public:
	DerbyLRCardReader() {
		getMieDevice()->setPipe(this);
	}

	~DerbyLRCardReader() {
		getMieDevice()->setPipe(nullptr);
	}
};

static std::unique_ptr<SanwaCRP1231BR> cardReader;

void initdInit() {
	term();
	cardReader = std::make_unique<InitialDCardReader>();
}

void derbyInit()
{
	term();
	if (settings.content.gameId == " DERBY OWNERS CLUB WE ---------")
		cardReader = std::make_unique<DerbyBRCardReader>();
	else
		cardReader = std::make_unique<DerbyLRCardReader>();
}

void term() {
	cardReader.reset();
}

class BarcodeReader final : public SerialPort::Pipe
{
public:
	BarcodeReader() {
		SCIFSerialPort::Instance().setPipe(this);
	}

	~BarcodeReader() {
		SCIFSerialPort::Instance().setPipe(nullptr);
	}

	int available() override {
		return toSend.size();
	}

	u8 read() override
	{
		u8 data = toSend.front();
		toSend.pop_front();
		return data;
	}

	void insertCard()
	{
		if (toSend.size() >= 32)
			return;
		INFO_LOG(NAOMI, "Card read: %s", card.c_str());
		std::string data = card + "*";
		toSend.insert(toSend.end(), (const u8 *)&data[0], (const u8 *)(&data.back() + 1));
		SCIFSerialPort::Instance().updateStatus();
	}

	std::string getCard() const	{
		return card;
	}

	void setCard(const std::string& card) {
		this->card = card;
	}

private:
	std::deque<u8> toSend;
	std::string card;
};

static std::unique_ptr<BarcodeReader> barcodeReader;

void barcodeInit() {
	barcodeReader = std::make_unique<BarcodeReader>();
}

void barcodeTerm() {
	barcodeReader.reset();
}

bool barcodeAvailable() {
	return barcodeReader != nullptr;
}

std::string barcodeGetCard()
{
	if (barcodeReader != nullptr)
		return barcodeReader->getCard();
	else
		return "";
}

void barcodeSetCard(const std::string& card)
{
	if (barcodeReader != nullptr)
		barcodeReader->setCard(card);
}

void insertCard(int playerNum)
{
	if (cardReader != nullptr)
		cardReader->insertCard();
	else if (barcodeReader != nullptr)
		barcodeReader->insertCard();
	else
		insertRfidCard(playerNum);
}

}
