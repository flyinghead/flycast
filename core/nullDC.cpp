// nullDC.cpp : Makes magic cookies
//

//initialse Emu
#include "types.h"
#include "oslib/oslib.h"
#include "hw/mem/_vmem.h"
#include "stdclass.h"
#include "cfg/cfg.h"

#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_mem.h"

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
#if defined(OMAP4)
		strcpy(szFileName,GetPath("gdimage/crazy_taxi.chd").c_str());
#else
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
	#else
		strcpy(szFileName,GetPath("discs/game.gdi").c_str());
	#endif
#endif
	}

	if (strcmp(szFileName,"hardpath")==0)
	{
		strcpy(szFileName,"D:\\DC\\shenmue.chd");
	}

	
	return 1; 
}


s32 plugins_Init()
{

	if (s32 rv = libPvr_Init())
		return rv;

	if (s32 rv = libGDR_Init())
		return rv;
	
	#ifdef BUILD_NAOMI
	if (!NaomiSelectFile(GetRenderTargetHandle()))
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


	if (!LoadRomFiles(GetPath("/data/")))
	{
		return -3;
	}

#if !defined(HOST_NO_REC)
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
	
	  void InitAudio();
  InitAudio();

	sh4_cpu.Init();
	mem_Init();

	plugins_Init();
	
	mem_map_default();

	mcfg_CreateDevices();

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

	SaveSettings();
	SaveRomFiles(GetPath("/data/"));
}

void LoadSettings()
{
	settings.dynarec.Enable			= cfgLoadInt("config","Dynarec.Enabled", 1)!=0;
	settings.dynarec.idleskip		= cfgLoadInt("config","Dynarec.idleskip",1)!=0;
	settings.dynarec.unstable_opt	= cfgLoadInt("config","Dynarec.unstable-opt",0);
	settings.dreamcast.cable		= cfgLoadInt("config","Dreamcast.Cable",3);
	settings.dreamcast.RTC			= cfgLoadInt("config","Dreamcast.RTC",GetRTC_now());
	settings.dreamcast.region		= cfgLoadInt("config","Dreamcast.Region",3);
	settings.dreamcast.broadcast	= cfgLoadInt("config","Dreamcast.Broadcast",4);
	settings.aica.LimitFPS			= cfgLoadInt("config","aica.LimitFPS",1);
	settings.aica.NoBatch			= cfgLoadInt("config","aica.NoBatch",0);
	settings.rend.UseMipmaps		= cfgLoadInt("config","rend.UseMipmaps",1);
	settings.rend.WideScreen		= cfgLoadInt("config","rend.WideScreen",0);
	
	settings.pvr.subdivide_transp	= cfgLoadInt("config","pvr.Subdivide",0);
	
	settings.pvr.ta_skip			= cfgLoadInt("config","ta.skip",0);
	settings.pvr.rend				= cfgLoadInt("config","pvr.rend",0);

#if (HOST_OS != OS_LINUX || defined(_ANDROID) || defined(TARGET_PANDORA))
	settings.aica.BufferSize=2048;
#else
	settings.aica.BufferSize=1024;
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
