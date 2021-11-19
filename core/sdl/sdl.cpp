
#if defined(USE_SDL)
#include "types.h"
#include "cfg/cfg.h"
#include "sdl/sdl.h"
#include <SDL_syswm.h>
#include <SDL_video.h>
#ifdef USE_VULKAN
#include <SDL_vulkan.h>
#endif
#endif
#include "hw/maple/maple_devs.h"
#include "sdl_gamepad.h"
#include "sdl_keyboard.h"
#include "wsi/context.h"
#include "emulator.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__SWITCH__)
#include "linux-dist/icon.h"
#endif
#ifdef _WIN32
#include "windows/rawinput.h"
#endif
#ifdef __SWITCH__
#include "nswitch.h"
#endif

static SDL_Window* window = NULL;

#ifdef TARGET_PANDORA
	#define WINDOW_WIDTH  800
#else
	#define WINDOW_WIDTH  640
#endif
#define WINDOW_HEIGHT  480

static std::shared_ptr<SDLMouse> sdl_mouse;
static std::shared_ptr<SDLKeyboardDevice> sdl_keyboard;
static bool window_fullscreen;
static bool window_maximized;
static SDL_Rect windowPos { SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT };
static bool gameRunning;
static bool mouseCaptured;
static std::string clipboardText;

static void sdl_open_joystick(int index)
{
	SDL_Joystick *pJoystick = SDL_JoystickOpen(index);

	if (pJoystick == NULL)
	{
		INFO_LOG(INPUT, "SDL: Cannot open joystick %d", index + 1);
		return;
	}
	std::shared_ptr<SDLGamepad> gamepad = std::make_shared<SDLGamepad>(index < MAPLE_PORTS ? index : -1, index, pJoystick);
	SDLGamepad::AddSDLGamepad(gamepad);
}

static void sdl_close_joystick(SDL_JoystickID instance)
{
	std::shared_ptr<SDLGamepad> gamepad = SDLGamepad::GetSDLGamepad(instance);
	if (gamepad != NULL)
		gamepad->close();
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
		SDL_SetWindowTitle(window, "Flycast");
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
	case Event::Pause:
		gameRunning = false;
		if (!config::UseRawInput)
			SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_ShowCursor(SDL_ENABLE);
		SDL_SetWindowTitle(window, "Flycast");
		break;
	case Event::Resume:
		gameRunning = true;
		captureMouse(mouseCaptured);
		if (window_fullscreen && !mouseCaptured)
			SDL_ShowCursor(SDL_DISABLE);

		break;
	default:
		break;
	}
}

static void checkRawInput()
{
#if defined(_WIN32) && !defined(TARGET_UWP)
	if ((bool)config::UseRawInput != (bool)sdl_mouse)
		return;
	if (config::UseRawInput)
	{
		GamepadDevice::Unregister(sdl_keyboard);
		sdl_keyboard = nullptr;
		GamepadDevice::Unregister(sdl_mouse);
		sdl_mouse = nullptr;
		rawinput::init();
	}
	else
	{
		rawinput::term();
		sdl_keyboard = std::make_shared<SDLKeyboardDevice>(0);
		GamepadDevice::Register(sdl_keyboard);
		sdl_mouse = std::make_shared<SDLMouse>();
		GamepadDevice::Register(sdl_mouse);
	}
#else
	if (!sdl_keyboard)
	{
		sdl_keyboard = std::make_shared<SDLKeyboardDevice>(0);
		GamepadDevice::Register(sdl_keyboard);
	}
	if (!sdl_mouse)
	{
		sdl_mouse = std::make_shared<SDLMouse>();
		GamepadDevice::Register(sdl_mouse);
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
	if (SDL_WasInit(SDL_INIT_HAPTIC) == 0)
		SDL_InitSubSystem(SDL_INIT_HAPTIC);

	SDL_SetRelativeMouseMode(SDL_FALSE);

	EventManager::listen(Event::Pause, emuEventCallback);
	EventManager::listen(Event::Resume, emuEventCallback);

	checkRawInput();

#ifdef __SWITCH__
    // when railed, both joycons are mapped to joystick #0,
    // else joycons are individually mapped to joystick #0, joystick #1, ...
    // https://github.com/devkitPro/SDL/blob/switch-sdl2/src/joystick/switch/SDL_sysjoystick.c#L45
	for (int joy = 0; joy < 4; joy++)
		sdl_open_joystick(joy);
#endif
}

inline void SDLMouse::setAbsPos(int x, int y) {
	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	if (width != 0 && height != 0)
		Mouse::setAbsPos(x, y, width, height);
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
					if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))
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
					}
					else if (event.type == SDL_KEYDOWN && (event.key.keysym.mod & KMOD_LALT) && (event.key.keysym.mod & KMOD_LCTRL))
					{
						captureMouse(!mouseCaptured);
					}
					else if (!config::UseRawInput)
					{
						sdl_keyboard->keyboard_input(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
					}
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
					if (mouseCaptured && gameRunning)
						sdl_mouse->setRelPos(event.motion.xrel, event.motion.yrel);
					else
						sdl_mouse->setAbsPos(event.motion.x, event.motion.y);
					sdl_mouse->setButton(Mouse::LEFT_BUTTON, event.motion.state & SDL_BUTTON_LMASK);
					sdl_mouse->setButton(Mouse::RIGHT_BUTTON, event.motion.state & SDL_BUTTON_RMASK);
					sdl_mouse->setButton(Mouse::MIDDLE_BUTTON, event.motion.state & SDL_BUTTON_MMASK);
					sdl_mouse->setButton(Mouse::BUTTON_4, event.motion.state & SDL_BUTTON_X1MASK);
					sdl_mouse->setButton(Mouse::BUTTON_5, event.motion.state & SDL_BUTTON_X2MASK);
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
				gui_set_mouse_position(event.button.x, event.button.y);
				gui_set_mouse_button(event.button.button - 1, event.button.state == SDL_PRESSED);
				checkRawInput();
				if (!config::UseRawInput)
				{
					if (!mouseCaptured || !gameRunning)
						sdl_mouse->setAbsPos(event.button.x, event.button.y);
					bool pressed = event.button.state == SDL_PRESSED;
					switch (event.button.button) {
					case SDL_BUTTON_LEFT:
						sdl_mouse->setButton(Mouse::LEFT_BUTTON, pressed);
						break;
					case SDL_BUTTON_RIGHT:
						sdl_mouse->setButton(Mouse::RIGHT_BUTTON, pressed);
						break;
					case SDL_BUTTON_MIDDLE:
						sdl_mouse->setButton(Mouse::MIDDLE_BUTTON, pressed);
						break;
					case SDL_BUTTON_X1:
						sdl_mouse->setButton(Mouse::BUTTON_4, pressed);
						break;
					case SDL_BUTTON_X2:
						sdl_mouse->setButton(Mouse::BUTTON_5, pressed);
						break;
					}
				}
				break;

			case SDL_MOUSEWHEEL:
				gui_set_mouse_wheel(-event.wheel.y * 35);
				checkRawInput();
				if (!config::UseRawInput)
					sdl_mouse->setWheel(-event.wheel.y);
				break;

			case SDL_JOYDEVICEADDED:
				sdl_open_joystick(event.jdevice.which);
				break;

			case SDL_JOYDEVICEREMOVED:
				sdl_close_joystick((SDL_JoystickID)event.jdevice.which);
				break;
		}
	}
}

void sdl_window_set_text(const char* text)
{
	if (window != nullptr)
		SDL_SetWindowTitle(window, text);
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
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	return wmInfo.info.win.window;
}
#endif

bool sdl_recreate_window(u32 flags)
{
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
            
            float ddpi;
            if (SDL_GetDisplayDPI(0, &ddpi, NULL, NULL) != -1){ //SDL_WINDOWPOS_UNDEFINED is Display 0
                //When using HiDPI mode, set correct DPI scaling
                scaling = ddpi/96.f;
                hdpiScaling = scaling;
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
		scaling = 1.5f;
	}
	else
	{
		windowPos.w  = 1920;
		windowPos.h = 1080;
		scaling = 1.0f;
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
		SDL_DestroyWindow(window);

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

#ifdef USE_DX9
	if (config::RendererType == RenderType::DirectX9)
		GraphicsContext::Instance()->setWindow(getNativeHwnd());
	else
#endif
	GraphicsContext::Instance()->setWindow(window);

	int displayIndex = SDL_GetWindowDisplayIndex(window);
	if (displayIndex < 0)
		WARN_LOG(RENDERER, "Cannot get the window display index: %s", SDL_GetError());
	else
	{
		SDL_DisplayMode mode{};
		if (SDL_GetDesktopDisplayMode(displayIndex, &mode) == 0) {
			INFO_LOG(RENDERER, "Monitor refresh rate: %d Hz", mode.refresh_rate);
			settings.display.refreshRate = mode.refresh_rate;
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
	initRenderApi();
	// ImGui copy & paste
	ImGui::GetIO().GetClipboardTextFn = getClipboardText;
	ImGui::GetIO().SetClipboardTextFn = setClipboardText;
}

void sdl_window_destroy()
{
#ifndef __SWITCH__
	get_window_state();
	cfgSaveInt("window", "left", windowPos.x);
	cfgSaveInt("window", "top", windowPos.y);
	cfgSaveInt("window", "width", windowPos.w);
	cfgSaveInt("window", "height", windowPos.h);
	cfgSaveBool("window", "maximized", window_maximized);
	cfgSaveBool("window", "fullscreen", window_fullscreen);
#endif
	termRenderApi();
	SDL_DestroyWindow(window);
}
