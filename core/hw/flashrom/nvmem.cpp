/*
	Copyright 2023 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "nvmem.h"
#include "flashrom.h"
#include "cfg/option.h"
#include "hw/aica/aica_if.h"
#include "reios/reios.h"
#include "oslib/oslib.h"
#include "oslib/resources.h"

extern bool bios_loaded;

namespace nvmem
{

static MemChip *sys_rom;
static WritableChip *sys_nvmem;

static std::string getRomPrefix()
{
	switch (settings.platform.system)
	{
	case DC_PLATFORM_DREAMCAST:
		return "dc_";
	case DC_PLATFORM_NAOMI:
		return "naomi_";
	case DC_PLATFORM_NAOMI2:
		return "naomi2_";
	case DC_PLATFORM_ATOMISWAVE:
		return "aw_";
	default:
		return "";
	}
}

static void add_isp_to_nvmem(DCFlashChip *flash, const char *username)
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

		flash_isp1_block isp1{};
		isp1._unknown[3] = 1;
		memcpy(isp1.sega, "SEGA", 4);
		if (username == nullptr || username[0] == '\0')
			strcpy(isp1.username, "flycast1");
		else
			strncpy(isp1.username, username, sizeof(isp1.username) - 1);
		strcpy(isp1.password, "password");
		strcpy(isp1.phone, "1234567");
		if (flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1, &isp1) != 1)
			WARN_LOG(FLASHROM, "Failed to save ISP1 information to flash RAM");

		memset(block, 0, sizeof(block));
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 1, block);
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 2, block);
		strcpy((char *)block + 2, "flycast@gmail.com");
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 3, block);
		memset(block, 0, sizeof(block));
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 4, block);
		block[60] = 1;
		flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1 + 5, block);

		flash_isp2_block isp2{};
		memcpy(isp2.sega, "SEGA", 4);
		if (username == nullptr || username[0] == '\0')
			strcpy(isp2.username, "flycast2");
		else
			strncpy(isp2.username, username, sizeof(isp2.username) - 1);
		strcpy(isp2.password, "password");
		strcpy(isp2.phone, "1234567");
		if (flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP2, &isp2) != 1)
			WARN_LOG(FLASHROM, "Failed to save ISP2 information to flash RAM");
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
	else if (username != nullptr && username[0] != '\0')
	{
		flash_isp1_block isp1{};
		flash->ReadBlock(FLASH_PT_USER, FLASH_USER_ISP1, &isp1);
		memset(isp1.username, 0, sizeof(isp1.username));
		strncpy(isp1.username, username, sizeof(isp1.username) - 1);
		if (flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP1, &isp1) != 1)
			WARN_LOG(FLASHROM, "Failed to save ISP1 information to flash RAM");

		flash_isp2_block isp2{};
		flash->ReadBlock(FLASH_PT_USER, FLASH_USER_ISP2, &isp2);
		memset(isp2.username, 0, sizeof(isp2.username));
		strncpy(isp2.username, username, sizeof(isp2.username) - 1);
		if (flash->WriteBlock(FLASH_PT_USER, FLASH_USER_ISP2, &isp2) != 1)
			WARN_LOG(FLASHROM, "Failed to save ISP2 information to flash RAM");
	}
	if (!flash->ReadBlock(FLASH_PT_USER, FLASH_USER_BBA, block) || memcmp(&block[2], "SEGA", 4))
	{
		flash_bba_block bba{};
		memcpy(bba.sega, "SEGA", 4);
		// Leave everything at 0 to use DHCP
		if (flash->WriteBlock(FLASH_PT_USER, FLASH_USER_BBA, &bba) != 1)
			WARN_LOG(FLASHROM, "Failed to save BBA information to flash RAM");
	}
}

static void fixUpDCFlash()
{
	if (settings.platform.isConsole())
	{
		static_cast<DCFlashChip*>(sys_nvmem)->Validate();

		// overwrite factory flash settings
		if (config::Region <= 2)
		{
			sys_nvmem->data[0x1a002] = '0' + config::Region;
			sys_nvmem->data[0x1a0a2] = '0' + config::Region;
		}
		if (config::Language <= 5)
		{
			sys_nvmem->data[0x1a003] = '0' + config::Language;
			sys_nvmem->data[0x1a0a3] = '0' + config::Language;
		}
		if (config::Broadcast <= 3)
		{
			sys_nvmem->data[0x1a004] = '0' + config::Broadcast;
			sys_nvmem->data[0x1a0a4] = '0' + config::Broadcast;
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
			syscfg.time_zone = 0;
			syscfg.lang = 0;
			syscfg.mono = 0;
			syscfg.autostart = 1;
		}
		u32 now = aica::GetRTC_now();
		syscfg.time_lo = now & 0xffff;
		syscfg.time_hi = now >> 16;
		if (config::Language <= 5)
			syscfg.lang = config::Language;

		if (static_cast<DCFlashChip*>(sys_nvmem)->WriteBlock(FLASH_PT_USER, FLASH_USER_SYSCFG, &syscfg) != 1)
			WARN_LOG(FLASHROM, "Failed to save time and language to flash RAM");

		add_isp_to_nvmem(static_cast<DCFlashChip*>(sys_nvmem), config::ISPUsername.get().c_str());

     	// Check the console ID used by some network games (chuchu rocket)
     	u8 *console_id = &sys_nvmem->data[0x1A058];
     	if (!memcmp(console_id, "\377\377\377\377\377\377", 6))
     	{
     		srand(now);
     		u8 sum = 0;
     		for (int i = 0; i < 6; i++)
     		{
     			console_id[i] = rand();
     			console_id[i + 0xA0] = console_id[i];	// copy at 1A0F8
     			sum += console_id[i];
     		}
     		console_id[-1] = console_id[0xA0 - 1] = sum;
     		console_id[-2] = console_id[0xA0 - 2] = ~sum;
     	}
     	else
     	{
     		// Fix checksum
     		u8 sum = 0;
     		for (int i = 0; i < 6; i++)
     			sum += console_id[i];
     		console_id[-1] = console_id[0xA0 - 1] = sum;
     		console_id[-2] = console_id[0xA0 - 2] = ~sum;
     	}
 		// machine_version: ff - VA0, fe - VA1, fd - VA2
     	// must be != 0xff
 		console_id[7] = console_id[0xA0 + 7] = 0xfe;
	}
}

static std::unique_ptr<u8[]> loadFlashResource(const std::string& name, size_t& size)
{
	return resource::load("flash/" + name, size);
}

static void loadDefaultAWBiosFlash()
{
	std::string flashName = get_file_basename(settings.content.fileName) + ".nvmem2";

	size_t size = settings.platform.bios_size / 2;
	std::unique_ptr<u8[]> buffer = loadFlashResource(flashName, size);
	if (buffer)
		sys_rom->Load(buffer.get(), size);
}

static bool loadFlash()
{
	bool rc = true;
	if (settings.platform.isConsole())
		rc = sys_nvmem->Load(getRomPrefix(), "%nvmem.bin", "nvram");
	else if (!settings.naomi.slave)
	{
		rc = sys_nvmem->Load(hostfs::getArcadeFlashPath() + ".nvmem");
		if (!rc)
		{
			std::string flashName = get_file_basename(settings.content.fileName) + ".nvmem";

			size_t size = settings.platform.flash_size;
			std::unique_ptr<u8[]> buffer = loadFlashResource(flashName, size);
			if (buffer)
			{
				sys_nvmem->Load(buffer.get(), size);
				rc = true;
			}
		}
	}
	if (!rc)
		INFO_LOG(FLASHROM, "flash/nvmem is missing, will create new file...");
	fixUpDCFlash();
	if (config::GGPOEnable)
		sys_nvmem->digest(settings.network.md5.nvmem);

	if (settings.platform.isAtomiswave())
	{
		rc = sys_rom->Load(hostfs::getArcadeFlashPath() + ".nvmem2");
		// TODO default AW .nvmem2
		if (!rc)
			loadDefaultAWBiosFlash();
		if (config::GGPOEnable)
			sys_nvmem->digest(settings.network.md5.nvmem2);
	}

	return true;
}

bool loadFiles()
{
	loadFlash();
	if (!settings.platform.isAtomiswave())
	{
		if (sys_rom->Load(getRomPrefix(), "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin", "bootrom"))
		{
			if (config::GGPOEnable)
				sys_rom->digest(settings.network.md5.bios);
			bios_loaded = true;
		}
		else if (settings.platform.isConsole())
			return false;
	}

	return true;
}

void saveFiles()
{
	if (settings.naomi.slave || settings.naomi.drivingSimSlave)
		return;
	if (settings.platform.isConsole())
		sys_nvmem->Save(getRomPrefix(), "nvmem.bin", "nvmem");
	else
		sys_nvmem->Save(hostfs::getArcadeFlashPath() + ".nvmem");
	if (settings.platform.isAtomiswave())
		((WritableChip *)sys_rom)->Save(hostfs::getArcadeFlashPath() + ".nvmem2");
}

bool loadHle()
{
	if (!loadFlash())
		WARN_LOG(FLASHROM, "No nvmem loaded");

	reios_reset(sys_rom->data);

	return true;
}

u32 readFlash(u32 addr, u32 sz)
{
	return sys_nvmem->Read(addr, sz);
}
void writeFlash(u32 addr, u32 data, u32 sz)
{
	sys_nvmem->Write(addr, data, sz);
}
u8 *getFlashData()
{
	return sys_nvmem->data;
}

u32 readBios(u32 addr, u32 sz)
{
	return sys_rom->Read(addr, sz);
}
void writeAWBios(u32 addr, u32 data, u32 sz)
{
	((WritableChip *)sys_rom)->Write(addr, data, sz);
}
u8 *getBiosData()
{
	return sys_rom->data;
}

void reloadAWBios()
{
	if (!settings.platform.isAtomiswave())
		return;
	if (sys_rom->Reload())
		return;
	loadDefaultAWBiosFlash();
}

void init()
{
	switch (settings.platform.system)
	{
	case DC_PLATFORM_DREAMCAST:
		sys_rom = new RomChip(settings.platform.bios_size);
		sys_nvmem = new DCFlashChip(settings.platform.flash_size);
		reios_set_flash(sys_nvmem);
		break;
	case DC_PLATFORM_NAOMI:
	case DC_PLATFORM_NAOMI2:
		sys_rom = new RomChip(settings.platform.bios_size);
		sys_nvmem = new SRamChip(settings.platform.flash_size);
		break;
	case DC_PLATFORM_ATOMISWAVE:
		sys_rom = new DCFlashChip(settings.platform.bios_size, settings.platform.bios_size / 2);
		sys_nvmem = new SRamChip(settings.platform.flash_size);
		break;
	case DC_PLATFORM_SYSTEMSP:
		sys_rom = new RomChip(settings.platform.bios_size);
		sys_nvmem = new SRamChip(settings.platform.flash_size);
		break;
	}
}

void term()
{
	delete sys_rom;
	sys_rom = nullptr;
	delete sys_nvmem;
	sys_nvmem = nullptr;
}

void reset()
{
	sys_rom->Reset();
	sys_nvmem->Reset();
}

void serialize(Serializer& ser)
{
	sys_rom->Serialize(ser);
	sys_nvmem->Serialize(ser);
}

void deserialize(Deserializer& deser)
{
	sys_rom->Deserialize(deser);
	sys_nvmem->Deserialize(deser);
}

}
