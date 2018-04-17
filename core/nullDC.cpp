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

	if (s32 rv = libGDR_Init())
		return rv;
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
	
	return rv;
}

void dc_run()
{
	sh4_cpu.Run();
}

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
	//disable_nvmem can't be loaded, because nvmem init is before cfg load
	settings.dreamcast.cable		= cfgLoadInt("config","Dreamcast.Cable",3);
	settings.dreamcast.RTC			= cfgLoadInt("config","Dreamcast.RTC",GetRTC_now());
	settings.dreamcast.region		= cfgLoadInt("config","Dreamcast.Region",3);
	settings.dreamcast.broadcast	= cfgLoadInt("config","Dreamcast.Broadcast",4);
	settings.aica.LimitFPS			= cfgLoadInt("config","aica.LimitFPS",1);
	settings.aica.NoBatch			= cfgLoadInt("config","aica.NoBatch",0);
    settings.aica.NoSound			= cfgLoadInt("config","aica.NoSound",0);
	settings.rend.UseMipmaps		= cfgLoadInt("config","rend.UseMipmaps",1);
	settings.rend.WideScreen		= cfgLoadInt("config","rend.WideScreen",0);
	
	settings.pvr.subdivide_transp	= cfgLoadInt("config","pvr.Subdivide",0);
	
	settings.pvr.ta_skip			= cfgLoadInt("config","ta.skip",0);
	settings.pvr.rend				= cfgLoadInt("config","pvr.rend",0);

	settings.pvr.MaxThreads			= cfgLoadInt("config", "pvr.MaxThreads", 3);
	settings.pvr.SynchronousRendering			= cfgLoadInt("config", "pvr.SynchronousRendering", 0);

	settings.debug.SerialConsole = cfgLoadInt("config", "Debug.SerialConsoleEnabled", 0) != 0;

	settings.reios.ElfFile = cfgLoadStr("reios", "ElfFile","");

	settings.validate.OpenGlChecks = cfgLoadInt("validate", "OpenGlChecks", 0) != 0;
#endif

	settings.bios.UseReios = cfgLoadInt("config", "bios.UseReios", 0);
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
