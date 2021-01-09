
#if defined(USE_SDL)
#include "types.h"
#include "cfg/cfg.h"
#include "sdl/sdl.h"
#include <SDL2/SDL_syswm.h>
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
#include <SDL2/SDL_vulkan.h>
#endif

static SDL_Window* window = NULL;

#ifdef TARGET_PANDORA
	#define WINDOW_WIDTH  800
#else
	#define WINDOW_WIDTH  640
#endif
#define WINDOW_HEIGHT  480

static std::shared_ptr<SDLMouseGamepadDevice> sdl_mouse_gamepad;
static std::shared_ptr<SDLKbGamepadDevice> sdl_kb_gamepad;
static SDLKeyboardDevice* sdl_keyboard = NULL;
static bool window_fullscreen;
static bool window_maximized;
static int window_width = WINDOW_WIDTH;
static int window_height = WINDOW_HEIGHT;

static void sdl_open_joystick(int index)
{
	SDL_Joystick *pJoystick = SDL_JoystickOpen(index);

	if (pJoystick == NULL)
	{
		INFO_LOG(INPUT, "SDL: Cannot open joystick %d", index + 1);
		return;
	}
	std::shared_ptr<SDLGamepadDevice> gamepad = std::make_shared<SDLGamepadDevice>(index < MAPLE_PORTS ? index : -1, index, pJoystick);
	SDLGamepadDevice::AddSDLGamepad(gamepad);
}

static void sdl_close_joystick(SDL_JoystickID instance)
{
	std::shared_ptr<SDLGamepadDevice> gamepad = SDLGamepadDevice::GetSDLGamepad(instance);
	if (gamepad != NULL)
		gamepad->close();
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
	sdl_mouse_gamepad = std::make_shared<SDLMouseGamepadDevice>(0);
	GamepadDevice::Register(sdl_mouse_gamepad);
#endif
}

static void set_mouse_position(int x, int y)
{
	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	if (width != 0 && height != 0)
		SetMousePosition(x, y, width, height);
}

void input_sdl_handle()
{
	SDLGamepadDevice::UpdateRumble();

	#define SET_FLAG(field, mask, expr) (field) = ((expr) ? ((field) & ~(mask)) : ((field) | (mask)))
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
				else
				{
					sdl_kb_gamepad->gamepad_btn_input(event.key.keysym.sym, event.type == SDL_KEYDOWN);
					int modifier_keys = 0;
					if (event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
						SET_FLAG(modifier_keys, (0x02 | 0x20), event.type == SDL_KEYUP);
					if (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
						SET_FLAG(modifier_keys, (0x01 | 0x10), event.type == SDL_KEYUP);
					sdl_keyboard->keyboard_input(event.key.keysym.sym, event.type == SDL_KEYDOWN, modifier_keys);
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
					std::shared_ptr<SDLGamepadDevice> device = SDLGamepadDevice::GetSDLGamepad((SDL_JoystickID)event.jbutton.which);
					if (device != NULL)
						device->gamepad_btn_input(event.jbutton.button, event.type == SDL_JOYBUTTONDOWN);
				}
				break;
			case SDL_JOYAXISMOTION:
				{
					std::shared_ptr<SDLGamepadDevice> device = SDLGamepadDevice::GetSDLGamepad((SDL_JoystickID)event.jaxis.which);
					if (device != NULL)
						device->gamepad_axis_input(event.jaxis.axis, event.jaxis.value);
				}
				break;
			case SDL_JOYHATMOTION:
				{
					std::shared_ptr<SDLGamepadDevice> device = SDLGamepadDevice::GetSDLGamepad((SDL_JoystickID)event.jhat.which);
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
				set_mouse_position(event.motion.x, event.motion.y);
				SET_FLAG(mo_buttons, 1 << 2, event.motion.state & SDL_BUTTON_LMASK);
				SET_FLAG(mo_buttons, 1 << 1, event.motion.state & SDL_BUTTON_RMASK);
				SET_FLAG(mo_buttons, 1 << 3, event.motion.state & SDL_BUTTON_MMASK);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				set_mouse_position(event.button.x, event.button.y);
				switch (event.button.button)
				{
				case SDL_BUTTON_LEFT:
					SET_FLAG(mo_buttons, 1 << 2, event.button.state == SDL_PRESSED);
					break;
				case SDL_BUTTON_RIGHT:
					SET_FLAG(mo_buttons, 1 << 1, event.button.state == SDL_PRESSED);
					break;
				case SDL_BUTTON_MIDDLE:
					SET_FLAG(mo_buttons, 1 << 3, event.button.state == SDL_PRESSED);
					break;
				}
				sdl_mouse_gamepad->gamepad_btn_input(event.button.button, event.button.state == SDL_PRESSED);
				break;

			case SDL_MOUSEWHEEL:
				mo_wheel_delta -= event.wheel.y * 35;
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
	if (window)
	{
		SDL_SetWindowTitle(window, text);    // *TODO*  Set Icon also...
	}
}

#if !defined(__APPLE__)
static void get_window_state()
{
	u32 flags = SDL_GetWindowFlags(window);
	window_fullscreen = flags & SDL_WINDOW_FULLSCREEN_DESKTOP;
	window_maximized = flags & SDL_WINDOW_MAXIMIZED;
	if (!window_fullscreen && !window_maximized)
		SDL_GetWindowSize(window, &window_width, &window_height);
}

void sdl_recreate_window(u32 flags)
{
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
#ifdef TARGET_PANDORA
	flags |= SDL_FULLSCREEN;
#else
	flags |= SDL_SWSURFACE | SDL_WINDOW_RESIZABLE;
	if (window_fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	else if (window_maximized)
		flags |= SDL_WINDOW_MAXIMIZED;
#endif
	window = SDL_CreateWindow("Flycast", x, y, window_width, window_height, flags);
	if (!window)
		die("error creating SDL window");

#ifndef _WIN32
	// Set the window icon
	u32 pixels[48 * 48];
	for (int i = 0; i < 48 * 48; i++)
		pixels[i] = reicast_icon[i + 2];
	SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels, 48, 48, 32, 4 * 48, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	if (surface == NULL)
	  INFO_LOG(COMMON, "Creating surface failed: %s", SDL_GetError());
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

