/*
	Dreamcast 'area 0' emulation
	Pretty much all peripheral registers are mapped here

	Routing is mostly handled here, as well as flash/SRAM emulation
*/
#include "sb_mem.h"
#include "sb.h"
#include "hw/aica/aica_if.h"
#include "hw/flashrom/flashrom.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/modem/modem.h"
#include "hw/naomi/naomi.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/sh4/sh4_mem.h"
#include "reios/reios.h"
#include "hw/bba/bba.h"

MemChip *sys_rom;
MemChip *sys_nvmem;

extern bool bios_loaded;

static std::string getRomPrefix()
{
	switch (settings.platform.system)
	{
	case DC_PLATFORM_DREAMCAST:
		return "dc_";
	case DC_PLATFORM_NAOMI:
		return "naomi_";
	case DC_PLATFORM_ATOMISWAVE:
		return "aw_";
	default:
		die("Unsupported platform");
		return "";
	}
}

static void add_isp_to_nvmem(DCFlashChip *flash)
{
	u8 block[64];
	if (!flash->ReadBlock(FLASH_PT_USER, FLASH_USER_INET, block))
	{
		memset(block, 0, sizeof(block));
		strcpy((char *)block + 2, "PWBrowser");
		block[12] = 0x1c;
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_INET, block);

		memset(block, 0, sizeof(block));
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_INET + 1, block);
		strcpy((char *)block + 32, "AT&F");
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_INET + 2, block);
		memset(block, 0, sizeof(block));
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_INET + 3, block);
		memset(block + 27, 0xFF, sizeof(block) - 27);
		block[10] = 1;
		block[14] = 1;
		block[16] = 1;
		block[19] = 6;
		block[26] = 5;
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_INET + 4, block);
		memset(block, 0xFF, sizeof(block));
		for (u32 i = FLASH_USER_INET + 5; i <= 0xbf; i++)
			flash->WriteBlock(FLASH_PT_USER, i, block);

		flash_isp1_block isp1;
		memset(&isp1, 0, sizeof(isp1));
		isp1._unknown[3] = 1;
		memcpy(isp1.sega, "SEGA", 4);
		strcpy(isp1.username, "flycast1");
		strcpy(isp1.password, "password");
		strcpy(isp1.phone, "1234567");
		if (flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1, &isp1) != 1)
			WARN_LOG(FLASHROM, "Failed to save ISP information to flash RAM");

		memset(block, 0, sizeof(block));
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 1, block);
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 2, block);
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 3, block);
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 4, block);
		block[60] = 1;
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 5, block);

		flash_isp2_block isp2;
		memset(&isp2, 0, sizeof(isp2));
		memcpy(isp2.sega, "SEGA", 4);
		strcpy(isp2.username, "flycast2");
		strcpy(isp2.password, "password");
		strcpy(isp2.phone, "1234567");
		if (flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP2, &isp2) != 1)
			WARN_LOG(FLASHROM, "Failed to save ISP information to flash RAM");
		u8 block[64];
		memset(block, 0, sizeof(block));
		for (u32 i = FLASH_USER_ISP2 + 1; i <= 0xEA; i++)
		{
			if (i == 0xcb)
				block[56] = 1;
			else
				block[56] = 0;
			flash->WriteBlock(FLASH_PT_USER, i, block);
		}
	}
}

void FixUpFlash()
{
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		static_cast<DCFlashChip*>(sys_nvmem)->Validate();

		// overwrite factory flash settings
		if (settings.dreamcast.region <= 2)
		{
			sys_nvmem->data[0x1a002] = '0' + settings.dreamcast.region;
			sys_nvmem->data[0x1a0a2] = '0' + settings.dreamcast.region;
		}
		if (settings.dreamcast.language <= 5)
		{
			sys_nvmem->data[0x1a003] = '0' + settings.dreamcast.language;
			sys_nvmem->data[0x1a0a3] = '0' + settings.dreamcast.language;
		}
		if (settings.dreamcast.broadcast <= 3)
		{
			sys_nvmem->data[0x1a004] = '0' + settings.dreamcast.broadcast;
			sys_nvmem->data[0x1a0a4] = '0' + settings.dreamcast.broadcast;
		}

		// overwrite user settings
		struct flash_syscfg_block syscfg;
		int res = static_cast<DCFlashChip*>(sys_nvmem)->ReadBlock(FLASH_PT_USER, FLASH_USER_SYSCFG, &syscfg);

		if (!res)
		{
			// write out default settings
			memset(&syscfg, 0xff, sizeof(syscfg));
			syscfg.time_lo = 0;
			syscfg.time_hi = 0;
			syscfg.lang = 0;
			syscfg.mono = 0;
			syscfg.autostart = 1;
		}
		u32 now = GetRTC_now();
		syscfg.time_lo = now & 0xffff;
		syscfg.time_hi = now >> 16;
		if (settings.dreamcast.language <= 5)
			syscfg.lang = settings.dreamcast.language;

		if (static_cast<DCFlashChip*>(sys_nvmem)->WriteBlock(FLASH_PT_USER, FLASH_USER_SYSCFG, &syscfg) != 1)
			WARN_LOG(FLASHROM, "Failed to save time and language to flash RAM");

		add_isp_to_nvmem(static_cast<DCFlashChip*>(sys_nvmem));

     	// Check the console ID used by some network games (chuchu rocket)
     	u8 *console_id = &sys_nvmem->data[0x1A058];
     	if (!memcmp(console_id, "\377\377\377\377\377\377", 6))
     	{
     		srand(now);
     		for (int i = 0; i < 6; i++)
     		{
     			console_id[i] = rand();
     			console_id[i + 0xA0] = console_id[i];	// copy at 1A0F8
     		}
     	}
	}
}

static bool nvmem_load()
{
	bool rc;
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
		rc = sys_nvmem->Load(getRomPrefix(), "%nvmem.bin", "nvram");
	else
		rc = sys_nvmem->Load(get_game_save_prefix() + ".nvmem");
	if (!rc)
		INFO_LOG(FLASHROM, "flash/nvmem is missing, will create new file...");
	
	if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		sys_rom->Load(get_game_save_prefix() + ".nvmem2");
	
	return true;
}

bool LoadRomFiles()
{
	nvmem_load();
	if (settings.platform.system != DC_PLATFORM_ATOMISWAVE)
	{
		if (sys_rom->Load(getRomPrefix(), "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin", "bootrom"))
			bios_loaded = true;
		else if (settings.platform.system == DC_PLATFORM_DREAMCAST)
			return false;
	}

	return true;
}

void SaveRomFiles()
{
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
		sys_nvmem->Save(getRomPrefix(), "nvmem.bin", "nvmem");
	else
		sys_nvmem->Save(get_game_save_prefix() + ".nvmem");
	if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		sys_rom->Save(get_game_save_prefix() + ".nvmem2");
}

bool LoadHle()
{
	if (!nvmem_load())
		WARN_LOG(FLASHROM, "No nvmem loaded\n");

	reios_reset(sys_rom->data);

	return true;
}

static u32 ReadFlash(u32 addr,u32 sz) { return sys_nvmem->Read(addr,sz); }
static void WriteFlash(u32 addr,u32 data,u32 sz) { sys_nvmem->Write(addr,data,sz); }

static u32 ReadBios(u32 addr,u32 sz)
{
	return sys_rom->Read(addr, sz);
}

static void WriteBios(u32 addr,u32 data,u32 sz)
{
	if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		if (sz != 1)
		{
			INFO_LOG(MEMORY, "Invalid access size @%08x data %x sz %d", addr, data, sz);
			return;
		}
		sys_rom->Write(addr, data, sz);
	}
	else
	{
		INFO_LOG(MEMORY, "Write to [Boot ROM] is not possible, addr=%x, data=%x, size=%d", addr, data, sz);
	}
}

//Area 0 mem map
//0x00000000- 0x001FFFFF	:MPX	System/Boot ROM
//0x00200000- 0x0021FFFF	:Flash Memory
//0x00400000- 0x005F67FF	:Unassigned
//0x005F6800- 0x005F69FF	:System Control Reg.
//0x005F6C00- 0x005F6CFF	:Maple i/f Control Reg.
//0x005F7000- 0x005F70FF	:GD-ROM / NAOMI BD Reg.
//0x005F7400- 0x005F74FF	:G1 i/f Control Reg.
//0x005F7800- 0x005F78FF	:G2 i/f Control Reg.
//0x005F7C00- 0x005F7CFF	:PVR i/f Control Reg.
//0x005F8000- 0x005F9FFF	:TA / PVR Core Reg.
//0x00600000- 0x006007FF	:MODEM
//0x00600800- 0x006FFFFF	:G2 (Reserved)
//0x00700000- 0x00707FFF	:AICA- Sound Cntr. Reg.
//0x00710000- 0x0071000B	:AICA- RTC Cntr. Reg.
//0x00800000- 0x00FFFFFF	:AICA- Wave Memory
//0x01000000- 0x01FFFFFF	:Ext. Device
//0x02000000- 0x03FFFFFF*	:Image Area*	2MB

//use unified size handler for registers
//it really makes no sense to use different size handlers on em -> especially when we can use templates :p
template<class T>
T DYNACALL ReadMem_area0(u32 addr)
{
	constexpr u32 sz = (u32)sizeof(T);
	addr &= 0x01FFFFFF;//to get rid of non needed bits
	const u32 base=(addr>>16);
	//map 0x0000 to 0x01FF to Default handler
	//mirror 0x0200 to 0x03FF , from 0x0000 to 0x03FFF
	//map 0x0000 to 0x001F

	//	:MPX	System/Boot ROM
	if (base <= (settings.platform.system == DC_PLATFORM_ATOMISWAVE ? 0x0001 : 0x001F))		// Only 128k BIOS on AtomisWave
	{
		return ReadBios(addr,sz);
	}
	//map 0x0020 to 0x0021
	else if ((base>= 0x0020) && (base<= 0x0021)) // :Flash Memory
	{
		return ReadFlash(addr&0x1FFFF,sz);
	}
	//map 0x005F to 0x005F
	else if (likely(base==0x005F))
	{
		if (addr <= 0x005F67FF) 	// :Unassigned
		{
			INFO_LOG(MEMORY, "Read from area0_32 not implemented [Unassigned], addr=%x", addr);
			return 0;
		}
		else if (addr >= 0x005F7000 && addr <= 0x005F70FF) // GD-ROM
		{
			if (settings.platform.system != DC_PLATFORM_DREAMCAST)
				return (T)ReadMem_naomi(addr, sz);
			else
				return (T)ReadMem_gdrom(addr, sz);
		}
		else if (likely(addr >= 0x005F6800 && addr <= 0x005F7CFF)) //	/*:PVR i/f Control Reg.*/ -> ALL SB registers now
		{
			return (T)sb_ReadMem(addr,sz);
		}
		else if (likely(addr >= 0x005F8000 && addr <= 0x005F9FFF)) //	:TA / PVR Core Reg.
		{
			if (sz != 4)
				// House of the Dead 2
				return 0;
			return (T)pvr_ReadReg(addr);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) //	:MODEM
	{
		if (settings.platform.system != DC_PLATFORM_DREAMCAST)
			return (T)libExtDevice_ReadMem_A0_006(addr, sz);
		else if (!settings.network.EmulateBBA)
			return (T)ModemReadMem_A0_006(addr, sz);
		else
			return (T)0;
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) //	:G2 (Reserved)
	{
		INFO_LOG(COMMON, "Read from area0_32 not implemented [G2 (Reserved)], addr=%x", addr);
		return 0;
	}
	//map 0x0070 to 0x0070
	else if ((base ==0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) //	:AICA- Sound Cntr. Reg.
	{
		return (T)ReadMem_aica_reg(addr, sz);
	}
	//map 0x0071 to 0x0071
	else if ((base ==0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) //	:AICA- RTC Cntr. Reg.
	{
		return (T)ReadMem_aica_rtc(addr,sz);
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) //	:AICA- Wave Memory
	{
		return (T)ReadMemArr<sz>(aica_ram.data, addr & ARAM_MASK);
	}
	//map 0x0100 to 0x01FF
	else if (base >= 0x0100 && base <= 0x01FF) // G2 Ext. Device #1
	{
		if (settings.platform.system == DC_PLATFORM_NAOMI)
			return (T)libExtDevice_ReadMem_A0_010(addr, sz);
		else if (settings.network.EmulateBBA)
			return (T)bba_ReadMem(addr, sz);
		else
			return (T)0;
	}
	INFO_LOG(MEMORY, "Read from area0<%d> not implemented [Unassigned], addr=%x", sz, addr);
	return 0;
}

template<class T>
void  DYNACALL WriteMem_area0(u32 addr,T data)
{
	constexpr u32 sz = (u32)sizeof(T);
	addr &= 0x01FFFFFF;//to get rid of non needed bits

	const u32 base=(addr>>16);

	//map 0x0000 to 0x001F
	 // :MPX System/Boot ROM
	if (base <= (settings.platform.system == DC_PLATFORM_ATOMISWAVE ? 0x0001 : 0x001F)) // Only 128k BIOS on AtomisWave
	{
		WriteBios(addr,data,sz);
	}
	//map 0x0020 to 0x0021
	else if ((base >=0x0020) && (base <=0x0021) /*&& (addr>= 0x00200000) && (addr<= 0x0021FFFF)*/) // Flash Memory
	{
		WriteFlash(addr,data,sz);
	}
	//map 0x0040 to 0x005F -> actually, I'll only map 0x005F to 0x005F, b/c the rest of it is unspammed (left to default handler)
	//map 0x005F to 0x005F
	else if ( likely(base==0x005F) )
	{
		if (addr <= 0x005F67FF)		// Unassigned
		{
			INFO_LOG(COMMON, "Write to area0_32 not implemented [Unassigned], addr=%x,data=%x,size=%d", addr, data, sz);
		}
		else if (addr >= 0x005F7000 && addr <= 0x005F70FF) // GD-ROM
		{
			if (settings.platform.system != DC_PLATFORM_DREAMCAST)
				WriteMem_naomi(addr,data,sz);
			else
				WriteMem_gdrom(addr,data,sz);
		}
		else if ( likely((addr>= 0x005F6800) && (addr<=0x005F7CFF)) ) // /*:PVR i/f Control Reg.*/ -> ALL SB registers
		{
			sb_WriteMem(addr,data,sz);
		}
		else if ( likely((addr>= 0x005F8000) && (addr<=0x005F9FFF)) ) // TA / PVR Core Reg.
		{
			verify(sz==4);
			pvr_WriteReg(addr,data);
		}
		else
			INFO_LOG(COMMON, "Write to area0_32 not implemented [Unassigned], addr=%x,data=%x,size=%d", addr, data, sz);
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) // MODEM
	{
		if (settings.platform.system != DC_PLATFORM_DREAMCAST)
			libExtDevice_WriteMem_A0_006(addr, data, sz);
		else if (!settings.network.EmulateBBA)
			ModemWriteMem_A0_006(addr, data, sz);
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) // G2 (Reserved)
	{
		INFO_LOG(COMMON, "Write to area0_32 not implemented [G2 (Reserved)], addr=%x,data=%x,size=%d", addr, data, sz);
	}
	//map 0x0070 to 0x0070
	else if ((base >=0x0070) && (base <=0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) // AICA- Sound Cntr. Reg.
	{
		WriteMem_aica_reg(addr,data,sz);
	}
	//map 0x0071 to 0x0071
	else if ((base >=0x0071) && (base <=0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) // AICA- RTC Cntr. Reg.
	{
		WriteMem_aica_rtc(addr,data,sz);
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) // AICA- Wave Memory
	{
		WriteMemArr<sz>(aica_ram.data, addr & ARAM_MASK, data);
	}
	//map 0x0100 to 0x01FF
	else if (base >= 0x0100 && base <= 0x01FF) // G2 Ext. Device #1
	{
		if (settings.platform.system == DC_PLATFORM_NAOMI)
			libExtDevice_WriteMem_A0_010(addr, data, sz);
		else if (settings.network.EmulateBBA)
			bba_WriteMem(addr, data, sz);
	}
	else
		INFO_LOG(COMMON, "Write to area0_32 not implemented [Unassigned], addr=%x,data=%x,size=%d", addr, data, sz);
}

//Init/Res/Term
void sh4_area0_Init()
{
	sb_Init();
}

void sh4_area0_Reset(bool hard)
{
	if (hard)
	{
		if (sys_rom != NULL)
		{
			delete sys_rom;
			sys_rom = NULL;
		}
		if (sys_nvmem != NULL)
		{
			delete sys_nvmem;
			sys_nvmem = NULL;
		}

		switch (settings.platform.system)
		{
		case DC_PLATFORM_DREAMCAST:
			sys_rom = new RomChip(settings.platform.bios_size);
			sys_nvmem = new DCFlashChip(settings.platform.flash_size);
			reios_set_flash(sys_nvmem);
			break;
		case DC_PLATFORM_NAOMI:
			sys_rom = new RomChip(settings.platform.bios_size);
			sys_nvmem = new SRamChip(settings.platform.bbsram_size);
			break;
		case DC_PLATFORM_ATOMISWAVE:
			sys_rom = new DCFlashChip(settings.platform.bios_size, settings.platform.bios_size / 2);
			sys_nvmem = new SRamChip(settings.platform.bbsram_size);
			break;
		}
	}
	else
	{
		sys_rom->Reset();
		sys_nvmem->Reset();
	}
	sb_Reset(hard);
}

void sh4_area0_Term()
{
	if (sys_rom != NULL)
	{
		delete sys_rom;
		sys_rom = NULL;
	}
	if (sys_nvmem != NULL)
	{
		delete sys_nvmem;
		sys_nvmem = NULL;
	}
	sb_Term();
}


//AREA 0
static _vmem_handler area0_handler;


void map_area0_init()
{

	area0_handler = _vmem_register_handler_Template(ReadMem_area0,WriteMem_area0);
}
void map_area0(u32 base)
{
	verify(base<0xE0);

	_vmem_map_handler(area0_handler,0x00|base,0x01|base);

	//0x0240 to 0x03FF mirrors 0x0040 to 0x01FF (no flashrom or bios)
	//0x0200 to 0x023F are unused
	_vmem_mirror_mapping(0x02|base,0x00|base,0x02);
}
