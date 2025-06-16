/*
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
#include "types.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/holly/holly_intc.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/hwreg.h"

#include "naomi.h"
#include "naomi_cart.h"
#include "naomi_regs.h"
#include "naomi_m3comm.h"
#include "serialize.h"
#include "network/output.h"
#include "hw/sh4/modules/modules.h"
#include "oslib/oslib.h"
#include "printer.h"
#include "hw/flashrom/x76f100.h"
#include "input/haptic.h"
#include "midiffb.h"

#include <algorithm>

static NaomiM3Comm m3comm;
Multiboard *multiboard;

static X76F100SerialFlash mainSerialId;
static X76F100SerialFlash romSerialId;

static int dmaSchedId = -1;
static int dmaXferDelay = 10;	// cart dma xfer speed, in cycles/byte (default 20 MB/s)

void NaomiBoardIDWrite(const u16 data)
{
	// bit 2: clock
	// bit 3: data
	// bit 4: reset (x76f100 only)
	// bit 5: chip select
	mainSerialId.writeCS(data & 0x20);
	mainSerialId.writeRST(data & 0x10);
	mainSerialId.writeSCL(data & 4);
	mainSerialId.writeSDA(data & 8);
}

u16 NaomiBoardIDRead()
{
	// bit 0 indicates the eeprom is a X76F100, otherwise the BIOS expects an AT93C46
	// bit 3 is xf76f100 SDA
	// bit 4 is at93c46 DO
	return (mainSerialId.readSDA() << 3) | 1;
}

void NaomiGameIDWrite(const u16 data)
{
	romSerialId.writeCS(data & 4);
	romSerialId.writeRST(data & 8);
	romSerialId.writeSCL(data & 2);
	romSerialId.writeSDA(data & 1);
}

u16 NaomiGameIDRead()
{
	return romSerialId.readSDA() << 15;
}

static bool aw_ram_test_skipped = false;


u32 ReadMem_naomi(u32 address, u32 size)
{
//	verify(size != 1);
	if (unlikely(CurrentCartridge == NULL))
	{
		INFO_LOG(NAOMI, "called without cartridge");
		return 0xFFFF;
	}
	if (address >= NAOMI_COMM2_CTRL_addr && address <= NAOMI_COMM2_STATUS1_addr)
		return m3comm.ReadMem(address, size);
	else
		return CurrentCartridge->ReadMem(address, size);
}

void WriteMem_naomi(u32 address, u32 data, u32 size)
{
	if (unlikely(CurrentCartridge == NULL))
	{
		INFO_LOG(NAOMI, "called without cartridge");
		return;
	}
	if (address >= NAOMI_COMM2_CTRL_addr && address <= NAOMI_COMM2_STATUS1_addr
			&& settings.platform.isNaomi())
		m3comm.WriteMem(address, data, size);
	else
		CurrentCartridge->WriteMem(address, data, size);
}

static int naomiDmaSched(int tag, int sch_cycl, int jitter, void *arg)
{
	u32 start = SB_GDSTARD;
	u32 len = std::min<int>(((SB_GDLEN + 31) & ~31) - SB_GDLEND, 1024);
	SB_GDLEND += len;
	while (len > 0)
	{
		u32 block_len = len;
		void* ptr = CurrentCartridge->GetDmaPtr(block_len);
		if (block_len == 0)
		{
			INFO_LOG(NAOMI, "Aborted DMA transfer. Read past end of cart?");
			for (u32 i = 0; i < len; i += 8, start += 8)
				addrspace::write64(start, 0);
			break;
		}
		WriteMemBlock_nommu_ptr(start, (u32*)ptr, block_len);
		CurrentCartridge->AdvancePtr(block_len);
		len -= block_len;
		start += block_len;
	}
	SB_GDSTARD = start;
	if (SB_GDLEN <= SB_GDLEND)
	{
		SB_GDST = 0;
		asic_RaiseInterrupt(holly_GDROM_DMA);
		return 0;
	}
	else {
		return std::min<int>(SB_GDLEN - SB_GDLEND, 1024) * dmaXferDelay;
	}
}

//Dma Start
static void Naomi_DmaStart(u32 addr, u32 data)
{
	if ((data & 1) == 0 || SB_GDST == 1)
		return;
	if (SB_GDEN == 0)
	{
		INFO_LOG(NAOMI, "Invalid NAOMI-DMA start, SB_GDEN=0. Ignoring it.");
		return;
	}
	
	if (multiboard != nullptr && multiboard->dmaStart())
	{
	}
	else if (!m3comm.DmaStart(addr, data) && CurrentCartridge != nullptr)
	{
		DEBUG_LOG(NAOMI, "NAOMI-DMA start addr %08X len %x", SB_GDSTAR, SB_GDLEN);
		verify(1 == SB_GDDIR);
		SB_GDST = 1;
		SB_GDSTARD = SB_GDSTAR & 0x1FFFFFE0;
		SB_GDLEND = 0;
		// Max G1 bus rate: 50 MHz x 16 bits
		// SH4_access990312_e.xls: 14.4 MB/s from GD-ROM to system RAM
		// Here: 20 MB/s
		sh4_sched_request(dmaSchedId, std::min<int>(SB_GDLEN, 1024) * dmaXferDelay);
		return;
	}
	else
	{
		SB_GDSTARD = SB_GDSTAR + SB_GDLEN;
		SB_GDLEND = SB_GDLEN;
	}
	asic_RaiseInterrupt(holly_GDROM_DMA);
}

void Naomi_setDmaDelay()
{
	if (settings.platform.isAtomiswave() || settings.content.gameId == "FORCE FIVE"
			|| settings.content.gameId == "KENJU")
		// 7 MB/s for Atomiwave games and conversions
		dmaXferDelay = 27;
	else
		dmaXferDelay = 10;
}

static void Naomi_DmaEnable(u32 addr, u32 data)
{
	SB_GDEN = data & 1;
	if (SB_GDEN == 0 && SB_GDST == 1)
	{
		INFO_LOG(NAOMI, "NAOMI-DMA aborted");
		SB_GDST = 0;
		sh4_sched_request(dmaSchedId, -1);
	}
}

void naomi_reg_Init()
{
	static const u8 romSerialData[0x84] = {
		0x19, 0x00, 0xaa, 0x55,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x69, 0x79, 0x68, 0x6b, 0x74, 0x6d, 0x68, 0x6d,
		0xa1, 0x09, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ', ' ', ' ', ' ',
		'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'
	};
	romSerialId.setData(romSerialData);
	mainSerialId.setData(romSerialData);
	if (dmaSchedId == -1)
		dmaSchedId = sh4_sched_register(0, naomiDmaSched);
}

// Sets the full content of the rom board serial eeprom (132 bytes)
// including response to reset and read/write passwords.
void setGameSerialId(const u8 *data)
{
	romSerialId.setData(data);
}

// Return the protected data from the rom board serial eeprom (112 bytes)
// excluding response to reset and passwords.
const u8 *getGameSerialId()
{
	return romSerialId.getProtectedData();
}

void naomi_reg_Term()
{
	if (multiboard != nullptr)
		delete multiboard;
	multiboard = nullptr;
	m3comm.closeNetwork();
	networkOutput.term();
	if (dmaSchedId != -1)
		sh4_sched_unregister(dmaSchedId);
	dmaSchedId = -1;
	midiffb::term();
}

void naomi_reg_Reset(bool hard)
{
	hollyRegs.setWriteHandler<SB_GDST_addr>(Naomi_DmaStart);
	hollyRegs.setWriteHandler<SB_GDEN_addr>(Naomi_DmaEnable);
	SB_GDST = 0;
	SB_GDEN = 0;
	sh4_sched_request(dmaSchedId, -1);

	aw_ram_test_skipped = false;

	m3comm.closeNetwork();
	if (hard)
	{
		naomi_cart_Close();
		if (multiboard != nullptr)
		{
			delete multiboard;
			multiboard = nullptr;
		}
		if (settings.naomi.multiboard)
			multiboard = new Multiboard();
		networkOutput.reset();
		mainSerialId.reset();
		romSerialId.reset();
	}
	else if (multiboard != nullptr)
		multiboard->reset();
	midiffb::reset();
}

static u8 aw_maple_devs;
static u64 coin_chute_time[4];
static u8 awDigitalOuput;

u32 libExtDevice_ReadMem_A0_006(u32 addr, u32 size)
{
	addr &= 0x7ff;
	//printf("libExtDevice_ReadMem_A0_006 %d@%08x: %x\n", size, addr, mem600[addr]);
	switch (addr)
	{
//	case 0:
//		return 0;
//	case 4:
//		return 1;
	case 0x280:
		// 0x00600280 r  0000dcba
		//	a/b - 1P/2P coin inputs (JAMMA), active low
		//	c/d - 3P/4P coin inputs (EX. IO board), active low
		//
		//	(ab == 0) -> BIOS skip RAM test
		if (!aw_ram_test_skipped)
		{
			// Skip RAM test at startup
			aw_ram_test_skipped = true;
			return 0;
		}
		{
			u8 coin_input = 0xF;
			u64 now = sh4_sched_now64();
			for (int slot = 0; slot < 4; slot++)
			{
				if (maple_atomiswave_coin_chute(slot))
				{
					// ggx15 needs 4 or 5 reads to register the coin but it needs to be limited to avoid coin errors
					// 1 s of cpu time is too much, 1/2 s seems to work, let's use 100 ms
					if (coin_chute_time[slot] == 0 || now - coin_chute_time[slot] < SH4_MAIN_CLOCK / 10)
					{
						if (coin_chute_time[slot] == 0)
							coin_chute_time[slot] = now;
						coin_input &= ~(1 << slot);
					}
				}
				else
				{
					coin_chute_time[slot] = 0;
				}
			}
			return coin_input;
		}

	case 0x284:		// Atomiswave maple devices
		// ddcc0000 where cc/dd are the types of devices on maple bus 2 and 3:
		// 0: regular AtomisWave controller
		// 1: light gun
		// 2,3: mouse/trackball
		//printf("NAOMI 600284 read %x\n", aw_maple_devs);
		return aw_maple_devs;
	case 0x288:
		// ??? Dolphin Blue
		return 0;
	case 0x28c:
		return awDigitalOuput;
	}
	INFO_LOG(NAOMI, "Unhandled read @ %x sz %d", addr, size);
	return 0xFF;
}

void libExtDevice_WriteMem_A0_006(u32 addr, u32 data, u32 size)
{
	addr &= 0x7ff;
	//printf("libExtDevice_WriteMem_A0_006 %d@%08x: %x\n", size, addr, data);
	switch (addr)
	{
	case 0x284:		// Atomiswave maple devices
		DEBUG_LOG(NAOMI, "NAOMI 600284 write %x", data);
		aw_maple_devs = data & 0xF0;
		return;
	case 0x288:
		// ??? Dolphin Blue
		return;
	case 0x28C:		// Digital output
		if ((u8)data != awDigitalOuput)
		{
			if (atomiswaveForceFeedback)
			{
				// Wheel force feedback:
				// bit 0    direction (0 pos, 1 neg)
				// bit 1-4  strength
				networkOutput.output("awffb", (u8)data);
				// This really needs to be soften
				haptic::setTorque(0, (data & 1 ? -1.f : 1.f) * ((data >> 1) & 0xf) / 15.f * 0.5f);
			}
			else
			{
				u8 changes = data ^ awDigitalOuput;
				for (int i = 0; i < 8; i++)
					if (changes & (1 << i))
					{
						std::string name = "lamp" + std::to_string(i);
						networkOutput.output(name.c_str(), (data >> i) & 1);
					}
			}
			awDigitalOuput = data;
			DEBUG_LOG(NAOMI, "AW output %02x", data);
		}
		return;
	default:
		break;
	}
	INFO_LOG(NAOMI, "Unhandled write @ %x (%d): %x", addr, size, data);
}

void naomi_Serialize(Serializer& ser)
{
	mainSerialId.serialize(ser);
	romSerialId.serialize(ser);
	ser << aw_maple_devs;
	ser << coin_chute_time;
	ser << aw_ram_test_skipped;
	// TODO serialize m3comm?
	midiffb::serialize(ser);
	sh4_sched_serialize(ser, dmaSchedId);
}
void naomi_Deserialize(Deserializer& deser)
{
	if (deser.version() < Deserializer::V40)
	{
		deser.skip<u32>();	// GSerialBuffer
		deser.skip<u32>();	// BSerialBuffer
		deser.skip<int>();	// GBufPos
		deser.skip<int>();	// BBufPos
		deser.skip<int>();	// GState
		deser.skip<int>();	// BState
		deser.skip<int>();	// GOldClk
		deser.skip<int>();	// BOldClk
		deser.skip<int>();	// BControl
		deser.skip<int>();	// BCmd
		deser.skip<int>();	// BLastCmd
		deser.skip<int>();	// GControl
		deser.skip<int>();	// GCmd
		deser.skip<int>();	// GLastCmd
		deser.skip<int>();	// SerStep
		deser.skip<int>();	// SerStep2
		deser.skip(69);		// BSerial
		deser.skip(69);		// GSerial
	}
	else
	{
		mainSerialId.deserialize(deser);
		romSerialId.deserialize(deser);
	}
	if (deser.version() < Deserializer::V36)
	{
		deser.skip<u32>(); // reg_dimm_command;
		deser.skip<u32>(); // reg_dimm_offsetl;
		deser.skip<u32>(); // reg_dimm_parameterl;
		deser.skip<u32>(); // reg_dimm_parameterh;
		deser.skip<u32>(); // reg_dimm_status;
	}
	deser >> aw_maple_devs;
	if (deser.version() >= Deserializer::V20)
	{
		deser >> coin_chute_time;
		deser >> aw_ram_test_skipped;
	}
	midiffb::deserialize(deser);
	if (deser.version() >= Deserializer::V45)
		sh4_sched_deserialize(deser, dmaSchedId);
}

struct DriveSimPipe : public SerialPort::Pipe
{
	void write(u8 data) override
	{
		if (buffer.empty() && data != 2)
			return;
		if (buffer.size() == 7)
		{
			u8 checksum = 0;
			for (u8 b : buffer)
				checksum += b;
			if (checksum == data)
			{
				int newTacho = (buffer[2] - 1) * 100;
				if (newTacho != tacho)
				{
					tacho = newTacho;
					networkOutput.output("tachometer", tacho);
				}
				int newSpeed = buffer[3] - 1;
				if (newSpeed != speed)
				{
					speed = newSpeed;
					networkOutput.output("speedometer", speed);
				}
				if (!config::NetworkOutput)
				{
					char message[16];
					snprintf(message, sizeof(message), "Speed: %3d", speed);
					os_notify(message, 1000);
				}
			}
			buffer.clear();
		}
		else
		{
			buffer.push_back(data);
		}
	}

	void reset()
	{
		buffer.clear();
		tacho = -1;
		speed = -1;
	}
private:
	std::vector<u8> buffer;
	int tacho = -1;
	int speed = -1;
};

void initDriveSimSerialPipe()
{
	static DriveSimPipe pipe;

	pipe.reset();
	SCIFSerialPort::Instance().setPipe(&pipe);
}

G2PrinterConnection g2PrinterConnection;

u32 G2PrinterConnection::read(u32 addr, u32 size)
{
	if (addr == STATUS_REG_ADDR)
	{
		u32 ret = printerStat;
		printerStat |= 1;
		DEBUG_LOG(NAOMI, "Printer status == %x", ret);
		return ret;
	}
	else
	{
		INFO_LOG(NAOMI, "Unhandled G2 Ext read<%d> at %x", size, addr);
		return 0;
	}
}

void G2PrinterConnection::write(u32 addr, u32 size, u32 data)
{
	switch (addr)
	{
	case DATA_REG_ADDR:
		for (u32 i = 0; i < size; i++)
			printer::print((char)(data >> (i * 8)));
		break;

	case STATUS_REG_ADDR:
		DEBUG_LOG(NAOMI, "Printer status = %x", data);
		printerStat &= ~1;
		break;

	default:
		INFO_LOG(NAOMI, "Unhandled G2 Ext write<%d> at %x: %x", size, addr, data);
		break;
	}
}

