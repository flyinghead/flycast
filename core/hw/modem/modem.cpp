/*
	Originally based on nullDC: nullExtDev/modem.cpp

	Created on: Sep 9, 2018

	Copyright 2018 skmp, flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "modem.h"
#include "modem_regs.h"
#include "hw/holly/holly_intc.h"
#include "hw/sh4/sh4_sched.h"
#include "network/netservice.h"
#include "serialize.h"
#include "cfg/option.h"
#include "stdclass.h"
#include "v42.h"
#include <cassert>
#include <map>
#include <deque>

#define MODEM_COUNTRY_RES 0
#define MODEM_COUNTRY_JAP 1
#define MODEM_COUNTRY_USA 2

#define MODEM_MAKER_SEGA 0
#define MODEM_MAKER_ROCKWELL 1

#define MODEM_TYPE_336K 0
// according to XDP
#define MODEM_TYPE_WIRELESS 2

#define LOG(...) DEBUG_LOG(MODEM, __VA_ARGS__)

const static u32 MODEM_ID[2] =
{
	MODEM_COUNTRY_USA,
	(MODEM_MAKER_ROCKWELL << 4) | MODEM_TYPE_336K,
};

static modemreg_t modem_regs;

static u8 dspram[0x1000];
static_assert(sizeof(regs_write_mask) == sizeof(modem_regs));
static_assert(sizeof(por_dspram) == sizeof(dspram));

static int modem_sched;

enum ModemStates
{
	MS_INVALID,				//needs reset
	MS_RESET,				//reset is low
	MS_RESETING,			//reset is hi
	MS_ST_CONTROLER,		//Controller self test
	MS_ST_DSP,				//DSP self test
	MS_END_DSP,				//DSP self test end
	MS_NORMAL,	 			//Normal operation

};
static ModemStates state = MS_INVALID;

enum ConnectState
{
	DISCONNECTED,
	DIALING,
	RINGING,
	HANDSHAKING,
	PRE_CONNECTED,
	CONNECTED,
	NEGO_COMPLETE
};
static ConnectState connect_state = DISCONNECTED;
static float txFifoSize = 0.f;
static std::deque<u8> rxFifo;
static int cyclesPerByte;
static float txRxRatio = 1.f;

static void schedule_callback(int ms);
static const char *getDSPRamLabel(u32 addr);

static void update_interrupt()
{
	modem_regs.reg1e.RDBIA = modem_regs.reg1e.RDBIE && modem_regs.reg1e.RDBF;
	modem_regs.reg1e.TDBIA = modem_regs.reg1e.TDBIE && modem_regs.reg1e.TDBE;

	if (modem_regs.reg1f.NCIA || modem_regs.reg1f.NSIA || modem_regs.reg1e.RDBIA || modem_regs.reg1e.TDBIA)
		asic_RaiseInterrupt(holly_EXP_8BIT);
	else
		asic_CancelInterrupt(holly_EXP_8BIT);
}

static u32 get_masked_status(u32 reg)
{
	u8 int_mask = dspram[regs_int_mask_addr[reg]];
	return int_mask & modem_regs.ptr[reg];
}

#define SET_STATUS_BIT(reg, bit, value) do {		\
	if ((bit) != (value))							\
	{												\
		if (!modem_regs.reg1f.NSIE)					\
		{											\
			bit = (value);							\
		}											\
		else										\
		{											\
			u8 before = get_masked_status(reg);		\
			bit = (value);							\
			if (before != get_masked_status(reg))	\
				modem_regs.reg1f.NSIA = 1;			\
		}											\
	}												\
} while (false)

static void ControllerTestEnd();
static void DSPTestStart();
static void DSPTestEnd();

static u64 last_dial_time;

#ifndef NDEBUG
static u64 last_comm_stats;
static int sent_bytes;
static int recvd_bytes;
#endif

using namespace modem;

class NetIn : public InStream
{
public:
	int read() override {
		return net::modbba::readModem();
	}
	int available() override {
		return net::modbba::modemAvailable();
	}
};

class NetOut : public OutStream
{
public:
	void write(u8 v) override {
		net::modbba::writeModem(v);
	}
};

static NetIn netIn;
static NetOut netOut;
static InStream *curInput = &netIn;
static OutStream *curOutput = &netOut;

static V42Protocol v42Proto { netIn, netOut };

static bool v8bis;
static V8bisProtocol v8bisProto { netIn, netOut };

static void updateRxFifoStatus()
{
	// The rx fifo timeout set in dspram[0x32c]:2-4 doesn't seem to be used
	if (!rxFifo.empty())
	{
		modem_regs.RBUFFER = rxFifo.front();
		modem_regs.reg1e.RDBF = (u8)(rxFifo.size() >= rxFifoTrigger[(dspram[0x32c] >> 6) & 3]);
		if (modem_regs.reg04.FIFOEN)
			SET_STATUS_BIT(0x0c, modem_regs.reg0c.RXFNE, 1);
		SET_STATUS_BIT(0x01, modem_regs.reg01.RXHF, rxFifo.size() >= 8);
	}
	else
	{
		modem_regs.reg1e.RDBF = 0;
		if (modem_regs.reg04.FIFOEN)
			SET_STATUS_BIT(0x0c, modem_regs.reg0c.RXFNE, 0);
		SET_STATUS_BIT(0x01, modem_regs.reg01.RXHF, 0);
	}
}

static void configureStreams()
{
	if (v8bis) {
		curInput = &v8bisProto;
		curOutput = &v8bisProto;
	}
	else if (modem_regs.reg08.ASYN == 0) {
		curInput = &v42Proto;
		curOutput = &v42Proto;
	}
	else {
		curInput = &netIn;
		curOutput = &netOut;
	}
}

static bool setModemSpeedFromCONF()
{
	static constexpr int v90Speeds[] = {
		0, 28800, 29333, 30667, 32000, 33333, 34667, 36000, 37333, 38667, 40000, 41333,
		42667, 44000, 45333, 46667, 48000, 49333, 50667, 52000, 53333, 54667, 56000
	};
	static constexpr int v34Speeds[] = {
		0, 2400, 4800, 7200, 9600, 12000, 14400, 16800, 19200, 21600, 24000, 26400, 28800, 31200, 33600
	};
	modem_regs.reg0e.SPEED = 0;
	int speed;
	int rxSpeed = 0;
	switch (modem_regs.CONF)
	{
	default:
		if (modem_regs.CONF >= 0xe0 && modem_regs.CONF <= 0xf6)
		{
			// V.90
			rxSpeed = v90Speeds[modem_regs.CONF - 0xe0];
			if (rxSpeed == 0) {
				speed = 0;
			}
			else
			{
				dspram[0x2e4] = modem_regs.CONF - 0xe0;
				speed = 33600;
				modem_regs.reg0e.SPEED = 0x10;
				dspram[0x2e5] = 0xd;
			}
		}
		else if (modem_regs.CONF >= 0x90 && modem_regs.CONF <= 0x9d)
		{
			// K56flex
			if (modem_regs.CONF == 0x90) {
				speed = 0;
			}
			else
			{
				rxSpeed = 30000 + (modem_regs.CONF - 0x90) * 2000;
				speed = 33600;
				modem_regs.reg0e.SPEED = 0x10;
			}
		}
		else if (modem_regs.CONF >= 0xc0 && modem_regs.CONF <= 0xce)
		{
			// V.34
			speed = v34Speeds[modem_regs.CONF - 0xc0];
			if (speed != 0)
				modem_regs.reg0e.SPEED = modem_regs.CONF - 0xc0 + 2;
			dspram[0x2e4] = modem_regs.CONF - 0xc0;
			dspram[0x2e5] = dspram[0x2e4];
		}
		else {
			speed = 0;
		}
		break;

	// V.33
	case 0x31: speed = 14400; break;
	case 0x32: speed = 12000; break;
	case 0x34: speed = 9600; break;
	case 0x38: speed = 7200; break;
	// V.32 [bis]
	case 0x76: speed = 14400; break;
	case 0x72: speed = 12000; break;
	case 0x74: speed = 9600; break;
	case 0x75: speed = 9600; break;
	case 0x78: speed = 7200; break;
	case 0x71: speed = 4800; break;
	case 0x70: speed = 0; break;
	// V.17
	case 0xb1: speed = 14400; break;
	case 0xb2: speed = 12000; break;
	case 0xb4: speed = 9600; break;
	case 0xb8: speed = 7200; break;
	// V.29
	case 0x14: speed = 9600; break;
	case 0x12: speed = 7200; break;
	case 0x11: speed = 4800; break;
	// V.27 ter, V.22 bis, V.22, V.21
	case 0x02: speed = 4800; break;
	case 0x01: speed = 2400; break;
	case 0x84: speed = 2400; break;
	case 0x82: speed = 1200; break;
	case 0x52: speed = 1200; break;
	case 0x51: speed = 600; break;
	case 0xa0: speed = 300; break;
	case 0xa8: speed = 300; break;
	// Bell, V.23
	case 0x23: speed = 4800; break;
	case 0x62: speed = 1200; break;
	case 0x60: speed = 300; break;
	case 0xa4: speed = 1200; rxSpeed = 75; break;
	case 0xa1: speed = 75; rxSpeed = 1200; break;
	}

	if (rxSpeed == 0)
		rxSpeed = speed;
	if (speed == 0)
	{
		modem_regs.reg0e.SPEED = 0;
		cyclesPerByte = SH4_MAIN_CLOCK * 8 / 300;
		txRxRatio = 1.f;
	}
	else
	{
		if (modem_regs.reg0e.SPEED == 0)
		{
			switch (speed)
			{
			case 14400: modem_regs.reg0e.SPEED = 7; break;
			case 12000: modem_regs.reg0e.SPEED = 6; break;
			case 9600: modem_regs.reg0e.SPEED = 5; break;
			case 7200: modem_regs.reg0e.SPEED = 8; break;
			case 4800: modem_regs.reg0e.SPEED = 4; break;
			case 2400: modem_regs.reg0e.SPEED = 3; break;
			case 1200: modem_regs.reg0e.SPEED = 2; break;
			case 600: modem_regs.reg0e.SPEED = 1; break;
			default: modem_regs.reg0e.SPEED = 0; break;
			}
		}
		cyclesPerByte = SH4_MAIN_CLOCK * 8 / speed;
		txRxRatio = (float)speed / rxSpeed;
	}
	NOTICE_LOG(MODEM, "MODEM Connected %s %d/%d bps", modem_regs.reg08.ASYN ? "ASYNC" : "SYNC", rxSpeed, speed);
	return speed != 0;
}

static int modem_sched_func(int tag, int cycles, int jitter, void *arg)
{
#ifndef NDEBUG
	if (getTimeMs() - last_comm_stats >= 2000)
	{
		if (last_comm_stats != 0)
		{
			DEBUG_LOG(MODEM, "Stats sent %d (%.2f kB/s) received %d (%.2f kB/s) TDBE %d RDBF %d", sent_bytes, sent_bytes / 2000.0,
					recvd_bytes, recvd_bytes / 2000.0,
					modem_regs.reg1e.TDBE, modem_regs.reg1e.RDBF);
			sent_bytes = 0;
			recvd_bytes = 0;
		}
		last_comm_stats = getTimeMs();
	}
#endif
	int callback_cycles = 0;

	switch (state)
	{
	case MS_ST_CONTROLER:
		ControllerTestEnd();
		break;
	case MS_ST_DSP:
		DSPTestStart();
		break;
	case MS_END_DSP:
		DSPTestEnd();
		break;
	case MS_NORMAL:
		modem_regs.reg1f.NEWC = 0;		// Not needed when state is CONNECTED but who cares

		switch (connect_state)
		{
		case DIALING:
			if (last_dial_time != 0 && sh4_sched_now64() - last_dial_time >= SH4_MAIN_CLOCK)
			{
				LOG("Switching to RINGING state");
				connect_state = RINGING;
				schedule_callback(100);
			}
			else
			{
				last_dial_time = sh4_sched_now64();

				modem_regs.reg1e.TDBE = 1;
				schedule_callback(1000);	// To switch to Ringing state
			}
			break;
		case RINGING:
		case NEGO_COMPLETE:
			last_dial_time = 0;
			LOG("\t\t *** %s STATE ***", connect_state == RINGING ? "RINGING" : "NEGO COMPLETE");
			modem_regs.reg1f.NEWS = 1;
			if (!modem_regs.reg09.DATA && connect_state == RINGING)
			{
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.RI, 1);
				SET_STATUS_BIT(0x0b, modem_regs.reg0b.ATV25, 1);
			}
			break;
		case HANDSHAKING:
			LOG("\t\t *** HANDSHAKING STATE ***");
			if (modem_regs.CONF == 0xAA) {
				// V8 AUTO mode
				dspram[0x302] |= 1 << 3;				// ANSam detected
			}
			modem_regs.reg1f.NEWS = 1;
			SET_STATUS_BIT(0x0f, modem_regs.reg0f.RI, 0);
			SET_STATUS_BIT(0x0b, modem_regs.reg0b.ATV25, 0);

			callback_cycles = SH4_MAIN_CLOCK / 1000 * 500;		// 500 ms
			connect_state = PRE_CONNECTED;

			break;

		case PRE_CONNECTED:
			if (modem_regs.reg03.RLSDE)
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.RLSD, 1);
			bool validConfig;
			if (modem_regs.CONF == 0xAA)
			{
				// V8 AUTO mode
				dspram[0x302] |= 1 << 4;				// protocol octet received
				dspram[0x302] = (dspram[0x302] & 0x1f) | (dspram[0x304] & 0xe0);	// Received Call Function
				dspram[0x301] |= 1 << 4;				// JM detected
				dspram[0x303] |= 0xE1;					// Received protocol bits (?), Received GSTN Octet
				dspram[0x2e3] = 5;						// Symbol rate 3429
				// set the negotiated speed to the max configured by the game
				if ((dspram[0x6a3] & 1) == 1 && (dspram[0x6a2] & 0x20) == 0x20)
				{
					// V.90
					modem_regs.CONF = 0xE0 | (dspram[0x6a2] & 0x1F);
					dspram[0x6c1] = 0xff;		// V90 Server DPCM Transmit Data Rate Mask
					dspram[0x6c2] = 0xff;
					dspram[0x6c3] = 0x3f;
				}
				else {
					modem_regs.CONF = dspram[0x309];
				}
				validConfig = setModemSpeedFromCONF();
				dspram[0x239] = 12;						// RTD 0 @ 3429 sym rate
				if (modem_regs.reg1f.NSIE)
				{
					// CONF
					if (dspram[regs_int_mask_addr[0x12]] & (1 << 7))
						modem_regs.reg1f.NSIA = 1;
				}
			}
			else {
				validConfig = setModemSpeedFromCONF();
			}
			if (modem_regs.reg1f.NSIE)
			{
				// SPEED
				if (dspram[regs_int_mask_addr[0x0e]] & 0x1f)
					modem_regs.reg1f.NSIA = 1;
			}
			if (modem_regs.reg15.AUTO == 1) {
				modem_regs.reg09.DATA = 1;
				modem_regs.reg15.AUTO = 0;
			}
			if (!validConfig)
			{
				modem_regs.ABCODE = 1;	// (fake) FED turned off while waiting to get into round trip delay estimate (RTDE)
				connect_state = DISCONNECTED;
				// TODO proper failure
			}
			else
			{
				modem_regs.ABCODE = 0;	// no error
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.DSR, 1);
				if (modem_regs.reg02.v0.RTSDE)
					SET_STATUS_BIT(0x0f, modem_regs.reg0f.RTSDT, 1);

				// Energy detected. Required for games to detect the connection
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.FED, 1);
				net::modbba::start();
				connect_state = CONNECTED;
			}
			if (modem_regs.reg1f.NSIE)
			{
				// ABCODE
				if (dspram[regs_int_mask_addr[0x014]])
					modem_regs.reg1f.NSIA = 1;
			}
			modem_regs.reg1f.NEWS = 1;

			// V.34 Remote Modem Data Rate Capability
			dspram[0x208] = 0xff;	// 2.4 - 19.2 kpbs supported
			dspram[0x209] = 0xbf;	// 21.6 - 33.6 kpbs supported, asymmetric supported

			callback_cycles = SH4_MAIN_CLOCK / 1000000 * 238;	// 238 us
			v42Proto.reset();

			break;

		case CONNECTED:
#ifndef NDEBUG
			static bool mem_dumped;
			if (!mem_dumped)
			{
				mem_dumped = true;
				for (size_t i = 0 ; i < sizeof(modem_regs); i++)
					LOG("modem_regs %02x == %02x", i, modem_regs.ptr[i]);
			}
#endif
			callback_cycles = cyclesPerByte;
			txFifoSize = std::max(txFifoSize - txRxRatio, 0.f);
			modem_regs.reg1e.TDBE = txFifoSize == 0.f;

			if (v8bis && v8bisProto.completed()) {
				connect_state = NEGO_COMPLETE;
				v8bis = false;
				configureStreams();
			}
			if (rxFifo.size() < 16)
			{
				int c = curInput->read();
				if (c >= 0) {
					rxFifo.push_back(c);
					updateRxFifoStatus();
				}
			}
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}
	update_interrupt();

	return callback_cycles;
}

void ModemInit()
{
	modem_sched = sh4_sched_register(0, &modem_sched_func);
}

void ModemReset()
{
	net::modbba::stop();
}

void ModemTerm()
{
	ModemReset();
	sh4_sched_unregister(modem_sched);
	modem_sched = -1;
}

static void schedule_callback(int ms)
{
	sh4_sched_request(modem_sched, SH4_MAIN_CLOCK / 1000 * ms);
}

#define SetReg16(rh,rl,v) {modem_regs.ptr[rh]=(v)>>8;modem_regs.ptr[rl]=(v)&0xFF; }

static void NormalDefaultRegs()
{
	assert(state == MS_NORMAL);
	// Default values for normal state
	memset(&modem_regs, 0, sizeof(modem_regs));
	memcpy(dspram, por_dspram, sizeof(dspram));
	modem_regs.reg05.CEQ = 1;
	modem_regs.CONF = 0x76;	// CONF: V.32 bis TCM 14400
	modem_regs.reg09.DATA = 1;
	modem_regs.reg09.DTMF = 1;
	modem_regs.reg15.HWRWK = 1;
	modem_regs.reg07.RDLE = 1;
	modem_regs.reg15.RDWK = 1;
	modem_regs.reg03.RLSDE = 1;
	modem_regs.reg02.TDE = 1;
	modem_regs.reg13.TLVL = 0x9;

	modem_regs.reg1e.TDBE = 1;
	connect_state = DISCONNECTED;
	last_dial_time = 0;
	txFifoSize = 0.f;
	rxFifo.clear();
	v8bis = false;
	configureStreams();
}
static void DSPTestEnd()
{
	assert(state == MS_END_DSP);
	state = MS_NORMAL;

	LOG("DSPTestEnd");
	NormalDefaultRegs();
}
static void DSPTestStart()
{
	assert(state == MS_ST_DSP);
	state = MS_END_DSP;
	LOG("DSPTestStart");

	modem_regs.reg1e.TDBE = 1;
	SetReg16(0x1B,0x1A,0xF083);	// EC checksum
	SetReg16(0x19,0x18,0x46EE); // Multiplier checksum
	SetReg16(0x17,0x16,0x00FA); // RAM checksum
	SetReg16(0x15,0x14,0x0A09); // ROM checksum
	SetReg16(0x13,0x12,0x3730); // Part number
	SetReg16(0x11,0x00,0x2041); // Revision level

	schedule_callback(50);
}
static void ControllerTestEnd()
{
	assert(state == MS_ST_CONTROLER);
	state = MS_ST_DSP;

	schedule_callback(50);
}

//End the reset and start internal tests
static void ControllerTestStart()
{
	assert(state == MS_RESETING);
	//Set Self test values :)
	state = MS_ST_CONTROLER;
	//k, lets set values

	//1E:3 -> set
	modem_regs.reg1e.TDBE = 1;

/*
	RAM1 Checksum  = 0xEA3C or 0x451
	RAM2 Checksum  = 0x5536 or 0x49A5
	ROM1 Checksum  = 0x5F4C
	ROM2 Checksum  = 0x3835 or 0x3836
	Timer/ROM/RAM  = 0x801 or 0xDE00
	Part Number    = 0x3730 or 0x3731
	Revision Level = 0x4241
*/
	SetReg16(0x1D, 0x1C, 0xEA3C);
	SetReg16(0x1B, 0x1A, 0x5536);
	SetReg16(0x19, 0x18, 0x5F4C);
	SetReg16(0x17, 0x16, 0x3835);
	SetReg16(0x15, 0x14, 0x801);
	SetReg16(0x13, 0x12, 0x3730);
	SetReg16(0x11, 0x0, 0x4241);

	ControllerTestEnd();
}

static void modem_reset(u32 v)
{
	if (v == 0)
	{
		memset(&modem_regs, 0, sizeof(modem_regs));
		state = MS_RESET;
		LOG("Modem reset start ...");
		net::modbba::stop();
	}
	else
	{
		if (state == MS_RESET)
		{
			memset(&modem_regs, 0, sizeof(modem_regs));
			state = MS_RESETING;
			ControllerTestStart();
			INFO_LOG(MODEM, "MODEM Reset");
		}
		modem_regs.ptr[0x20] = v;
	}
}

static void check_start_handshake()
{
	if (modem_regs.reg09.DTR
			&& (modem_regs.reg09.DATA || modem_regs.reg15.AUTO)
			&& (connect_state == RINGING || connect_state == NEGO_COMPLETE))
	{
		LOG("DTR asserted. starting handshaking");
		connect_state = HANDSHAKING;
		schedule_callback(1);
	}
}

static bool word_dspram_write;
static bool module_download;
static u32 reg1b_save;
static u8 download_crc;

static void ModemNormalWrite(u32 reg, u32 data)
{
	//if (!module_download && reg != 0x10)
	//	LOG("ModemNormalWrite : %03X=%X", reg,data);
	u32 old = modem_regs.ptr[reg];
	modem_regs.ptr[reg] = (old & ~regs_write_mask[reg]) | (data & regs_write_mask[reg]);

	switch(reg)
	{
	case 0x02:	// TDE SQDIS S511/DCDEN CDEN RTSDE V54TE V54AE V54PE / CODBITS
		modem_regs.reg0f.RTSDT = modem_regs.reg02.v0.RTSDE && connect_state == CONNECTED;
		LOG("reg02 = %x", modem_regs.ptr[reg]);
		break;

	case 0x06:	// EXOS CCRTN HDLC PEN STB WDSZ/DECBITS
		LOG("PEN = %d", modem_regs.reg06.PEN);
		if (modem_regs.reg06.PEN)
			WARN_LOG(MODEM, "Parity not supported");
		if (modem_regs.reg06.HDLC)
			WARN_LOG(MODEM, "HDLC mode not supported");
		break;

	case 0x08:	// ASYN TPDM V21S V54T V54A V54P RTRN RTS
		LOG("TPDM = %d ASYN = %d V54T,A,P = %d,%d,%d RTS = %d", modem_regs.reg08.TPDM, modem_regs.reg08.ASYN,
				modem_regs.reg08.V54T, modem_regs.reg08.V54A, modem_regs.reg08.V54P, modem_regs.reg08.RTS);
		if ((old ^ data) & 0x80)
			// ASYN changed
			configureStreams();
		break;

	case 0x09:	// NV25 CC DTMF ORG LL DATA RRTSE DTR
		LOG("reg09 = %x", modem_regs.ptr[reg]);
		check_start_handshake();	// DTR and DATA
		if (connect_state == CONNECTED)
		{
			if (modem_regs.reg09.DTR) {
				LOG("DTR asserted");
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.DSR, 1);
			}
			else
			{
				LOG("DTR reset");
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.DSR, 0);
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.RLSD, 0);
			}
		}
		break;

	case 0x10:	// TBUFFER
		if (v8bis && modem_regs.reg08.RTS == 0) {
			// emit V8 bis tone
			v8bisProto.emitTone((u8)data);
		}
		else if (module_download)
		{
			download_crc = (download_crc << 1) + ((download_crc & 0x80) >> 7) + (data & 0xFF);
		}
		else if (connect_state == DISCONNECTED || connect_state == DIALING)
		{
			//LOG("ModemNormalWrite : TBUFFER = %X", data);
			if (connect_state == DISCONNECTED)
			{
				INFO_LOG(MODEM, "MODEM Dialing");
				connect_state = DIALING;
				modem_regs.SECRXB = 0;
			}
			schedule_callback(100);
		}
		else if (connect_state == CONNECTED && modem_regs.reg08.RTS)
		{
			//LOG("ModemNormalWrite : TBUFFER = %X", data);
#ifndef NDEBUG
			sent_bytes++;
#endif
			curOutput->write(data);

			modem_regs.reg1e.TDBE = 0;
			txFifoSize += 1.f;
			if (modem_regs.reg04.FIFOEN) {
				SET_STATUS_BIT(0x0d, modem_regs.reg0d.TXFNF, (u8)(txFifoSize <= 15.f));
				SET_STATUS_BIT(0x01, modem_regs.reg01.TXHF, (u8)(txFifoSize >= 8.f));
			}
			else {
				SET_STATUS_BIT(0x0d, modem_regs.reg0d.TXFNF, 0);
				SET_STATUS_BIT(0x01, modem_regs.reg01.TXHF, 1);
			}
		}
		break;

	case 0x11:	// BRKS PARSL TXV RXV V23HDX TEOF TXP
		LOG("PARSL = %d", modem_regs.reg11.PARSL);
		break;

	case 0x14:	// ABCODE
		LOG("ABCODE = %x", modem_regs.ptr[reg]);
		if (data == 0x4f || data == 0x5f)
		{
			reg1b_save = modem_regs.ptr[0x1b];
			modem_regs.ptr[0x1b] = data;
			//LOG("SPX/CTL download initiated");
			module_download = true;
			download_crc = 0;
		}
		else if (data == 0 && module_download)
		{
			// If module download is in progress, this signals the end of it
			modem_regs.SECTXB = 0xFF;
			modem_regs.SECRXB = download_crc;
			// Restore reg 1b
			modem_regs.ptr[0x1b] = reg1b_save;
			module_download = false;
			//LOG("SPX/CTL download finished CRC %02x", download_crc);
		}
		break;

	case 0x15:	// SLEEP STOP RDWK HWRWK AUTO RREN EXL3 EARC
		LOG("reg15 = %x", modem_regs.ptr[reg]);
		check_start_handshake();	// AUTO
		break;

		//Data Write Regs, for transfers to DSP
	case 0x18:	// MEDAL
		break;

	case 0x19:	// MEDAM
		word_dspram_write = true;
		break;

	case 0x1a:	// SFRES RIEN RION DMAE SCOBF SCIBE SECEN
		LOG("reg1a = %x", modem_regs.ptr[reg]);
		if (connect_state == CONNECTED && modem_regs.reg1a.SCIBE)
			WARN_LOG(MODEM, "Unexpected state: connected and SCIBE==1");
		break;

		//Address low
	case 0x1C:	// MEADDL
		break;

	case 0x1D:	// MEACC MEADDHB12 MEMW MEMCR MEADDH
		if (modem_regs.reg1c_1d.MEMW && !(old & (1 << 5)))
		{
			word_dspram_write = false;
		}
		if (modem_regs.reg1c_1d.MEACC)
		{
			modem_regs.reg1c_1d.MEACC = 0;
			modem_regs.reg1f.NEWS = 1;
			u32 dspram_addr = modem_regs.reg1c_1d.MEMADD_l | (modem_regs.reg1c_1d.MEMADD_h << 8);
			if (modem_regs.reg1c_1d.MEMW)
			{
				LOG("DSPRam Write<%d> %03x (%s) = %x", word_dspram_write ? 16 : 8,
						dspram_addr, getDSPRamLabel(dspram_addr),
						modem_regs.MEDA & (word_dspram_write ? 0xffff : 0xff));
				if (word_dspram_write)
				{
					if (dspram_addr & 1)
					{
						dspram[dspram_addr] = modem_regs.MEDA & 0xFF;
						dspram[dspram_addr + 1] = (modem_regs.MEDA >> 8) & 0xFF;
					}
					else {
						*(u16*)&dspram[dspram_addr] = modem_regs.MEDA;
					}
				}
				else {
					dspram[dspram_addr] = modem_regs.MEDA & 0xFF;
				}
				if ((dspram_addr == 0x26B || dspram_addr == 0x26c) && (modem_regs.MEDA & 0xFF) != 0)
					dspram[0x26F] = 1; // Saved Filtered EQM
			}
			else
			{
				if (dspram_addr & 1)
					modem_regs.MEDA = dspram[dspram_addr] | (dspram[dspram_addr + 1] << 8);
				else
					modem_regs.MEDA = *(u16*)&dspram[dspram_addr];
				LOG("DSPRam Read<16> %03x (%s) == %x", dspram_addr, getDSPRamLabel(dspram_addr), modem_regs.MEDA);
			}
		}
		break;
	case 0x1E:	// TDBIA RDBIA TDBIE TDBE RDBIE RDBF
		//LOG("TDBIE=%d RDBIE=%d", modem_regs.reg1e.TDBIE, modem_regs.reg1e.RDBIE);
		break;
	case 0x1F:	// NSIA NCIA NSIE NEWS NCIE NEWC
		if (old != modem_regs.ptr[reg])
			LOG("reg1f = %x", modem_regs.ptr[reg]);
		if (!modem_regs.reg1f.NCIE)
			modem_regs.reg1f.NCIA = 0;
		if (modem_regs.reg1f.NEWC)
		{
			if(modem_regs.reg1a.SFRES)
			{
				modem_regs.reg1a.SFRES = 0;
				LOG("Soft Reset SET && NEWC, executing reset and init");
				modem_reset(0);
				modem_reset(1);
				modem_regs.reg1f.NEWC = 1;
				modem_regs.ptr[0x20] = 0;
			}
			else
			{
				modem_regs.reg1f.NEWC = 0;	// accept the settings
				if (modem_regs.reg1f.NCIE)
					modem_regs.reg1f.NCIA = 1;
				LOG("NEWC CONF=%x", modem_regs.CONF);
			}
			if (modem_regs.CONF == 0x81 && !v8bis)
			{
				// V.8 bis detector
				v8bis = (dspram[0x439] & 1) == 1;
				if (v8bis) {
					v8bisProto.reset();
					configureStreams();
				}
			}
		}
		// Don't allow NEWS to be set if 0
		if ((old & (1 << 3)) == 0)
			modem_regs.reg1f.NEWS = 0;
		if (!modem_regs.reg1f.NEWS)
		{
			modem_regs.reg1f.NSIA = 0;
		}
		break;

	default:
		LOG("ModemNormalWrite: reg %02x = %x", reg, data);
		break;
	}
	update_interrupt();
}

u32 ModemReadMem_A0_006(u32 addr, u32 size)
{
	u32 reg = (addr & 0x7FF) >> 2;

	if (reg < 0x100)
		return MODEM_ID[reg & 1];

	reg -= 0x100;
	if (reg < 0x21)
	{
		switch (state)
		{
		case MS_NORMAL:
			{
				// Dial tone is detected if TONEA, TONEB and TONEC are set
				//if (reg==0xF)
				{
					SET_STATUS_BIT(0x0f, modem_regs.reg0f.CTS, modem_regs.reg08.RTS && connect_state == CONNECTED);
					SET_STATUS_BIT(0x0b, modem_regs.reg0b.TONEA, connect_state == DISCONNECTED || connect_state == NEGO_COMPLETE);
					SET_STATUS_BIT(0x0b, modem_regs.reg0b.TONEB, connect_state == DISCONNECTED || connect_state == NEGO_COMPLETE);
					SET_STATUS_BIT(0x0b, modem_regs.reg0b.TONEC, connect_state == DISCONNECTED || connect_state == NEGO_COMPLETE);
					if (modem_regs.reg04.FIFOEN || module_download) {
						SET_STATUS_BIT(0x0d, modem_regs.reg0d.TXFNF, (u8)(txFifoSize <= 15.f));
						SET_STATUS_BIT(0x01, modem_regs.reg01.TXHF, (u8)(txFifoSize >= 8.f));
					}
					else {
						SET_STATUS_BIT(0x0d, modem_regs.reg0d.TXFNF, (u8)(txFifoSize == 0.f));
						SET_STATUS_BIT(0x01, modem_regs.reg01.TXHF, (u8)(txFifoSize >= 1.f));
					}
				}
				u8 data = modem_regs.ptr[reg];
				if (reg == 0x00)	// RBUFFER
				{
					//LOG("Read RBUFFER = %X", data);
					if (!rxFifo.empty())
					{
						rxFifo.pop_front();
						updateRxFifoStatus();
						update_interrupt();
					}
				}
				else if (reg == 0x16)
				{
					if (v8bis)
					{
						if (connect_state == CONNECTED)
							data = 0;
						else
							data = v8bisProto.detectTone();
						modem_regs.SECRXB = data;
					}
					LOG("Read SECRXB == %x", data);
				}
				else if (reg == 0x17) {
					LOG("Read SECTXB == %x", data);
				}
				else {
					//LOG("Read reg%02x == %x", reg, data);
				}

				return data;
			}

		case MS_ST_CONTROLER:
		case MS_ST_DSP:
		case MS_END_DSP:
			if (reg == 0x10)
				// don't reset TBDE to help kos modem self test
				//modem_regs.reg1e.TDBE = 0;
				return 0;
			else
				return modem_regs.ptr[reg];

		case MS_RESETING:
			return 0; //still reset

		default:
			//LOG("Read (reset) reg %03x == %x", reg, modem_regs.ptr[reg]);
			return 0;
		}
	}

	LOG("modem reg %03X read -- wtf is it ?",reg);
	return 0;
}

void ModemWriteMem_A0_006(u32 addr, u32 data, u32 size)
{
	u32 reg = (addr & 0x7FF) >> 2;
	if (reg < 0x100)
	{
		LOG("modem reg %03X write -- MODEM ID?!",reg);
		return;
	}

	reg -= 0x100;
	if (reg < 0x20)
	{
		if (state == MS_NORMAL)
			ModemNormalWrite(reg,data);
		else
			LOG("modem reg %03X write %X -- undef state?", reg, data);
		return;
	}
	if (reg == 0x20)
	{
		//Hard reset
		modem_reset(data);
		return;
	}

	LOG("modem reg %03X write %X -- wtf is it?",reg,data);
}

void ModemSerialize(Serializer& ser)
{
	sh4_sched_serialize(ser, modem_sched);
	ser << modem_regs;
	ser << dspram;
	ser << state;
	ser << connect_state;
	ser << last_dial_time;
	ser << false;	// data_sent TODO get rid of this in the next savestate version
}
void ModemDeserialize(Deserializer& deser)
{
	if (!config::EmulateBBA || deser.version() > Deserializer::V31)
		sh4_sched_deserialize(deser, modem_sched);
	if (deser.version() >= Deserializer::V20)
	{
		deser >> modem_regs;
		deser >> dspram;
		deser >> state;
		deser >> connect_state;
		deser >> last_dial_time;
		bool data_sent;
		deser >> data_sent;
	}
	rxFifo.clear();
	v42Proto.reset();
	v8bis = false;
	v8bisProto.reset();
	configureStreams();
}

static const std::map<u32, const char *> dspRamDesc
{
	{ 0x26b,	"EQM Baud Interval" },
	{ 0x26C,	"Num EQM Samples" },
	{ 0x26F,	"Saved Filtered EQM" },
	{ 0x208,	"V.34 Remote Mode Data Rate Capability" },
	{ 0x209,	"V.34 Remote Mode Data Rate Capability 2" },
	{ 0x302,	"V.8 Status Bits 2" },
	{ 0x303,	"V.8 Status Bits 3" },
	{ 0x2e5,	"K56flex/V.34 Transmitter Speed" },
	{ 0x2e4,	"K56flex/V.34 Receiver Speed" },
	{ 0x239,	"Round Trip Far Echo Delay" },
	{ 0x2e3,	"V.34 Symbol Rate Value" },
	{ 0x247,	"NEWS Masking Register for 01" },
	{ 0x246,	"NEWS Masking Register for 0A" },
	{ 0x245,	"NEWS Masking Register for 0B" },
	{ 0x244,	"NEWS Masking Register for 0C" },
	{ 0x243,	"NEWS Masking Register for 0D" },
	{ 0x242,	"NEWS Masking Register for 0E" },
	{ 0x241,	"NEWS Masking Register for 0F" },
	{ 0x089,	"NEWS Masking Register for CONF" },
	{ 0x38a,	"NEWS Masking Register for ABCODE" },
	{ 0x370,	"NEWS Masking Register for SECRXB" },
	{ 0x371,	"NEWS Masking Register for SECTXB" },
	{ 0x27d,	"NEWS Masking Register for 1A" },
	{ 0x27c,	"NEWS Masking Register for 1B" },
	{ 0x32c,	"Receive FIFO Trigger Level" },
	{ 0x701,	"Receive FIFO Extension Enable" },
	{ 0x702,	"Transmit FIFO Extension Enable" },
	{ 0x309,	"V.34 Full-Duplex configuration" },
	{ 0x312,	"Modulation mode V23 full" },
	{ 0x313,	"Modulation mode V23 half" },
	{ 0x382,	"V.34 Data Rate Mask" },
	{ 0x383,	"V.34 Data Rate Mask 2" },

	{ 0x21e,	"Minimum Period of Valid Ring Signal" },
	{ 0x21f,	"Maximum Period of Valid Ring Signal" },
	{ 0xaa0,	"TONEA LPGAIN" },
	{ 0xaa1,	"TONEA Biquad1 A3" },
	{ 0xaa2,	"TONEA Biquad1 A2" },
	{ 0xaa3,	"TONEA Biquad1 A1" },
	{ 0xaa4,	"TONEA Biquad1 B2" },
	{ 0xaa5,	"TONEA Biquad1 B1" },
	{ 0xaa6,	"TONEB LPGAIN" },
	{ 0xaa7,	"TONEB Biquad1 A3" },
	{ 0xaa8,	"TONEB Biquad1 A2" },
	{ 0xaa9,	"TONEB Biquad1 A1" },
	{ 0xaaa,	"TONEB Biquad1 B2" },
	{ 0xaab,	"TONEB Biquad1 B1" },
	{ 0xaac,	"TONEC LPGAIN" },
	{ 0xaad,	"TONEC Biquad1 A3" },
	{ 0xaae,	"TONEC Biquad1 A2" },
	{ 0xaaf,	"TONEC Biquad1 A1" },
	{ 0xab0,	"TONEC Biquad1 B2" },
	{ 0xab1,	"TONEC Biquad1 B1" },
	{ 0xab2,	"PreFilter Biquad1 A3" },
	{ 0xab3,	"PreFilter Biquad1 A2" },
	{ 0xab4,	"PreFilter Biquad1 A1" },
	{ 0xab5,	"PreFilter Biquad1 B2" },
	{ 0xab6,	"PreFilter Biquad1 B1" },
	{ 0xab8,	"TONEA THRESHU" },
	{ 0xab9,	"TONEB THRESHU" },
	{ 0xaba,	"TONEC THRESHU" },

	{ 0xba0,	"TONEA LPFBK" },
	{ 0xba1,	"TONEA Biquad2 A3" },
	{ 0xba2,	"TONEA Biquad2 A2" },
	{ 0xba3,	"TONEA Biquad2 A1" },
	{ 0xba4,	"TONEA Biquad2 B2" },
	{ 0xba5,	"TONEA Biquad2 B1" },
	{ 0xba6,	"TONEB LPFBK" },
	{ 0xba7,	"TONEB Biquad2 A3" },
	{ 0xba8,	"TONEB Biquad2 A2" },
	{ 0xba9,	"TONEB Biquad2 A1" },
	{ 0xbaa,	"TONEB Biquad2 B2" },
	{ 0xbab,	"TONEB Biquad2 B1" },
	{ 0xbac,	"TONEC LPFBK" },
	{ 0xbad,	"TONEC Biquad2 A3" },
	{ 0xbae,	"TONEC Biquad2 A2" },
	{ 0xbaf,	"TONEC Biquad2 A1" },
	{ 0xbb0,	"TONEC Biquad2 B2" },
	{ 0xbb1,	"TONEC Biquad2 B1" },
	{ 0xbb2,	"PreFilter Biquad2 A3" },
	{ 0xbb3,	"PreFilter Biquad2 A2" },
	{ 0xbb4,	"PreFilter Biquad2 A1" },
	{ 0xbb5,	"PreFilter Biquad2 B2" },
	{ 0xbb6,	"PreFilter Biquad2 B1" },
	{ 0xbb8,	"TONEA THRESHL" },
	{ 0xbb9,	"TONEB THRESHL" },
	{ 0xbba,	"TONEC THRESHL" },

	{ 0x29c,	"DTMF Low Band Power Level" },
	{ 0x29b,	"DTMF Low Band Power Level 2" },
	{ 0x29e,	"DTMF High Band Power Level" },
	{ 0x29d,	"DTMF High Band Power Level 2" },
	{ 0x218,	"DTMF Tone Duration 2" },
	{ 0x2db,	"DTMF Tone Duration" },
	{ 0x219,	"DTMF Interdigit Delay 2" },
	{ 0x2dc,	"DTMF Interdigit Delay" },
	{ 0x304,	"V.8 Control Register 1" },
	{ 0x305,	"V.8 Control Register 2" },
	{ 0x306,	"V.8 Control Register 3" },
	{ 0x307,	"V.8 Control Register 4" },
	{ 0x308,	"V.8 Control Register 5" },
	{ 0x13f,	"V.34 Asymmetric Data Rates Enable / No Automode to FSK" },
	{ 0x100,	"V.34 PREDIS and TLDDIS" },
	{ 0x105,	"V.34 Spectral Parameters Control" },
	{ 0x101,	"V.34 Baud Rate Mask (BRM)" },
	{ 0x2c1,	"V.32/V.32 bis R1 Mask" },
	{ 0x2c0,	"V.32/V.32 bis R1 Mask 2" },
	{ 0x2c3,	"V.32/V.32 bis R2 Mask" },
	{ 0x2c2,	"V.32/V.32 bis R2 Mask 2" },
	{ 0x2c5,	"V.32 bis R4 Mask" },
	{ 0x2c4,	"V.32 bis R4 Mask 2" },
	{ 0x2c7,	"V.32 bis R5 Mask" },
	{ 0x2c6,	"V.32 bis R5 Mask 2" },
	{ 0x6a2,	"Desired V.90 Receive Speed" },
	{ 0x6a3,	"V.90 Host Control" },
	{ 0x3db,	"Transmitter Output Level Gain (G) - All Modes" },
	{ 0x3da,	"Transmitter Output Level Gain (G) - All Modes 2" },
	{ 0xb57,	"Transmitter Output Level Gain (G) - FSK Modes" },
	{ 0x3a5,	"ARA-in-RAM Enable" },
	{ 0x10d,	"V.21/V.23 CTS Mark Qualify / RLSD Overwrite Control / Extended RTH Control" },
	{ 0x6c1,	"V.90 Server DPCM Tx Data Rate Mask 1" },
	{ 0x6c2,	"V.90 Server DPCM Tx Data Rate Mask 2" },
	{ 0x6c3,	"V.90 Server DPCM Tx Data Rate Mask 3" },
	{ 0x09f,	"Available modulation types?" },
	{ 0x439,	"V.8bis Detector / Host AudioSpan" },
	{ 0x3e1,	"Voice path control bits" },
	{ 0x20c,	"Eye Quality Monitor" },
	{ 0x810,	"V.90/K56 Eye Quality Monitor" },
	{ 0x202,	"RTS-CTS Delay" },
	{ 0x203,	"RTS-CTS Delay 2" },
	{ 0x270,	"RLSD Drop Out Timer" },
	{ 0x271,	"RLSD Drop Out Timer 2" },
};

static const char *getDSPRamLabel(u32 addr)
{
	auto it = dspRamDesc.find(addr);
	if (it == dspRamDesc.end())
		return "?";
	else
		return it->second;
}
