#if defined(SUPPORT_X11)
#include <map>
#include <memory>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#if !defined(GLES)
	#include <GL/gl.h>
	#include <GL/glx.h>
#endif

#include "types.h"
#include "cfg/cfg.h"
#include "linux-dist/x11.h"
#include "linux-dist/main.h"
#include "rend/gui.h"
#include "input/gamepad.h"

#if FEAT_HAS_NIXPROF
#include "profiler/profiler.h"
#endif
#include "x11_keyboard.h"

#if defined(TARGET_PANDORA)
	#define DEFAULT_FULLSCREEN    true
	#define DEFAULT_WINDOW_WIDTH  800
#else
	#define DEFAULT_FULLSCREEN    false
	#define DEFAULT_WINDOW_WIDTH  640
#endif
#define DEFAULT_WINDOW_HEIGHT   480


class MouseInputMapping : public InputMapping
{
public:
	MouseInputMapping()
	{
		name = "X11 Mouse";
		set_button(DC_BTN_A, Button1);
		set_button(DC_BTN_B, Button3);
		set_button(DC_BTN_START, Button2);

		dirty = false;
	}
};

class X11MouseGamepadDevice : public GamepadDevice
{
public:
	X11MouseGamepadDevice(int maple_port) : GamepadDevice(maple_port, "X11")
	{
		_name = "Mouse";
		_unique_id = "x11_mouse";
		if (!find_mapping())
			input_mapper = new MouseInputMapping();
	}
	bool gamepad_btn_input(u32 code, bool pressed) override
	{
		if (gui_is_open())
			// Don't register mouse clicks as gamepad presses when gui is open
			// This makes the gamepad presses to be handled first and the mouse position to be ignored
			// TODO Make this generic
			return false;
		else
			return GamepadDevice::gamepad_btn_input(code, pressed);
	}
};

int x11_keyboard_input = 0;
static std::shared_ptr<X11KeyboardDevice> x11_keyboard;
static std::shared_ptr<X11KbGamepadDevice> kb_gamepad;
static std::shared_ptr<X11MouseGamepadDevice> mouse_gamepad;

int x11_width;
int x11_height;

int ndcid = 0;
void* x11_glc = NULL;
bool x11_fullscreen = false;
Atom wmDeleteMessage;

void* x11_vis;

extern bool dump_frame_switch;

void dc_exit(void);

enum
{
	_NET_WM_STATE_REMOVE =0,
	_NET_WM_STATE_ADD = 1,
	_NET_WM_STATE_TOGGLE =2
};

void x11_window_set_fullscreen(bool fullscreen)
{
		XEvent xev;
		xev.xclient.type         = ClientMessage;
		xev.xclient.window       = (Window)x11_win;
		xev.xclient.message_type = XInternAtom((Display*)x11_disp, "_NET_WM_STATE", False);
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = 2;    // _NET_WM_STATE_TOGGLE
		xev.xclient.data.l[1] = XInternAtom((Display*)x11_disp, "_NET_WM_STATE_FULLSCREEN", True);
		xev.xclient.data.l[2] = 0;    // no second property to toggle
		xev.xclient.data.l[3] = 1;
		xev.xclient.data.l[4] = 0;

		printf("x11: setting fullscreen to %d\n", fullscreen);
		XSendEvent((Display*)x11_disp, DefaultRootWindow((Display*)x11_disp), False, SubstructureNotifyMask, &xev);
}

void event_x11_handle()
{
	XEvent event;

	while(XPending((Display *)x11_disp))
	{
		XNextEvent((Display *)x11_disp, &event);

		if (event.type == ClientMessage &&
				event.xclient.data.l[0] == wmDeleteMessage)
			dc_exit();
		else if (event.type == ConfigureNotify)
		{
			x11_width = event.xconfigure.width;
			x11_height = event.xconfigure.height;
		}
	}
}

extern u32 mo_buttons;
extern f32 mo_x_delta;
extern f32 mo_y_delta;
extern f32 mo_wheel_delta;
extern s32 mo_x_abs;
extern s32 mo_y_abs;

static bool capturing_mouse;
static Cursor empty_cursor = None;

static Cursor create_empty_cursor()
{
	if (empty_cursor == None)
	{
		Display *display = (Display*)x11_disp;
		char data[] = { 0 };

		XColor color;
		color.red = color.green = color.blue = 0;

		Pixmap pixmap = XCreateBitmapFromData(display, DefaultRootWindow(display),
				data, 1, 1);
		if (pixmap)
		{
			empty_cursor = XCreatePixmapCursor(display, pixmap, pixmap, &color, &color, 0, 0);
			XFreePixmap(display, pixmap);
		}
	}
	return empty_cursor;
}

static void destroy_empty_cursor()
{
	if (empty_cursor != None)
	{
		XFreeCursor((Display*)x11_disp, empty_cursor);
		empty_cursor = None;
	}
}

static void x11_capture_mouse()
{
	x11_window_set_text("Reicast - mouse capture");
	capturing_mouse = true;
	Cursor cursor = create_empty_cursor();
	Display *display = (Display*)x11_disp;
	Window window = (Window)x11_win;
	XDefineCursor(display, window, cursor);
	XGrabPointer(display, window, False,
			ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask,
			GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

static void x11_uncapture_mouse()
{
	x11_window_set_text("Reicast");
	capturing_mouse = false;
	Display *display = (Display*)x11_disp;
	Window window = (Window)x11_win;
	XUndefineCursor(display, window);
	XUngrabPointer(display, CurrentTime);
}

void input_x11_handle()
{
	//Handle X11
	static int prev_x = -1;
	static int prev_y = -1;
	bool mouse_moved = false;
	XEvent e;

	Display *display = (Display*)x11_disp;

	while (XCheckWindowEvent(display, (Window)x11_win,
			KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask
			| PointerMotionMask | FocusChangeMask,
			&e))
	{
		switch(e.type)
		{
			case KeyPress:
				{
					char buf[2];
					KeySym keysym_return;
					int len = XLookupString(&e.xkey, buf, 1, &keysym_return, NULL);
					if (len > 0)
						x11_keyboard->keyboard_character(buf[0]);
				}
				/* no break */
			case KeyRelease:
				{
					if (e.type == KeyRelease && XEventsQueued(display, QueuedAfterReading))
					{
						XEvent nev;
						XPeekEvent(display, &nev);

						if (nev.type == KeyPress && nev.xkey.time == e.xkey.time &&
								nev.xkey.keycode == e.xkey.keycode)
							// Key wasn’t actually released: auto repeat
							continue;
					}
					// Dreamcast keyboard emulation
					x11_keyboard->keyboard_input(e.xkey.keycode, e.type == KeyPress);
					// keyboard-based emulated gamepad
					kb_gamepad->gamepad_btn_input(e.xkey.keycode, e.type == KeyPress);

					// Start/stop mouse capture with Left Ctrl + Left Alt
					if (e.type == KeyPress
							&& ((e.xkey.keycode == KEY_LALT && (e.xkey.state & ControlMask))
								|| (e.xkey.keycode == KEY_LCTRL && (e.xkey.state & Mod1Mask))))
					{
						capturing_mouse = !capturing_mouse;
						if (capturing_mouse)
							x11_capture_mouse();
						else
							x11_uncapture_mouse();
					}
					// TODO Move this to bindable keys or in the gui menu
					if (x11_keyboard_input)
					{
#if 1
						if (e.xkey.keycode == KEY_F10)
						{
							// Dump the next frame into a file
							dump_frame_switch = e.type == KeyPress;
						}
						else
#elif FEAT_HAS_NIXPROF
						if (e.type == KeyRelease && e.xkey.keycode == KEY_F10)
						{
							if (sample_Switch(3000)) {
								printf("Starting profiling\n");
							} else {
								printf("Stopping profiling\n");
							}
						}
						else
#endif
						if (e.type == KeyRelease && e.xkey.keycode == KEY_F11)
						{
							x11_fullscreen = !x11_fullscreen;
							x11_window_set_fullscreen(x11_fullscreen);
						}
					}
				}
				break;

			case FocusOut:
				{
					if (capturing_mouse)
						x11_uncapture_mouse();
					capturing_mouse = false;
				}
				break;

			case ButtonPress:
			case ButtonRelease:
				mouse_gamepad->gamepad_btn_input(e.xbutton.button, e.type == ButtonPress);
				{
					u32 button_mask = 0;
					switch (e.xbutton.button)
					{
					case Button1:		// Left button
						button_mask = 1 << 2;
						break;
					case Button2:		// Middle button
						button_mask = 1 << 3;
						break;
					case Button3:		// Right button
						button_mask = 1 << 1;
						break;
					case Button4: 		// Mouse wheel up
						mo_wheel_delta -= 16;
						break;
					case Button5: 		// Mouse wheel down
						mo_wheel_delta += 16;
						break;
					default:
						break;
					}

					if (button_mask)
					{
						if (e.type == ButtonPress)
							mo_buttons &= ~button_mask;
						else
							mo_buttons |= button_mask;
					}
				}
				/* no break */

			case MotionNotify:
				// For Light gun
				mo_x_abs = (e.xmotion.x - (x11_width - x11_height * 640 / 480) / 2) * 480 / x11_height;
				mo_y_abs = e.xmotion.y * 480 / x11_height;

				// For mouse
				mouse_moved = true;
				if (prev_x != -1)
					mo_x_delta += (f32)(e.xmotion.x - prev_x) * settings.input.MouseSensitivity / 100.f;
				if (prev_y != -1)
					mo_y_delta += (f32)(e.xmotion.y - prev_y) * settings.input.MouseSensitivity / 100.f;
				prev_x = e.xmotion.x;
				prev_y = e.xmotion.y;

				break;
		}
	}
	if (capturing_mouse && mouse_moved)
	{
		prev_x = x11_width / 2;
		prev_y = x11_height / 2;
		XWarpPointer(display, None, (Window)x11_win, 0, 0, 0, 0,
				prev_x, prev_y);
		XSync(display, true);
	}
}

void input_x11_init()
{
	x11_keyboard = std::make_shared<X11KeyboardDevice>(0);
	kb_gamepad = std::make_shared<X11KbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
	mouse_gamepad = std::make_shared<X11MouseGamepadDevice>(0);
	GamepadDevice::Register(mouse_gamepad);

	x11_keyboard_input = (cfgLoadInt("input", "enable_x11_keyboard", 1) >= 1);
	if (!x11_keyboard_input)
		printf("X11 Keyboard input disabled by config.\n");
}

void x11_window_create()
{
	if (cfgLoadInt("pvr", "nox11", 0) == 0)
	{
		XInitThreads();
		// X11 variables
		Window       x11Window = 0;
		Display*     x11Display = 0;
		long         x11Screen = 0;
		XVisualInfo* x11Visual = 0;
		Colormap     x11Colormap = 0;

		/*
		Step 0 - Create a NativeWindowType that we can use it for OpenGL ES output
		*/
		Window sRootWindow;
		XSetWindowAttributes sWA;
		unsigned int ui32Mask;
		int i32Depth;

		// Initializes the display and screen
		x11Display = XOpenDisplay(NULL);
		if (!x11Display && !(x11Display = XOpenDisplay(":0")))
		{
			printf("Error: Unable to open X display\n");
			return;
		}
		x11Screen = XDefaultScreen(x11Display);
		float xdpi = (float)DisplayWidth(x11Display, x11Screen) / DisplayWidthMM(x11Display, x11Screen) * 25.4;
		float ydpi = (float)DisplayHeight(x11Display, x11Screen) / DisplayHeightMM(x11Display, x11Screen) * 25.4;
		screen_dpi = max(xdpi, ydpi);

		// Gets the window parameters
		sRootWindow = RootWindow(x11Display, x11Screen);

		int depth = CopyFromParent;

		#if !defined(GLES)
			// Get a matching FB config
			static int visual_attribs[] =
			{
				GLX_X_RENDERABLE    , True,
				GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
				GLX_RENDER_TYPE     , GLX_RGBA_BIT,
				GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
				GLX_RED_SIZE        , 8,
				GLX_GREEN_SIZE      , 8,
				GLX_BLUE_SIZE       , 8,
				GLX_ALPHA_SIZE      , 8,
				GLX_DEPTH_SIZE      , 24,
				GLX_STENCIL_SIZE    , 8,
				GLX_DOUBLEBUFFER    , True,
				//GLX_SAMPLE_BUFFERS  , 1,
				//GLX_SAMPLES         , 4,
				None
			};

			int glx_major, glx_minor;

			// FBConfigs were added in GLX version 1.3.
			if (!glXQueryVersion(x11Display, &glx_major, &glx_minor) ||
					((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1))
			{
				printf("Invalid GLX version");
				exit(1);
			}

			int fbcount;
			GLXFBConfig* fbc = glXChooseFBConfig(x11Display, x11Screen, visual_attribs, &fbcount);
			if (!fbc)
			{
				printf("Failed to retrieve a framebuffer config\n");
				exit(1);
			}
			printf("Found %d matching FB configs.\n", fbcount);

			GLXFBConfig bestFbc = fbc[0];
			XFree(fbc);

			// Get a visual
			XVisualInfo *vi = glXGetVisualFromFBConfig(x11Display, bestFbc);
			printf("Chosen visual ID = 0x%lx\n", vi->visualid);


			depth = vi->depth;
			x11Visual = vi;

			x11Colormap = XCreateColormap(x11Display, RootWindow(x11Display, x11Screen), vi->visual, AllocNone);
		#else
			i32Depth = DefaultDepth(x11Display, x11Screen);
			x11Visual = new XVisualInfo;
			XMatchVisualInfo(x11Display, x11Screen, i32Depth, TrueColor, x11Visual);
			if (!x11Visual)
			{
				printf("Error: Unable to acquire visual\n");
				return;
			}
			x11Colormap = XCreateColormap(x11Display, sRootWindow, x11Visual->visual, AllocNone);
		#endif

		sWA.colormap = x11Colormap;

		// Add to these for handling other events
		sWA.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
		sWA.event_mask |= PointerMotionMask | FocusChangeMask;
		ui32Mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

		x11_width = cfgLoadInt("x11", "width", DEFAULT_WINDOW_WIDTH);
		x11_height = cfgLoadInt("x11", "height", DEFAULT_WINDOW_HEIGHT);
		x11_fullscreen = cfgLoadBool("x11", "fullscreen", DEFAULT_FULLSCREEN);

		if (x11_width < 0 || x11_height < 0)
		{
			x11_width = XDisplayWidth(x11Display, x11Screen);
			x11_height = XDisplayHeight(x11Display, x11Screen);
		}

		// Creates the X11 window
		x11Window = XCreateWindow(x11Display, RootWindow(x11Display, x11Screen), (ndcid%3)*640, (ndcid/3)*480, x11_width, x11_height,
			0, depth, InputOutput, x11Visual->visual, ui32Mask, &sWA);

		// Capture the close window event
		wmDeleteMessage = XInternAtom(x11Display, "WM_DELETE_WINDOW", False);
		XSetWMProtocols(x11Display, x11Window, &wmDeleteMessage, 1);

		if(x11_fullscreen)
		{

			// fullscreen
			Atom wmState = XInternAtom(x11Display, "_NET_WM_STATE", False);
			Atom wmFullscreen = XInternAtom(x11Display, "_NET_WM_STATE_FULLSCREEN", False);
			XChangeProperty(x11Display, x11Window, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)&wmFullscreen, 1);

			XMapRaised(x11Display, x11Window);
		}
		else
		{
			XMapWindow(x11Display, x11Window);
		}

		#if !defined(GLES)
			#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
			#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
			typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

			glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
			glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
			verify(glXCreateContextAttribsARB != 0);
			int context_attribs[] =
			{
				GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
				GLX_CONTEXT_MINOR_VERSION_ARB, 3,
#ifndef RELEASE
				GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
				GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
				None
			};

			x11_glc = glXCreateContextAttribsARB(x11Display, bestFbc, 0, True, context_attribs);
			if (!x11_glc)
			{
				printf("Open GL 4.3 not supported\n");
				// Try GL 3.1
				context_attribs[1] = 3;
				context_attribs[3] = 1;
				x11_glc = glXCreateContextAttribsARB(x11Display, bestFbc, 0, True, context_attribs);
				if (!x11_glc)
				{
					die("Open GL 3.1 not supported\n");
				}
			}
			XSync(x11Display, False);

		#endif

		XFlush(x11Display);

		//(EGLNativeDisplayType)x11Display;
		x11_disp = (void*)x11Display;
		x11_win = (void*)x11Window;
		x11_vis = (void*)x11Visual->visual;

		x11_window_set_text("Reicast");
	}
	else
	{
		printf("Not creating X11 window ..\n");
	}
}

void x11_window_set_text(const char* text)
{
	if (x11_win)
	{
		XChangeProperty((Display*)x11_disp, (Window)x11_win,
			XInternAtom((Display*)x11_disp, "WM_NAME", False),     //WM_NAME,
			XInternAtom((Display*)x11_disp, "UTF8_STRING", False), //UTF8_STRING,
			8, PropModeReplace, (const unsigned char *)text, strlen(text));
	}
}

void x11_gl_context_destroy()
{
	glXMakeCurrent((Display*)x11_disp, None, NULL);
	glXDestroyContext((Display*)x11_disp, (GLXContext)x11_glc);
}

void x11_window_destroy()
{
	destroy_empty_cursor();

	// close XWindow
	if (x11_win)
	{
		if (!x11_fullscreen)
		{
			cfgSaveInt("x11", "width", x11_width);
			cfgSaveInt("x11", "height", x11_height);
		}
		cfgSaveBool("x11", "fullscreen", x11_fullscreen);
		XDestroyWindow((Display*)x11_disp, (Window)x11_win);
		x11_win = NULL;
	}
	if (x11_disp)
	{
#if !defined(GLES)
		if (x11_glc)
		{
			glXMakeCurrent((Display*)x11_disp, None, NULL);
			glXDestroyContext((Display*)x11_disp, (GLXContext)x11_glc);
			x11_glc = NULL;
		}
#endif
		XCloseDisplay((Display*)x11_disp);
		x11_disp = NULL;
	}
}
#endif
