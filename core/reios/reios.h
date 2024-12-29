#pragma once

#include "types.h"
#include "hw/flashrom/flashrom.h"

void reios_init();
void reios_set_flash(MemChip* flash);
void reios_reset(u8* rom);
void reios_serialize(Serializer& ser);
void reios_deserialize(Deserializer& deser);
void reios_term();

struct Sh4Context;
void DYNACALL reios_trap(Sh4Context *ctx, u32 op);

void reios_disk_id();

struct ip_meta_t
{
	char hardware_id[16];
	char maker_id[16];
	char ks[5];
	char disk_type[6];
	char disk_num[5];
	char area_symbols[8];
	char ctrl[4];
	char dev;
	char vga;
	char wince;
	char _unk1;
	char product_number[10];
	char product_version[6];
	char release_date[8];
	char _unk2[8];
	char boot_filename[16];
	char software_company[16];
	char software_name[128];

	bool isWindowsCE() { return wince == '1' || memcmp("0WINCEOS.BIN", boot_filename, 12) == 0; }
	bool supportsVGA() { return vga == '1'; }
};
extern ip_meta_t ip_meta;

#define REIOS_OPCODE 0x085B
