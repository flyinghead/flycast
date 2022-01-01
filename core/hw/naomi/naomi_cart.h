#pragma once

#include <algorithm>
#include <string>
#include "types.h"
#include "emulator.h"

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
	virtual std::string GetGameId();
	virtual void Serialize(Serializer& ser) const {}
	virtual void Deserialize(Deserializer& deser) {}
	virtual void SetKey(u32 key) { }
	virtual void SetKeyData(u8 *key_data) { }

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
	std::string GetGameId() override;

private:
	u8 naomi_cart_ram[64 * 1024];
};

class NaomiCartException : public FlycastException
{
public:
	NaomiCartException(const std::string& reason) : FlycastException(reason) {}
};

void naomi_cart_LoadRom(const char* file, LoadProgress *progress);
void naomi_cart_Close();
int naomi_cart_GetPlatform(const char *path);
void naomi_cart_LoadBios(const char *filename);

extern char naomi_game_id[];
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
