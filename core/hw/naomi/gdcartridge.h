/*
 * gdcartridge.h
 *
 *  Created on: Nov 16, 2018
 *      Author: flyinghead
 *
 * From mame naomigd.h
 * // license:BSD-3-Clause
 * // copyright-holders:Olivier Galibert
 *
 */

#ifndef CORE_HW_NAOMI_GDCARTRIDGE_H_
#define CORE_HW_NAOMI_GDCARTRIDGE_H_

#include "naomi_cart.h"
#include "imgread/common.h"

class GDCartridge: public NaomiCartridge {
public:
	GDCartridge(u32 size) : NaomiCartridge(size)
	{
	}
	~GDCartridge() override
	{
		free(dimm_data);
	}
	void Init(LoadProgress *progress = nullptr, std::vector<u8> *digest = nullptr) override
	{
		device_start(progress, digest);
		device_reset();
	}
	void* GetDmaPtr(u32 &size) override;
	void AdvancePtr(u32 size) override;
	bool Read(u32 offset, u32 size, void* dst) override;
	std::string GetGameId() override;

	void SetGDRomName(const char *name, const char *parentName) { this->gdrom_name = name; this->gdrom_parent_name = parentName; }

private:
	enum { FILENAME_LENGTH=24 };

	const char *gdrom_name = nullptr;
	const char *gdrom_parent_name = nullptr;

	u32 dimm_cur_address = 0;

	u8 *dimm_data = nullptr;
	u32 dimm_data_size = 0;

	static const u32 DES_LEFTSWAP[];
	static const u32 DES_RIGHTSWAP[];
	static const u32 DES_SBOX1[];
	static const u32 DES_SBOX2[];
	static const u32 DES_SBOX3[];
	static const u32 DES_SBOX4[];
	static const u32 DES_SBOX5[];
	static const u32 DES_SBOX6[];
	static const u32 DES_SBOX7[];
	static const u32 DES_SBOX8[];
	static const u32 DES_MASK_TABLE[];
	static const u8 DES_ROTATE_TABLE[16];

	void device_start(LoadProgress *progress, std::vector<u8> *digest);
	void device_reset();
	void find_file(const char *name, const u8 *dir_sector, u32 &file_start, u32 &file_size);

	inline void permutate(u32 &a, u32 &b, u32 m, int shift);
	void des_generate_subkeys(u64 key, u32 *subkeys);
	template<bool decrypt>
	u64 des_encrypt_decrypt(u64 src, const u32 *des_subkeys);
	u64 rev64(u64 src);
	void read_gdrom(Disc *gdrom, u32 sector, u8* dst, u32 count = 1, LoadProgress *progress = nullptr);
};

#endif /* CORE_HW_NAOMI_GDCARTRIDGE_H_ */
