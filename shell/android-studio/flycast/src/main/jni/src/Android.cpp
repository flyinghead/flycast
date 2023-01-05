#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "rend/osd.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "hw/naomi/naomi_cart.h"
#include "oslib/audiostream.h"
#include "imgread/common.h"
#include "rend/gui.h"
#include "rend/osd.h"
#include "cfg/cfg.h"
#include "log/LogManager.h"
#include "wsi/context.h"
#include "emulator.h"
#include "rend/mainui.h"
#include "cfg/option.h"
#include "stdclass.h"
#include "oslib/oslib.h"
#ifdef USE_BREAKPAD
#include "client/linux/handler/exception_handler.h"
#endif

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
#include "android_keyboard.h"
#include "http_client.h"

extern "C" JNIEXPORT jint JNICALL Java_com_reicast_emulator_emu_JNIdc_getVirtualGamepadVibration(JNIEnv *env, jobject obj)
{
    return (jint)config::VirtualGamepadVibration;
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_screenCharacteristics(JNIEnv *env, jobject obj, jfloat screenDpi, jfloat refreshRate)
{
	settings.display.dpi = screenDpi;
	settings.display.refreshRate = refreshRate;
}

std::shared_ptr<AndroidMouse> mouse;
std::shared_ptr<AndroidKeyboard> keyboard;

float vjoy_pos[15][8];

static bool game_started;

//stuff for saving prefs
jobject g_emulator;
jmethodID saveAndroidSettingsMid;
static ANativeWindow *g_window = 0;

static void emuEventCallback(Event event, void *)
{
	switch (event)
	{
	case Event::Pause:
		game_started = false;
		break;
	case Event::Resume:
		game_started = true;
		break;
	default:
		break;
	}
}

void os_DoEvents()
{
}

void os_CreateWindow()
{
}

void UpdateInputState()
{
}

void common_linux_setup();

void os_SetupInput()
{
}
void os_TermInput()
{
}

void os_SetWindowText(char const *Text)
{
}

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

extern "C" JNIEXPORT jstring JNICALL Java_com_reicast_emulator_emu_JNIdc_initEnvironment(JNIEnv *env, jobject obj, jobject emulator, jstring filesDirectory, jstring homeDirectory, jstring locale)
{
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
    if (first_init)
    	LogManager::Init();

#if defined(USE_BREAKPAD)
    if (exceptionHandler == nullptr)
    {
        jstring directory = homeDirectory != NULL && env->GetStringLength(homeDirectory) > 0 ? homeDirectory : filesDirectory;
    	const char *jchar = env->GetStringUTFChars(directory, 0);
    	std::string path(jchar);
        env->ReleaseStringUTFChars(directory, jchar);


        google_breakpad::MinidumpDescriptor descriptor(path);
        exceptionHandler = new google_breakpad::ExceptionHandler(descriptor, nullptr, dumpCallback, nullptr, true, -1);
    }
#endif
    // Initialize platform-specific stuff
    common_linux_setup();

    // Set home directory based on User config
    if (homeDirectory != NULL)
    {
    	const char *jchar = env->GetStringUTFChars(homeDirectory, 0);
    	std::string path = jchar;
		if (!path.empty())
		{
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
    	env->ReleaseStringUTFChars(homeDirectory, jchar);
    }
    INFO_LOG(BOOT, "Config dir is: %s", get_writable_config_path("").c_str());
    INFO_LOG(BOOT, "Data dir is:   %s", get_writable_data_path("").c_str());
    if (locale != nullptr)
    {
        const char *jchar = env->GetStringUTFChars(locale, 0);
        std::string localeName = jchar;
        env->ReleaseStringUTFChars(locale, jchar);
        setenv("FLYCAST_LOCALE", localeName.c_str(), 1);
    }
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
			static cThread uploadThread(uploadCrashThread, &crashPath);
			crashPath = get_writable_config_path("");
			uploadThread.Start();
        }
#endif

        return msg;
    }
    else
        return NULL;
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setExternalStorageDirectories(JNIEnv *env, jobject obj, jobjectArray pathList)
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
    setenv("FLYCAST_HOME", paths.c_str(), 1);
    gui_refresh_files();
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setGameUri(JNIEnv *env,jobject obj,jstring fileName)
{
    if (fileName != NULL)
    {
        // Get filename string from Java
        const char* file_path = env->GetStringUTFChars(fileName, 0);
        NOTICE_LOG(BOOT, "Game Disk URI: '%s'", file_path);
        settings.content.path = strlen(file_path) >= 7 && !memcmp(file_path, "file://", 7) ? file_path + 7 : file_path;
        env->ReleaseStringUTFChars(fileName, file_path);
        // TODO game paused/settings/...
        if (game_started) {
            emu.unloadGame();
            gui_state = GuiState::Main;
        }
    }
}

//stuff for microphone
jobject sipemu;
jmethodID getmicdata;
jmethodID startRecordingMid;
jmethodID stopRecordingMid;

//stuff for audio
jshortArray jsamples;
jmethodID writeBufferMid;
jmethodID audioInitMid;
jmethodID audioTermMid;
static jobject g_audioBackend;

// Activity
static jobject g_activity;
static jmethodID VJoyStartEditingMID;
static jmethodID VJoyStopEditingMID;
static jmethodID VJoyResetEditingMID;
static jmethodID showTextInputMid;
static jmethodID hideTextInputMid;
static jmethodID isScreenKeyboardShownMid;

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)
{
    sipemu = env->NewGlobalRef(sip);
    getmicdata = env->GetMethodID(env->GetObjectClass(sipemu),"getData","(I)[B");
    startRecordingMid = env->GetMethodID(env->GetObjectClass(sipemu),"startRecording","(I)V");
    stopRecordingMid = env->GetMethodID(env->GetObjectClass(sipemu),"stopRecording","()V");
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_pause(JNIEnv *env,jobject obj)
{
    if (game_started)
    {
        emu.stop();
        game_started = true; // restart when resumed
        if (config::AutoSaveState)
            dc_savestate(config::SavestateSlot);
    }
    gui_save();
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_resume(JNIEnv *env,jobject obj)
{
    if (game_started)
        emu.start();
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_stop(JNIEnv *env,jobject obj)
{
    if (emu.running()) {
        emu.stop();
        if (config::AutoSaveState)
            dc_savestate(config::SavestateSlot);
    }
    emu.unloadGame();
    gui_state = GuiState::Main;
    settings.content.path.clear();
}

static void *render_thread_func(void *)
{
	initRenderApi(g_window);

	mainui_loop();

	termRenderApi();
	ANativeWindow_release(g_window);
    g_window = NULL;

    return NULL;
}

static cThread render_thread(render_thread_func, NULL);

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_rendinitNative(JNIEnv * env, jobject obj, jobject surface, jint width, jint height)
{
	if (render_thread.thread.joinable())
	{
		if (surface == NULL)
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
	else if (surface != NULL)
	{
        g_window = ANativeWindow_fromSurface(env, surface);
        render_thread.Start();
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_vjoy(JNIEnv * env, jobject obj,int id,float x, float y, float w, float h)
{
    if (id < ARRAY_SIZE(vjoy_pos))
    {
        vjoy_pos[id][0] = x;
        vjoy_pos[id][1] = y;
        vjoy_pos[id][2] = w;
        vjoy_pos[id][3] = h;
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_hideOsd(JNIEnv * env, jobject obj)
{
    HideOSD();
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_getControllers(JNIEnv *env, jobject obj, jintArray controllers, jobjectArray peripherals)
{
    jint *controllers_body = env->GetIntArrayElements(controllers, 0);
    for (u32 i = 0; i < config::MapleMainDevices.size(); i++)
    	controllers_body[i] = (MapleDeviceType)config::MapleMainDevices[i];
    env->ReleaseIntArrayElements(controllers, controllers_body, 0);

    int obj_len = env->GetArrayLength(peripherals);
    for (int i = 0; i < obj_len; ++i) {
        jintArray port = (jintArray) env->GetObjectArrayElement(peripherals, i);
        jint *items = env->GetIntArrayElements(port, 0);
        items[0] = (MapleDeviceType)config::MapleExpansionDevices[i][0];
        items[1] = (MapleDeviceType)config::MapleExpansionDevices[i][1];
        env->ReleaseIntArrayElements(port, items, 0);
        env->DeleteLocalRef(port);
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_guiOpenSettings(JNIEnv *env, jobject obj)
{
    gui_open_settings();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsOpen(JNIEnv *env, jobject obj)
{
    return gui_is_open();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_emu_JNIdc_guiIsContentBrowser(JNIEnv *env,jobject obj)
{
    return gui_is_content_browser();
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_guiSetInsets(JNIEnv *env, jobject obj, jint left, jint right, jint top, jint bottom)
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
		jvm_attacher.getEnv()->SetShortArrayRegion(jsamples, 0, amt * 2, (jshort *)frame);
		return jvm_attacher.getEnv()->CallIntMethod(g_audioBackend, writeBufferMid, jsamples, wait);
	}

	bool init() override
	{
		jint bufferSize = config::AutoLatency ? 0 : config::AudioBufferSize;
		return jvm_attacher.getEnv()->CallBooleanMethod(g_audioBackend, audioInitMid, bufferSize);
	}

	void term() override
	{
		jvm_attacher.getEnv()->CallVoidMethod(g_audioBackend, audioTermMid);
	}

	bool initRecord(u32 sampling_freq) override
	{
		if (sipemu == nullptr)
			return false;
		jvm_attacher.getEnv()->CallVoidMethod(sipemu, startRecordingMid, sampling_freq);
		return true;
	}

	void termRecord() override
	{
		jvm_attacher.getEnv()->CallVoidMethod(sipemu, stopRecordingMid);
	}

	u32 record(void *buffer, u32 samples) override
	{
		jbyteArray jdata = (jbyteArray)jvm_attacher.getEnv()->CallObjectMethod(sipemu, getmicdata, samples);
		if (jdata == NULL)
			return 0;
		jsize size = jvm_attacher.getEnv()->GetArrayLength(jdata);
		samples = std::min(samples, (u32)size * 2);
		jvm_attacher.getEnv()->GetByteArrayRegion(jdata, 0, samples * 2, (jbyte*)buffer);
		jvm_attacher.getEnv()->DeleteLocalRef(jdata);

		return samples;
	}
};
static AndroidAudioBackend androidAudioBackend;

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_AudioBackend_setInstance(JNIEnv *env, jobject obj, jobject instance)
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
        audioInitMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "init", "(I)Z");
        audioTermMid = env->GetMethodID(env->GetObjectClass(g_audioBackend), "term", "()V");
        if (jsamples == NULL) {
            jsamples = env->NewShortArray(SAMPLE_COUNT * 2);
            jsamples = (jshortArray) env->NewGlobalRef(jsamples);
        }
    }
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

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_init(JNIEnv *env, jobject obj)
{
    input_device_manager = env->NewGlobalRef(obj);
    input_device_manager_rumble = env->GetMethodID(env->GetObjectClass(obj), "rumble", "(IFFI)Z");
    // FIXME Don't connect it by default or any screen touch will register as button A press
    mouse = std::make_shared<AndroidMouse>(-1);
    GamepadDevice::Register(mouse);
    keyboard = std::make_shared<AndroidKeyboard>();
    GamepadDevice::Register(keyboard);
    gui_setOnScreenKeyboardCallback([](bool show) {
    	if (g_activity == nullptr)
    		return;
        JNIEnv *env = jvm_attacher.getEnv();
        if (show != env->CallBooleanMethod(g_activity, isScreenKeyboardShownMid))
        {
            INFO_LOG(INPUT, "show/hide keyboard %d", show);
            if (show)
                env->CallVoidMethod(g_activity, showTextInputMid, 0, 0, 16, 100);
            else
                env->CallVoidMethod(g_activity, hideTextInputMid);
        }
    });
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAdded(JNIEnv *env, jobject obj, jint id, jstring name,
		jint maple_port, jstring junique_id, jintArray fullAxes, jintArray halfAxes)
{
    const char* joyname = env->GetStringUTFChars(name,0);
    const char* unique_id = env->GetStringUTFChars(junique_id, 0);

    jsize size = jvm_attacher.getEnv()->GetArrayLength(fullAxes);
    std::vector<int> full(size);
    jvm_attacher.getEnv()->GetIntArrayRegion(fullAxes, 0, size, (jint*)&full[0]);
    size = jvm_attacher.getEnv()->GetArrayLength(halfAxes);
    std::vector<int> half(size);
    jvm_attacher.getEnv()->GetIntArrayRegion(halfAxes, 0, size, (jint*)&half[0]);

    std::shared_ptr<AndroidGamepadDevice> gamepad = std::make_shared<AndroidGamepadDevice>(maple_port, id, joyname, unique_id, full, half);
    AndroidGamepadDevice::AddAndroidGamepad(gamepad);
    env->ReleaseStringUTFChars(name, joyname);
    env->ReleaseStringUTFChars(name, unique_id);
}
extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickRemoved(JNIEnv *env, jobject obj, jint id)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    if (device != NULL)
        AndroidGamepadDevice::RemoveAndroidGamepad(device);
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_virtualGamepadEvent(JNIEnv *env, jobject obj, jint kcode, jint joyx, jint joyy, jint lt, jint rt, jboolean fastForward)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(AndroidGamepadDevice::VIRTUAL_GAMEPAD_ID);
    if (device != NULL)
        device->virtual_gamepad_event(kcode, joyx, joyy, lt, rt, fastForward);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickButtonEvent(JNIEnv *env, jobject obj, jint id, jint key, jboolean pressed)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    if (device != NULL)
        return device->gamepad_btn_input(key, pressed);
    else
    	return false;

}

extern "C" JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_keyboardEvent(JNIEnv *env, jobject obj, jint key, jboolean pressed)
{
       keyboard->keyboard_input(key, pressed);
       return true;
}


extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_keyboardText(JNIEnv *env, jobject obj, jint c)
{
       gui_keyboard_input((u16)c);
}

static std::map<std::pair<jint, jint>, jint> previous_axis_values;

extern "C" JNIEXPORT jboolean JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_joystickAxisEvent(JNIEnv *env, jobject obj, jint id, jint key, jint value)
{
    std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
    if (device != nullptr)
    	return device->gamepad_axis_input(key, value);
    else
    	return false;
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_mouseEvent(JNIEnv *env, jobject obj, jint xpos, jint ypos, jint buttons)
{
	mouse->setAbsPos(xpos, ypos, settings.display.width, settings.display.height);
	mouse->setButton(Mouse::LEFT_BUTTON, (buttons & 1) != 0);
	mouse->setButton(Mouse::RIGHT_BUTTON, (buttons & 2) != 0);
	mouse->setButton(Mouse::MIDDLE_BUTTON, (buttons & 4) != 0);
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_periph_InputDeviceManager_mouseScrollEvent(JNIEnv *env, jobject obj, jint scrollValue)
{
    mouse->setWheel(scrollValue);
}

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_BaseGLActivity_register(JNIEnv *env, jobject obj, jobject activity)
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
        showTextInputMid = env->GetMethodID(env->GetObjectClass(activity), "showTextInput", "(IIII)V");
        hideTextInputMid = env->GetMethodID(env->GetObjectClass(activity), "hideTextInput", "()V");
        isScreenKeyboardShownMid = env->GetMethodID(env->GetObjectClass(activity), "isScreenKeyboardShown", "()Z");
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

extern "C" JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_JNIdc_setButtons(JNIEnv *env, jobject obj, jbyteArray data)
{
    u32 len = env->GetArrayLength(data);
    DefaultOSDButtons.resize(len);
    jboolean isCopy;
    jbyte* b = env->GetByteArrayElements(data, &isCopy);
    memcpy(DefaultOSDButtons.data(), b, len);
    env->ReleaseByteArrayElements(data, b, JNI_ABORT);
}

void enableNetworkBroadcast(bool enable)
{
    JNIEnv *env = jvm_attacher.getEnv();
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
