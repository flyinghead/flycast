
#if defined(USE_SDL)
#include "types.h"
#include "cfg/cfg.h"
#include "sdl/sdl.h"
#include <SDL_syswm.h>
#endif
#include "hw/maple/maple_devs.h"
#include "sdl_gamepad.h"
#include "sdl_keyboard.h"
#include "wsi/context.h"
#include "emulator.h"
#include "stdclass.h"
#if !defined(_WIN32) && !defined(__APPLE__)
#include "linux-dist/icon.h"
#endif

#ifdef USE_VULKAN
#include <SDL_vulkan.h>
#endif

static SDL_Window* window = NULL;

#ifdef TARGET_PANDORA
	#define WINDOW_WIDTH  800
#else
	#define WINDOW_WIDTH  640
#endif
#define WINDOW_HEIGHT  480

static std::shared_ptr<SDLMouse> sdl_mouse_gamepad;
static std::shared_ptr<SDLKbGamepadDevice> sdl_kb_gamepad;
static SDLKeyboardDevice* sdl_keyboard = NULL;
static bool window_fullscreen;
static bool window_maximized;
static int window_width = WINDOW_WIDTH;
static int window_height = WINDOW_HEIGHT;
static bool gameRunning;
static bool mouseCaptured;
static std::map<u32, std::shared_ptr<SDLMouse>> mice;

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
		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_SetWindowTitle(window, "Flycast");
		mouseCaptured = false;
	}
	else if (SDL_SetRelativeMouseMode(SDL_TRUE) == 0)
	{
		SDL_SetWindowTitle(window, "Flycast - mouse capture");
		mouseCaptured = true;
	}
}

static void emuEventCallback(Event event)
{
	switch (event)
	{
	case Event::Pause:
		gameRunning = false;
		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_SetWindowTitle(window, "Flycast");
		break;
	case Event::Resume:
		gameRunning = true;
		captureMouse(mouseCaptured);
		break;
	default:
		break;
	}
}

static void clearMice()
{
	for (const auto& pair : mice)
		GamepadDevice::Unregister(pair.second);
	mice.clear();
}

static void discoverMice()
{
	clearMice();

	auto defaultMouse = std::make_shared<SDLMouse>();
	mice[0] = defaultMouse;
	GamepadDevice::Register(defaultMouse);

#ifdef _WIN32
	u32 numDevices;
	GetRawInputDeviceList(NULL, &numDevices, sizeof(RAWINPUTDEVICELIST));
	if (numDevices > 0)
	{
		RAWINPUTDEVICELIST *deviceList;
		deviceList = new RAWINPUTDEVICELIST[numDevices];
		if (deviceList != nullptr)
		{
			GetRawInputDeviceList(deviceList, &numDevices, sizeof(RAWINPUTDEVICELIST));
			for (u32 i = 0; i < numDevices; ++i)
			{
				RAWINPUTDEVICELIST& device = deviceList[i];
				if (device.dwType == RIM_TYPEMOUSE)
				{
					// Get the device name
					std::string name;
					std::string uniqueId;
					u32 size;
					GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, nullptr, &size);
					if (size > 0)
					{
						std::vector<char> deviceNameData(size);
						u32 res = GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, &deviceNameData[0], &size);
						if (res != (u32)-1)
						{
							std::string deviceName(&deviceNameData[0], std::strlen(&deviceNameData[0]));
							name = "Mouse " + deviceName;
							uniqueId = "sdl_mouse_" + deviceName;
						}
					}
					u32 handle = (u32)(uintptr_t)device.hDevice;
					if (name.empty())
						name = "Mouse " + std::to_string(handle);
					if (uniqueId.empty())
						uniqueId = "sdl_mouse_" + std::to_string(handle);

					auto ptr = std::make_shared<SDLMouse>(mice.size() >= 4 ? 3 : mice.size(), name, uniqueId, handle);
					mice[handle] = ptr;
					GamepadDevice::Register(ptr);
				}
			}
			delete [] deviceList;
		}
	}
#endif
}

static std::shared_ptr<SDLMouse> getMouse(u32 handle)
{
	auto it = mice.find(handle);
	if (it != mice.end())
		return it->second;
	else
		return nullptr;
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
		if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
			die("SDL: error initializing Joystick subsystem");

		std::string db = get_readonly_data_path("gamecontrollerdb.txt");
		int rv = SDL_GameControllerAddMappingsFromFile(db.c_str());
		if (rv < 0)
		{
			db = get_readonly_config_path("gamecontrollerdb.txt");
			rv = SDL_GameControllerAddMappingsFromFile(db.c_str());
		}
		if (rv > 0)
			DEBUG_LOG(INPUT ,"%d mappings loaded from %s", rv, db.c_str());
	}
	if (SDL_WasInit(SDL_INIT_HAPTIC) == 0)
		SDL_InitSubSystem(SDL_INIT_HAPTIC);

#if !defined(__APPLE__)
	SDL_SetRelativeMouseMode(SDL_FALSE);

	sdl_keyboard = new SDLKeyboardDevice(0);
	sdl_kb_gamepad = std::make_shared<SDLKbGamepadDevice>(0);
	GamepadDevice::Register(sdl_kb_gamepad);
	discoverMice();

	EventManager::listen(Event::Pause, emuEventCallback);
	EventManager::listen(Event::Resume, emuEventCallback);
#endif
}

inline void SDLMouse::detect_btn_input(input_detected_cb button_pressed)
{
	GamepadDevice::detect_btn_input(button_pressed);
	if (rawHandle != 0)
	{
		auto defaultMouse = getMouse(0);
		defaultMouse->detectedRawMouse = getMouse(rawHandle);
	}
}

inline void SDLMouse::cancel_detect_input()
{
	GamepadDevice::cancel_detect_input();
	if (rawHandle != 0)
	{
		auto defaultMouse = getMouse(0);
		defaultMouse->detectedRawMouse = nullptr;
	}
}

inline void SDLMouse::setMouseAbsPos(int x, int y) {
	if (maple_port() < 0)
		return;

	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	if (width != 0 && height != 0)
		SetMousePosition(x, y, width, height, maple_port());
}

inline void SDLMouse::setMouseRelPos(int deltax, int deltay) {
	if (maple_port() < 0)
		return;
	SetRelativeMousePosition(deltax, deltay, maple_port());
}

#define SET_FLAG(field, mask, expr) (field) = ((expr) ? ((field) & ~(mask)) : ((field) | (mask)))

inline void SDLMouse::setMouseButton(u32 button, bool pressed) {
	if (maple_port() < 0)
		return;

	switch (button)
	{
	case SDL_BUTTON_LEFT:
		SET_FLAG(mo_buttons[maple_port()], 1 << 2, pressed);
		break;
	case SDL_BUTTON_RIGHT:
		SET_FLAG(mo_buttons[maple_port()], 1 << 1, pressed);
		break;
	case SDL_BUTTON_MIDDLE:
		SET_FLAG(mo_buttons[maple_port()], 1 << 3, pressed);
		break;
	}
}

void input_sdl_handle()
{
	SDLGamepad::UpdateRumble();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
#if !defined(__APPLE__)
			case SDL_QUIT:
				dc_exit();
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))
				{
					if (window_fullscreen)
						SDL_SetWindowFullscreen(window, 0);
					else
						SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
					window_fullscreen = !window_fullscreen;
				}
				else if (event.type == SDL_KEYDOWN && (event.key.keysym.mod & KMOD_LALT) && (event.key.keysym.mod & KMOD_LCTRL))
				{
					captureMouse(!mouseCaptured);
				}
				else
				{
					sdl_kb_gamepad->gamepad_btn_input(event.key.keysym.sym, event.type == SDL_KEYDOWN);
					sdl_keyboard->keyboard_input(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
				}
				break;
			case SDL_TEXTINPUT:
				for (int i = 0; event.text.text[i] != '\0'; i++)
					sdl_keyboard->keyboard_character(event.text.text[i]);
				break;
#ifdef USE_VULKAN
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED
						|| event.window.event == SDL_WINDOWEVENT_RESTORED
						|| event.window.event == SDL_WINDOWEVENT_MINIMIZED
						|| event.window.event == SDL_WINDOWEVENT_MAXIMIZED)
				{
                	theVulkanContext.SetResized();
				}
				break;
#endif
#endif
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

#if !defined(__APPLE__)
			case SDL_MOUSEMOTION:
				{
					std::shared_ptr<SDLMouse> mouse = getMouse(event.motion.which);
					if (mouse != nullptr)
					{
						if (mouseCaptured && gameRunning)
							mouse->setMouseRelPos(event.motion.xrel, event.motion.yrel);
						else
							mouse->setMouseAbsPos(event.motion.x, event.motion.y);
						mouse->setMouseButton(SDL_BUTTON_LEFT, event.motion.state & SDL_BUTTON_LMASK);
						mouse->setMouseButton(SDL_BUTTON_RIGHT, event.motion.state & SDL_BUTTON_RMASK);
						mouse->setMouseButton(SDL_BUTTON_MIDDLE, event.motion.state & SDL_BUTTON_MMASK);
					}
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					std::shared_ptr<SDLMouse> mouse = getMouse(event.button.which);
					if (mouse != nullptr)
					{
						if (!mouseCaptured || !gameRunning)
							mouse->setMouseAbsPos(event.button.x, event.button.y);
						mouse->setMouseButton(event.button.button, event.button.state == SDL_PRESSED);
						mouse->gamepad_btn_input(event.button.button, event.button.state == SDL_PRESSED);
					}
				}
				break;

			case SDL_MOUSEWHEEL:
				mo_wheel_delta[0] -= event.wheel.y * 35;
				break;
#endif
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

#if !defined(__APPLE__)
static void get_window_state()
{
	u32 flags = SDL_GetWindowFlags(window);
	window_fullscreen = flags & SDL_WINDOW_FULLSCREEN_DESKTOP;
	window_maximized = flags & SDL_WINDOW_MAXIMIZED;
    if (!window_fullscreen && !window_maximized){
        SDL_GetWindowSize(window, &window_width, &window_height);
        window_width /= scaling;
        window_height /= scaling;
    }
		
}

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
            }
        }
    }
#endif
    
	int x = SDL_WINDOWPOS_UNDEFINED;
	int y = SDL_WINDOWPOS_UNDEFINED;
	window_width  = cfgLoadInt("window", "width", window_width);
	window_height = cfgLoadInt("window", "height", window_height);
	window_fullscreen = cfgLoadBool("window", "fullscreen", window_fullscreen);
	window_maximized = cfgLoadBool("window", "maximized", window_maximized);
	if (window != nullptr)
	{
		SDL_GetWindowPosition(window, &x, &y);
		get_window_state();
		SDL_DestroyWindow(window);
	}
	flags |= SDL_SWSURFACE;
#if !defined(GLES)
	flags |= SDL_WINDOW_RESIZABLE;
	if (window_fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	else if (window_maximized)
		flags |= SDL_WINDOW_MAXIMIZED;
#else
	flags |= SDL_WINDOW_FULLSCREEN;
#endif

	window = SDL_CreateWindow("Flycast", x, y, window_width * scaling, window_height * scaling, flags);
	if (window == nullptr)
	{
		ERROR_LOG(COMMON, "Window creation failed: %s", SDL_GetError());
		return false;
	}

#if !defined(GLES) && !defined(_WIN32)
	// Set the window icon
	u32 pixels[48 * 48];
	for (int i = 0; i < 48 * 48; i++)
		pixels[i] = reicast_icon[i + 2];
	SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels, 48, 48, 32, 4 * 48, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	if (surface == NULL)
	  INFO_LOG(COMMON, "Creating icon surface failed: %s", SDL_GetError());
	else
	{
		SDL_SetWindowIcon(window, surface);
		SDL_FreeSurface(surface);
	}
#endif

#ifdef USE_VULKAN
	theVulkanContext.SetWindow(window, nullptr);
#endif
	theGLContext.SetWindow(window);

	return true;
}

void sdl_window_create()
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
	{
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		{
			die("error initializing SDL Video subsystem");
		}
	}
	InitRenderApi();
}

void sdl_window_destroy()
{
	get_window_state();
	cfgSaveInt("window", "width", window_width);
	cfgSaveInt("window", "height", window_height);
	cfgSaveBool("window", "maximized", window_maximized);
	cfgSaveBool("window", "fullscreen", window_fullscreen);
	TermRenderApi();
	SDL_DestroyWindow(window);
}

#ifdef _WIN32
#include <windows.h>

HWND sdl_get_native_hwnd()
{
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	return wmInfo.info.win.window;
}
#endif

#endif // !defined(__APPLE__)

