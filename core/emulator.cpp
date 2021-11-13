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
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/holly/sb_mem.h"
#include "cheats.h"
#include "oslib/audiostream.h"
#include "debug/gdb_server.h"
#include "hw/pvr/Renderer_if.h"
#include "rend/CustomTexture.h"
#include "hw/arm7/arm7_rec.h"
#include "network/ggpo.h"
#include "hw/mem/mem_watch.h"
#include "network/net_handshake.h"
#include "rend/gui.h"
#include "lua/lua.h"
#include "network/naomi_network.h"
#include "serialize.h"
#include "hw/pvr/pvr.h"
#include <chrono>

settings_t settings;

static void loadSpecialSettings()
{
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		std::string prod_id(ip_meta.product_number, sizeof(ip_meta.product_number));
		prod_id = trim_trailing_ws(prod_id);

		NOTICE_LOG(BOOT, "Game ID is [%s]", prod_id.c_str());

		if (ip_meta.isWindowsCE() || config::ForceWindowsCE
				|| prod_id == "T26702N") // PBA Tour Bowling 2001
		{
			INFO_LOG(BOOT, "Enabling Full MMU and Extra depth scaling for Windows CE game");
			config::ExtraDepthScale.override(0.1); // taxi 2 needs 0.01 for FMV (amd, per-tri)
			config::FullMMU.override(true);
			if (!config::ForceWindowsCE)
				config::ForceWindowsCE.override(true);
		}

		// Tony Hawk's Pro Skater 2
		if (prod_id == "T13008D" || prod_id == "T13006N"
				// Tony Hawk's Pro Skater 1
				|| prod_id == "T40205N"
				// Tony Hawk's Skateboarding
				|| prod_id == "T40204D"
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
				|| prod_id == "MK-5105850")
		{
			INFO_LOG(BOOT, "Enabling render to texture buffer for game %s", prod_id.c_str());
			config::RenderToTextureBuffer.override(true);
		}
		if (prod_id == "HDR-0176" || prod_id == "RDC-0057")
		{
			INFO_LOG(BOOT, "Enabling translucent depth multipass for game %s", prod_id.c_str());
			// Cosmic Smash
			config::TranslucentPolygonDepthMask.override(true);
		}
		// NHL 2K2
		if (prod_id == "MK-51182")
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(1000000.f);	// Mali needs 1M, 10K is enough for others
		}
		// Re-Volt (US, EU)
		else if (prod_id == "T-8109N" || prod_id == "T8107D  50")
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(100.f);
		}
		// Samurai Shodown 6 dc port
		else if (prod_id == "T0002M")
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id.c_str());
			config::ExtraDepthScale.override(1e26f);
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
		if (config::Cable <= 1 && !ip_meta.supportsVGA())
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
				|| prod_id == "T1235M")) // Vampire Chronicle for Matching Service
		{
			NOTICE_LOG(BOOT, "Game doesn't support RGB. Using TV Composite instead");
			config::Cable.override(3);
		}
		if (prod_id == "T9512N"			// The Grinch (US)
			|| prod_id == "T9503D"		// The Grinch (EU)
			|| prod_id == "T0000M"		// Hell Gate FIXME
			|| prod_id == "MK-51012"	// Metropolis Street Racer (US)
			|| prod_id == "MK-5102250"	// Metropolis Street Racer (EU)
			|| prod_id == "T-31101N"	// Psychic Force 2012 (US)
			|| prod_id == "T1101M"		// Psychic Force 2012 (JP)
			|| prod_id == "T-8106D-50"	// Psychic Force 2012 (EU)
			|| prod_id == "T-9707N"		// San Francisco Rush 2049 (US)
			|| prod_id == "T-9709D-50"	// San Francisco Rush 2049 (EU)
			|| prod_id == "MK-51146"	// Sega Smashpack vol.1 (Sega Swirl)
			|| prod_id == "MK-51152"	// World Series Baseball 2K2
			|| prod_id == "T20401M"		// Zero Gunner
			|| prod_id == "12502D-50"	// Caesar's palace 2000 (EU)
			|| prod_id == "T7001D  50"	// Jimmy White's 2 Cueball
			|| prod_id == "T17717D 50"	// The Next Tetris (EU)
			|| prod_id == "T40506D 50"	// KISS (EU)
			|| prod_id == "T40505D 50"	// Railroad Tycoon 2 (EU)
			|| prod_id == "T18702M"		// Miss Moonlight
			|| prod_id == "T0019M")		// KenJu Atomiswave DC Conversion
		{
			NOTICE_LOG(BOOT, "Forcing real BIOS");
			config::UseReios.override(false);
		}
	}
	else if (settings.platform.system == DC_PLATFORM_NAOMI || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		NOTICE_LOG(BOOT, "Game ID is [%s]", naomi_game_id);
		if (!strcmp("SAMURAI SPIRITS 6", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", naomi_game_id);
			config::ExtraDepthScale.override(1e26f);
		}
		if (!strcmp("COSMIC SMASH IN JAPAN", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling translucent depth multipass for game %s", naomi_game_id);
			config::TranslucentPolygonDepthMask.override(true);
		}
		// Input configuration
		settings.input.JammaSetup = JVS::Default;
		if (!strcmp("DYNAMIC GOLF", naomi_game_id)
				|| !strcmp("SHOOTOUT POOL", naomi_game_id)
				|| !strcmp("SHOOTOUT POOL MEDAL", naomi_game_id)
				|| !strcmp("CRACKIN'DJ  ver JAPAN", naomi_game_id)
				|| !strcmp("CRACKIN'DJ PART2  ver JAPAN", naomi_game_id)
				|| !strcmp("KICK '4' CASH", naomi_game_id)
				|| !strcmp("DRIVE", naomi_game_id))			// Waiwai drive
		{
			INFO_LOG(BOOT, "Enabling JVS rotary encoders for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::RotaryEncoders;
		}
		else if (!strcmp("POWER STONE 2 JAPAN", naomi_game_id)		// Naomi
				|| !strcmp("GUILTY GEAR isuka", naomi_game_id))		// AW
		{
			INFO_LOG(BOOT, "Enabling 4-player setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::FourPlayers;
		}
		else if (!strcmp("SEGA MARINE FISHING JAPAN", naomi_game_id)
					|| !strcmp(naomi_game_id, "BASS FISHING SIMULATOR VER.A"))	// AW
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::SegaMarineFishing;
		}
		else if (!strcmp("RINGOUT 4X4 JAPAN", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::DualIOBoards4P;
		}
		else if (!strcmp("NINJA ASSAULT", naomi_game_id)
					|| !strcmp(naomi_game_id, "Sports Shooting USA")	// AW
					|| !strcmp(naomi_game_id, "SEGA CLAY CHALLENGE")	// AW
					|| !strcmp(naomi_game_id, "RANGER MISSION")			// AW
					|| !strcmp(naomi_game_id, "EXTREME HUNTING"))		// AW
		{
			INFO_LOG(BOOT, "Enabling lightgun setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::LightGun;
		}
		else if (!strcmp("MAZAN", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::Mazan;
		}
		else if (!strcmp(" BIOHAZARD  GUN SURVIVOR2", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::GunSurvivor;
		}
		else if (!strcmp("WORLD KICKS", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::WorldKicks;
		}
		else if (!strcmp("WORLD KICKS PCB", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::WorldKicksPCB;
		}
		else if (!strcmp("THE TYPING OF THE DEAD", naomi_game_id)
				|| !strcmp(" LUPIN THE THIRD  -THE TYPING-", naomi_game_id)
				|| !strcmp("------La Keyboardxyu------", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling keyboard for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::Keyboard;
		}
		else if (!strcmp("OUTTRIGGER     JAPAN", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling JVS rotary encoders for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::OutTrigger;
		}
		else if (!strcmp(naomi_game_id, "THE MAZE OF THE KINGS")
				|| !strcmp(naomi_game_id, " CONFIDENTIAL MISSION ---------")
				|| !strcmp(naomi_game_id, "DEATH CRIMSON OX")
				|| !strncmp(naomi_game_id, "hotd2", 5)	// House of the Dead 2
				|| !strcmp(naomi_game_id, "LUPIN THE THIRD  -THE SHOOTING-"))
		{
			INFO_LOG(BOOT, "Enabling lightgun as analog setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::LightGunAsAnalog;
		}
		else if (!strcmp("WAVE RUNNER GP", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::WaveRunnerGP;
		}
		else if (!strcmp("INU NO OSANPO", naomi_game_id))	// Dog Walking
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::DogWalking;
		}
		else if (!strcmp(" TOUCH DE UNOH -------------", naomi_game_id)
				|| !strcmp("POKASUKA GHOST (JAPANESE)", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling specific JVS setup for game %s", naomi_game_id);
			settings.input.JammaSetup = JVS::TouchDeUno;
		}
	}
}

void dc_reset(bool hard)
{
	NetworkHandshake::term();
	if (hard)
		_vmem_unprotect_vram(0, VRAM_SIZE);
	sh4_sched_reset(hard);
	pvr::reset(hard);
	libAICA_Reset(hard);
	libARM_Reset(hard);
	sh4_cpu.Reset(true);
	mem_Reset(hard);
}

static void setPlatform(int platform)
{
	if (VRAM_SIZE != 0)
		_vmem_unprotect_vram(0, VRAM_SIZE);
	switch (platform)
	{
	case DC_PLATFORM_DREAMCAST:
		settings.platform.ram_size = 16 * 1024 * 1024;
		settings.platform.vram_size = 8 * 1024 * 1024;
		settings.platform.aram_size = 2 * 1024 * 1024;
		settings.platform.bios_size = 2 * 1024 * 1024;
		settings.platform.flash_size = 128 * 1024;
		break;
	case DC_PLATFORM_NAOMI:
		settings.platform.ram_size = 32 * 1024 * 1024;
		settings.platform.vram_size = 16 * 1024 * 1024;
		settings.platform.aram_size = 8 * 1024 * 1024;
		settings.platform.bios_size = 2 * 1024 * 1024;
		settings.platform.flash_size = 32 * 1024;	// battery-backed ram
		break;
	case DC_PLATFORM_ATOMISWAVE:
		settings.platform.ram_size = 16 * 1024 * 1024;
		settings.platform.vram_size = 8 * 1024 * 1024;
		settings.platform.aram_size = 8 * 1024 * 1024;
		settings.platform.bios_size = 128 * 1024;
		settings.platform.flash_size = 128 * 1024;	// sram
		break;
	default:
		die("Unsupported platform");
		break;
	}
	settings.platform.system = platform;
	settings.platform.ram_mask = settings.platform.ram_size - 1;
	settings.platform.vram_mask = settings.platform.vram_size - 1;
	settings.platform.aram_mask = settings.platform.aram_size - 1;
	_vmem_init_mappings();
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

	pvr::init();
	libAICA_Init();
	libARM_Init();
	mem_Init();
	reios_init();

	// the recompiler may start generating code at this point and needs a fully configured machine
#if FEAT_SHREC != DYNAREC_NONE
	Get_Sh4Recompiler(&sh4_cpu);
	sh4_cpu.Init();		// Also initialize the interpreter
	if(config::DynarecEnabled)
	{
		INFO_LOG(DYNAREC, "Using Recompiler");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		sh4_cpu.Init();
		INFO_LOG(INTERPRETER, "Using Interpreter");
	}
	state = Init;
}

static int getGamePlatform(const char *path)
{
	if (path == NULL)
		// Dreamcast BIOS
		return DC_PLATFORM_DREAMCAST;

	std::string extension = get_file_extension(path);
	if (extension.empty())
		return DC_PLATFORM_DREAMCAST;	// unknown
	if (extension == "zip" || extension == "7z")
		return naomi_cart_GetPlatform(path);
	if (extension == "bin" || extension == "dat" || extension == "lst")
		return DC_PLATFORM_NAOMI;

	return DC_PLATFORM_DREAMCAST;
}

void Emulator::loadGame(const char *path, LoadProgress *progress)
{
	init();
	try {
		DEBUG_LOG(BOOT, "Loading game %s", path == nullptr ? "(nil)" : path);

		if (path != nullptr)
			settings.content.path = path;
		else
			settings.content.path.clear();

		setPlatform(getGamePlatform(path));
		mem_map_default();

		config::Settings::instance().reset();
		dc_reset(true);
		config::Settings::instance().load(false);
		memset(&settings.network.md5, 0, sizeof(settings.network.md5));

		if (settings.platform.system == DC_PLATFORM_DREAMCAST)
		{
			if (settings.content.path.empty())
			{
				// Boot BIOS
				if (!LoadRomFiles())
					throw FlycastException("No BIOS file found in " + hostfs::getFlashSavePath("", ""));
				InitDrive("");
			}
			else
			{
				std::string extension = get_file_extension(settings.content.path);
				if (extension != "elf")
				{
					if (InitDrive(settings.content.path))
					{
						loadGameSpecificSettings();
						if (config::UseReios || !LoadRomFiles())
						{
							LoadHle();
							NOTICE_LOG(BOOT, "Did not load BIOS, using reios");
							if (!config::UseReios && config::UseReios.isReadOnly())
								gui_display_notification("This game requires a real BIOS", 15000);
						}
					}
					else
					{
						// Content load failed. Boot the BIOS
						settings.content.path.clear();
						if (!LoadRomFiles())
							throw FlycastException("This media cannot be loaded");
						InitDrive("");
					}
				}
				else
				{
					// Elf only supported with HLE BIOS
					LoadHle();
				}
			}
		}
		else if (settings.platform.system == DC_PLATFORM_NAOMI || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		{
			LoadRomFiles();
			naomi_cart_LoadRom(path, progress);
			loadGameSpecificSettings();
			// Reload the BIOS in case a game-specific region is set
			naomi_cart_LoadBios(path);
		}
		mcfg_CreateDevices();
		cheatManager.reset(settings.content.gameId);
		if (cheatManager.isWidescreen())
		{
			gui_display_notification("Widescreen cheat activated", 1000);
			config::ScreenStretching.override(134);	// 4:3 -> 16:9
		}
		NetworkHandshake::init();
		settings.input.fastForwardMode = false;
		if (!settings.content.path.empty())
		{
			if (config::GGPOEnable)
				dc_loadstate(-1);
			else if (config::AutoLoadState && !NaomiNetworkSupported())
				dc_loadstate(config::SavestateSlot);
		}
		EventManager::event(Event::Start);
		state = Loaded;
	} catch (...) {
		state = Error;
		throw;
	}
}

void Emulator::runInternal()
{
	if (singleStep)
	{
		singleStep = false;
		sh4_cpu.Step();
	}
	else
	{
		do {
			resetRequested = false;

			sh4_cpu.Run();

			if (resetRequested)
			{
				SaveRomFiles();
				dc_reset(false);
			}
		} while (resetRequested);
	}
}

void Emulator::unloadGame()
{
	stop();
	if (state == Loaded || state == Error)
	{
		if (state == Loaded && config::AutoSaveState && !settings.content.path.empty())
			dc_savestate(config::SavestateSlot);
		dc_reset(true);

		config::Settings::instance().reset();
		config::Settings::instance().load(false);
		settings.content.path.clear();
		settings.content.gameId.clear();
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
		sh4_cpu.Term();
		custom_texture.Terminate();	// lr: avoid deadlock on exit (win32)
		reios_term();
		libARM_Term();
		libAICA_Term();
		pvr::term();
		mem_Term();

		_vmem_release();
		state = Terminated;
	}
}

void Emulator::stop() {
	if (state != Running)
		return;
	state = Loaded;
	sh4_cpu.Stop();
	if (config::ThreadedRendering)
	{
		rend_cancel_emu_wait();
		try {
			auto future = threadResult;
			future.get();
		} catch (const FlycastException& e) {
			WARN_LOG(COMMON, "%s", e.what());
		}
	}
	else
	{
		// FIXME Android: need to terminate render thread before
		TermAudio();
	}
	SaveRomFiles();
	EventManager::event(Event::Pause);
}

// Called on the emulator thread for soft reset
void Emulator::requestReset()
{
	resetRequested = true;
	sh4_cpu.Stop();
}

void loadGameSpecificSettings()
{
	char *reios_id;
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		static char _disk_id[sizeof(ip_meta.product_number) + 1];

		reios_disk_id();
		memcpy(_disk_id, ip_meta.product_number, sizeof(ip_meta.product_number));
		reios_id = _disk_id;

		char *p = reios_id + strlen(reios_id) - 1;
		while (p >= reios_id && *p == ' ')
			*p-- = '\0';
		if (*p == '\0')
			return;
	}
	else
	{
		reios_id = naomi_game_id;
	}

	// Default per-game settings
	loadSpecialSettings();

	settings.content.gameId = reios_id;
	config::Settings::instance().setGameId(reios_id);

	// Reload per-game settings
	config::Settings::instance().load(true);
}

void Emulator::step()
{
	// FIXME single thread is better
	singleStep = true;
	start();
	stop();
}

void dc_loadstate(Deserializer& deser)
{
	custom_texture.Terminate();
#if FEAT_AREC == DYNAREC_JIT
	aicaarm::recompiler::flush();
#endif
	mmu_flush_table();
#if FEAT_SHREC != DYNAREC_NONE
	bm_Reset();
#endif

	dc_deserialize(deser);

	mmu_set_state();
	sh4_cpu.ResetCache();
}

void Emulator::setNetworkState(bool online)
{
	if (settings.network.online != online)
		DEBUG_LOG(NETWORK, "Network state %d", online);
	settings.network.online = online;
	settings.input.fastForwardMode &= !online;
}

EventManager EventManager::Instance;

void EventManager::registerEvent(Event event, Callback callback, void *param)
{
	unregisterEvent(event, callback, param);
	auto it = callbacks.find(event);
	if (it != callbacks.end())
		it->second.push_back(std::make_pair(callback, param));
	else
		callbacks.insert({ event, { std::make_pair(callback, param) } });
}

void EventManager::unregisterEvent(Event event, Callback callback, void *param) {
	auto it = callbacks.find(event);
	if (it == callbacks.end())
		return;

	auto it2 = std::find(it->second.begin(), it->second.end(), std::make_pair(callback, param));
	if (it2 == it->second.end())
		return;

	it->second.erase(it2);
}

void EventManager::broadcastEvent(Event event) {
	auto it = callbacks.find(event);
	if (it == callbacks.end())
		return;

	for (auto& pair : it->second)
		pair.first(event, pair.second);
}

void Emulator::run()
{
	verify(state == Running);
	startTime = sh4_sched_now64();
	renderTimeout = false;
	try {
		runInternal();
		if (ggpo::active())
			ggpo::nextFrame();
	} catch (...) {
		setNetworkState(false);
		state = Error;
		sh4_cpu.Stop();
		EventManager::event(Event::Pause);
		throw;
	}
}

void Emulator::start()
{
	verify(state == Loaded);
	state = Running;
	SetMemoryHandlers();
	settings.aica.NoBatch = config::ForceWindowsCE || config::DSPEnabled || config::GGPOEnable;
	rend_resize_renderer();
#if FEAT_SHREC != DYNAREC_NONE
	if (config::DynarecEnabled)
	{
		Get_Sh4Recompiler(&sh4_cpu);
		INFO_LOG(DYNAREC, "Using Recompiler");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		INFO_LOG(DYNAREC, "Using Interpreter");
	}
	EventManager::event(Event::Resume);
	memwatch::protect();

	if (config::ThreadedRendering)
	{
		threadResult = std::async(std::launch::async, [this] {
				InitAudio();

				try {
					while (state == Running)
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
					state = Error;
					sh4_cpu.Stop();
					TermAudio();
					throw;
				}
		}).share();
	}
	else
	{
		InitAudio();
	}
}

bool Emulator::checkStatus()
{
	try {
		if (threadResult.wait_for(std::chrono::seconds(0)) == std::future_status::timeout)
			return true;
		threadResult.get();
		return false;
	} catch (...) {
		EventManager::event(Event::Pause);
		throw;
	}
}

bool Emulator::render()
{
	if (!config::ThreadedRendering)
	{
		if (state != Running)
			return false;
		run();
		// TODO if stopping due to a user request, no frame has been rendered
		return !renderTimeout;
	}
	if (!checkStatus())
		return false;
	return rend_single_frame(true); // FIXME stop flag?
}

void Emulator::vblank()
{
	lua::vblank();
	// Time out if a frame hasn't been rendered for 50 ms
	if (sh4_sched_now64() - startTime <= 10000000)
		return;
	renderTimeout = true;
	if (ggpo::active())
		ggpo::endOfFrame();
	else if (!config::ThreadedRendering)
		sh4_cpu.Stop();
}

Emulator emu;
