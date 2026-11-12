/*
	Sega Hikaru board emulation: memory map, SH-4/PCI bridges, external bus, DMA.

	Hardware notes are in HIKARU_SPEC.md (sections 2-9).

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
#include "hikaru.h"
#include "hikaru_gpu.h"
#include "hikaru_mie.h"
#include "hikaru_slave.h"
#include "hw/mem/addrspace.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/flashrom/nvmem.h"
#include "hw/aica/aica_if.h"
#include "hw/aica/aica_chip.h"
#include "hw/flashrom/x76f100.h"
#include "hw/flashrom/at93cxx.h"
#include "hw/naomi/decrypt.h"
#include "serialize.h"
#include "cfg/option.h"
#include "oslib/oslib.h"
#include "emulator.h"

#include <cstring>

namespace hikaru
{

static inline u32 bswap32(u32 v)
{
	return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
}

static u8 *texRam;
static u8 *cmdRam;
static u8 *slaveRam;

//
// SH-4 <-> PCI bridge (315-6154). One per CPU.
//
struct Bridge
{
	u8 regs[0x40];
	bool slave;

	u32 reg32(u32 offset) const {
		u32 v;
		memcpy(&v, &regs[offset & 0x3c], 4);
		return v;
	}
	void setReg32(u32 offset, u32 v) {
		memcpy(&regs[offset & 0x3c], &v, 4);
	}
	// Bank selected for each external bus window
	u32 bank(u32 regOffset) const { return regs[regOffset] & 0x7f; }
	// The page with its bit 7, which only page 8C tells apart (Odin)
	u32 page(u32 regOffset) const { return regs[regOffset] & 0xff; }
};
static Bridge masterBridge;
static Bridge slaveBridge { {}, true };

// Window base address (bits 24-28) -> bridge register holding its bank
static int windowBankReg(u32 region)
{
	switch (region)
	{
	case 0x02: return 0x1a;
	case 0x03: return 0x1b;
	case 0x16: return 0x12;
	case 0x17: return 0x13;
	case 0x18: return 0x1c;
	default: return -1;
	}
}

static void updateBridgeIrq(const Bridge& br)
{
	// Bytes 4-5: interrupt status (write-1-to-clear)
	bool pending = br.regs[4] != 0 || br.regs[5] != 0;
	if (br.slave)
		slave::setBridgeIrq(pending);
	else
		// Master IRL1 pin (BIOS interrupt handler for INTEVT 0x2A0 checks the bridge status)
		InterruptPend(sh4_IRL1_INDEP, pending);
}

static void startDma(Bridge& br);

static void bridgeWrite8(Bridge& br, u32 offset, u8 data)
{
	offset &= 0x3f;
	if (offset == 4 || offset == 5)
	{
		br.regs[offset] &= ~data;
		updateBridgeIrq(br);
	}
	else if (offset == 0x3b)
	{
		br.regs[offset] = data;
		if (data & 1)
			startDma(br);
	}
	else {
		br.regs[offset] = data;
	}
}

template<typename T>
static T bridgeRead(const Bridge& br, u32 addr)
{
	u32 offset = addr & 0x3f;
	if (offset == 0)
		// CPU id: the master reads 0, the slave all ones
		return br.slave ? (T)0xffffffff : 0;
	T v;
	memcpy(&v, &br.regs[offset], sizeof(T));
	return v;
}

template<typename T>
static void bridgeWrite(Bridge& br, u32 addr, T data)
{
	for (u32 i = 0; i < sizeof(T); i++)
		bridgeWrite8(br, addr + i, (u8)(data >> (i * 8)));
}

template<typename T>
static T DYNACALL readBridge(u32 addr) {
	return bridgeRead<T>(masterBridge, addr);
}

template<typename T>
static void DYNACALL writeBridge(u32 addr, T data) {
	bridgeWrite(masterBridge, addr, data);
}

template<typename T>
T slaveBridgeRead(u32 addr) {
	return bridgeRead<T>(slaveBridge, addr);
}
template<typename T>
void slaveBridgeWrite(u32 addr, T data) {
	bridgeWrite(slaveBridge, addr, data);
}
template u8 slaveBridgeRead<u8>(u32);
template u16 slaveBridgeRead<u16>(u32);
template u32 slaveBridgeRead<u32>(u32);
template void slaveBridgeWrite<u8>(u32, u8);
template void slaveBridgeWrite<u16>(u32, u16);
template void slaveBridgeWrite<u32>(u32, u32);

//
// External bus
//
static u8 *romData() {
	HikaruCartridge *cart = dynamic_cast<HikaruCartridge *>(CurrentCartridge);
	return cart == nullptr ? nullptr : cart->data();
}
static u32 romSize() {
	HikaruCartridge *cart = dynamic_cast<HikaruCartridge *>(CurrentCartridge);
	return cart == nullptr ? 0 : cart->size();
}

u8 *busPtr(u32 busAddr)
{
	u32 bank = (busAddr >> 24) & 0x7f;
	u32 offset = busAddr & 0xffffff;
	switch (bank)
	{
	case 0x04:
		return texRam + 0x800000 + (offset & 0x3fffff);
	case 0x06:
		return texRam + 0xc00000 + (offset & 0x3fffff);
	case 0x0c:
	case 0x0d:
		if (offset >= 0x800000)
			return aica::chipRam(bank - 0x0c) + (offset & ARAM_MASK);
		return nullptr;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
		if (romSize() == 0)
			return nullptr;
		return romData() + ((offset & 0x7fffff) + (bank - 0x10) * 0x800000) % romSize();
	case 0x40:
	case 0x41:
		return slaveRam + (bank - 0x40) * 0x1000000 + offset;
	case 0x48:
		return cmdRam + (offset & (CMDRAM_SIZE - 1));
	case 0x70:
	case 0x71:
		return &mem_b[0] + (((bank - 0x70) * 0x1000000 + offset) & RAM_MASK);
	default:
		if (bank >= 0x20 && bank <= 0x2f)
		{
			if (romSize() == 0)
				return nullptr;
			return romData() + (ROM_MASK_OFFSET + (bank - 0x20) * 0x1000000 + offset) % romSize();
		}
		return nullptr;
	}
}

static bool busOpen(u32 bank)
{
	return bank == 0x0e
			|| (bank >= 0x15 && bank <= 0x1f)
			|| (bank >= 0x30 && bank <= 0x3f);
}

// ROM board (bank 0x0A): 64 KB of local RAM (two 8-bit SRAMs) and, on the boards that have
// one, a 315-5881 at 0x10000-0x1000F (Odin, MAME).
// TODO the Actel 0x4252 board.
static u16 romBoardRam[0x8000];
static u32 romBoardOffset;
static u32 romBoardReg8;
static u32 romBoardRegC;
// 315-5881: the first read after an address write gives a dummy 0 (Odin, MAME)
static bool cryptoFirstRead;

static u32 cartKey() {
	HikaruCartridge *cart = dynamic_cast<HikaruCartridge *>(CurrentCartridge);
	return cart == nullptr ? 0 : cart->getKey();
}

// The 315-5881 reads its ciphertext from the local RAM, one big-endian word at a time
static u16 cryptoRead(u32 addr)
{
	u16 w = romBoardRam[addr & 0x7fff];
	return (u16)((w >> 8) | (w << 8));
}

template<typename T>
static T romBoardRead(u32 offset)
{
	if (offset < 0x10000)
	{
		T v;
		memcpy(&v, (u8 *)romBoardRam + offset, sizeof(T));
		return v;
	}
	// 315-5881, 16-bit registers: +0 status (always ready), +E next decrypted word
	if (sizeof(T) == 2 && offset >= 0x10000 && offset < 0x10010 && cartKey() != 0)
	{
		if (offset == 0x1000e)
		{
			if (cryptoFirstRead)
			{
				cryptoFirstRead = false;
				return 0;
			}
			cyptoSetKey(cartKey());
			return (T)cryptoDecrypt(cryptoRead);
		}
		return 0;
	}
	if (sizeof(T) == 4)
	{
		switch (offset & 0x1f)
		{
		case 0x04: return (T)romBoardOffset;
		case 0x08: return (T)romBoardReg8;
		case 0x0c: return (T)romBoardRegC;
		default: break;
		}
	}
	INFO_LOG(NAOMI, "hikaru: rom board read%d %06x", (int)sizeof(T) * 8, offset);
	return 0;
}

template<typename T>
static void romBoardWrite(u32 offset, T data)
{
	if (offset < 0x10000)
	{
		memcpy((u8 *)romBoardRam + offset, &data, sizeof(T));
		return;
	}
	// 315-5881: +8 address low word, +A address high word (taken as 0, as in MAME), +C sequence key
	if (sizeof(T) == 2 && offset >= 0x10000 && offset < 0x10010 && cartKey() != 0)
	{
		switch (offset)
		{
		case 0x10008:
			cyptoSetLowAddr((u16)data);
			cryptoFirstRead = true;
			break;
		case 0x1000a:
			cyptoSetHighAddr(0);
			cryptoFirstRead = true;
			break;
		case 0x1000c:
			cyptoSetSubkey((u16)data);
			break;
		default:
			INFO_LOG(NAOMI, "hikaru: 315-5881 write %06x = %x", offset, (u32)data);
			break;
		}
		return;
	}
	if (sizeof(T) == 4)
	{
		switch (offset & 0x1ff)
		{
		case 0x00: romBoardOffset = data; return;
		case 0x08: romBoardReg8 = data; return;
		case 0x0c: romBoardRegC = data; return;
		default: break;
		}
	}
	INFO_LOG(NAOMI, "hikaru: rom board write%d %06x = %x", (int)sizeof(T) * 8, offset, (u32)data);
}

// Sound boards (bus pages 0x0C main, 0x0D optional): AICA registers at 0x700000-0x707FFF,
// RTC at 0x710000, wave RAM from 0x800000 (Odin, Valkyrie). The main board drives the speakers
// is AICA chip 0. The optional board is chip 1: its output goes to the external inputs (EXTS) of the
// main board (Odin).
template<typename T>
static T aicaRegRead(int aica, u32 offset)
{
	aica::ChipScope scope(aica);
	{
		if (offset >= 0x700000 && offset < 0x708000)
			return aica::readAicaReg<T>(offset);
		if (offset >= 0x710000 && offset < 0x710010)
			return aica::readRtcReg<T>(offset);
	}
	DEBUG_LOG(AICA, "hikaru: AICA%d read%d %06x", aica, (int)sizeof(T) * 8, offset);
	return 0;
}

template<typename T>
static void aicaRegWrite(int aica, u32 offset, T data)
{
	aica::ChipScope scope(aica);
	{
		if (offset >= 0x700000 && offset < 0x708000)
		{
			aica::writeAicaReg<T>(offset, data);
			return;
		}
		if (offset >= 0x710000 && offset < 0x710010)
		{
			aica::writeRtcReg<T>(offset, data);
			return;
		}
	}
	DEBUG_LOG(AICA, "hikaru: AICA%d write%d %06x = %x", aica, (int)sizeof(T) * 8, offset, (u32)data);
}

static u16 idChipRead();
static void idChipWrite(u16 data);

template<typename T>
static T busRead(u32 busAddr)
{
	// The first word of page 8C (bit 31 set): pharrier stops with "NO AICA EXTENSION BOARD"
	// unless its bit 2 is set. The other bits are not known (Odin).
	if (busAddr == 0x8c000000)
		return (T)4;
	u32 bank = (busAddr >> 24) & 0x7f;
	u32 offset = busAddr & 0xffffff;
	if (u8 *p = busPtr(busAddr))
	{
		T v;
		memcpy(&v, p, sizeof(T));
		return v;
	}
	switch (bank)
	{
	case 0x0a:
		return romBoardRead<T>(offset);
	case 0x0c:
	case 0x0d:
		return aicaRegRead<T>(bank - 0x0c, offset);
	case 0x14:
		return (T)idChipRead();
	default:
		if (busOpen(bank))
			return (T)0xffffffff;
		INFO_LOG(NAOMI, "hikaru: bus read%d from unknown bank %02x offset %06x", (int)sizeof(T) * 8, bank, offset);
		return 0;
	}
}

template<typename T>
static void busWrite(u32 busAddr, T data)
{
	u32 bank = (busAddr >> 24) & 0x7f;
	u32 offset = busAddr & 0xffffff;
	if (bank < 0x10 || bank > 0x2f)	// ROM is read-only
	{
		if (u8 *p = busPtr(busAddr))
		{
			if (bank == 0x04 || bank == 0x06)
				gpu::texRamWritten((u32)(p - texRam), sizeof(T));
			memcpy(p, &data, sizeof(T));
			return;
		}
	}
	switch (bank)
	{
	case 0x0a:
		romBoardWrite<T>(offset, data);
		break;
	case 0x0c:
	case 0x0d:
		aicaRegWrite<T>(bank - 0x0c, offset, data);
		break;
	case 0x14:
		idChipWrite((u16)data);
		break;
	default:
		INFO_LOG(NAOMI, "hikaru: bus write%d to bank %02x offset %06x = %x", (int)sizeof(T) * 8, bank, offset, (u32)data);
		break;
	}
}

u32 busRead32(u32 busAddr) {
	return busRead<u32>(busAddr);
}
void busWrite32(u32 busAddr, u32 data) {
	busWrite<u32>(busAddr, data);
}

// Banked windows at 0x02, 0x03, 0x16, 0x17 and 0x18 (except the GPU registers in 0x18)
template<typename T>
static T DYNACALL readWindow(u32 addr)
{
	u32 region = (addr >> 24) & 0x1f;
	if (region == 0x18 && (addr & 0xffffff) < 0x160000)
		return gpu::readReg<T>(addr);
	u32 page = masterBridge.page(windowBankReg(region));
	return busRead<T>((page << 24) | (addr & 0xffffff));
}

template<typename T>
static void DYNACALL writeWindow(u32 addr, T data)
{
	u32 region = (addr >> 24) & 0x1f;
	if (region == 0x18 && (addr & 0xffffff) < 0x160000)
	{
		gpu::writeReg<T>(addr, data);
		return;
	}
	u32 bank = masterBridge.bank(windowBankReg(region));
	busWrite<T>((bank << 24) | (addr & 0xffffff), data);
}

//
// Bridge DMA
//
// Copy to texture RAM: the 4 MB page is filled in Z-order. Each leaf is a 2x2 texel block
// fed by two 32-bit source words (byte-swapped): the first goes to row y, the second to row y+1.
static void dmaTwiddleBlock(u32& src, u32 dstBase, u32 x, u32 y, u32 size, u32 xOffset)
{
	while (size != 2)
	{
		size /= 2;
		dmaTwiddleBlock(src, dstBase, x, y, size, xOffset);
		dmaTwiddleBlock(src, dstBase, x + size, y, size, xOffset);
		dmaTwiddleBlock(src, dstBase, x, y + size, size, xOffset);
		x += size;
		y += size;
	}
	u32 w0 = busRead<u32>(src);
	u32 w1 = busRead<u32>(src + 4);
	src += 8;
	u32 dst = dstBase + ((y << 11) + xOffset + x) * 2;
	busWrite<u32>(dst, bswap32(w0));
	busWrite<u32>(dst + 0x1000, bswap32(w1));
}

static void startDma(Bridge& br)
{
	u32 src = br.reg32(0x30);
	u32 dst = br.reg32(0x34);
	u32 len = br.reg32(0x38) & 0xffffff;	// in 32-bit words
	DEBUG_LOG(NAOMI, "hikaru: DMA %08x -> %08x len %x", src, dst, len * 4);

	u32 dstBank = (dst >> 24) & 0x7f;
	if (dstBank == 0x04 || dstBank == 0x06)
	{
		dmaTwiddleBlock(src, dst, 0, 0, 0x400, 0);
		dmaTwiddleBlock(src, dst, 0, 0, 0x400, 0x400);
		dst += len * 4;
	}
	else
	{
		u8 *srcp = busPtr(src);
		u8 *dstp = busPtr(dst);
		if (srcp != nullptr && dstp != nullptr && ((dst >> 24) < 0x10 || (dst >> 24) > 0x2f))
		{
			if (dstBank == 0x04 || dstBank == 0x06)
				gpu::texRamWritten((u32)(dstp - texRam), len * 4);
			memcpy(dstp, srcp, len * 4);
			src += len * 4;
			dst += len * 4;
		}
		else
		{
			for (; len != 0; len--, src += 4, dst += 4)
				busWrite<u32>(dst, busRead<u32>(src));
		}
	}
	br.setReg32(0x04, 0x1000);
	br.setReg32(0x30, src);
	br.setReg32(0x34, dst);
	br.setReg32(0x38, 0);
	updateBridgeIrq(br);
}

//
// Region 0x00/0x01: BIOS, misc registers, MIE, backup SRAM
//
static u32 reg00400000;
static u16 reg01000000;
static u16 reg01000100;

// Main board security ID chip: X76F100 serial EEPROM on bus bank 0x14 (16-bit)
//	bit 0: SDA (in/out), bit 1: SCL, bit 2: chip select (active high), bit 3: reset (active low)
// The BIOS reads 16 bytes of data (an ASCII board serial number) followed by a 16-bit checksum,
// using an all-zero read password. A missing or invalid record gives "ERROR 2".
static X76F100SerialFlash idChip;
static u16 idChipReg;

// Board serial number, stored in both the ID chip and the board EEPROM
static const char boardSerial[] = "AABE-02A62354306";

static u16 boardSerialCrc(const u8 *data, int len)
{
	// CRC-CCITT computed by the BIOS on a 32-bit register with a fixed seed
	u32 crc = 0x01a18104;
	for (int i = 0; i <= len; i++)
	{
		if (i < len)
			crc |= data[i];
		for (int b = 0; b < 8; b++)
			crc = (crc & 0x80000000) ? (crc << 1) ^ 0x10210000 : crc << 1;
	}
	return crc >> 16;
}

static void initIdChip()
{
	u8 image[4 + 8 + 8 + 112];
	memset(image, 0xff, sizeof(image));
	static const u8 responseToReset[4] { 0x19, 0x00, 0xaa, 0x55 };
	memcpy(&image[0], responseToReset, 4);
	memcpy(&image[4], "iyhktmhm", 8);		// write password
	memset(&image[12], 0, 8);				// read password
	u8 *data = &image[20];
	memcpy(data, boardSerial, 16);
	u16 crc = boardSerialCrc(data, 16);
	data[16] = crc & 0xff;
	data[17] = crc >> 8;
	idChip.setData(image);
}

static void idChipWrite(u16 data)
{
	idChipReg = data;
	idChip.writeCS(!(data & 4));
	idChip.writeRST(!(data & 8));
	idChip.writeSCL(data & 2);
	idChip.writeSDA(data & 1);
}

static u16 idChipRead() {
	return (idChipReg & ~1) | (idChip.readSDA() ? 1 : 0);
}
static u16 reg1a800008;
static u16 reg1b000100;

// Master SH-4 port A
//	bit 0: falling edge raises an NMI on the slave
//	bits 2-5: main board 93C46 serial EEPROM in 8-bit organization
//		bit 2: DI, bit 3: CLK, bit 4: CS, read bit 5: DO
// The BIOS reads the board serial record (16 ASCII bytes + checksum) at address 0.
// A missing or invalid record gives "ERROR 1".
static u16 portAValue;
static AT93C46SerialEeprom boardEeprom(true);

static void initBoardEeprom()
{
	u8 record[18];
	memcpy(record, boardSerial, 16);
	u16 crc = boardSerialCrc(record, 16);
	record[16] = crc & 0xff;
	record[17] = crc >> 8;
	u8 *data = boardEeprom.data;
	memset(data, 0xff, boardEeprom.size);
	memcpy(data, record, sizeof(record));
}

// System configuration: 93C46 serial EEPROM (8-bit organization) on the MIE GPIO ports,
// as on Naomi. Port 5 bit 0: CLK, bit 1: DI, bit 3: CS. Port 6 bit 0: DO.
// The BIOS reads 128 bytes: two copies of a 64-byte record, each with a checksum (same CRC
// as the board serial) of bytes 0-61 stored little-endian in bytes 62-63. Byte 5 is the
// region (0 Japan, 1 USA, 2 export, 3 Australia, 4 Korea). With no valid copy the BIOS
// uses built-in defaults, which select Japan.
static AT93C46SerialEeprom configEeprom(true);

static std::string getConfigEepromPath() {
	return hostfs::getArcadeFlashPath() + ".eeprom";
}

static void initConfigEeprom()
{
	if (configEeprom.Load(getConfigEepromPath()))
		return;
	// BIOS default record values
	static const u8 defaults[8] { 0x17, 0x02, 0x62, 0x19, 0x02, 0x00, 0x00, 0x00 };
	u8 record[64] {};
	memcpy(record, defaults, sizeof(defaults));
	record[8] = 0x18;
	record[12] = record[13] = record[14] = 1;
	record[16] = record[17] = record[18] = record[19] = 0x11;
	record[28] = 6;
	int region = config::Region;
	record[5] = region == 0 ? 0 : region == 2 ? 2 : 1;
	u16 crc = boardSerialCrc(record, 62);
	record[62] = crc & 0xff;
	record[63] = crc >> 8;
	memcpy(configEeprom.data, record, 64);
	memcpy(configEeprom.data + 64, record, 64);
}

void configEepromWrite(u8 data)
{
	configEeprom.writeCS(data & 8);
	configEeprom.writeDI(data & 2);
	configEeprom.writeCLK(data & 1);
}

bool configEepromRead() {
	return configEeprom.readDO();
}

void portAWrite(u16 data)
{
	if ((portAValue & 1) && !(data & 1))
		slave::nmi();
	boardEeprom.writeCS(data & 0x10);
	boardEeprom.writeDI(data & 4);
	boardEeprom.writeCLK(data & 8);
	portAValue = data;
}

u16 portARead() {
	return boardEeprom.readDO() ? 0x20 : 0;
}

// Registers shared by both CPUs (16-bit)
bool sharedRegRead(u32 addr, u16& value)
{
	switch (addr & 0x1fffffff)
	{
	case 0x1a800008: value = reg1a800008; return true;
	case 0x1b000100: value = reg1b000100; return true;
	default: return false;
	}
}

bool sharedRegWrite(u32 addr, u16 value)
{
	switch (addr & 0x1fffffff)
	{
	case 0x1a800008: reg1a800008 = value; return true;
	case 0x1b000100: reg1b000100 = value ^ 0x100; return true;
	default: return false;
	}
}

// MIE (Maple/JVS controller): 0x00800000-0x0081FFFF Z80 I/O ports, 0x00820000-0x0083FFFF Z80 memory.
// One Z80 byte per 16-bit word, sign-extended on reads.
template<typename T>
static T mieRead(u32 addr)
{
	u8 v = addr < 0x820000 ? mie::hostReadPort(addr) : mie::hostReadMem(addr);
	return (T)(s8)v;
}

template<typename T>
static void mieWrite(u32 addr, T data)
{
	if (addr < 0x820000)
		mie::hostWritePort(addr, (u8)data);
	else
		mie::hostWriteMem(addr, (u8)data);
}

template<typename T>
static T DYNACALL readArea0(u32 addr)
{
	addr &= 0x1fffffff;
	if (addr < 0x200000)
	{
		T v;
		memcpy(&v, nvmem::getBiosData() + addr, sizeof(T));
		return v;
	}
	if (addr >= 0x800000 && addr < 0x840000)
		return mieRead<T>(addr);
	if (addr >= 0xc00000 && addr < 0xc00000 + SRAM_SIZE)
		return (T)nvmem::readFlash(addr & (SRAM_SIZE - 1), sizeof(T));
	switch (addr)
	{
	case 0x00400000:
		return (T)reg00400000;
	case 0x01000000:
		return (T)reg01000000;
	case 0x01000100:
		return (T)reg01000100;
	default:
		INFO_LOG(NAOMI, "hikaru: read%d from %08x", (int)sizeof(T) * 8, addr);
		return 0;
	}
}

template<typename T>
static void DYNACALL writeArea0(u32 addr, T data)
{
	addr &= 0x1fffffff;
	if (addr < 0x200000)
		return;
	if (addr >= 0x800000 && addr < 0x840000)
	{
		mieWrite<T>(addr, data);
		return;
	}
	if (addr >= 0xc00000 && addr < 0xc00000 + SRAM_SIZE)
	{
		nvmem::writeFlash(addr & (SRAM_SIZE - 1), data, sizeof(T));
		return;
	}
	switch (addr)
	{
	case 0x00400000:
		reg00400000 = data;
		break;
	case 0x01000000:
		reg01000000 = (u16)data ^ 0x100;
		break;
	case 0x01000100:
		reg01000100 = (u16)data ^ 0x100;
		break;
	default:
		INFO_LOG(NAOMI, "hikaru: write%d to %08x = %x", (int)sizeof(T) * 8, addr, (u32)data);
		break;
	}
}

// Region 0x1A: GPU control block, plus a misc register at 0x1A800008
template<typename T>
static T DYNACALL readArea1A(u32 addr)
{
	if ((addr & 0x1fffffff) == 0x1a800008)
		return (T)reg1a800008;
	return gpu::readReg<T>(addr);
}

template<typename T>
static void DYNACALL writeArea1A(u32 addr, T data)
{
	if ((addr & 0x1fffffff) == 0x1a800008)
		reg1a800008 = (u16)data;
	else
		gpu::writeReg<T>(addr, data);
}

// Region 0x1B: texture RAM (low 8 MB) and a misc register at 0x1B000100
template<typename T>
static T DYNACALL readArea1B(u32 addr)
{
	if ((addr & 0x1fffffff) == 0x1b000100)
		return (T)reg1b000100;
	T v;
	memcpy(&v, texRam + (addr & 0x7fffff), sizeof(T));
	return v;
}

template<typename T>
static void DYNACALL writeArea1B(u32 addr, T data)
{
	if ((addr & 0x1fffffff) == 0x1b000100)
		reg1b000100 = (u16)data ^ 0x100;
	else
	{
		gpu::texRamWritten(addr & 0x7fffff, sizeof(T));
		memcpy(texRam + (addr & 0x7fffff), &data, sizeof(T));
	}
}

static addrspace::handler area0Handler;
static addrspace::handler bridgeHandler;
static addrspace::handler windowHandler;
static addrspace::handler gpuHandler;
static addrspace::handler area1AHandler;
static addrspace::handler area1BHandler;

void mapArea(u32 base)
{
	if (base == 0)
	{
		area0Handler = addrspaceRegisterHandlerTemplate(readArea0, writeArea0);
		bridgeHandler = addrspaceRegisterHandlerTemplate(readBridge, writeBridge);
		windowHandler = addrspaceRegisterHandlerTemplate(readWindow, writeWindow);
		gpuHandler = addrspaceRegisterHandlerTemplate(gpu::readReg, gpu::writeReg);
		area1AHandler = addrspaceRegisterHandlerTemplate(readArea1A, writeArea1A);
		area1BHandler = addrspaceRegisterHandlerTemplate(readArea1B, writeArea1B);
	}
	addrspace::mapHandler(area0Handler, base | 0x00, base | 0x01);
	addrspace::mapHandler(windowHandler, base | 0x02, base | 0x03);
	addrspace::mapHandler(bridgeHandler, base | 0x04, base | 0x04);
	addrspace::mapBlockMirror(&mem_b[0], base | 0x0c, base | 0x0f, RAM_SIZE);
	addrspace::mapBlock(cmdRam, base | 0x14, base | 0x14, CMDRAM_SIZE - 1);
	addrspace::mapHandler(gpuHandler, base | 0x15, base | 0x15);
	addrspace::mapHandler(windowHandler, base | 0x16, base | 0x18);
	addrspace::mapHandler(area1AHandler, base | 0x1a, base | 0x1a);
	addrspace::mapHandler(area1BHandler, base | 0x1b, base | 0x1b);
}

static void *allocZeroed(size_t size)
{
	void *p = allocAligned(PAGE_SIZE, size);
	memset(p, 0, size);
	return p;
}

u8 *getTexRam() { return texRam; }
u8 *getCmdRam() { return cmdRam; }
u8 *getSlaveRam() { return slaveRam; }

static void saveConfigEeprom(Event, void *) {
	configEeprom.Save(getConfigEepromPath());
}

void init()
{
	if (texRam != nullptr)
		return;
	EventManager::listen(Event::Pause, saveConfigEeprom);
	texRam = (u8 *)allocZeroed(TEXRAM_SIZE);
	cmdRam = (u8 *)allocZeroed(CMDRAM_SIZE);
	slaveRam = (u8 *)allocZeroed(SLAVE_RAM_SIZE);
	gpu::init();
	mie::init();
	slave::init();
}

void term()
{
	EventManager::unlisten(Event::Pause, saveConfigEeprom);
	slave::term();
	mie::term();
	gpu::term();
	freeAligned(texRam);
	freeAligned(cmdRam);
	freeAligned(slaveRam);
	texRam = cmdRam = slaveRam = nullptr;
}

void reset(bool hard)
{
	memset(masterBridge.regs, 0, sizeof(masterBridge.regs));
	memset(slaveBridge.regs, 0, sizeof(slaveBridge.regs));
	memset(romBoardRam, 0, sizeof(romBoardRam));
	romBoardOffset = romBoardReg8 = romBoardRegC = 0;
	cryptoFirstRead = false;
	cryptoReset();
	reg00400000 = 0;
	reg01000000 = reg01000100 = 0;
	idChipReg = 0;
	idChip.reset();
	initIdChip();
	reg1a800008 = reg1b000100 = 0;
	portAValue = 0;
	initBoardEeprom();
	initConfigEeprom();
	if (hard)
	{
		memset(texRam, 0, TEXRAM_SIZE);
		gpu::texRamWritten(0, TEXRAM_SIZE);
		memset(cmdRam, 0, CMDRAM_SIZE);
		memset(slaveRam, 0, SLAVE_RAM_SIZE);
	}
	aica::setExternalSource(1);
	gpu::reset(hard);
	mie::reset();
	slave::reset();
	updateBridgeIrq(masterBridge);
	updateBridgeIrq(slaveBridge);
}

void serialize(Serializer& ser)
{
	ser.serialize(texRam, TEXRAM_SIZE);
	ser.serialize(cmdRam, CMDRAM_SIZE);
	ser.serialize(slaveRam, SLAVE_RAM_SIZE);
	ser << masterBridge.regs;
	ser << slaveBridge.regs;
	ser << romBoardRam;
	ser << romBoardOffset;
	ser << romBoardReg8;
	ser << romBoardRegC;
	ser << reg00400000;
	ser << reg01000000;
	ser << reg01000100;
	ser << idChipReg;
	idChip.serialize(ser);
	ser << reg1a800008;
	ser << reg1b000100;
	ser << portAValue;
	boardEeprom.Serialize(ser);
	configEeprom.Serialize(ser);
	gpu::serialize(ser);
	mie::serialize(ser);
	slave::serialize(ser);
	// The optional sound board (chip 0 is saved with the other systems)
	aica::ChipScope scope(1);
	aica::serialize(ser);
}

void deserialize(Deserializer& deser)
{
	deser.deserialize(texRam, TEXRAM_SIZE);
	gpu::texRamWritten(0, TEXRAM_SIZE);
	deser.deserialize(cmdRam, CMDRAM_SIZE);
	deser.deserialize(slaveRam, SLAVE_RAM_SIZE);
	deser >> masterBridge.regs;
	deser >> slaveBridge.regs;
	deser >> romBoardRam;
	deser >> romBoardOffset;
	deser >> romBoardReg8;
	deser >> romBoardRegC;
	deser >> reg00400000;
	deser >> reg01000000;
	deser >> reg01000100;
	deser >> idChipReg;
	idChip.deserialize(deser);
	deser >> reg1a800008;
	deser >> reg1b000100;
	deser >> portAValue;
	boardEeprom.Deserialize(deser);
	configEeprom.Deserialize(deser);
	gpu::deserialize(deser);
	mie::deserialize(deser);
	slave::deserialize(deser);
	{
		aica::ChipScope scope(1);
		aica::deserialize(deser);
	}
	// The sound update always runs, but a state may hold it unscheduled or already due
	aica::rescheduleUpdate();
	updateBridgeIrq(masterBridge);
	updateBridgeIrq(slaveBridge);
}

}
