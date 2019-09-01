#ifndef REIOS_H
#define REIOS_H

#include "types.h"
#include "hw/flashrom/flashrom.h"

bool reios_init(u8* rom, MemChip *flash);

void reios_reset();

void reios_term();

void DYNACALL reios_trap(u32 op);

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

#endif //REIOS_H
