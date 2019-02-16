
#if defined(USE_SDL)
#include <map>
#include "types.h"
#include "cfg/cfg.h"
#include "linux-dist/main.h"
#include "sdl/sdl.h"
#include "rend/gui.h"
#ifndef GLES
#include "khronos/GL3/gl3w.h"
#endif
#endif
#include "hw/maple/maple_devs.h"
#include "sdl_gamepad.h"
#include "sdl_keyboard.h"

static SDL_Window* window = NULL;
static SDL_GLContext glcontext;

#ifdef TARGET_PANDORA
	#define WINDOW_WIDTH  800
#else
	#define WINDOW_WIDTH  640
#endif
#define WINDOW_HEIGHT  480

static SDLMouseGamepadDevice* sdl_mouse_gamepad = NULL;
static SDLKbGamepadDevice* sdl_kb_gamepad = NULL;
static SDLKeyboardDevice* sdl_keyboard = NULL;

extern void dc_stop();

#ifdef TARGET_PANDORA
	extern char OSD_Info[128];
	extern int OSD_Delay;
	extern char OSD_Counters[256];
	extern int OSD_Counter;
#endif

extern u32 mo_buttons;
extern s32 mo_x_abs;
extern s32 mo_y_abs;

extern int screen_width, screen_height;

static void sdl_open_joystick(int index)
{
	SDL_Joystick *pJoystick = SDL_JoystickOpen(index);

	if (pJoystick == NULL)
	{
		printf("SDL: Cannot open joystick %d\n", index + 1);
		return;
	}
	new SDLGamepadDevice(index < MAPLE_PORTS ? index : -1, pJoystick);
}

static void sdl_close_joystick(SDL_JoystickID instance)
{
	SDLGamepadDevice *device = SDLGamepadDevice::GetSDLGamepad(instance);
	if (device != NULL)
		delete device;
}

void input_sdl_init()
{
	if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0)
	{
		if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
		{
			die("error initializing SDL Joystick subsystem");
		}
	}

	SDL_SetRelativeMouseMode(SDL_FALSE);

	sdl_keyboard = new SDLKeyboardDevice(0);
	sdl_kb_gamepad = new SDLKbGamepadDevice(0);
	sdl_mouse_gamepad = new SDLMouseGamepadDevice(0);
}

static void set_mouse_position(int x, int y)
{
	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	if (width != 0 && height != 0)
	{
		float scale = 480.f / height;
		mo_x_abs = (x - (width - 640.f / scale) / 2.f) * scale;
		mo_y_abs = y * scale;
	}
}

void input_sdl_handle(u32 port)
{
	#define SET_FLAG(field, mask, expr) field =((expr) ? (field & ~mask) : (field | mask))
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				dc_stop();
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				sdl_kb_gamepad->gamepad_btn_input(event.key.keysym.sym, event.type == SDL_KEYDOWN);
				{
					int modifier_keys = 0;
					if (event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
						SET_FLAG(modifier_keys, (0x02 | 0x20), event.type == SDL_KEYUP);
					if (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
						SET_FLAG(modifier_keys, (0x01 | 0x10), event.type == SDL_KEYUP);
					sdl_keyboard->keyboard_input(event.key.keysym.sym, event.type == SDL_KEYDOWN, modifier_keys);
				}
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				{
					SDLGamepadDevice *device = SDLGamepadDevice::GetSDLGamepad((SDL_JoystickID)event.jbutton.which);
					if (device != NULL)
						device->gamepad_btn_input(event.jbutton.button, event.type == SDL_JOYBUTTONDOWN);
				}
				break;
			case SDL_JOYAXISMOTION:
				{
					SDLGamepadDevice *device = SDLGamepadDevice::GetSDLGamepad((SDL_JoystickID)event.jaxis.which);
					if (device != NULL)
						device->gamepad_axis_input(event.jaxis.axis, event.jaxis.value);
				}
				break;

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
	#ifdef TARGET_PANDORA
		strncpy(OSD_Counters, text, 256);
	#else
		if(window)
		{
			SDL_SetWindowTitle(window, text);    // *TODO*  Set Icon also...
		}
	#endif
}

void sdl_window_create()
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		{
			die("error initializing SDL Joystick subsystem");
		}
	}

	int window_width  = cfgLoadInt("x11","width", WINDOW_WIDTH);
	int window_height = cfgLoadInt("x11","height", WINDOW_HEIGHT);

	int flags = SDL_WINDOW_OPENGL;
	#ifdef TARGET_PANDORA
		flags |= SDL_FULLSCREEN;
	#else
		flags |= SDL_SWSURFACE | SDL_WINDOW_RESIZABLE;
	#endif

	#ifdef GLES
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	#endif

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	window = SDL_CreateWindow("Reicast Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,	window_width, window_height, flags);
	if (!window)
	{
		die("error creating SDL window");
	}

	glcontext = SDL_GL_CreateContext(window);
	if (!glcontext)
	{
		die("Error creating SDL GL context");
	}
	SDL_GL_MakeCurrent(window, NULL);

	SDL_GL_GetDrawableSize(window, &screen_width, &screen_height);

	float ddpi, hdpi, vdpi;
	if (!SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), &ddpi, &hdpi, &vdpi))
		screen_dpi = (int)roundf(max(hdpi, vdpi));

	printf("Created SDL Window (%ix%i) and GL Context successfully\n", window_width, window_height);
}

bool gl_init(void* wind, void* disp)
{
	SDL_GL_MakeCurrent(window, glcontext);
	#ifdef GLES
		return true;
	#else
		return gl3wInit() != -1 && gl3wIsSupported(3, 1);
	#endif
}

void gl_swap()
{
	SDL_GL_SwapWindow(window);

	/* Check if drawable has been resized */
	SDL_GL_GetDrawableSize(window, &screen_width, &screen_height);
}

void gl_term()
{
	SDL_GL_DeleteContext(glcontext);
}
