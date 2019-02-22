#include <jni.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <types.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "hw/maple/maple_cfg.h"
#include "profiler/profiler.h"
#include "rend/TexCache.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "hw/naomi/naomi_cart.h"
#include "oslib/audiobackend_android.h"
#include "reios/reios.h"
#include "imgread/common.h"
#include "rend/gui.h"
#include "android_gamepad.h"

#define SETTINGS_ACCESSORS(jsetting, csetting, type)                                                                                                    \
JNIEXPORT type JNICALL Java_com_reicast_emulator_emu_JNIdc_get ## jsetting(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));           \
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_set ## jsetting(JNIEnv *env, jobject obj, type v)  __attribute__((visibility("default")));   \
JNIEXPORT type JNICALL Java_com_reicast_emulator_emu_JNIdc_get ## jsetting(JNIEnv *env, jobject obj)                                                    \
{                                                                                                                                                       \
    return settings.csetting;                                                                                                                           \
}                                                                                                                                                       \
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_set ## jsetting(JNIEnv *env, jobject obj, type v)                                            \
{                                                                                                                                                       \
    /* settings.csetting = v; */                                                                                                                        \
}

extern "C"
{
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_initEnvironment(JNIEnv *env, jobject obj, jobject emulator)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_config(JNIEnv *env,jobject obj,jstring dirName)  __attribute__((visibility("default")));
JNIEXPORT jstring JNICALL Java_com_reicast_emulator_emu_JNIdc_init(JNIEnv *env,jobject obj,jstring fileName)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_query(JNIEnv *env,jobject obj,jobject emu_thread)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_run(JNIEnv *env,jobject obj,jobject emu_thread)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pause(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_resume(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_destroy(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_send(JNIEnv *env,jobject obj,jint id, jint v)  __attribute__((visibility("default")));
JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_data(JNIEnv *env,jobject obj,jint id, jbyteArray d)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitNative(JNIEnv *env, jobject obj, jobject surface, jint w, jint h)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframeNative(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitJava(JNIEnv *env, jobject obj, jint w, jint h)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframeJava(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendtermJava(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vjoy(JNIEnv * env, jobject obj,u32 id,float x, float y, float w, float h)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_initControllers(JNIEnv *env, jobject obj, jintArray controllers, jobjectArray peripherals)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_getControllers(JNIEnv *env, jobject obj, jintArray controllers, jobjectArray peripherals)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_diskSwap(JNIEnv *env,jobject obj,jstring disk)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vmuSwap(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupVmu(JNIEnv *env,jobject obj,jobject sip)  __attribute__((visibility("default")));

SETTINGS_ACCESSORS(Dynarec, dynarec.Enable, jboolean)
SETTINGS_ACCESSORS(Idleskip, dynarec.idleskip, jboolean)
SETTINGS_ACCESSORS(Unstable, dynarec.unstable_opt, jboolean)
SETTINGS_ACCESSORS(Safemode, dynarec.safemode, jboolean)
SETTINGS_ACCESSORS(Cable, dreamcast.cable, jint)
SETTINGS_ACCESSORS(Region, dreamcast.region, jint)
SETTINGS_ACCESSORS(Broadcast, dreamcast.broadcast, jint)
SETTINGS_ACCESSORS(Language, dreamcast.language, jint)
SETTINGS_ACCESSORS(Limitfps, aica.LimitFPS, jboolean)
SETTINGS_ACCESSORS(Nobatch, aica.NoBatch, jboolean)
SETTINGS_ACCESSORS(Nosound, aica.NoSound, jboolean)
SETTINGS_ACCESSORS(Mipmaps, rend.UseMipmaps, jboolean)
SETTINGS_ACCESSORS(Widescreen, rend.WideScreen, jboolean)
SETTINGS_ACCESSORS(Frameskip, pvr.ta_skip, jint)
SETTINGS_ACCESSORS(Pvrrender, pvr.rend, jint)
SETTINGS_ACCESSORS(Syncedrender, pvr.SynchronousRender, jboolean)
SETTINGS_ACCESSORS(Modvols, rend.ModifierVolumes, jboolean)
SETTINGS_ACCESSORS(Clipping, rend.Clipping, jboolean)
SETTINGS_ACCESSORS(Usereios, bios.UseReios, jboolean)
SETTINGS_ACCESSORS(Customtextures, rend.CustomTextures, jboolean)
SETTINGS_ACCESSORS(Showfps, rend.ShowFPS, jboolean)
SETTINGS_ACCESSORS(RenderToTextureBuffer, rend.RenderToTextureBuffer, jboolean)
SETTINGS_ACCESSORS(RenderToTextureUpscale, rend.RenderToTextureUpscale, jint)
SETTINGS_ACCESSORS(TextureUpscale, rend.TextureUpscale, jint)
SETTINGS_ACCESSORS(MaxFilteredTextureSize, rend.MaxFilteredTextureSize, jint)
SETTINGS_ACCESSORS(MaxThreads, pvr.MaxThreads, jint)

JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_getBootdisk(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_bootdisk(JNIEnv *env,jobject obj, jstring disk)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_screenDpi(JNIEnv *env,jobject obj, jint screenDpi)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_guiOpenSettings(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsOpen(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAdded(JNIEnv *env, jobject obj, jint id, jstring name, jint maple_port)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickRemoved(JNIEnv *env, jobject obj, jint id)  __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_virtualGamepadEvent(JNIEnv *env, jobject obj, jint kcode, jint joyx, jint joyy, jint lt, jint rt)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickButtonEvent(JNIEnv *env, jobject obj, jint id, jint key, jboolean pressed)  __attribute__((visibility("default")));
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAxisEvent(JNIEnv *env, jobject obj, jint id, jint key, jint value) __attribute__((visibility("default")));
JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_mouseEvent(JNIEnv *env, jobject obj, jint xpos, jint ypos, jint buttons) __attribute__((visibility("default")));
};

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_screenDpi(JNIEnv *env,jobject obj, jint screenDpi)
{
    screen_dpi = screenDpi;
}

void SetApplicationPath(wchar *path);
int dc_init(int argc, wchar* argv[]);
void dc_run();
void dc_pause();
void dc_pause_emu();
void dc_resume_emu(bool continue_running);
void dc_stop();
void dc_term();
bool dc_is_running();

bool VramLockedWrite(u8* address);

bool rend_single_frame();
void rend_init_renderer();
void rend_term_renderer();
void rend_cancel_emu_wait();
bool egl_makecurrent();

//extern cResetEvent rs,re;
extern int screen_width,screen_height;

static u64 tvs_base;
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
JavaVM* g_jvm;
jobject g_emulator;
jmethodID saveSettingsMid;
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

MapleDeviceType GetMapleDeviceType(int value)
{
    switch (value)
    {
        case 1:
            return MDT_SegaVMU;
        case 2:
            return MDT_Microphone;
        case 3:
            return MDT_PurupuruPack;
        default:
            return MDT_None;
    }
}

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

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_initEnvironment(JNIEnv *env, jobject obj, jobject emulator)
{
    // Initialize platform-specific stuff
    common_linux_setup();

    // Keep reference to global JVM and Emulator objects
    if (g_jvm == NULL)
        env->GetJavaVM(&g_jvm);
    if (g_emulator == NULL) {
        g_emulator = env->NewGlobalRef(emulator);
        saveSettingsMid = env->GetMethodID(env->GetObjectClass(emulator), "SaveSettings", "()V");
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_config(JNIEnv *env,jobject obj,jstring dirName)
{
    // Set home directory based on User config
    const char* D = dirName? env->GetStringUTFChars(dirName,0):0;
    set_user_config_dir(D);
    set_user_data_dir(D);
    printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
    printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());
    env->ReleaseStringUTFChars(dirName,D);
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

JNIEXPORT jstring JNICALL Java_com_reicast_emulator_emu_JNIdc_init(JNIEnv *env,jobject obj,jstring fileName)
{
    // Get filename string from Java
    const char* P = fileName ? env->GetStringUTFChars(fileName,0) : 0;
    if (!P) gamedisk[0] = '\0';
    else
    {
        printf("Game Disk URI: '%s'\n",P);
        strncpy(gamedisk,(strlen(P)>=7)&&!memcmp(P,"file://",7)? P+7:P,sizeof(gamedisk));
        gamedisk[sizeof(gamedisk)-1] = '\0';
        env->ReleaseStringUTFChars(fileName,P);
    }

    // Set configuration
    settings.profile.run_counts = 0;

    // Make up argument list
    char *args[3];
    args[0] = "dc";
    args[1] = "-config";
    args[2] = gamedisk[0] != 0 ? (char *)malloc(strlen(gamedisk) + 32) : NULL;

    if (args[2] != NULL)
    {
        strcpy(args[2], "config:image=");
        strcat(args[2], gamedisk);
    }

    // Run nullDC emulator
    int rc = dc_init(args[2] ? 3 : 1, args);

    if (args[2] != NULL)
        free(args[2]);

    jstring msg = NULL;
    if (rc == -5)
        msg = env->NewStringUTF("BIOS files cannot be found");
    else if (rc == -4)
        msg = env->NewStringUTF("Cannot find configuration");
    else if (rc == -3)
        msg = env->NewStringUTF("Sound/GPU initialization failed");
    else if (rc == 69)
        msg = env->NewStringUTF("Invalid command line");
    else if (rc == -1)
        msg = env->NewStringUTF("Memory initialization failed");

    return msg;
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

#define SAMPLE_COUNT 512

JNIEnv* jenv; //we are abusing the f*** out of this poor guy
//JavaVM* javaVM = NULL; //this seems like the right way to go
//stuff for audio
jshortArray jsamples;
jmethodID writemid;
jmethodID coreMessageMid;
jmethodID dieMid;
jobject emu;
//stuff for microphone
jobject sipemu;
jmethodID getmicdata;
//stuff for vmu lcd
jobject vmulcd = NULL;
jbyteArray jpix = NULL;
jmethodID updatevmuscreen;

// Convenience class to get the java environment for the current thread.
// Also attach the threads, and detach it on destruction, if needed. This is probably not very efficient
// but shouldn't be needed except for error reporting.
class JVMAttacher {
public:
    JVMAttacher() : env(NULL), detach_thread(false) {
        if (g_jvm == NULL) {
            log_error("g_jvm == NULL");
            return;
        }
        int rc = g_jvm->GetEnv((void **)&env, JNI_VERSION_1_6);
        if (rc  == JNI_EDETACHED) {
            if (g_jvm->AttachCurrentThread(&env, NULL) != 0) {
                log_error("AttachCurrentThread failed");
                return;
            }
            detach_thread = true;
        }
        else if (rc == JNI_EVERSION) {
            log_error("JNI version error");
            return;
        }
    }

    ~JVMAttacher()
    {
        if (detach_thread)
            g_jvm->DetachCurrentThread();
    }

    void log_error(const char *reason)
    {
        LOGE("JVMAttacher cannot attach to JVM: %s", reason);
    }

    bool failed() { return env == NULL; }

    JNIEnv *env;
    bool detach_thread = false;
};

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_query(JNIEnv *env,jobject obj,jobject emu_thread)
{
    jmethodID reiosInfoMid=env->GetMethodID(env->GetObjectClass(emu_thread),"reiosInfo","(Ljava/lang/String;Ljava/lang/String;)V");

    char *id = (char*)malloc(11);
    strcpy(id, reios_disk_id());
    jstring reios_id = env->NewStringUTF(id);

    char *name = (char*)malloc(129);
    strcpy(name, reios_software_name);
    jstring reios_name = env->NewStringUTF(name);

    env->CallVoidMethod(emu_thread, reiosInfoMid, reios_id, reios_name);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_run(JNIEnv *env,jobject obj,jobject emu_thread)
{
    install_prof_handler(0);
    jenv = env;
    emu = env->NewGlobalRef(emu_thread);

    jsamples=env->NewShortArray(SAMPLE_COUNT*2);
    writemid=env->GetMethodID(env->GetObjectClass(emu),"WriteBuffer","([SI)I");
    coreMessageMid=env->GetMethodID(env->GetObjectClass(emu),"coreMessage","([B)I");
    dieMid=env->GetMethodID(env->GetObjectClass(emu),"Die","()V");

    dc_run();

    env->DeleteGlobalRef(emu);
    emu = NULL;
}

int msgboxf(const wchar* text,unsigned int type,...) {
    va_list args;

    wchar temp[2048];
    va_start(args, type);
    vsprintf(temp, text, args);
    va_end(args);
    LOGE("%s", temp);

    if (emu == NULL)
        return 0;
    JVMAttacher attacher;
    if (attacher.failed())
        return 0;

    int byteCount = strlen(temp);
    jbyteArray bytes = attacher.env->NewByteArray(byteCount);
    attacher.env->SetByteArrayRegion(bytes, 0, byteCount, (jbyte *) temp);

    return (int)attacher.env->CallIntMethod(emu, coreMessageMid, bytes);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)
{
    sipemu = env->NewGlobalRef(sip);
    getmicdata = env->GetMethodID(env->GetObjectClass(sipemu),"getData","()[B");
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupVmu(JNIEnv *env,jobject obj,jobject vmu)
{
    //env->GetJavaVM(&javaVM);
    vmulcd = env->NewGlobalRef(vmu);
    updatevmuscreen = env->GetMethodID(env->GetObjectClass(vmu),"updateBytes","([B)V");
    //jpix=env->NewByteArray(1536);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pause(JNIEnv *env,jobject obj)
{
    dc_pause();
    dc_pause_emu();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_resume(JNIEnv *env,jobject obj)
{
    dc_resume_emu(true);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)
{
    if (dc_is_running()) {
        dc_stop();
        rend_cancel_emu_wait();
    }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_destroy(JNIEnv *env,jobject obj)
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

JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_rendframeNative(JNIEnv *env,jobject obj)
{
    if (g_window == NULL)
        return false;
    if (!egl_makecurrent())
        return false;
    jboolean ret = (jboolean)rend_single_frame();
    if (ret)
        gl_swap();
    return ret;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitNative(JNIEnv * env, jobject obj, jobject surface, jint width, jint height)
{
    if (surface != NULL)
    {
        g_window = ANativeWindow_fromSurface(env, surface);
        rend_init_renderer();
        screen_width = width;
        screen_height = height;
    }
    else
    {
        egl_makecurrent();
        rend_term_renderer();
        ANativeWindow_release(g_window);
        g_window = NULL;
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

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_initControllers(JNIEnv *env, jobject obj, jintArray controllers, jobjectArray peripherals)
{
    jint *controllers_body = env->GetIntArrayElements(controllers, 0);
    memcpy(settings.input.maple_devices, controllers_body, sizeof(settings.input.maple_devices));
    env->ReleaseIntArrayElements(controllers, controllers_body, 0);

    int obj_len = env->GetArrayLength(peripherals);
    for (int i = 0; i < obj_len; ++i) {
        jintArray port = (jintArray) env->GetObjectArrayElement(peripherals, i);
        jint *items = env->GetIntArrayElements(port, 0);
        settings.input.maple_expansion_devices[i][0] = items[0];
        settings.input.maple_expansion_devices[i][1] = items[1];
        env->ReleaseIntArrayElements(port, items, 0);
        env->DeleteLocalRef(port);
    }
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

// Audio Stuff
u32 androidaudio_push(void* frame, u32 amt, bool wait)
{
    verify(amt==SAMPLE_COUNT);
    //yeah, do some audio piping magic here !
    jenv->SetShortArrayRegion(jsamples,0,amt*2,(jshort*)frame);
    return jenv->CallIntMethod(emu,writemid,jsamples,wait);
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

void os_DebugBreak()
{
    // TODO: notify the parent thread about it ...

	//raise(SIGABRT);
    pthread_exit(NULL);
	
    // Attach debugger here to figure out what went wrong
    for(;;) ;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAdded(JNIEnv *env, jobject obj, jint id, jstring name, jint maple_port)
{
    const char* joyname = env->GetStringUTFChars(name,0);
    std::shared_ptr<AndroidGamepadDevice> gamepad = std::make_shared<AndroidGamepadDevice>(maple_port, id, joyname);
    AndroidGamepadDevice::AddAndroidGamepad(gamepad);
    env->ReleaseStringUTFChars(name, joyname);

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
JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAxisEvent(JNIEnv *env, jobject obj, jint id, jint key, jint value)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    if (device != NULL)
    	return device->gamepad_axis_input(key, value);
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
