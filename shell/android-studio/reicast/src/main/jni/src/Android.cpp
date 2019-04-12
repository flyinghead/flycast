#include <jni.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>
#include <unistd.h>
#include <stdlib.h>

#include "types.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <types.h>

#include "hw/maple/maple_cfg.h"
#include "hw/pvr/Renderer_if.h"
#include "profiler/profiler.h"
#include "rend/TexCache.h"
#include "rend/gles/gles.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "hw/naomi/naomi_cart.h"
#include "oslib/audiostream.h"
#include "imgread/common.h"
#include "rend/gui.h"
#include "cfg/cfg.h"

JavaVM* g_jvm;

// Convenience class to get the java environment for the current thread.
// Also attach the threads, and detach it on destruction, if needed.
class JVMAttacher {
public:
    JVMAttacher() : _env(NULL), _detach_thread(false) {
    }
    JNIEnv *getEnv()
    {
        if (_env == NULL)
        {
            if (g_jvm == NULL) {
                die("g_jvm == NULL");
                return NULL;
            }
            int rc = g_jvm->GetEnv((void **)&_env, JNI_VERSION_1_6);
            if (rc  == JNI_EDETACHED) {
                if (g_jvm->AttachCurrentThread(&_env, NULL) != 0) {
                    die("AttachCurrentThread failed");
                    return NULL;
                }
                _detach_thread = true;
            }
            else if (rc == JNI_EVERSION) {
                die("JNI version error");
                return NULL;
            }
        }
        return _env;
    }

    ~JVMAttacher()
    {
        if (_detach_thread)
            g_jvm->DetachCurrentThread();
    }

private:
    JNIEnv *_env;
    bool _detach_thread;
};
static thread_local JVMAttacher jvm_attacher;

#include "android_gamepad.h"

#define SETTINGS_ACCESSORS(jsetting, csetting, type)                                                                                                    \
JNIEXPORT type JNICALL Java_com_reicast_emulator_emu_JNIdc_get ## jsetting(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));           \
JNIEXPORT type JNICALL Java_com_reicast_emulator_emu_JNIdc_get ## jsetting(JNIEnv *env, jobject obj)                                                    \
{                                                                                                                                                       \
    return settings.csetting;                                                                                                                           \
}

extern "C"
{
JNIEXPORT jstring JNICALL Java_com_reicast_emulator_emu_JNIdc_initEnvironment(JNIEnv *env, jobject obj, jobject emulator, jstring homeDirectory)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setExternalStorageDirectories(JNIEnv *env, jobject obj, jobjectArray pathList)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setGameUri(JNIEnv *env,jobject obj,jstring fileName)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pause(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_resume(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_destroy(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_send(JNIEnv *env,jobject obj,jint id, jint v)  __attribute__((visibility("default")));
JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_data(JNIEnv *env,jobject obj,jint id, jbyteArray d)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitNative(JNIEnv *env, jobject obj, jobject surface)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitJava(JNIEnv *env, jobject obj, jint w, jint h)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframeJava(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendtermJava(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vjoy(JNIEnv * env, jobject obj,int id,float x, float y, float w, float h)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_hideOsd(JNIEnv * env, jobject obj)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_getControllers(JNIEnv *env, jobject obj, jintArray controllers, jobjectArray peripherals)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)  __attribute__((visibility("default")));

SETTINGS_ACCESSORS(Nosound, aica.NoSound, jboolean)
SETTINGS_ACCESSORS(Widescreen, rend.WideScreen, jboolean)
SETTINGS_ACCESSORS(VirtualGamepadVibration, input.VirtualGamepadVibration, jint);

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_screenDpi(JNIEnv *env,jobject obj, jint screenDpi)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_guiOpenSettings(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsOpen(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsContentBrowser(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_init(JNIEnv *env, jobject obj) __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAdded(JNIEnv *env, jobject obj, jint id, jstring name, jint maple_port, jstring junique_id)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickRemoved(JNIEnv *env, jobject obj, jint id)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_virtualGamepadEvent(JNIEnv *env, jobject obj, jint kcode, jint joyx, jint joyy, jint lt, jint rt)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickButtonEvent(JNIEnv *env, jobject obj, jint id, jint key, jboolean pressed)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAxisEvent(JNIEnv *env, jobject obj, jint id, jint key, jint value) __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_mouseEvent(JNIEnv *env, jobject obj, jint xpos, jint ypos, jint buttons) __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_AudioBackend_setInstance(JNIEnv *env, jobject obj, jobject instance) __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_BaseGLActivity_register(JNIEnv *env, jobject obj, jobject activity) __attribute__((visibility("default")));
};

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_screenDpi(JNIEnv *env,jobject obj, jint screenDpi)
{
    screen_dpi = screenDpi;
}

int reicast_init(int argc, char* argv[]);
void dc_resume();
void dc_stop();
void dc_term();

bool egl_makecurrent();

extern int screen_width,screen_height;

static char gamedisk[256];

u16 kcode[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];
float vjoy_pos[14][8];

extern s32 mo_x_abs;
extern s32 mo_y_abs;
extern u32 mo_buttons;

extern bool print_stats;

//stuff for saving prefs
jobject g_emulator;
jmethodID saveAndroidSettingsMid;
static ANativeWindow *g_window = 0;

void os_DoEvents()
{
    // @@@ Nothing here yet
}

void os_CreateWindow()
{
}

//
// Platform-specific NullDC functions
//


void UpdateInputState(u32 Port)
{
    // @@@ Nothing here yet
}

void *libPvr_GetRenderTarget()
{
    return g_window;    // the surface to render to
}

void *libPvr_GetRenderSurface()
{
    return NULL;    // default display
}

void common_linux_setup();

void os_SetupInput()
{
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
    mcfg_CreateDevices();
#endif
}

void os_SetWindowText(char const *Text)
{
    putinf("%s",Text);
}

JNIEXPORT jstring JNICALL Java_com_reicast_emulator_emu_JNIdc_initEnvironment(JNIEnv *env, jobject obj, jobject emulator, jstring homeDirectory)
{
    // Initialize platform-specific stuff
    common_linux_setup();

    bool first_init = false;

    // Keep reference to global JVM and Emulator objects
    if (g_jvm == NULL)
    {
        first_init = true;
        env->GetJavaVM(&g_jvm);
    }
    if (g_emulator == NULL) {
        g_emulator = env->NewGlobalRef(emulator);
        saveAndroidSettingsMid = env->GetMethodID(env->GetObjectClass(emulator), "SaveAndroidSettings", "(Ljava/lang/String;)V");
    }
    // Set home directory based on User config
    const char* path = homeDirectory != NULL ? env->GetStringUTFChars(homeDirectory, 0) : "";
    set_user_config_dir(path);
    set_user_data_dir(path);
    printf("Config dir is: %s\n", get_writable_config_path("").c_str());
    printf("Data dir is:   %s\n", get_writable_data_path("").c_str());
    if (homeDirectory != NULL)
    	env->ReleaseStringUTFChars(homeDirectory, path);

    if (first_init)
    {
        // Do one-time initialization
        jstring msg = NULL;
        int rc = reicast_init(0, NULL);
        if (rc == -4)
            msg = env->NewStringUTF("Cannot find configuration");
        else if (rc == 69)
            msg = env->NewStringUTF("Invalid command line");
        else if (rc == -1)
            msg = env->NewStringUTF("Memory initialization failed");
        return msg;
    }
    else
        return NULL;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setExternalStorageDirectories(JNIEnv *env, jobject obj, jobjectArray pathList)
{
    std::string paths;
    int obj_len = env->GetArrayLength(pathList);
    for (int i = 0; i < obj_len; ++i) {
        jstring dir = (jstring)env->GetObjectArrayElement(pathList, i);
        const char* p = env->GetStringUTFChars(dir, 0);
        if (!paths.empty())
            paths += ":";
        paths += p;
        env->ReleaseStringUTFChars(dir, p);
        env->DeleteLocalRef(dir);
    }
    setenv("REICAST_HOME", paths.c_str(), 1);
    gui_refresh_files();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_bootdisk(JNIEnv *env,jobject obj, jstring disk) {
    if (disk != NULL) {
        settings.imgread.LoadDefaultImage = true;
        const char *P = env->GetStringUTFChars(disk, 0);
        if (!P) settings.imgread.DefaultImage[0] = '\0';
        else {
            printf("Boot Disk URI: '%s'\n", P);
            strncpy(settings.imgread.DefaultImage,(strlen(P)>=7)&&!memcmp(
                    P,"file://",7)? P+7:P,sizeof(settings.imgread.DefaultImage));
            settings.imgread.DefaultImage[sizeof(settings.imgread.DefaultImage) - 1] = '\0';
        }
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setGameUri(JNIEnv *env,jobject obj,jstring fileName)
{
    if (fileName != NULL)
    {
        // Get filename string from Java
        const char* file_path = env->GetStringUTFChars(fileName, 0);
        printf("Game Disk URI: '%s'\n", file_path);
        strncpy(gamedisk, strlen(file_path) >= 7 && !memcmp(file_path, "file://", 7) ? file_path + 7 : file_path, sizeof(gamedisk));
        gamedisk[sizeof(gamedisk) - 1] = '\0';
        env->ReleaseStringUTFChars(fileName, file_path);

        cfgSetVirtual("config", "image", file_path);
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_diskSwap(JNIEnv *env,jobject obj,jstring disk)
{
    if (settings.imgread.LoadDefaultImage) {
        strncpy(settings.imgread.DefaultImage, gamedisk, sizeof(settings.imgread.DefaultImage));
        settings.imgread.DefaultImage[sizeof(settings.imgread.DefaultImage) - 1] = '\0';
        DiscSwap();
    } else if (disk != NULL) {
        settings.imgread.LoadDefaultImage = true;
        const char *P = env->GetStringUTFChars(disk, 0);
        if (!P) settings.imgread.DefaultImage[0] = '\0';
        else {
            printf("Swap Disk URI: '%s'\n", P);
            strncpy(settings.imgread.DefaultImage,(strlen(P)>=7)&&!memcmp(
                    P,"file://",7)? P+7:P,sizeof(settings.imgread.DefaultImage));
            settings.imgread.DefaultImage[sizeof(settings.imgread.DefaultImage) - 1] = '\0';
            env->ReleaseStringUTFChars(disk, P);
        }
        DiscSwap();
    }
}

//stuff for microphone
jobject sipemu;
jmethodID getmicdata;
extern bool game_started;

//stuff for audio
#define SAMPLE_COUNT 512
jshortArray jsamples;
jmethodID writeBufferMid;
static jobject g_audioBackend;

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)
{
    sipemu = env->NewGlobalRef(sip);
    getmicdata = env->GetMethodID(env->GetObjectClass(sipemu),"getData","()[B");
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pause(JNIEnv *env,jobject obj)
{
    if (game_started)
        dc_stop();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_resume(JNIEnv *env,jobject obj)
{
    if (game_started)
        dc_resume();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)
{
    if (game_started)
        dc_stop();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_destroy(JNIEnv *env,jobject obj)
{
    dc_term();
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
#if FEAT_SHREC != DYNAREC_NONE
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
    return 0;
}

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_data(JNIEnv *env, jobject obj, jint id, jbyteArray d)
{
    if (id==1)
    {
        printf("Loading symtable (%p,%p,%d,%p)\n",env,obj,id,d);
        jsize len=env->GetArrayLength(d);
        u8* syms=(u8*)malloc((size_t)len);
        printf("Loading symtable to %8s, %d\n",syms,len);
        env->GetByteArrayRegion(d,0,len,(jbyte*)syms);
        sample_Syms(syms, (size_t)len);
    }
    return 0;
}

extern void gl_swap();
extern void egl_stealcntx();
volatile static bool render_running;
volatile static bool render_reinit;

void *render_thread_func(void *)
{
	render_running = true;

	rend_init_renderer();

    while (render_running) {
        if (render_reinit)
        {
        	render_reinit = false;
        	rend_init_renderer();
        }
        else
            if (!egl_makecurrent())
                break;;

        bool ret = rend_single_frame();
        if (ret)
            gl_swap();
    }
    egl_makecurrent();
    rend_term_renderer();
    ANativeWindow_release(g_window);
    g_window = NULL;
	render_running = false;

    return NULL;
}

static cThread render_thread(render_thread_func, NULL);

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitNative(JNIEnv * env, jobject obj, jobject surface)
{
	if (render_thread.hThread != NULL)
	{
		if (surface == NULL)
		{
			render_running = false;
	        render_thread.WaitToEnd();
		}
		else
			render_reinit = true;
	}
	else if (surface != NULL)
	{
        g_window = ANativeWindow_fromSurface(env, surface);
        render_thread.Start();
	}
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitJava(JNIEnv * env, jobject obj, jint width, jint height)
{
    screen_width = width;
    screen_height = height;
    egl_stealcntx();
    rend_init_renderer();
}

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframeJava(JNIEnv *env,jobject obj)
{
    egl_stealcntx();
    return (jboolean)rend_single_frame();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendtermJava(JNIEnv * env, jobject obj)
{
    egl_stealcntx();
    rend_term_renderer();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vjoy(JNIEnv * env, jobject obj,int id,float x, float y, float w, float h)
{
    if(id<sizeof(vjoy_pos)/sizeof(vjoy_pos[0]))
    {
        vjoy_pos[id][0] = x;
        vjoy_pos[id][1] = y;
        vjoy_pos[id][2] = w;
        vjoy_pos[id][3] = h;
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_hideOsd(JNIEnv * env, jobject obj) {
    HideOSD();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_getControllers(JNIEnv *env, jobject obj, jintArray controllers, jobjectArray peripherals)
{
    jint *controllers_body = env->GetIntArrayElements(controllers, 0);
    memcpy(controllers_body, settings.input.maple_devices, sizeof(settings.input.maple_devices));
    env->ReleaseIntArrayElements(controllers, controllers_body, 0);

    int obj_len = env->GetArrayLength(peripherals);
    for (int i = 0; i < obj_len; ++i) {
        jintArray port = (jintArray) env->GetObjectArrayElement(peripherals, i);
        jint *items = env->GetIntArrayElements(port, 0);
        items[0] = settings.input.maple_expansion_devices[i][0];
        items[1] = settings.input.maple_expansion_devices[i][1];
        env->ReleaseIntArrayElements(port, items, 0);
        env->DeleteLocalRef(port);
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_guiOpenSettings(JNIEnv *env, jobject obj)
{
    gui_open_settings();
}

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsOpen(JNIEnv *env, jobject obj)
{
    return gui_is_open();
}

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsContentBrowser(JNIEnv *env,jobject obj)
{
    return gui_is_content_browser();
}

// Audio Stuff
u32 androidaudio_push(void* frame, u32 amt, bool wait)
{
    verify(amt==SAMPLE_COUNT);
    //yeah, do some audio piping magic here !
    jvm_attacher.getEnv()->SetShortArrayRegion(jsamples, 0, amt * 2, (jshort *)frame);
    return jvm_attacher.getEnv()->CallIntMethod(g_audioBackend, writeBufferMid, jsamples, wait);
}

void androidaudio_init()
{
    // Nothing to do here...
}

void androidaudio_term()
{
    // Move along, there is nothing to see here!
}

audiobackend_t audiobackend_android = {
        "android", // Slug
        "Android Audio", // Name
        &androidaudio_init,
        &androidaudio_push,
        &androidaudio_term
};

static bool android = RegisterAudioBackend(&audiobackend_android);


JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_AudioBackend_setInstance(JNIEnv *env, jobject obj, jobject instance)
{
    if (g_audioBackend != NULL)
        env->DeleteGlobalRef(g_audioBackend);
    if (instance == NULL) {
        g_audioBackend = NULL;
        if (jsamples != NULL) {
            env->DeleteGlobalRef(jsamples);
            jsamples = NULL;
        }
    }
    else {
        g_audioBackend = env->NewGlobalRef(instance);
        writeBufferMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "writeBuffer", "([SZ)I");
        if (jsamples == NULL) {
            jsamples = env->NewShortArray(SAMPLE_COUNT * 2);
            jsamples = (jshortArray) env->NewGlobalRef(jsamples);
        }
    }
}

int get_mic_data(u8* buffer)
{
    jbyteArray jdata = (jbyteArray)jvm_attacher.getEnv()->CallObjectMethod(sipemu,getmicdata);
    if(jdata==NULL){
        //LOGW("get_mic_data NULL");
        return 0;
    }
    jvm_attacher.getEnv()->GetByteArrayRegion(jdata, 0, SIZE_OF_MIC_DATA, (jbyte*)buffer);
    jvm_attacher.getEnv()->DeleteLocalRef(jdata);
    return 1;
}

void os_DebugBreak()
{
    // TODO: notify the parent thread about it ...

	raise(SIGABRT);
    //pthread_exit(NULL);

    // Attach debugger here to figure out what went wrong
    for(;;) ;
}

void SaveAndroidSettings()
{
    jstring homeDirectory = jvm_attacher.getEnv()->NewStringUTF(get_writable_config_path("").c_str());

    jvm_attacher.getEnv()->CallVoidMethod(g_emulator, saveAndroidSettingsMid, homeDirectory);
    jvm_attacher.getEnv()->DeleteLocalRef(homeDirectory);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_init(JNIEnv *env, jobject obj)
{
    input_device_manager = env->NewGlobalRef(obj);
    input_device_manager_rumble = env->GetMethodID(env->GetObjectClass(obj), "rumble", "(IFFI)Z");
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAdded(JNIEnv *env, jobject obj, jint id, jstring name, jint maple_port, jstring junique_id)
{
    const char* joyname = env->GetStringUTFChars(name,0);
    const char* unique_id = env->GetStringUTFChars(junique_id, 0);
    std::shared_ptr<AndroidGamepadDevice> gamepad = std::make_shared<AndroidGamepadDevice>(maple_port, id, joyname, unique_id);
    AndroidGamepadDevice::AddAndroidGamepad(gamepad);
    env->ReleaseStringUTFChars(name, joyname);
    env->ReleaseStringUTFChars(name, unique_id);
}
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickRemoved(JNIEnv *env, jobject obj, jint id)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    if (device != NULL)
        AndroidGamepadDevice::RemoveAndroidGamepad(device);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_virtualGamepadEvent(JNIEnv *env, jobject obj, jint kcode, jint joyx, jint joyy, jint lt, jint rt)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(AndroidGamepadDevice::VIRTUAL_GAMEPAD_ID);
    if (device != NULL)
        device->virtual_gamepad_event(kcode, joyx, joyy, lt, rt);
}

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickButtonEvent(JNIEnv *env, jobject obj, jint id, jint key, jboolean pressed)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    if (device != NULL)
        return device->gamepad_btn_input(key, pressed);
    else
    	return false;

}

static std::map<std::pair<jint, jint>, jint> previous_axis_values;

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAxisEvent(JNIEnv *env, jobject obj, jint id, jint key, jint value)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    // Only handle Left Stick on an Xbox 360 controller if there was actual
    // motion on the stick, otherwise event can be handled as a DPAD event
    if (device != NULL && previous_axis_values[std::make_pair(id, key)] != value)
    {
    	previous_axis_values[std::make_pair(id, key)] = value;
    	return device->gamepad_axis_input(key, value);
    }
    else
    	return false;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_mouseEvent(JNIEnv *env, jobject obj, jint xpos, jint ypos, jint buttons)
{
    mo_x_abs = xpos;
    mo_y_abs = ypos;
    mo_buttons = 0xFFFF;
    if (buttons & 1)	// Left
    	mo_buttons &= ~4;
    if (buttons & 2)	// Right
    	mo_buttons &= ~2;
    if (buttons & 4)	// Middle
    	mo_buttons &= ~8;
    mouse_gamepad.gamepad_btn_input(1, (buttons & 1) != 0);
    mouse_gamepad.gamepad_btn_input(2, (buttons & 2) != 0);
    mouse_gamepad.gamepad_btn_input(4, (buttons & 4) != 0);
}

static jobject g_activity;
static jmethodID VJoyStartEditingMID;
static jmethodID VJoyStopEditingMID;
static jmethodID VJoyResetEditingMID;

JNIEXPORT void JNICALL Java_com_reicast_emulator_BaseGLActivity_register(JNIEnv *env, jobject obj, jobject activity)
{
    if (g_activity != NULL)
    {
        env->DeleteGlobalRef(g_activity);
        g_activity = NULL;
    }
    if (activity != NULL) {
        g_activity = env->NewGlobalRef(activity);
        VJoyStartEditingMID = env->GetMethodID(env->GetObjectClass(activity), "VJoyStartEditing", "()V");
        VJoyStopEditingMID = env->GetMethodID(env->GetObjectClass(activity), "VJoyStopEditing", "(Z)V");
        VJoyResetEditingMID = env->GetMethodID(env->GetObjectClass(activity), "VJoyResetEditing", "()V");
    }
}

void vjoy_start_editing()
{
    jvm_attacher.getEnv()->CallVoidMethod(g_activity, VJoyStartEditingMID);
}

void vjoy_reset_editing()
{
    jvm_attacher.getEnv()->CallVoidMethod(g_activity, VJoyResetEditingMID);
}

void vjoy_stop_editing(bool canceled)
{
    jvm_attacher.getEnv()->CallVoidMethod(g_activity, VJoyStopEditingMID, canceled);
}

void android_send_logs()
{
    JNIEnv *env = jvm_attacher.getEnv();
    jmethodID generateErrorLogMID = env->GetMethodID(env->GetObjectClass(g_activity), "generateErrorLog", "()V");
    env->CallVoidMethod(g_activity, generateErrorLogMID);
}
