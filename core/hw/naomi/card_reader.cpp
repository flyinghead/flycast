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
#include <cerrno>

namespace card_reader {

class CardReaderWriter
{
public:
	virtual ~CardReaderWriter() = default;

	void insertCard()
	{
		cardInserted = loadCard();
		if (cardInserted)
			INFO_LOG(NAOMI, "Card inserted");
	}

protected:
	virtual bool loadCard() = 0;

	bool loadCard(u8 *cardData, u32 len)
	{
		std::string path = hostfs::getArcadeFlashPath() + ".card";
		FILE *fp = nowide::fopen(path.c_str(), "rb");
		if (fp == nullptr)
			return false;

		INFO_LOG(NAOMI, "Loading card file from %s", path.c_str());
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
		INFO_LOG(NAOMI, "Saving card file to %s", path.c_str());
		if (fwrite(cardData, 1, len, fp) != len)
			WARN_LOG(NAOMI, "Truncated write to file: %s", path.c_str());
		fclose(fp);
	}

	template<typename T>
	static u8 calcCrc(T begin, T end)
	{
		u32 crc = 0;
		for (auto it = begin; it != end; it++)
			crc ^= *it;
		return crc;
	}

	bool cardInserted = false;
	std::deque<u8> outBuffer;
	std::vector<u8> inBuffer;

	static constexpr u8 STX = 2;
	static constexpr u8 ETX = 3;
	static constexpr u8 ENQ = 5;
	static constexpr u8 ACK = 6;
};

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

class SanwaCRP1231BR : public CardReaderWriter, public SerialPort::Pipe
{
public:
	void write(u8 b) override
	{
		if (inBuffer.empty() && b == ENQ)
		{
			DEBUG_LOG(NAOMI, "Received RQ(5)");
			handleCommand();
			return;
		}
		inBuffer.push_back(b);
		if (inBuffer.size() >= 3)
		{
			if (inBuffer[0] != STX)
			{
				INFO_LOG(NAOMI, "Unexpected cmd start byte %x", inBuffer[0]);
				inBuffer.clear();
				return;
			}
			u32 len = inBuffer[1];
			if (inBuffer.size() < len + 2)
			{
				if (inBuffer.size() == 256)
				{
					WARN_LOG(NAOMI, "Card reader buffer overflow");
					inBuffer.clear();
				}
				return;
			}
			u32 crc = calcCrc(inBuffer.begin() + 1, inBuffer.end() - 1);
			if (crc != inBuffer.back())
			{
				INFO_LOG(NAOMI, "Wrong crc: expected %x got %x", crc, inBuffer.back());
				inBuffer.clear();
				return;
			}
			DEBUG_LOG(NAOMI, "Received cmd %x len %d", inBuffer[2], inBuffer[1]);
			outBuffer.push_back(ACK);
			rxCommandLen = std::min(inBuffer.size() - 3, sizeof(rxCommand));
			memcpy(rxCommand, &inBuffer[2], rxCommandLen);
			inBuffer.clear();
		}
	}

	u8 read() override
	{
		if (outBuffer.empty())
			return 0;
		u8 b = outBuffer.front();
		outBuffer.pop_front();
		DEBUG_LOG(NAOMI, "Sending %x", b);
		return b;
	}

	int available() override {
		return outBuffer.size();
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

	bool loadCard() override {
		return CardReaderWriter::loadCard(cardData, sizeof(cardData));
	}

	virtual u8 getStatus1()
	{
		return ((doorOpen ? 2 : 1) << 6) | 0x20 | (cardInserted ? 0x18 : 0);
	}

	void handleCommand()
	{
		if (rxCommandLen == 0)
			return;
		outBuffer.push_back(STX);
		u32 crcIdx = outBuffer.size();
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
			NOTICE_LOG(NAOMI, "Card ejected");
			if (cardInserted)
				os_notify("Card ejected", 2000);
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
		outBuffer.push_back(ACK);
		outBuffer.push_back(rxCommand[0]);
		outBuffer.push_back(status1);
		outBuffer.push_back(status2);
		outBuffer.push_back(status3);
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
				for (u32 i = 0; i < TRACK_SIZE; i++)
					outBuffer.push_back(cardData[i]);
				outBuffer[crcIdx] += TRACK_SIZE;
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
			for (u32 i = 0; i < size; i++)
				outBuffer.push_back(cardData[idx + i]);
			outBuffer[crcIdx] += size;
		}
		outBuffer.push_back(ETX);
		outBuffer.push_back(calcCrc(outBuffer.begin() + crcIdx, outBuffer.end()));
	}

	u8 rxCommand[256];
	u32 rxCommandLen = 0;

	static constexpr u32 TRACK_SIZE = 0x45;
	u8 cardData[TRACK_SIZE * 3];
	bool doorOpen = false;
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

	~InitialDCardReader() override {
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

	~DerbyBRCardReader() override {
		getMieDevice()->setPipe(nullptr);
	}
};

class DerbyLRCardReader final : public SanwaCRP1231LR
{
public:
	DerbyLRCardReader() {
		getMieDevice()->setPipe(this);
	}

	~DerbyLRCardReader() override {
		getMieDevice()->setPipe(nullptr);
	}
};

/*
	Club Kart - Sanwa CR-1231R

>>>	SEND CMD: [START 02][CMD char1][CMD char2]{parameter char}{data}[STOP 03][CRC]
<<<	RECV ACK: [OK 06] or [ERR 15]
<<<	RECV STX: [START 02]{REPLY}{data}[STOP 03][CRC]
	note: it seems reply packet sent only after command fully completed or error happened

	REPLY: 2chars
		OK - RESULT_OK
		O1 - RESULT_CANCEL_INSERT
		N0 - ERROR_CONNECT / Unknown Error
		N1 - ERROR_COMMAND / Connection Error
		N2 - ERROR_MOTOR / Mechanic Error 1
		N3 - ERROR_HEAD_UPDOWN / Mechanic Error 2
		N4 - ERROR_CARD_STUCK / Card Stuffed
		N5 - ERROR_VERIFY / OK ????
		N6 - ERROR_HEAD_TEMP / Mechanic Error 3
		N7 - ERROR_CARD_EMPTY / Card Empty
		N8 - ERROR_CARD_LOAD / Draw Card Error
		N9 - ERROR_NO_HOPPER / Card Empty
		NA - ERROR_CARD_PRESENT
		NB - ERROR_CARD_EJECT
		NC - ERROR_CANT_CANCEL
		ND - ERROR_NOT_INSERT
		NE - ERROR_NOT_WAIT
		NF - ERROR_BAD_CARD

	CMD SS REPLY: 6 ASCII characters
		5chars '0'/'1' - Card Sensors Status, MSB first
			0
			10 18
			1C C E 8 7 3
			other
		1char '0'/'1'  - Dispenser Status
			'0' - Empty
			'1' - Full

Commands:
IN   - init
CA   - cancel command
OT0  - eject card
HI   - get new card from dispenser
CL   - cleaning
RT5  - unknown
RL   - read data/load card into reader
WL   - write data (followed by 69 bytes of data)
SS   - get status

reply for commands is simple
06
02 'O' 'K' 03 crc
except
- RL command, which have 69 bytes of card data after OK (or no any reply if no card insterted)
- SS command, which is
06
02 '0/1' '0/1' '0/1' '0/1' '0/1' '0/1' 03 crc
there 0/1 - encoded in char binary value described earlier
 */
class ClubKartCardReader : public CardReaderWriter, SerialPort::Pipe
{
public:
	ClubKartCardReader() {
		SCIFSerialPort::Instance().setPipe(this);
	}
	~ClubKartCardReader() override {
		SCIFSerialPort::Instance().setPipe(nullptr);
	}

	void write(u8 data) override
	{
		inBuffer.push_back(data);
		if (inBuffer.size() == 5)
		{
			if ((inBuffer[1] != 'W' || inBuffer[2] != 'L') && inBuffer[2] != 'T')
			{
				handleCommand();
				inBuffer.clear();
			}
		}
		else if (inBuffer.size() == 6 && inBuffer[2] == 'T') // OT0, RT5
		{
			handleCommand();
			inBuffer.clear();
		}
		else if (inBuffer.size() == TRACK_SIZE + 5) // WL
		{
			handleCommand();
			inBuffer.clear();
		}
	}

	int available() override {
		return outBuffer.size();
	}

	u8 read() override
	{
		if (outBuffer.empty())
			return 0;
		u8 b = outBuffer.front();
		outBuffer.pop_front();
		return b;
	}

private:
	enum Commands {
		CARD_INIT,
		CARD_CANCEL_CMD,
		CARD_EJECT,
		CARD_NEW,
		CARD_CLEAN,
		CARD_RT5,
		CARD_READ,
		CARD_WRITE,
		CARD_STATUS,

		CARD_MAX
	};
	static const u8 CommandBytes[][2];

	bool loadCard() override
	{
		bool rc = CardReaderWriter::loadCard(cardData, sizeof(cardData));
		if (rc && readPending)
		{
			sendReply(CARD_READ);
			readPending = false;
		}
		return rc;
	}

	void handleCommand()
	{
		readPending = false;
		int cmd;
		for (cmd = 0; cmd < CARD_MAX; cmd++)
			if (inBuffer[1] == CommandBytes[cmd][0] && inBuffer[2] == CommandBytes[cmd][1])
				break;
		if (cmd == CARD_MAX)
		{
			WARN_LOG(NAOMI, "Unhandled command '%c%c'", inBuffer[1], inBuffer[2]);
			return;
		}
		u32 crc = calcCrc(inBuffer.begin() + 1, inBuffer.end() - 1);
		if (crc != inBuffer.back())
		{
			WARN_LOG(NAOMI, "Wrong crc: expected %x got %x", crc, inBuffer.back());
			return;
		}
		outBuffer.push_back(ACK);
		switch (cmd)
		{
		case CARD_WRITE:
			INFO_LOG(NAOMI, "Card write");
			for (u32 i = 0; i < sizeof(cardData); i++)
				cardData[i] = inBuffer[i + 3];
			saveCard(cardData, sizeof(cardData));
			break;
		case CARD_READ:
			INFO_LOG(NAOMI, "Card read");
			if (!cardInserted) {
				readPending = true;
				return;
			}
			break;
		case CARD_EJECT:
			NOTICE_LOG(NAOMI, "Card ejected");
			if (cardInserted)
				os_notify("Card ejected", 2000);
			cardInserted = false;
			break;
		case CARD_NEW:
			INFO_LOG(NAOMI, "New card");
			cardInserted = true;
			break;
		case CARD_INIT:
			DEBUG_LOG(NAOMI, "Card init");
			break;
		case CARD_CANCEL_CMD:
			DEBUG_LOG(NAOMI, "Cancel cmd");
			break;
		case CARD_CLEAN:
			DEBUG_LOG(NAOMI, "Card clean");
			break;
		case CARD_RT5:
			DEBUG_LOG(NAOMI, "Card RT5");
			break;
		case CARD_STATUS:
			DEBUG_LOG(NAOMI, "Card status (cardInserted %d)", cardInserted);
			break;
		}
		sendReply(cmd);
	}

	void sendReply(int cmd)
	{
		outBuffer.push_back(STX);
		u32 crcIndex = outBuffer.size();
		if (cmd == CARD_STATUS)
		{
			outBuffer.push_back('0');
			outBuffer.push_back('0');
			outBuffer.push_back('0');
			outBuffer.push_back(cardInserted ? '1' : '0');
			outBuffer.push_back(cardInserted ? '1' : '0');
			outBuffer.push_back('1'); // dispenser full
		}
		else
		{
			outBuffer.push_back('O');
			outBuffer.push_back('K');
			if (cmd == CARD_READ) {
				for (u32 i = 0; i < sizeof(cardData); i++)
					outBuffer.push_back(cardData[i]);
			}
		}
		outBuffer.push_back(ETX);
		outBuffer.push_back(calcCrc(outBuffer.begin() + crcIndex, outBuffer.end()));
	}

	static constexpr u32 TRACK_SIZE = 0x45;
	u8 cardData[TRACK_SIZE];

	bool readPending = false;
};

const u8 ClubKartCardReader::CommandBytes[][2]
{
	{ 'I', 'N' },	// init
	{ 'C', 'A' },	// cancel command
	{ 'O', 'T' },	// ...0  - eject card
	{ 'H', 'I' },	// get new card from dispenser
	{ 'C', 'L' },	// cleaning
	{ 'R', 'T' },	// ...5  - unknown
	{ 'R', 'L' },	// read data/load card into reader
	{ 'W', 'L' },	// write data (followed by 69 bytes of data)
	{ 'S', 'S' },	// get status
};

static std::unique_ptr<CardReaderWriter> cardReader;

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

void clubkInit() {
	term();
	cardReader = std::make_unique<ClubKartCardReader>();
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

	~BarcodeReader() override {
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

bool readerAvailable()
{
	return cardReader != nullptr
			|| barcodeAvailable()
			|| getRfidCardData(0) != nullptr;
}

}
