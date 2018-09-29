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
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/pvr/Renderer_if.h"

settings_t settings;
static bool performed_serialization = false;
static cMutex mtx_serialization ;
static cMutex mtx_mainloop ;

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

/**
 * cpu_features_get_time_usec:
 *
 * Gets time in microseconds.
 *
 * Returns: time in microseconds.
 **/
int64_t get_time_usec(void)
{
#if HOST_OS==OS_WINDOWS
   static LARGE_INTEGER freq;
   LARGE_INTEGER count;

   /* Frequency is guaranteed to not change. */
   if (!freq.QuadPart && !QueryPerformanceFrequency(&freq))
      return 0;

   if (!QueryPerformanceCounter(&count))
      return 0;
   return count.QuadPart * 1000000 / freq.QuadPart;
#elif defined(_POSIX_MONOTONIC_CLOCK) || defined(__QNX__) || defined(ANDROID) || defined(__MACH__) || HOST_OS==OS_LINUX
   struct timespec tv = {0};
   if (clock_gettime(CLOCK_MONOTONIC, &tv) < 0)
      return 0;
   return tv.tv_sec * INT64_C(1000000) + (tv.tv_nsec + 500) / 1000;
#elif defined(EMSCRIPTEN)
   return emscripten_get_now() * 1000;
#elif defined(__mips__) || defined(DJGPP)
   struct timeval tv;
   gettimeofday(&tv,NULL);
   return (1000000 * tv.tv_sec + tv.tv_usec);
#else
#error "Your platform does not have a timer function implemented in cpu_features_get_time_usec(). Cannot continue."
#endif
}


int GetFile(char *szFileName, char *szParse=0, u32 flags=0)
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

#if !defined(TARGET_NO_WEBUI) && !defined(TARGET_NO_THREADS)

void* webui_th(void* p)
{
	webui_start();
	return 0;
}

cThread webui_thd(&webui_th,0);
#endif

void LoadSpecialSettings()
{
	// Tony Hawk's Pro Skater 2
	if (!strncmp("T13008D", reios_product_number, 7) || !strncmp("T13006N", reios_product_number, 7)
			// Tony Hawk's Pro Skater 1
			|| !strncmp("T40205N", reios_product_number, 7)
			// Tony Hawk's Skateboarding
			|| !strncmp("T40204D", reios_product_number, 7))
		settings.rend.RenderToTextureBuffer = 1;
	if (!strncmp("HDR-0176", reios_product_number, 8) || !strncmp("RDC-0057", reios_product_number, 8))
		// Cosmic Smash
		settings.rend.TranslucentPolygonDepthMask = 1;
	// Pro Pinball Trilogy
	if (!strncmp("T30701D", reios_product_number, 7)
		// Demolition Racer
		|| !strncmp("T15112N", reios_product_number, 7)
		// Star Wars - Episode I - Racer (United Kingdom)
		|| !strncmp("T23001D", reios_product_number, 7)
		// Record of Lodoss War (EU)
		|| !strncmp("T7012D", reios_product_number, 6)
		// Record of Lodoss War (USA)
		|| !strncmp("T40218N", reios_product_number, 7)
		// Surf Rocket Racers
		|| !strncmp("T40216N", reios_product_number, 7))
	{
		printf("Enabling Dynarec safe mode for game %s\n", reios_product_number);
		settings.dynarec.safemode = 1;
	}
}

#if defined(_ANDROID)
int reios_init_value;

void reios_init(int argc,wchar* argv[])
#else
int dc_init(int argc,wchar* argv[])
#endif
{
	setbuf(stdin,0);
	setbuf(stdout,0);
	setbuf(stderr,0);
	if (!_vmem_reserve())
	{
		printf("Failed to alloc mem\n");
#if defined(_ANDROID)
		reios_init_value = -1;
		return;
#else
		return -1;
#endif
	}

#if !defined(TARGET_NO_WEBUI) && !defined(TARGET_NO_THREADS)
	webui_thd.Start();
#endif

	if(ParseCommandLine(argc,argv))
	{
#if defined(_ANDROID)
        reios_init_value = 69;
        return;
#else
        return 69;
#endif
	}
	if(!cfgOpen())
	{
		msgboxf("Unable to open config file",MBX_ICONERROR);
#if defined(_ANDROID)
		reios_init_value = -4;
		return;
#else
		return -4;
#endif
	}
	LoadSettings();
#ifndef _ANDROID
	os_CreateWindow();
#endif

	int rv = 0;

#if HOST_OS != OS_DARWIN
    #define DATA_PATH "/data/"
#else
    #define DATA_PATH "/"
#endif

	if (settings.bios.UseReios || !LoadRomFiles(get_readonly_data_path(DATA_PATH)))
	{
		if (!LoadHle(get_readonly_data_path(DATA_PATH)))
		{
#if defined(_ANDROID)
			reios_init_value = -4;
			return;
#else
			return -3;
#endif
		}
		else
		{
			printf("Did not load bios, using reios\n");
		}
	}

	plugins_Init();

#if defined(_ANDROID)
}

int dc_init()
{
	int rv = 0;
	if (reios_init_value != 0)
		return reios_init_value;
#else
	LoadCustom();
#endif

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

	mem_map_default();

	os_SetupInput();

#if DC_PLATFORM == DC_PLATFORM_NAOMI
	mcfg_CreateNAOMIJamma();
#endif

	plugins_Reset(false);
	mem_Reset(false);

	sh4_cpu.Reset(false);
	
	return rv;
}

#ifndef TARGET_DISPFRAME
bool dc_is_running()
{
	return sh4_cpu.IsCpuRunning();
}

void dc_run()
{
    while ( true )
    {
    	performed_serialization = false ;
    	mtx_mainloop.Lock() ;
    	sh4_cpu.Run();
        mtx_mainloop.Unlock() ;

    	mtx_serialization.Lock() ;
    	mtx_serialization.Unlock() ;

    	if (!performed_serialization)
    		break ;
    }
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

    TermAudio();

#if !defined(TARGET_NO_WEBUI) && !defined(TARGET_NO_THREADS)
    extern void sighandler(int sig);
	sighandler(0);
	webui_thd.WaitToEnd();
#endif

}

#if defined(_ANDROID)
void dc_pause()
{
	SaveRomFiles(get_writable_data_path("/data/"));
}
#endif

void dc_stop()
{
	sh4_cpu.Stop();
}

void dc_start()
{
	sh4_cpu.Start();
}

void LoadSettings()
{
#ifndef _ANDROID
	settings.dynarec.Enable			= cfgLoadInt("config","Dynarec.Enabled", 1)!=0;
	settings.dynarec.idleskip		= cfgLoadInt("config","Dynarec.idleskip",1)!=0;
	settings.dynarec.unstable_opt	= cfgLoadInt("config","Dynarec.unstable-opt",0);
	settings.dynarec.safemode		= cfgLoadInt("config", "Dynarec.safe-mode", 0);
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
	settings.rend.ShowFPS			= cfgLoadInt("config", "rend.ShowFPS", 0);
	settings.rend.RenderToTextureBuffer = cfgLoadInt("config", "rend.RenderToTextureBuffer", 0);
	settings.rend.RenderToTextureUpscale = cfgLoadInt("config", "rend.RenderToTextureUpscale", 1);
	settings.rend.TranslucentPolygonDepthMask = cfgLoadInt("config", "rend.TranslucentPolygonDepthMask", 0);
	settings.rend.ModifierVolumes	= cfgLoadInt("config","rend.ModifierVolumes",1);
	settings.rend.Clipping			= cfgLoadInt("config","rend.Clipping",1);
	settings.rend.TextureUpscale	= cfgLoadInt("config","rend.TextureUpscale", 1);
	settings.rend.MaxFilteredTextureSize = cfgLoadInt("config","rend.MaxFilteredTextureSize", 256);
	char extra_depth_scale_str[128];
	cfgLoadStr("config","rend.ExtraDepthScale", extra_depth_scale_str, "1");
	settings.rend.ExtraDepthScale = atof(extra_depth_scale_str);
	if (settings.rend.ExtraDepthScale == 0)
		settings.rend.ExtraDepthScale = 1.f;

	settings.pvr.subdivide_transp	= cfgLoadInt("config","pvr.Subdivide",0);
	
	settings.pvr.ta_skip			= cfgLoadInt("config","ta.skip",0);
	settings.pvr.rend				= cfgLoadInt("config","pvr.rend",0);

	settings.pvr.MaxThreads		= cfgLoadInt("config", "pvr.MaxThreads", 3);
	settings.pvr.SynchronousRender	= cfgLoadInt("config", "pvr.SynchronousRendering", 0);

	settings.debug.SerialConsole	= cfgLoadInt("config", "Debug.SerialConsoleEnabled", 0) != 0;

	settings.bios.UseReios		= cfgLoadInt("config", "bios.UseReios", 0);
	settings.reios.ElfFile		= cfgLoadStr("reios", "ElfFile", "");

	settings.validate.OpenGlChecks = cfgLoadInt("validate", "OpenGlChecks", 0) != 0;

	settings.input.DCKeyboard = cfgLoadInt("input", "DCKeyboard", 0);
	settings.input.DCMouse = cfgLoadInt("input", "DCMouse", 0);
	settings.input.MouseSensitivity = cfgLoadInt("input", "MouseSensitivity", 100);
#else
    // TODO Expose this with JNI
	settings.rend.Clipping = 1;
	settings.rend.ExtraDepthScale = 1.f;

	// Configured on a per-game basis
	settings.dynarec.safemode	= 0;
#endif

	settings.pvr.HashLogFile	= cfgLoadStr("testing", "ta.HashLogFile", "");
	settings.pvr.HashCheckFile	= cfgLoadStr("testing", "ta.HashCheckFile", "");

#if SUPPORT_DISPMANX
	settings.dispmanx.Width		= cfgLoadInt("dispmanx","width",640);
	settings.dispmanx.Height	= cfgLoadInt("dispmanx","height",480);
	settings.dispmanx.Keep_Aspect	= cfgLoadBool("dispmanx","maintain_aspect",true);
#endif

#if (HOST_OS != OS_LINUX || defined(_ANDROID) || defined(TARGET_PANDORA))
	settings.aica.BufferSize=2048;
#else
	settings.aica.BufferSize=1024;
#endif

#if USE_OMX
	settings.omx.Audio_Latency	= cfgLoadInt("omx","audio_latency",100);
	settings.omx.Audio_HDMI		= cfgLoadBool("omx","audio_hdmi",true);
#endif

/*
	//make sure values are valid
	settings.dreamcast.cable	= min(max(settings.dreamcast.cable,    0),3);
	settings.dreamcast.region	= min(max(settings.dreamcast.region,   0),3);
	settings.dreamcast.broadcast= min(max(settings.dreamcast.broadcast,0),4);
*/
}

void LoadCustom()
{
	char *reios_id = reios_disk_id();

	char *p = reios_id + strlen(reios_id) - 1;
	while (p >= reios_id && *p == ' ')
		*p-- = '\0';
	if (*p == '\0')
		return;

	LoadSpecialSettings();	// Default per-game settings

	if (reios_software_name[0] != '\0')
		cfgSaveStr(reios_id, "software.name", reios_software_name);
	settings.dynarec.Enable		= cfgGameInt(reios_id,"Dynarec.Enabled", settings.dynarec.Enable ? 1 : 0) != 0;
	settings.dynarec.idleskip	= cfgGameInt(reios_id,"Dynarec.idleskip", settings.dynarec.idleskip ? 1 : 0) != 0;
	settings.dynarec.unstable_opt	= cfgGameInt(reios_id,"Dynarec.unstable-opt", settings.dynarec.unstable_opt);
	settings.dynarec.safemode	= cfgGameInt(reios_id,"Dynarec.safe-mode", settings.dynarec.safemode);
	settings.rend.ModifierVolumes	= cfgGameInt(reios_id,"rend.ModifierVolumes", settings.rend.ModifierVolumes);
	settings.rend.Clipping		= cfgGameInt(reios_id,"rend.Clipping", settings.rend.Clipping);

	settings.pvr.subdivide_transp	= cfgGameInt(reios_id,"pvr.Subdivide", settings.pvr.subdivide_transp);

	settings.pvr.ta_skip		= cfgGameInt(reios_id,"ta.skip", settings.pvr.ta_skip);
	settings.pvr.rend		= cfgGameInt(reios_id,"pvr.rend", settings.pvr.rend);

	settings.pvr.MaxThreads		= cfgGameInt(reios_id, "pvr.MaxThreads", settings.pvr.MaxThreads);
	settings.pvr.SynchronousRender	= cfgGameInt(reios_id, "pvr.SynchronousRendering", settings.pvr.SynchronousRender);
	settings.dreamcast.cable = cfgGameInt(reios_id, "Dreamcast.Cable", settings.dreamcast.cable);
	settings.dreamcast.region = cfgGameInt(reios_id, "Dreamcast.Region", settings.dreamcast.region);
	settings.dreamcast.broadcast = cfgGameInt(reios_id, "Dreamcast.Broadcast", settings.dreamcast.broadcast);
}

void SaveSettings()
{
	cfgSaveInt("config","Dynarec.Enabled",		settings.dynarec.Enable);
	cfgSaveInt("config","Dreamcast.Cable",		settings.dreamcast.cable);
	cfgSaveInt("config","Dreamcast.RTC",		settings.dreamcast.RTC);
	cfgSaveInt("config","Dreamcast.Region",		settings.dreamcast.region);
	cfgSaveInt("config","Dreamcast.Broadcast",	settings.dreamcast.broadcast);
}

bool wait_until_dc_running()
{
	int64_t start_time = get_time_usec() ;
	const int64_t FIVE_SECONDS = 5*1000000 ;
	while(!dc_is_running())
	{
		if ( start_time+FIVE_SECONDS < get_time_usec() )
		{
			//timeout elapsed - dc not getting a chance to run - just bail
			return false ;
		}
	}
	return true ;
}

bool acquire_mainloop_lock()
{
	bool result = false ;
	int64_t start_time = get_time_usec() ;
	const int64_t FIVE_SECONDS = 5*1000000 ;

	while ( ( start_time+FIVE_SECONDS > get_time_usec() ) && !(result = mtx_mainloop.TryLock())  )
	{
		rend_cancel_emu_wait() ;
	}

	return result ;
}

void cleanup_serialize(void *data)
{
	if ( data != NULL )
		free(data) ;

	performed_serialization = true ;
	dc_start() ;
	mtx_serialization.Unlock() ;
	mtx_mainloop.Unlock() ;

}

static string get_savestate_file_path()
{
	char image_path[512];
	cfgLoadStr("config", "image", image_path, "./");
	string state_file = image_path;
	size_t lastindex = state_file.find_last_of("/");
	if (lastindex != -1)
		state_file = state_file.substr(lastindex + 1);
	lastindex = state_file.find_last_of(".");
	if (lastindex != -1)
		state_file = state_file.substr(0, lastindex);
	state_file = state_file + ".state";
	return get_writable_data_path("/data/") + state_file;
}

void* dc_savestate_thread(void* p)
{
	string filename;
	unsigned int total_size = 0 ;
	void *data = NULL ;
	void *data_ptr = NULL ;
	FILE *f ;

	mtx_serialization.Lock() ;
	if ( !wait_until_dc_running()) {
		printf("Failed to save state - dc loop kept running\n") ;
    	mtx_serialization.Unlock() ;
    	return NULL;
	}

	dc_stop() ;

	if ( !acquire_mainloop_lock() )
	{
		printf("Failed to save state - could not acquire main loop lock\n") ;
		performed_serialization = true ;
		dc_start() ;
		mtx_serialization.Unlock() ;
    	return NULL;
	}

	if ( ! dc_serialize(&data, &total_size) )
	{
		printf("Failed to save state - could not initialize total size\n") ;
		cleanup_serialize(data) ;
    	return NULL;
	}

	data = malloc(total_size) ;
	if ( data == NULL )
	{
		printf("Failed to save state - could not malloc %d bytes", total_size) ;
		cleanup_serialize(data) ;
    	return NULL;
	}

	data_ptr = data ;

	if ( ! dc_serialize(&data_ptr, &total_size) )
	{
		printf("Failed to save state - could not serialize data\n") ;
		cleanup_serialize(data) ;
    	return NULL;
	}

	filename = get_savestate_file_path();
	f = fopen(filename.c_str(), "wb") ;

	if ( f == NULL )
	{
		printf("Failed to save state - could not open %s for writing\n", filename.c_str()) ;
		cleanup_serialize(data) ;
    	return NULL;
	}

	fwrite(data, 1, total_size, f) ;
	fclose(f);

	cleanup_serialize(data) ;
	printf("Saved state to %s\n size %d", filename.c_str(), total_size) ;

	return NULL;
}

void* dc_loadstate_thread(void* p)
{
	string filename;
	unsigned int total_size = 0 ;
	void *data = NULL ;
	void *data_ptr = NULL ;
	FILE *f ;

	mtx_serialization.Lock() ;
	if ( !wait_until_dc_running()) {
		printf("Failed to load state - dc loop kept running\n") ;
    	mtx_serialization.Unlock() ;
    	return NULL;
	}

	dc_stop() ;

	if ( !acquire_mainloop_lock() )
	{
		printf("Failed to load state - could not acquire main loop lock\n") ;
		performed_serialization = true ;
		dc_start() ;
		mtx_serialization.Unlock() ;
    	return NULL;
	}

	if ( ! dc_serialize(&data, &total_size) )
	{
		printf("Failed to load state - could not initialize total size\n") ;
		cleanup_serialize(data) ;
    	return NULL;
	}

	data = malloc(total_size) ;
	if ( data == NULL )
	{
		printf("Failed to load state - could not malloc %d bytes", total_size) ;
		cleanup_serialize(data) ;
    	return NULL;
	}

	filename = get_savestate_file_path();
	f = fopen(filename.c_str(), "rb") ;

	if ( f == NULL )
	{
		printf("Failed to load state - could not open %s for reading\n", filename.c_str()) ;
		cleanup_serialize(data) ;
    	return NULL;
	}

	fread(data, 1, total_size, f) ;
	fclose(f);


	data_ptr = data ;

    bm_Reset() ;

	if ( ! dc_unserialize(&data_ptr, &total_size) )
	{
		printf("Failed to load state - could not unserialize data\n") ;
		cleanup_serialize(data) ;
    	return NULL;
	}

	cleanup_serialize(data) ;
	printf("Loaded state from %s size %d\n", filename.c_str(), total_size) ;
	rend_cancel_emu_wait();

	return NULL;
}


void dc_savestate()
{
	cThread thd(dc_savestate_thread,0);
	thd.Start() ;
}

void dc_loadstate()
{
	cThread thd(dc_loadstate_thread,0);
	thd.Start() ;
}
