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
#include "hw/hwreg.h"
#include "hw/naomi/m4cartridge.h"
#include "hw/flashrom/at93cxx.h"
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

class SystemSpCart;

// rom board custom UART ports
class SerialPort : public ::SerialPort
{
public:
	class Pipe : public ::SerialPort::Pipe
	{
	public:
		virtual void serialize(Serializer& ser) const {}
		virtual void deserialize(Deserializer& deser) {}
	};

	SerialPort(SystemSpCart *cart, int index) : cart(cart), index(index) {
	}

	~SerialPort() {
		if (pipe != nullptr)
			delete pipe;
	}

	u8 readReg(u32 addr);

	void writeReg(u32 addr, u8 v);

	void serialize(Serializer& ser) const {
		if (pipe != nullptr)
			pipe->serialize(ser);
	}

	void deserialize(Deserializer& deser) {
		if (pipe != nullptr)
			pipe->deserialize(deser);
	}

	void setPipe(::SerialPort::Pipe *pipe) override {
		this->pipe = (Pipe *)pipe;
	}

	void updateStatus() override;

private:
	void flush()
	{
		if (pipe != nullptr)
			while (pipe->available())
				pipe->read();
	}

	SystemSpCart *cart;
	const int index;
	Pipe *pipe = nullptr;
};

//
// ATA registers for CompactFlash interface
//
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

class InPortManager
{
public:
	virtual u8 getCN9_17_24() = 0;
	virtual u8 getCN9_25_32() = 0;
	virtual u8 getCN9_33_40() = 0;
	virtual u8 getCN9_41_48() = 0;
	virtual u8 getCN9_49_56() = 0;
	virtual ~InPortManager() = default;
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
		if (!romBootId)
			return false;
		memcpy(bootId, romBootId.get(), sizeof(RomBootID));
		return true;
	}

	void updateInterrupt(u32 mask = 0);

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
	const char *mediaName = nullptr;
	const char *parentName = nullptr;
	FILE *chdFile = nullptr;
	chd_file *chd = nullptr;
	u32 hunkbytes = 0;
	std::unique_ptr<u8[]> hunkmem;
	u32 hunknum = ~0;
	std::unique_ptr<RomBootID> romBootId;

	AT93C46SerialEeprom eeprom;
	SerialPort uart1;
	SerialPort uart2;
	u16 bank = 0;
	int region = 0;
	std::unique_ptr<InPortManager> inPortManager;

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
	static constexpr u32 INT_UART1 = 0x01;
	static constexpr u32 INT_UART2 = 0x02;
	static constexpr u32 INT_DIMM = 0x08;
	static constexpr u32 INT_ATA = 0x10;

	static SystemSpCart *Instance;

	friend class BootIdLoader;
};

}
