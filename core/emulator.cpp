/*
    Copyright 2021 flyinghead

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
#include "emulator.h"
#include "types.h"
#include "stdclass.h"
#include "cfg/option.h"
#include "hw/aica/aica_if.h"
#include "imgread/common.h"
#include "hw/naomi/naomi_cart.h"
#include "reios/reios.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/flashrom/nvmem.h"
#include "cheats.h"
#include "audio/audiostream.h"
#include "debug/gdb_server.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/arm7/arm7_rec.h"
#include "network/ggpo.h"
#include "network/ice.h"
#include "hw/mem/mem_watch.h"
#include "network/net_handshake.h"
#include "network/naomi_network.h"
#include "serialize.h"
#include "hw/pvr/pvr.h"
#include "profiler/fc_profiler.h"
#include "oslib/storage.h"
#include "wsi/context.h"
#include <chrono>
#ifndef LIBRETRO
#include "ui/gui.h"
#endif
#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/dyna/ngen.h"
#include "oslib/i18n.h"

settings_t settings;
constexpr char const *BIOS_TITLE = "Dreamcast BIOS";

static void loadSpecialSettings()
{
	std::string& prod_id = settings.content.gameId;
	NOTICE_LOG(BOOT, "Game ID is [%s]", prod_id.c_str());

	if (settings.platform.isConsole())
	{
		// Tony Hawk's Pro Skater 2
		if (prod_id == "T13008D 05" || prod_id == "T13006N"
				// Tony Hawk's Pro Skater 1
				|| prod_id == "T40205N"
				// Tony Hawk's Skateboarding
				|| prod_id == "T40204D 50"
				// Skies of Arcadia
				|| prod_id == "MK-51052"
				// Eternal Arcadia (JP)
				|| prod_id == "HDR-0076"
				// Flag to Flag (US)
				|| prod_id == "MK-51007"
				// Super Speed Racing (JP)
				|| prod_id == "HDR-0013"
				// Yu Suzuki Game Works Vol. 1
				|| prod_id == "6108099"
				// L.O.L
				|| prod_id == "T2106M"
				// Miss Moonlight
				|| prod_id == "T18702M"
				// Tom Clancy's Rainbow Six (US)
				|| prod_id == "T40401N"
				// Tom Clancy's Rainbow Six incl. Eagle Watch Missions (EU)
				|| prod_id == "T-45001D05"
				// Jet Grind Radio (US)
				|| prod_id == "MK-51058"
				// JSR (JP)
				|| prod_id == "HDR-0078"
				// JSR (EU)
				|| prod_id == "MK-5105850"
				// Worms World Party (US)
				|| prod_id == "T22904N"
				// Worms World Party (EU)
				|| prod_id == "T7016D  50"
				// Shenmue (US)
				|| prod_id == "MK-51059"
				// Shenmue (EU)
				|| prod_id == "MK-5105950"
				// Shenmue (JP)
				|| prod_id == "HDR-0016"
				// Izumo
				|| prod_id == "T46902M"
				// Cardcaptor Sakura
				|| prod_id == "HDR-0115"
				// Grandia II (US)
				|| prod_id == "T17716N"
				// Grandia II (EU)
				|| prod_id == "T17715D"
				// Grandia II (JP)
				|| prod_id == "T4503M"
				// Canvas: Sepia Iro no Motif
				|| prod_id == "T20108M"
				// Kimi ga Nozomu Eien
				|| prod_id == "T47101M"
				// Pro Mahjong Kiwame D
				|| prod_id == "T16801M"
				// Yoshia no Oka de Nekoronde...
				|| prod_id == "T18704M"
				// Tamakyuu (a.k.a. Tama-cue)
				|| prod_id == "T20133M"
				// Sakura Taisen 1
				|| prod_id == "HDR-0072"
				// Sakura Taisen 3
				|| prod_id == "HDR-0152"
				// Hundred Swords
				|| prod_id == "HDR-0124"
				// Musapey's Choco Marker
				|| prod_id == "T23203M"
				// Sister Princess Premium Edition
				|| prod_id == "T27802M"
				// Sentimental Graffiti
				|| prod_id == "T20128M"
				// Sentimental Graffiti 2
				|| prod_id == "T20104M"
				// Kanon
				|| prod_id == "T20105M"
				// Aikagi
				|| prod_id == "T20130M"
				// AIR
				|| prod_id == "T20112M"
				// Cool Boarders Burrrn (JP)
				|| prod_id == "T36901M"
				// Castle Fantasia - Seima Taisen (JP)
				|| prod_id == "T46901M"
				// Silent Scope (US)
				|| prod_id == "T9507N"
				// Silent Scope (EU)
				|| prod_id == "T9505D"
				// Silent Scope (JP)
				|| prod_id == "T9513M"
				// Pro Pinball - Trilogy (EU)
				|| prod_id == "T30701D 50"
				// Jikkyo Powerful Pro Yakyu
				|| prod_id == "T9507M")
		{
			INFO_LOG(BOOT, "Enabling RTT Copy to VRAM for game %s", prod_id.c_str());
			config::RenderToTextureBuffer.override(true);
		}
		// Cosmic Smash
		if (prod_id == "HDR-0176" || prod_id == "RDC-0057")
		{
			INFO_LOG(BOOT, "Enabling translucent depth multipass for game %s", prod_id.c_str());
			config::TranslucentPolygonDepthMask.override(true);
		}
		// Extra Depth Scaling
		if (prod_id == "MK-51182")			// NHL 2K2
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(1e8f);
		}
		else if (prod_id == "T-8109N"		// Re-Volt (US, EU, JP)
				|| prod_id == "T8107D  50"
				|| prod_id == "T-8101M"
				|| prod_id ==  "DR001")		// Sturmwind
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(100.f);
		}
		else if (prod_id == "T15110N"		// Test Drive V-Rally
				|| prod_id == "T15105D 50")
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(0.1f);
		}
		else if (prod_id == "T-8116N"		// South Park Rally
				|| prod_id == "T-8112D-50")
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(1000.f);
		}
		else if (prod_id == "T1247M")		// Capcom vs. SNK - Millennium Fight 2000 Pro
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(10000.f);
		}

		std::string areas(ip_meta.area_symbols, sizeof(ip_meta.area_symbols));
		bool region_usa = areas.find('U') != std::string::npos;
		bool region_eu = areas.find('E') != std::string::npos;
		bool region_japan = areas.find('J') != std::string::npos;
		if (region_usa || region_eu || region_japan)
		{
			switch (config::Region)
			{
			case 0: // Japan
				if (!region_japan)
				{
					NOTICE_LOG(BOOT, "Japan region not supported. Using %s instead", region_usa ? "USA" : "Europe");
					config::Region.override(region_usa ? 1 : 2);
				}
				break;
			case 1: // USA
				if (!region_usa)
				{
					NOTICE_LOG(BOOT, "USA region not supported. Using %s instead", region_eu ? "Europe" : "Japan");
					config::Region.override(region_eu ? 2 : 0);
				}
				break;
			case 2: // Europe
				if (!region_eu)
				{
					NOTICE_LOG(BOOT, "Europe region not supported. Using %s instead", region_usa ? "USA" : "Japan");
					config::Region.override(region_usa ? 1 : 0);
				}
				break;
			case 3: // Default
				if (region_usa)
					config::Region.override(1);
				else if (region_eu)
					config::Region.override(2);
				else
					config::Region.override(0);
				break;
			}
		}
		else
			WARN_LOG(BOOT, "No region specified in IP.BIN");
		if (config::Cable <= 1 && (!ip_meta.supportsVGA()
				|| prod_id == "T-12504N"	// Caesar's Palace (NTSC)
				|| prod_id == "12502D-50"))	// Caesar's Palace (PAL)
		{
			NOTICE_LOG(BOOT, "Game doesn't support VGA. Using TV Composite instead");
			config::Cable.override(3);
		}
		if (config::Cable == 2 &&
				(prod_id == "T40602N"	 // Centipede
				|| prod_id == "T9710N"   // Gauntlet Legends (US)
				|| prod_id == "MK-51152" // World Series Baseball 2K2
				|| prod_id == "T-9701N"	 // Mortal Kombat Gold (US)
				|| prod_id == "T1203N"	 // Street Fighter Alpha 3 (US)
				|| prod_id == "T1203M"	 // Street Fighter Zero 3 (JP)
				|| prod_id == "T13002N"	 // Vigilante 8 (US)
				|| prod_id == "T13003N"	 // Toy Story 2 (US)
				|| prod_id == "T1209N"	 // Gigawing (US)
				|| prod_id == "T1208M"	 // Gigawing (JP)
				|| prod_id == "T1235M"   // Vampire Chronicle for Matching Service
				|| prod_id == "T22901N"  // Roadsters (US)
				|| prod_id == "T28202M"))// Shin Nihon Pro Wrestling 4
		{
			NOTICE_LOG(BOOT, "Game doesn't support RGB. Using TV Composite instead");
			config::Cable.override(3);
		}
		if (prod_id == "T7001D  50"	// Jimmy White's 2 Cueball
			|| prod_id == "T40505D 50"	// Railroad Tycoon 2 (EU)
			|| prod_id == "T18702M"		// Miss Moonlight
			|| prod_id == "T0019M"		// KenJu Atomiswave DC Conversion
			|| prod_id == "T0020M"		// Force Five Atomiswave DC Conversion
			|| prod_id == "HDR-0187"	// Fushigi no Dungeon - Fuurai no Shiren Gaiden - Onna Kenshi Asuka Kenzan!
			|| prod_id == "T15104D 50"	// Slave Zero (PAL)
			|| prod_id == "MK-51152")	// World Series Baseball 2K2
		{
			NOTICE_LOG(BOOT, "Forcing real BIOS");
			config::UseReios.override(false);
		}
		else if (prod_id == "T17708N"	// Stupid Invaders (US)
			|| prod_id == "T17711D"		// Stupid Invaders (EU)
			|| prod_id == "T46509M"		// Suika (JP)
			|| prod_id == "T36901M")	// Cool Boarders Burrrn (JP)
		{
			NOTICE_LOG(BOOT, "Forcing HLE BIOS");
			config::UseReios.override(true);
		}
		if (prod_id == "T-9707N"		// San Francisco Rush 2049 (US)
			|| prod_id == "MK-51146"	// Sega Smash Pack - Volume 1
			|| prod_id == "T-9702D-50"	// Hydro Thunder (PAL)
			|| prod_id == "T41601N"		// Elemental Gimmick Gear (US)
			|| prod_id == "T-8116N"		// South Park Rally (US)
			|| prod_id == "T1206N")		// JoJo's Bizarre Adventure (US)
		{
			NOTICE_LOG(BOOT, "Forcing NTSC broadcasting");
			config::Broadcast.override(0);
		}
		else if (prod_id == "T-9709D-50"	// San Francisco Rush 2049 (EU)
			|| prod_id == "T-8112D-50"		// South Park Rally (EU)
			|| prod_id == "T7014D  50"		// Super Runabout (EU)
			|| prod_id == "T10001D 50"		// MTV Sport - Skateboarding (PAL)
			|| prod_id == "MK-5101050"		// Snow Surfers
			|| prod_id == "12502D-50")		// Caesar's Palace (PAL)
		{
			NOTICE_LOG(BOOT, "Forcing PAL broadcasting");
			config::Broadcast.override(1);
		}
		if (prod_id == "T1102M"				// Densha de Go! 2
				|| prod_id == "T00000A"		// The Ring of the Nibelungen (demo, hack)
				|| prod_id == "T15124N 00"	// Worms Pinball (prototype)
				|| prod_id == "T9503M"		// Eisei Meijin III
				|| prod_id == "T5202M"		// Marionette Company
				|| prod_id == "T5301M")		// World Neverland Plus
		{
			NOTICE_LOG(BOOT, "Forcing Full Framebuffer Emulation");
			config::EmulateFramebuffer.override(true);
		}
		if (prod_id == "T-8102N")		// TrickStyle (US)
		{
			NOTICE_LOG(BOOT, "Forcing English Language");
			config::Language.override(1);
		}
		if (prod_id == "T-9701N"			// Mortal Kombat (US)
				|| prod_id == "T9701D")		// Mortal Kombat (EU)
		{
			NOTICE_LOG(BOOT, "Disabling Native Depth Interpolation");
			config::NativeDepthInterpolation.override(false);
		}
		// Per-pixel transparent layers
		int layers = 0;
		if (prod_id == "MK-51011"			// Time Stalkers (US)
				|| prod_id == "MK-5101153")	// Time Stalkers (EU)
			layers = 72;
		else if (prod_id == "T13001N"		// Blue Stinger (US)
				|| prod_id == "HDR-0003"	// Blue Stinger (JP)
				|| prod_id == "T13001D-05"	// Blue Stinger (EU)
				|| prod_id == "T13001D 18")	// Blue Stinger (DE)
			layers = 80;
		else if (prod_id == "T2102M"		// Panzer Front
				|| prod_id == "T-8118N"		// Spirit of Speed (US)
				|| prod_id == "T-8117D-50"	// Spirit of Speed (EU)
				|| prod_id == "T13002N"		// Vigilante 8 (US)
				|| prod_id == "T13002D")	// Vigilante 8 (EU)
			layers = 64;
		else if (prod_id == "T2106M")		// L.O.L. Lack of Love
			layers = 48;
		else if (prod_id == "T1212M")		// Gaiamaster - Kessen! Seikioh Densetsu
			layers = 96;
		else if (prod_id == "T-9707N"		// San Francisco Rush 2049 (US)
				|| prod_id == "T-9709D-50"	// San Francisco Rush 2049 (EU)
				|| prod_id == "T17721N"		// Conflict Zone (US)
				|| prod_id == "T46604D")	// Conflict Zone (EU)
			layers = 152;
		else if (prod_id == "MK-51033"		// ECCO the Dolphin (US)
				|| prod_id == "MK-5103350"	// ECCO the Dolphin (EU)
				|| prod_id == "HDR-0103")	// ECCO the Dolphin (JP)
			layers = 96;
		else if (prod_id == "T40203N")		// Draconus: Cult of the Wyrm
			layers = 80;
		else if (prod_id == "T40212N"		// Soldier of Fortune (US)
				|| prod_id == "T17726D 50")	// Soldier of Fortune (EU)
			layers = 86;
		else if (prod_id == "T44102N")		// BANG! Gunship Elite
			layers = 100;
		else if (prod_id == "T12502N"		// MDK 2 (US)
				|| prod_id == "T12501D 50")	// MDK 2 (EU)
			layers = 200;
		else if (prod_id == "T9708D  50")	// Army Men
			layers = 173;
		else if (prod_id == "MK-51038"		// Zombie Revenge (US)
				|| prod_id == "MK-5103850"	// Zombie Revenge (EU)
				|| prod_id == "HDR-0026"	// Zombie Revenge (JP)
				|| prod_id == "36801N"		// Fighting Force 2 (US)
				|| prod_id == "36802D 80"	// Fighting Force 2 (PAL, en-fr)
				|| prod_id == "36802D 18")	// Fighting Force 2 (PAL, de)
			layers = 116;
		else if (prod_id == "T15112N")		// Demolition Racer (US)
			layers = 44;
		else if (prod_id == "T1208N"		// Tech Romancer (US)
				|| prod_id == "T7009D50")	// Tech Romancer (EU)
			layers = 56;
		if (layers != 0) {
			NOTICE_LOG(BOOT, "Forcing %d transparent layers", layers);
			config::PerPixelLayers.override(layers);
		}
	}
	else if (settings.platform.isArcade())
	{
		if (prod_id == "COSMIC SMASH IN JAPAN")
		{
			INFO_LOG(BOOT, "Enabling translucent depth multipass for game %s", prod_id.c_str());
			config::TranslucentPolygonDepthMask.override(true);
		}
		if (prod_id == "BEACH SPIKERS JAPAN"
				|| prod_id == "CHOCO MARKER"
				|| prod_id == "LOVE AND BERRY USA VER1.003"		// lovebero
				|| prod_id == "LOVE AND BERRY USA VER2.000")	// lovebery
		{
			INFO_LOG(BOOT, "Enabling RTT Copy to VRAM for game %s", prod_id.c_str());
			config::RenderToTextureBuffer.override(true);
		}
		if (prod_id == "RADIRGY NOA")
		{
			INFO_LOG(BOOT, "Disabling Free Play for game %s", prod_id.c_str());
			config::ForceFreePlay.override(false);
		}
		if (prod_id == "VIRTUAL-ON ORATORIO TANGRAM") {
			INFO_LOG(BOOT, "Forcing Japan region for game %s", prod_id.c_str());
			config::Region.override(0);
		}
		if (prod_id == "CAPCOM VS SNK PRO  JAPAN")
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(10000.f);
		}
	}
}

void Emulator::dc_reset(bool hard)
{
	if (hard)
	{
		NetworkHandshake::term();
		memwatch::unprotect();
		memwatch::reset();
	}
	sh4_sched_reset(hard);
	pvr::reset(hard);
	aica::reset(hard);
	getSh4Executor()->Reset(true);
	mem_Reset(hard);
}

static void setPlatform(int platform)
{
	if (VRAM_SIZE != 0)
		addrspace::unprotectVram(0, VRAM_SIZE);
	elan::ERAM_SIZE = 0;
	switch (platform)
	{
	case DC_PLATFORM_DREAMCAST:
		settings.platform.ram_size = config::RamMod32MB ? 32_MB : 16_MB;
		settings.platform.vram_size = 8_MB;
		settings.platform.aram_size = 2_MB;
		settings.platform.bios_size = 2_MB;
		settings.platform.flash_size = 128_KB;
		break;
	case DC_PLATFORM_NAOMI:
		settings.platform.ram_size = 32_MB;
		settings.platform.vram_size = 16_MB;
		settings.platform.aram_size = 8_MB;
		settings.platform.bios_size = 2_MB;
		settings.platform.flash_size = 32_KB;	// battery-backed ram
		break;
	case DC_PLATFORM_NAOMI2:
		settings.platform.ram_size = 32_MB;
		settings.platform.vram_size = 16_MB; // 2x16 MB VRAM, only 16 emulated
		settings.platform.aram_size = 8_MB;
		settings.platform.bios_size = 2_MB;
		settings.platform.flash_size = 32_KB;	// battery-backed ram
		elan::ERAM_SIZE = 32_MB;
		break;
	case DC_PLATFORM_ATOMISWAVE:
		settings.platform.ram_size = 16_MB;
		settings.platform.vram_size = 8_MB;
		settings.platform.aram_size = 2_MB;
		settings.platform.bios_size = 128_KB;
		settings.platform.flash_size = 128_KB;	// sram
		break;
	case DC_PLATFORM_SYSTEMSP:
		settings.platform.ram_size = 32_MB;
		settings.platform.vram_size = 16_MB;
		settings.platform.aram_size = 8_MB;
		settings.platform.bios_size = 2_MB;
		settings.platform.flash_size = 128_KB;	// sram
		break;
	default:
		die("Unsupported platform");
		break;
	}
	settings.platform.system = platform;
	settings.platform.ram_mask = settings.platform.ram_size - 1;
	settings.platform.vram_mask = settings.platform.vram_size - 1;
	settings.platform.aram_mask = settings.platform.aram_size - 1;
	addrspace::initMappings();
}

void Emulator::init()
{
	if (state != Uninitialized)
	{
		verify(state == Init);
		return;
	}
	// Default platform
	setPlatform(DC_PLATFORM_DREAMCAST);

	libGDR_init();
	pvr::init();
	aica::init();
	mem_Init();
	reios_init();

	// the recompiler may start generating code at this point and needs a fully configured machine
#if FEAT_SHREC != DYNAREC_NONE
	recompiler = Get_Sh4Recompiler();
	recompiler->Init();
	if(config::DynarecEnabled)
		INFO_LOG(DYNAREC, "Using Recompiler");
	else
#endif
		INFO_LOG(INTERPRETER, "Using Interpreter");
	interpreter = Get_Sh4Interpreter();
	interpreter->Init();
	state = Init;
}

Sh4Executor *Emulator::getSh4Executor()
{
#if FEAT_SHREC != DYNAREC_NONE
	if(config::DynarecEnabled)
		return recompiler;
	else
#endif
		return interpreter;
}

int getGamePlatform(const std::string& filename)
{
	if (settings.naomi.slave)
		// Multiboard slave
		return DC_PLATFORM_NAOMI;

	if (filename.empty())
		// Dreamcast BIOS
		return DC_PLATFORM_DREAMCAST;

	std::string extension = get_file_extension(filename);
	if (extension.empty())
		return DC_PLATFORM_DREAMCAST;	// unknown
	if (extension == "zip" || extension == "7z")
		return naomi_cart_GetPlatform(filename.c_str());
	if (extension == "bin" || extension == "dat" || extension == "lst")
		return DC_PLATFORM_NAOMI;

	return DC_PLATFORM_DREAMCAST;
}

void Emulator::loadGame(const char *path, LoadProgress *progress)
{
	init();
	try {
		DEBUG_LOG(BOOT, "Loading game %s", path == nullptr ? "(nil)" : path);

		if (path != nullptr && strlen(path) > 0)
		{
			settings.content.path = path;
			if (settings.naomi.slave) {
				settings.content.fileName = path;
			}
			else
			{
				hostfs::FileInfo info = hostfs::storage().getFileInfo(settings.content.path);
				settings.content.fileName = info.name;
				if (settings.content.title.empty())
					settings.content.title = get_file_basename(info.name);
			}
		}
		else
		{
			settings.content.path.clear();
			settings.content.fileName.clear();
		}

		setPlatform(getGamePlatform(settings.content.fileName));
		mem_map_default();

		config::Settings::instance().reset();
		config::Settings::instance().load(false);
		dc_reset(true);
		memset(&settings.network.md5, 0, sizeof(settings.network.md5));

		if (settings.platform.isConsole())
		{
			if (settings.content.path.empty())
			{
				// Boot BIOS
				if (!nvmem::loadFiles())
					throw FlycastException(strprintf(i18n::T("No BIOS file found in %s"), hostfs::getFlashSavePath("", "").c_str()));
				gdr::initDrive("");
			}
			else
			{
				std::string extension = get_file_extension(settings.content.path);
				if (extension != "elf")
				{
					if (gdr::initDrive(settings.content.path))
					{
						loadGameSpecificSettings();
						if (config::UseReios || !nvmem::loadFiles())
						{
							nvmem::loadHle();
							NOTICE_LOG(BOOT, "Did not load BIOS, using reios");
							if (!config::UseReios && config::UseReios.isReadOnly())
								os_notify(i18n::T("This game requires a real BIOS"), 15000);
						}
					}
					else
					{
						// Content load failed. Boot the BIOS
						settings.content.path.clear();
						if (!nvmem::loadFiles())
							throw FlycastException(i18n::Ts("This media cannot be loaded"));
						gdr::initDrive("");
					}
				}
				else
				{
					// Elf only supported with HLE BIOS
					nvmem::loadHle();
					gdr::initDrive("");
				}
			}
			if (settings.content.path.empty())
				settings.content.title = BIOS_TITLE;

			if (progress)
				progress->progress = 1.0f;
		}
		else if (settings.platform.isArcade())
		{
			nvmem::loadFiles();
			naomi_cart_LoadRom(settings.content.path, settings.content.fileName, progress);
			loadGameSpecificSettings();
			// Reload the BIOS in case a game-specific region is set
			naomi_cart_LoadBios(path);
		}
		if (!settings.naomi.slave)
		{
			mcfg_DestroyDevices();
			mcfg_CreateDevices();
			if (settings.platform.isNaomi())
				// Must be done after the maple devices are created and EEPROM is accessible
				naomi_cart_ConfigureEEPROM();
		}
#ifdef USE_RACHIEVEMENTS
		// RA probably isn't expecting to travel back in the past so disable it
		if (config::GGPOEnable)
			config::EnableAchievements.override(false);
		// Hardcore mode disables all cheats, under/overclocking, load state, lua and forces dynarec on
		settings.raHardcoreMode = config::EnableAchievements && config::AchievementsHardcoreMode
			&& !NaomiNetworkSupported();
#endif
		cheatManager.reset(settings.content.gameId);
		if (cheatManager.isWidescreen())
		{
			os_notify(i18n::T("Widescreen cheat activated"), 2000);
			config::ScreenStretching.override(134);	// 4:3 -> 16:9
		}
		// reload settings so that all settings can be overridden
		loadGameSpecificSettings();
		NetworkHandshake::init();
		settings.input.fastForwardMode = false;
		EventManager::event(Event::Start);
		if (!settings.content.path.empty())
		{
#ifndef LIBRETRO
			if (config::GGPOEnable)
				dc_loadstate(-1);
			else if (config::AutoLoadState && !NaomiNetworkSupported() && !settings.naomi.multiboard)
				dc_loadstate(config::SavestateSlot);
#endif
		}

		if (progress)
		{
#ifdef GDB_SERVER
			if (config::GDB && config::GDBWaitForConnection)
				progress->label = "Waiting for debugger...";
			else
#endif
				progress->label = i18n::T("Starting...");
		}

		state = Loaded;
	} catch (...) {
		state = Error;
		throw;
	}
}

void Emulator::runInternal()
{
	runner.init();
	try {
		if (singleStep)
		{
			getSh4Executor()->Step();
			singleStep = false;
		}
		else if (stepRangeTo != 0)
		{
			while (Sh4cntx.pc >= stepRangeFrom && Sh4cntx.pc < stepRangeTo)
				getSh4Executor()->Step();

			stepRangeFrom = 0;
			stepRangeTo = 0;
		}
		else
		{
			do {
				resetRequested = false;

				getSh4Executor()->Run();

				if (resetRequested)
				{
					nvmem::saveFiles();
					dc_reset(false);
					if (!restartCpu())
						resetRequested = false;
				}
			} while (resetRequested);
		}
	} catch (...) {
		runner.term();
		throw;
	}
}

void Emulator::unloadGame()
{
	try {
		stop();
	} catch (...) { }
	if (state == Loaded || state == Error)
	{
#ifndef LIBRETRO
		if (state == Loaded && config::AutoSaveState && !settings.content.path.empty()
				&& !settings.naomi.multiboard && !config::GGPOEnable && !NaomiNetworkSupported())
			gui_saveState(false);
#endif
		try {
			dc_reset(true);
		} catch (const FlycastException& e) {
			ERROR_LOG(COMMON, "%s", e.what());
		}
		// Flush the VMU files to disk
		mcfg_DestroyDevices(true);
		config::Settings::instance().reset();
		config::Settings::instance().load(false);
		settings.content.path.clear();
		settings.content.gameId.clear();
		settings.content.fileName.clear();
		settings.content.title.clear();
		settings.platform.system = DC_PLATFORM_DREAMCAST;
		custom_texture.terminate();
		state = Init;
		EventManager::event(Event::Terminate);
	}
}

void Emulator::term()
{
	unloadGame();
	if (state == Init)
	{
		debugger::term();
		if (interpreter != nullptr)
		{
			interpreter->Term();
			delete interpreter;
			interpreter = nullptr;
		}
		if (recompiler != nullptr)
		{
			recompiler->Term();
			delete recompiler;
			recompiler = nullptr;
		}
		custom_texture.terminate();	// lr: avoid deadlock on exit (win32)
		reios_term();
		aica::term();
		pvr::term();
		mem_Term();
		libGDR_term();
		ice::term();

		state = Terminated;
	}
	addrspace::release();
}

void Emulator::stop()
{
	if (state != Running)
		return;
	// Avoid race condition with GGPO restarting the sh4 for a new frame
	if (config::GGPOEnable)
		NetworkHandshake::term();
	{
		const std::lock_guard<std::mutex> _(mutex);
		// must be updated after GGPO is stopped since it may run some rollback frames
		state = Loaded;
		getSh4Executor()->Stop();
	}
	if (config::ThreadedRendering)
	{
		rend_cancel_emu_wait();
		try {
			checkStatus(true);
		} catch (const FlycastException& e) {
			WARN_LOG(COMMON, "%s", e.what());
			throw e;
		}
		nvmem::saveFiles();
		EventManager::event(Event::Pause);
	}
	else
	{
#ifdef __ANDROID__
		// defer stopping audio until after the current frame is finished
		// normally only useful on android due to multithreading
		stopRequested = true;
#else
		TermAudio();
		nvmem::saveFiles();
		EventManager::event(Event::Pause);
#endif
	}
}

// Called on the emulator thread for soft reset
void Emulator::requestReset()
{
	resetRequested = true;
	if (config::GGPOEnable)
		NetworkHandshake::term();
	getSh4Executor()->Stop();
}

void loadGameSpecificSettings()
{
	if (settings.platform.isConsole())
	{
		reios_disk_id();
		settings.content.gameId = trim_trailing_ws(std::string(ip_meta.product_number, sizeof(ip_meta.product_number)));
		// in case there is a null character followed by garbage, which happens
		settings.content.gameId = settings.content.gameId.c_str();

		if (settings.content.gameId.empty())
			return;
	}

	// Default per-game settings
	loadSpecialSettings();

	config::Settings::instance().setGameId(settings.content.gameId);
	custom_texture.init();

	// Reload per-game settings
	config::Settings::instance().load(true);

	if (config::GGPOEnable || settings.raHardcoreMode)
		config::Sh4Clock.override(200);
	if (settings.raHardcoreMode)
	{
		config::WidescreenGameHacks.override(false);
		config::DynarecEnabled.override(true);
	}
}

void Emulator::step()
{
	// FIXME single thread is better
	singleStep = true;
	start();
	stop();
}

void Emulator::stepRange(u32 from, u32 to)
{
	stepRangeFrom = from;
	stepRangeTo = to;
	start();
	stop();
}

void Emulator::loadstate(Deserializer& deser)
{
	if (!custom_texture.preloaded())
	{
		custom_texture.terminate();
		custom_texture.init();
	}
#if FEAT_AREC == DYNAREC_JIT
	aica::arm::recompiler::flush();
#endif
	mmu_flush_table();
#if FEAT_SHREC != DYNAREC_NONE
	bm_Reset();
#endif
	memwatch::unprotect();
	memwatch::reset();

	dc_deserialize(deser);

	mmu_set_state();
	getSh4Executor()->ResetCache();
	EventManager::event(Event::LoadState);
}

void Emulator::setNetworkState(bool online)
{
	if (settings.network.online != online)
	{
		settings.network.online = online;
		DEBUG_LOG(NETWORK, "Network state %d", online);
		if (online && settings.platform.isConsole()
				&& config::Sh4Clock != 200)
		{
			config::Sh4Clock.override(200);
			getSh4Executor()->ResetCache();
		}
		EventManager::event(Event::Network);
	}
	settings.input.fastForwardMode &= !online;
}

void EventManager::registerEvent(Event event, Callback callback, void *param)
{
	unregisterEvent(event, callback, param);
	auto& vector = callbacks[static_cast<size_t>(event)];
	vector.push_back(std::make_pair(callback, param));
}

void EventManager::unregisterEvent(Event event, Callback callback, void *param)
{
	auto& vector = callbacks[static_cast<size_t>(event)];
	auto it = std::find(vector.begin(), vector.end(), std::make_pair(callback, param));
	if (it != vector.end())
		vector.erase(it);
}

void EventManager::broadcastEvent(Event event)
{
	auto& vector = callbacks[static_cast<size_t>(event)];
	for (auto& pair : vector)
		pair.first(event, pair.second);
}

void Emulator::run()
{
	verify(state == Running);
	startTime = sh4_sched_now64();
	renderTimeout = false;
	if (!singleStep && stepRangeTo == 0)
	getSh4Executor()->Start();
	try {
		runInternal();
		if (ggpo::active())
			ggpo::nextFrame();
	} catch (const std::exception& e) {
		ERROR_LOG(COMMON, "Exception: %s", e.what());
		setNetworkState(false);
		state = Error;
		getSh4Executor()->Stop();
		EventManager::event(Event::Pause);
		throw;
	}
}

void Emulator::start()
{
	if (state == Running)
		return;
	if (state != Loaded) {
		WARN_LOG(COMMON, "Unexpected emu state %d", state);
		return;
	}
	state = Running;
	SetMemoryHandlers();
	if (config::GGPOEnable && config::ThreadedRendering)
		// Not supported with GGPO
		config::EmulateFramebuffer.override(false);
	setupPtyPipe();

	memwatch::protect();

	if (config::ThreadedRendering)
	{
		const std::lock_guard<std::mutex> lock(mutex);
		getSh4Executor()->Start();
		threadResult = std::async(std::launch::async, [this] {
				ThreadName _("Flycast-emu");
				InitAudio();

				try {
					while (state == Running || singleStep || stepRangeTo != 0)
					{
						startTime = sh4_sched_now64();
						renderTimeout = false;
						runInternal();
						if (!ggpo::nextFrame())
							break;
					}
					TermAudio();
				} catch (...) {
					setNetworkState(false);
					getSh4Executor()->Stop();
					TermAudio();
					throw;
				}
		});
	}
	else
	{
		stopRequested = false;
		InitAudio();
	}

	EventManager::event(Event::Resume);
}

bool Emulator::checkStatus(bool wait)
{
	try {
		std::unique_lock<std::mutex> lock(mutex);
		if (threadResult.valid())
		{
            auto localResult = threadResult;
			lock.unlock();
			if (wait) {
				localResult.wait();
			}
			else {
				auto result = localResult.wait_for(std::chrono::seconds(0));
				if (result == std::future_status::timeout)
					return true;
			}
			localResult.get();
		}
		return false;
	} catch (...) {
		EventManager::event(Event::Pause);
		state = Error;
		throw;
	}
}

bool Emulator::render()
{
	FC_PROFILE_SCOPE;

	if (!config::ThreadedRendering)
	{
		if (stopRequested)
		{
			stopRequested = false;
			TermAudio();
			nvmem::saveFiles();
			EventManager::event(Event::Pause);
			return false;
		}
		if (state != Running)
			return false;
		run();
		// TODO if stopping due to a user request, no frame has been rendered
		return !renderTimeout;
	}
	if (!checkStatus())
		return false;
	if (state != Running)
		return false;
	return rend_single_frame(true); // FIXME stop flag?
}

void Emulator::vblank()
{
	EventManager::event(Event::VBlank);
	runner.execTasks();
	// Time out if a frame hasn't been rendered for 50 ms
	if (sh4_sched_now64() - startTime <= 10000000)
		return;
	renderTimeout = true;
	if (ggpo::active())
		ggpo::endOfFrame();
	else if (!config::ThreadedRendering)
		getSh4Executor()->Stop();
}

bool Emulator::restartCpu()
{
	const std::lock_guard<std::mutex> _(mutex);
	if (state != Running)
		return false;
	getSh4Executor()->Start();
	return true;
}

void Emulator::insertGdrom(const std::string& path)
{
	if (settings.platform.isArcade())
		return;
	gdr::insertDisk(path);
	diskChange();
}

void Emulator::openGdrom()
{
	if (settings.platform.isArcade())
		return;
	gdr::openLid();
	diskChange();
}

void Emulator::diskChange()
{
	config::Settings::instance().reset();
	config::Settings::instance().load(false);
	custom_texture.terminate();
	if (!settings.content.path.empty())
	{
		hostfs::FileInfo info = hostfs::storage().getFileInfo(settings.content.path);
		settings.content.fileName = info.name;
		loadGameSpecificSettings();
	}
	else
	{
		settings.content.fileName.clear();
		settings.content.gameId.clear();
		settings.content.title = BIOS_TITLE;
	}
	cheatManager.reset(settings.content.gameId);
	if (cheatManager.isWidescreen())
		config::ScreenStretching.override(134);	// 4:3 -> 16:9
	EventManager::event(Event::DiskChange);
}

Emulator emu;
