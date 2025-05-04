
#if defined(USE_SDL)
#include "types.h"
#include "cfg/cfg.h"
#include "sdl/sdl.h"
#ifdef _WIN32
#include <SDL_syswm.h>
#endif
#include <SDL_video.h>
#if defined(USE_VULKAN)
#include <SDL_vulkan.h>
#endif
#endif
#include "hw/maple/maple_devs.h"
#include "sdl_gamepad.h"
#include "sdl_keyboard.h"
#include "sdl_keyboard_mac.h"
#include "wsi/context.h"
#include "emulator.h"
#include "stdclass.h"
#include "imgui.h"
#include "hw/naomi/card_reader.h"
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__SWITCH__)
#include "linux-dist/icon.h"
#endif
#ifdef _WIN32
#include "windows/rawinput.h"
#endif
#ifdef __SWITCH__
#include "nswitch.h"
#include "switch_gamepad.h"
#endif
#include "dreamlink.h"
#include <unordered_map>

static SDL_Window* window = NULL;
static u32 windowFlags;

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT  480

std::map<SDL_JoystickID, std::shared_ptr<SDLGamepad>> SDLGamepad::sdl_gamepads;
static std::unordered_map<u64, std::shared_ptr<SDLMouse>> sdl_mice;
static std::shared_ptr<SDLKeyboardDevice> sdl_keyboard;
static bool window_fullscreen;
static bool window_maximized;
static SDL_Rect windowPos { SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT };
static bool gameRunning;
static bool mouseCaptured;
static std::string clipboardText;
static std::string barcode;
static u64 lastBarcodeTime;

static KeyboardLayout detectKeyboardLayout();
static bool handleBarcodeScanner(const SDL_Event& event);
void sdl_stopHaptic(int port);
static void pauseHaptic();
static void resumeHaptic();

static struct SDLDeInit
{
	~SDLDeInit() {
		if (initialized)
			SDL_Quit();
	}

	bool initialized = false;
} sdlDeInit;

static void sdl_open_joystick(int index)
{
	if (settings.naomi.slave)
		return;
	SDL_Joystick *pJoystick = SDL_JoystickOpen(index);

	if (pJoystick == NULL)
	{
		INFO_LOG(INPUT, "SDL: Cannot open joystick %d", index + 1);
		return;
	}
	try {
#ifdef __SWITCH__
		std::shared_ptr<SDLGamepad> gamepad = std::make_shared<SwitchGamepad>(index < MAPLE_PORTS ? index : -1, index, pJoystick);
#else
		std::shared_ptr<SDLGamepad> gamepad;
		if (DreamLinkGamepad::isDreamcastController(index))
			gamepad = std::make_shared<DreamLinkGamepad>(index < MAPLE_PORTS ? index : -1, index, pJoystick);
		else
			gamepad = std::make_shared<SDLGamepad>(index < MAPLE_PORTS ? index : -1, index, pJoystick);
#endif
		SDLGamepad::AddSDLGamepad(gamepad);
	} catch (const FlycastException& e) {
	}
}

static void sdl_close_joystick(SDL_JoystickID instance)
{
	if (settings.naomi.slave)
		return;
	std::shared_ptr<SDLGamepad> gamepad = SDLGamepad::GetSDLGamepad(instance);
	if (gamepad != NULL)
		gamepad->close();
}

static void setWindowTitleGame()
{
	if (settings.naomi.slave)
		SDL_SetWindowTitle(window, ("Flycast - Multiboard Slave " + cfgLoadStr("naomi", "BoardId", "")).c_str());
	else
		SDL_SetWindowTitle(window, ("Flycast - " + settings.content.title).c_str());
}

static void captureMouse(bool capture)
{
	if (window == nullptr || !gameRunning)
		return;
	if (!capture)
	{
		if (!config::UseRawInput)
			SDL_SetRelativeMouseMode(SDL_FALSE);
		else
			SDL_ShowCursor(SDL_ENABLE);
		setWindowTitleGame();
		mouseCaptured = false;
	}
	else
	{
		if (config::UseRawInput
				|| SDL_SetRelativeMouseMode(SDL_TRUE) == 0)
		{
			if (config::UseRawInput)
				SDL_ShowCursor(SDL_DISABLE);
			SDL_SetWindowTitle(window, "Flycast - mouse capture");
			mouseCaptured = true;
		}
	}
}

static void emuEventCallback(Event event, void *)
{
	switch (event)
	{
	case Event::Terminate:
		SDL_SetWindowTitle(window, "Flycast");
		sdl_stopHaptic(0);
		break;
	case Event::Pause:
		gameRunning = false;
		if (!config::UseRawInput)
			SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_ShowCursor(SDL_ENABLE);
		setWindowTitleGame();
		pauseHaptic();
		break;
	case Event::Resume:
		gameRunning = true;
		captureMouse(mouseCaptured);
		if (window_fullscreen && !mouseCaptured)
			SDL_ShowCursor(SDL_DISABLE);
		resumeHaptic();
		break;
	default:
		break;
	}
}

static void checkRawInput()
{
#if defined(_WIN32) && !defined(TARGET_UWP)
	if ((bool)config::UseRawInput != (bool)sdl_keyboard)
		return;
	if (config::UseRawInput)
	{
		GamepadDevice::Unregister(sdl_keyboard);
		sdl_keyboard = nullptr;
		for (auto& it : sdl_mice)
			GamepadDevice::Unregister(it.second);
		sdl_mice.clear();
		rawinput::init();
	}
	else
	{
		rawinput::term();
		sdl_keyboard = std::make_shared<SDLKeyboardDevice>(0);
		GamepadDevice::Register(sdl_keyboard);
	}
#else
	if (!sdl_keyboard)
	{
#ifdef __APPLE__
		sdl_keyboard = std::make_shared<SDLMacKeyboard>(0);
#else
		sdl_keyboard = std::make_shared<SDLKeyboardDevice>(0);
#endif
		GamepadDevice::Register(sdl_keyboard);
	}
#endif
}

void input_sdl_init()
{
	if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0)
	{
		// We want joystick events even if we loose focus
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#ifdef _WIN32
		if (cfgLoadBool("input", "DisableXInput", false))
		{
			// Disable XInput for some old joysticks
			NOTICE_LOG(INPUT, "Disabling XInput, using DirectInput");
			SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
		}
		// Don't close the app when pressing the B button
		SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
#endif
		std::string db = get_readonly_data_path("gamecontrollerdb.txt");
		int rv = SDL_GameControllerAddMappingsFromFile(db.c_str());
		if (rv < 0)
		{
			db = get_readonly_config_path("gamecontrollerdb.txt");
			rv = SDL_GameControllerAddMappingsFromFile(db.c_str());
		}
		if (rv > 0)
			DEBUG_LOG(INPUT ,"%d mappings loaded from %s", rv, db.c_str());

		if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
			die("SDL: error initializing Joystick subsystem");
	}
	sdlDeInit.initialized = true;
	if (SDL_WasInit(SDL_INIT_HAPTIC) == 0)
		SDL_InitSubSystem(SDL_INIT_HAPTIC);

	SDL_SetRelativeMouseMode(SDL_FALSE);

	// Event::Start is called on a background thread, so we can't use it to change the window title (macOS)
	// However it's followed by Event::Resume which is fine.
	EventManager::listen(Event::Terminate, emuEventCallback);
	EventManager::listen(Event::Pause, emuEventCallback);
	EventManager::listen(Event::Resume, emuEventCallback);

	checkRawInput();

#if defined(__SWITCH__) || defined(__OpenBSD__)
    // when railed, both joycons are mapped to joystick #0,
    // else joycons are individually mapped to joystick #0, joystick #1, ...
    // https://github.com/devkitPro/SDL/blob/switch-sdl2/src/joystick/switch/SDL_sysjoystick.c#L45
	for (int joy = 0; joy < 4; joy++)
		sdl_open_joystick(joy);
#endif
	if (SDL_HasScreenKeyboardSupport())
	{
		NOTICE_LOG(INPUT, "On-screen keyboard supported");
		gui_setOnScreenKeyboardCallback([](bool show) {
			// We should be able to use SDL_IsScreenKeyboardShown() but it doesn't seem to work on Xbox
			static bool visible;
			if (window != nullptr && visible != show)
			{
				visible = show;
				if (show)
					SDL_StartTextInput();
				else
					SDL_StopTextInput();
			}
		});
	}
	if (settings.input.keyboardLangId == KeyboardLayout::US)
		settings.input.keyboardLangId = detectKeyboardLayout();
	barcode.clear();

	// Add MacOS and Windows mappings for Dreamcast Controller USB
	// Linux mappings are OK by default
	// Can be removed once mapping is merged into SDL, see https://github.com/libsdl-org/SDL/pull/12039
#if (defined(__APPLE__) && defined(TARGET_OS_MAC))
	SDL_GameControllerAddMapping("0300000009120000072f000000010000,OrangeFox86 DreamPicoPort,a:b0,b:b1,x:b3,y:b4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,dpdown:h0.4,leftx:a0,lefty:a1,lefttrigger:a2,righttrigger:a5,start:b11");
#elif defined(_WIN32)
	SDL_GameControllerAddMapping("0300000009120000072f000000000000,OrangeFox86 DreamPicoPort,a:b0,b:b1,x:b3,y:b4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,dpdown:h0.4,leftx:a0,lefty:a1,lefttrigger:-a2,righttrigger:-a5,start:b11");
#endif
}

void input_sdl_quit()
{
	EventManager::unlisten(Event::Terminate, emuEventCallback);
	EventManager::unlisten(Event::Pause, emuEventCallback);
	EventManager::unlisten(Event::Resume, emuEventCallback);
	SDLGamepad::closeAllGamepads();
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC);
}

inline void SDLMouse::setAbsPos(int x, int y)
{
	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	if (width != 0 && height != 0)
		Mouse::setAbsPos(x, y, width, height);
}

static std::shared_ptr<SDLMouse> getMouse(u64 mouseId)
{
	auto& mouse = sdl_mice[mouseId];
	if (mouse == nullptr)
	{
		mouse = std::make_shared<SDLMouse>(mouseId);
		GamepadDevice::Register(mouse);
	}
	return mouse;
}

void input_sdl_handle()
{
	SDLGamepad::UpdateRumble();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				dc_exit();
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				checkRawInput();
				if (event.key.repeat == 0)
				{
					auto is_key_mapped = [](u32 code) -> bool {
						const InputMapping::InputSet inputSet{InputMapping::InputDef::from_button(code)};
#if defined(_WIN32) && !defined(TARGET_UWP)
						if (config::UseRawInput)
						{
							for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
							{
								auto gamepad = GamepadDevice::GetGamepad(i);
								if (dynamic_cast<rawinput::RawKeyboard*>(gamepad.get()) != nullptr)
								{
									bool mapped = !(gamepad->get_input_mapping()->get_button_ids(0, inputSet).empty());
									if (mapped) return true;
								}
							}
							return false;
						}
						else
#endif
						{
							return !(sdl_keyboard->get_input_mapping()->get_button_ids(0, inputSet).empty());
						}
					};
					if (event.type == SDL_KEYDOWN)
					{
						// Alt-Return and F11 toggle full screen
						if ((event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))
								|| (event.key.keysym.sym == SDLK_F11 && (event.key.keysym.mod & (KMOD_ALT | KMOD_CTRL | KMOD_SHIFT | KMOD_GUI)) == 0))
						{
							if (window_fullscreen)
							{
								SDL_SetWindowFullscreen(window, 0);
								if (!gameRunning || !mouseCaptured)
									SDL_ShowCursor(SDL_ENABLE);
							}
							else
							{
								SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
								if (gameRunning)
									SDL_ShowCursor(SDL_DISABLE);
							}
							window_fullscreen = !window_fullscreen;
							break;
						}
						// Left-Alt + Left-CTRL toggles mouse capture
						if ((event.key.keysym.mod & KMOD_LALT) && (event.key.keysym.mod & KMOD_LCTRL)
								&& !(is_key_mapped(SDL_SCANCODE_LALT) || is_key_mapped(SDL_SCANCODE_LCTRL)))
						{
							captureMouse(!mouseCaptured);
							break;
						}
						// Barcode scanner
						if (card_reader::barcodeAvailable() && handleBarcodeScanner(event))
							break;
					}
					if (!config::UseRawInput)
						sdl_keyboard->input(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
				}
				break;

			case SDL_TEXTINPUT:
				gui_keyboard_inputUTF8(event.text.text);
				break;

			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED
						|| event.window.event == SDL_WINDOWEVENT_RESTORED
						|| event.window.event == SDL_WINDOWEVENT_MINIMIZED
						|| event.window.event == SDL_WINDOWEVENT_MAXIMIZED)
				{
#ifdef USE_VULKAN
					if (windowFlags & SDL_WINDOW_VULKAN)
						SDL_Vulkan_GetDrawableSize(window, &settings.display.width, &settings.display.height);
					else
#endif
#ifdef USE_OPENGL
					if (windowFlags & SDL_WINDOW_OPENGL)
						SDL_GL_GetDrawableSize(window, &settings.display.width, &settings.display.height);
					else
#endif
						SDL_GetWindowSize(window, &settings.display.width, &settings.display.height);
					GraphicsContext::Instance()->resize();
				}
				else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				{
					if (window_fullscreen && gameRunning)
						SDL_ShowCursor(SDL_DISABLE);
				}
				else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				{
					if (window_fullscreen)
						SDL_ShowCursor(SDL_ENABLE);
				}
				break;

			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				{
					std::shared_ptr<SDLGamepad> device = SDLGamepad::GetSDLGamepad((SDL_JoystickID)event.jbutton.which);
					if (device != NULL)
						device->gamepad_btn_input(event.jbutton.button, event.type == SDL_JOYBUTTONDOWN);
				}
				break;
			case SDL_JOYAXISMOTION:
				{
					std::shared_ptr<SDLGamepad> device = SDLGamepad::GetSDLGamepad((SDL_JoystickID)event.jaxis.which);
					if (device != NULL)
						device->gamepad_axis_input(event.jaxis.axis, event.jaxis.value);
				}
				break;
			case SDL_JOYHATMOTION:
				{
					std::shared_ptr<SDLGamepad> device = SDLGamepad::GetSDLGamepad((SDL_JoystickID)event.jhat.which);
					if (device != NULL)
					{
						u32 hatid = (event.jhat.hat + 1) << 8;
						if (event.jhat.value & SDL_HAT_UP)
						{
							device->gamepad_btn_input(hatid + 0, true);
							device->gamepad_btn_input(hatid + 1, false);
						}
						else if (event.jhat.value & SDL_HAT_DOWN)
						{
							device->gamepad_btn_input(hatid + 0, false);
							device->gamepad_btn_input(hatid + 1, true);
						}
						else
						{
							device->gamepad_btn_input(hatid + 0, false);
							device->gamepad_btn_input(hatid + 1, false);
						}
						if (event.jhat.value & SDL_HAT_LEFT)
						{
							device->gamepad_btn_input(hatid + 2, true);
							device->gamepad_btn_input(hatid + 3, false);
						}
						else if (event.jhat.value & SDL_HAT_RIGHT)
						{
							device->gamepad_btn_input(hatid + 2, false);
							device->gamepad_btn_input(hatid + 3, true);
						}
						else
						{
							device->gamepad_btn_input(hatid + 2, false);
							device->gamepad_btn_input(hatid + 3, false);
						}
					}
				}
				break;

			case SDL_MOUSEMOTION:
				gui_set_mouse_position(event.motion.x, event.motion.y);
				checkRawInput();
				if (!config::UseRawInput)
				{
					auto mouse = getMouse(event.motion.which);
					if (mouseCaptured && gameRunning)
						mouse->setRelPos(event.motion.xrel, event.motion.yrel);
					else
						mouse->setAbsPos(event.motion.x, event.motion.y);
				}
				else if (mouseCaptured && gameRunning)
				{
					int x, y;
					SDL_GetWindowSize(window, &x, &y);
					x /= 2;
					y /= 2;
					if (std::abs(x - event.motion.x) > 10 || std::abs(y - event.motion.y) > 10 )
						SDL_WarpMouseInWindow(window, x, y);
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					Uint8 button;
					gui_set_mouse_position(event.button.x, event.button.y);
					// Swap middle and right clicks for GUI
					button = event.button.button;
					if (button == SDL_BUTTON_MIDDLE || button == SDL_BUTTON_RIGHT)
						button ^= 1;
					gui_set_mouse_button(button - 1, event.button.state == SDL_PRESSED);
					checkRawInput();
					if (!config::UseRawInput)
					{
						auto mouse = getMouse(event.button.which);
						if (!mouseCaptured || !gameRunning)
							mouse->setAbsPos(event.button.x, event.button.y);
						bool pressed = event.button.state == SDL_PRESSED;
						switch (event.button.button) {
						case SDL_BUTTON_LEFT:
							mouse->setButton(Mouse::LEFT_BUTTON, pressed);
							break;
						case SDL_BUTTON_RIGHT:
							mouse->setButton(Mouse::RIGHT_BUTTON, pressed);
							break;
						case SDL_BUTTON_MIDDLE:
							mouse->setButton(Mouse::MIDDLE_BUTTON, pressed);
							break;
						case SDL_BUTTON_X1:
							mouse->setButton(Mouse::BUTTON_4, pressed);
							break;
						case SDL_BUTTON_X2:
							mouse->setButton(Mouse::BUTTON_5, pressed);
							break;
						}
					}
				}
				break;

			case SDL_MOUSEWHEEL:
				gui_set_mouse_wheel(-event.wheel.y * 35);
				checkRawInput();
				if (!config::UseRawInput) {
					auto mouse = getMouse(event.wheel.which);
					mouse->setWheel(-event.wheel.y);
				}
				break;

			case SDL_JOYDEVICEADDED:
				sdl_open_joystick(event.jdevice.which);
				break;

			case SDL_JOYDEVICEREMOVED:
				sdl_close_joystick((SDL_JoystickID)event.jdevice.which);
				break;

			case SDL_DROPFILE:
				gui_start_game(event.drop.file);
				break;

			// Switch touchscreen support
			case SDL_FINGERDOWN:
			case SDL_FINGERMOTION:
				{
					auto mouse = getMouse(0);
					int x = event.tfinger.x * settings.display.width;
					int y = event.tfinger.y * settings.display.height;
					gui_set_mouse_position(x, y);
					if (mouseCaptured && gameRunning && event.type == SDL_FINGERMOTION)
					{
						int dx = event.tfinger.dx * settings.display.width;
						int dy = event.tfinger.dy * settings.display.height;
						mouse->setRelPos(dx, dy);
					}
					else
						mouse->setAbsPos(x, y);
					if (event.type == SDL_FINGERDOWN) {
						mouse->setButton(Mouse::LEFT_BUTTON, true);
						gui_set_mouse_button(0, true);
					}
				}
				break;
			case SDL_FINGERUP:
				{
					auto mouse = getMouse(0);
					int x = event.tfinger.x * settings.display.width;
					int y = event.tfinger.y * settings.display.height;
					gui_set_mouse_position(x, y);
					gui_set_mouse_button(0, false);
					mouse->setAbsPos(x, y);
					mouse->setButton(Mouse::LEFT_BUTTON, false);
				}
				break;
		}
	}
}

static float hdpiScaling = 1.f;

static inline void get_window_state()
{
	u32 flags = SDL_GetWindowFlags(window);
	window_fullscreen = flags & SDL_WINDOW_FULLSCREEN_DESKTOP;
	window_maximized = flags & SDL_WINDOW_MAXIMIZED;
    if (!window_fullscreen && !window_maximized){
        SDL_GetWindowSize(window, &windowPos.w, &windowPos.h);
        windowPos.w /= hdpiScaling;
        windowPos.h /= hdpiScaling;
        SDL_GetWindowPosition(window, &windowPos.x, &windowPos.y);
    }

}

#if defined(_WIN32) && !defined(TARGET_UWP)
#include <windows.h>

HWND getNativeHwnd()
{
	if (window == nullptr)
		return NULL;
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	return wmInfo.info.win.window;
}
#endif

bool sdl_recreate_window(u32 flags)
{
	windowFlags = flags;
#ifdef _WIN32
    //Enable HiDPI mode in Windows
    typedef enum PROCESS_DPI_AWARENESS {
        PROCESS_DPI_UNAWARE = 0,
        PROCESS_SYSTEM_DPI_AWARE = 1,
        PROCESS_PER_MONITOR_DPI_AWARE = 2
    } PROCESS_DPI_AWARENESS;

    HRESULT(WINAPI *SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS dpiAwareness); // Windows 8.1 and later
    void* shcoreDLL = SDL_LoadObject("SHCORE.DLL");
    if (shcoreDLL) {
        SetProcessDpiAwareness = (HRESULT(WINAPI *)(PROCESS_DPI_AWARENESS)) SDL_LoadFunction(shcoreDLL, "SetProcessDpiAwareness");
        if (SetProcessDpiAwareness) {
            SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

            if (SDL_GetDisplayDPI(0, &settings.display.dpi, NULL, NULL) != -1){ //SDL_WINDOWPOS_UNDEFINED is Display 0
                //When using HiDPI mode, set correct DPI scaling
            	hdpiScaling = settings.display.dpi / 96.f;
            }
        }
        SDL_UnloadObject(shcoreDLL);
    }
#endif

#ifdef __SWITCH__
	AppletOperationMode om = appletGetOperationMode();
	if (om == AppletOperationMode_Handheld)
	{
		windowPos.w  = 1280;
		windowPos.h = 720;
		settings.display.uiScale = 1.5f;
	}
	else
	{
		windowPos.w  = 1920;
		windowPos.h = 1080;
		settings.display.uiScale = 1.4f;
	}
#else
	windowPos.x = cfgLoadInt("window", "left", windowPos.x);
	windowPos.y = cfgLoadInt("window", "top", windowPos.y);
	windowPos.w = cfgLoadInt("window", "width", windowPos.w);
	windowPos.h = cfgLoadInt("window", "height", windowPos.h);
	window_fullscreen = cfgLoadBool("window", "fullscreen", window_fullscreen);
	window_maximized = cfgLoadBool("window", "maximized", window_maximized);
	if (window != nullptr)
		get_window_state();
#endif
	if (window != nullptr)
	{
		SDL_DestroyWindow(window);
		window = nullptr;
	}

#if !defined(GLES)
	flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	if (window_fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	else if (window_maximized)
		flags |= SDL_WINDOW_MAXIMIZED;
#else
	flags |= SDL_WINDOW_FULLSCREEN;
#endif

	window = SDL_CreateWindow("Flycast", windowPos.x, windowPos.y,
			windowPos.w * hdpiScaling, windowPos.h * hdpiScaling, flags);
	if (window == nullptr)
	{
		ERROR_LOG(COMMON, "Window creation failed: %s", SDL_GetError());
		return false;
	}
	settings.display.width = windowPos.w * hdpiScaling;
	settings.display.height = windowPos.h * hdpiScaling;

#ifdef __linux__
	if (flags & SDL_WINDOW_RESIZABLE)
	{
		// The position passed to SDL_CreateWindow doesn't take decorations into account on linux.
		// SDL_ShowWindow retrieves the border dimensions and SDL_SetWindowPosition uses them
		// to correctly (re)position the window if needed.
		// TODO a similar issue happens when switching back from fullscreen
		SDL_ShowWindow(window);
		SDL_SetWindowPosition(window, windowPos.x, windowPos.y);
	}
#endif

#if !defined(GLES) && !defined(_WIN32) && !defined(__SWITCH__) && !defined(__APPLE__)
	// Set the window icon
	u32 pixels[48 * 48];
	for (int i = 0; i < 48 * 48; i++)
		pixels[i] = window_icon[i + 2];
	SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels, 48, 48, 32, 4 * 48, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	if (surface == NULL)
	  INFO_LOG(COMMON, "Creating icon surface failed: %s", SDL_GetError());
	else
	{
		SDL_SetWindowIcon(window, surface);
		SDL_FreeSurface(surface);
	}
#endif

	void *windowCtx = window;
#ifdef _WIN32
	if (isDirectX(config::RendererType))
#ifdef TARGET_UWP
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo(window, &wmInfo);
		windowCtx = wmInfo.info.winrt.window;
	}
#else
		windowCtx = getNativeHwnd();
#endif
#endif
	GraphicsContext::Instance()->setWindow(windowCtx);

	int displayIndex = SDL_GetWindowDisplayIndex(window);
	if (displayIndex < 0)
		WARN_LOG(RENDERER, "Cannot get the window display index: %s", SDL_GetError());
	else
	{
		SDL_DisplayMode mode{};
		if (SDL_GetDesktopDisplayMode(displayIndex, &mode) == 0) {
			NOTICE_LOG(RENDERER, "Monitor refresh rate: %d Hz (%d x %d)", mode.refresh_rate, mode.w, mode.h);
			settings.display.refreshRate = mode.refresh_rate;
			if (flags & SDL_WINDOW_FULLSCREEN)
			{
				settings.display.width = mode.w;
				settings.display.height = mode.h;
			}
		}
	}

	return true;
}

static const char *getClipboardText(void *)
{
	clipboardText.clear();
	if (SDL_HasClipboardText())
	{
		char *text = SDL_GetClipboardText();
		clipboardText = text;
		SDL_free(text);
	}
	return clipboardText.c_str();
}

static void setClipboardText(void *, const char *text)
{
	SDL_SetClipboardText(text);
}

#ifdef TARGET_UWP
static int suspendEventFilter(void *userdata, SDL_Event *event)
{
	if (event->type == SDL_APP_WILLENTERBACKGROUND)
	{
		if (gameRunning)
		{
			try {
				emu.stop();
				if (config::AutoSaveState)
					dc_savestate(config::SavestateSlot);
			} catch (const FlycastException& e) { }
		}
		return 0;
	}
	return 1;
}
#endif

void sdl_window_create()
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
	{
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		{
			die("error initializing SDL Video subsystem");
		}
#if defined(__APPLE__) && defined(USE_VULKAN)
		SDL_Vulkan_LoadLibrary("libvulkan.dylib");
#endif
	}
	sdlDeInit.initialized = true;
	initRenderApi();
	// ImGui copy & paste
	ImGui::GetIO().GetClipboardTextFn = getClipboardText;
	ImGui::GetIO().SetClipboardTextFn = setClipboardText;
#ifdef TARGET_UWP
	// Must be fast so an event filter is required
	SDL_SetEventFilter(suspendEventFilter, nullptr);
#endif
}

void sdl_window_destroy()
{
#ifndef __SWITCH__
	if (!settings.naomi.slave && settings.naomi.drivingSimSlave == 0)
	{
		get_window_state();
		cfgSaveInt("window", "left", windowPos.x);
		cfgSaveInt("window", "top", windowPos.y);
		cfgSaveInt("window", "width", windowPos.w);
		cfgSaveInt("window", "height", windowPos.h);
		cfgSaveBool("window", "maximized", window_maximized);
		cfgSaveBool("window", "fullscreen", window_fullscreen);
	}
#endif
	termRenderApi();
	SDL_DestroyWindow(window);
	window = nullptr;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void sdl_fix_steamdeck_dpi(SDL_Window *window)
{
#ifdef __linux__
	// Fixing Steam Deck's incorrect 60mm * 60mm EDID
	if (settings.display.dpi > 500)
	{
		int displayIndex = SDL_GetWindowDisplayIndex(window);
		SDL_DisplayMode mode;
		SDL_GetDisplayMode(displayIndex, 0, &mode);
		if (displayIndex == 0
				&& (strcmp(SDL_GetDisplayName(displayIndex), "ANX7530 U 3\"") == 0
						|| strcmp(SDL_GetDisplayName(displayIndex), "XWAYLAND0 3\"") == 0)
				&& mode.w == 1280 && mode.h == 800)
			settings.display.dpi = 206;
	}
#endif
}

static KeyboardLayout detectKeyboardLayout()
{
	SDL_Keycode key = SDL_GetKeyFromScancode(SDL_SCANCODE_Q);
	if (key == SDLK_a) {
		INFO_LOG(INPUT, "French keyboard detected");
		return KeyboardLayout::FR;
	}
	key = SDL_GetKeyFromScancode(SDL_SCANCODE_Y);
	if (key == SDLK_z)
	{
		// GE or CH
		key = SDL_GetKeyFromScancode(SDL_SCANCODE_MINUS);
		if (key == '\'') {
			// CH has no direct ss
			INFO_LOG(INPUT, "Swiss keyboard detected");
			return KeyboardLayout::CH;
		}
		else {
			INFO_LOG(INPUT, "German keyboard detected");
			return KeyboardLayout::GE;
		}
	}
	key = SDL_GetKeyFromScancode(SDL_SCANCODE_SEMICOLON);
	if (key == 0xf1) // n with tilde
	{
		// SP or LATAM
		key = SDL_GetKeyFromScancode(SDL_SCANCODE_APOSTROPHE);
		if (key == '{') {
			INFO_LOG(INPUT, "Latam keyboard detected");
			return KeyboardLayout::LATAM;
		}
		else {
			INFO_LOG(INPUT, "Spanish keyboard detected");
			return KeyboardLayout::SP;
		}
	}
	if (key == 0xe7) // c with cedilla
	{
		// PT or BR
		key = SDL_GetKeyFromScancode(SDL_SCANCODE_RIGHTBRACKET);
		if (key == SDLK_LEFTBRACKET)
			INFO_LOG(INPUT, "Portuguese (BR) keyboard detected");
		else
			INFO_LOG(INPUT, "Portuguese keyboard detected");
		return KeyboardLayout::PT;
	}
	key = SDL_GetKeyFromScancode(SDL_SCANCODE_MINUS);
	if (key == SDLK_PLUS) {
		INFO_LOG(INPUT, "Swedish keyboard detected");
		return KeyboardLayout::SW;
	}
	key = SDL_GetKeyFromScancode(SDL_SCANCODE_RIGHTBRACKET);
	if (key == SDLK_ASTERISK) {
		// Not on MacOS
		INFO_LOG(INPUT, "Dutch keyboard detected");
		return KeyboardLayout::NL;
	}
	if (key == SDLK_LEFTBRACKET)
	{
		key = SDL_GetKeyFromScancode(SDL_SCANCODE_SEMICOLON);
		if (key == SDLK_SEMICOLON) {
			// FIXME not working on MacOS
			INFO_LOG(INPUT, "Japanese keyboard detected");
			return KeyboardLayout::JP;
		}
	}
	if (key == SDLK_PLUS)
	{
		// IT
		key = SDL_GetKeyFromScancode(SDL_SCANCODE_GRAVE);
		if (key == SDLK_BACKSLASH) {
			INFO_LOG(INPUT, "Italian keyboard detected");
			return KeyboardLayout::IT;
		}
	}
	if (key == 0xe7) { // c with cedilla
		// MacOS
		INFO_LOG(INPUT, "FR_CA keyboard detected");
		return KeyboardLayout::FR_CA;
	}
	key = SDL_GetKeyFromScancode(SDL_SCANCODE_GRAVE);
	if (key == SDLK_HASH) {
		// linux
		INFO_LOG(INPUT, "FR_CA keyboard detected");
		return KeyboardLayout::FR_CA;
	}
	key = SDL_GetKeyFromScancode(SDL_SCANCODE_BACKSLASH);
	if (key == SDLK_HASH) {
		// MacOS: regular British keyboard not detected, only British - PC
		INFO_LOG(INPUT, "UK keyboard detected");
		return KeyboardLayout::UK;
	}
	// TODO CN, KO have no special keyboard layout

	INFO_LOG(INPUT, "Unknown or US keyboard");
	return KeyboardLayout::US;
}

// All known card games use simple Code 39 barcodes.
// The barcode scanner should be configured to use HID-USB (act like a keyboard)
// and use '*' as preamble and terminator, which are the Code 39 start and stop characters.
// So disable the default terminator ('\n') and enable sending the Code 39 start and stop characters.
static bool handleBarcodeScanner(const SDL_Event& event)
{
	static const std::unordered_map<u16, char> keymapDefault {
		{ SDL_SCANCODE_SPACE, ' ' },
		{ 0x100 | SDL_SCANCODE_B, 'B' },
		{ 0x100 | SDL_SCANCODE_C, 'C' },
		{ 0x100 | SDL_SCANCODE_D, 'D' },
		{ 0x100 | SDL_SCANCODE_E, 'E' },
		{ 0x100 | SDL_SCANCODE_F, 'F' },
		{ 0x100 | SDL_SCANCODE_G, 'G' },
		{ 0x100 | SDL_SCANCODE_H, 'H' },
		{ 0x100 | SDL_SCANCODE_I, 'I' },
		{ 0x100 | SDL_SCANCODE_J, 'J' },
		{ 0x100 | SDL_SCANCODE_K, 'K' },
		{ 0x100 | SDL_SCANCODE_L, 'L' },
		{ 0x100 | SDL_SCANCODE_N, 'N' },
		{ 0x100 | SDL_SCANCODE_O, 'O' },
		{ 0x100 | SDL_SCANCODE_P, 'P' },
		{ 0x100 | SDL_SCANCODE_R, 'R' },
		{ 0x100 | SDL_SCANCODE_S, 'S' },
		{ 0x100 | SDL_SCANCODE_T, 'T' },
		{ 0x100 | SDL_SCANCODE_U, 'U' },
		{ 0x100 | SDL_SCANCODE_V, 'V' },
		{ 0x100 | SDL_SCANCODE_X, 'X' },
	};
	static const std::unordered_map<u16, char> keymapUS {
		{ 0x100 | SDL_SCANCODE_8, '*' },
		{ SDL_SCANCODE_MINUS, '-' },
		{ SDL_SCANCODE_PERIOD, '.' },
		{ 0x100 | SDL_SCANCODE_4, '$' },
		{ SDL_SCANCODE_SLASH, '/' },
		{ 0x100 | SDL_SCANCODE_EQUALS, '+' },
		{ 0x100 | SDL_SCANCODE_5, '%' },
		{ 0x100 | SDL_SCANCODE_A, 'A' },
		{ 0x100 | SDL_SCANCODE_M, 'M' },
		{ 0x100 | SDL_SCANCODE_Q, 'Q' },
		{ 0x100 | SDL_SCANCODE_W, 'W' },
		{ 0x100 | SDL_SCANCODE_Y, 'Y' },
		{ 0x100 | SDL_SCANCODE_Z, 'Z' },
		{ SDL_SCANCODE_0, '0' },
		{ SDL_SCANCODE_1, '1' },
		{ SDL_SCANCODE_2, '2' },
		{ SDL_SCANCODE_3, '3' },
		{ SDL_SCANCODE_4, '4' },
		{ SDL_SCANCODE_5, '5' },
		{ SDL_SCANCODE_6, '6' },
		{ SDL_SCANCODE_7, '7' },
		{ SDL_SCANCODE_8, '8' },
		{ SDL_SCANCODE_9, '9' },
	};
	static const std::unordered_map<u16, char> keymapFr {
		{ SDL_SCANCODE_BACKSLASH, '*' },
		{ SDL_SCANCODE_6, '-' },
		{ 0x100 | SDL_SCANCODE_COMMA, '.' },
		{ 0x100 | SDL_SCANCODE_RIGHTBRACKET, '$' },
		{ 0x100 | SDL_SCANCODE_PERIOD, '/' },
		{ 0x100 | SDL_SCANCODE_EQUALS, '+' },
		{ 0x100 | SDL_SCANCODE_APOSTROPHE, '%' },
		{ 0x100 | SDL_SCANCODE_Q, 'A' },
		{ 0x100 | SDL_SCANCODE_SEMICOLON, 'M' },
		{ 0x100 | SDL_SCANCODE_A, 'Q' },
		{ 0x100 | SDL_SCANCODE_Z, 'W' },
		{ 0x100 | SDL_SCANCODE_Y, 'Y' },
		{ 0x100 | SDL_SCANCODE_W, 'Z' },
		{ 0x100 | SDL_SCANCODE_0, '0' },
		{ 0x100 | SDL_SCANCODE_1, '1' },
		{ 0x100 | SDL_SCANCODE_2, '2' },
		{ 0x100 | SDL_SCANCODE_3, '3' },
		{ 0x100 | SDL_SCANCODE_4, '4' },
		{ 0x100 | SDL_SCANCODE_5, '5' },
		{ 0x100 | SDL_SCANCODE_6, '6' },
		{ 0x100 | SDL_SCANCODE_7, '7' },
		{ 0x100 | SDL_SCANCODE_8, '8' },
		{ 0x100 | SDL_SCANCODE_9, '9' },
	};
	static const std::unordered_map<u16, char> keymapGe {
		{ 0x100 | SDL_SCANCODE_RIGHTBRACKET, '*' },
		{ SDL_SCANCODE_SLASH, '-' },
		{ SDL_SCANCODE_PERIOD, '.' },
		{ 0x100 | SDL_SCANCODE_4, '$' },
		{ 0x100 | SDL_SCANCODE_7, '/' },
		{ SDL_SCANCODE_RIGHTBRACKET, '+' },
		{ 0x100 | SDL_SCANCODE_5, '%' },
		{ 0x100 | SDL_SCANCODE_A, 'A' },
		{ 0x100 | SDL_SCANCODE_M, 'M' },
		{ 0x100 | SDL_SCANCODE_Q, 'Q' },
		{ 0x100 | SDL_SCANCODE_W, 'W' },
		{ 0x100 | SDL_SCANCODE_Z, 'Y' },
		{ 0x100 | SDL_SCANCODE_Y, 'Z' },
		{ SDL_SCANCODE_0, '0' },
		{ SDL_SCANCODE_1, '1' },
		{ SDL_SCANCODE_2, '2' },
		{ SDL_SCANCODE_3, '3' },
		{ SDL_SCANCODE_4, '4' },
		{ SDL_SCANCODE_5, '5' },
		{ SDL_SCANCODE_6, '6' },
		{ SDL_SCANCODE_7, '7' },
		{ SDL_SCANCODE_8, '8' },
		{ SDL_SCANCODE_9, '9' },
	};
	static const std::unordered_map<u16, char> keymapItSp {
		{ 0x100 | SDL_SCANCODE_RIGHTBRACKET, '*' },
		{ SDL_SCANCODE_SLASH, '-' },
		{ SDL_SCANCODE_PERIOD, '.' },
		{ 0x100 | SDL_SCANCODE_4, '$' },
		{ 0x100 | SDL_SCANCODE_7, '/' },
		{ SDL_SCANCODE_RIGHTBRACKET, '+' },
		{ 0x100 | SDL_SCANCODE_5, '%' },
		{ 0x100 | SDL_SCANCODE_A, 'A' },
		{ 0x100 | SDL_SCANCODE_M, 'M' },
		{ 0x100 | SDL_SCANCODE_Q, 'Q' },
		{ 0x100 | SDL_SCANCODE_W, 'W' },
		{ 0x100 | SDL_SCANCODE_Z, 'Z' },
		{ 0x100 | SDL_SCANCODE_Y, 'Y' },
		{ SDL_SCANCODE_0, '0' },
		{ SDL_SCANCODE_1, '1' },
		{ SDL_SCANCODE_2, '2' },
		{ SDL_SCANCODE_3, '3' },
		{ SDL_SCANCODE_4, '4' },
		{ SDL_SCANCODE_5, '5' },
		{ SDL_SCANCODE_6, '6' },
		{ SDL_SCANCODE_7, '7' },
		{ SDL_SCANCODE_8, '8' },
		{ SDL_SCANCODE_9, '9' },
	};
	static const std::unordered_map<u16, char> keymapCH {
		{ 0x100 | SDL_SCANCODE_3, '*' },
		{ SDL_SCANCODE_SLASH, '-' },
		{ SDL_SCANCODE_PERIOD, '.' },
		{ SDL_SCANCODE_BACKSLASH, '$' },
		{ 0x100 | SDL_SCANCODE_7, '/' },
		{ 0x100 | SDL_SCANCODE_1, '+' },
		{ 0x100 | SDL_SCANCODE_5, '%' },
		{ 0x100 | SDL_SCANCODE_A, 'A' },
		{ 0x100 | SDL_SCANCODE_M, 'M' },
		{ 0x100 | SDL_SCANCODE_Q, 'Q' },
		{ 0x100 | SDL_SCANCODE_W, 'W' },
		{ 0x100 | SDL_SCANCODE_Y, 'Z' },
		{ 0x100 | SDL_SCANCODE_Z, 'Y' },
		{ SDL_SCANCODE_0, '0' },
		{ SDL_SCANCODE_1, '1' },
		{ SDL_SCANCODE_2, '2' },
		{ SDL_SCANCODE_3, '3' },
		{ SDL_SCANCODE_4, '4' },
		{ SDL_SCANCODE_5, '5' },
		{ SDL_SCANCODE_6, '6' },
		{ SDL_SCANCODE_7, '7' },
		{ SDL_SCANCODE_8, '8' },
		{ SDL_SCANCODE_9, '9' },
	};
	static const std::unordered_map<u16, char> keymapJp {
		{ 0x100 | SDL_SCANCODE_APOSTROPHE, '*' },
		{ SDL_SCANCODE_MINUS, '-' },
		{ SDL_SCANCODE_PERIOD, '.' },
		{ 0x100 | SDL_SCANCODE_4, '$' },
		{ SDL_SCANCODE_SLASH, '/' },
		{ 0x100 | SDL_SCANCODE_SEMICOLON, '+' },
		{ 0x100 | SDL_SCANCODE_5, '%' },
		{ 0x100 | SDL_SCANCODE_A, 'A' },
		{ 0x100 | SDL_SCANCODE_M, 'M' },
		{ 0x100 | SDL_SCANCODE_Q, 'Q' },
		{ 0x100 | SDL_SCANCODE_W, 'W' },
		{ 0x100 | SDL_SCANCODE_Y, 'Y' },
		{ 0x100 | SDL_SCANCODE_Z, 'Z' },
		{ SDL_SCANCODE_0, '0' },
		{ SDL_SCANCODE_1, '1' },
		{ SDL_SCANCODE_2, '2' },
		{ SDL_SCANCODE_3, '3' },
		{ SDL_SCANCODE_4, '4' },
		{ SDL_SCANCODE_5, '5' },
		{ SDL_SCANCODE_6, '6' },
		{ SDL_SCANCODE_7, '7' },
		{ SDL_SCANCODE_8, '8' },
		{ SDL_SCANCODE_9, '9' },
	};
	static const std::unordered_map<u16, char>* keymap;

	if (keymap == nullptr)
	{
		switch (settings.input.keyboardLangId)
		{
		case KeyboardLayout::FR:
			keymap = &keymapFr;
			break;
		case KeyboardLayout::GE:
			keymap = &keymapGe;
			break;
		case KeyboardLayout::CH:
			keymap = &keymapCH;
			break;
		case KeyboardLayout::IT:
		case KeyboardLayout::SP:
		case KeyboardLayout::LATAM:
			keymap = &keymapItSp;
			break;
		case KeyboardLayout::JP:
			keymap = &keymapJp;
			break;
		case KeyboardLayout::US:
		case KeyboardLayout::UK:
		default:
			keymap = &keymapUS;
			break;
		}
	}
	SDL_Scancode scancode = event.key.keysym.scancode;
	if (scancode >= SDL_SCANCODE_LCTRL)
		// Ignore modifier keys
		return false;
	u16 mod = event.key.keysym.mod;
	if (mod & (KMOD_LALT | KMOD_CTRL | KMOD_GUI))
		// Ignore unused modifiers
		return false;

	u16 k = 0;
	if (mod & (KMOD_LSHIFT | KMOD_RSHIFT))
		k |= 0x100;
	if ((mod & KMOD_CAPS)
			&& ((scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
					|| settings.input.keyboardLangId == KeyboardLayout::FR
					|| settings.input.keyboardLangId == KeyboardLayout::GE
					|| settings.input.keyboardLangId == KeyboardLayout::CH))
		// FIXME all this depends on the OS so best not to use caps lock for now
		k ^= 0x100;
	if (mod & KMOD_RALT)
		k |= 0x200;
	k |= scancode & 0xff;
	auto it = keymap->find(k);
	if (it == keymap->end())
	{
		it = keymapDefault.find(k);
		if (it == keymapDefault.end())
		{
			if (!barcode.empty())
			{
				INFO_LOG(INPUT, "Unrecognized barcode scancode %d mod 0x%x", scancode, mod);
				barcode.clear();
			}
			return false;
		}
	}
	u64 now = getTimeMs();
	if (!barcode.empty() && now - lastBarcodeTime >= 500)
	{
		INFO_LOG(INPUT, "Barcode timeout");
		barcode.clear();
	}
	char c = it->second;
	if (c == '*')
	{
		if (barcode.empty())
		{
			DEBUG_LOG(INPUT, "Barcode start");
			barcode += '*';
			lastBarcodeTime = now;
		}
		else
		{
			card_reader::barcodeSetCard(barcode);
			barcode.clear();
			card_reader::insertCard(0);
		}
		return true;
	}
	if (barcode.empty())
		return false;
	barcode += c;
	lastBarcodeTime = now;

	return true;
}

static float torque;
static float springSat;
static float springSpeed;
static float damperParam;
static float damperSpeed;

void sdl_setTorque(int port, float torque)
{
	::torque = torque;
	if (gameRunning)
		SDLGamepad::SetTorque(port, torque);
}

void sdl_setSpring(int port, float saturation, float speed)
{
	springSat = saturation;
	springSpeed = speed;
	SDLGamepad::SetSpring(port, saturation, speed);
}

void sdl_setDamper(int port, float param, float speed)
{
	damperParam = param;
	damperSpeed = speed;
	SDLGamepad::SetDamper(port, param, speed);
}

void sdl_stopHaptic(int port)
{
	torque = 0.f;
	springSat = 0.f;
	springSpeed = 0.f;
	damperParam = 0.f;
	damperSpeed = 0.f;
	SDLGamepad::StopHaptic(port);
}

void pauseHaptic() {
	SDLGamepad::SetTorque(0, 0.f);
}

void resumeHaptic() {
	SDLGamepad::SetTorque(0, torque);
}

#if 0
#include "ui/gui_util.h"

void sdl_displayHapticStats()
{
	ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
	ImguiStyleVar _1(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ScaledVec2(120, 0));
	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin("##ggpostats", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
	ImguiStyleColor _2(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	ImGui::Text("Torque");
	char s[32];
	snprintf(s, sizeof(s), "%.1f", torque);
	ImGui::ProgressBar(0.5f + torque / 2.f, ImVec2(-1, 0), s);

	ImGui::Text("Spring Sat");
	snprintf(s, sizeof(s), "%.1f", springSat);
	ImGui::ProgressBar(springSat, ImVec2(-1, 0), s);

	ImGui::Text("Spring Speed");
	snprintf(s, sizeof(s), "%.1f", springSpeed);
	ImGui::ProgressBar(springSpeed, ImVec2(-1, 0), s);

	ImGui::Text("Damper Param");
	snprintf(s, sizeof(s), "%.1f", damperParam);
	ImGui::ProgressBar(damperParam, ImVec2(-1, 0), s);

	ImGui::Text("Damper Speed");
	snprintf(s, sizeof(s), "%.1f", damperSpeed);
	ImGui::ProgressBar(damperSpeed, ImVec2(-1, 0), s);

	ImGui::End();
}
#endif
