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

#ifndef CORE_HW_NAOMI_AWCARTRIDGE_H_
#define CORE_HW_NAOMI_AWCARTRIDGE_H_

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
	std::string GetGameId() override;

	void SetKey(u32 key) override;
	void Serialize(Serializer& ser) const override;
	void Deserialize(Deserializer& deser) override;

private:
	void device_reset();

	enum { EPR, MPR_RECORD, MPR_FILE };

	u32 rombd_key;
	u32 mpr_offset, mpr_bank;
	u32 epr_offset, mpr_file_offset;
	u16 mpr_record_index, mpr_first_file_index;
	u16 decrypted_buf[16];

	u32 dma_offset, dma_limit;

	struct sbox_set {
		u8 S0[32];
		u8 S1[16];
		u8 S2[16];
		u8 S3[8];
	};

	static const u8 permutation_table[4][16];
	static const sbox_set sboxes_table[4];
	static const int xor_table[16];
	static u16 decrypt(u16 cipherText, u32 address, u8 key);
	u16 decrypt16(u32 address) { return decrypt(((u16 *)RomPtr)[address % (RomSize / 2)], address, rombd_key); }

	void recalc_dma_offset(int mode);
};

#endif /* CORE_HW_NAOMI_AWCARTRIDGE_H_ */
