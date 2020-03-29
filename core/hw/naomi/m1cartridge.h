/*
 * m1cartridge.h
 *
 *  Created on: Nov 4, 2018
 *      Author: flyinghead
 *  Plagiarized from mame (mame/src/mame/machine/naomim1.h)
 *  // license:BSD-3-Clause
 *  // copyright-holders:Olivier Galibert
 */
#pragma once
#include "naomi_cart.h"

class M1Cartridge : public NaomiCartridge
{
public:
	M1Cartridge(u32 size);

	virtual u32 ReadMem(u32 address, u32 size) override
	{
		if ((address & 0xff) == 0x14)
			return actel_id;
		return NaomiCartridge::ReadMem(address, size);
	}

	virtual void* GetDmaPtr(u32 &size) override
	{
		if (encryption)
		{
			size = std::min(size, (u32)sizeof(buffer));
			return buffer;
		}
		else
			return NaomiCartridge::GetDmaPtr(size);
	}

	virtual void AdvancePtr(u32 size) override;
	virtual void Serialize(void** data, unsigned int* total_size) override;
	virtual void Unserialize(void** data, unsigned int* total_size) override;

	void setActelId(u32 actel_id) { this->actel_id = actel_id; }

protected:
	virtual void DmaOffsetChanged(u32 dma_offset) override
	{
		rom_cur_address = dma_offset & 0x1fffffff;
		if ((dma_offset & 0x20000000) == 0 && rom_cur_address < RomSize)
		{
			//printf("M1 ENCRYPTION ON @ %08x\n", dma_offset);
			encryption = true;
			enc_reset();
			enc_fill();
		}
		else
			encryption = false;
	}

private:
	u32 get_decrypted_32b();

	inline u32 lookb(int bits)
	{
		if(bits > avail_bits) {
			avail_val = (avail_val << 32) | get_decrypted_32b();
			avail_bits += 32;
		}
		return (avail_val >> (avail_bits - bits)) & ((1 << bits)-1);
	}

	inline void skipb(int bits)
	{
		avail_bits -= bits;
	}

	inline u32 getb(int bits)
	{
		u32 res = lookb(bits);
		skipb(bits);
		return res;
	}

	void gb_reset()
	{
		avail_val = 0;
		avail_bits = 0;
	}

	void enc_reset()
	{
		gb_reset();
		stream_ended = false;
		has_history = false;
		buffer_actual_size = 0;

		for(auto & elem : dict)
			elem = getb(8);
	}

	void wb(u8 byte);
	void enc_fill();

	u16 actel_id;

	u8 buffer[32768];
	u8 dict[111], hist[2];
	u64 avail_val;
	u32 rom_cur_address, buffer_actual_size, avail_bits;
	bool stream_ended, has_history;
	bool encryption;
};
