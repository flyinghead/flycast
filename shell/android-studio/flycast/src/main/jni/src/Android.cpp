#include "types.h"
#include "hw/naomi/naomi_cart.h"
#include "audio/audiostream.h"
#include "imgread/common.h"
#include "ui/gui.h"
#include "rend/osd.h"
#include "cfg/cfg.h"
#include "log/LogManager.h"
#include "wsi/context.h"
#include "emulator.h"
#include "ui/mainui.h"
#include "cfg/option.h"
#include "stdclass.h"
#include "oslib/oslib.h"
#ifdef USE_BREAKPAD
#include "client/linux/handler/exception_handler.h"
#endif
#include "jni_util.h"
#include "android_storage.h"
#include "http_client.h"

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <jni.h>
#include <unistd.h>

JavaVM* g_jvm;
namespace jni
{
	thread_local JVMAttacher jvm_attacher;
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_screenCharacteristics(JNIEnv *env, jobject obj, jfloat screenDpi, jfloat refreshRate)
{
	settings.display.dpi = screenDpi;
	settings.display.refreshRate = refreshRate;
}

static bool game_started;

//stuff for saving prefs
jobject g_emulator;
jmethodID saveAndroidSettingsMid;
static ANativeWindow *g_window = 0;

// Activity
jobject g_activity;
extern jmethodID showScreenKeyboardMid;
static jmethodID onGameStateChangeMid;
extern jmethodID setVGamepadEditModeMid;

static void emuEventCallback(EmuEvent event, void *)
{
	switch (event)
	{
	case EmuEvent::Pause:
		game_started = false;
		if (g_activity != nullptr)
			jni::env()->CallVoidMethod(g_activity, onGameStateChangeMid, false);
		break;
	case EmuEvent::Resume:
		game_started = true;
		if (g_activity != nullptr)
			jni::env()->CallVoidMethod(g_activity, onGameStateChangeMid, true);
		break;
	default:
		break;
	}
}

void os_DoEvents()
{
}

void common_linux_setup();

#if defined(USE_BREAKPAD)
static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded)
{
    if (succeeded)
    {
    	__android_log_print(ANDROID_LOG_ERROR, "Flycast", "Minidump saved to '%s'\n", descriptor.path());
    	registerCrash(descriptor.directory().c_str(), descriptor.path());
    }
	return succeeded;
}

static void *uploadCrashThread(void *p)
{
	uploadCrashes(*(std::string *)p);

	return nullptr;
}

static google_breakpad::ExceptionHandler *exceptionHandler;

#endif

extern "C" JNIEXPORT jstring JNICALL Java_com_flycast_emulator_emu_JNIdc_initEnvironment(JNIEnv *env, jobject obj, jobject emulator, jstring filesDirectory, jstring homeDirectory, jstring jlocale)
{
    bool first_init = false;

    // Keep reference to global JVM and Emulator objects
    if (g_jvm == NULL)
        env->GetJavaVM(&g_jvm);
    if (g_emulator == NULL)
    {
        first_init = true;
        g_emulator = env->NewGlobalRef(emulator);
        saveAndroidSettingsMid = env->GetMethodID(env->GetObjectClass(emulator), "SaveAndroidSettings", "(Ljava/lang/String;)V");
    }
    if (first_init)
    	LogManager::Init();

#if defined(USE_BREAKPAD)
    if (exceptionHandler == nullptr)
    {
    	jni::String directory(homeDirectory, false);
    	if (directory.empty())
    		directory = jni::String(filesDirectory, false);

        google_breakpad::MinidumpDescriptor descriptor(directory.to_string());
        exceptionHandler = new google_breakpad::ExceptionHandler(descriptor, nullptr, dumpCallback, nullptr, true, -1);
    }
#endif
    // Initialize platform-specific stuff
    common_linux_setup();

    // Set home directory based on User config
	jni::String home(homeDirectory, false);
    if (!home.empty())
    {
    	std::string path = home.to_string();
		if (path.back() != '/')
			path += '/';
		set_user_config_dir(path);
		add_system_data_dir(path);
		std::string data_path = path + "data/";
		set_user_data_dir(data_path);
		if (!file_exists(data_path))
		{
			if (!make_directory(data_path))
			{
				WARN_LOG(BOOT, "Cannot create 'data' directory");
				set_user_data_dir(path);
			}
		}
    }
    INFO_LOG(BOOT, "Config dir is: %s", get_writable_config_path("").c_str());
    INFO_LOG(BOOT, "Data dir is:   %s", get_writable_data_path("").c_str());
	jni::String locale(jlocale, false);
    if (!locale.empty())
        setenv("FLYCAST_LOCALE", locale.to_string().c_str(), 1);

    if (first_init)
    {
        // Do one-time initialization
    	EventManager::listen(Event::Pause, emuEventCallback);
    	EventManager::listen(Event::Resume, emuEventCallback);
        jstring msg = NULL;
        int rc = flycast_init(0, NULL);
        if (rc == -1)
            msg = env->NewStringUTF("Memory initialization failed");
#ifdef USE_BREAKPAD
        else
        {
			static std::string crashPath;
			static cThread uploadThread(uploadCrashThread, &crashPath, "SentryUpload");
			crashPath = get_writable_config_path("");
			uploadThread.Start();
        }
#endif

        return msg;
    }
    else
        return NULL;
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_disableOmpAffinity(JNIEnv *env, jobject obj)
{
	setenv("KMP_AFFINITY", "disabled", 1);
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_setExternalStorageDirectories(JNIEnv *env, jobject obj, jobjectArray jpathList)
{
	jni::ObjectArray<jni::String> pathList(jpathList, false);
    std::string paths;
    int obj_len = pathList.size();
    for (int i = 0; i < obj_len; ++i)
    {
        if (!paths.empty())
            paths += ":";
        paths += pathList[i].to_string();
    }
    setenv("FLYCAST_HOME", paths.c_str(), 1);
    gui_refresh_files();
}

static bool stopEmu()
{
	if (!emu.running())
	{
		game_started = false;
	}
	else
	{
		try {
			emu.stop();
		} catch (const FlycastException& e) {
			game_started = false;
			return false;
		}
	}
	// in single-threaded mode, stopping is delayed
	while (game_started)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	return true;
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_setGameUri(JNIEnv *env, jobject obj, jstring jfileName)
{
    std::string fileName = jni::String(jfileName, false).to_string();
	if (!fileName.empty())
	{
		NOTICE_LOG(BOOT, "Game Disk URI: '%s'", fileName.c_str());
		if (game_started)
		{
			stopEmu();
			gui_stop_game();
		}
		std::string path = fileName.substr(0, 7) == "file://" ? fileName.substr(7) : fileName;
		gui_start_game(fileName);
	}
}

//stuff for microphone
jobject sipemu;
jmethodID getmicdata;
jmethodID startRecordingMid;
jmethodID stopRecordingMid;

//stuff for audio
jni::ShortArray jsamples;
jmethodID writeBufferMid;
jmethodID audioInitMid;
jmethodID audioTermMid;
static jobject g_audioBackend;

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)
{
    sipemu = env->NewGlobalRef(sip);
    getmicdata = env->GetMethodID(env->GetObjectClass(sipemu),"getData","(I)[B");
    startRecordingMid = env->GetMethodID(env->GetObjectClass(sipemu),"startRecording","(I)V");
    stopRecordingMid = env->GetMethodID(env->GetObjectClass(sipemu),"stopRecording","()V");
}

static void *savestateThreadFunc(void *)
{
	dc_savestate(config::SavestateSlot);
	return nullptr;
}

static cThread savestateThread(savestateThreadFunc, nullptr, "Flycast-save");

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_pause(JNIEnv *env,jobject obj)
{
	if (config::GGPOEnable)
	{
		stopEmu();
		gui_stop_game();
	}
	else if (game_started && stopEmu())
	{
		game_started = true; // restart when resumed
		if (config::AutoSaveState) {
			savestateThread.WaitToEnd();
			savestateThread.Start();
		}
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_resume(JNIEnv *env,jobject obj)
{
    if (game_started) {
		savestateThread.WaitToEnd();
        emu.start();
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)
{
	stopEmu();
	savestateThread.WaitToEnd();
	gui_stop_game();
}

static void *render_thread_func(void *)
{
	initRenderApi(g_window);

	mainui_loop(false);

	termRenderApi();
	ANativeWindow_release(g_window);
    g_window = nullptr;

    return nullptr;
}

static cThread render_thread(render_thread_func, nullptr, "Flycast-rend");

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_rendinitNative(JNIEnv * env, jobject obj, jobject surface, jint width, jint height)
{
	if (render_thread.thread.joinable())
	{
		if (surface == nullptr)
		{
			mainui_stop();
	        render_thread.WaitToEnd();
		}
		else
		{
			settings.display.width = width;
			settings.display.height = height;
		    mainui_reinit();
		}
	}
	else if (surface != nullptr)
	{
        g_window = ANativeWindow_fromSurface(env, surface);
        mainui_start();
        render_thread.Start();
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_guiOpenSettings(JNIEnv *env, jobject obj)
{
    gui_open_settings();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_flycast_emulator_emu_JNIdc_guiIsOpen(JNIEnv *env, jobject obj)
{
    return gui_is_open();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_flycast_emulator_emu_JNIdc_guiIsContentBrowser(JNIEnv *env,jobject obj)
{
    return gui_is_content_browser();
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_guiSetInsets(JNIEnv *env, jobject obj, jint left, jint right, jint top, jint bottom)
{
	gui_set_insets(left, right, top, bottom);
}

// Audio Stuff
class AndroidAudioBackend : AudioBackend
{
public:
	AndroidAudioBackend()
		: AudioBackend("android", "Android Audio") {}

	u32 push(const void* frame, u32 amt, bool wait) override
	{
		jsamples.setData((short *)frame, 0, amt * 2);
		return jni::env()->CallIntMethod(g_audioBackend, writeBufferMid, (jshortArray)jsamples, wait);
	}

	bool init() override
	{
		jint bufferSize = config::AutoLatency ? 0 : config::AudioBufferSize;
		return jni::env()->CallBooleanMethod(g_audioBackend, audioInitMid, bufferSize);
	}

	void term() override
	{
		jni::env()->CallVoidMethod(g_audioBackend, audioTermMid);
	}

	bool initRecord(u32 sampling_freq) override
	{
		if (sipemu == nullptr)
			return false;
		jni::env()->CallVoidMethod(sipemu, startRecordingMid, sampling_freq);
		return true;
	}

	void termRecord() override
	{
		jni::env()->CallVoidMethod(sipemu, stopRecordingMid);
	}

	u32 record(void *buffer, u32 samples) override
	{
		jni::ByteArray jdata = jni::env()->CallObjectMethod(sipemu, getmicdata, samples);
		if (jdata.size() == 0)
			return 0;
		samples = std::min(samples, (u32)jdata.size() / 2);
		jdata.getData((u8 *)buffer, 0, samples * 2);

		return samples;
	}
};
static AndroidAudioBackend androidAudioBackend;

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_AudioBackend_setInstance(JNIEnv *env, jobject obj, jobject instance)
{
    if (g_audioBackend != nullptr)
        env->DeleteGlobalRef(g_audioBackend);
    if (instance == nullptr) {
        g_audioBackend = nullptr;
        jsamples = {};
    }
    else {
        g_audioBackend = env->NewGlobalRef(instance);
        writeBufferMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "writeBuffer", "([SZ)I");
        audioInitMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "init", "(I)Z");
        audioTermMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "term", "()V");
        if (jsamples.isNull())
            jsamples = jni::ShortArray(SAMPLE_COUNT * 2).globalRef<jni::ShortArray>();
    }
}

[[noreturn]] void os_DebugBreak()
{
    // TODO: notify the parent thread about it ...

	raise(SIGABRT);
    //pthread_exit(NULL);

    // Attach debugger here to figure out what went wrong
    for(;;) ;
}

void SaveAndroidSettings()
{
    jni::String homeDirectory(get_writable_config_path(""));

    jni::env()->CallVoidMethod(g_emulator, saveAndroidSettingsMid, (jstring)homeDirectory);
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_BaseGLActivity_register(JNIEnv *env, jobject obj, jobject activity)
{
    if (g_activity != nullptr) {
        env->DeleteGlobalRef(g_activity);
        g_activity = nullptr;
    }
    if (activity != nullptr)
    {
        g_activity = env->NewGlobalRef(activity);
        jclass actClass = env->GetObjectClass(activity);
        showScreenKeyboardMid = env->GetMethodID(actClass, "showScreenKeyboard", "(Z)V");
        onGameStateChangeMid = env->GetMethodID(actClass, "onGameStateChange", "(Z)V");
        setVGamepadEditModeMid = env->GetMethodID(actClass, "setVGamepadEditMode", "(Z)V");
    }
}

void enableNetworkBroadcast(bool enable)
{
    JNIEnv *env = jni::env();
    jmethodID enableNetworkBroadcastMID = env->GetMethodID(env->GetObjectClass(g_emulator), "enableNetworkBroadcast", "(Z)V");
    env->CallVoidMethod(g_emulator, enableNetworkBroadcastMID, enable);
}

// Useful for armv7 since exceptions cannot traverse dynarec blocks and terminate the process
// The abort message will be stored in the minidump and logged to logcat
extern "C" void abort_message(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	char *buffer;
	vasprintf(&buffer, format, list);
	va_end(list);

	ERROR_LOG(BOOT, "%s", buffer);
	abort();
}

std::string getNativeLibraryPath()
{
	JNIEnv *env = jni::env();
	jmethodID getNativeLibDir = env->GetMethodID(env->GetObjectClass(g_activity), "getNativeLibDir", "()Ljava/lang/String;");
	jni::String nativeLibDir(jni::env()->CallObjectMethod(g_activity, getNativeLibDir));
	return nativeLibDir;
}

std::string getFilesPath()
{
	JNIEnv *env = jni::env();
	jmethodID getInternalFilesDir = env->GetMethodID(env->GetObjectClass(g_activity), "getInternalFilesDir", "()Ljava/lang/String;");
	jni::String filesDir(jni::env()->CallObjectMethod(g_activity, getInternalFilesDir));
	return filesDir;
}

void dc_exit()
{
	settings.content.path.clear();
	if (g_activity != nullptr)
	{
		JNIEnv *env = jni::env();
		jmethodID finishAffinity = env->GetMethodID(env->GetObjectClass(g_activity), "finishAffinity", "()V");
		jni::env()->CallVoidMethod(g_activity, finishAffinity);
	}
}
