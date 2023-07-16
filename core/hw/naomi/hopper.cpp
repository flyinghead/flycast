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

#include <array>
#include <vector>
#include <deque>

namespace hopper
{

class BaseHopper : public SerialPipe
{
public:
	BaseHopper() {
		schedId = sh4_sched_register(0, schedCallback, this);
		sh4_sched_request(schedId, SCHED_CYCLES);
	}
	virtual ~BaseHopper() {
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
	
	virtual void serialize(Serializer& ser)
	{
		ser << (u32)recvBuffer.size();
		ser.serialize(recvBuffer.data(), recvBuffer.size());
		ser << credit0;
		ser << credit1;
		ser << totalCredit;
		ser << premium;
		ser << gameNumber;
		ser << freePlay;
		ser << autoPayOut;
		ser << autoExchange;
		ser << twoWayMode;
		ser << coinDisc;
		ser << currency;
		ser << medalExchRate;
		ser << maxHopperFloat;
		ser << maxPay;
		ser << maxCredit;
		ser << hopperSize;
		ser << maxBet;
		ser << minBet;
		ser << addBet;
		ser << expectedBytes;
		ser << (u32)toSend.size();
		for (u8 b : toSend)
			ser << b;
		ser << started;
		sh4_sched_serialize(ser, schedId);
	}
	
	virtual void deserialize(Deserializer& deser)
	{
		u32 size;
		deser >> size;
		recvBuffer.resize(size);
		deser.deserialize(recvBuffer.data(), size);
		deser >> credit0;
		deser >> credit1;
		deser >> totalCredit;
		deser >> premium;
		deser >> gameNumber;
		deser >> freePlay;
		deser >> autoPayOut;
		deser >> autoExchange;
		deser >> twoWayMode;
		deser >> coinDisc;
		deser >> currency;
		deser >> medalExchRate;
		deser >> maxHopperFloat;
		deser >> maxPay;
		deser >> maxCredit;
		deser >> hopperSize;
		deser >> maxBet;
		deser >> minBet;
		deser >> addBet;
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

protected:
	virtual void handleMessage(u8 command) = 0;
	virtual void sendCoinInMessage() = 0;

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
		serial_updateStatusRegister();
	}

	std::vector<u8> recvBuffer;
	u32 credit0 = 0;
	u32 credit1 = 0;
	u32 totalCredit = 0;
	u32 premium = 0;
	u32 gameNumber = 0;
	bool freePlay = false;
	bool autoPayOut = false;
	bool autoExchange = false;
	bool twoWayMode = true;
	bool coinDisc = true;
	u8 currency = 0;	// 0:medal, 1:pound, 2:dollar, 3:euro, 4:token
	u8 medalExchRate = 5;
	u32 maxHopperFloat = 200;
	u32 maxPay = 1999900;
	u32 maxCredit = 1999900;
	u32 hopperSize = 39900;
	u32 maxBet = 10000;
	u32 minBet = 100; // normally 1000
	u32 addBet = 100;

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
				std::array<u32, 0x1ea> payload{};
				payload[0] = gameNumber;	// game number
				payload[1] = 0;         	// atp num
				payload[2] = 0;         	// atp kind and error code
				payload[3] = status;		// status bitfield. rectangle in background if != 0
				payload[4] = freePlay | (autoPayOut << 8) | (twoWayMode << 16) | (medalExchRate << 24);
				payload[5] = autoExchange | (coinDisc << 8);
				payload[6] = 0;				// ? 8c027124
				payload[7] = maxPay;
				payload[8] = maxCredit;
				payload[9] = hopperSize;
				payload[10] = maxBet;
				payload[11] = minBet;
				payload[12] = addBet;
				payload[13] = currency;		// currency (lsb, 0:medal, 1:pound, 2:dollar, 3:euro, 4:token)
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
				payload[0x2d] = 0;		// wins
				payload[0x2e] = 0;		// ? 8c02717c
				payload[0x2f] = 0;		// ? 8c027180
				payload[0x30] = 0;		// ? 8c027188
				payload[0x31] = 0;		// ? 8c02718c
				payload[0x32] = 0;		// paid? 8c027190
				
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
				payload[0x1c0] = 0x5010000;	// medal exch rate (5), 2-way (1), autoPO (0), free play (0)
				payload[0x1c1] = 0x1640100;	// ? (1), ? (100), coin disc (1), auto exch (0)
				payload[0x1c3] = 1999900;	// max pay
				payload[0x1c4] = 1999900;	// max credit
				payload[0x1c5] = 39900;		// hopper size
				payload[0x1c6] = 10000;		// max bet
				payload[0x1c7] = 1000;		// min bet
				payload[0x1c8] = 100;		// add bet
				payload[0x1c9] = 100;
				payload[0x1ca] = 10000;
				payload[0x1cb] = 100 | (499 << 16);
				payload[0x1cc] = 500 | (499 << 16);
				payload[0x1cd] = 500 | (49 << 16);
				payload[0x1ce] = 50;
				payload[0x1cf] = 200;
				payload[0x1d3] = 2000 | (200 << 16);
				payload[0x1d4] = 100;
				payload[0x1d6] = 100;
				payload[0x1da] = 100 | (200 << 16);
				payload[0x1db] = 300 | (400 << 16);
				payload[0x1dc] = 500 | (1000 << 16);
				payload[0x1dd] = 2000 | (2500 << 16);
				payload[0x1de] = 5000 | (10000 << 16);
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
				// 4: credit used?
				credit0 -= *(u32 *)&recvBuffer[4];
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
					coinDisc = recvBuffer[9] & 1;
					// 0xc ??
					maxPay = *(u32 *)&recvBuffer[0x10];
					maxCredit = *(u32 *)&recvBuffer[0x14];
					hopperSize = *(u32 *)&recvBuffer[0x18];
					maxBet = *(u32 *)&recvBuffer[0x1c];
					minBet = *(u32 *)&recvBuffer[0x20];
					addBet = *(u32 *)&recvBuffer[0x24];
					currency = recvBuffer[0x28]; // ??
				}
				std::array<u32, 0xa> payload{};
				payload[0] = freePlay | (autoPayOut << 8) | (twoWayMode << 16) | (medalExchRate << 24);
				payload[1] = autoExchange | (coinDisc << 8);
				payload[2] = 0;				// ? 8c027124
				payload[3] = maxPay;
				payload[4] = maxCredit;
				payload[5] = hopperSize;
				payload[6] = maxBet;
				payload[7] = minBet;
				payload[8] = addBet;
				payload[9] = currency;		// currency (lsb, 0:medal, 1:pound, 2:dollar, 3:euro, 4:token)
										// ? 8c027141, 8c027142, 8c027143
				sendMessage(0x2b, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
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
		credit0 += 100;
		totalCredit += 100;
		std::array<u32, 8> payload{};
		payload[0] = 0;         // atp num
		payload[1] = 0;         // atp kind and error code
		payload[2] = status;
		payload[3] = credit0;
		payload[4] = credit1;
		payload[5] = totalCredit;
		payload[5] = premium;
		sendMessage(1, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
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

				INFO_LOG(NAOMI, "hopper received POWER ON");
				std::array<u32, 0x13a> payload{};
				payload[0] = gameNumber;
				payload[1] = 0;			// atp num
				payload[2] = status;	// status bitfield. rectangle in background if != 0
				payload[3] = freePlay | (autoPayOut << 8) | (twoWayMode << 16) | (medalExchRate << 24);
				payload[4] = autoExchange | (coinDisc << 8);
				payload[5] = maxPay;
				payload[6] = maxCredit;
				payload[7] = hopperSize;
				payload[8] = maxBet;
				payload[9] = minBet;
				payload[10] = addBet;
				payload[11] = 0;		// ? BYTE_8c0c9fa8
				payload[12] = credit0;
				payload[13] = credit1;
				payload[14] = totalCredit;
				payload[15] = premium;
				payload[16] = 1;		// min count
				payload[17] = 666;		// paid amount
				
				/* doesn't seem to matter */
				payload[18] = ~0;
				payload[19] = ~0;
				payload[20] = ~0;
				payload[21] = ~0;
				payload[22] = ~0;
				payload[23] = ~0;
				payload[24] = ~0;

				payload[25] = ~0;        // ?
				payload[26] = ~0;        // ?
				payload[27] = ~0;        // ?
				// don't care?
				payload[28] = ~0;
				payload[29] = ~0;        // ?
				payload[30] = ~0;        // ?
				payload[31] = ~0;        // ?
				// don't care?
				payload[32] = ~0;
				size_t idx = 0x84;
				for (size_t i = 0; i < std::size(hopperErrors); i++) {
					memcpy((u8 *)payload.data() + idx, hopperErrors[i], sizeof(hopperErrors[i]));
					idx += 32;
				}
				// power down in game if lsb != 0
				payload[0x139] = 1;
				sendMessage(0x10, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);

				started = true;
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
				// 4: credit used?
				credit0 -= *(u32 *)&recvBuffer[4];
				std::array<u32, 0x17> payload{};
				payload[0] = ++gameNumber;
				payload[1] = 0;				// atp num
				payload[2] = status;
				payload[3] = credit0;
				payload[4] = credit1;
				payload[5] = totalCredit;
				payload[6] = premium;
				sendMessage(0x12, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				break;
			}

			case 0x23:	// GAME END
			{
				INFO_LOG(NAOMI, "hopper received GAME END");
				// 8: amount to pay?
				//credit0 += *(u32 *)&recvBuffer[8];
				std::array<u32, 0x15> payload{};
				payload[0] = 1;	// gameNum, upper 16 bits: ? 0, -1 or -2
				payload[1] = credit0;
				payload[2] = credit1;
				payload[3] = totalCredit;
				payload[4] = premium;
				sendMessage(0x13, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
				// TODO send PAY WIN message?
				break;
			}

			case 0x29:	// SWITCH
			{
				INFO_LOG(NAOMI, "hopper received SWITCH");
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
					coinDisc = recvBuffer[9] & 1;
					maxPay = *(u32 *)&recvBuffer[0xc];
					maxCredit = *(u32 *)&recvBuffer[0x10];
					hopperSize = *(u32 *)&recvBuffer[0x14];
					maxBet = *(u32 *)&recvBuffer[0x18];
					minBet = *(u32 *)&recvBuffer[0x1c];
					addBet = *(u32 *)&recvBuffer[0x20];
					//currency = recvBuffer[0x24]; // ??
				}
				std::array<u32, 9> payload{};
				payload[0] = freePlay | (autoPayOut << 8) | (twoWayMode << 16) | (medalExchRate << 24);
				payload[1] = autoExchange | (coinDisc << 8);
				payload[2] = maxPay;
				payload[3] = maxCredit;
				payload[4] = hopperSize;
				payload[5] = maxBet;
				payload[6] = minBet;
				payload[7] = addBet;
				payload[8] = currency;		// currency (lsb, 0:medal, 1:pound, 2:dollar, 3:euro, 4:token)
											// ?, ?, ?
				sendMessage(0x1a, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
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
		credit0 += 100;
		totalCredit += 100;
		std::array<u32, 7> payload{};
		payload[0] = 0;			// atp num
		payload[1] = status;
		payload[2] = credit0;
		payload[3] = credit1;
		payload[4] = totalCredit;
		payload[5] = premium;
		sendMessage(1, (const u8 *)payload.data(), payload.size() * sizeof(u32) - 1);
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

static BaseHopper *pipe;

void init()
{
	term();
	if (settings.content.gameId == "KICK '4' CASH")
		pipe = new Sega837_14438Hopper();
	else
		pipe = new NaomiHopper();
	serial_setPipe(pipe);
}

void term()
{
	delete pipe;
	pipe = nullptr;
}

void serialize(Serializer& ser)
{
	if (pipe != nullptr)
		pipe->serialize(ser);
}

void deserialize(Deserializer& deser)
{
	if (pipe != nullptr)
		pipe->deserialize(deser);
}

}	// namespace hopper
