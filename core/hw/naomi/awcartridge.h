/*
 * awcartridge.h
 *
 *  Created on: Nov 5, 2018
 *      Author: flyinghead
 *  Plagiarized from mame (mame/src/mame/machine/awboard.h)
 *  // license:BSD-3-Clause
 *  // copyright-holders:Olivier Galibert,Andreas Naive
 *
 */
#pragma once
#include "naomi_cart.h"

class AWCartridge: public Cartridge
{
public:
	AWCartridge(u32 size) : Cartridge(size) {}

	void Init(LoadProgress *progress = nullptr, std::vector<u8> *digest = nullptr) override;
	u32 ReadMem(u32 address, u32 size) override;
	void WriteMem(u32 address, u32 data, u32 size) override;

	void *GetDmaPtr(u32 &size) override;
	void AdvancePtr(u32 size) override;
	bool GetBootId(RomBootID *bootId) override;

	void SetKey(u32 key) override;
	void Serialize(Serializer& ser) const override;
	void Deserialize(Deserializer& deser) override;

private:
	void device_reset();
	u16 decrypt16(u32 address);
	void recalc_dma_offset(int mode);

	u32 rombd_key;
	u32 mpr_offset, mpr_bank;
	u32 epr_offset, mpr_file_offset;
	u16 mpr_record_index, mpr_first_file_index;
	u16 decrypted_buf[16];

	u32 dma_offset, dma_limit;
};
