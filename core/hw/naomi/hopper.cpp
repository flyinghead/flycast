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
#include "hopper.h"
#include "network/ggpo.h"
#include "input/gamepad.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/modules/modules.h"
#include "serialize.h"
#include "oslib/oslib.h"
#include "emulator.h"
#include "cfg/option.h"

#include <array>
#include <vector>
#include <deque>
#include <memory>

namespace hopper
{

class BaseHopper : public SerialPort::Pipe
{
public:
	BaseHopper()
	{
		schedId = sh4_sched_register(0, schedCallback, this);
		sh4_sched_request(schedId, SCHED_CYCLES);
		EventManager::listen(Event::Pause, handleEvent, this);

		std::string path = getConfigFileName();
		FILE *f = fopen(path.c_str(), "rb");
		if (f == nullptr) {
			INFO_LOG(NAOMI, "Hopper config not found at %s", path.c_str());
		}
		else
		{
			u8 data[4096];
			size_t len = fread(data, 1, sizeof(data), f);
			fclose(f);
			verify(len < sizeof(data));
			if (len <= 0) {
				ERROR_LOG(NAOMI, "Hopper config empty or I/O error: %s", path.c_str());
			}
			else
			{
				Deserializer deser(data, len);
				deserializeConfig(deser);
			}
		}
	}

	virtual ~BaseHopper() {
		EventManager::unlisten(Event::Pause, handleEvent, this);
		sh4_sched_unregister(schedId);
	}

	u8 read() override
	{
		if (toSend.empty())
			return 0;
		else
		{
			u8 v = toSend.front();
			toSend.pop_front();
			return v;
		}
	}
	
	int available() override {
		return toSend.size();
	}

	void write(u8 data) override
	{
		if (recvBuffer.empty() && data != 'H') {
			WARN_LOG(NAOMI, "Ignored data %02x %c", data, data);
			return;
		}
		recvBuffer.push_back(data);
		if (recvBuffer.size() == 3)
			expectedBytes = data;
		else if (recvBuffer.size() == 4)
			expectedBytes += data << 8;
		else if (expectedBytes > 0 && recvBuffer.size() == expectedBytes)
		{
			handleMessage(recvBuffer[1]);
			recvBuffer.clear();
			expectedBytes = 0;
		}
	}
	
	void serialize(Serializer& ser) const
	{
		ser << (u32)recvBuffer.size();
		ser.serialize(recvBuffer.data(), recvBuffer.size());
		serializeConfig(ser);
		ser << expectedBytes;
		ser << (u32)toSend.size();
		for (u8 b : toSend)
			ser << b;
		ser << started;
		sh4_sched_serialize(ser, schedId);
	}
	
	void deserialize(Deserializer& deser)
	{
		u32 size;
		deser >> size;
		recvBuffer.resize(size);
		deser.deserialize(recvBuffer.data(), size);
		deserializeConfig(deser);
		deser >> expectedBytes;
		deser >> size;
		toSend.clear();
		for (u32 i = 0; i < size; i++)
		{
			u8 b;
			deser >> b;
			toSend.push_back(b);
		}
		deser >> started;
		sh4_sched_deserialize(deser, schedId);
	}

	void saveConfig() const
	{
		std::string path = getConfigFileName();
		FILE *f = fopen(path.c_str(), "wb");
		if (f == nullptr) {
			ERROR_LOG(NAOMI, "Can't save hopper config to %s", path.c_str());
			return;
		}
		Serializer ser;
		serializeConfig(ser);
		std::unique_ptr<u8[]> data = std::make_unique<u8[]>(ser.size());
		ser = Serializer(data.get(), ser.size());
		serializeConfig(ser);

		size_t len = fwrite(data.get(), 1, ser.size(), f);
		fclose(f);
		if (len != ser.size())
			ERROR_LOG(NAOMI, "Hopper config I/O error: %s", path.c_str());
	}

protected:
	virtual void handleMessage(u8 command) = 0;
	virtual void sendCoinInMessage() = 0;
	virtual void sendCoinOutMessage() = 0;
	virtual void sendPayWinMessage() = 0;

	void sendMessage(u8 command, const u8 *payload, size_t len)
	{
		DEBUG_LOG(NAOMI, "hopper sending command %x size %x", command, (int)len + 5);
		// 'H' <u8 command> <u16 length> payload ... <u8 checksum>
		u8 chksum = 'H' + command;
		toSend.push_back('H');
		toSend.push_back(command);
		len += 5;
		toSend.push_back(len & 0xff);
		chksum += len & 0xff;
		toSend.push_back(len >> 8);
		chksum += len >> 8;
		len -= 5;
		for (size_t i = 0; i < len; i++, payload++) {
			toSend.push_back(*payload);
			chksum += *payload;
		}
		toSend.push_back(chksum);
		SCIFSerialPort::Instance().updateStatus();
	}

	void bet(const u32 *values)
	{
		for (int i = 0; i < 2; i++)
		{
			u32& primary = i == 0 ? credit0 : credit1;
			u32& second = i == 0 ? credit1 : credit0;
			if (primary >= values[i]) {
				primary -= values[i];
			}
			else
			{
				int residual = values[i] - primary;
				primary = 0;
				second = std::max(0, (int)second - residual);
			}
		}
		premium = std::max(0, (int)premium - (int)values[2]);
	}

	void payOut(u32 value)
	{
		if (value == 0)
			return;
		wonAmount += value;
		if (!autoPayOut)
			credit0 += value;
		else {
			paidAmount += value;
			sendPayWinMessage();
		}
	}

	void insertCoin(u32 value) {
		credit0 += value;
	}

	std::vector<u8> recvBuffer;
	u32 credit0 = 0;
	u32 credit1 = 0;
	u32 totalCredit = 100; // min bet
	u32 premium = 0;
	u32 gameNumber = 0;
	bool freePlay = false;
	bool autoPayOut = false;
	bool autoExchange = false;
	bool twoWayMode = true;
	bool coinDiscrimination = true;
	bool betButton = true;
	u8 currency = ~0;	// 0:medal, 1:pound, 2:dollar, 3:euro, 4:token, 5: any cash (837-14438 only)
	u8 medalExchRate = 5;
	u32 maxHopperFloat = 200;
	u32 maxPay = 1999900;
	u32 maxCredit = 1999900;
	u32 hopperSize = 39900;
	u32 maxBet = 10000;
	u32 minBet = 100; // normally 1000
	u32 addBet = 100;
	u32 paidAmount = 0;
	u32 wonAmount = 0;
	u32 curBase = 100;

	int schedId;
	bool started = false;
	bool coinKey = false;

	static constexpr u32 SCHED_CYCLES = SH4_MAIN_CLOCK / 60;

private:
	static int schedCallback(int tag, int cycles, int jitter, void *p)
	{
		BaseHopper *board = (BaseHopper *)p;
		if (board->started)
		{
			 // button D is coin
			if ((mapleInputState[0].kcode & DC_BTN_D) == 0 && !board->coinKey)
				board->sendCoinInMessage();
			board->coinKey = (mapleInputState[0].kcode & DC_BTN_D) == 0;
		}
		return SCHED_CYCLES;
	}

	static void handleEvent(Event event, void *p) {
		((BaseHopper *)p)->saveConfig();
	}

	std::string getConfigFileName() const
	{
		return hostfs::getArcadeFlashPath() + "-hopper.bin";
	}

	void serializeConfig(Serializer& ser) const
	{
		ser << credit0;
		ser << credit1;
		ser << totalCredit;
		ser << premium;
		ser << gameNumber;
		ser << freePlay;
		ser << autoPayOut;
		ser << autoExchange;
		ser << twoWayMode;
		ser << coinDiscrimination;
		ser << currency;
		ser << medalExchRate;
		ser << maxHopperFloat;
		ser << maxPay;
		ser << maxCredit;
		ser << hopperSize;
		ser << maxBet;
		ser << minBet;
		ser << addBet;
		ser << paidAmount;
		ser << wonAmount;
		ser << betButton;
		ser << curBase;
	}

	void deserializeConfig(Deserializer& deser)
	{
		deser >> credit0;
		deser >> credit1;
		deser >> totalCredit;
		deser >> premium;
		deser >> gameNumber;
		deser >> freePlay;
		deser >> autoPayOut;
		deser >> autoExchange;
		deser >> twoWayMode;
		deser >> coinDiscrimination;
		deser >> currency;
		deser >> medalExchRate;
		deser >> maxHopperFloat;
		deser >> maxPay;
		deser >> maxCredit;
		deser >> hopperSize;
		deser >> maxBet;
		deser >> minBet;
		deser >> addBet;
		if (deser.version() >= Deserializer::V39)
		{
			deser >> paidAmount;
			deser >> wonAmount;
			deser >> betButton;
			deser >> curBase;
		}
		else
		{
			paidAmount = 0;
			wonAmount = 0;
		}
	}

	u32 expectedBytes = 0;
	std::deque<u8> toSend;
};

//
// SEGA 837-14438 hopper board
//
// Used by kick'4'cash
// Can be used by club kart prize (ver.b) and shootout pool prize (ver.b) if P1 btn 0 is pressed at boot
//
class Sega837_14438Hopper : public BaseHopper
{
protected:
	void handleMessage(u8 command) override
	{
		switch (command)
		{
			case 0x40:
			{
				// VERSION
				INFO_LOG(NAOMI, "hopper received VERSION");
				std::array<u32, 4> payload{};
				payload[0] = 0x00010001;
				payload[1] = 3;
				sendMessage(0x20, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}
			
			case 0x41:
			{
				// POWER ON
				INFO_LOG(NAOMI, "hopper received POWER ON");
				// 8: currency
				if (currency != recvBuffer[8]) {
					currency = recvBuffer[8];
					setDefaults();
				}
				std::array<u32, 0x1ea> payload{};
				payload[0] = gameNumber;	// game number
				payload[1] = 0;         	// atp num
				payload[2] = 0;         	// atp kind and error code
				payload[3] = status;		// status bitfield. rectangle in background if != 0
				payload[4] = freePlay | (autoPayOut << 8) | (twoWayMode << 16) | (medalExchRate << 24);
				payload[5] = autoExchange | (coinDiscrimination << 8) | (curBase << 16) | (betButton << 24);
				payload[6] = 0;				// ? 8c027124
				payload[7] = maxPay;
				payload[8] = maxCredit;
				payload[9] = hopperSize;
				payload[10] = maxBet;
				payload[11] = minBet;
				payload[12] = addBet;
				payload[13] = currency;		// currency (lsb, 0:medal, 1:pound, 2:dollar, 3:euro, 4:token, 5: any cash)
										// ? 8c027141, 8c027142, 8c027143
				payload[14] = credit0;		// credit0
				payload[15] = credit1;		// credit1
				payload[16] = totalCredit;	// totalCredit
				payload[17] = premium;		// premium
				// FUN_8c00e26e
				payload[18] = 0;		// ? 8c02cd60
				payload[19] = 0;		// ? FUN_8c014806() or FUN_8c00817c()
				// &payload[20]			// copy of mem (8c02715c, 0x58 bytes)
				// FUN_8c009f8a
				payload[0x2a] = 0;		// ? FUN_8c015b5a()
				payload[0x2b] = 0;		// ? FUN_8c0163d8() + FUN_8c0148d4()
				payload[0x2c] = 0;		// hopperOutLap
				payload[0x2d] = wonAmount;
				payload[0x2e] = 0;		// ? 8c02717c
				payload[0x2f] = 0;		// ? 8c027180
				payload[0x30] = 0;		// ? 8c027188
				payload[0x31] = 0;		// ? 8c02718c
				payload[0x32] = paidAmount;	// paid? 8c027190
				
				payload[0x33] = maxHopperFloat;
				payload[0x34] = 0;		// showLastWinAndHopperFloat (b6: show last win b7: show hopperfloat)
				payload[0x35] = 0;		// ? 8c02714c
				payload[0x36] = 0;		// ? 8c027150
				payload[0x37] = 0x80;	// dataportCondition (b6: protocol on, bit7: forever)
				payload[0x38] = 0;		// ? 8c027f48
				payload[0x39] = 0;		// ? 8c027f4c
				payload[0x3a] = 0;		// ? 8c027f50
				// FUN_8c00ae24
				payload[0x3b] = 0;		// ? 20 bytes
				// 1c2		0 ?
				// 1c9		100 or 50 depending on currency
				// 1ca		10000 or 100 depending...
				// 1cb		(short)same as 1c9
				//			(short)499 or 5 dep...
				// 1cc		(short)500 or 5
				//			499
				// 1cd		(short)500
				//			(short)49 or 19 dep...
				// 1ce		50 or 20 dep...
				// 1cf		200, 70 or 100
				// 1d0		0 or 100
				// 1d1		0 or 100
				// 1d2		0, 2 or 3
				// 1d3		(short)2000, 70 or 200
				//			(short)200, 70 or 50
				// 1d4		100, 0 or 10
				// 1d6		(short)100 0 0 0 0 0 0 0
				// 1d8		(short)12 shorts?
				//				100 200 300 400 500 1000 2000 2500 5000 10000 0 ...
				// Default values
				const u32 *def = getDefaultValues();
				payload[0x1c0] = def[0];
				payload[0x1c1] = def[1];
				payload[0x1c3] = def[2];
				payload[0x1c4] = def[3];
				payload[0x1c5] = def[4];
				payload[0x1c6] = def[5];
				payload[0x1c7] = def[6];
				payload[0x1c8] = def[7];
				getRanges(&payload[0x1c9]);
				payload[0x1e9] = 1;		// hopper ready flag
				sendMessage(0x21, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				started = true;
				break;
			}

			case 0x42:
			{
				// GET_STATUS
				INFO_LOG(NAOMI, "hopper received GET STATUS");
				// TODO offset 4: 0 or 2
				std::array<u32, 4> payload{};
				payload[0] = 0;         // atp num
				payload[1] = 0;         // atp kind and error code
				payload[2] = status;
				sendMessage(0x22, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x43:
			{
				// GAME START
				INFO_LOG(NAOMI, "hopper received GAME START");
				u32 *betValues = (u32 *)&recvBuffer[4];
				bet(betValues);
				std::array<u32, 0x1f> payload{};
				payload[0] = ++gameNumber;	// game#, ?
				payload[1] = 0;				// atp num
				payload[2] = 0;				// atp kind and error code
				payload[3] = status;
				payload[4] = credit0;
				payload[5] = credit1;
				payload[6] = totalCredit;
				payload[7] = premium;
				// &payload[8]	? 0x58 bytes
				sendMessage(0x23, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x44:
			{
				// GAME END
				INFO_LOG(NAOMI, "hopper received GAME END");
				std::array<u32, 0x1e> payload{};
				payload[0] = gameNumber;	// gameNum, upper 16 bits: ? 0, -1 or -2
				payload[1] = credit0;
				payload[2] = credit1;
				payload[3] = totalCredit;
				payload[4] = premium;
				// ? (58 bytes)
				payload[27] = 2;			// wins
				payload[28] = 0;			// last paid
				sendMessage(0x24, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x45:
			{
				// TEST
				INFO_LOG(NAOMI, "hopper received TEST");
				std::array<u32, 0x3c> payload{};
				payload[0] = 0;         // atp num
				payload[1] = 0;         // atp kind and error code
				payload[2] = status;
				// TODO
				// 3: memcpy 8c027028, 0x60
				// 0x18: memcpy &errorCode, 0x80 (follows previous in ram)
				// 0x38: bit0: dataport temp? bit1: ?
				payload[0x38] = 0;
				sendMessage(0x25, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x4a:
			{
				// SWITCH
				INFO_LOG(NAOMI, "hopper received SWITCH");
				// TODO ?
				break;
			}

			case 0x4b:
			{
				// CONFIG_HOP
				INFO_LOG(NAOMI, "hopper received CONFIG HOP");
				if (recvBuffer[2] == 0x2c && recvBuffer[3] == 0)
				{
					freePlay = recvBuffer[4] & 1;
					autoPayOut = recvBuffer[5] & 1;
					twoWayMode = recvBuffer[6] & 1;
					medalExchRate = recvBuffer[7];
					autoExchange = recvBuffer[8] & 1;
					coinDiscrimination = recvBuffer[9] & 1;
					curBase = recvBuffer[0xa];
					betButton = recvBuffer[0xb] & 1;
					// 0xc ??
					maxPay = *(u32 *)&recvBuffer[0x10];
					maxCredit = *(u32 *)&recvBuffer[0x14];
					hopperSize = *(u32 *)&recvBuffer[0x18];
					maxBet = *(u32 *)&recvBuffer[0x1c];
					minBet = *(u32 *)&recvBuffer[0x20];
					addBet = *(u32 *)&recvBuffer[0x24];
				}
				std::array<u32, 0xa> payload{};
				payload[0] = freePlay | (autoPayOut << 8) | (twoWayMode << 16) | (medalExchRate << 24);
				payload[1] = autoExchange | (coinDiscrimination << 8) | (curBase << 16) | (betButton << 24);
				payload[2] = 0;				// ? 8c027124
				payload[3] = maxPay;
				payload[4] = maxCredit;
				payload[5] = hopperSize;
				payload[6] = maxBet;
				payload[7] = minBet;
				payload[8] = addBet;
				payload[9] = currency;		// currency (lsb, 0:medal, 1:pound, 2:dollar, 3:euro, 4:token, 5:any cash)
										// ? 8c027141, 8c027142, 8c027143
				sendMessage(0x2b, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x61: // COIN IN ACK
			case 0x62: // COIN OUT ACK
				break;

			default:
				WARN_LOG(NAOMI, "Unexpected hopper message: %x", command);
				break;
		}
	}
	
private:
	/*
	void sendStatusMessage()
	{
		std::array<u32, 4> payload{};
		payload[0] = 0;         // atp num
		payload[1] = 0;         // atp kind and error code
		payload[2] = status;	// status bitfield. rectangle in background if != 0
		sendMessage(0, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}
	void sendErrorMessage()
	{
		std::array<u32, 4> payload{};
		payload[0] = 0;         // atp num
		payload[1] = 0;         // atp kind and error code
		payload[2] = status;	// status bitfield. rectangle in background if != 0
		sendMessage(7, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}
	*/

	void sendCoinInMessage() override
	{
		insertCoin(100);
		std::array<u32, 8> payload{};
		payload[0] = 0;         // atp num
		payload[1] = 0;         // atp kind and error code
		payload[2] = status;
		payload[3] = credit0;
		payload[4] = credit1;
		payload[5] = totalCredit;
		payload[6] = premium;
		sendMessage(1, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}

	void sendCoinOutMessage() override
	{
		insertCoin(100);
		std::array<u32, 8> payload{};
		payload[0] = 0;         // atp num
		payload[1] = 0;         // atp kind and error code
		payload[2] = status;
		payload[3] = credit0;
		payload[4] = credit1;
		payload[5] = totalCredit;
		payload[6] = premium;
		sendMessage(2, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}

	void sendPayWinMessage() override
	{
		std::array<u32, 0xa> payload{};
		payload[0] = 0;         // atp num
		payload[1] = 0;         // atp kind and error code
		payload[2] = status;
		payload[3] = credit0;
		payload[4] = credit1;
		payload[5] = totalCredit;
		payload[6] = premium;
		payload[7] = wonAmount;
		payload[8] = paidAmount;
		sendMessage(3, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}

	const u32 *getDefaultValues()
	{
		static const u32 defaults[][8] = {
			// free play...           max pay         hopper size   min bet
			//            auto exch...        max credit      max bet     add bet
			{ 0x05010000, 0x01640100, 1999900, 1999900, 39900, 10000, 1000, 100 },
			{      0x100,    0x50000,   10000, 1999900, 39900,   100,  100,  50 },
			{      0x100,    0x50000,   10000, 1999900, 39900,   100,  100, 100 },
			{      0x100,    0x50000,   10000, 1999900, 39900,   100,  100, 100 },
			{          0, 0x01640000, 1999900, 1999900, 39900, 10000,  100, 100 },
			{      0x100,    0x50000,   10000, 1999900, 39900,   100,  100, 100 },
		};
		if (currency >= std::size(defaults))
			return defaults[0];
		else
			return defaults[currency];
	}

	void setDefaults()
	{
		const u32 *def = getDefaultValues();
		freePlay = def[0] & 1;
		autoPayOut = def[0] & 0x100;
		twoWayMode = def[0] & 0x10000;
		medalExchRate = def[0] >> 24;
		autoExchange = def[1] & 1;
		coinDiscrimination = def[1] & 0x100;
		curBase = (def[1] >> 16) & 0xff;
		betButton = def[1] & 0x1000000;
		maxPay = def[2];
		maxCredit = def[3];
		hopperSize = def[4];
		maxBet = def[5];
		minBet = def[6];
		addBet = def[7];
		totalCredit = minBet;
	}

	void getRanges(u32 *p)
	{
		switch (currency)
		{
		case 0: // medal
			p[0x0] = 100;
			p[0x1] = 10000;
			p[0x2] = 100 | (499 << 16);
			p[0x3] = 500 | (499 << 16);
			p[0x4] = 500 | (49 << 16);
			p[0x5] = 50;
			p[0x6] = 200;
			p[0x7] = 0;
			p[0x8] = 0;
			p[0x9] = 0;
			p[0xa] = 2000 | (200 << 16);
			p[0xb] = 100;
			p[0xc] = 0;

			p[0xd] = 100;
			p[0xe] = 0;
			p[0xf] = 0;
			p[0x10] = 0;

			p[0x11] = 100 | (200 << 16);
			p[0x12] = 300 | (400 << 16);
			p[0x13] = 500 | (1000 << 16);
			p[0x14] = 2000 | (2500 << 16);
			p[0x15] = 5000 | (10000 << 16);
			p[0x16] = 0;
			break;
		case 1: // pound
			p[0x0] = 50;
			p[0x1] = 100;
			p[0x2] = 50 | (5 << 16);
			p[0x3] = 5 | (499 << 16);
			p[0x4] = 500 | (19 << 16);
			p[0x5] = 20;
			p[0x6] = 70;
			p[0x7] = 0;
			p[0x8] = 0;
			p[0x9] = 0;
			p[0xa] = 70 | (70 << 16);
			p[0xb] = 0;
			p[0xc] = 0;

			p[0xd] = 5 | (10 << 16);
			p[0xe] = 20 | (50 << 16);
			p[0xf] = 50 | (100 << 16);
			p[0x10] = 200;

			p[0x11] = 5 | (10 << 16);
			p[0x12] = 20 | (50 << 16);
			p[0x13] = 100 | (200 << 16);
			p[0x14] = 500 | (1000 << 16);
			p[0x15] = 0;
			p[0x16] = 0;
			break;
		case 2: // dollar
		case 5: // any cash
		default:
			p[0x0] = 50;
			p[0x1] = 100;
			p[0x2] = 50 | (5 << 16);
			p[0x3] = 5 | (499 << 16);
			p[0x4] = 500 | (19 << 16);
			p[0x5] = 20;
			p[0x6] = 200;
			p[0x7] = 0;
			p[0x8] = 0;
			p[0x9] = 0;
			p[0xa] = 200 | (50 << 16);
			p[0xb] = 10;
			p[0xc] = 0;

			p[0xd] = 5 | (10 << 16);
			p[0xe] = 20 | (25 << 16);
			p[0xf] = 50 | (100 << 16);
			p[0x10] = 200 | (100 << 16);

			p[0x11] = 5 | (10 << 16);
			p[0x12] = 20 | (25 << 16);
			p[0x13] = 50 | (100 << 16);
			p[0x14] = 200 | (500 << 16);
			p[0x15] = 1000;
			p[0x16] = 0;
			break;
		case 3: // euro
			p[0x0] = 50;
			p[0x1] = 100;
			p[0x2] = 50 | (5 << 16);
			p[0x3] = 5 | (499 << 16);
			p[0x4] = 500 | (19 << 16);
			p[0x5] = 20;
			p[0x6] = 200;
			p[0x7] = 0;
			p[0x8] = 0;
			p[0x9] = 0;
			p[0xa] = 200 | (50 << 16);
			p[0xb] = 10;
			p[0xc] = 0;

			p[0xd] = 5 | (10 << 16);
			p[0xe] = 20 | (50 << 16);
			p[0xf] = 50 | (100 << 16);
			p[0x10] = 200 | (100 << 16);

			p[0x11] = 5 | (10 << 16);
			p[0x12] = 20 | (50 << 16);
			p[0x13] = 100 | (200 << 16);
			p[0x14] = 500 | (1000 << 16);
			p[0x15] = 0;
			p[0x16] = 0;
			break;
		case 4: // token
			p[0x0] = 100;
			p[0x1] = 10000;
			p[0x2] = 100 | (499 << 16);
			p[0x3] = 500 | (499 << 16);
			p[0x4] = 500 | (19 << 16);
			p[0x5] = 20;
			p[0x6] = 200;
			p[0x7] = 0;
			p[0x8] = 0;
			p[0x9] = 0;
			p[0xa] = 200 | (50 << 16);
			p[0xb] = 10;
			p[0xc] = 0;

			p[0xd] = 100;
			p[0xe] = 0;
			p[0xf] = 0;
			p[0x10] = 0;

			p[0x11] = 100 | (200 << 16);
			p[0x12] = 300 | (400 << 16);
			p[0x13] = 500 | (1000 << 16);
			p[0x14] = 2000 | (2500 << 16);
			p[0x15] = 5000 | (10000 << 16);
			p[0x16] = 0;
			break;
		}
	}

	// ?:1
	// ?:1
	// coinInEnabled?:1
	// yenInEnabled?:1
	// frontDoorOpen:1
	// backDoorOpen:1
	// cashDoorOpen:1
	// needsRefill?:1
	// ?:1
	const u32 status = 0x00000000;
};

// Naomi SWP Hopper Board
class NaomiHopper : public BaseHopper
{
protected:
	void handleMessage(u8 command) override
	{
		switch (command)
		{
			case 0x20:
			{
				// POWER ON
				static const char hopperErrors[][32] = {
					"UNKNOWN ERROR",
					"ROM IS BAD",
					"LOW BATTERY",
					"ROM HAS CHANGED",
					"RAM DATA IS BAD",
					"I/O ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"COIN IN JAM(HOPPER)",
					"COIN IN JAM(GAME)",
					"HOPPER OVER PAID",
					"HOPPER RUNAWAY",
					"HOPPER EMPTY/JAM",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"COM. TIME OUT",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"I/O BOARD IS BAD",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"UNKNOWN ERROR",
					"ATP NONE\0\0\0\0\0\0\0\0MAX. PAY",
					"HOPPER SIZE\0\0\0\0\0MAX. CREDIT",
					"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0DOOR IS OPEN"
				};

				// 8: currency
				if (currency != recvBuffer[8]) {
					currency = recvBuffer[8];
					setDefaults();
				}
				INFO_LOG(NAOMI, "hopper received POWER ON");
				std::array<u32, 0x13a> payload{};
				payload[0] = gameNumber;
				payload[1] = 0;			// atp num
				payload[2] = status;	// status bitfield. rectangle in background if != 0
				payload[3] = freePlay | (autoPayOut << 8) | (twoWayMode << 16) | (medalExchRate << 24);
				payload[4] = autoExchange | (coinDiscrimination << 8) | (curBase << 16) | (betButton << 24);
				payload[5] = maxPay;
				payload[6] = maxCredit;
				payload[7] = hopperSize;
				payload[8] = maxBet;
				payload[9] = minBet;
				payload[10] = addBet;
				payload[11] = currency;		// ? BYTE_8c0c9fa8-b
				payload[12] = credit0;
				payload[13] = credit1;
				payload[14] = totalCredit;
				payload[15] = premium;
				payload[16] = wonAmount;
				payload[17] = paidAmount;	// paid amount (if autoPO) else credit won?
				
				payload[18] = 0;		// coin1 in count
				payload[19] = 0;		// coin2 in count
				payload[20] = 0;		// coin3 in count
				payload[21] = 0;		// coin4 in count
				payload[22] = 0;		// coin5 in count
				payload[23] = 0;		// coin6 in count
				payload[24] = 0;		// coin out count

				payload[25] = 0;        // big int(1/2) 8c0ca674?
				payload[26] = 0;        // big int(2/2)?
				payload[27] = 0;        // 8c0ca67c?

				payload[28] = 0;        // 8c0ca680?
				payload[29] = 0;        // big int(1/2) 8c0ca684?
				payload[30] = 0;        // big int(2/2)?
				payload[31] = 0;        // 8c0ca68c?
				payload[32] = 0;		// 8c0ca690?
				size_t idx = 0x84;
				for (size_t i = 0; i < std::size(hopperErrors); i++) {
					memcpy((u8 *)payload.data() + idx, hopperErrors[i], sizeof(hopperErrors[i]));
					idx += 32;
				}
				// init done/hopper ready?
				payload[0x139] = 1;
				sendMessage(0x10, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);

				started = true;

				// Displays the in-game hopper debug overlay
				//u32 payload2 = 0;
				//sendMessage(0xf, (const u8 *)&payload2, 3); // DEBUG
				break;
			}

			case 0x21:	// GET_STATUS
			{
				INFO_LOG(NAOMI, "hopper received GET STATUS");
				std::array<u32, 3> payload{};
				payload[0] = 0;			// atp num?
				payload[1] = status;
				sendMessage(0x11, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x22:	// GAME START
			{
				INFO_LOG(NAOMI, "hopper received GAME START");
				// 4: bet value: credit0
				// 8:            credit1
				// c:			 premium
				u32 *betValues = (u32 *)&recvBuffer[4];
				bet(betValues);
				wonAmount = 0;
				paidAmount = 0;
				std::array<u32, 0x17> payload{};
				payload[0] = ++gameNumber;
				payload[1] = 0;				// atp num
				payload[2] = status;
				payload[3] = credit0;
				payload[4] = credit1;
				payload[5] = totalCredit;
				payload[6] = premium;
				// 7-c: coinIn1-6Count
				// d: coinOutCount
				// e-f: bigint 8c0ca674
				// 10: 8c0ca67c
				// 11: 8c0ca680
				// 12-13: bigint 8c0ca684
				// 14: 8c0ca68c
				// 15: 8c0ca690
				sendMessage(0x12, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x23:	// GAME END
			{
				INFO_LOG(NAOMI, "hopper received GAME END");
				// 8: amount to pay?
				u32 value = *(u32 *)&recvBuffer[8];
				payOut(value);
				std::array<u32, 0x15> payload{};
				payload[0] = gameNumber;	// gameNum, upper 16 bits: ? 0, -1 or -2
				payload[1] = credit0;
				payload[2] = credit1;
				payload[3] = totalCredit;
				payload[4] = premium;
				// 5-a: coinIn1-6Count
				// b: coinOutCount
				// c-d: bigint 8c0ca674
				// e: 8c0ca67c
				// f: 8c0ca680
				// 10-11: bigint 8c0ca684
				// 12: 8c0ca68c
				// 13: 8c0ca690
				sendMessage(0x13, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x24:	// TEST START
			{
				INFO_LOG(NAOMI, "hopper received TEST START");
				// recv[4] is 1
				std::array<u32, 0x33> payload{};
				payload[0] = 0;				// atp num
				payload[1] = status;
				// c0 bytes from 8c0ca1b4
				payload[0x12] = 2023 | (7 << 16) | (19 << 24);	// year month day
				payload[0x13] = (55 << 24) | (48 << 16) | (11 << 8) | 3; // day of week, hour, minute, second
				payload[0x14] = gameNumber;
				sendMessage(0x14, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x25:	// BACKUP CLEAR
			{
				INFO_LOG(NAOMI, "hopper received BACKUP CLEAR");
				credit0 = 0;
				credit1 = 0;
				premium = 0;
				paidAmount = 0;
				wonAmount = 0;
				std::array<u32, 0x44> payload{};
				payload[0] = credit0;
				payload[1] = credit1;
				payload[2] = totalCredit;
				payload[3] = premium;
				// coin1InCount -> coin6InCount
				// coinOutCount
				// bigint 8c0ca674
				// 8c0ca67c
				// 8c0ca680
				// bigint 8c0ca684
				// 8c0ca68c
				// 8c0ca690
				// copy c0 bytes from 8c0ca1b4
				payload[0x23] = 2023 | (7 << 16) | (19 << 24);	// year month day
				payload[0x24] = (55 << 24) | (48 << 16) | (11 << 8) | 3; // day of week, hour, minute, second
				payload[0x25] = gameNumber;
				sendMessage(0x15, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x26:	// BET
			{
				INFO_LOG(NAOMI, "hopper received BET");
				u32 v = *(u32 *)&recvBuffer[4];
				std::array<u32, 2> payload{};
				payload[0] = v;
				sendMessage(0x16, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x29:	// SWITCH
			{
				INFO_LOG(NAOMI, "hopper received SWITCH");
				// 4: test button
				// 5: service button
				// 6-7: c01 in test menu
				break;
			}

			case 0x2a:	// CONFIG HOP
			{
				INFO_LOG(NAOMI, "hopper received CONFIG HOP");
				if (recvBuffer[2] == 0x28 && recvBuffer[3] == 0)
				{
					freePlay = recvBuffer[4] & 1;
					autoPayOut = recvBuffer[5] & 1;
					twoWayMode = recvBuffer[6] & 1;
					medalExchRate = recvBuffer[7];
					autoExchange = recvBuffer[8] & 1;
					coinDiscrimination = recvBuffer[9] & 1;
					curBase = recvBuffer[0xa];
					betButton = recvBuffer[0xb] & 1;
					maxPay = *(u32 *)&recvBuffer[0xc];
					maxCredit = *(u32 *)&recvBuffer[0x10];
					hopperSize = *(u32 *)&recvBuffer[0x14];
					maxBet = *(u32 *)&recvBuffer[0x18];
					minBet = *(u32 *)&recvBuffer[0x1c];
					addBet = *(u32 *)&recvBuffer[0x20];
				}
				std::array<u32, 9> payload{};
				payload[0] = freePlay | (autoPayOut << 8) | (twoWayMode << 16) | (medalExchRate << 24);
				payload[1] = autoExchange | (coinDiscrimination << 8) | (curBase << 16) | (betButton << 24);
				payload[2] = maxPay;
				payload[3] = maxCredit;
				payload[4] = hopperSize;
				payload[5] = maxBet;
				payload[6] = minBet;
				payload[7] = addBet;
				//payload[8] = currency;		// currency (lsb, 0:medal, 1:pound, 2:dollar, 3:euro, 4:token)
											// ?, ?, ?
				sendMessage(0x1a, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x2f: // TEST DATA MON
			{
				INFO_LOG(NAOMI, "hopper received cmd TEST DATA MON");
				// recv[4] == 2 (3 in coin test, 4 in error log)
				// recv[8] b0	payout lamp
				//		   b1   coin in lamp
				//		   b2	divider cash box side
				//		   b3	divider hopper side
				//		   b5	coin inhibit
				//         b6	cash inhibit
				// recv[c] hopper run
				// no reply
				break;
			}

			case 0x31: // COIN IN ACK?
			{
				INFO_LOG(NAOMI, "hopper received cmd 31");
				// no reply
				break;
			}

			case 0x32: // COIN OUT ACK?
			{
				INFO_LOG(NAOMI, "hopper received cmd 32");
				// no reply
				break;
			}

			default:
				WARN_LOG(NAOMI, "Unexpected hopper message: %x", command);
				break;
		}
	}

private:
	/*
	void sendStatusMessage()
	{
		std::array<u32, 3> payload{};
		payload[0] = 0;			// atp num
		payload[1] = status;	// status bitfield
		sendMessage(0, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}
	*/

	void sendCoinInMessage() override
	{
		insertCoin(100);
		std::array<u32, 7> payload{};
		payload[0] = 0;			// atp num
		payload[1] = status;
		payload[2] = credit0;
		payload[3] = credit1;
		payload[4] = totalCredit;
		payload[5] = premium;
		sendMessage(1, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}

	void sendCoinOutMessage() override
	{
		insertCoin(100);
		std::array<u32, 7> payload{};
		payload[0] = 0;			// atp num
		payload[1] = status;
		payload[2] = credit0;
		payload[3] = credit1;
		payload[4] = totalCredit;
		payload[5] = premium;
		sendMessage(2, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}

	void sendPayWinMessage() override
	{
		std::array<u32, 9> payload{};
		payload[0] = 0;			// atp num
		payload[1] = status;
		payload[2] = credit0;
		payload[3] = credit1;
		payload[4] = totalCredit;
		payload[5] = premium;
		payload[6] = wonAmount;
		payload[7] = paidAmount;
		sendMessage(3, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
	}

	void setDefaults()
	{
		switch (currency)
		{
		case 0:	// Medal
			medalExchRate = 5;
			twoWayMode = true;
			autoPayOut = false;
			coinDiscrimination = true;
			curBase = 100;
			betButton = true;
			maxPay = 1999900;
			maxCredit = 1999900;
			hopperSize = 39900;
			maxBet = 10000;
			minBet = 1000;
			addBet = 100;
			break;
		case 1:	// Pound
			medalExchRate = 0;
			twoWayMode = false;
			autoPayOut = true;
			coinDiscrimination = false;
			curBase = 5;
			betButton = false;
			maxPay = 4000;
			maxCredit = 1999900;
			hopperSize = 1999900;
			maxBet = 100;
			minBet = 100;
			addBet = 50;
			break;
		case 2:	// Dollar
		case 3:	// Euro
			medalExchRate = 0;
			twoWayMode = false;
			autoPayOut = false;
			coinDiscrimination = false;
			curBase = 5;
			betButton = true;
			maxPay = 1999900;
			maxCredit = 1999900;
			hopperSize = 39900;
			maxBet = 10000;
			minBet = 100;
			addBet = 100;
			break;
		case 4:	// Token
			medalExchRate = 0;
			twoWayMode = false;
			autoPayOut = false;
			coinDiscrimination = false;
			curBase = 100;
			betButton = true;
			maxPay = 1999900;
			maxCredit = 1999900;
			hopperSize = 39900;
			maxBet = 10000;
			minBet = 100;
			addBet = 100;
			break;
		default:
			WARN_LOG(NAOMI, "Unsupported currency %d", currency);
			break;
		}
		totalCredit = minBet;
	}

	// error:16
	// atpType:4
	// doorOpen:1
	// unk1:1
	// unk2:1
	// unk3:1
	// unk4:1
	const u32 status = 0x00000000;
};

static BaseHopper *hopper;

void init()
{
	term();
	if (settings.content.gameId == "KICK '4' CASH")
		hopper = new Sega837_14438Hopper();
	else
		hopper = new NaomiHopper();
	SCIFSerialPort::Instance().setPipe(hopper);
	config::ForceFreePlay.override(false);
}

void term()
{
	delete hopper;
	hopper = nullptr;
}

void serialize(Serializer& ser)
{
	if (hopper != nullptr)
		hopper->serialize(ser);
}

void deserialize(Deserializer& deser)
{
	if (hopper != nullptr)
		hopper->deserialize(deser);
}

}	// namespace hopper
