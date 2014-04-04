#include <jni.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>  
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "types.h"
#include "profiler/profiler.h"
#include "cfg/cfg.h"
#include "rend/TexCache.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"

#include "util.h"

extern "C"
{
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_config(JNIEnv *env,jobject obj,jstring dirName)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_init(JNIEnv *env,jobject obj,jstring fileName)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_run(JNIEnv *env,jobject obj,jobject track)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

  JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_send(JNIEnv *env,jobject obj,jint id, jint v)  __attribute__((visibility("default")));
  JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_data(JNIEnv *env,jobject obj,jint id, jbyteArray d)  __attribute__((visibility("default")));

  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinit(JNIEnv *env,jobject obj,jint w,jint h)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframe(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_kcode(JNIEnv * env, jobject obj, jintArray k_code, jintArray l_t, jintArray r_t, jintArray jx, jintArray jy)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vjoy(JNIEnv * env, jobject obj,u32 id,float x, float y, float w, float h)  __attribute__((visibility("default")));
  //JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_play(JNIEnv *env,jobject obj,jshortArray result,jint size);

  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_initControllers(JNIEnv *env, jobject obj, jbooleanArray controllers)  __attribute__((visibility("default")));
  
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vmuSwap(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupVmu(JNIEnv *env,jobject obj,jobject sip)  __attribute__((visibility("default")));

    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_dynarec(JNIEnv *env,jobject obj, jint dynarec)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_idleskip(JNIEnv *env,jobject obj, jint idleskip)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_unstable(JNIEnv *env,jobject obj, jint unstable)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_cable(JNIEnv *env,jobject obj, jint cable)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_region(JNIEnv *env,jobject obj, jint region)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_broadcast(JNIEnv *env,jobject obj, jint broadcast)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_limitfps(JNIEnv *env,jobject obj, jint limiter)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_nobatch(JNIEnv *env,jobject obj, jint nobatch)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_nosound(JNIEnv *env,jobject obj, jint noaudio)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_mipmaps(JNIEnv *env,jobject obj, jint mipmaps)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_widescreen(JNIEnv *env,jobject obj, jint stretch)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_subdivide(JNIEnv *env,jobject obj, jint subdivide)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_frameskip(JNIEnv *env,jobject obj, jint frames)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pvrrender(JNIEnv *env,jobject obj, jint render)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_cheatdisk(JNIEnv *env,jobject obj, jstring disk)  __attribute__((visibility("default")));
    JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_dreamtime(JNIEnv *env,jobject obj, jlong clock)  __attribute__((visibility("default")));
};

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_dynarec(JNIEnv *env,jobject obj, jint dynarec)
{
    settings.dynarec.Enable = dynarec;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_idleskip(JNIEnv *env,jobject obj, jint idleskip)
{
    settings.dynarec.idleskip = idleskip;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_unstable(JNIEnv *env,jobject obj, jint unstable)
{
    settings.dynarec.unstable_opt = unstable;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_cable(JNIEnv *env,jobject obj, jint cable)
{
    settings.dreamcast.cable = cable;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_region(JNIEnv *env,jobject obj, jint region)
{
    settings.dreamcast.region = region;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_broadcast(JNIEnv *env,jobject obj, jint broadcast)
{
    settings.dreamcast.broadcast = broadcast;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_limitfps(JNIEnv *env,jobject obj, jint limiter)
{
    settings.aica.LimitFPS = limiter;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_nobatch(JNIEnv *env,jobject obj, jint nobatch)
{
    settings.aica.NoBatch = nobatch;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_nosound(JNIEnv *env,jobject obj, jint noaudio)
{
    settings.aica.NoSound = noaudio;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_mipmaps(JNIEnv *env,jobject obj, jint mipmaps)
{
    settings.rend.UseMipmaps = mipmaps;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_widescreen(JNIEnv *env,jobject obj, jint stretch)
{
    settings.rend.WideScreen = stretch;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_subdivide(JNIEnv *env,jobject obj, jint subdivide)
{
    settings.pvr.subdivide_transp = subdivide;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_frameskip(JNIEnv *env,jobject obj, jint frames)
{
    settings.pvr.ta_skip = frames;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pvrrender(JNIEnv *env,jobject obj, jint render)
{
    settings.pvr.rend = render;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_cheatdisk(JNIEnv *env,jobject obj, jstring disk)
{

}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_dreamtime(JNIEnv *env,jobject obj, jlong clock)
{
    settings.dreamcast.RTC = (u32)(clock);
}

void egl_stealcntx();
void SetApplicationPath(wchar *path);
int dc_init(int argc,wchar* argv[]);
void dc_run();
void dc_term();
void mcfg_Create(MapleDeviceType type,u32 bus,u32 port);

bool VramLockedWrite(u8* address);

bool rend_single_frame();
bool gles_init();

//extern cResetEvent rs,re;
extern int screen_width,screen_height;

static u64 tvs_base;
static char CurFileName[256];

// Additonal controllers 2, 3 and 4 connected ?
static bool add_controllers[3] = { false, false, false };

u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];
float vjoy_pos[14][8];

extern bool print_stats;



void os_DoEvents()
{
  // @@@ Nothing here yet
}

//
// Native thread that runs the actual nullDC emulator
//
static void *ThreadHandler(void *UserData)
{
  char *Args[3];
  const char *P;

  // Make up argument list
  P       = (const char *)UserData;
  Args[0] = "dc";
  Args[1] = "-config";
  Args[2] = P&&P[0]? (char *)malloc(strlen(P)+32):0;

  if(Args[2])
  {
    strcpy(Args[2],"config:image=");
    strcat(Args[2],P);
  }

  // Add additonal controllers
  for (int i = 0; i < 3; i++)
  {
    if (add_controllers[i])
      mcfg_Create(MDT_SegaController,i+1,5);
  }

  // Run nullDC emulator
  dc_init(Args[2]? 3:1,Args);
}

//
// Platform-specific NullDC functions
//

int msgboxf(const wchar* Text,unsigned int Type,...)
{
  wchar S[2048];
  va_list Args;

  va_start(Args,Type);
  vsprintf(S,Text,Args);
  va_end(Args);

  puts(S);
  return(MBX_OK);
}

void UpdateInputState(u32 Port)
{
  // @@@ Nothing here yet
}

void *libPvr_GetRenderTarget() 
{
  // No X11 window in Android 
  return(0);
}

void *libPvr_GetRenderSurface() 
{ 
  // No X11 display in Android 
  return(0);
}

void common_linux_setup();

void os_SetWindowText(char const *Text)
{
	putinf(Text);
}
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_config(JNIEnv *env,jobject obj,jstring dirName)
{
  // Set home directory based on User config
  const char* D = dirName? env->GetStringUTFChars(dirName,0):0;
  SetHomeDir(D);
  printf("Home dir is: '%s'\n",GetPath("/").c_str());
  env->ReleaseStringUTFChars(dirName,D);
}
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_init(JNIEnv *env,jobject obj,jstring fileName)
{
  // Get filename string from Java
  const char* P = fileName? env->GetStringUTFChars(fileName,0):0;
  if(!P) CurFileName[0] = '\0';
  else
  {
    printf("Got URI: '%s'\n",P);
    strncpy(CurFileName,(strlen(P)>=7)&&!memcmp(P,"file://",7)? P+7:P,sizeof(CurFileName));
    CurFileName[sizeof(CurFileName)-1] = '\0';
    env->ReleaseStringUTFChars(fileName,P);
  }

  printf("Opening file: '%s'\n",CurFileName);

  // Initialize platform-specific stuff
  common_linux_setup();

  // Set configuration
  settings.profile.run_counts = 0;
  

/*
  // Start native thread
  pthread_attr_init(&PTAttr);
  pthread_attr_setdetachstate(&PTAttr,PTHREAD_CREATE_DETACHED);
  pthread_create(&PThread,&PTAttr,ThreadHandler,CurFileName);
  pthread_attr_destroy(&PTAttr);
  */

  ThreadHandler(CurFileName);
}

#define SAMPLE_COUNT 512

JNIEnv* jenv; //we are abusing the f*** out of this poor guy
//JavaVM* javaVM = NULL; //this seems like the right way to go
//stuff for audio
jshortArray jsamples;
jmethodID writemid;
jobject track;
//stuff for microphone
jobject sipemu;
jmethodID getmicdata;
//stuff for vmu lcd
jobject vmulcd = NULL;
jbyteArray jpix = NULL;
jmethodID updatevmuscreen;


JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_run(JNIEnv *env,jobject obj,jobject trk)
{
	install_prof_handler(0);

	jenv=env;
	track=trk;

	jsamples=env->NewShortArray(SAMPLE_COUNT*2);
	writemid=env->GetMethodID(env->GetObjectClass(track),"WriteBuffer","([SI)I");

	dc_run();

}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)
{
	sipemu = env->NewGlobalRef(sip);
	getmicdata = env->GetMethodID(env->GetObjectClass(sipemu),"getData","()[B");	
	delete MapleDevices[0][1];
	mcfg_Create(MDT_Microphone,0,1);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupVmu(JNIEnv *env,jobject obj,jobject vmu)
{
	//env->GetJavaVM(&javaVM);
	vmulcd = env->NewGlobalRef(vmu);
	updatevmuscreen = env->GetMethodID(env->GetObjectClass(vmu),"updateBytes","([B)V");
	//jpix=env->NewByteArray(1536);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)
{
	dc_term();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vmuSwap(JNIEnv *env,jobject obj)
{
	maple_device* olda = MapleDevices[0][0];
	maple_device* oldb = MapleDevices[0][1];
	MapleDevices[0][0] = NULL;
	MapleDevices[0][1] = NULL;
	usleep(50000);//50 ms, wait for host to detect disconnect

	MapleDevices[0][0] = oldb;
	MapleDevices[0][1] = olda;
}

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_send(JNIEnv *env,jobject obj,jint cmd, jint param)
{
	if (cmd==0)
	{
		if (param==0)
		{
			KillTex=true;
			printf("Killing texture cache\n");
		}

		if (param==1)
		{
	  settings.pvr.ta_skip^=1;
	  printf("settings.pvr.ta_skip: %d\n",settings.pvr.ta_skip);
		}
		if (param==2)
		{
		#if !defined(HOST_NO_REC)
			print_stats=true;
			printf("Storing blocks ...\n");
		#endif
		}
	}
	else if (cmd==1)
	{
    if (param==0)
      sample_Stop();
    else
      sample_Start(param);
	}
	else if (cmd==2)
	{
	}
}

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_data(JNIEnv *env,jobject obj,jint id, jbyteArray d)
{
	if (id==1)
	{
		printf("Loading symtable (%p,%p,%p,%p)\n",env,obj,id,d);
		int len=env->GetArrayLength(d);
		u8* syms=(u8*)malloc(len);
		printf("Loading symtable to %p, %d\n",syms,len);
		env->GetByteArrayRegion(d,0,len,(jbyte*)syms);
		sample_Syms(syms,len);
	}
}


JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframe(JNIEnv *env,jobject obj)
{
	while(!rend_single_frame()) ;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_kcode(JNIEnv * env, jobject obj, jintArray k_code, jintArray l_t, jintArray r_t, jintArray jx, jintArray jy)
{
	jint *k_code_body = env->GetIntArrayElements(k_code, 0);
	jint *l_t_body = env->GetIntArrayElements(l_t, 0);
	jint *r_t_body = env->GetIntArrayElements(r_t, 0);
	jint *jx_body = env->GetIntArrayElements(jx, 0);
	jint *jy_body = env->GetIntArrayElements(jy, 0);

	for(int i = 0; i < 4; i++)
	{
		kcode[i] = k_code_body[i];	
		lt[i] = l_t_body[i];
		rt[i] = r_t_body[i];
		joyx[i] = jx_body[i];
		joyy[i] = jy_body[i];
	}

	env->ReleaseIntArrayElements(k_code, k_code_body, 0);
	env->ReleaseIntArrayElements(l_t, l_t_body, 0);
	env->ReleaseIntArrayElements(r_t, r_t_body, 0);
	env->ReleaseIntArrayElements(jx, jx_body, 0);
	env->ReleaseIntArrayElements(jy, jy_body, 0);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinit(JNIEnv * env, jobject obj, jint w,jint h)
{             
  screen_width  = w;
  screen_height = h;

  //gles_term();

  egl_stealcntx();

  if (!gles_init())
	die("OPENGL FAILED");

  install_prof_handler(1);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vjoy(JNIEnv * env, jobject obj,u32 id,float x, float y, float w, float h)
{
  if(id<sizeof(vjoy_pos)/sizeof(vjoy_pos[0]))
  {
    vjoy_pos[id][0] = x;
    vjoy_pos[id][1] = y;
	vjoy_pos[id][2] = w;
	vjoy_pos[id][3] = h;
  }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_initControllers(JNIEnv *env, jobject obj, jbooleanArray controllers)
{
	jboolean *controllers_body = env->GetBooleanArrayElements(controllers, 0);
	memcpy(add_controllers, controllers_body, 3);
	env->ReleaseBooleanArrayElements(controllers, controllers_body, 0);
}

u32 os_Push(void* frame, u32 amt, bool wait)
{
	verify(amt==SAMPLE_COUNT);
	//yeah, do some audio piping magic here !
	jenv->SetShortArrayRegion(jsamples,0,amt*2,(jshort*)frame);
	return jenv->CallIntMethod(track,writemid,jsamples,wait);
}

bool os_IsAudioBuffered()
{
    return jenv->CallIntMethod(track,writemid,jsamples,-1)==0;
}

int get_mic_data(u8* buffer)
{
	jbyteArray jdata = (jbyteArray)jenv->CallObjectMethod(sipemu,getmicdata);
	if(jdata==NULL){
		//LOGW("get_mic_data NULL");
		return 0;
	}
	jenv->GetByteArrayRegion(jdata, 0, SIZE_OF_MIC_DATA, (jbyte*)buffer);
	jenv->DeleteLocalRef(jdata);
	return 1;
}

int push_vmu_screen(u8* buffer)
{
	if(vmulcd==NULL){
		return 0;
	}
	JNIEnv *env = jenv;
	//javaVM->AttachCurrentThread(&env, NULL);
	if(jpix==NULL){
		jpix=env->NewByteArray(1536);
	}
	env->SetByteArrayRegion(jpix,0,1536,(jbyte*)buffer);
	env->CallVoidMethod(vmulcd,updatevmuscreen,jpix);
	return 1;
}
