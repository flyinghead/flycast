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
#include "oslib/oslib.h"
#include "network/picoppp.h"
#include "serialize.h"

#define MODEM_COUNTRY_RES 0
#define MODEM_COUNTRY_JAP 1
#define MODEM_COUNTRY_USA 2

#define MODEM_MAKER_SEGA 0
#define MODEM_MAKER_ROCKWELL 1

#define MODEM_DEVICE_TYPE_336K 0

#define LOG(...) DEBUG_LOG(MODEM, __VA_ARGS__)

const static u32 MODEM_ID[2] =
{
	MODEM_COUNTRY_RES,
	(MODEM_MAKER_ROCKWELL<<4) | (MODEM_DEVICE_TYPE_336K),
};

static modemreg_t modem_regs;

static u8 dspram[0x1000];

int modem_sched;

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
};
static ConnectState connect_state = DISCONNECTED;

static void schedule_callback(int ms);

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
} while (false);

static void ControllerTestEnd();
static void DSPTestStart();
static void DSPTestEnd();

static u64 last_dial_time;
static bool data_sent;

#ifndef NDEBUG
static double last_comm_stats;
static int sent_bytes;
static int recvd_bytes;
static FILE *recv_fp;
static FILE *sent_fp;
#endif

static int modem_sched_func(int tag, int cycles, int jitter)
{
#ifndef NDEBUG
	if (os_GetSeconds() - last_comm_stats >= 2)
	{
		if (last_comm_stats != 0)
		{
			DEBUG_LOG(MODEM, "Stats sent %d (%.2f kB/s) received %d (%.2f kB/s) TDBE %d RDBF %d\n", sent_bytes, sent_bytes / 2000.0,
					recvd_bytes, recvd_bytes / 2000.0,
					modem_regs.reg1e.TDBE, modem_regs.reg1e.RDBF);
			sent_bytes = 0;
			recvd_bytes = 0;
		}
		last_comm_stats = os_GetSeconds();
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
			if (last_dial_time != 0 && sh4_sched_now64() - last_dial_time >= (u64)(SH4_MAIN_CLOCK + jitter))
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
			last_dial_time = 0;
			LOG("\t\t *** RINGING STATE ***");
			modem_regs.reg1f.NEWS = 1;
			if (!modem_regs.reg09.DATA)
			{
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.RI, 1);
				SET_STATUS_BIT(0x0b, modem_regs.reg0b.ATV25, 1);
			}
			break;
		case HANDSHAKING:
			LOG("\t\t *** HANDSHAKING STATE ***");
			if (modem_regs.reg12 == 0xAA)
			{
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
			INFO_LOG(MODEM, "MODEM Connected");
			if (modem_regs.reg03.RLSDE)
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.RLSD, 1);
			if (modem_regs.reg12 == 0xAA)
			{
				// V8 AUTO mode
				dspram[0x302] |= 1 << 4;				// protocol octet received
				dspram[0x302] = (dspram[0x302] & 0x1f) | (dspram[0x304] & 0xe0);	// Received Call Function
				dspram[0x301] |= 1 << 4;				// JM detected
				dspram[0x303] |= 0xE0;					// Received protocol bits (?)
				dspram[0x2e3] = 5;						// Symbol rate 3429
				dspram[0x2e4] = 0xe;					// V.34 Receiver Speed 33.6
				dspram[0x2e5] = 0xe;					// V.34 Transmitter Speed 33.6
				dspram[0x239] = 12;						// RTD 0 @ 3429 sym rate
				if (modem_regs.reg08.ASYN)
				{
					modem_regs.reg12 = 0xce;		// CONF V34 - K56flex
					modem_regs.reg0e.SPEED = 0x10;	// 33.6k
				}
				else
				{
					// Force the driver to ASYN=1 so it sends raw data
					modem_regs.reg12 = 0xa1;		// CONF V23 75 TX/1200 RX
					modem_regs.reg0e.SPEED = 0x02;	// 1.2k
				}
				if (modem_regs.reg1f.NSIE)
				{
					// CONF
					if (dspram[regs_int_mask_addr[0x12]] & (1 << 7))
						modem_regs.reg1f.NSIA = 1;
					// SPEED
					if (dspram[regs_int_mask_addr[0x0e]] & 0x1f)
						modem_regs.reg1f.NSIA = 1;
				}
				modem_regs.reg09.DATA = 1;
				modem_regs.reg15.AUTO = 0;
			}
			modem_regs.reg14 = 0x00;			// ABCODE: no error
			if (modem_regs.reg1f.NSIE)
			{
				// ABCODE
				if (dspram[regs_int_mask_addr[0x014]])
					modem_regs.reg1f.NSIA = 1;
			}
			modem_regs.reg1f.NEWS = 1;
			SET_STATUS_BIT(0x0f, modem_regs.reg0f.DSR, 1);
			if (modem_regs.reg02.v0.RTSDE)
				SET_STATUS_BIT(0x0f, modem_regs.reg0f.RTSDT, 1);

			// Energy detected. Required for games to detect the connection
			SET_STATUS_BIT(0x0f, modem_regs.reg0f.FED, 1);

			// V.34 Remote Modem Data Rate Capability
			dspram[0x208] = 0xff;	// 2.4 - 19.2 kpbs supported
			dspram[0x209] = 0xbf;	// 21.6 - 33.6 kpbs supported, asymmetric supported

			start_pico();
			connect_state = CONNECTED;
			callback_cycles = SH4_MAIN_CLOCK / 1000000 * 238;	// 238 us
			data_sent = false;

			break;

		case CONNECTED:
#ifndef NDEBUG
			static bool mem_dumped;
			if (!mem_dumped)
			{
				mem_dumped = true;
				for (int i = 0 ; i < sizeof(modem_regs); i++)
					LOG("modem_regs %02x == %02x", i, modem_regs.ptr[i]);
			}
#endif
			// This value is critical. Setting it too low will cause some sockets to stall.
			// Check Sonic Adventure 2 and Samba de Amigo (PAL) integrated browsers.
			// 143 us/bytes corresponds to 56K
			callback_cycles = SH4_MAIN_CLOCK / 1000000 * 143;
			modem_regs.reg1e.TDBE = 1;

			// Let WinCE send data first to avoid choking it
			if (!modem_regs.reg1e.RDBF && data_sent)
			{
				int c = read_pico();
				if (c >= 0)
				{
					//LOG("pppd received %02x", c);
#ifndef NDEBUG
					recvd_bytes++;
#endif
					modem_regs.reg00 = c & 0xFF;
					modem_regs.reg1e.RDBF = 1;
					if (modem_regs.reg04.FIFOEN)
						SET_STATUS_BIT(0x0c, modem_regs.reg0c.RXFNE, 1);
					SET_STATUS_BIT(0x01, modem_regs.reg01.RXHF, 1);
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
	stop_pico();
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
	verify(state == MS_NORMAL);
	verify(sizeof(regs_write_mask) == sizeof(modem_regs));
	verify(sizeof(por_dspram) == sizeof(dspram));

	// Default values for normal state
	memset(&modem_regs, 0, sizeof(modem_regs));
	memcpy(dspram, por_dspram, sizeof(dspram));
	modem_regs.reg05.CEQ = 1;
	modem_regs.reg12 = 0x76;	// CONF: V.32 bis TCM 14400
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
}
static void DSPTestEnd()
{
	verify(state==MS_END_DSP);
	state=MS_NORMAL;

	LOG("DSPTestEnd");
	NormalDefaultRegs();
}
static void DSPTestStart()
{
	verify(state==MS_ST_DSP);
	state=MS_END_DSP;
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
	verify(state==MS_ST_CONTROLER);
	state=MS_ST_DSP;

	schedule_callback(50);
}

//End the reset and start internal tests
static void ControllerTestStart()
{
	verify(state==MS_RESETING);
	//Set Self test values :)
	state=MS_ST_CONTROLER;
	//k, lets set values

	//1E:3 -> set
	modem_regs.reg1e.TDBE=1;

/*
	RAM1 Checksum  = 0xEA3C or 0x451
	RAM2 Checksum  = 0x5536 or 0x49A5
	ROM1 Checksum  = 0x5F4C
	ROM2 Checksum  = 0x3835 or 0x3836
	Timer/ROM/RAM  = 0x801 or 0xDE00
	Part Number    = 0x3730 or 0x3731
	Revision Level = 0x4241
*/
	SetReg16(0x1D,0x1C,0xEA3C);
	SetReg16(0x1B,0x1A,0x5536);
	SetReg16(0x19,0x18,0x5F4C);
	SetReg16(0x17,0x16,0x3835);
	SetReg16(0x15,0x14,0x801);
	SetReg16(0x13,0x12,0x3730);
	SetReg16(0x11,0x0,0x4241);

	ControllerTestEnd();
}

static void modem_reset(u32 v)
{
	if (v == 0)
	{
		memset(&modem_regs, 0, sizeof(modem_regs));
		state = MS_RESET;
		LOG("Modem reset start ...");
	}
	else
	{
		if (state == MS_RESET)
		{
			stop_pico();
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
			&& connect_state == RINGING)
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
#ifndef NDEBUG
	if (recv_fp == NULL)
	{
		recv_fp = fopen("ppp_recv.dump", "w");
		sent_fp = fopen("ppp_sent.dump", "w");
	}
#endif
	//if (!module_download && reg != 0x10)
	//	LOG("ModemNormalWrite : %03X=%X", reg,data);
	u32 old = modem_regs.ptr[reg];
	modem_regs.ptr[reg] = (old & ~regs_write_mask[reg]) | (data & regs_write_mask[reg]);

	switch(reg)
	{
	case 0x02:
		modem_regs.reg0f.RTSDT = modem_regs.reg02.v0.RTSDE && connect_state == CONNECTED;
		break;

	case 0x06:
		LOG("PEN = %d", modem_regs.reg06.PEN);
		if (modem_regs.reg06.PEN)
			die("PEN = 1");
		if (modem_regs.reg06.HDLC)
			die("HDLC = 1");
		break;

	case 0x08:
		LOG("TPDM = %d ASYN = %d", modem_regs.reg08.TPDM, modem_regs.reg08.ASYN);
		break;

	case 0x09:
		check_start_handshake();	// DTR and DATA
		break;

	case 0x10:	// TBUFFER
		if (module_download)
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
			}
			schedule_callback(100);
		}
		else if (connect_state == CONNECTED && modem_regs.reg08.RTS)
		{
			//LOG("ModemNormalWrite : TBUFFER = %X", data);
			data_sent = true;
#ifndef NDEBUG
			sent_bytes++;
			if (sent_fp)
				fputc(data, sent_fp);
#endif
			write_pico(data);
			modem_regs.reg1e.TDBE = 0;
		}
		break;

	case 0x11:
		LOG("PARSL = %d", modem_regs.reg11.PARSL);
		die("PARSL");
		break;

	case 0x14:	// ABCODE
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
			modem_regs.reg17 = 0xFF;
			modem_regs.reg16 = download_crc;
			// Restore reg 1b
			modem_regs.ptr[0x1b] = reg1b_save;
			module_download = false;
			//LOG("SPX/CTL download finished CRC %02x", download_crc);
		}
		break;

	case 0x15:
		check_start_handshake();	// AUTO
		break;

		//Data Write Regs, for transfers to DSP
	case 0x18:	// MEDAL
		break;

	case 0x19:	// MEDAM
		word_dspram_write = true;
		break;

	case 0x1a:
		verify(connect_state != CONNECTED || !modem_regs.reg1a.SCIBE);
		break;

		//Address low
	case 0x1C:	// MEADDL
		break;

	case 0x1D:	// MEADDH
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
				LOG("DSP mem Write%s address %08x = %x", word_dspram_write ? " (w)" : "", dspram_addr, modem_regs.reg18_19);
				if (word_dspram_write)
				{
					if (dspram_addr & 1)
					{
						dspram[dspram_addr] = modem_regs.reg18_19 & 0xFF;
						dspram[dspram_addr + 1] = (modem_regs.reg18_19 >> 8) & 0xFF;
					}
					else
						*(u16*)&dspram[dspram_addr] = modem_regs.reg18_19;
				}
				else
					dspram[dspram_addr] = modem_regs.reg18_19 & 0xFF;
			}
			else
			{
				if (dspram_addr & 1)
					modem_regs.reg18_19 = dspram[dspram_addr] | (dspram[dspram_addr + 1] << 8);
				else
					modem_regs.reg18_19 = *(u16*)&dspram[dspram_addr];
				LOG("DSP mem Read address %08x == %x", dspram_addr, modem_regs.reg18_19 );
			}
		}
		break;

	case 0x1F:
		if (!modem_regs.reg1f.NCIE)
			modem_regs.reg1f.NCIA = 0;
		if (modem_regs.reg1f.NEWC)
		{
			if(modem_regs.reg1a.SFRES)
			{
				modem_regs.reg1a.SFRES = 0;
				LOG("Soft Reset SET && NEWC, executing reset and init");
				modem_reset(1);
			}
			else
			{
				modem_regs.reg1f.NEWC = 0;	// accept the settings
				if (modem_regs.reg1f.NCIE)
					modem_regs.reg1f.NCIA = 1;
				LOG("NEWC CONF=%x", modem_regs.reg12);
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
		//LOG("ModemNormalWrite : undef %03X = %X", reg, data);
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
					SET_STATUS_BIT(0x0b, modem_regs.reg0b.TONEA, connect_state == DISCONNECTED);
					SET_STATUS_BIT(0x0b, modem_regs.reg0b.TONEB, connect_state == DISCONNECTED);
					SET_STATUS_BIT(0x0b, modem_regs.reg0b.TONEC, connect_state == DISCONNECTED);
					// FIXME This should be reset if transmit buffer is full
					if (modem_regs.reg04.FIFOEN || module_download)
						SET_STATUS_BIT(0x0d, modem_regs.reg0d.TXFNF, 1);
				}
				u8 data = modem_regs.ptr[reg];
				if (reg == 0x00)	// RBUFFER
				{
					//LOG("Read RBUFFER = %X", data);
					modem_regs.reg1e.RDBF = 0;
					SET_STATUS_BIT(0x0c, modem_regs.reg0c.RXFNE, 0);
					SET_STATUS_BIT(0x01, modem_regs.reg01.RXHF, 0);
#ifndef NDEBUG
					if (connect_state == CONNECTED && recv_fp)
						fputc(data, recv_fp);
#endif
					update_interrupt();
				}
				else if (reg == 0x16 || reg == 0x17)
				{
					//LOG("SECTXB / SECTXB being read %02x", reg)
				}
				//else
				//	LOG("Read Reg %03x = %x", reg, data);

				return data;
			}

		case MS_ST_CONTROLER:
		case MS_ST_DSP:
			if (reg==0x10)
			{
				modem_regs.reg1e.TDBE=0;
				return 0;
			}
			else
			{
				return modem_regs.ptr[reg];
			}

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
	ser << modem_regs;
	ser << dspram;
	ser << state;
	ser << connect_state;
	ser << last_dial_time;
	ser << data_sent;
}
void ModemDeserialize(Deserializer& deser)
{
	if (deser.version() >= Deserializer::V20)
	{
		deser >> modem_regs;
		deser >> dspram;
		deser >> state;
		deser >> connect_state;
		deser >> last_dial_time;
		deser >> data_sent;
	}
}
