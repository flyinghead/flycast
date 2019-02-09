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

static SDL_Window* window = NULL;
static SDL_GLContext glcontext;

#ifdef TARGET_PANDORA
	#define WINDOW_WIDTH  800
#else
	#define WINDOW_WIDTH  640
#endif
#define WINDOW_HEIGHT  480

static SDL_Joystick* JoySDL = 0;

extern bool FrameSkipping;
extern void dc_stop();
extern bool KillTex;

#ifdef TARGET_PANDORA
	extern char OSD_Info[128];
	extern int OSD_Delay;
	extern char OSD_Counters[256];
	extern int OSD_Counter;
#endif

#define SDL_MAP_SIZE 32

const u32 sdl_map_btn_usb[SDL_MAP_SIZE] =
	{ DC_BTN_Y, DC_BTN_B, DC_BTN_A, DC_BTN_X, 0, 0, 0, 0, 0, DC_BTN_START };

const u32 sdl_map_axis_usb[SDL_MAP_SIZE] =
	{ DC_AXIS_X, DC_AXIS_Y, 0, 0, 0, 0, 0, 0, 0, 0 };

const u32 sdl_map_btn_xbox360[SDL_MAP_SIZE] =
	{ DC_BTN_A, DC_BTN_B, DC_BTN_X, DC_BTN_Y, 0, 0, 0, DC_BTN_START, 0, 0 };

const u32 sdl_map_axis_xbox360[SDL_MAP_SIZE] =
	{ DC_AXIS_X, DC_AXIS_Y, DC_AXIS_LT, 0, 0, DC_AXIS_RT, DC_DPAD_LEFT, DC_DPAD_UP, 0, 0 };

const u32* sdl_map_btn  = sdl_map_btn_usb;
const u32* sdl_map_axis = sdl_map_axis_usb;

#ifdef TARGET_PANDORA
u32  JSensitivity[256];  // To have less sensitive value on nubs
#endif

static std::map<int, u8> kb_map;
static u32 kb_used = 0;
extern u8 kb_key[6];		// normal keys pressed
extern u8 kb_shift; 		// shift keys pressed (bitmask)
extern u32 mo_buttons;
extern s32 mo_x_abs;
extern s32 mo_y_abs;

static void init_kb_map()
{
	//04-1D Letter keys A-Z (in alphabetic order)
	kb_map[SDLK_a] = 0x04;
	kb_map[SDLK_b] = 0x05;
	kb_map[SDLK_c] = 0x06;
	kb_map[SDLK_d] = 0x07;
	kb_map[SDLK_e] = 0x08;
	kb_map[SDLK_f] = 0x09;
	kb_map[SDLK_g] = 0x0A;
	kb_map[SDLK_h] = 0x0B;
	kb_map[SDLK_i] = 0x0C;
	kb_map[SDLK_j] = 0x0D;
	kb_map[SDLK_k] = 0x0E;
	kb_map[SDLK_l] = 0x0F;
	kb_map[SDLK_m] = 0x10;
	kb_map[SDLK_n] = 0x11;
	kb_map[SDLK_o] = 0x12;
	kb_map[SDLK_p] = 0x13;
	kb_map[SDLK_q] = 0x14;
	kb_map[SDLK_r] = 0x15;
	kb_map[SDLK_s] = 0x16;
	kb_map[SDLK_t] = 0x17;
	kb_map[SDLK_u] = 0x18;
	kb_map[SDLK_v] = 0x19;
	kb_map[SDLK_w] = 0x1A;
	kb_map[SDLK_x] = 0x1B;
	kb_map[SDLK_y] = 0x1C;
	kb_map[SDLK_z] = 0x1D;

	//1E-27 Number keys 1-0
	kb_map[SDLK_1] = 0x1E;
	kb_map[SDLK_2] = 0x1F;
	kb_map[SDLK_3] = 0x20;
	kb_map[SDLK_4] = 0x21;
	kb_map[SDLK_5] = 0x22;
	kb_map[SDLK_6] = 0x23;
	kb_map[SDLK_7] = 0x24;
	kb_map[SDLK_8] = 0x25;
	kb_map[SDLK_9] = 0x26;
	kb_map[SDLK_0] = 0x27;

	kb_map[SDLK_RETURN] = 0x28;
	kb_map[SDLK_ESCAPE] = 0x29;
	kb_map[SDLK_BACKSPACE] = 0x2A;
	kb_map[SDLK_TAB] = 0x2B;
	kb_map[SDLK_SPACE] = 0x2C;

	kb_map[SDLK_MINUS] = 0x2D;	// -
	kb_map[SDLK_EQUALS] = 0x2E;	// =
	kb_map[SDLK_LEFTBRACKET] = 0x2F;	// [
	kb_map[SDLK_RIGHTBRACKET] = 0x30;	// ]

	kb_map[SDLK_BACKSLASH] = 0x31;	// \ (US) unsure of keycode

	//32-34 "]", ";" and ":" (the 3 keys right of L)
	kb_map[SDLK_ASTERISK] = 0x32;	// ~ (non-US) *,µ in FR layout
	kb_map[SDLK_SEMICOLON] = 0x33;	// ;
	kb_map[SDLK_QUOTE] = 0x34;	// '

	//35 hankaku/zenkaku / kanji (top left)
	kb_map[SDLK_BACKQUOTE] = 0x35;	// `~ (US)

	//36-38 ",", "." and "/" (the 3 keys right of M)
	kb_map[SDLK_COMMA] = 0x36;
	kb_map[SDLK_PERIOD] = 0x37;
	kb_map[SDLK_SLASH] = 0x38;

	// CAPSLOCK
	kb_map[SDLK_CAPSLOCK] = 0x39;

	//3A-45 Function keys F1-F12
	for (int i = 0;i < 10; i++)
		kb_map[SDLK_F1 + i] = 0x3A + i;
	kb_map[SDLK_F11] = 0x44;
	kb_map[SDLK_F12] = 0x45;

	//46-4E Control keys above cursor keys
	kb_map[SDLK_PRINTSCREEN] = 0x46;		// Print Screen
	kb_map[SDLK_SCROLLLOCK] = 0x47;		// Scroll Lock
	kb_map[SDLK_PAUSE] = 0x48;		// Pause
	kb_map[SDLK_INSERT] = 0x49;
	kb_map[SDLK_HOME] = 0x4A;
	kb_map[SDLK_PAGEUP] = 0x4B;
	kb_map[SDLK_DELETE] = 0x4C;
	kb_map[SDLK_END] = 0x4D;
	kb_map[SDLK_PAGEDOWN] = 0x4E;

	//4F-52 Cursor keys
	kb_map[SDLK_RIGHT] = 0x4F;
	kb_map[SDLK_LEFT] = 0x50;
	kb_map[SDLK_DOWN] = 0x51;
	kb_map[SDLK_UP] = 0x52;

	//53 Num Lock (Numeric keypad)
	kb_map[SDLK_NUMLOCKCLEAR] = 0x53;
	//54 "/" (Numeric keypad)
	kb_map[SDLK_KP_DIVIDE] = 0x54;
	//55 "*" (Numeric keypad)
	kb_map[SDLK_KP_MULTIPLY] = 0x55;
	//56 "-" (Numeric keypad)
	kb_map[SDLK_KP_MINUS] = 0x56;
	//57 "+" (Numeric keypad)
	kb_map[SDLK_KP_PLUS] = 0x57;
	//58 Enter (Numeric keypad)
	kb_map[SDLK_KP_ENTER] = 0x58;
	//59-62 Number keys 1-0 (Numeric keypad)
	kb_map[SDLK_KP_1] = 0x59;
	kb_map[SDLK_KP_2] = 0x5A;
	kb_map[SDLK_KP_3] = 0x5B;
	kb_map[SDLK_KP_4] = 0x5C;
	kb_map[SDLK_KP_5] = 0x5D;
	kb_map[SDLK_KP_6] = 0x5E;
	kb_map[SDLK_KP_7] = 0x5F;
	kb_map[SDLK_KP_8] = 0x60;
	kb_map[SDLK_KP_9] = 0x61;
	kb_map[SDLK_KP_0] = 0x62;
	//63 "." (Numeric keypad)
	kb_map[SDLK_KP_PERIOD] = 0x63;
	//64 #| (non-US)
	//kb_map[94] = 0x64;
	//65 S3 key
	//66-A4 Not used
	//A5-DF Reserved
	//E0 Left Control
	//E1 Left Shift
	//E2 Left Alt
	//E3 Left S1
	//E4 Right Control
	//E5 Right Shift
	//E6 Right Alt
	//E7 Right S3
	//E8-FF Reserved
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
	// Open joystick device
	int numjoys = SDL_NumJoysticks();
	printf("Number of Joysticks found = %i\n", numjoys);
	if (numjoys > 0)
	{
		JoySDL = SDL_JoystickOpen(0);
	}

	printf("Joystick opened\n");

	if (JoySDL)
	{
		int AxisCount,ButtonCount;
		const char* Name;

		AxisCount   = 0;
		ButtonCount = 0;
		//Name[0]     = '\0';

		AxisCount = SDL_JoystickNumAxes(JoySDL);
		ButtonCount = SDL_JoystickNumButtons(JoySDL);
		Name = SDL_JoystickName(JoySDL);

		printf("SDK: Found '%s' joystick with %d axes and %d buttons\n", Name, AxisCount, ButtonCount);

		if (Name != NULL && strcmp(Name,"Microsoft X-Box 360 pad")==0)
		{
			sdl_map_btn  = sdl_map_btn_xbox360;
			sdl_map_axis = sdl_map_axis_xbox360;
			printf("Using Xbox 360 map\n");
		}
	}
	else
	{
		printf("SDK: No Joystick Found\n");
	}

	#ifdef TARGET_PANDORA
		float v;
		int j;
		for (int i=0; i<128; i++)
		{
			v = ((float)i)/127.0f;
			v = (v+v*v)/2.0f;
			j = (int)(v*127.0f);
			if (j > 127)
			{
				j = 127;
			}
			JSensitivity[128-i] = -j;
			JSensitivity[128+i] = j;
		}
	#endif

	SDL_SetRelativeMouseMode(SDL_FALSE);

	init_kb_map();
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
	int k, value;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				dc_stop();
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				k = event.key.keysym.sym;
				value = (event.type == SDL_KEYDOWN) ? 1 : 0;
				switch (k) {
					case SDLK_SPACE:
						SET_FLAG(kcode[port], DC_BTN_C, value);
						break;
					case SDLK_UP:
						SET_FLAG(kcode[port], DC_DPAD_UP, value);
						break;
					case SDLK_DOWN:
						SET_FLAG(kcode[port], DC_DPAD_DOWN, value);
						break;
					case SDLK_LEFT:
						SET_FLAG(kcode[port], DC_DPAD_LEFT, value);
						break;
					case SDLK_RIGHT:
						SET_FLAG(kcode[port], DC_DPAD_RIGHT, value);
						break;
					case SDLK_d:
						SET_FLAG(kcode[port], DC_BTN_Y, value);
						break;
					case SDLK_x:
						SET_FLAG(kcode[port], DC_BTN_A, value);
						break;
					case SDLK_c:
						SET_FLAG(kcode[port], DC_BTN_B, value);
						break;
					case SDLK_s:
						SET_FLAG(kcode[port], DC_BTN_X, value);
						break;
					case SDLK_MENU:
					case SDLK_TAB:
						gui_open_settings();
						break;
					case SDLK_f:
						lt[port] = (value ? 255 : 0);
						break;
					case SDLK_v:
						rt[port] = (value ? 255 : 0);;
						break;
					case SDLK_RETURN:
						SET_FLAG(kcode[port], DC_BTN_START, value);
						break;

				#if defined(TARGET_PANDORA)
					case SDLK_s:
						if (value)
						{
							settings.aica.NoSound = !settings.aica.NoSound;
							snprintf(OSD_Info, 128, "Sound %s\n", (settings.aica.NoSound) ? "Off" : "On");
							OSD_Delay=300;
						}
						break;
					case SDLK_f:
						if (value)
						{
							FrameSkipping = !FrameSkipping;
							snprintf(OSD_Info, 128, "FrameSkipping %s\n", (FrameSkipping) ? "On" : "Off");
							OSD_Delay = 300;
						}
						break;
					case SDLK_c:
						if (value)
						{
							OSD_Counter = 1 - OSD_Counter;
						}
						break;
				#endif
				}
				{
					auto it = kb_map.find(k);
					if (it != kb_map.end())
					{
						u8 dc_keycode = it->second;
						if (event.type == SDL_KEYDOWN)
						{
							if (kb_used < ARRAY_SIZE(kb_key))
							{
								bool found = false;
								for (int i = 0; !found && i < kb_used; i++)
								{
									if (kb_key[i] == dc_keycode)
										found = true;
								}
								if (!found)
									kb_key[kb_used++] = dc_keycode;
							}
						}
						else
						{
							for (int i = 0; i < kb_used; i++)
							{
								if (kb_key[i] == dc_keycode)
								{
									kb_used--;
									for (int j = i; j < ARRAY_SIZE(kb_key) - 1; j++)
										kb_key[j] = kb_key[j + 1];
									kb_key[ARRAY_SIZE(kb_key) - 1] = 0;
									break;
								}
							}
						}
						if (event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
							SET_FLAG(kb_shift, (0x02 | 0x20), event.type == SDL_KEYUP);
						if (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
							SET_FLAG(kb_shift, (0x01 | 0x10), event.type == SDL_KEYUP);
					}
				}
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				value = (event.type == SDL_JOYBUTTONDOWN) ? 1 : 0;
				k = event.jbutton.button;
				{
					u32 mt = sdl_map_btn[k] >> 16;
					u32 mo = sdl_map_btn[k] & 0xFFFF;

					// printf("BUTTON %d,%d\n",JE.number,JE.value);

					if (mt == 0)
					{
						// printf("Mapped to %d\n",mo);
						if (value)
							kcode[port] &= ~mo;
						else
							kcode[port] |= mo;
					}
					else if (mt == 1)
					{
						// printf("Mapped to %d %d\n",mo,JE.value?255:0);
						if (mo == 0)
						{
							lt[port] = value ? 255 : 0;
						}
						else if (mo == 1)
						{
							rt[port] = value ? 255 : 0;
						}
					}

				}
				break;
			case SDL_JOYAXISMOTION:
				k = event.jaxis.axis;
				value = event.jaxis.value;
				{
					u32 mt = sdl_map_axis[k] >> 16;
					u32 mo = sdl_map_axis[k] & 0xFFFF;

					//printf("AXIS %d,%d\n",JE.number,JE.value);
					s8 v=(s8)(value/256); //-127 ... + 127 range
					#ifdef TARGET_PANDORA
						v = JSensitivity[128+v];
					#endif

					if (mt == 0)
					{
						kcode[port] |= mo;
						kcode[port] |= mo*2;
						if (v < -64)
						{
							kcode[port] &= ~mo;
						}
						else if (v > 64)
						{
							kcode[port] &= ~(mo*2);
						}

						// printf("Mapped to %d %d %d\n",mo,kcode[port]&mo,kcode[port]&(mo*2));
					}
					else if (mt == 1)
					{
						if (v >= 0) v++;  //up to 255

						//   printf("AXIS %d,%d Mapped to %d %d %d\n",JE.number,JE.value,mo,v,v+127);

						if (mo == 0)
						{
							lt[port] = v + 127;
						}
						else if (mo == 1)
						{
							rt[port] = v + 127;
						}
					}
					else if (mt == 2)
					{
						//  printf("AXIS %d,%d Mapped to %d %d [%d]",JE.number,JE.value,mo,v);
						if (mo == 0)
						{
							joyx[port] = v;
						}
						else if (mo==1)
						{
							joyy[port] = v;
						}
					}
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

	printf("Created SDL Window (%ix%i) and GL Context successfully\n", window_width, window_height);
}

extern int screen_width, screen_height;

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
	int new_width, new_height;
	SDL_GL_GetDrawableSize(window, &new_width, &new_height);

	if (new_width != screen_width || new_height != screen_height)
	{
		screen_width = new_width;
		screen_height = new_height;
	}
}

void gl_term()
{
	SDL_GL_DeleteContext(glcontext);
}
