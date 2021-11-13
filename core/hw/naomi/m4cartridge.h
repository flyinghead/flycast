/*
 * m4cartridge.h
 *
 *  Created on: Nov 5, 2018
 *      Author: flyinghead
 *  Plagiarized from mame (mame/src/mame/machine/naomim4.h)
 *  // license:BSD-3-Clause
 *  // copyright-holders:Olivier Galibert,Andreas Naive
 *
 */

#ifndef CORE_HW_NAOMI_M4CARTRIDGE_H_
#define CORE_HW_NAOMI_M4CARTRIDGE_H_

#include "naomi_cart.h"
#include "naomi_regs.h"

class M4Cartridge: public NaomiCartridge {
public:
	M4Cartridge(u32 size) : NaomiCartridge(size) { }
	~M4Cartridge() override;

	void Init(LoadProgress *progress = nullptr, std::vector<u8> *digest = nullptr) override
	{
		device_start();
		device_reset();
	}

	u32 ReadMem(u32 address, u32 size) override
	{
		if ((address & 0xff) == 0x34)
			return m4id & 0xff80;

		u32 data = NaomiCartridge::ReadMem(address, size);

		if ((address & 0xff) == (NAOMI_ROM_OFFSETH_addr & 0xff))
			// indicates that security PIC is present
			data |= 0x2000;

		return data;
	}
	bool Read(u32 offset, u32 size, void *dst) override;
	bool Write(u32 offset, u32 size, u32 data) override;

	void* GetDmaPtr(u32 &size) override;
	void AdvancePtr(u32 size) override;
	std::string GetGameId() override;
	void Serialize(Serializer& ser) const override;
	void Deserialize(Deserializer& deser) override;

	void SetKey(u32 key) override { this->m4id = key; }
	void SetKeyData(u8 *key_data) override { this->m_key_data = key_data; }

protected:
	void DmaOffsetChanged(u32 dma_offset) override;
	void PioOffsetChanged(u32 pio_offset) override;

private:
	void device_start();
	void device_reset();

	static const u8 k_sboxes[4][16];

	u16 m4id;
	u8 *m_key_data = NULL;			// 2048 bytes
	u16 subkey1, subkey2;
	u16 one_round[0x10000];

	u8 buffer[32768];
	u32 rom_cur_address, buffer_actual_size;
	u16 iv;
	u8 counter;
	bool encryption;
	bool cfi_mode;
	bool xfer_ready;

	void enc_init();
	void enc_reset();
	void enc_fill();
	u16 decrypt_one_round(u16 word, u16 subkey);
};

#endif /* CORE_HW_NAOMI_M4CARTRIDGE_H_ */
