// nullDC.cpp : Makes magic cookies
//
#include <atomic>
#include <future>
#include <thread>

#include "types.h"
#include "emulator.h"
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "hw/mem/_vmem.h"
#include "stdclass.h"
#include "cfg/cfg.h"
#include "cfg/option.h"

#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/holly/sb_mem.h"

#include "hw/naomi/naomi_cart.h"
#include "reios/reios.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_if.h"
#include "hw/pvr/spg.h"
#include "hw/aica/aica_if.h"
#include "hw/aica/dsp.h"
#include "hw/arm7/arm7_rec.h"
#include "imgread/common.h"
#include "rend/gui.h"
#include "profiler/profiler.h"
#include "input/gamepad_device.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "log/LogManager.h"
#include "cheats.h"
#include "rend/CustomTexture.h"
#include "hw/maple/maple_devs.h"
#include "network/naomi_network.h"
#include "rend/mainui.h"
#include "archive/rzip.h"
#include "debug/gdb_server.h"

settings_t settings;

cThread emu_thread(&dc_run, NULL);

static std::future<void> loading_done;
std::atomic<bool> loading_canceled;
static bool init_done;

static s32 plugins_Init()
{

	if (s32 rv = libPvr_Init())
		return rv;

#ifndef TARGET_DISPFRAME
	if (s32 rv = libGDR_Init())
		return rv;
#endif

	if (s32 rv = libAICA_Init())
		return rv;

	if (s32 rv = libARM_Init())
		return rv;

	return 0;
}

static void plugins_Term()
{
	//term all plugins
	libARM_Term();
	libAICA_Term();
	libGDR_Term();
	libPvr_Term();
}

static void plugins_Reset(bool hard)
{
	libPvr_Reset(hard);
	libGDR_Reset(hard);
	libAICA_Reset(hard);
	libARM_Reset(hard);
	//libExtDevice_Reset(Manual);
}

static void LoadSpecialSettings()
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
				|| prod_id == "T-45001D05")
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
		// Super Producers
		if (prod_id == "T14303M"
			// Giant Killers
			|| prod_id == "T45401D 50"
			// Wild Metal (US)
			|| prod_id == "T42101N 00"
			// Wild Metal (EU)
			|| prod_id == "T40501D-50"
			// Resident Evil 2 (US)
			|| prod_id == "T1205N"
			// Resident Evil 2 (EU)
			|| prod_id == "T7004D  50"
			// Rune Jade
			|| prod_id == "T14304M"
			// Marionette Company
			|| prod_id == "T5202M"
			// Marionette Company 2
			|| prod_id == "T5203M"
			// Maximum Pool (for online support)
			|| prod_id == "T11010N"
			// StarLancer (US) (for online support)
			|| prod_id == "T40209N"
			// StarLancer (EU) (for online support)
			|| prod_id == "T17723D 05"
			// Heroes of might and magic III
			|| prod_id == "T0000M"
			// WebTV
			|| prod_id == "6107117" || prod_id == "610-7390" || prod_id == "610-7391"
			// PBA
			|| prod_id == "T26702N")
		{
			INFO_LOG(BOOT, "Disabling 32-bit virtual memory for game %s", prod_id.c_str());
			config::DisableVmem32.override(true);
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
	plugins_Reset(hard);
	sh4_cpu.Reset(hard);
	mem_Reset(hard);
}

static bool reset_requested;
static bool singleStep;

int reicast_init(int argc, char* argv[])
{
#if defined(TEST_AUTOMATION)
	setbuf(stdout, 0);
	setbuf(stderr, 0);
#endif
	if (!_vmem_reserve())
	{
		ERROR_LOG(VMEM, "Failed to alloc mem");
		return -1;
	}
	if (ParseCommandLine(argc, argv))
	{
        return 69;
	}
	config::Settings::instance().reset();
	LogManager::Shutdown();
	if (!cfgOpen())
	{
		LogManager::Init();
		NOTICE_LOG(BOOT, "Config directory is not set. Starting onboarding");
		gui_open_onboarding();
	}
	else
	{
		LogManager::Init();
		config::Settings::instance().load(false);
	}
	// Force the renderer type now since we're not switching
	config::RendererType.commit();

	os_CreateWindow();
	os_SetupInput();

	// Needed to avoid crash calling dc_is_running() in gui
	Get_Sh4Interpreter(&sh4_cpu);
	sh4_cpu.Init();
	debugger::init();

	return 0;
}

static void set_platform(int platform)
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

void dc_init()
{
	if (init_done)
		return;

	// Default platform
	set_platform(DC_PLATFORM_DREAMCAST);

	plugins_Init();
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

	init_done = true;
}

static int get_game_platform(const char *path)
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

static void dc_start_game(const char *path)
{
	DEBUG_LOG(BOOT, "Loading game %s", path == nullptr ? "(nil)" : path);

	if (path != nullptr)
		strcpy(settings.imgread.ImagePath, path);
	else
		settings.imgread.ImagePath[0] = '\0';

	dc_init();

	set_platform(get_game_platform(path));
	mem_map_default();

	config::Settings::instance().reset();
	dc_reset(true);
	config::Settings::instance().load(false);
	
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		if (path == NULL)
		{
			// Boot BIOS
			if (!LoadRomFiles())
				throw ReicastException("No BIOS file found");
			TermDrive();
			InitDrive();
		}
		else
		{
			std::string extension = get_file_extension(settings.imgread.ImagePath);
			if (extension != "elf")
			{
				if (InitDrive())
				{
					LoadGameSpecificSettings();
					if (config::UseReios || !LoadRomFiles())
					{
						LoadHle();
						NOTICE_LOG(BOOT, "Did not load BIOS, using reios");
					}
				}
				else
				{
					// Content load failed. Boot the BIOS
					settings.imgread.ImagePath[0] = '\0';
					if (!LoadRomFiles())
						throw ReicastException("This media cannot be loaded");
					InitDrive();
				}
			}
			else
			{
				// Elf only supported with HLE BIOS
				LoadHle();
			}
		}
		mcfg_CreateDevices();
		FixUpFlash();
	}
	else if (settings.platform.system == DC_PLATFORM_NAOMI || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		LoadRomFiles();
		naomi_cart_LoadRom(path);
		if (loading_canceled)
			return;
		LoadGameSpecificSettings();
		// Reload the BIOS in case a game-specific region is set
		naomi_cart_LoadBios(path);
		if (settings.platform.system == DC_PLATFORM_NAOMI)
		{
			mcfg_CreateNAOMIJamma();
			SetNaomiNetworkConfig(-1);
		}
		else if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
			mcfg_CreateAtomisWaveControllers();
	}
	if (cheatManager.Reset())
	{
		gui_display_notification("Widescreen cheat activated", 1000);
		config::ScreenStretching.override(134);	// 4:3 -> 16:9
	}
	settings.input.fastForwardMode = false;
	EventManager::event(Event::Start);
	settings.gameStarted = true;
}

bool dc_is_running()
{
	return sh4_cpu.IsCpuRunning();
}

#ifndef TARGET_DISPFRAME
void* dc_run(void*)
{
	InitAudio();

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
	if (singleStep)
	{
		singleStep = false;
		sh4_cpu.Step();
	}
	else
	{
		do {
			reset_requested = false;

			sh4_cpu.Run();

			SaveRomFiles();

			if (reset_requested)
				dc_reset(false);
		} while (reset_requested);
	}

    TermAudio();

    return NULL;
}
#endif

void dc_term_game()
{
	if (settings.gameStarted)
	{
		settings.gameStarted = false;
		EventManager::event(Event::Terminate);
	}
	if (init_done)
		dc_reset(true);

	config::Settings::instance().reset();
	config::Settings::instance().load(false);
}

void dc_term()
{
	dc_term_game();
	debugger::term();
	dc_cancel_load();
	sh4_cpu.Term();
	if (settings.platform.system != DC_PLATFORM_DREAMCAST)
		naomi_cart_Close();
	plugins_Term();
	mem_Term();
	_vmem_release();

	mcfg_DestroyDevices();

	SaveSettings();
}

void dc_stop()
{
	bool running = dc_is_running();
	sh4_cpu.Stop();
	rend_cancel_emu_wait();
	emu_thread.WaitToEnd();
	if (running)
		EventManager::event(Event::Pause);
}

// Called on the emulator thread for soft reset
void dc_request_reset()
{
	reset_requested = true;
	sh4_cpu.Stop();
}

void dc_exit()
{
	dc_stop();
	mainui_stop();
}

void LoadGameSpecificSettings()
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
	LoadSpecialSettings();

	config::Settings::instance().setGameId(reios_id);

	// Reload per-game settings
	config::Settings::instance().load(true);
}

void SaveSettings()
{
	config::Settings::instance().save();
	GamepadDevice::SaveMaplePorts();

#ifdef __ANDROID__
	void SaveAndroidSettings();
	SaveAndroidSettings();
#endif
}

void dc_resume()
{
	SetMemoryHandlers();
	settings.aica.NoBatch = config::ForceWindowsCE || config::DSPEnabled;
	int hres;
	int vres = config::RenderResolution;
	if (config::Widescreen && !config::Rotate90)
	{
		hres = config::RenderResolution * 16 / 9;
	}
	else if (config::Rotate90)
	{
		vres = vres * config::ScreenStretching / 100;
		hres = config::RenderResolution * 4 / 3;
	}
	else
	{
		hres = config::RenderResolution * 4 * config::ScreenStretching / 3 / 100;
	}
	if (renderer != nullptr)
		renderer->Resize(hres, vres);

	EventManager::event(Event::Resume);
	if (!emu_thread.thread.joinable())
		emu_thread.Start();
}

void dc_step()
{
	singleStep = true;
	dc_resume();
	dc_stop();
}

static void cleanup_serialize(void *data)
{
	free(data);
}

static std::string get_savestate_file_path(bool writable)
{
	std::string state_file = settings.imgread.ImagePath;
	size_t lastindex = state_file.find_last_of('/');
#ifdef _WIN32
	size_t lastindex2 = state_file.find_last_of('\\');
	if (lastindex == std::string::npos)
		lastindex = lastindex2;
	else if (lastindex2 != std::string::npos)
		lastindex = std::max(lastindex, lastindex2);
#endif
	if (lastindex != std::string::npos)
		state_file = state_file.substr(lastindex + 1);
	lastindex = state_file.find_last_of('.');
	if (lastindex != std::string::npos)
		state_file = state_file.substr(0, lastindex);
	state_file = state_file + ".state";
	if (writable)
		return get_writable_data_path(state_file);
	else
		return get_readonly_data_path(state_file);
}

void dc_savestate()
{
	unsigned int total_size = 0 ;
	void *data = NULL ;

	if ( ! dc_serialize(&data, &total_size) )
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not initialize total size") ;
		gui_display_notification("Save state failed", 2000);
		cleanup_serialize(data) ;
    	return;
	}

	data = malloc(total_size) ;
	if ( data == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not malloc %d bytes", total_size) ;
		gui_display_notification("Save state failed - memory full", 2000);
		cleanup_serialize(data) ;
    	return;
	}

	void *data_ptr = data;

	if ( ! dc_serialize(&data_ptr, &total_size) )
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not serialize data") ;
		gui_display_notification("Save state failed", 2000);
		cleanup_serialize(data) ;
    	return;
	}

	std::string filename = get_savestate_file_path(true);
#if 0
	FILE *f = nowide::fopen(filename.c_str(), "wb") ;

	if ( f == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str()) ;
		gui_display_notification("Cannot open save file", 2000);
		cleanup_serialize(data) ;
    	return;
	}

	std::fwrite(data, 1, total_size, f) ;
	std::fclose(f);
#else
	RZipFile zipFile;
	if (!zipFile.Open(filename, true))
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str());
		gui_display_notification("Cannot open save file", 2000);
		cleanup_serialize(data);
    	return;
	}
	if (zipFile.Write(data, total_size) != total_size)
	{
		WARN_LOG(SAVESTATE, "Failed to save state - error writing %s", filename.c_str());
		gui_display_notification("Error saving state", 2000);
		zipFile.Close();
		cleanup_serialize(data);
    	return;
	}
	zipFile.Close();
#endif

	cleanup_serialize(data) ;
	INFO_LOG(SAVESTATE, "Saved state to %s size %d", filename.c_str(), total_size) ;
	gui_display_notification("State saved", 1000);
}

void dc_loadstate()
{
	u32 total_size = 0;
	FILE *f = nullptr;

	dc_stop();

	std::string filename = get_savestate_file_path(false);
	RZipFile zipFile;
	if (zipFile.Open(filename, false))
	{
		total_size = (u32)zipFile.Size();
	}
	else
	{
		f = nowide::fopen(filename.c_str(), "rb") ;

		if ( f == NULL )
		{
			WARN_LOG(SAVESTATE, "Failed to load state - could not open %s for reading", filename.c_str()) ;
			gui_display_notification("Save state not found", 2000);
			return;
		}
		std::fseek(f, 0, SEEK_END);
		total_size = (u32)std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
	}
	void *data = malloc(total_size);
	if ( data == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to load state - could not malloc %d bytes", total_size) ;
		gui_display_notification("Failed to load state - memory full", 2000);
		if (f != nullptr)
			std::fclose(f);
		else
			zipFile.Close();
		return;
	}

	size_t read_size;
	if (f == nullptr)
	{
		read_size = zipFile.Read(data, total_size);
		zipFile.Close();
	}
	else
	{
		read_size = fread(data, 1, total_size, f) ;
		std::fclose(f);
	}
	if (read_size != total_size)
	{
		WARN_LOG(SAVESTATE, "Failed to load state - I/O error");
		gui_display_notification("Failed to load state - I/O error", 2000);
		cleanup_serialize(data) ;
		return;
	}

	void *data_ptr = data;

	custom_texture.Terminate();
#if FEAT_AREC == DYNAREC_JIT
	aicaarm::recompiler::flush();
#endif
#ifndef NO_MMU
    mmu_flush_table();
#endif
#if FEAT_SHREC != DYNAREC_NONE
	bm_Reset();
#endif

	u32 unserialized_size = 0;
	if ( ! dc_unserialize(&data_ptr, &unserialized_size) )
	{
		WARN_LOG(SAVESTATE, "Failed to load state - could not unserialize data") ;
		gui_display_notification("Invalid save state", 2000);
		cleanup_serialize(data) ;
    	return;
	}
	if (unserialized_size != total_size)
		WARN_LOG(SAVESTATE, "Save state error: read %d bytes but used %d", total_size, unserialized_size);

	mmu_set_state();
	sh4_cpu.ResetCache();
    dsp.dyndirty = true;
    sh4_sched_ffts();

    cleanup_serialize(data) ;
	EventManager::event(Event::LoadState);
    INFO_LOG(SAVESTATE, "Loaded state from %s size %d", filename.c_str(), total_size) ;
}

void dc_load_game(const char *path)
{
	loading_canceled = false;

	loading_done = std::async(std::launch::async, [path] {
		dc_start_game(path);
	});
}

bool dc_is_load_done()
{
	if (!loading_done.valid())
		return true;
	if (loading_done.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
		return true;
	return false;
}

void dc_cancel_load()
{
	if (loading_done.valid())
	{
		loading_canceled = true;
		loading_done.get();
	}
	settings.imgread.ImagePath[0] = '\0';
}

void dc_get_load_status()
{
	if (loading_done.valid())
		loading_done.get();
}

EventManager EventManager::Instance;

void EventManager::registerEvent(Event event, Callback callback)
{
	unregisterEvent(event, callback);
	auto it = callbacks.find(event);
	if (it != callbacks.end())
		it->second.push_back(callback);
	else
		callbacks.insert({ event, { callback } });
}

void EventManager::unregisterEvent(Event event, Callback callback) {
	auto it = callbacks.find(event);
	if (it == callbacks.end())
		return;

	auto it2 = std::find(it->second.begin(), it->second.end(), callback);
	if (it2 == it->second.end())
		return;

	it->second.erase(it2);
}

void EventManager::broadcastEvent(Event event) {
	auto it = callbacks.find(event);
	if (it == callbacks.end())
		return;

	for (auto& callback : it->second)
		callback(event);
}
