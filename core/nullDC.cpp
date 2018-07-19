// nullDC.cpp : Makes magic cookies
//

//initialse Emu
#include "types.h"
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "hw/mem/_vmem.h"
#include "stdclass.h"
#include "cfg/cfg.h"

#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_mem.h"

#include "webui/server.h"
#include "hw/naomi/naomi_cart.h"
#include "reios/reios.h"

settings_t settings;

/*
	libndc

	//initialise (and parse the command line)
	ndc_init(argc,argv);

	...
	//run a dreamcast slice
	//either a frame, or up to 25 ms of emulation
	//returns 1 if the frame is ready (fb needs to be flipped -- i'm looking at you android)
	ndc_step();

	...
	//terminate (and free everything)
	ndc_term()
*/

#if HOST_OS==OS_WINDOWS
#include <windows.h>
#endif

int GetFile(char *szFileName, char *szParse=0,u32 flags=0) 
{
	cfgLoadStr("config","image",szFileName,"null");
	if (strcmp(szFileName,"null")==0)
	{
	#if HOST_OS==OS_WINDOWS
		OPENFILENAME ofn;
		ZeroMemory( &ofn , sizeof( ofn));
	ofn.lStructSize = sizeof ( ofn );
	ofn.hwndOwner = NULL  ;
	ofn.lpstrFile = szFileName ;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = "All\0*.*\0\0";
	ofn.nFilterIndex =1;
	ofn.lpstrFileTitle = NULL ;
	ofn.nMaxFileTitle = 0 ;
	ofn.lpstrInitialDir=NULL ;
	ofn.Flags = OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST ;

		if (GetOpenFileNameA(&ofn))
		{
			//already there
			//strcpy(szFileName,ofn.lpstrFile);
		}
	#endif
	}

	return 1; 
}


s32 plugins_Init()
{

	if (s32 rv = libPvr_Init())
		return rv;

	#ifndef TARGET_DISPFRAME
	if (s32 rv = libGDR_Init())
		return rv;
	#endif
	#if DC_PLATFORM == DC_PLATFORM_NAOMI
	if (!naomi_cart_SelectFile(libPvr_GetRenderTarget()))
		return rv_serror;
	#endif

	if (s32 rv = libAICA_Init())
		return rv;
	
	if (s32 rv = libARM_Init())
		return rv;
	
	//if (s32 rv = libExtDevice_Init())
	//	return rv;



	return rv_ok;
}

void plugins_Term()
{
	//term all plugins
	//libExtDevice_Term();
	libARM_Term();
	libAICA_Term();
	libGDR_Term();
	libPvr_Term();
}

void plugins_Reset(bool Manual)
{
	libPvr_Reset(Manual);
	libGDR_Reset(Manual);
	libAICA_Reset(Manual);
	libARM_Reset(Manual);
	//libExtDevice_Reset(Manual);
}

#if !defined(TARGET_NO_WEBUI)

void* webui_th(void* p)
{
	webui_start();
	return 0;
}

cThread webui_thd(&webui_th,0);
#endif

const char *EGHackGamesList[] = {
		"T46703M",		/* Border Down (Japan) */
		"MK-51065",		/* Bomberman Online (USA) */
		"RDC-0117",		/* Bomberman Online (USA) */
		"T47801M",		/* Chaos Field (Japan) */
		"T23202M",		/* Death Crimson OX (Japan) */
		"T2401N",		/* Death Crimson OX (USA) */
		"T1223M",		/* Eldorado Gate Vol. 1 (Japan) */
		"T44306N",		/* Fatal Fury: Mark of the Wolves (USA) */
		"T3108M",		/* Garou: Mark of the Wolves (Japan) */
		"HDR-0078",		/* Jet Set Radio (Japan) */
		"MK-51058",		/* Jet Grind Radio (USA, Europe) */
		"HDR-0079",		/* Napple Tale (Japan) */
		"MK-5110050",	/* Phantasy Star Online (Europe) */
		"HDR-0129",		/* Phantasy Star Online (Japan) */
		"MK-51100",		/* Phantasy Star Online (USA) */
		"MK-5119350",	/* Phantasy Star Online Ver. 2 (Europe) */
		"HDR-0163",		/* Phantasy Star Online Ver. 2 (Japan) */
		"MK-51193",		/* Phantasy Star Online Ver. 2 (USA) */
		"T9907M",		/* Psyvariar 2 (Japan) */
		"HDR-0216",		/* Puyo Puyo Fever (Japan) */
		"T47802M",		/* Radirgy (Japan) */
		"HDR-0151",		/* Segagaga (Japan) */
		"HDR-0125",		/* Sonic Shuffle (Japan) */
		"MK-5106050",	/* Sonic Shuffle (Europe) */
		"MK-51060",		/* Sonic Shuffle (USA) */
		"T29102M",		/* Trigger Heart Exelica (Japan) */
		"T45101M",		/* WWF Royal Rumble (Japan) */
		"T10003D 50",	/* WWF Royal Rumble (Europe) */
		"T10005N",		/* WWF Royal Rumble (USA) */
		NULL
};

void LoadSpecialSettings()
{
	// Tony Hawk's Pro Skater 2
	if (!strncmp("T13008D", reios_product_number, 7) || !strncmp("T13006N", reios_product_number, 7)
			// Tony Hawk's Pro Skater 1
			|| !strncmp("T40205N", reios_product_number, 7))
		settings.rend.RenderToTextureBuffer = 1;
	if (!strncmp("HDR-0176", reios_product_number, 8) || !strncmp("RDC-0057", reios_product_number, 8))
		// Cosmic Smash
		settings.rend.TranslucentPolygonDepthMask = 1;
	for (int i = 0; EGHackGamesList[i] != NULL; i++)
		if (!strncmp(reios_product_number, EGHackGamesList[i], strlen(EGHackGamesList[i])))
		{
			printf("Enabling EG Hack\n");
			settings.aica.EGHack = 1;
			break;
		}
	// Pro Pinball Trilogy
	if (!strncmp("T30701D", reios_product_number, 7)
		// Demolition Racer
		|| !strncmp("T15112N", reios_product_number, 7)
		// Star Wars - Episode I - Racer (United Kingdom)
		|| !strncmp("T23001D", reios_product_number, 7))
		settings.dynarec.DisableDivMatching = true;
}

int dc_init(int argc,wchar* argv[])
{
	setbuf(stdin,0);
	setbuf(stdout,0);
	setbuf(stderr,0);
	if (!_vmem_reserve())
	{
		printf("Failed to alloc mem\n");
		return -1;
	}

#if !defined(TARGET_NO_WEBUI)
	webui_thd.Start();
#endif

	if(ParseCommandLine(argc,argv))
	{
		return 69;
	}
	if(!cfgOpen())
	{
		msgboxf("Unable to open config file",MBX_ICONERROR);
		return -4;
	}
	LoadSettings();
#ifndef _ANDROID
	os_CreateWindow();
#endif

	int rv= 0;

#if HOST_OS != OS_DARWIN
    #define DATA_PATH "/data/"
#else
    #define DATA_PATH "/"
#endif
    
	if (settings.bios.UseReios || !LoadRomFiles(get_readonly_data_path(DATA_PATH)))
	{
		if (!LoadHle(get_readonly_data_path(DATA_PATH)))
			return -3;
		else
			printf("Did not load bios, using reios\n");
	}

#if FEAT_SHREC != DYNAREC_NONE
	if(settings.dynarec.Enable)
	{
		Get_Sh4Recompiler(&sh4_cpu);
		printf("Using Recompiler\n");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		printf("Using Interpreter\n");
	}
	
  InitAudio();

	sh4_cpu.Init();
	mem_Init();

	plugins_Init();
	
	mem_map_default();

#ifndef _ANDROID
	mcfg_CreateDevices();
#else
    mcfg_CreateDevices();
#endif

	plugins_Reset(false);
	mem_Reset(false);
	

	sh4_cpu.Reset(false);
	
	const char* bootfile = reios_locate_ip();
	if (!bootfile || !reios_locate_bootfile("1ST_READ.BIN"))
		printf("Failed to locate bootfile.\n");

	LoadSpecialSettings();

	return rv;
}

#ifndef TARGET_DISPFRAME
void dc_run()
{
	sh4_cpu.Run();
}
#endif

void dc_term()
{
	sh4_cpu.Term();
	plugins_Term();
	_vmem_release();

#ifndef _ANDROID
	SaveSettings();
#endif
	SaveRomFiles(get_writable_data_path("/data/"));
}

void LoadSettings()
{
#ifndef _ANDROID
	settings.dynarec.Enable			= cfgLoadInt("config","Dynarec.Enabled", 1)!=0;
	settings.dynarec.idleskip		= cfgLoadInt("config","Dynarec.idleskip",1)!=0;
	settings.dynarec.unstable_opt	= cfgLoadInt("config","Dynarec.unstable-opt",0);
	settings.dynarec.DisableDivMatching	= cfgLoadInt("config", "Dynarec.DisableDivMatching", 0);
	//disable_nvmem can't be loaded, because nvmem init is before cfg load
	settings.dreamcast.cable		= cfgLoadInt("config","Dreamcast.Cable",3);
	settings.dreamcast.RTC			= cfgLoadInt("config","Dreamcast.RTC",GetRTC_now());
	settings.dreamcast.region		= cfgLoadInt("config","Dreamcast.Region",3);
	settings.dreamcast.broadcast	= cfgLoadInt("config","Dreamcast.Broadcast",4);
	settings.aica.LimitFPS			= cfgLoadInt("config","aica.LimitFPS",1);
	settings.aica.NoBatch			= cfgLoadInt("config","aica.NoBatch",0);
    settings.aica.NoSound			= cfgLoadInt("config","aica.NoSound",0);
    settings.aica.EGHack			= cfgLoadInt("config","aica.EGHack", 0);;
	settings.rend.UseMipmaps		= cfgLoadInt("config","rend.UseMipmaps",1);
	settings.rend.WideScreen		= cfgLoadInt("config","rend.WideScreen",0);
	settings.rend.ShowFPS			= cfgLoadInt("config", "rend.ShowFPS", 0);
	settings.rend.RenderToTextureBuffer = cfgLoadInt("config", "rend.RenderToTextureBuffer", 0);
	settings.rend.RenderToTextureUpscale = cfgLoadInt("config", "rend.RenderToTextureUpscale", 1);
	settings.rend.TranslucentPolygonDepthMask = cfgLoadInt("config", "rend.TranslucentPolygonDepthMask", 0);
	settings.rend.ModifierVolumes	= cfgLoadInt("config","rend.ModifierVolumes",1);
	settings.rend.Clipping			= cfgLoadInt("config","rend.Clipping",1);
	
	settings.pvr.subdivide_transp	= cfgLoadInt("config","pvr.Subdivide",0);
	
	settings.pvr.ta_skip			= cfgLoadInt("config","ta.skip",0);
	settings.pvr.rend				= cfgLoadInt("config","pvr.rend",0);

	settings.pvr.MaxThreads			= cfgLoadInt("config", "pvr.MaxThreads", 3);
	settings.pvr.SynchronousRendering			= cfgLoadInt("config", "pvr.SynchronousRendering", 0);

	settings.debug.SerialConsole = cfgLoadInt("config", "Debug.SerialConsoleEnabled", 0) != 0;

	settings.bios.UseReios = cfgLoadInt("config", "bios.UseReios", 0);
	settings.reios.ElfFile = cfgLoadStr("reios", "ElfFile", "");

	settings.validate.OpenGlChecks = cfgLoadInt("validate", "OpenGlChecks", 0) != 0;
#endif

	settings.pvr.HashLogFile = cfgLoadStr("testing", "ta.HashLogFile", "");
	settings.pvr.HashCheckFile = cfgLoadStr("testing", "ta.HashCheckFile", "");

#if SUPPORT_DISPMANX
	settings.dispmanx.Width = cfgLoadInt("dispmanx","width",640);
	settings.dispmanx.Height = cfgLoadInt("dispmanx","height",480);
	settings.dispmanx.Maintain_Aspect = cfgLoadBool("dispmanx","maintain_aspect",true);
#endif

#if (HOST_OS != OS_LINUX || defined(_ANDROID) || defined(TARGET_PANDORA))
	settings.aica.BufferSize=2048;
#else
	settings.aica.BufferSize=1024;
#endif

#if USE_OMX
	settings.omx.Audio_Latency = cfgLoadInt("omx","audio_latency",100);
	settings.omx.Audio_HDMI = cfgLoadBool("omx","audio_hdmi",true);
#endif

/*
	//make sure values are valid
	settings.dreamcast.cable	= min(max(settings.dreamcast.cable,    0),3);
	settings.dreamcast.region	= min(max(settings.dreamcast.region,   0),3);
	settings.dreamcast.broadcast= min(max(settings.dreamcast.broadcast,0),4);
*/
}
void SaveSettings()
{
	cfgSaveInt("config","Dynarec.Enabled",	settings.dynarec.Enable);
	cfgSaveInt("config","Dreamcast.Cable",	settings.dreamcast.cable);
	cfgSaveInt("config","Dreamcast.RTC",	settings.dreamcast.RTC);
	cfgSaveInt("config","Dreamcast.Region",	settings.dreamcast.region);
	cfgSaveInt("config","Dreamcast.Broadcast",settings.dreamcast.broadcast);
}
