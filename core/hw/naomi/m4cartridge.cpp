/*
 * m4cartridge.cpp
 *
 *  Created on: Nov 5, 2018
 *      Author: flyinghead
 *  Plagiarized from mame (mame/src/mame/machine/naomim4.cpp)
 *  // license:BSD-3-Clause
 *  // copyright-holders:Olivier Galibert,Andreas Naive
 */

#include "m4cartridge.h"
#include "serialize.h"


// Decoder for M4-type NAOMI cart encryption

// In hardware, the decryption is managed by the XC3S50 Xilinx Spartan FPGA (IC2)
// and the annexed PIC16C621A PIC MCU (IC3).
// - The FPGA control the clock line of the security PIC.
// - The protocol between the FPGA and the MCU is nibble-based, though it hasn't been RE for now.
// - The decryption algorithm is clearly nibble-based too.

// The decryption algorithm itself implements a stream cipher built on top of a 16-bits block cipher.
// The underlying block-cipher is a SP-network of 2 rounds (both identical in structure). In every
// round, the substitution phase is done using 4 fixed 4-to-4 sboxes acting on every nibble. The permutation
// phase is indeed a nibble-based linear combination.
// With that block cipher, a stream cipher is constructed by feeding the output result of the 1st round
// of a certain 16-bits block as a whitening value for the next block. The cart dependent data used by
// the algorithm is a 32-bits key stored in the PIC16C621A. The hardware auto-reset the feed value
// to the cart-based IV every 16 blocks (32 bytes); that reset is not address-based, but index-based.

const u8 M4Cartridge::k_sboxes[4][16] = {
	{9,8,2,11,1,14,5,15,12,6,0,3,7,13,10,4},
	{2,10,0,15,14,1,11,3,7,12,13,8,4,9,5,6},
	{4,11,3,8,7,2,15,13,1,5,14,9,6,12,0,10},
	{1,13,8,2,0,5,6,14,4,11,15,10,12,3,7,9}
};

// from S29GL512N datasheet
static u8 cfidata[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x51,0x00,0x52,0x00,0x59,0x00,0x02,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x27,0x00,0x36,0x00,0x00,0x00,0x00,0x00,0x07,0x00,
0x07,0x00,0x0a,0x00,0x00,0x00,0x01,0x00,0x05,0x00,0x04,0x00,0x00,0x00,0x1a,0x00,0x02,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x01,0x00,0xff,0x00,0x01,0x00,0x00,0x00,
0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x50,0x00,0x52,0x00,0x49,0x00,0x31,0x00,0x33,0x00,0x10,0x00,0x02,0x00,0x01,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0xb5,0x00,0xc5,0x00,0x04,0x00,
0x01,0x00
};

void M4Cartridge::device_start()
{
	if (m4id == 0)
	{
		INFO_LOG(COMMON, "Warning: M4 ID not provided\n");
		m4id = 0x5504;
	}

	subkey1 = (m_key_data[0x5e2] << 8) | m_key_data[0x5e0];
	subkey2 = (m_key_data[0x5e6] << 8) | m_key_data[0x5e4];

	enc_init();
}

void M4Cartridge::enc_init()
{
	for (int round_input = 0; round_input < 0x10000; round_input++)
	{
		u8 input_nibble[4];
		u8 output_nibble[4];

		for (int nibble_idx = 0; nibble_idx < 4; ++nibble_idx) {
			input_nibble[nibble_idx] = (round_input >> (nibble_idx*4)) & 0xf;
			output_nibble[nibble_idx] = 0;
		}

		u8 aux_nibble = input_nibble[3];
		for (int nibble_idx = 0; nibble_idx < 4; ++nibble_idx) { // 4 s-boxes per round
			aux_nibble ^= k_sboxes[nibble_idx][input_nibble[nibble_idx]];
			for (int i = 0; i < 4; ++i)  // diffusion of the bits
				output_nibble[(nibble_idx - i) & 3] |= aux_nibble & (1 << i);
		}

		u16 result = 0;
		for (int nibble_idx = 0; nibble_idx < 4; ++nibble_idx)
			result |= (output_nibble[nibble_idx] << (4 * nibble_idx));

		one_round[round_input] = result;
	}
}

void M4Cartridge::device_reset()
{
	rom_cur_address = 0;
	buffer_actual_size = 0;
	encryption = false;
	cfi_mode = false;
	counter = 0;
	iv = 0;
	xfer_ready = false;
}

void M4Cartridge::DmaOffsetChanged(u32 dma_offset)
{
	xfer_ready = false;
}

void M4Cartridge::PioOffsetChanged(u32 pio_offset)
{
	encryption = pio_offset & 0x40000000;
	xfer_ready = false;
}


bool M4Cartridge::Read(u32 offset, u32 size, void *dst) {
	if (cfi_mode)
	{
		u32 fpr_num = m4id & 0x7f;

		if (((offset >> 26) & 0x07) < fpr_num) {
			*(u16 *)dst = *(u16 *)&cfidata[offset & 0xffff];
			return true;
		}
	}
	if (!xfer_ready)
	{
		rom_cur_address = RomPioOffset & 0x1ffffffe;
		if (encryption)
		{
			//printf("M4 CRYPT m4id %x skey1 %x skey2 %x RomPioOffset %08x\n", m4id, subkey1, subkey2, RomPioOffset);
			enc_reset();
			enc_fill();
		}
		xfer_ready = true;
	}
	if (encryption)
	{
		switch (size)
		{
		case 2:
			*(u16 *)dst = *(u16 *)buffer;
			break;
		case 4:
			*(u32 *)dst = *(u32 *)buffer;
			break;
		}
		if (RomPioAutoIncrement)
			AdvancePtr(size);

		return true;
	}
	else
		return NaomiCartridge::Read(offset & 0x1ffffffe, size, dst);
}

void *M4Cartridge::GetDmaPtr(u32 &size)
{
	static u8 retzero[2] = { 0, 0 };

	if (cfi_mode) {
		u32 fpr_num = m4id & 0x7f;

		if (((rom_cur_address >> 26) & 0x07) < fpr_num) {
			size = std::min(size, 2u);
			return &cfidata[rom_cur_address & 0xffff];
		}
	}

	if (!xfer_ready)
	{
		rom_cur_address = DmaOffset & 0x1ffffffe;
		if (encryption)
		{
			//printf("M4 CRYPT m4id %x skey1 %x skey2 %x DmaOffset %08x\n", m4id, subkey1, subkey2, DmaOffset);
			enc_reset();
			enc_fill();
		}
		xfer_ready = true;
	}
	if (encryption)
	{
		size = std::min(size, (u32)sizeof(buffer));
		return buffer;

	}
	else
	{
		if ((DmaOffset & 0x1ffffffe) < RomSize)
		{
			size = std::min(size, RomSize - (DmaOffset & 0x1ffffffe));
			return RomPtr + (DmaOffset & 0x1ffffffe);
		}
		else
		{
			size = 2;
			return retzero;
		}
	}
}

void M4Cartridge::AdvancePtr(u32 size)
{
	if (encryption)
	{
		if (size < buffer_actual_size)
		{
			memmove(buffer, buffer + size, buffer_actual_size - size);
			buffer_actual_size -= size;
		}
		else
			buffer_actual_size = 0;
		enc_fill();
	}
	else
		rom_cur_address += size;
}

void M4Cartridge::enc_reset()
{
	buffer_actual_size = 0;
	iv = 0;
	counter = 0;
}

u16 M4Cartridge::decrypt_one_round(u16 word, u16 subkey)
{
	return one_round[word ^ subkey] ^ subkey ;
}

void M4Cartridge::enc_fill()
{
	const u8 *base = RomPtr + rom_cur_address;
	while (buffer_actual_size < sizeof(buffer))
	{
		u16 enc = base[0] | (base[1] << 8);
		u16 dec = iv;
		iv = decrypt_one_round(enc ^ iv, subkey1);
		dec ^= decrypt_one_round(iv, subkey2);

		buffer[buffer_actual_size++] = dec;
		buffer[buffer_actual_size++] = dec >> 8;

		base += 2;
		rom_cur_address += 2;

		counter++;
		if(counter == 16) {
			counter = 0;
			iv = 0;
		}
	}
//	printf("Decrypted M4 data:\n");
//	for (int i = 0; i < buffer_actual_size; i++)
//	{
//		printf("%c ", buffer[i]);
//		if ((i + 1) % 16 == 0)
//			printf("\n");
//	}
//	printf("\n");


}

bool M4Cartridge::Write(u32 offset, u32 size, u32 data)
{
	if (((offset&0xffff) == 0x00aa) && (data == 0x0098))
		cfi_mode = true;
	if (((offset&0xffff) == 0x0000) && (data == 0x00f0))
		cfi_mode = false;
	return true;	// ?
}

M4Cartridge::~M4Cartridge()
{
	free(m_key_data);
}

std::string M4Cartridge::GetGameId()
{
	if (RomSize < 0x30 + 0x20)
		return "(ROM too small)";

	std::string game_id;
	if (RomPtr[0] == 'N' && RomPtr[1] == 'A')
		game_id = std::string((char *)(RomPtr + 0x30), 0x20);
	else
	{
		rom_cur_address = 0;
		enc_reset();
		enc_fill();

		game_id = std::string((char *)(buffer + 0x30), 0x20);
	}

	while (!game_id.empty() && game_id.back() == ' ')
		game_id.pop_back();
	return game_id;
}

void M4Cartridge::Serialize(Serializer& ser) const
{
	ser << buffer;
	ser << rom_cur_address;
	ser << buffer_actual_size;
	ser << iv;
	ser << counter;
	ser << encryption;
	ser << cfi_mode;
	ser << xfer_ready;

	NaomiCartridge::Serialize(ser);
}

void M4Cartridge::Deserialize(Deserializer& deser)
{
	deser >> buffer;
	deser >> rom_cur_address;
	deser >> buffer_actual_size;
	deser >> iv;
	deser >> counter;
	deser >> encryption;
	deser >> cfi_mode;
	deser >> xfer_ready;

	NaomiCartridge::Deserialize(deser);
}
