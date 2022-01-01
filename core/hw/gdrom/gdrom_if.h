#pragma once
#include "types.h"

void gdrom_reg_Init();
void gdrom_reg_Term();
void gdrom_reg_Reset(bool hard);

u32  ReadMem_gdrom(u32 Addr, u32 sz);
void WriteMem_gdrom(u32 Addr, u32 data, u32 sz);
void libCore_CDDA_Sector(s16* sector);
void libCore_gdrom_disc_change();
u32 gd_get_subcode(u32 format, u32 fad, u8 *subc_info);
void gd_setdisc();

enum DiscType
{
	CdDA=0x00,
	CdRom=0x10,
	CdRom_XA=0x20,
	CdRom_Extra=0x30,
	CdRom_CDI=0x40,
	GdRom=0x80,

	NoDisk=0x1,			//These are a bit hacky .. but work for now ...
	Open=0x2,			//tray is open :)
};

namespace gdrom
{
void serialize(Serializer& ser);
void deserialize(Deserializer& deser);
}
