/*
 * awcartridge.cpp
 *
 *  Created on: Nov 5, 2018
 *      Author: flyinghead
 *  Plagiarized from mame (mame/src/mame/machine/awboard.cpp)
 *  // license:BSD-3-Clause
 *  // copyright-holders:Olivier Galibert,Andreas Naive
 *
 */

/*

Atomiswave ROM board specs from Cah4e3 @ http://cah4e3.wordpress.com/2009/07/26/some-atomiswave-info/

 AW_EPR_OFFSETL                                          Register addres: 0x5f7000
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                         EPR data offset low word                              |
 +-------------------------------------------------------------------------------+

 AW_EPR_OFFSETH                                          Register addres: 0x5f7004
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                          EPR data offset hi word                              |
 +-------------------------------------------------------------------------------+

  Both low and high words of 32-bit offset from start of EPR-ROM area. Used for
  reading header and program code data, cannot be used for reading MPR-ROMs data.
  During program code DMA transfer Romeo MCU perform data checksuming (decrypted data, in 8bit units),
  result must match some CPLD / encryption key-specific value, otherwise MPR-ROM access
  described below will not work correctly.
  Game header (first 256 bytes of ROM) is not checksum protected.

 AW_MPR_RECORD_INDEX                                     Register addres: 0x5f700c
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                          File system record index                             |
 +-------------------------------------------------------------------------------+

  This register contains index of MPR-ROM file system record (64-bytes in size) to
  read throught DMA. Internal DMA offset register is assigned as AW_MPR_RECORD_INDEX<<6
  from start of MPR-ROM area. Size of DMA transaction not limited, it is possible
  to read any number of records or just part of it.

 AW_MPR_FIRST_FILE_INDEX                                 Register addres: 0x5f7010
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                           First file record index                             |
 +-------------------------------------------------------------------------------+

  This register assign for internal cart circuit index of record in MPR-ROM file
  system sub-area that contain information about first file of MPR-ROM files
  sub-area. Internal circuit using this record to read absolute first file offset
  from start of MPR-ROM area and calculate normal offset for each other file
  requested, since MPR-ROM file data sub-area can be assighed only with relative
  offsets from start of such sub-area.

 AW_MPR_FILE_OFFSETL                                     Register addres: 0x5f7014
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                         MPR file offset low word                              |
 +-------------------------------------------------------------------------------+

 AW_MPR_FILE_OFFSETH                                     Register addres: 0x5f7018
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                          MPR file offset hi word                              |
 +-------------------------------------------------------------------------------+

  Both low and high words of 32-bit relative offset from start of MPR-ROM files
  sub-area. Used by internal circuit to calculate absolute offset using data
  from AW_MPR_FIRST_FILE_INDEX register. Cannot be used for reading EPR-ROM
  data nor even MPR-ROM file system sub-area data.

 AW_PIO_DATA                                             Register addres: 0x5f7080
 +-------------------------------------------------------------------------------+
 |                                  bit15-0                                      |
 +-------------------------------------------------------------------------------+
 |                Read/Write word from/to ROM board address space                |
 +-------------------------------------------------------------------------------+

  Using this register data can be read or written to ROM BD at AW_EPR_OFFSET directly,
  decryption is not used, flash ROMs (re)programming via CFI commands possible.

  Type 2 ROM BD have MPR_BANK register at AW_EPR_OFFSET 007fffff, which selects
  1 of 4 mask ROM banks.

ROM board internal layouts:

 Type 1:

 00000000 - 00800000 IC18 flash ROM
 00800000 - 01000000 IC10 or mirror of above
 01000000 - 02000000 IC11 \
        .....               mask ROMs
 07000000 - 08000000 IC17 /

 Type 2:

 00000000 - 00800000 FMEM1 flash ROM
 00800000 - 01000000 mirror of above
 01000000 - 01800000 FMEM2 flash ROM
 01800000 - 02000000 mirror of above
 02000000 - 04000000 MROM1 MROM4 MROM7 MROM10 \
 04000000 - 06000000 MROM2 MROM5 MROM8 MROM11   banked mask ROMs
 06000000 - 08000000 MROM3 MROM6 MROM9 MROM12 /

 Type 3:

 00000000 - 01000000 U3  flash ROM
 01000000 - 02000000 U1  flash ROM
 02000000 - 03000000 U4  flash ROM
 03000000 - 04000000 U2  flash ROM
 04000000 - 05000000 U15 flash ROM
 05000000 - 06000000 U17 flash ROM
 06000000 - 07000000 U14 flash ROM
 07000000 - 08000000 U16 flash ROM

 Development:

 00000000 - 00800000 IC12 \
        .....               flash ROMs
 07800000 - 08000000 IC27 /


 In short:

     EPR-ROM
 +--------------+ 0x00000000
 |              |
 |    HEADER    +- AW_EPR_OFFSET << 1
 |              |
 +--------------+
 |              |
 |     CODE     +- AW_EPR_OFFSET << 1
 |              |
 |              |
 +--------------+ 0x007fffff

     MPR-ROMS
 +--------------+ 0x00000000
 | FS_HEADER    |
 | FS_RECORD[1] +- (AW_MPR_RECORD_INDEX << 6)
 | FS_RECORD[2] |
 | FS_RECORD[3] +- (AW_MPR_FIRST_FILE_INDEX << 6)
 |     ...      |
 | FS_RECORD[N] |
 +--------------+- FS_RECORD[AW_MPR_FIRST_FILE_INDEX].FILE_ABS_OFFSET
 | FILE_0       |
 | FILE_1       +- (AW_MPR_FILE_OFFSET << 1) + FS_RECORD[AW_MPR_FIRST_FILE_INDEX].FILE_ABS_OFFSET
 |     ...      |
 | FILE_N       |
 +--------------+ 0x07ffffff

*/
#include "awcartridge.h"
#include "awave_regs.h"
#include "serialize.h"

u32 AWCartridge::ReadMem(u32 address, u32 size) {
	verify(size != 1);
	switch(address & 255)
	{
//	case AW_EPR_OFFSETH_addr:
//		break;
//	case AW_EPR_OFFSETL_addr:
//		break;
//	case AW_MPR_RECORD_INDEX_addr:
//		break;
//	case AW_MPR_FIRST_FILE_INDEX_addr:
//		break;
//	case AW_MPR_FILE_OFFSETH_addr:
//		break;
//	case AW_MPR_FILE_OFFSETL_addr:
//		break;
	case AW_PIO_DATA_addr:
		{
			u32 roffset = epr_offset & 0x3ffffff;
			if (roffset >= (mpr_offset / 2))
				roffset += mpr_bank * 0x4000000;
			u16 retval = (RomSize > (roffset * 2)) ? ((u16 *)RomPtr)[roffset] : 0; // not endian-safe?
			DEBUG_LOG(NAOMI, "AWCART ReadMem %08x: %x", address, retval);
			return retval;
		}
	default:
		INFO_LOG(NAOMI, "Unhandled awcart read %X, %d", address, size);
		return 0xffff;
	}
}

void AWCartridge::WriteMem(u32 address, u32 data, u32 size)
{
	switch(address & 255)
	{
	case AW_EPR_OFFSETH_addr:
		epr_offset = (epr_offset & 0x0000ffff) | (data << 16);
		recalc_dma_offset(EPR);
		break;

	case AW_EPR_OFFSETL_addr:
		epr_offset = (epr_offset & 0xffff0000) | (u16)data;
		recalc_dma_offset(EPR);
		break;

	case AW_MPR_RECORD_INDEX_addr:
		mpr_record_index = data;
		recalc_dma_offset(MPR_RECORD);
		break;

	case AW_MPR_FIRST_FILE_INDEX_addr:
		mpr_first_file_index = data;
		recalc_dma_offset(MPR_FILE);
		break;

	case AW_MPR_FILE_OFFSETH_addr:
		mpr_file_offset = (mpr_file_offset & 0x0000ffff) | (data << 16);
		recalc_dma_offset(MPR_FILE);
		break;

	case AW_MPR_FILE_OFFSETL_addr:
		mpr_file_offset = (mpr_file_offset & 0xffff0000) | (u16)data;
		recalc_dma_offset(MPR_FILE);
		break;

	case AW_PIO_DATA_addr:
		// write to ROM board address space, including FlashROM programming using CFI (TODO)
		DEBUG_LOG(NAOMI, "Write to AW_PIO_DATA: %x", data);
		if (epr_offset == 0x7fffff)
			mpr_bank = data & 3;
		break;

	default:
		INFO_LOG(NAOMI, "Unhandled awcart write %X: %d sz %d", address, data, size);
		break;
	}
}

/*
We are using 8 bits keys with the following subfields' structure:
bits 0-3 is an index of 16-bits XOR (only 11 was used in known games)
bits 4-5 is an index to the sboxes table
bits 6-7 is an index to the permutation table

These subfields could be differing from the "real" ones in the following ways:
- Current keys equal to decrypted game code binary 8-bit sum (of each byte's swapped 4-bit nibbles)
- Every one of the index subfields could be suffering an arbitrary bitswap and XOR
- The 16-bits-XOR subfield could suffer an arbitrary XOR which could depend on the 4 index bits (that is: a different XOR per every index combination)
- Of course, the way in which we are mixing 3 subfields in one only key is arbitrary too.
*/


const u8 AWCartridge::permutation_table[4][16] =
{
	{14,1,11,15,7,3,8,13,0,4,2,12,6,10,5,9},
	{8,10,1,3,7,4,11,2,5,15,6,0,12,13,9,14},
	{4,5,9,6,1,13,7,11,10,0,14,12,8,15,2,3},
	{12,7,11,2,0,5,15,6,1,8,14,4,9,13,3,10}
};

const AWCartridge::sbox_set AWCartridge::sboxes_table[4] =
{
	{
		{11,8,6,25,2,7,23,28,5,10,21,20,1,26,17,19,14,27,22,30,15,4,9,24,31,3,16,12,0,18,29,13},
		{13,5,9,6,4,2,11,10,12,0,8,1,3,14,15,7},
		{1,13,11,3,8,7,9,10,12,15,4,14,0,5,6,2},
		{3,0,5,6,2,4,1,7}
	},
	{
		{9,15,28,7,13,24,2,23,21,1,22,16,18,8,17,31,27,6,30,12,4,20,5,19,0,25,3,29,10,14,11,26},
		{5,2,13,11,8,6,12,1,4,3,0,10,14,15,7,9},
		{11,6,10,0,12,1,8,14,2,9,13,3,7,4,15,5},
		{1,5,6,2,4,7,3,0}
	},
	{
		{17,25,29,27,5,11,10,21,2,8,13,0,30,3,14,16,22,1,7,15,31,18,4,20,9,19,26,24,23,28,12,6},
		{15,3,2,11,7,14,6,12,1,13,0,8,9,4,10,5},
		{6,12,5,7,4,8,2,3,1,14,13,11,9,10,0,15},
		{5,1,0,3,4,6,2,7}
	},
	{
		{7,21,9,20,10,28,31,11,16,15,14,30,27,23,5,25,0,22,24,2,6,17,3,18,4,12,13,19,26,1,29,8},
		{10,6,5,1,13,0,9,2,15,14,7,11,8,3,12,4},
		{8,6,15,13,2,7,1,3,11,0,14,10,4,5,12,9},
		{6,5,4,1,0,2,3,7}
	}
};

const int AWCartridge::xor_table[16] =  // -1 = unknown/unused
{
	0x0000, -1, 0x97CF, 0x4BE3, 0x2255, 0x8DD6, -1, 0xC6A2,	0xA1E8, 0xB3BF, 0x3B1A, 0x547A, -1, 0x935F, -1, -1
};

static u16 bitswap16(u16 in, const u8* vec)
{
  u16 ret = 0;
  for (int i = 0; i<16; i++) {
    ret |= ((in >> vec[i])&0x1)<<(15-i);
  }
  return ret;
}

u16 AWCartridge::decrypt(u16 cipherText, u32 address, const u8 key)
{
	u8 b0,b1,b2,b3;
	u16 aux;
	const u8* pbox = permutation_table[key>>6];
	const sbox_set* ss = &sboxes_table[(key>>4)&3];

	const u8 text_swap_vec[] = {
			pbox[15],pbox[14],pbox[13],pbox[12],pbox[11],pbox[10],pbox[9],pbox[8],
			pbox[7],pbox[6],pbox[5],pbox[4],pbox[3],pbox[2],pbox[1],pbox[0] };
	aux = bitswap16(cipherText, text_swap_vec);
	const u8 addr_swap_vec[] = { 13,5,2, 14,10,9,4, 15,11,6,1, 12,8,7,3,0 };
	aux = aux ^ bitswap16((u16)address, addr_swap_vec);

	b0 = aux&0x1f;
	b1 = (aux>>5)&0xf;
	b2 = (aux>>9)&0xf;
	b3 = aux>>13;

	b0 = ss->S0[b0];
	b1 = ss->S1[b1];
	b2 = ss->S2[b2];
	b3 = ss->S3[b3];

	return ((b3<<13)|(b2<<9)|(b1<<5)|b0)^xor_table[key&0xf];
}


void AWCartridge::Init(LoadProgress *progress, std::vector<u8> *digest)
{
	mpr_offset = decrypt16(0x58/2) | (decrypt16(0x5a/2) << 16);
	INFO_LOG(NAOMI, "AWCartridge::SetKey rombd_key %02x mpr_offset %08x", rombd_key, mpr_offset);
	device_reset();
}

void AWCartridge::SetKey(u32 key)
{
	rombd_key = key;
}

void AWCartridge::device_reset()
{
	epr_offset = 0;
	mpr_record_index = 0;
	mpr_first_file_index = 0;
	mpr_file_offset = 0;
	mpr_bank = 0;

	dma_offset = 0;
	dma_limit  = 0;
}

void AWCartridge::recalc_dma_offset(int mode)
{
	switch(mode) {
	case EPR:
		dma_offset = epr_offset * 2;
		dma_limit  = mpr_offset;
		break;

	case MPR_RECORD:
		dma_offset = mpr_offset + mpr_record_index * 0x40;
		dma_limit = std::min((u32)0x8000000, RomSize);
		break;

	case MPR_FILE: {
		u32 filedata_offs = (mpr_bank * 0x8000000 + mpr_offset + mpr_first_file_index * 0x40 + 8) / 2;
		dma_offset = decrypt16(filedata_offs) | (decrypt16(filedata_offs + 1) << 16);
		dma_offset = (mpr_offset + dma_offset + mpr_file_offset * 2) & 0x7ffffff;
		dma_limit  = std::min((uint32_t)0x8000000, RomSize);
		break;
	}
	}

	if (dma_offset >= mpr_offset) {
		u32 bank_base = mpr_bank * 0x8000000;
		dma_offset += bank_base;
		dma_limit = std::min(dma_limit + bank_base, RomSize);
	}
}

void *AWCartridge::GetDmaPtr(u32 &size)
{
	size = std::min(std::min(size, 32u), dma_limit - dma_offset);
	u32 offset = dma_offset / 2;
	for (u32 i = 0; i < size / 2; i++)
		decrypted_buf[i] = decrypt16(offset + i);

//	printf("AWCART Decrypted data @ %08x:\n", dma_offset);
//	for (int i = 0; i < 16; i++)
//	{
//		printf("%02x %02x ", decrypted_buf[i] & 0xff, decrypted_buf[i] >> 8);
//		if ((i + 1) % 8 == 0)
//			printf("\n");
//	}

	return decrypted_buf;
}

void AWCartridge::AdvancePtr(u32 size)
{
	dma_offset += size;
}

std::string AWCartridge::GetGameId()
{
	if (RomSize < 0x30 + 0x20)
		return "(ROM too small)";

	dma_offset = 0x30;
	u32 limit = 0x20;
	char *name = (char *)GetDmaPtr(limit);
	std::string game_id(name, limit);
	while (!game_id.empty() && game_id.back() == ' ')
		game_id.pop_back();
	return game_id;
}

void AWCartridge::Serialize(Serializer& ser) const
{
	ser << mpr_offset;
	ser << mpr_bank;
	ser << epr_offset;
	ser << mpr_file_offset;
	ser << mpr_record_index;
	ser << mpr_first_file_index;
	ser << dma_offset;
	ser << dma_limit;
	Cartridge::Serialize(ser);
}

void AWCartridge::Deserialize(Deserializer& deser)
{
	deser >> mpr_offset;
	deser >> mpr_bank;
	deser >> epr_offset;
	deser >> mpr_file_offset;
	deser >> mpr_record_index;
	deser >> mpr_first_file_index;
	deser >> dma_offset;
	deser >> dma_limit;
	Cartridge::Deserialize(deser);
}
