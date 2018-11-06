#ifndef NAOMI_CART_H
#define NAOMI_CART_H

#include <string>
#include "types.h"

class Cartridge
{
public:
	Cartridge(u32 size);
	virtual ~Cartridge();

	virtual u32 ReadMem(u32 address, u32 size) = 0;
	virtual void WriteMem(u32 address, u32 data, u32 size) = 0;

	virtual bool Read(u32 offset, u32 size, void* dst);
	virtual bool Write(u32 offset, u32 size, u32 data);
	virtual void* GetPtr(u32 offset, u32& size);
	virtual void* GetDmaPtr(u32 &size) = 0;
	virtual void AdvancePtr(u32 size) = 0;
	virtual std::string GetGameId();
	virtual void Serialize(void **data, unsigned int *total_size) {}
	virtual void Unserialize(void **data, unsigned int *total_size) {}

protected:
	u8* RomPtr;
	u32 RomSize;
};

class NaomiCartridge : public Cartridge
{
public:
	NaomiCartridge(u32 size) : Cartridge(size), RomPioOffset(0), RomPioAutoIncrement(0), DmaOffset(0), DmaCount(0xffff) {}

	virtual u32 ReadMem(u32 address, u32 size);
	virtual void WriteMem(u32 address, u32 data, u32 size);
	virtual void* GetDmaPtr(u32 &size);
	virtual void AdvancePtr(u32 size);
	virtual void Serialize(void** data, unsigned int* total_size);
	virtual void Unserialize(void** data, unsigned int* total_size);

	void SetKey(u32 key) { this->key = key; }

protected:
	virtual void DmaOffsetChanged(u32 dma_offset) {}
	virtual void PioOffsetChanged(u32 pio_offset) {}
	u32 RomPioOffset;
	bool RomPioAutoIncrement;
	u32 DmaOffset;
	u32 DmaCount;
	u32 key;
};

class DecryptedCartridge : public NaomiCartridge
{
public:
	DecryptedCartridge(u8 *rom_ptr, u32 size) : NaomiCartridge(size) { RomPtr = rom_ptr; }
	// FIXME Must do a munmap and close for each segment
};

class M2Cartridge : public NaomiCartridge
{
public:
	M2Cartridge(u32 size) : NaomiCartridge(size) {}

	virtual bool Read(u32 offset, u32 size, void* dst);
	virtual bool Write(u32 offset, u32 size, u32 data);
	u16 ReadCipheredData(u32 offset);
	virtual void Serialize(void** data, unsigned int* total_size);
	virtual void Unserialize(void** data, unsigned int* total_size);

private:
	u8 naomi_cart_ram[64 * 1024];
};

bool naomi_cart_SelectFile(void* handle);

extern char naomi_game_id[];

extern Cartridge *CurrentCartridge;

#endif //NAOMI_CART_H
