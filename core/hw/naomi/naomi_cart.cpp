/*
	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
*/
// Naomi comm board emulation from mame
// https://github.com/mamedev/mame/blob/master/src/mame/machine/m3comm.cpp
// license:BSD-3-Clause
// copyright-holders:MetalliC

#include <memory>
#include "naomi_cart.h"
#include "naomi_regs.h"
#include "naomi.h"
#include "decrypt.h"
#include "naomi_roms.h"
#include "hw/flashrom/flashrom.h"
#include "hw/holly/holly_intc.h"
#include "m1cartridge.h"
#include "m4cartridge.h"
#include "awcartridge.h"
#include "gdcartridge.h"
#include "archive/archive.h"
#include "stdclass.h"
#include "emulator.h"
#include "cfg/option.h"
#include "oslib/oslib.h"
#include "serialize.h"

Cartridge *CurrentCartridge;
bool bios_loaded = false;

char naomi_game_id[33];
InputDescriptors *NaomiGameInputs;
u8 *naomi_default_eeprom;

extern MemChip *sys_rom;

static bool loadBios(const char *filename, Archive *child_archive, Archive *parent_archive, int region)
{
	int biosid = 0;
	for (; BIOS[biosid].name != NULL; biosid++)
		if (!stricmp(BIOS[biosid].name, filename))
			break;
	if (BIOS[biosid].name == NULL)
	{
		WARN_LOG(NAOMI, "Unknown BIOS %s", filename);
		return false;
	}

	struct BIOS_t *bios = &BIOS[biosid];

	std::string arch_name(filename);
	std::string path = hostfs::findNaomiBios(arch_name + ".zip");
	if (!file_exists(path))
		path = hostfs::findNaomiBios(arch_name + ".7z");
	DEBUG_LOG(NAOMI, "Loading BIOS from %s", path.c_str());
	std::unique_ptr<Archive> bios_archive(OpenArchive(path.c_str()));

	MD5Sum md5;

	bool found_region = false;

	for (int romid = 0; bios->blobs[romid].filename != NULL; romid++)
	{
		if (region == -1)
			region = bios->blobs[romid].region;
		else
		{
			if (bios->blobs[romid].region != (u32)region)
				continue;
		}
		found_region = true;
		if (bios->blobs[romid].blob_type == Copy)
		{
			verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
			verify(bios->blobs[romid].src_offset + bios->blobs[romid].length <= BIOS_SIZE);
			memcpy(sys_rom->data + bios->blobs[romid].offset, sys_rom->data + bios->blobs[romid].src_offset, bios->blobs[romid].length);
			DEBUG_LOG(NAOMI, "Copied: %x bytes from %07x to %07x", bios->blobs[romid].length, bios->blobs[romid].src_offset, bios->blobs[romid].offset);
		}
		else
		{
			std::unique_ptr<ArchiveFile> file;
			// Find by CRC
			if (child_archive != NULL)
				file.reset(child_archive->OpenFileByCrc(bios->blobs[romid].crc));
			if (!file && parent_archive != NULL)
				file.reset(parent_archive->OpenFileByCrc(bios->blobs[romid].crc));
			if (!file && bios_archive != NULL)
				file.reset(bios_archive->OpenFileByCrc(bios->blobs[romid].crc));
			// Fallback to find by filename
			if (!file && child_archive != NULL)
				file.reset(child_archive->OpenFile(bios->blobs[romid].filename));
			if (!file && parent_archive != NULL)
				file.reset(parent_archive->OpenFile(bios->blobs[romid].filename));
			if (!file && bios_archive != NULL)
				file.reset(bios_archive->OpenFile(bios->blobs[romid].filename));
			if (!file) {
				WARN_LOG(NAOMI, "%s: Cannot open %s", filename, bios->blobs[romid].filename);
				return false;
			}
			switch (bios->blobs[romid].blob_type)
			{
				case Normal:
				{
					verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
					u32 read = file->Read(sys_rom->data + bios->blobs[romid].offset, bios->blobs[romid].length);
					if (config::GGPOEnable)
						md5.add(sys_rom->data + bios->blobs[romid].offset, bios->blobs[romid].length);
					DEBUG_LOG(NAOMI, "Mapped %s: %x bytes at %07x", bios->blobs[romid].filename, read, bios->blobs[romid].offset);
				}
				break;

				case InterleavedWord:
				{
					u8 *buf = (u8 *)malloc(bios->blobs[romid].length);
					if (buf == NULL)
						throw NaomiCartException(std::string("Memory allocation failed"));

					verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
					u32 read = file->Read(buf, bios->blobs[romid].length);
					u16 *to = (u16 *)(sys_rom->data + bios->blobs[romid].offset);
					u16 *from = (u16 *)buf;
					for (int i = bios->blobs[romid].length / 2; --i >= 0; to++)
						*to++ = *from++;
					free(buf);
					if (config::GGPOEnable)
						md5.add(sys_rom->data + bios->blobs[romid].offset, bios->blobs[romid].length);
					DEBUG_LOG(NAOMI, "Mapped %s: %x bytes (interleaved word) at %07x", bios->blobs[romid].filename, read, bios->blobs[romid].offset);
				}
				break;

			default:
				die("Unknown blob type\n");
				break;
			}
		}
	}
	if (config::GGPOEnable)
		md5.getDigest(settings.network.md5.bios);

	if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		// Reload the writeable portion of the FlashROM
		sys_rom->Reload();

	return found_region;
}

static Game *FindGame(const char *filename)
{
	std::string gameName = get_file_basename(filename);
	size_t folder_pos = get_last_slash_pos(gameName);
	if (folder_pos != std::string::npos)
		gameName = gameName.substr(folder_pos + 1);

	for (int i = 0; Games[i].name != nullptr; i++)
		if (gameName == Games[i].name)
			return &Games[i];

	return nullptr;
}

void naomi_cart_LoadBios(const char *filename)
{
	Game *game = FindGame(filename);
	if (game == nullptr)
		return;

	// Open archive and parent archive if any
	std::unique_ptr<Archive> archive(OpenArchive(filename));

	std::unique_ptr<Archive> parent_archive;
	if (game->parent_name != NULL)
		parent_archive.reset(OpenArchive((get_game_dir() + game->parent_name).c_str()));

	const char *bios = "naomi";
	if (game->bios != nullptr)
		bios = game->bios;
	u32 region_flag = std::min((int)config::Region, (int)game->region_flag);
	if (game->region_flag == REGION_EXPORT_ONLY)
	   region_flag = REGION_EXPORT;
	if (!loadBios(bios, archive.get(), parent_archive.get(), region_flag))
	{
		WARN_LOG(NAOMI, "Warning: Region %d bios not found in %s", region_flag, bios);
		if (!loadBios(bios, archive.get(), parent_archive.get(), -1))
		{
			// If a specific BIOS is needed for this game, fail.
			if (game->bios != NULL || !bios_loaded)
				throw NaomiCartException(std::string("Error: cannot load BIOS ") + (game->bios != NULL ? game->bios : "naomi.zip"));

			// otherwise use the default BIOS
		}
	}
	bios_loaded = true;
}

static void naomi_cart_LoadZip(const char *filename, LoadProgress *progress)
{
	Game *game = FindGame(filename);
	if (game == NULL)
		throw NaomiCartException("Unknown game");

	// Open archive and parent archive if any
	std::unique_ptr<Archive> archive(OpenArchive(filename));
	if (archive != NULL)
		INFO_LOG(NAOMI, "Opened %s", filename);

	std::unique_ptr<Archive> parent_archive;
	if (game->parent_name != NULL)
	{
		parent_archive.reset(OpenArchive((get_game_dir() + game->parent_name).c_str()));
		if (parent_archive != NULL)
			INFO_LOG(NAOMI, "Opened %s", game->parent_name);
	}

	if (archive == NULL && parent_archive == NULL)
	{
		if (game->parent_name != NULL)
			throw NaomiCartException(std::string("Cannot open ") + filename + std::string(" or ") + game->parent_name);
		else
			throw NaomiCartException(std::string("Cannot open ") + filename);
	}

	// Load the BIOS
	naomi_cart_LoadBios(filename);

	// Now load the cartridge data
	try {
		switch (game->cart_type)
		{
		case M1:
			CurrentCartridge = new M1Cartridge(game->size);
			break;
		case M2:
			CurrentCartridge = new M2Cartridge(game->size);
			break;
		case M4:
			CurrentCartridge = new M4Cartridge(game->size);
			break;
		case AW:
			CurrentCartridge = new AWCartridge(game->size);
			break;
		case GD:
			{
				GDCartridge *gdcart = new GDCartridge(game->size);
				gdcart->SetGDRomName(game->gdrom_name, game->parent_name);
				CurrentCartridge = gdcart;
			}
			break;
		default:
			die("Unsupported cartridge type\n");
			break;
		}
		CurrentCartridge->SetKey(game->key);
		NaomiGameInputs = game->inputs;

		MD5Sum md5;

		int romCount = 0;
		while (game->blobs[romCount].filename != nullptr)
			romCount++;
		for (int romid = 0; romid < romCount; romid++)
		{
			if (progress != nullptr)
			{
				if (progress->cancelled)
					throw LoadCancelledException();
				if (game->cart_type != GD)
				{
					static std::string label;
					label = "ROM " + std::to_string(romid + 1);
					progress->label = label.c_str();
					progress->progress = (float)(romid + 1) / romCount;
				}
			}

			u32 len = game->blobs[romid].length;

			if (game->blobs[romid].blob_type == Copy)
			{
				u8 *dst = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
				u8 *src = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].src_offset, len);
				memcpy(dst, src, game->blobs[romid].length);
				DEBUG_LOG(NAOMI, "Copied: %x bytes from %07x to %07x", game->blobs[romid].length, game->blobs[romid].src_offset, game->blobs[romid].offset);
			}
			else
			{
				std::unique_ptr<ArchiveFile> file;
				// Find by CRC
				if (archive != NULL)
					file.reset(archive->OpenFileByCrc(game->blobs[romid].crc));
				if (!file && parent_archive != NULL)
					file.reset(parent_archive->OpenFileByCrc(game->blobs[romid].crc));
				// Fallback to find by filename
				if (!file && archive != NULL)
					file.reset(archive->OpenFile(game->blobs[romid].filename));
				if (!file && parent_archive != NULL)
					file.reset(parent_archive->OpenFile(game->blobs[romid].filename));
				if (!file) {
					WARN_LOG(NAOMI, "%s: Cannot open %s", filename, game->blobs[romid].filename);
					if (game->blobs[romid].blob_type != Eeprom)
						// Default eeprom file is optional
						throw NaomiCartException(std::string("Cannot find ") + game->blobs[romid].filename);
					else
						continue;
				}
				switch (game->blobs[romid].blob_type)
				{
					case Normal:
						{
							u8 *dst = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
							u32 read = file->Read(dst, game->blobs[romid].length);
							if (config::GGPOEnable)
								md5.add(dst, game->blobs[romid].length);
							DEBUG_LOG(NAOMI, "Mapped %s: %x bytes at %07x", game->blobs[romid].filename, read, game->blobs[romid].offset);
						}
						break;

					case InterleavedWord:
						{
							u8 *buf = (u8 *)malloc(game->blobs[romid].length);
							if (buf == NULL)
								throw NaomiCartException(std::string("Memory allocation failed"));

							u32 read = file->Read(buf, game->blobs[romid].length);
							u16 *to = (u16 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
							u16 *from = (u16 *)buf;
							for (int i = game->blobs[romid].length / 2; --i >= 0; to++)
								*to++ = *from++;
							free(buf);
							if (config::GGPOEnable)
								md5.add((u8*)CurrentCartridge->GetPtr(game->blobs[romid].offset, len), game->blobs[romid].length);
							DEBUG_LOG(NAOMI, "Mapped %s: %x bytes (interleaved word) at %07x", game->blobs[romid].filename, read, game->blobs[romid].offset);
						}
						break;

					case Key:
						{
							u8 *buf = (u8 *)malloc(game->blobs[romid].length);
							if (buf == NULL)
								throw NaomiCartException(std::string("Memory allocation failed"));

							u32 read = file->Read(buf, game->blobs[romid].length);
							CurrentCartridge->SetKeyData(buf);
							if (config::GGPOEnable)
								md5.add(buf, game->blobs[romid].length);
							DEBUG_LOG(NAOMI, "Loaded %s: %x bytes cart key", game->blobs[romid].filename, read);
						}
						break;

					case Eeprom:
						{
							naomi_default_eeprom = (u8 *)malloc(game->blobs[romid].length);
							if (naomi_default_eeprom == NULL)
								throw NaomiCartException(std::string("Memory allocation failed"));

							u32 read = file->Read(naomi_default_eeprom, game->blobs[romid].length);
							if (config::GGPOEnable)
								md5.add(naomi_default_eeprom, game->blobs[romid].length);
							DEBUG_LOG(NAOMI, "Loaded %s: %x bytes default eeprom", game->blobs[romid].filename, read);
						}
						break;

					default:
						die("Unknown blob type\n");
						break;
				}
			}
		}
		if (naomi_default_eeprom == NULL && game->eeprom_dump != NULL)
			naomi_default_eeprom = game->eeprom_dump;
		if (game->rotation_flag == ROT270)
			config::Rotate90.override(true);

		std::vector<u8> gdromDigest;
		CurrentCartridge->Init(progress, config::GGPOEnable ? &gdromDigest : nullptr);

		if (config::GGPOEnable)
		{
			if (game->cart_type == GD)
			{
				std::vector<u8> romMD5 = md5.getDigest();
				md5 = MD5Sum().add(romMD5).add(gdromDigest);
			}
			md5.getDigest(settings.network.md5.game);
		}

		strcpy(naomi_game_id, CurrentCartridge->GetGameId().c_str());
		if (naomi_game_id[0] == '\0')
			strcpy(naomi_game_id, game->name);
		NOTICE_LOG(NAOMI, "NAOMI GAME ID [%s]", naomi_game_id);

	} catch (...) {
		delete CurrentCartridge;
		CurrentCartridge = NULL;
		throw;
	}
}

void naomi_cart_LoadRom(const char* file, LoadProgress *progress)
{
	naomi_cart_Close();

	std::string extension = get_file_extension(file);

	if (extension == "zip" || extension == "7z")
	{
		naomi_cart_LoadZip(file, progress);
		return;
	}

	// Try to load BIOS from naomi.zip
	if (!loadBios("naomi", NULL, NULL, config::Region))
	{
		WARN_LOG(NAOMI, "Warning: Region %d bios not found in naomi.zip", config::Region.get());
		if (!loadBios("naomi", NULL, NULL, -1))
		{
			if (!bios_loaded)
				throw FlycastException("Error: cannot load BIOS from naomi.zip");
		}
	}

	std::string folder;
	std::vector<std::string> files;
	std::vector<u32> fstart;
	std::vector<u32> fsize;
	u32 romSize = 0;

	if (extension == "lst")
	{
		// LST file
		size_t folder_pos = get_last_slash_pos(file);
		if (folder_pos != std::string::npos)
			folder = std::string(file).substr(0, folder_pos + 1);

		FILE *fl = nowide::fopen(file, "r");
		if (!fl)
			throw FlycastException("Error: can't open " + std::string(file));

		char t[512];
		char* line = std::fgets(t, sizeof(t), fl);
		if (!line)
		{
			std::fclose(fl);
			throw FlycastException("Error: Invalid LST file");
		}

		char* eon = strstr(line, "\n");
		if (!eon)
			DEBUG_LOG(NAOMI, "+Loading naomi rom that has no name");
		else
		{
			*eon = 0;
			DEBUG_LOG(NAOMI, "+Loading naomi rom : %s", line);
		}

		line = std::fgets(t, sizeof(t), fl);
		if (!line)
		{
			std::fclose(fl);
			throw FlycastException("Error: Invalid LST file");
		}

		while (line)
		{
			char filename[512];
			u32 addr, sz;
			if (sscanf(line, "\"%[^\"]\",%x,%x", filename, &addr, &sz) == 3)
			{
				files.emplace_back(filename);
				fstart.push_back(addr);
				fsize.push_back(sz);
				romSize = std::max(romSize, (addr + sz));
			}
			else if (line[0] != 0 && line[0] != '\n' && line[0] != '\r')
				WARN_LOG(NAOMI, "Warning: invalid line in .lst file: %s", line);

			line = std::fgets(t, sizeof(t), fl);
		}
		std::fclose(fl);
	}
	else
	{
		// BIN loading
		FILE* fp = nowide::fopen(file, "rb");
		if (fp == NULL)
			throw FlycastException("Error: can't open " + std::string(file));

		std::fseek(fp, 0, SEEK_END);
		u32 file_size = (u32)std::ftell(fp);
		std::fclose(fp);
		files.emplace_back(file);
		fstart.push_back(0);
		fsize.push_back(file_size);
		romSize = file_size;
	}

	INFO_LOG(NAOMI, "+%zd romfiles, %.2f MB set address space", files.size(), romSize / 1024.f / 1024.f);

	MD5Sum md5;

	// Allocate space for the rom
	u8 *romBase = (u8 *)malloc(romSize);
	verify(romBase != nullptr);

	bool load_error = false;

	for (size_t i = 0; i<files.size(); i++)
	{
		FILE *fp = nullptr;

		if (files[i] != "null")
		{
			std::string file(folder + files[i]);

			fp = nowide::fopen(file.c_str(), "rb");
			if (fp == nullptr)
			{
				ERROR_LOG(NAOMI, "Unable to open file %s: error %d", file.c_str(), errno);
				load_error = true;
				break;
			}
		}
		u8* romDest = romBase + fstart[i];

		if (fp == nullptr)
		{
			//printf("-Reserving ram at 0x%08X, size 0x%08X\n", fstart[i], fsize[i]);
			memset(romDest, -1, fsize[i]);
		}
		else
		{
			//printf("-Mapping \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
			bool mapped = fread(romDest, 1, fsize[i], fp) == fsize[i];
			if (config::GGPOEnable)
				md5.add(fp);
			fclose(fp);
			if (!mapped)
			{
				ERROR_LOG(NAOMI, "Unable to read file %s @ %08x size %x", files[i].c_str(),
						fstart[i], fsize[i]);
				load_error = true;
				break;
			}
		}
	}

	if (load_error)
	{
		free(romBase);
		throw FlycastException("Error: Failed to load BIN/DAT file");
	}
	if (config::GGPOEnable)
		md5.getDigest(settings.network.md5.game);

	DEBUG_LOG(NAOMI, "Legacy ROM loaded successfully");

	CurrentCartridge = new DecryptedCartridge(romBase, romSize);
	strcpy(naomi_game_id, CurrentCartridge->GetGameId().c_str());
	NOTICE_LOG(NAOMI, "NAOMI GAME ID [%s]", naomi_game_id);
}

void naomi_cart_Close()
{
	delete CurrentCartridge;
	CurrentCartridge = nullptr;
	NaomiGameInputs = nullptr;
	bios_loaded = false;
}

int naomi_cart_GetPlatform(const char *path)
{
	Game *game = FindGame(path);
	if (game == NULL)
		return DC_PLATFORM_NAOMI;
	else if (game->cart_type == AW)
		return DC_PLATFORM_ATOMISWAVE;
	else
		return DC_PLATFORM_NAOMI;
}

Cartridge::Cartridge(u32 size)
{
	RomPtr = (u8 *)malloc(size);
	RomSize = size;
	memset(RomPtr, 0xFF, RomSize);
}

Cartridge::~Cartridge()
{
	if (RomPtr != NULL)
		free(RomPtr);
}

bool Cartridge::Read(u32 offset, u32 size, void* dst)
{
	offset &= 0x1FFFFFFF;
	if (offset >= RomSize || (offset + size) > RomSize)
	{
		static u32 ones = 0xffffffff;

		// Makes Outtrigger boot
		INFO_LOG(NAOMI, "offset %d > %d", offset, RomSize);
		memcpy(dst, &ones, size);
	}
	else
	{
		memcpy(dst, &RomPtr[offset], size);
	}

	return true;
}

bool Cartridge::Write(u32 offset, u32 size, u32 data)
{
	INFO_LOG(NAOMI, "Invalid write @ %08x data %x", offset, data);
	return false;
}

void* Cartridge::GetPtr(u32 offset, u32& size)
{
	offset &= 0x1FFFffff;

	verify(offset < RomSize);
	verify((offset + size) <= RomSize);

	return &RomPtr[offset];
}

std::string Cartridge::GetGameId() {
	if (RomSize < 0x30 + 0x20)
		return "(ROM too small)";

	std::string game_id((char *)RomPtr + 0x30, 0x20);
	if (game_id == "AWNAOMI                         " && RomSize >= 0xFF50)
	{
		game_id = std::string((char *)RomPtr + 0xFF30, 0x20);
	}
	while (!game_id.empty() && game_id.back() == ' ')
		game_id.pop_back();

	return game_id;
}

void* NaomiCartridge::GetDmaPtr(u32& size)
{
	if ((DmaOffset & 0x1fffffff) >= RomSize)
	{
		INFO_LOG(NAOMI, "Error: DmaOffset >= RomSize");
		size = 0;
		return NULL;
	}
	size = std::min(size, RomSize - (DmaOffset & 0x1fffffff));
	return GetPtr(DmaOffset, size);
}

u32 NaomiCartridge::ReadMem(u32 address, u32 size)
{
	verify(size!=1);

	switch(address & 255)
	{
	case 0x3c:	// 5f703c: DIMM COMMAND
		DEBUG_LOG(NAOMI, "DIMM COMMAND read<%d>", size);
		return 0xffff; //reg_dimm_command
	case 0x40:	// 5f7040: DIMM OFFSETL
		DEBUG_LOG(NAOMI, "DIMM OFFSETL read<%d>", size);
		return reg_dimm_offsetl;
	case 0x44:	// 5f7044: DIMM PARAMETERL
		DEBUG_LOG(NAOMI, "DIMM PARAMETERL read<%d>", size);
		return reg_dimm_parameterl;
	case 0x48:	// 5f7048: DIMM PARAMETERH
		DEBUG_LOG(NAOMI, "DIMM PARAMETERH read<%d>", size);
		return reg_dimm_parameterh;
	case 0x04C:	// 5f704c: DIMM STATUS
		DEBUG_LOG(NAOMI, "DIMM STATUS read<%d>", size);
		return reg_dimm_status;

	case NAOMI_ROM_OFFSETH_addr&255:
		return RomPioOffset>>16 | (RomPioAutoIncrement << 15);

	case NAOMI_ROM_OFFSETL_addr&255:
		return RomPioOffset&0xFFFF;

	case NAOMI_ROM_DATA_addr & 255:
		{
			u32 rv = 0;
			Read(RomPioOffset, 2, &rv);
			if (RomPioAutoIncrement)
				RomPioOffset += 2;

			return rv;
		}

	case NAOMI_DMA_COUNT_addr&255:
		return (u16) DmaCount;

	case NAOMI_BOARDID_READ_addr&255:
		return NaomiGameIDRead()?0x8000:0x0000;

		//What should i do to emulate 'nothing' ?
	case NAOMI_COMM_OFFSET_addr&255:
		#ifdef NAOMI_COMM
		DEBUG_LOG(NAOMI, "naomi COMM offs READ: %X, %d", address, size);
		return CommOffset;
		#endif
	case NAOMI_COMM_DATA_addr&255:
		#ifdef NAOMI_COMM
		DEBUG_LOG(NAOMI, "naomi COMM data read: %X, %d", CommOffset, size);
		if (CommSharedMem)
		{
			return CommSharedMem[CommOffset&0xF];
		}
		#endif
		return 1;


	case NAOMI_DMA_OFFSETH_addr&255:
		return DmaOffset>>16;
	case NAOMI_DMA_OFFSETL_addr&255:
		return DmaOffset&0xFFFF;

	case NAOMI_BOARDID_WRITE_addr&255:
		DEBUG_LOG(NAOMI, "naomi ReadBoardId: %X, %d", address, size);
		return 1;

	default:
		break;
	}
	DEBUG_LOG(NAOMI, "naomi?WTF? ReadMem: %X, %d", address, size);

	return 0xFFFF;
}

void NaomiCartridge::WriteMem(u32 address, u32 data, u32 size)
{
	switch(address & 255)
	{
	case 0x3c:	// 5f703c: DIMM COMMAND
		 if (0x1E03 == data)
		 {
			 /*
			 if (!(reg_dimm_status & 0x100))
				asic_RaiseInterrupt(holly_EXP_PCI);
			 reg_dimm_status |= 1;*/
		 }
		 reg_dimm_command = data;
		 DEBUG_LOG(NAOMI, "DIMM COMMAND Write: %X <= %X, %d", address, data, size);
		 return;

	case 0x40:	// 5f7040: DIMM OFFSETL
		reg_dimm_offsetl = data;
		DEBUG_LOG(NAOMI, "DIMM OFFSETL Write: %X <= %X, %d", address, data, size);
		return;
	case 0x44:	// 5f7044: DIMM PARAMETERL
		reg_dimm_parameterl = data;
		DEBUG_LOG(NAOMI, "DIMM PARAMETERL Write: %X <= %X, %d", address, data, size);
		return;
	case 0x48:	// 5f7048: DIMM PARAMETERH
		reg_dimm_parameterh = data;
		DEBUG_LOG(NAOMI, "DIMM PARAMETERH Write: %X <= %X, %d", address, data, size);
		return;

	case 0x4C:	// 5f704c: DIMM STATUS
		if (data&0x100)
		{
			asic_CancelInterrupt(holly_EXP_PCI);
		}
		else if ((data&1)==0)
		{
			/*FILE* ramd=fopen("c:\\ndc.ram.bin","wb");
			fwrite(mem_b.data,1,RAM_SIZE,ramd);
			fclose(ramd);*/
			naomi_process(reg_dimm_command, reg_dimm_offsetl, reg_dimm_parameterl, reg_dimm_parameterh);
		}
		reg_dimm_status = data & ~0x100;
		DEBUG_LOG(NAOMI, "DIMM STATUS Write: %X <= %X, %d", address, data, size);
		return;

		//These are known to be valid on normal ROMs and DIMM board
	case NAOMI_ROM_OFFSETH_addr&255:
		RomPioAutoIncrement = (data & 0x8000) != 0;
		RomPioOffset&=0x0000ffff;
		RomPioOffset|=(data<<16)&0x7fff0000;
		PioOffsetChanged(RomPioOffset);
		return;

	case NAOMI_ROM_OFFSETL_addr&255:
		RomPioOffset&=0xffff0000;
		RomPioOffset|=data;
		PioOffsetChanged(RomPioOffset);
		return;

	case NAOMI_ROM_DATA_addr&255:
		Write(RomPioOffset, size, data);
		if (RomPioAutoIncrement)
			RomPioOffset += 2;

		return;

	case NAOMI_DMA_OFFSETH_addr&255:
		DmaOffset&=0x0000ffff;
		DmaOffset|=(data&0x7fff)<<16;
		DmaOffsetChanged(DmaOffset);
		return;

	case NAOMI_DMA_OFFSETL_addr&255:
		DmaOffset&=0xffff0000;
		DmaOffset|=data;
		DmaOffsetChanged(DmaOffset);
		return;

	case NAOMI_DMA_COUNT_addr&255:
		{
			DmaCount=data;
		}
		return;
	case NAOMI_BOARDID_WRITE_addr&255:
		NaomiGameIDWrite((u16)data);
		return;

		//What should i do to emulate 'nothing' ?
	case NAOMI_COMM_OFFSET_addr&255:
#ifdef NAOMI_COMM
		DEBUG_LOG(NAOMI, "naomi COMM ofset Write: %X <= %X, %d", address, data, size);
		CommOffset=data&0xFFFF;
#endif
		return;

	case NAOMI_COMM_DATA_addr&255:
		#ifdef NAOMI_COMM
		DEBUG_LOG(NAOMI, "naomi COMM data Write: %X <= %X, %d", CommOffset, data, size);
		if (CommSharedMem)
		{
			CommSharedMem[CommOffset&0xF]=data;
		}
		#endif
		return;

		//This should be valid
	case NAOMI_BOARDID_READ_addr&255:
		DEBUG_LOG(NAOMI, "naomi WriteMem: %X <= %X, %d", address, data, size);
		return;

	default: break;
	}
	DEBUG_LOG(NAOMI, "naomi?WTF? WriteMem: %X <= %X, %d", address, data, size);
}

void NaomiCartridge::Serialize(Serializer& ser) const
{
	ser << RomPioOffset;
	ser << RomPioAutoIncrement;
	ser << DmaOffset;
	ser << DmaCount;
	Cartridge::Serialize(ser);
}

void NaomiCartridge::Deserialize(Deserializer& deser)
{
	deser >> RomPioOffset;
	deser >> RomPioAutoIncrement;
	deser >> DmaOffset;
	deser >> DmaCount;
	Cartridge::Deserialize(deser);
}

bool M2Cartridge::Read(u32 offset, u32 size, void* dst)
{
	if (offset & 0x40000000)
	{
		if (offset == 0x4001fffe)
		{
			//printf("NAOMI CART DECRYPT read: %08x sz %d\n", offset, size);
			cyptoSetKey(key);
			u16 data = cryptoDecrypt();
			*(u16 *)dst = data;
			return true;
		}
		INFO_LOG(NAOMI, "Invalid read @ %08x", offset);
		return false;
	}
	else if (!(RomPioOffset & 0x20000000))
	{
		// 4MB mode
		offset = (offset & 0x103fffff) | ((offset & 0x07c00000) << 1);
	}
	return NaomiCartridge::Read(offset, size, dst);
}

void* M2Cartridge::GetDmaPtr(u32& size)
{
	if (RomPioOffset & 0x20000000)
		return NaomiCartridge::GetDmaPtr(size);

	// 4MB mode
	u32 offset4mb = (DmaOffset & 0x103fffff) | ((DmaOffset & 0x07c00000) << 1);
	size = std::min(std::min(size, 0x400000 - (offset4mb & 0x3FFFFF)), RomSize - offset4mb);

	return GetPtr(offset4mb, size);
}

bool M2Cartridge::Write(u32 offset, u32 size, u32 data)
{
	if (offset & 0x40000000)
	{
		//printf("NAOMI CART CRYPT write: %08x data %x sz %d\n", offset, data, size);
		if (offset & 0x00020000)
		{
			offset &= sizeof(naomi_cart_ram) - 1;
			naomi_cart_ram[offset] = data;
			naomi_cart_ram[offset + 1] = data >> 8;
			return true;
		}
		switch (offset & 0x1ffff)
		{
			case 0x1fff8:
				cyptoSetLowAddr(data);
				return true;
			case 0x1fffa:
				cyptoSetHighAddr(data);
				return true;
			case 0x1fffc:
				cyptoSetSubkey(data);
				return true;
		}
	}
	return NaomiCartridge::Write(offset, size, data);
}

u16 M2Cartridge::ReadCipheredData(u32 offset)
{
	if ((offset & 0xffff0000) == 0x01000000)
	{
		int base = 2 * (offset & 0x7fff);
		return naomi_cart_ram[base + 1] | (naomi_cart_ram[base] << 8);
	}
	verify(2 * offset + 1 < RomSize);
	return RomPtr[2 * offset + 1] | (RomPtr[2 * offset] << 8);

}

std::string M2Cartridge::GetGameId()
{
	std::string game_id = NaomiCartridge::GetGameId();
	if ((game_id.size() < 2 || ((u8)game_id[0] == 0xff && (u8)game_id[1] == 0xff)) && RomSize >= 0x800050)
	{
		game_id = std::string((char *)RomPtr + 0x800030, 0x20);
		while (!game_id.empty() && game_id.back() == ' ')
			game_id.pop_back();
	}
	return game_id;
}

void M2Cartridge::Serialize(Serializer& ser) const {
	ser << naomi_cart_ram;
	NaomiCartridge::Serialize(ser);
}

void M2Cartridge::Deserialize(Deserializer& deser) {
	deser >> naomi_cart_ram;
	NaomiCartridge::Deserialize(deser);
}
