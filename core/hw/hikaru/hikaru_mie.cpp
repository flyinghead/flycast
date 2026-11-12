/*
	Sega 315-6146 "MIE" (Maple/JVS controller) low-level emulation for the Hikaru.

	Z80 memory map:
		0000-07ff	firmware ROM (2 KB)
		0800-6fff	open bus (ff)
		7000-703f	maple interface registers (mirrored every 0x40 up to 77ff)
		7800-ffff	RAM
	Z80 I/O ports:
		00-07		GPIO ports
		08			GPIO direction (1: input)
		0f			ADC
		10-15		JVS UART (8250-like): 10 data, 12 destination, 13 line control, 15 status
		30			IRQ enable
		50			maple IRQ level
		70			IRQ pending
		90			JVS control (bit 0 falling edge: send the buffered packet)
		91			JVS sense lines

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
#include "hikaru_mie.h"
#include "hikaru.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_devs.h"
#include "network/ggpo.h"
#include "archive/archive.h"
#include "oslib/oslib.h"
#include "serialize.h"
#include "stdclass.h"
extern "C" {
#include "z80/z80.h"
}
#include <cstring>
#include <memory>
#include <vector>

namespace hikaru::mie
{

constexpr u32 ROM_SIZE = 0x800;
constexpr u32 ROM_CRC = 0x9b197e35;
constexpr int Z80_CLOCK = 16000000;
// Run the Z80 in slices of this many SH-4 cycles
constexpr int SLICE_SH4_CYCLES = 2000;
constexpr int SLICE_Z80_CYCLES = (int)((u64)SLICE_SH4_CYCLES * Z80_CLOCK / SH4_MAIN_CLOCK);

static u8 rom[ROM_SIZE];
static bool romLoaded;

struct State
{
	z80 cpu;
	u8 ram[0x8800];		// 7800-ffff
	// maple interface
	u8 mapleRegs[0x40];
	// GPIO
	u8 gpioVal[8];
	u8 gpioDir;
	// interrupt controller
	u8 irqEnable;
	u8 irqPending;
	u8 mapleIrqLevel;
	// JVS UART
	u8 jvsLcr;
	u8 jvsDest;
	u8 jvsControl;
	u8 txBuf[0x200];
	u32 txLen;
	u8 rxBuf[0x400];
	u32 rxLen;
	u32 rxPos;
	u32 boardsAddressed;
};
static State state;
static int schedId = -1;

//
// JVS
//
static void jvsCommit()
{
	// Decode the escaped packet: E0 node len data... checksum. D0 xx means xx+1.
	std::vector<u8> pkt;
	bool escape = false;
	for (u32 i = 0; i < state.txLen; i++)
	{
		u8 b = state.txBuf[i];
		if (i == 0 && b == 0xe0)
			continue;
		if (escape) {
			pkt.push_back(b + 1);
			escape = false;
		}
		else if (b == 0xd0)
			escape = true;
		else
			pkt.push_back(b);
	}
	state.txLen = 0;
	state.rxLen = 0;
	state.rxPos = 0;
	if (pkt.size() < 3)
		return;
	u8 node = pkt[0];
	u32 len = pkt[1];	// data + checksum
	if (len < 1 || len + 2 > pkt.size())
	{
		WARN_LOG(MAPLE, "MIE: truncated JVS packet");
		return;
	}
	const u8 *data = &pkt[2];
	u32 dataLen = len - 1;
	if (data[0] == 0xf0)
		state.boardsAddressed = 0;
	else if (data[0] == 0xf1)
		state.boardsAddressed++;

	std::shared_ptr<MIE> mie = getMieDevice();
	if (mie == nullptr)
		return;
	// The controllers are read here: the maple DMA, which reads them on Naomi, is not used
	ggpo::getInput(mapleInputState);

	u8 reply[0x200];
	u32 replyLen = mie->jvsRawMessage(node, data, dataLen, reply);

	if (replyLen == 0)
		return;
	// Encode: sync byte as is, then escape E0 and D0
	state.rxBuf[state.rxLen++] = reply[0];
	for (u32 i = 1; i < replyLen && state.rxLen < sizeof(state.rxBuf) - 1; i++)
	{
		u8 b = reply[i];
		if (b == 0xe0 || b == 0xd0) {
			state.rxBuf[state.rxLen++] = 0xd0;
			state.rxBuf[state.rxLen++] = b - 1;
		}
		else
			state.rxBuf[state.rxLen++] = b;
	}
}

static u8 jvsSense()
{
	// bit 0: no board present, bit 1: all boards addressed
	std::shared_ptr<MIE> mie = getMieDevice();
	u32 boards = mie == nullptr ? 0 : mie->jvsBoardCount();
	return 0x8c | (boards == 0 ? 1 : 0) | (boards != 0 && state.boardsAddressed >= boards ? 2 : 0);
}

//
// Interrupts
//
static void updateIrq()
{
	u8 active = state.irqEnable & state.irqPending & 0x7f;
	if (active == 0 || !state.cpu.iff1 || state.cpu.int_pending)
		return;
	int level = 0;
	while (!(active & (1 << level)))
		level++;
	// Acknowledge: the vector is fetched now
	state.irqPending &= ~(1 << level);
	z80_gen_int(&state.cpu, 0xf2 + 2 * level);
}

//
// Z80 buses
//
static u8 memRead(void *, u16 addr)
{
	if (addr < ROM_SIZE)
		return rom[addr];
	if (addr < 0x7000)
		return 0xff;
	if (addr < 0x7800)
		return state.mapleRegs[addr & 0x3f];
	return state.ram[addr - 0x7800];
}

static void memWrite(void *, u16 addr, u8 data)
{
	if (addr < 0x7000)
		return;
	if (addr < 0x7800)
		state.mapleRegs[addr & 0x3f] = data;
	else
		state.ram[addr - 0x7800] = data;
}

static u8 portRead(u8 port)
{
	switch (port)
	{
	case 0x06:
		// bit 0: config EEPROM DO
		return (state.gpioVal[6] & ~1) | (hikaru::configEepromRead() ? 1 : 0);
	case 0x00: case 0x01: case 0x02: case 0x03:
	case 0x04: case 0x05: case 0x07:
		// TODO Hikaru cabinet inputs. Inputs idle high.
		if (state.gpioDir & (1 << port))
			return 0xff;
		return state.gpioVal[port];
	case 0x08:
		return state.gpioDir;
	case 0x0f:
		return 0;
	case 0x10:
		if (state.jvsLcr & 0x80)
			return 0;
		return state.rxPos < state.rxLen ? state.rxBuf[state.rxPos++] : 0;
	case 0x15:
		// 01: data available, 20: transmitter ready, 40: transmission done
		return 0x60 | (state.rxPos < state.rxLen ? 1 : 0);
	case 0x30:
		return state.irqEnable;
	case 0x50:
		return state.mapleIrqLevel;
	case 0x70:
		return state.irqPending;
	case 0x91:
		return jvsSense();
	default:
		DEBUG_LOG(MAPLE, "MIE: port read %02x", port);
		return 0;
	}
}

static void portWrite(u8 port, u8 data)
{
	switch (port)
	{
	case 0x00: case 0x01: case 0x02: case 0x03:
	case 0x04: case 0x05: case 0x06: case 0x07:
		state.gpioVal[port] = data;
		if (port == 5)
			hikaru::configEepromWrite(data);
		break;
	case 0x08:
		state.gpioDir = data;
		break;
	case 0x10:
		if (!(state.jvsLcr & 0x80) && state.txLen < sizeof(state.txBuf))
			state.txBuf[state.txLen++] = data;
		break;
	case 0x12:
		state.jvsDest = data;
		break;
	case 0x13:
		state.jvsLcr = data;
		break;
	case 0x30:
		state.irqEnable = data;
		updateIrq();
		break;
	case 0x50:
		state.mapleIrqLevel = data;
		break;
	case 0x70:
		state.irqPending = data;
		updateIrq();
		break;
	case 0x90:
		if ((state.jvsControl & 1) && !(data & 1))
			jvsCommit();
		state.jvsControl = data;
		break;
	default:
		DEBUG_LOG(MAPLE, "MIE: port write %02x = %02x", port, data);
		break;
	}
}

static u8 z80PortIn(z80 *, u8 port) {
	return portRead(port);
}
static void z80PortOut(z80 *, u8 port, u8 data) {
	portWrite(port, data);
}

static int schedCallback(int tag, int cycles, int jitter, void *arg)
{
	if (!romLoaded)
		return SLICE_SH4_CYCLES;
	// The cycle count is an unsigned long, 32-bit on Windows: it wraps after a few minutes,
	// so count the cycles of the slice rather than compare with an end value
	const unsigned long start = state.cpu.cyc;
	while (state.cpu.cyc - start < (unsigned long)SLICE_Z80_CYCLES)
	{
		updateIrq();
		z80_step(&state.cpu);
	}
	return SLICE_SH4_CYCLES;
}

//
// Host (SH-4) access
//
u8 hostReadPort(u32 addr) {
	return portRead((addr >> 1) & 0xff);
}
void hostWritePort(u32 addr, u8 data) {
	portWrite((addr >> 1) & 0xff, data);
}
u8 hostReadMem(u32 addr) {
	return memRead(nullptr, (addr >> 1) & 0xffff);
}
void hostWriteMem(u32 addr, u8 data) {
	memWrite(nullptr, (addr >> 1) & 0xffff, data);
}

bool loadFirmware()
{
	if (romLoaded)
		return true;
	std::string path = hostfs::findNaomiBios("mie.zip");
	if (path.empty())
		path = hostfs::findNaomiBios("mie.7z");
	std::unique_ptr<Archive> archive(OpenArchive(path));
	if (archive == nullptr)
	{
		WARN_LOG(MAPLE, "MIE: mie.zip not found");
		return false;
	}
	std::unique_ptr<ArchiveFile> file(archive->OpenFileByCrc(ROM_CRC));
	if (!file)
		file.reset(archive->OpenFile("315-6146.bin"));
	if (!file || file->Read(rom, ROM_SIZE) != ROM_SIZE)
	{
		WARN_LOG(MAPLE, "MIE: 315-6146.bin not found in %s", path.c_str());
		return false;
	}
	romLoaded = true;
	NOTICE_LOG(MAPLE, "MIE: firmware loaded from %s", path.c_str());
	return true;
}

static void setCallbacks()
{
	state.cpu.read_byte = memRead;
	state.cpu.write_byte = memWrite;
	state.cpu.port_in = z80PortIn;
	state.cpu.port_out = z80PortOut;
	state.cpu.userdata = nullptr;
}

void init()
{
	if (schedId == -1)
		schedId = sh4_sched_register(0, schedCallback);
}

void term()
{
	if (schedId != -1)
		sh4_sched_unregister(schedId);
	schedId = -1;
}

void reset()
{
	memset(&state, 0, sizeof(state));
	z80_init(&state.cpu);
	setCallbacks();
	state.gpioDir = 0xff;
	state.mapleIrqLevel = 0xff;
	if (!loadFirmware())
		WARN_LOG(MAPLE, "MIE: no firmware, I/O won't work");
	sh4_sched_request(schedId, SLICE_SH4_CYCLES);
}

void serialize(Serializer& ser)
{
	ser << state;
	sh4_sched_serialize(ser, schedId);
}

void deserialize(Deserializer& deser)
{
	deser >> state;
	setCallbacks();
	sh4_sched_deserialize(deser, schedId);
}

}
