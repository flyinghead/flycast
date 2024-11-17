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
#pragma once
#include "naomi_cart.h"
#include "imgread/common.h"

class GDCartridge: public NaomiCartridge
{
public:
	GDCartridge(u32 size);
	~GDCartridge() override;

	void Init(LoadProgress *progress = nullptr, std::vector<u8> *digest = nullptr) override {
		device_start(progress, digest);
		device_reset();
	}
	void* GetDmaPtr(u32 &size) override;
	bool Read(u32 offset, u32 size, void* dst) override;
	u32 ReadMem(u32 address, u32 size) override;
	void WriteMem(u32 address, u32 data, u32 size) override;

	void SetGDRomName(const char *name, const char *parentName) { this->gdrom_name = name; this->gdrom_parent_name = parentName; }

	void Serialize(Serializer &ser) const override;
	void Deserialize(Deserializer &deser) override;

protected:
	virtual void process();
	virtual int schedCallback();
	void returnToNaomi(bool failed, u16 offsetl, u32 parameter);

	template<typename T>
	void peek(u32 address)
	{
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4);
		int size;
		switch (sizeof(T))
		{
		case 1:
			size = 4;
			break;
		case 2:
			size = 5;
			break;
		case 4:
			size = 6;
			break;
		}
		dimm_command = ((address >> 16) & 0x1ff) | (size << 9) | 0x8000;
		dimm_offsetl = address & 0xffff;
		dimm_parameterl = 0;
		dimm_parameterh = 0;
	}

	u8 *dimm_data = nullptr;
	u32 dimm_data_size = 0;
	u16 dimm_command;
	u16 dimm_offsetl;
	u16 dimm_parameterl;
	u16 dimm_parameterh;
	static constexpr u16 DIMM_STATUS = 0x111;
	int schedId;

private:
	static constexpr int FILENAME_LENGTH = 24;

	const char *gdrom_name = nullptr;
	const char *gdrom_parent_name = nullptr;

	u32 dimm_cur_address = 0;

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

	std::vector<bool> loadedSegments;
	static constexpr u32 SEGMENT_SIZE = 16_KB;
	std::unique_ptr<Disc> gdrom;
	u32 file_start = 0;
	u32 des_subkeys[32];

	void device_start(LoadProgress *progress, std::vector<u8> *digest);
	void device_reset();
	void find_file(const char *name, const u8 *dir_sector, u32 &file_start, u32 &file_size);

	inline void permutate(u32 &a, u32 &b, u32 m, int shift);
	void des_generate_subkeys(u64 key, u32 *subkeys);
	template<bool decrypt>
	u64 des_encrypt_decrypt(u64 src, const u32 *des_subkeys);
	u64 rev64(u64 src);
	void read_gdrom(Disc *gdrom, u32 sector, u8* dst, u32 count = 1, LoadProgress *progress = nullptr);
	void loadSegments(u32 offset, u32 size);
	void systemCmd(int cmd);
};
