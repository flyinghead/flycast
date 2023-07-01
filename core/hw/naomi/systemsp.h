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
#pragma once
#include "types.h"
#include "emulator.h"
#include "hw/naomi/m4cartridge.h"
#include "hw/flashrom/flashrom.h"
#include "serialize.h"
#include <deque>
#include <array>
#include <memory>
#include <libchdr/chd.h>

namespace systemsp
{

template<typename T>
T readMemArea0(u32 addr);
template<typename T>
void writeMemArea0(u32 addr, T v);

class SerialEeprom93Cxx : public WritableChip
{
public:
	SerialEeprom93Cxx(u32 size) : WritableChip(size) {
		memset(data, 0xff, size);
	}

	// combined DO + READY/BUSY
	int readDO()
	{
		if (dataOutBits > 0)
			// DO
			return (dataOut >> (dataOutBits - 1)) & 1;
		else
			// Ready
			return 1;
	}

	// chip select (active high)
	void writeCS(int state) {
		cs = state;
	}

	// clock
	void writeCLK(int state);

	// data in
	void writeDI(int state) {
		di = state;
	}

	void Write(u32 addr, u32 data, u32 size) override {
		die("Unsupported");
	}

	void serialize(Serializer& ser) const
	{
		ser << cs;
		ser << clk;
		ser << di;
		ser << (u32)command.size();
		for (bool b : command)
			ser << b;
		ser << expected;
		ser << writeEnable;
		ser << dataOut;
		ser << dataOutBits;
	}
	void deserialize(Deserializer& deser)
	{
		deser >> cs;
		deser >> clk;
		deser >> di;
		u32 size;
		deser >> size;
		command.resize(size);
		for (u32 i = 0; i < size; i++) {
			bool b;
			deser >> b;
			command[i] = b;
		}
		deser >> expected;
		deser >> writeEnable;
		deser >> dataOut;
		deser >> dataOutBits;
	}

	void save(const std::string& path);

private:
	u8 getCommandAddress() const
	{
		verify(command.size() >= 9);
		u8 addr = 0;
		for (int i = 3; i < 9; i++) {
			addr <<= 1;
			addr |= command[i];
		}
		return addr;
	}

	u16 getCommandData() const
	{
		verify(command.size() >= 25);
		u16 v = 0;
		for (int i = 9; i < 25; i++) {
			v <<= 1;
			v |= command[i];
		}
		return v;
	}

	u8 cs = 0;
	u8 clk = 0;
	u8 di = 0;
	std::vector<bool> command;
	int expected = 0;
	bool writeEnable = false;
	u16 dataOut = 0;
	u8 dataOutBits = 0;
};

class SerialPort
{
public:
	SerialPort(int index) : index(index)
	{
	}

	u8 readReg(u32 addr);

	void writeReg(u32 addr, u8 v);

	bool hasData() const {
		return !toSend.empty();
	}

	void serialize(Serializer& ser) const
	{
		ser << (u32)toSend.size();
		for (u8 b : toSend)
			ser << b;
		ser << expectedBytes;
		ser << (u32)recvBuffer.size();
		ser.serialize(recvBuffer.data(), recvBuffer.size());
	}
	void deserialize(Deserializer& deser)
	{
		u32 size;
		deser >> size;
		toSend.resize(size);
		for (u32 i = 0; i < size; i++)
			deser >> toSend[i];
		deser >> expectedBytes;
		deser >> size;
		recvBuffer.resize(size);
		deser.deserialize(recvBuffer.data(), recvBuffer.size());
	}

private:
	const int index;
	std::deque<u8> toSend;
	std::array<u8, 128> cardData;
	u8 expectedBytes = 0;
	std::vector<u8> recvBuffer;
};

union AtaStatusRegister
{
	u8 full;
	struct {
		u8 err:1;	// error
		u8 idx:1;	// index, always 0
		u8 corr:1;	// corrected data, always 0
		u8 drq:1;	// pio data available
		u8 dsc:1;	// seek complete
		u8 df:1;	// drive fault error
		u8 rdy:1;	// drive ready
		u8 bsy:1;	// drive busy
	};
};

union AtaDriveHeadRegister
{
	u8 full;
	struct {
		u8 head:4;
		u8 drv:1;
		u8 :1;
		u8 lba:1;
		u8 :1;
	};
};

union AtaDevCtrlRegister
{
	u8 full;
	struct {
		u8 :1;
		u8 nien:1;	// disable interrupts
		u8 srst:1;	// software reset
		u8 :4;
		u8 hob:1;	// high-order byte
	};
};

class SystemSpCart : public M4Cartridge
{
public:
	SystemSpCart(u32 size);
	~SystemSpCart();

	void Init(LoadProgress *progress = nullptr, std::vector<u8> *digest = nullptr) override;
	void setMediaName(const char *mediaName, const char *parentName) {
		this->mediaName = mediaName;
		this->parentName = parentName;
	}

	u32 ReadMem(u32 address, u32 size) override;
	void WriteMem(u32 address, u32 data, u32 size) override;

	bool Read(u32 offset, u32 size, void *data) override;
	bool Write(u32 offset, u32 size, u32 data) override;

	template<typename T>
	T readMemArea0(u32 addr);
	template<typename T>
	void writeMemArea0(u32 addr, T v);

	void Serialize(Serializer &ser) const override;
	void Deserialize(Deserializer &deser) override;

	bool GetBootId(RomBootID *bootId) override
	{
		if (mediaName == nullptr)
			return M4Cartridge::GetBootId(bootId);
		else
			// TODO
			return false;
	}

private:
	static int schedCallback(int tag, int sch_cycl, int jitter, void *arg);
	int schedCallback();
	chd_file *openChd(const std::string path);
	void readSectors();
	void saveFiles();
	std::string getEepromPath() const;

	static void handleEvent(Event event, void *p) {
		((SystemSpCart *)p)->saveFiles();
	}

	int schedId;
	const char *mediaName;
	const char *parentName;
	FILE *chdFile = nullptr;
	chd_file *chd = nullptr;
	u32 hunkbytes = 0;
	std::unique_ptr<u8[]> hunkmem;
	u32 hunknum = ~0;

	SerialEeprom93Cxx eeprom;
	SerialPort uart1;
	SerialPort uart2;
	u16 bank = 0;
	int region = 0;
	u32 last_kcode[2] = {};

	static constexpr u32 SECTOR_SIZE = 512;

	struct {
		u8 features = 0;
		u16 cylinder = 0;
		u8 sectorCount = 0;
		u8 sectorNumber = 0;
		AtaStatusRegister status = { 0x50 };
		u8 error = 0;
		AtaDriveHeadRegister driveHead = { 0xa0 };
		AtaDevCtrlRegister devCtrl = { 0 };
		u8 interruptPending = 0;
		u8 reg84 = 0;
		u8 buffer[SECTOR_SIZE];
		u32 bufferIndex = 0;
	} ata;

	enum class CmdState {
		INIT,
		AAA_AA_1,
		_555_55_1,
		WRITE_BUF_1,
		WRITE_BUF_2,
		PROGRAM,
		AAA_80,
		AAA_AA_2,
		_555_55_2,
	};
	struct {
		CmdState cmdState = CmdState::INIT;
		u32 progAddress = ~0;
		u16 wordCount = 0;
	} flash;

public:
	static SystemSpCart *Instance;
};

}
