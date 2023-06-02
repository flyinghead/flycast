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
#include "hw/flashrom/nvmem.h"
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
#include "card_reader.h"
#include "naomi_flashrom.h"
#include "touchscreen.h"
#include "printer.h"
#include "oslib/storage.h"
#include "network/alienfnt_modem.h"
#include "netdimm.h"

Cartridge *CurrentCartridge;
bool bios_loaded = false;

InputDescriptors *NaomiGameInputs;
u8 *naomi_default_eeprom;

bool atomiswaveForceFeedback;

static bool loadBios(const char *filename, Archive *child_archive, Archive *parent_archive, int region)
{
	int biosid = 0;
	for (; BIOS[biosid].name != nullptr; biosid++)
		if (!stricmp(BIOS[biosid].name, filename))
			break;
	if (BIOS[biosid].name == nullptr)
	{
		WARN_LOG(NAOMI, "Unknown BIOS %s", filename);
		return false;
	}

	const BIOS_t *bios = &BIOS[biosid];

	std::string arch_name(filename);
	std::string path = hostfs::findNaomiBios(arch_name + ".zip");
	if (path.empty())
		path = hostfs::findNaomiBios(arch_name + ".7z");
	DEBUG_LOG(NAOMI, "Loading BIOS from %s", path.c_str());
	std::unique_ptr<Archive> bios_archive(OpenArchive(path));

	bool found_region = false;
	u8 *biosData = nvmem::getBiosData();

	for (int romid = 0; bios->blobs[romid].filename != nullptr && !found_region; romid++)
	{
		if (region == -1)
		{
			region = bios->blobs[romid].region;
			config::Region.override(region);
		}
		else if (bios->blobs[romid].region != (u32)region)
			continue;

		std::unique_ptr<ArchiveFile> file;
		// Find by CRC
		if (child_archive != nullptr)
			file.reset(child_archive->OpenFileByCrc(bios->blobs[romid].crc));
		if (!file && parent_archive != nullptr)
			file.reset(parent_archive->OpenFileByCrc(bios->blobs[romid].crc));
		if (!file && bios_archive != nullptr)
			file.reset(bios_archive->OpenFileByCrc(bios->blobs[romid].crc));
		// Fallback to find by filename
		if (!file && child_archive != nullptr)
			file.reset(child_archive->OpenFile(bios->blobs[romid].filename));
		if (!file && parent_archive != nullptr)
			file.reset(parent_archive->OpenFile(bios->blobs[romid].filename));
		if (!file && bios_archive != nullptr)
			file.reset(bios_archive->OpenFile(bios->blobs[romid].filename));
		if (!file) {
			WARN_LOG(NAOMI, "%s: Cannot open %s", filename, bios->blobs[romid].filename);
			continue;
		}
		verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
		u32 read = file->Read(biosData + bios->blobs[romid].offset, bios->blobs[romid].length);
		if (config::GGPOEnable)
		{
			MD5Sum md5;
			md5.add(biosData + bios->blobs[romid].offset, bios->blobs[romid].length);
			md5.getDigest(settings.network.md5.bios);
		}
		DEBUG_LOG(NAOMI, "Mapped %s: %x bytes at %07x", bios->blobs[romid].filename, read, bios->blobs[romid].offset);
		found_region = true;
	}

	// Reload the writeable portion of the FlashROM
	if (found_region)
		nvmem::reloadAWBios();

	return found_region;
}

static const Game *FindGame(const char *filename)
{
	std::string gameName = get_file_basename(filename);
	size_t folder_pos = get_last_slash_pos(gameName);	// Only for standard path
	if (folder_pos != std::string::npos)
		gameName = gameName.substr(folder_pos + 1);

	for (int i = 0; Games[i].name != nullptr; i++)
		if (gameName == Games[i].name)
			return &Games[i];

	return nullptr;
}

void naomi_cart_LoadBios(const char *filename)
{
	if (settings.naomi.slave)
	{
		if (!loadBios(filename, nullptr, nullptr, config::Region))
			throw NaomiCartException(std::string("Error: cannot load BIOS ") + filename);
		bios_loaded = true;
		return;
	}
	const Game *game = FindGame(filename);
	if (game == nullptr)
		return;

	// Open archive and parent archive if any
	std::unique_ptr<Archive> archive(OpenArchive(filename));

	std::unique_ptr<Archive> parent_archive;
	if (game->parent_name != nullptr)
	{
		std::string parentPath = hostfs::storage().getParentPath(filename);
		parentPath = hostfs::storage().getSubPath(parentPath, game->parent_name);
		parent_archive.reset(OpenArchive(parentPath));
	}

	const char *bios = "naomi";
	if (game->bios != nullptr)
		bios = game->bios;
	if (!loadBios(bios, archive.get(), parent_archive.get(), config::Region))
	{
		WARN_LOG(NAOMI, "Warning: Region %d bios not found in %s", (int)config::Region, bios);
		if (!loadBios(bios, archive.get(), parent_archive.get(), -1))
		{
			// If a specific BIOS is needed for this game, fail.
			if (game->bios != nullptr || !bios_loaded)
				throw NaomiCartException(std::string("Error: cannot load BIOS ") + (game->bios != nullptr ? game->bios : "naomi.zip"));

			// otherwise use the default BIOS
		}
	}
	bios_loaded = true;
}

static void loadMameRom(const std::string& path, const std::string& fileName, LoadProgress *progress)
{
	const Game *game = FindGame(fileName.c_str());
	if (game == nullptr)
		throw NaomiCartException("Unknown game");

	// Open archive and parent archive if any
	std::unique_ptr<Archive> archive(OpenArchive(path));
	if (archive != NULL)
		INFO_LOG(NAOMI, "Opened %s", path.c_str());

	std::unique_ptr<Archive> parent_archive;
	if (game->parent_name != nullptr)
	{
		std::string parentPath = hostfs::storage().getParentPath(path);
		parentPath = hostfs::storage().getSubPath(parentPath, game->parent_name);
		parent_archive.reset(OpenArchive(parentPath));
		if (parent_archive != nullptr)
			INFO_LOG(NAOMI, "Opened %s", game->parent_name);
	}

	if (archive == nullptr && parent_archive == nullptr)
	{
		if (game->parent_name != nullptr)
			throw NaomiCartException(std::string("Cannot open ") + fileName + std::string(" or ") + game->parent_name);
		else
			throw NaomiCartException(std::string("Cannot open ") + fileName);
	}

	// Load the BIOS
	naomi_cart_LoadBios(fileName.c_str());

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
				GDCartridge *gdcart;
				if (strncmp(game->name, "vf4", 3) == 0)
					gdcart = new NetDimm(game->size);
				else
					gdcart = new GDCartridge(game->size);
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
		CurrentCartridge->game = game;

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
				if (dst == nullptr || src == nullptr)
					throw NaomiCartException("Invalid ROM");
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
					WARN_LOG(NAOMI, "%s: Cannot open %s", fileName.c_str(), game->blobs[romid].filename);
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
							if (dst == nullptr)
								throw NaomiCartException(std::string("Invalid ROM: truncated ") + game->blobs[romid].filename);
							u32 read = file->Read(dst, game->blobs[romid].length);
							if (config::GGPOEnable)
								md5.add(dst, game->blobs[romid].length);
							DEBUG_LOG(NAOMI, "Mapped %s: %x bytes at %07x", game->blobs[romid].filename, read, game->blobs[romid].offset);
						}
						break;

					case InterleavedWord:
						{
							u8 *buf = (u8 *)malloc(game->blobs[romid].length);
							if (buf == nullptr)
								throw NaomiCartException("Memory allocation failed");

							u32 read = file->Read(buf, game->blobs[romid].length);
							u16 *to = (u16 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
							if (to == nullptr)
								throw NaomiCartException(std::string("Invalid ROM: truncated ") + game->blobs[romid].filename);
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
							if (buf == nullptr)
								throw NaomiCartException("Memory allocation failed");

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
							if (naomi_default_eeprom == nullptr)
								throw NaomiCartException("Memory allocation failed");

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
		// Default game name if ROM boot id isn't found
		settings.content.gameId = game->name;

	} catch (...) {
		delete CurrentCartridge;
		CurrentCartridge = NULL;
		throw;
	}
}

static void loadDecryptedRom(const std::string& path, const std::string& fileName, LoadProgress *progress)
{
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

	std::string extension = get_file_extension(fileName);
	if (extension == "lst")
	{
		// LST file
		FILE *fl = hostfs::storage().openFile(path, "r");
		if (!fl)
			throw FlycastException("Error: can't open " + path);
		folder = hostfs::storage().getParentPath(path);

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
		FILE* fp = hostfs::storage().openFile(path, "rb");
		if (fp == nullptr)
			throw FlycastException("Error: can't open " + path);

		std::fseek(fp, 0, SEEK_END);
		u32 file_size = (u32)std::ftell(fp);
		std::fclose(fp);
		files.emplace_back(path);
		fstart.push_back(0);
		fsize.push_back(file_size);
		romSize = file_size;
	}

	INFO_LOG(NAOMI, "+%zd romfiles, %.2f MB set address space", files.size(), romSize / 1024.f / 1024.f);
	if (romSize == 0)
		throw FlycastException("Invalid empty ROM");

	MD5Sum md5;

	// Allocate space for the rom
	u8 *romBase = (u8 *)malloc(romSize);
	if (romBase == nullptr)
		throw FlycastException("Out of memory");

	bool load_error = false;

	for (size_t i = 0; i<files.size(); i++)
	{
		FILE *fp = nullptr;

		if (files[i] != "null")
		{
			std::string filePath;
			if (folder.empty())
				filePath = files[i];
			else
				filePath = hostfs::storage().getSubPath(folder, files[i]);

			fp = hostfs::storage().openFile(filePath, "rb");
			if (fp == nullptr)
			{
				ERROR_LOG(NAOMI, "Unable to open file %s: error %d", filePath.c_str(), errno);
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
}

void naomi_cart_LoadRom(const std::string& path, const std::string& fileName, LoadProgress *progress)
{
	naomi_cart_Close();

	if (settings.naomi.slave)
	{
		CurrentCartridge = new NaomiCartridge(0);
		return;
	}
	std::string extension = get_file_extension(fileName);

	if (extension == "zip" || extension == "7z")
		loadMameRom(path, fileName, progress);
	else
		loadDecryptedRom(path, fileName, progress);

	atomiswaveForceFeedback = false;
	RomBootID bootId;
	if (CurrentCartridge->GetBootId(&bootId)
			&& (!memcmp(bootId.boardName, "NAOMI", 5)
					|| !memcmp(bootId.boardName, "Naomi2", 6)
					|| !memcmp(bootId.boardName, "SYSTEM_X_APP", 12))) // Atomiswave
	{
		std::string gameId = trim_trailing_ws(std::string(bootId.gameTitle[0], &bootId.gameTitle[0][32]));
		if (gameId == "SAMPLE GAME MAX LONG NAME-")
		{
			// Use better game names
			if (!strcmp(CurrentCartridge->game->name, "sgdrvsim"))
				gameId = "SEGA DRIVING SIMULATOR";
			else if (!strcmp(CurrentCartridge->game->name, "dragntr3"))
				gameId = "DRAGON TREASURE 3";
		}
		if (!gameId.empty())
			settings.content.gameId = gameId;
		NOTICE_LOG(NAOMI, "NAOMI GAME ID [%s] region %x players %x vertical %x", settings.content.gameId.c_str(), (u8)bootId.country, bootId.cabinet, bootId.vertical);

		if (gameId == "INITIAL D"
				|| gameId == "INITIAL D Ver.2"
				|| gameId == "INITIAL D Ver.3"
				|| gameId == "INITIAL D CYCRAFT")
		{
			card_reader::initdInit();
			initMidiForceFeedback();
		}
		else if (gameId == "MAXIMUM SPEED" || gameId == "FASTER THAN SPEED")
		{
			atomiswaveForceFeedback = true;
		}
		else if (gameId == "THE KING OF ROUTE66"
				|| gameId == "CLUB KART IN JAPAN"
				|| gameId == "SEGA DRIVING SIMULATOR")
		{
			if (settings.naomi.drivingSimSlave == 0)
				initMidiForceFeedback();
		}
		else if (gameId == "POKASUKA GHOST (JAPANESE)"	// Manic Panic Ghosts
				|| gameId == "TOUCH DE ZUNO (JAPAN)")
		{
			touchscreen::init();
		}
		else if (gameId.substr(0, 8) == "MKG TKOB"
				|| gameId.substr(0, 9) == "MUSHIKING")
		{
			card_reader::barcodeInit();
		}
		else if (gameId == "ALIEN FRONT")
		{
			serialModemInit();
		}
		if (gameId == " TOUCH DE UNOH -------------"
			|| gameId == " TOUCH DE UNOH 2 -----------"
					// only for F355 Deluxe
			|| (gameId == "F355 CHALLENGE JAPAN" && !strcmp(CurrentCartridge->game->name, "f355")))
		{
			printer::init();
		}

#ifdef NAOMI_MULTIBOARD
		// Not a multiboard game but needs the same desktop environment
		if (gameId == "SEGA DRIVING SIMULATOR")
		{
			initDriveSimSerialPipe();

			config::NetworkEnable.override(true);
			config::ActAsServer.override(settings.naomi.drivingSimSlave == 0);
			config::NetworkServer.override("localhost:" + std::to_string(config::LocalPort));
			config::LocalPort.override(config::LocalPort + settings.naomi.drivingSimSlave);
			if (settings.naomi.drivingSimSlave == 0)
			{
				int x = cfgLoadInt("window", "left", (1920 - 640) / 2);
				int w = cfgLoadInt("window", "width", 640);
				std::string region = "config:Dreamcast.Region=" + std::to_string(config::Region);
				for (int i = 0; i < 2; i++)
				{
					std::string slaveNum = "naomi:DrivingSimSlave=" + std::to_string(i + 1);
					std::string left = "window:left=" + std::to_string(i == 1 ? x - w : x + w);
					const char *args[] = {
						"-config", slaveNum.c_str(),
						"-config", region.c_str(),
						"-config", left.c_str(),
						settings.content.path.c_str()
					};
					os_RunInstance(std::size(args), args);
				}
			}
		}
#endif
	}
	else
	{
		NOTICE_LOG(NAOMI, "NAOMI GAME ID [%s]", settings.content.gameId.c_str());
	}
}

void naomi_cart_ConfigureEEPROM()
{
	if (!settings.platform.isNaomi())
		return;

	RomBootID bootId;
	if (CurrentCartridge->GetBootId(&bootId)
			&& (!memcmp(bootId.boardName, "NAOMI", 5) || !memcmp(bootId.boardName, "Naomi2", 6)))
		configure_naomi_eeprom(&bootId);
	else
		WARN_LOG(NAOMI, "Can't read ROM boot ID");
}

void naomi_cart_Close()
{
	touchscreen::term();
	printer::term();
	card_reader::initdTerm();
	card_reader::barcodeTerm();
	serialModemTerm();
	delete CurrentCartridge;
	CurrentCartridge = nullptr;
	NaomiGameInputs = nullptr;
	bios_loaded = false;
}

void naomi_cart_serialize(Serializer& ser)
{
	if (CurrentCartridge != nullptr)
		CurrentCartridge->Serialize(ser);
	touchscreen::serialize(ser);
	printer::serialize(ser);
}

void naomi_cart_deserialize(Deserializer& deser)
{
	if (CurrentCartridge != nullptr && (!settings.platform.isAtomiswave() || deser.version() >= Deserializer::V10_LIBRETRO))
		CurrentCartridge->Deserialize(deser);
	touchscreen::deserialize(deser);
	printer::deserialize(deser);
}

int naomi_cart_GetPlatform(const char *path)
{
	settings.naomi.multiboard = false;
	const Game *game = FindGame(path);
	if (game == nullptr)
		return DC_PLATFORM_NAOMI;
	else if (game->cart_type == AW)
		return DC_PLATFORM_ATOMISWAVE;
	else if (game->bios != nullptr && !strcmp("naomi2", game->bios))
		return DC_PLATFORM_NAOMI2;
	else
	{
#ifdef NAOMI_MULTIBOARD
		if (game->multiboard > 0)
			settings.naomi.multiboard = true;
#endif
		return DC_PLATFORM_NAOMI;
	}
}

Cartridge::Cartridge(u32 size)
{
	RomPtr = (u8 *)malloc(size);
	if (RomPtr == nullptr)
		throw NaomiCartException("Memory allocation failed");
	RomSize = size;
	if (size != 0)
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
	offset &= 0x1fffffff;

	if (offset >= RomSize || offset + size > RomSize)
	{
		WARN_LOG(NAOMI, "Invalid naomi cart: offset %x size %x rom size %x", offset, size, RomSize);
		size = 0;
		return nullptr;
	}

	return &RomPtr[offset];
}

bool NaomiCartridge::GetBootId(RomBootID *bootId)
{
	if (RomSize < sizeof(RomBootID))
		return false;
	u8 *p = (u8 *)bootId;
	u32 size = sizeof(RomBootID);
	DmaOffset = 0;
	while (size > 0)
	{
		u32 chunkSize = size;
		void *src = GetDmaPtr(chunkSize);
		if (chunkSize == 0)
			return false;
		memcpy(p, src, chunkSize);
		p += chunkSize;
		size -= chunkSize;
		AdvancePtr(chunkSize);
	}
	return true;
}

void* NaomiCartridge::GetDmaPtr(u32& size)
{
	if ((DmaOffset & 0x1fffffff) >= RomSize)
	{
		INFO_LOG(NAOMI, "Error: DmaOffset >= RomSize");
		size = 0;
		return nullptr;
	}
	size = std::min(size, RomSize - (DmaOffset & 0x1fffffff));
	return GetPtr(DmaOffset, size);
}

u32 NaomiCartridge::ReadMem(u32 address, u32 size)
{
//	verify(size != 1); not true anymore with multiboard

	switch (address)
	{
	case NAOMI_DIMM_COMMAND:
		//DEBUG_LOG(NAOMI, "DIMM COMMAND read");
		return 0xffff;
	case NAOMI_DIMM_OFFSETL:
		DEBUG_LOG(NAOMI, "DIMM OFFSETL read");
		return 0xffff;
	case NAOMI_DIMM_PARAMETERL:
		DEBUG_LOG(NAOMI, "DIMM PARAMETERL read");
		return 0xffff;
	case NAOMI_DIMM_PARAMETERH:
		DEBUG_LOG(NAOMI, "DIMM PARAMETERH read");
		return 0xffff;
	case NAOMI_DIMM_STATUS:
		DEBUG_LOG(NAOMI, "DIMM STATUS read");
		return 0xffff;

	case NAOMI_ROM_OFFSETH_addr:
		return RomPioOffset >> 16 | (RomPioAutoIncrement << 15);

	case NAOMI_ROM_OFFSETL_addr:
		return RomPioOffset & 0xFFFF;

	case NAOMI_ROM_DATA_addr:
		{
			u32 rv = 0;
			Read(RomPioOffset, 2, &rv);
			if (RomPioAutoIncrement)
				RomPioOffset += 2;

			return rv;
		}

	case NAOMI_DMA_COUNT_addr:
		return (u16)DmaCount;

	case NAOMI_BOARDID_READ_addr:
		return NaomiGameIDRead() ? 0x8000 : 0x0000;

	case NAOMI_DMA_OFFSETH_addr:
		return DmaOffset >> 16;
	case NAOMI_DMA_OFFSETL_addr:
		return DmaOffset & 0xFFFF;

	case NAOMI_BOARDID_WRITE_addr:
		DEBUG_LOG(NAOMI, "naomi ReadBoardId: %X, %d", address, size);
		return 1;

	default:
		break;
	}
	if (multiboard != nullptr)
		return multiboard->readG1(address, size);

	if (address == NAOMI_MBOARD_DATA_addr || address == NAOMI_MBOARD_OFFSET_addr)
		return 1;
	else {
		DEBUG_LOG(NAOMI, "naomiCart::ReadMem<%d> unknown: %08x", size, address);
		return 0xFFFF;
	}
}

void NaomiCartridge::WriteMem(u32 address, u32 data, u32 size)
{
	switch (address)
	{
	case NAOMI_DIMM_COMMAND:
		 DEBUG_LOG(NAOMI, "DIMM COMMAND Write<%d>: %x", size, data);
		 return;

	case NAOMI_DIMM_OFFSETL:
		DEBUG_LOG(NAOMI, "DIMM OFFSETL Write<%d>: %x", size, data);
		return;
	case NAOMI_DIMM_PARAMETERL:
		DEBUG_LOG(NAOMI, "DIMM PARAMETERL Write<%d>: %x", size, data);
		return;
	case NAOMI_DIMM_PARAMETERH:
		DEBUG_LOG(NAOMI, "DIMM PARAMETERH Write<%d>: %x", size, data);
		return;

	case NAOMI_DIMM_STATUS:
		DEBUG_LOG(NAOMI, "DIMM STATUS Write<%d>: %x", size, data);
		return;

		//These are known to be valid on normal ROMs and DIMM board
	case NAOMI_ROM_OFFSETH_addr:
		RomPioAutoIncrement = (data & 0x8000) != 0;
		RomPioOffset &= 0x0000ffff;
		RomPioOffset |= (data << 16) & 0x7fff0000;
		PioOffsetChanged(RomPioOffset);
		return;

	case NAOMI_ROM_OFFSETL_addr:
		RomPioOffset &= 0xffff0000;
		RomPioOffset |= data;
		PioOffsetChanged(RomPioOffset);
		return;

	case NAOMI_ROM_DATA_addr:
		Write(RomPioOffset, size, data);
		if (RomPioAutoIncrement)
			RomPioOffset += 2;

		return;

	case NAOMI_DMA_OFFSETH_addr:
		DmaOffset &= 0x0000ffff;
		DmaOffset |= (data & 0x7fff) << 16;
		DmaOffsetChanged(DmaOffset);
		return;

	case NAOMI_DMA_OFFSETL_addr:
		DmaOffset &= 0xffff0000;
		DmaOffset |= data;
		DmaOffsetChanged(DmaOffset);
		return;

	case NAOMI_DMA_COUNT_addr:
		DmaCount = data;
		return;

	case NAOMI_BOARDID_WRITE_addr:
		NaomiGameIDWrite((u16)data);
		return;

		//This should be valid
	case NAOMI_BOARDID_READ_addr:
		DEBUG_LOG(NAOMI, "naomi WriteMem: %X <= %X, %d", address, data, size);
		return;

	case NAOMI_LED_addr:
		DEBUG_LOG(NAOMI, "LED %d %d %d %d %d %d %d %d", (data >> 7) & 1, (data >> 6) & 1, (data >> 5) & 1, (data >> 4) & 1,
				(data >> 3) & 1, (data >> 2) & 1, (data >> 1) & 1, data & 1);
		return;

	default:
		break;
	}
	if (multiboard != nullptr)
		multiboard->writeG1(address, size, data);
	else
		DEBUG_LOG(NAOMI, "naomiCart::WriteMem<%d>: unknown %08x <= %x", size, address, data);
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

bool M2Cartridge::GetBootId(RomBootID *bootId)
{
	if (RomSize < sizeof(RomBootID))
		return false;
	RomBootID *pBootId = (RomBootID *)RomPtr;
	if ((pBootId->gameTitle[0][0] == '\0'
			|| ((u8)pBootId->gameTitle[0][0] == 0xff && (u8)pBootId->gameTitle[0][1] == 0xff)))
	{
		if (RomSize < 0x800000 + sizeof(RomBootID))
			return false;
		pBootId = (RomBootID *)(RomPtr + 0x800000);
	}
	memcpy(bootId, pBootId, sizeof(RomBootID));

	return true;
}

void M2Cartridge::Serialize(Serializer& ser) const {
	ser << naomi_cart_ram;
	NaomiCartridge::Serialize(ser);
}

void M2Cartridge::Deserialize(Deserializer& deser) {
	deser >> naomi_cart_ram;
	NaomiCartridge::Deserialize(deser);
}
