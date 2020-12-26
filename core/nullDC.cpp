// nullDC.cpp : Makes magic cookies
//
#include <atomic>
#include <future>
#include <thread>

//initialse Emu
#include "types.h"
#include "emulator.h"
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "hw/mem/_vmem.h"
#include "stdclass.h"
#include "cfg/cfg.h"

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

void FlushCache();
static void LoadCustom();

extern bool fast_forward_mode;

settings_t settings;
// Set if game has corresponding option by default, so that it's not saved in the config
static bool rtt_to_buffer_game;
static bool safemode_game;
static bool tr_poly_depth_mask_game;
static bool extra_depth_game;
static bool disable_vmem32_game;
static int forced_game_region = -1;
static int forced_game_cable = -1;
static int saved_screen_stretching = -1;

cThread emu_thread(&dc_run, NULL);

static std::future<void> loading_done;
std::atomic<bool> loading_canceled;

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
		char prod_id[sizeof(ip_meta.product_number) + 1] = {0};
		memcpy(prod_id, ip_meta.product_number, sizeof(ip_meta.product_number));

		NOTICE_LOG(BOOT, "Game ID is [%s]", prod_id);
		rtt_to_buffer_game = false;
		safemode_game = false;
		tr_poly_depth_mask_game = false;
		extra_depth_game = false;
		disable_vmem32_game = false;
		forced_game_region = -1;
		forced_game_cable = -1;

		if (ip_meta.isWindowsCE() || settings.dreamcast.ForceWindowsCE
				|| !strncmp("T26702N", prod_id, 7)) // PBA Tour Bowling 2001
		{
			INFO_LOG(BOOT, "Enabling Full MMU and Extra depth scaling for Windows CE game");
			settings.rend.ExtraDepthScale = 0.1; // taxi 2 needs 0.01 for FMV (amd, per-tri)
			extra_depth_game = true;
			settings.dreamcast.FullMMU = true;
			settings.aica.NoBatch = true;
		}

		// Tony Hawk's Pro Skater 2
		if (!strncmp("T13008D", prod_id, 7) || !strncmp("T13006N", prod_id, 7)
				// Tony Hawk's Pro Skater 1
				|| !strncmp("T40205N", prod_id, 7)
				// Tony Hawk's Skateboarding
				|| !strncmp("T40204D", prod_id, 7)
				// Skies of Arcadia
				|| !strncmp("MK-51052", prod_id, 8)
				// Eternal Arcadia (JP)
				|| !strncmp("HDR-0076", prod_id, 8)
				// Flag to Flag (US)
				|| !strncmp("MK-51007", prod_id, 8)
				// Super Speed Racing (JP)
				|| !strncmp("HDR-0013", prod_id, 8)
				// Yu Suzuki Game Works Vol. 1
				|| !strncmp("6108099", prod_id, 7)
				// L.O.L
				|| !strncmp("T2106M", prod_id, 6)
				// Miss Moonlight
				|| !strncmp("T18702M", prod_id, 7)
				// Tom Clancy's Rainbow Six (US)
				|| !strncmp("T40401N", prod_id, 7)
				// Tom Clancy's Rainbow Six incl. Eagle Watch Missions (EU)
				|| !strncmp("T-45001D05", prod_id, 10))
		{
			INFO_LOG(BOOT, "Enabling render to texture buffer for game %s", prod_id);
			settings.rend.RenderToTextureBuffer = 1;
			rtt_to_buffer_game = true;
		}
		if (!strncmp("HDR-0176", prod_id, 8) || !strncmp("RDC-0057", prod_id, 8))
		{
			INFO_LOG(BOOT, "Enabling translucent depth multipass for game %s", prod_id);
			// Cosmic Smash
			settings.rend.TranslucentPolygonDepthMask = 1;
			tr_poly_depth_mask_game = true;
		}
		// NHL 2K2
		if (!strncmp("MK-51182", prod_id, 8))
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id);
			settings.rend.ExtraDepthScale = 1000000;	// Mali needs 1M, 10K is enough for others
			extra_depth_game = true;
		}
		// Re-Volt (US, EU)
		else if (!strncmp("T-8109N", prod_id, 7) || !strncmp("T8107D  50", prod_id, 10))
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id);
			settings.rend.ExtraDepthScale = 100;
			extra_depth_game = true;
		}
		// Samurai Shodown 6 dc port
		else if (!strncmp("T0002M", prod_id, 6))
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", prod_id);
			settings.rend.ExtraDepthScale = 1e26;
			extra_depth_game = true;
		}
		// Super Producers
		if (!strncmp("T14303M", prod_id, 7)
			// Giant Killers
			|| !strncmp("T45401D 50", prod_id, 10)
			// Wild Metal (US)
			|| !strncmp("T42101N 00", prod_id, 10)
			// Wild Metal (EU)
			|| !strncmp("T40501D-50", prod_id, 10)
			// Resident Evil 2 (US)
			|| !strncmp("T1205N", prod_id, 6)
			// Resident Evil 2 (EU)
			|| !strncmp("T7004D  50", prod_id, 10)
			// Rune Jade
			|| !strncmp("T14304M", prod_id, 7)
			// Marionette Company
			|| !strncmp("T5202M", prod_id, 6)
			// Marionette Company 2
			|| !strncmp("T5203M", prod_id, 6)
			// Maximum Pool (for online support)
			|| !strncmp("T11010N", prod_id, 7)
			// StarLancer (US) (for online support)
			|| !strncmp("T40209N", prod_id, 7)
			// StarLancer (EU) (for online support)
			|| !strncmp("T17723D 05", prod_id, 10)
			// Heroes of might and magic III
			|| !strncmp("T0000M", prod_id, 6)
			// WebTV
			|| !strncmp("6107117", prod_id, 7)
			// PBA
			|| !strncmp("T26702N", prod_id, 7))
		{
			INFO_LOG(BOOT, "Disabling 32-bit virtual memory for game %s", prod_id);
			settings.dynarec.disable_vmem32 = true;
			disable_vmem32_game = true;
		}
		std::string areas(ip_meta.area_symbols, sizeof(ip_meta.area_symbols));
		bool region_usa = areas.find('U') != std::string::npos;
		bool region_eu = areas.find('E') != std::string::npos;
		bool region_japan = areas.find('J') != std::string::npos;
		if (region_usa || region_eu || region_japan)
		{
			switch (settings.dreamcast.region)
			{
			case 0: // Japan
				if (!region_japan)
				{
					NOTICE_LOG(BOOT, "Japan region not supported. Using %s instead", region_usa ? "USA" : "Europe");
					settings.dreamcast.region = region_usa ? 1 : 2;
					forced_game_region = settings.dreamcast.region;
				}
				break;
			case 1: // USA
				if (!region_usa)
				{
					NOTICE_LOG(BOOT, "USA region not supported. Using %s instead", region_eu ? "Europe" : "Japan");
					settings.dreamcast.region = region_eu ? 2 : 0;
					forced_game_region = settings.dreamcast.region;
				}
				break;
			case 2: // Europe
				if (!region_eu)
				{
					NOTICE_LOG(BOOT, "Europe region not supported. Using %s instead", region_usa ? "USA" : "Japan");
					settings.dreamcast.region = region_usa ? 1 : 0;
					forced_game_region = settings.dreamcast.region;
				}
				break;
			case 3: // Default
				if (region_usa)
					settings.dreamcast.region = 1;
				else if (region_eu)
					settings.dreamcast.region = 2;
				else
					settings.dreamcast.region = 0;
				forced_game_region = settings.dreamcast.region;
				break;
			}
		}
		else
			WARN_LOG(BOOT, "No region specified in IP.BIN");
		if (settings.dreamcast.cable <= 1 && !ip_meta.supportsVGA())
		{
			NOTICE_LOG(BOOT, "Game doesn't support VGA. Using TV Composite instead");
			settings.dreamcast.cable = 3;
			forced_game_cable = settings.dreamcast.cable;
		}
		if (settings.dreamcast.cable == 2 &&
				(!strncmp("T40602N", prod_id, 7)	// Centipede
				|| !strncmp("T9710N", prod_id, 6)	// Gauntlet Legends (US)
				|| !strncmp("MK-51152", prod_id, 8) // World Series Baseball 2K2
				|| !strncmp("T-9701N", prod_id, 7)	// Mortal Kombat Gold (US)
				|| !strncmp("T1203N", prod_id, 6)	// Street Fighter Alpha 3 (US)
				|| !strncmp("T1203M", prod_id, 6)	// Street Fighter Zero 3 (JP)
				|| !strncmp("T13002N", prod_id, 7)	// Vigilante 8 (US)
				|| !strncmp("T13003N", prod_id, 7)	// Toy Story 2 (US)
				|| !strncmp("T1209N", prod_id, 6)	// Gigawing (US)
				|| !strncmp("T1208M", prod_id, 6)	// Gigawing (JP)
				|| !strncmp("T1235M", prod_id, 6)))	// Vampire Chronicle for Matching Service
		{
			NOTICE_LOG(BOOT, "Game doesn't support RGB. Using TV Composite instead");
			settings.dreamcast.cable = 3;
			forced_game_cable = settings.dreamcast.cable;
		}
	}
	else if (settings.platform.system == DC_PLATFORM_NAOMI || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		NOTICE_LOG(BOOT, "Game ID is [%s]", naomi_game_id);
		if (!strcmp("SAMURAI SPIRITS 6", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling Extra depth scaling for game %s", naomi_game_id);
			settings.rend.ExtraDepthScale = 1e26;
			extra_depth_game = true;
		}
		if (!strcmp("COSMIC SMASH IN JAPAN", naomi_game_id))
		{
			INFO_LOG(BOOT, "Enabling translucent depth multipass for game %s", naomi_game_id);
			settings.rend.TranslucentPolygonDepthMask = true;
			tr_poly_depth_mask_game = true;
		}
		// Input configuration
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
		settings.rend.Rotate90 = naomi_rotate_screen;
	}
}

void dc_reset(bool hard)
{
	plugins_Reset(hard);
	mem_Reset(hard);

	sh4_cpu.Reset(hard);
}

static bool reset_requested;

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
	InitSettings();
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
		LoadSettings(false);
	}
	settings.pvr.rend = (RenderType)cfgLoadInt("config", "pvr.rend", (int)settings.pvr.rend);

	os_CreateWindow();
	os_SetupInput();

	// Needed to avoid crash calling dc_is_running() in gui
	Get_Sh4Interpreter(&sh4_cpu);
	sh4_cpu.Init();

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
		settings.platform.bbsram_size = 0;
		break;
	case DC_PLATFORM_NAOMI:
		settings.platform.ram_size = 32 * 1024 * 1024;
		settings.platform.vram_size = 16 * 1024 * 1024;
		settings.platform.aram_size = 8 * 1024 * 1024;
		settings.platform.bios_size = 2 * 1024 * 1024;
		settings.platform.flash_size = 0;
		settings.platform.bbsram_size = 32 * 1024;
		break;
	case DC_PLATFORM_ATOMISWAVE:
		settings.platform.ram_size = 16 * 1024 * 1024;
		settings.platform.vram_size = 8 * 1024 * 1024;
		settings.platform.aram_size = 8 * 1024 * 1024;
		settings.platform.bios_size = 128 * 1024;
		settings.platform.flash_size = 0;
		settings.platform.bbsram_size = 128 * 1024;
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
	static bool init_done;

	if (init_done)
		return;

	// Default platform
	set_platform(DC_PLATFORM_DREAMCAST);

	plugins_Init();

#if FEAT_SHREC != DYNAREC_NONE
	Get_Sh4Recompiler(&sh4_cpu);
	sh4_cpu.Init();		// Also initialize the interpreter
	if(settings.dynarec.Enable)
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

	mem_Init();
	reios_init();

	init_done = true;
}

bool game_started;

static int get_game_platform(const char *path)
{
	if (path == NULL)
		// Dreamcast BIOS
		return DC_PLATFORM_DREAMCAST;

	std::string extension = get_file_extension(path);
	if (extension == "")
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
	bool forced_bios_file = false;

	if (path != NULL)
	{
		strcpy(settings.imgread.ImagePath, path);
	}
	else
	{
		// Booting the BIOS requires a BIOS file
		forced_bios_file = true;
		settings.imgread.ImagePath[0] = '\0';
	}

	dc_init();

	set_platform(get_game_platform(path));
	mem_map_default();

	InitSettings();
	dc_reset(true);
	LoadSettings(false);
	
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		if ((settings.bios.UseReios && !forced_bios_file) || !LoadRomFiles())
		{
			if (forced_bios_file)
				throw ReicastException("No BIOS file found");

			if (!LoadHle())
				throw ReicastException("Failed to initialize HLE BIOS");

			NOTICE_LOG(BOOT, "Did not load BIOS, using reios");
		}
	}
	else
	{
		LoadRomFiles();
	}
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		mcfg_CreateDevices();

		if (path == NULL)
		{
			// Boot BIOS
			TermDrive();
			InitDrive();
		}
		else
		{
			std::string extension = get_file_extension(settings.imgread.ImagePath);
			if (extension != "elf")
			{
				if (InitDrive())
					LoadCustom();
				else
				{
					// Content load failed. Boot the BIOS
					settings.imgread.ImagePath[0] = '\0';
					forced_bios_file = true;
					if (!LoadRomFiles())
						throw ReicastException("No BIOS file found");
					InitDrive();
				}
			}
		}
		FixUpFlash();
	}
	else if (settings.platform.system == DC_PLATFORM_NAOMI || settings.platform.system == DC_PLATFORM_ATOMISWAVE)
	{
		naomi_cart_LoadRom(path);
		if (loading_canceled)
			return;
		LoadCustom();
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
		if (saved_screen_stretching == -1)
			saved_screen_stretching = settings.rend.ScreenStretching;
		settings.rend.ScreenStretching = 133;	// 4:3 -> 16:9
	}
	else
	{
		if (saved_screen_stretching != -1)
		{
			settings.rend.ScreenStretching = saved_screen_stretching;
			saved_screen_stretching = -1;
		}
	}
	fast_forward_mode = false;
}

bool dc_is_running()
{
	return sh4_cpu.IsCpuRunning();
}

#ifndef TARGET_DISPFRAME
void* dc_run(void*)
{
	InitAudio();

	if (settings.dynarec.Enable)
	{
		Get_Sh4Recompiler(&sh4_cpu);
		INFO_LOG(DYNAREC, "Using Recompiler");
	}
	else
	{
		Get_Sh4Interpreter(&sh4_cpu);
		INFO_LOG(DYNAREC, "Using Interpreter");
	}
	do {
		reset_requested = false;

		sh4_cpu.Run();

   		SaveRomFiles();
   		if (reset_requested)
   		{
   			dc_reset(false);
   		}
	} while (reset_requested);

    TermAudio();

    return NULL;
}
#endif

void dc_term()
{
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
	sh4_cpu.Stop();
	rend_cancel_emu_wait();
	emu_thread.WaitToEnd();
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

void InitSettings()
{
	settings.dynarec.Enable			= true;
	settings.dynarec.idleskip		= true;
	settings.dynarec.unstable_opt	= false;
	settings.dynarec.safemode		= false;
	settings.dynarec.disable_vmem32	= false;
	settings.dreamcast.cable		= 3;	// TV composite
	settings.dreamcast.region		= 3;	// default
	settings.dreamcast.broadcast	= 4;	// default
	settings.dreamcast.language     = 6;	// default
	settings.dreamcast.FullMMU      = false;
	settings.dreamcast.ForceWindowsCE = false;
	settings.dreamcast.HideLegacyNaomiRoms = true;
	settings.aica.DSPEnabled		= false;
	settings.aica.LimitFPS			= true;
	settings.aica.NoBatch			= false;
    settings.aica.NoSound			= false;
	settings.audio.backend 			= "auto";
	settings.rend.UseMipmaps		= true;
	settings.rend.WideScreen		= false;
	settings.rend.ShowFPS			= false;
	settings.rend.RenderToTextureBuffer = false;
	settings.rend.RenderToTextureUpscale = 1;
	settings.rend.TranslucentPolygonDepthMask = false;
	settings.rend.ModifierVolumes	= true;
	settings.rend.Clipping			= true;
	settings.rend.TextureUpscale	= 1;
	settings.rend.MaxFilteredTextureSize = 256;
	settings.rend.ExtraDepthScale   = 1.f;
	settings.rend.CustomTextures    = false;
	settings.rend.DumpTextures      = false;
	settings.rend.ScreenScaling     = 100;
	settings.rend.ScreenStretching  = 100;
	settings.rend.Fog				= true;
	settings.rend.FloatVMUs			= false;
	settings.rend.Rotate90			= false;
	settings.rend.PerStripSorting	= false;
	settings.rend.DelayFrameSwapping = false;
	settings.rend.WidescreenGameHacks = false;

	settings.pvr.ta_skip			= 0;

	settings.pvr.MaxThreads		    = 3;
	settings.pvr.AutoSkipFrame		= 0;

	settings.debug.SerialConsole	= false;
	settings.debug.SerialPTY        = false;

	settings.bios.UseReios		    = false;

	settings.validate.OpenGlChecks  = false;

	settings.input.MouseSensitivity = 100;
	settings.input.JammaSetup = JVS::Default;
	settings.input.VirtualGamepadVibration = 20;
	for (int i = 0; i < MAPLE_PORTS; i++)
	{
		settings.input.maple_devices[i] = i == 0 ? MDT_SegaController : MDT_None;
		settings.input.maple_expansion_devices[i][0] = i == 0 ? MDT_SegaVMU : MDT_None;
		settings.input.maple_expansion_devices[i][1] = i == 0 ? MDT_SegaVMU : MDT_None;
	}
	settings.network.Enable = false;
	settings.network.ActAsServer = false;
	settings.network.dns = "46.101.91.123";		// Dreamcast Live DNS
	settings.network.server = "";

#if SUPPORT_DISPMANX
	settings.dispmanx.Width		= 0;
	settings.dispmanx.Height	= 0;
	settings.dispmanx.Keep_Aspect = true;
#endif

#if HOST_CPU == CPU_ARM
	settings.aica.BufferSize = 5644;	// 128 ms
#else
	settings.aica.BufferSize = 2822;	// 64 ms
#endif

#if USE_OMX
	settings.omx.Audio_Latency	= 100;
	settings.omx.Audio_HDMI		= true;
#endif
}

void LoadSettings(bool game_specific)
{
	const char *config_section = game_specific ? cfgGetGameId() : "config";
	const char *input_section = game_specific ? cfgGetGameId() : "input";
	const char *audio_section = game_specific ? cfgGetGameId() : "audio";

	settings.dynarec.Enable			= cfgLoadBool(config_section, "Dynarec.Enabled", settings.dynarec.Enable);
	settings.dynarec.idleskip		= cfgLoadBool(config_section, "Dynarec.idleskip", settings.dynarec.idleskip);
	settings.dynarec.unstable_opt	= cfgLoadBool(config_section, "Dynarec.unstable-opt", settings.dynarec.unstable_opt);
	settings.dynarec.safemode		= cfgLoadBool(config_section, "Dynarec.safe-mode", settings.dynarec.safemode);
	settings.dynarec.disable_vmem32 = cfgLoadBool(config_section, "Dynarec.DisableVmem32", settings.dynarec.disable_vmem32);
	//disable_nvmem can't be loaded, because nvmem init is before cfg load
	settings.dreamcast.cable		= cfgLoadInt(config_section, "Dreamcast.Cable", settings.dreamcast.cable);
	settings.dreamcast.region		= cfgLoadInt(config_section, "Dreamcast.Region", settings.dreamcast.region);
	settings.dreamcast.broadcast	= cfgLoadInt(config_section, "Dreamcast.Broadcast", settings.dreamcast.broadcast);
	settings.dreamcast.language     = cfgLoadInt(config_section, "Dreamcast.Language", settings.dreamcast.language);
	settings.dreamcast.FullMMU      = cfgLoadBool(config_section, "Dreamcast.FullMMU", settings.dreamcast.FullMMU);
	settings.dreamcast.ForceWindowsCE = cfgLoadBool(config_section, "Dreamcast.ForceWindowsCE", settings.dreamcast.ForceWindowsCE);
	if (settings.dreamcast.ForceWindowsCE)
		settings.aica.NoBatch = true;
	settings.aica.LimitFPS			= cfgLoadBool(config_section, "aica.LimitFPS", settings.aica.LimitFPS)
			|| cfgLoadInt(config_section, "aica.LimitFPS", 0) == 2;
	settings.aica.DSPEnabled		= cfgLoadBool(config_section, "aica.DSPEnabled", settings.aica.DSPEnabled);
    settings.aica.NoSound			= cfgLoadBool(config_section, "aica.NoSound", settings.aica.NoSound);
    settings.aica.BufferSize        = cfgLoadInt(config_section, "aica.BufferSize", settings.aica.BufferSize);
    settings.aica.BufferSize = std::max(512u, settings.aica.BufferSize);
    settings.audio.backend			= cfgLoadStr(audio_section, "backend", settings.audio.backend.c_str());
	settings.rend.UseMipmaps		= cfgLoadBool(config_section, "rend.UseMipmaps", settings.rend.UseMipmaps);
	settings.rend.WideScreen		= cfgLoadBool(config_section, "rend.WideScreen", settings.rend.WideScreen);
	settings.rend.ShowFPS			= cfgLoadBool(config_section, "rend.ShowFPS", settings.rend.ShowFPS);
	settings.rend.RenderToTextureBuffer = cfgLoadBool(config_section, "rend.RenderToTextureBuffer", settings.rend.RenderToTextureBuffer);
	settings.rend.RenderToTextureUpscale = cfgLoadInt(config_section, "rend.RenderToTextureUpscale", settings.rend.RenderToTextureUpscale);
	settings.rend.TranslucentPolygonDepthMask = cfgLoadBool(config_section, "rend.TranslucentPolygonDepthMask", settings.rend.TranslucentPolygonDepthMask);
	settings.rend.ModifierVolumes	= cfgLoadBool(config_section, "rend.ModifierVolumes", settings.rend.ModifierVolumes);
	settings.rend.Clipping			= cfgLoadBool(config_section, "rend.Clipping", settings.rend.Clipping);
	settings.rend.TextureUpscale	= cfgLoadInt(config_section, "rend.TextureUpscale", settings.rend.TextureUpscale);
	settings.rend.MaxFilteredTextureSize = cfgLoadInt(config_section,"rend.MaxFilteredTextureSize", settings.rend.MaxFilteredTextureSize);
	std::string extra_depth_scale_str = cfgLoadStr(config_section,"rend.ExtraDepthScale", "");
	if (!extra_depth_scale_str.empty())
	{
		settings.rend.ExtraDepthScale = atof(extra_depth_scale_str.c_str());
		if (settings.rend.ExtraDepthScale == 0)
			settings.rend.ExtraDepthScale = 1.f;
	}
	settings.rend.CustomTextures    = cfgLoadBool(config_section, "rend.CustomTextures", settings.rend.CustomTextures);
	settings.rend.DumpTextures      = cfgLoadBool(config_section, "rend.DumpTextures", settings.rend.DumpTextures);
	settings.rend.ScreenScaling     = cfgLoadInt(config_section, "rend.ScreenScaling", settings.rend.ScreenScaling);
	settings.rend.ScreenScaling = std::min(std::max(1, settings.rend.ScreenScaling), 800);
	settings.rend.ScreenStretching  = cfgLoadInt(config_section, "rend.ScreenStretching", settings.rend.ScreenStretching);
	settings.rend.Fog				= cfgLoadBool(config_section, "rend.Fog", settings.rend.Fog);
	settings.rend.FloatVMUs			= cfgLoadBool(config_section, "rend.FloatVMUs", settings.rend.FloatVMUs);
	settings.rend.Rotate90			= cfgLoadBool(config_section, "rend.Rotate90", settings.rend.Rotate90);
	settings.rend.PerStripSorting	= cfgLoadBool(config_section, "rend.PerStripSorting", settings.rend.PerStripSorting);
	settings.rend.DelayFrameSwapping = cfgLoadBool(config_section, "rend.DelayFrameSwapping", settings.rend.DelayFrameSwapping);
	settings.rend.WidescreenGameHacks = cfgLoadBool(config_section, "rend.WidescreenGameHacks", settings.rend.WidescreenGameHacks);

	settings.pvr.ta_skip			= cfgLoadInt(config_section, "ta.skip", settings.pvr.ta_skip);

	settings.pvr.MaxThreads		    = cfgLoadInt(config_section, "pvr.MaxThreads", settings.pvr.MaxThreads);
	if (game_specific)
		settings.pvr.AutoSkipFrame = cfgLoadInt(config_section, "pvr.AutoSkipFrame", settings.pvr.AutoSkipFrame);
	else
	{
		// compatibility with previous SynchronousRendering option
		int autoskip = cfgLoadInt(config_section, "pvr.AutoSkipFrame", 99);
		if (autoskip == 99)
			autoskip = cfgLoadBool(config_section, "pvr.SynchronousRendering", true) ? 1 : 2;
		settings.pvr.AutoSkipFrame = autoskip;
	}

	settings.debug.SerialConsole	= cfgLoadBool(config_section, "Debug.SerialConsoleEnabled", settings.debug.SerialConsole);
	settings.debug.SerialPTY		= cfgLoadBool(config_section, "Debug.SerialPTY", settings.debug.SerialPTY);

	settings.bios.UseReios		    = cfgLoadBool(config_section, "bios.UseReios", settings.bios.UseReios);

	settings.validate.OpenGlChecks  = cfgLoadBool(game_specific ? cfgGetGameId() : "validate", "OpenGlChecks", settings.validate.OpenGlChecks);

	settings.input.MouseSensitivity = cfgLoadInt(input_section, "MouseSensitivity", settings.input.MouseSensitivity);
	settings.input.JammaSetup = (JVS)cfgLoadInt(input_section, "JammaSetup", (int)settings.input.JammaSetup);
	settings.input.VirtualGamepadVibration = cfgLoadInt(input_section, "VirtualGamepadVibration", settings.input.VirtualGamepadVibration);
	for (int i = 0; i < MAPLE_PORTS; i++)
	{
		char device_name[32];
		sprintf(device_name, "device%d", i + 1);
		settings.input.maple_devices[i] = (MapleDeviceType)cfgLoadInt(input_section, device_name, settings.input.maple_devices[i]);
		sprintf(device_name, "device%d.1", i + 1);
		settings.input.maple_expansion_devices[i][0] = (MapleDeviceType)cfgLoadInt(input_section, device_name, settings.input.maple_expansion_devices[i][0]);
		sprintf(device_name, "device%d.2", i + 1);
		settings.input.maple_expansion_devices[i][1] = (MapleDeviceType)cfgLoadInt(input_section, device_name, settings.input.maple_expansion_devices[i][1]);
	}
	settings.network.Enable = cfgLoadBool("network", "Enable", settings.network.Enable);
	settings.network.ActAsServer = cfgLoadBool("network", "ActAsServer", settings.network.ActAsServer);
	settings.network.dns = cfgLoadStr("network", "DNS", settings.network.dns.c_str());
	settings.network.server = cfgLoadStr("network", "server", settings.network.server.c_str());

#if SUPPORT_DISPMANX
	settings.dispmanx.Width		= cfgLoadInt(game_specific ? cfgGetGameId() : "dispmanx", "width", settings.dispmanx.Width);
	settings.dispmanx.Height	= cfgLoadInt(game_specific ? cfgGetGameId() : "dispmanx", "height", settings.dispmanx.Height);
	settings.dispmanx.Keep_Aspect	= cfgLoadBool(game_specific ? cfgGetGameId() : "dispmanx", "maintain_aspect", settings.dispmanx.Keep_Aspect);
#endif

#if USE_OMX
	settings.omx.Audio_Latency	= cfgLoadInt(game_specific ? cfgGetGameId() : "omx", "audio_latency", settings.omx.Audio_Latency);
	settings.omx.Audio_HDMI		= cfgLoadBool(game_specific ? cfgGetGameId() : "omx", "audio_hdmi", settings.omx.Audio_HDMI);
#endif

	if (!game_specific)
	{
		settings.dreamcast.ContentPath.clear();
		std::string paths = cfgLoadStr(config_section, "Dreamcast.ContentPath", "");
		std::string::size_type start = 0;
		while (true)
		{
			std::string::size_type end = paths.find(';', start);
			if (end == std::string::npos)
				end = paths.size();
			if (start != end)
				settings.dreamcast.ContentPath.push_back(paths.substr(start, end - start));
			if (end == paths.size())
				break;
			start = end + 1;
		}
		settings.dreamcast.HideLegacyNaomiRoms = cfgLoadBool(config_section, "Dreamcast.HideLegacyNaomiRoms", settings.dreamcast.HideLegacyNaomiRoms);
	}
/*
	//make sure values are valid
	settings.dreamcast.cable		= std::min(std::max(settings.dreamcast.cable,    0),3);
	settings.dreamcast.region		= std::min(std::max(settings.dreamcast.region,   0),3);
	settings.dreamcast.broadcast	= std::min(std::max(settings.dreamcast.broadcast,0),4);
*/
}

static void LoadCustom()
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

	cfgSetGameId(reios_id);

	// Reload per-game settings
	LoadSettings(true);
}

void SaveSettings()
{
	cfgSetAutoSave(false);
	cfgSaveBool("config", "Dynarec.Enabled", settings.dynarec.Enable);
	if (forced_game_cable == -1 || forced_game_cable != (int)settings.dreamcast.cable)
		cfgSaveInt("config", "Dreamcast.Cable", settings.dreamcast.cable);
	if (forced_game_region == -1 || forced_game_region != (int)settings.dreamcast.region)
		cfgSaveInt("config", "Dreamcast.Region", settings.dreamcast.region);
	cfgSaveInt("config", "Dreamcast.Broadcast", settings.dreamcast.broadcast);
	cfgSaveBool("config", "Dreamcast.ForceWindowsCE", settings.dreamcast.ForceWindowsCE);
	cfgSaveBool("config", "Dynarec.idleskip", settings.dynarec.idleskip);
	cfgSaveBool("config", "Dynarec.unstable-opt", settings.dynarec.unstable_opt);
	if (!safemode_game || !settings.dynarec.safemode)
		cfgSaveBool("config", "Dynarec.safe-mode", settings.dynarec.safemode);
	cfgSaveBool("config", "bios.UseReios", settings.bios.UseReios);

//	if (!disable_vmem32_game || !settings.dynarec.disable_vmem32)
//		cfgSaveBool("config", "Dynarec.DisableVmem32", settings.dynarec.disable_vmem32);
	cfgSaveInt("config", "Dreamcast.Language", settings.dreamcast.language);
	cfgSaveBool("config", "aica.LimitFPS", settings.aica.LimitFPS);
	cfgSaveBool("config", "aica.DSPEnabled", settings.aica.DSPEnabled);
	cfgSaveBool("config", "aica.NoSound", settings.aica.NoSound);
	cfgSaveInt("config", "aica.BufferSize", settings.aica.BufferSize);
	cfgSaveStr("audio", "backend", settings.audio.backend.c_str());

	// Write backend specific settings
	// std::map<std::string, std::map<std::string, std::string>>
	for (const auto& pair : settings.audio.options)
	{
		const std::string& section = pair.first;
		const auto& options = pair.second;
		for (const auto& option : options)
			cfgSaveStr(section.c_str(), option.first.c_str(), option.second.c_str());
	}

	cfgSaveBool("config", "rend.WideScreen", settings.rend.WideScreen);
	cfgSaveBool("config", "rend.ShowFPS", settings.rend.ShowFPS);
	if (!rtt_to_buffer_game || !settings.rend.RenderToTextureBuffer)
		cfgSaveBool("config", "rend.RenderToTextureBuffer", settings.rend.RenderToTextureBuffer);
	cfgSaveInt("config", "rend.RenderToTextureUpscale", settings.rend.RenderToTextureUpscale);
	cfgSaveBool("config", "rend.ModifierVolumes", settings.rend.ModifierVolumes);
	cfgSaveBool("config", "rend.Clipping", settings.rend.Clipping);
	cfgSaveInt("config", "rend.TextureUpscale", settings.rend.TextureUpscale);
	cfgSaveInt("config", "rend.MaxFilteredTextureSize", settings.rend.MaxFilteredTextureSize);
	cfgSaveBool("config", "rend.CustomTextures", settings.rend.CustomTextures);
	cfgSaveBool("config", "rend.DumpTextures", settings.rend.DumpTextures);
	cfgSaveInt("config", "rend.ScreenScaling", settings.rend.ScreenScaling);
	if (saved_screen_stretching != -1)
		cfgSaveInt("config", "rend.ScreenStretching", saved_screen_stretching);
	else
		cfgSaveInt("config", "rend.ScreenStretching", settings.rend.ScreenStretching);
	cfgSaveBool("config", "rend.Fog", settings.rend.Fog);
	cfgSaveBool("config", "rend.FloatVMUs", settings.rend.FloatVMUs);
	if (!naomi_rotate_screen || !settings.rend.Rotate90)
		cfgSaveBool("config", "rend.Rotate90", settings.rend.Rotate90);
	cfgSaveInt("config", "ta.skip", settings.pvr.ta_skip);
	cfgSaveInt("config", "pvr.rend", (int)settings.pvr.rend);
	cfgSaveBool("config", "rend.PerStripSorting", settings.rend.PerStripSorting);
	cfgSaveBool("config", "rend.DelayFrameSwapping", settings.rend.DelayFrameSwapping);
	cfgSaveBool("config", "rend.WidescreenGameHacks", settings.rend.WidescreenGameHacks);

	cfgSaveInt("config", "pvr.MaxThreads", settings.pvr.MaxThreads);
	cfgSaveInt("config", "pvr.AutoSkipFrame", settings.pvr.AutoSkipFrame);

	cfgSaveBool("config", "Debug.SerialConsoleEnabled", settings.debug.SerialConsole);
	cfgSaveBool("config", "Debug.SerialPTY", settings.debug.SerialPTY);
	cfgSaveInt("input", "MouseSensitivity", settings.input.MouseSensitivity);
	cfgSaveInt("input", "VirtualGamepadVibration", settings.input.VirtualGamepadVibration);
	for (int i = 0; i < MAPLE_PORTS; i++)
	{
		char device_name[32];
		sprintf(device_name, "device%d", i + 1);
		cfgSaveInt("input", device_name, (s32)settings.input.maple_devices[i]);
		sprintf(device_name, "device%d.1", i + 1);
		cfgSaveInt("input", device_name, (s32)settings.input.maple_expansion_devices[i][0]);
		sprintf(device_name, "device%d.2", i + 1);
		cfgSaveInt("input", device_name, (s32)settings.input.maple_expansion_devices[i][1]);
	}
	// FIXME This should never be a game-specific setting
	std::string paths;
	for (auto& path : settings.dreamcast.ContentPath)
	{
		if (!paths.empty())
			paths += ";";
		paths += path;
	}
	cfgSaveStr("config", "Dreamcast.ContentPath", paths.c_str());
	cfgSaveBool("config", "Dreamcast.HideLegacyNaomiRoms", settings.dreamcast.HideLegacyNaomiRoms);
	cfgSaveBool("network", "Enable", settings.network.Enable);
	cfgSaveBool("network", "ActAsServer", settings.network.ActAsServer);
	cfgSaveStr("network", "DNS", settings.network.dns.c_str());
	cfgSaveStr("network", "server", settings.network.server.c_str());

	GamepadDevice::SaveMaplePorts();

#ifdef __ANDROID__
	void SaveAndroidSettings();
	SaveAndroidSettings();
#endif

	cfgSetAutoSave(true);
}

void dc_resume()
{
	SetMemoryHandlers();
	game_started = true;
	if (!emu_thread.thread.joinable())
		emu_thread.Start();
}

static void cleanup_serialize(void *data)
{
	if ( data != NULL )
		free(data) ;
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

	dc_stop();

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
	FILE *f = fopen(filename.c_str(), "wb") ;

	if ( f == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str()) ;
		gui_display_notification("Cannot open save file", 2000);
		cleanup_serialize(data) ;
    	return;
	}

	fwrite(data, 1, total_size, f) ;
	fclose(f);
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
		f = fopen(filename.c_str(), "rb") ;

		if ( f == NULL )
		{
			WARN_LOG(SAVESTATE, "Failed to load state - could not open %s for reading", filename.c_str()) ;
			gui_display_notification("Save state not found", 2000);
			return;
		}
		fseek(f, 0, SEEK_END);
		total_size = (u32)ftell(f);
		fseek(f, 0, SEEK_SET);
	}
	void *data = malloc(total_size);
	if ( data == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to load state - could not malloc %d bytes", total_size) ;
		gui_display_notification("Failed to load state - memory full", 2000);
		if (f != nullptr)
			fclose(f);
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
		fclose(f);
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
    FlushCache();
#endif
#ifndef NO_MMU
    mmu_flush_table();
#endif
	bm_Reset();

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
    CalculateSync();

    cleanup_serialize(data) ;
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
	loading_done.get();
}
