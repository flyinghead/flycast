#pragma once

#include "types.h"
#include "emulator.h"

#include <string>
#include <vector>

struct RomBootID
{
	char boardName[16];
	char vendorName[32];
	char gameTitle[8][32];
	u16  year;
	u8   month;
	u8   day;
	char gameID[4];		// copied to eeprom[3]
	u16  romMode;		// != 0 => offset | 0x20000000
	u16  romWaitFlag;	// != 0 => configure G1 BUS
	u32  romWait[8];	// init values for SB_G1RRC, SB_G1RWC, SB_G1FRC, SB_G1FWC, SB_G1CRC, SB_G1CWC, SB_G1GDRC, SB_G1GDWC
	u16  romID[22][3];	// M2/M4-type ROM checksums
	u8   coinFlag[8][16];// Init instructions for EEPROM for each region
  	  	  	  	  	  	// b0: if 0, use default BIOS settings, if 1, use settings below
  	  	  	  	  	    // b1: bit 0: vertical monitor, bit 1: forces attract mode sound off
  	  	  	  	  	  	// b2: coin chute type: 0 common, 1; individual
  	  	  	  	  	    // b3: default coin setting (1-28)
  	  	  	  	  	    // b4: coin 1 rate (for manual coin setting)
  	  	  	  	  	    // b5: coin 2 rate (for manual coin setting)
  	  	  	  	  	  	// b6: credit rate (for manual coin setting)
  	  	  	  	  	    // b7: bonus credit rate (for manual coin setting)
  	  	  	  	  	    // b8-15: coin sequences
	char sequence[8][32];// text ("CREDIT TO START", "CREDIT TO CONTINUE", ...)
	u32  gameLoad[8][3]; // regions to copy from ROM to system RAM: offset, RAM address, length (offset=0xffffffff means end of list)
	u32  testLoad[8][3]; // same for game test mode
	u32  gamePC;		// main game entry point
	u32  testPC;		// test mode entry point
	u8   country;		// supported regions bitmap
	u8   cabinet;		// supported # of players bitmap: b0 1 player, b1 2 players, ...
	u8   resolution;	// 0: 31kHz, 1: 15kHz, other: no check
	u8   vertical;		// 1: horizontal mode, 2: vertical mode, other: no check
	u8   serialID;		// if 1, check the ROM/DIMM board serial# eeprom
	u8   serviceMode;	// 0: common, 1: individual, other: use configured setting
	u8   _res[210];		// _res[210]: if 0xFF then the header is unencrypted. if not then the header is encrypted starting at offset 0x010.

	// Note: this structure is copied to system RAM by the BIOS at location 0c01f400
};

struct Game;

class Cartridge
{
public:
	Cartridge(u32 size);
	virtual ~Cartridge();

	virtual void Init(LoadProgress *progress = nullptr, std::vector<u8> *digest = nullptr) {
		if (digest != nullptr)
			digest->clear();
	}
	virtual u32 ReadMem(u32 address, u32 size) = 0;
	virtual void WriteMem(u32 address, u32 data, u32 size) = 0;

	virtual bool Read(u32 offset, u32 size, void* dst);
	virtual bool Write(u32 offset, u32 size, u32 data);
	virtual void* GetPtr(u32 offset, u32& size);
	virtual void* GetDmaPtr(u32 &size) = 0;
	virtual void AdvancePtr(u32 size) = 0;
	virtual void Serialize(Serializer& ser) const {}
	virtual void Deserialize(Deserializer& deser) {}
	virtual void SetKey(u32 key) { }
	virtual void SetKeyData(u8 *key_data) { }
	virtual bool GetBootId(RomBootID *bootId) = 0;

	const Game *game = nullptr;

protected:
	u8* RomPtr;
	u32 RomSize;
};

class NaomiCartridge : public Cartridge
{
public:
	NaomiCartridge(u32 size) : Cartridge(size), RomPioOffset(0), RomPioAutoIncrement(false), DmaOffset(0), DmaCount(0xffff) {}

	u32 ReadMem(u32 address, u32 size) override;
	void WriteMem(u32 address, u32 data, u32 size) override;
	void* GetDmaPtr(u32 &size) override;
	void AdvancePtr(u32 size) override {}
	void Serialize(Serializer& ser) const override;
	void Deserialize(Deserializer& deser) override;
	bool GetBootId(RomBootID *bootId) override;

	void SetKey(u32 key) override { this->key = key; }

protected:
	virtual void DmaOffsetChanged(u32 dma_offset) {}
	virtual void PioOffsetChanged(u32 pio_offset) {}
	u32 RomPioOffset;
	bool RomPioAutoIncrement;
	u32 DmaOffset;
	u32 DmaCount;
	u32 key = 0;
};

class DecryptedCartridge : public NaomiCartridge
{
public:
	DecryptedCartridge(u8 *rom_ptr, u32 size) : NaomiCartridge(size) { free(RomPtr); RomPtr = rom_ptr; }
};

class M2Cartridge : public NaomiCartridge
{
public:
	M2Cartridge(u32 size) : NaomiCartridge(size) {}

	bool Read(u32 offset, u32 size, void* dst) override;
	bool Write(u32 offset, u32 size, u32 data) override;
	u16 ReadCipheredData(u32 offset);
	void Serialize(Serializer& ser) const override;
	void Deserialize(Deserializer& deser) override;
	void* GetDmaPtr(u32& size) override;
	bool GetBootId(RomBootID *bootId) override;

private:
	u8 naomi_cart_ram[64_KB];
};

class NaomiCartException : public FlycastException
{
public:
	NaomiCartException(const std::string& reason) : FlycastException(reason) {}
};

void naomi_cart_LoadRom(const std::string& path, const std::string& fileName, LoadProgress *progress);
void naomi_cart_Close();
int naomi_cart_GetPlatform(const char *path);
void naomi_cart_LoadBios(const char *filename);
void naomi_cart_ConfigureEEPROM();
void naomi_cart_serialize(Serializer& ser);
void naomi_cart_deserialize(Deserializer& deser);

extern u8 *naomi_default_eeprom;

extern Cartridge *CurrentCartridge;

struct ButtonDescriptor
{
   u32 source;
   const char *name;
   u32 target;
   u32 p2_target;	// map P1 input to JVS P2
   u32 p1_target;	// map P2 input to JVS P1
};

enum AxisType {
   Full,
   Half
};

struct AxisDescriptor
{
   const char *name;
   AxisType type;
   u32 axis;
   bool inverted;
};

struct InputDescriptors
{
   ButtonDescriptor buttons[18];
   AxisDescriptor axes[8];
};

extern InputDescriptors *NaomiGameInputs;
extern bool atomiswaveForceFeedback;
