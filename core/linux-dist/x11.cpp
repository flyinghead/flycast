#if defined(SUPPORT_X11)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "types.h"
#include "cfg/cfg.h"
#include "x11.h"
#include "rend/gui.h"
#include "input/gamepad.h"
#include "icon.h"
#include "wsi/context.h"
#include "hw/maple/maple_devs.h"
#include "emulator.h"

#include "x11_keyboard.h"

#if defined(TARGET_PANDORA)
	#define DEFAULT_FULLSCREEN    true
	#define DEFAULT_WINDOW_WIDTH  800
#else
	#define DEFAULT_FULLSCREEN    false
	#define DEFAULT_WINDOW_WIDTH  640
#endif
#define DEFAULT_WINDOW_HEIGHT   480

static Window x11_win;
Display *x11_disp;

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
			input_mapper = std::make_shared<MouseInputMapping>();
	}

	bool gamepad_btn_input(u32 code, bool pressed) override
	{
		if (gui_is_open() && !is_detecting_input())
			// Don't register mouse clicks as gamepad presses when gui is open
			// This makes the gamepad presses to be handled first and the mouse position to be ignored
			// TODO Make this generic
			return false;
		else
			return GamepadDevice::gamepad_btn_input(code, pressed);
	}

	virtual const char *get_button_name(u32 code) override
	{
		switch (code)
		{
		case Button1:
			return "Left Button";
		case Button2:
			return "Middle Button";
		case Button3:
			return "Right Button";
		case Button4:
			return "Scroll Up";
		case Button5:
			return "Scroll Down";
		case 6:
			return "Scroll Left";
		case 7:
			return "Scroll Right";
		case 8:
			return "Button 4";
		case 9:
			return "Button 5";
		default:
			return nullptr;
		}
	}
};

static int x11_keyboard_input = 0;
static std::shared_ptr<X11KeyboardDevice> x11_keyboard;
static std::shared_ptr<X11KbGamepadDevice> kb_gamepad;
static std::shared_ptr<X11MouseGamepadDevice> mouse_gamepad;

int x11_width;
int x11_height;

static bool x11_fullscreen = false;
static Atom wmDeleteMessage;

extern bool dump_frame_switch;

enum
{
	_NET_WM_STATE_REMOVE =0,
	_NET_WM_STATE_ADD = 1,
	_NET_WM_STATE_TOGGLE =2
};

static void x11_window_set_fullscreen(bool fullscreen)
{
		XEvent xev;
		xev.xclient.type         = ClientMessage;
		xev.xclient.window       = x11_win;
		xev.xclient.message_type = XInternAtom(x11_disp, "_NET_WM_STATE", False);
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = 2;    // _NET_WM_STATE_TOGGLE
		xev.xclient.data.l[1] = XInternAtom(x11_disp, "_NET_WM_STATE_FULLSCREEN", True);
		xev.xclient.data.l[2] = 0;    // no second property to toggle
		xev.xclient.data.l[3] = 1;
		xev.xclient.data.l[4] = 0;

		INFO_LOG(RENDERER, "x11: setting fullscreen to %d", fullscreen);
		XSendEvent(x11_disp, DefaultRootWindow(x11_disp), False, SubstructureNotifyMask, &xev);
}

void event_x11_handle()
{
	XEvent event;

	while(XPending(x11_disp))
	{
		XNextEvent(x11_disp, &event);

		if (event.type == ClientMessage &&
				(unsigned long)event.xclient.data.l[0] == wmDeleteMessage)
			dc_exit();
		else if (event.type == ConfigureNotify)
		{
			x11_width = event.xconfigure.width;
			x11_height = event.xconfigure.height;
		}
	}
}

static bool capturing_mouse;
static Cursor empty_cursor = None;

static Cursor create_empty_cursor()
{
	if (empty_cursor == None)
	{
		char data[] = { 0 };

		XColor color;
		color.red = color.green = color.blue = 0;

		Pixmap pixmap = XCreateBitmapFromData(x11_disp, DefaultRootWindow(x11_disp),
				data, 1, 1);
		if (pixmap)
		{
			empty_cursor = XCreatePixmapCursor(x11_disp, pixmap, pixmap, &color, &color, 0, 0);
			XFreePixmap(x11_disp, pixmap);
		}
	}
	return empty_cursor;
}

static void destroy_empty_cursor()
{
	if (empty_cursor != None)
	{
		XFreeCursor(x11_disp, empty_cursor);
		empty_cursor = None;
	}
}

static void x11_capture_mouse()
{
	x11_window_set_text("Flycast - mouse capture");
	capturing_mouse = true;
	Cursor cursor = create_empty_cursor();
	XDefineCursor(x11_disp, x11_win, cursor);
	XGrabPointer(x11_disp, x11_win, False,
			ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask,
			GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

static void x11_uncapture_mouse()
{
	x11_window_set_text("Flycast");
	capturing_mouse = false;
	XUndefineCursor(x11_disp, x11_win);
	XUngrabPointer(x11_disp, CurrentTime);
}

void input_x11_handle()
{
	//Handle X11
	bool mouse_moved = false;
	XEvent e;

	while (XCheckWindowEvent(x11_disp, x11_win,
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
					if (e.type == KeyRelease && XEventsQueued(x11_disp, QueuedAfterReading))
					{
						XEvent nev;
						XPeekEvent(x11_disp, &nev);

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
						if (e.xkey.keycode == KEY_F10)
						{
							// Dump the next frame into a file
							dump_frame_switch = e.type == KeyPress;
						}
						else if (e.type == KeyPress && e.xkey.keycode == KEY_F11)
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
				SetMousePosition(e.xmotion.x, e.xmotion.y, x11_width, x11_height);
				// For mouse
				mouse_moved = true;

				break;
		}
	}
	if (gui_is_open() && capturing_mouse)
	{
		x11_uncapture_mouse();
		capturing_mouse = false;
	}
	if (capturing_mouse && mouse_moved)
	{
		mo_x_prev = x11_width / 2;
		mo_y_prev = x11_height / 2;
		XWarpPointer(x11_disp, None, x11_win, 0, 0, 0, 0,
				mo_x_prev, mo_y_prev);
		XSync(x11_disp, true);
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
		INFO_LOG(INPUT, "X11 Keyboard input disabled by config.");
}

void x11_window_create()
{
	if (cfgLoadInt("pvr", "nox11", 0) == 0)
	{
		XInitThreads();

		// Initializes the display and screen
		x11_disp = XOpenDisplay(NULL);
		if (x11_disp == nullptr && (x11_disp = XOpenDisplay(":0")) == nullptr)
		{
			ERROR_LOG(RENDERER, "Error: Unable to open X display");
			return;
		}
		int x11Screen = XDefaultScreen(x11_disp);
		float xdpi = (float)DisplayWidth(x11_disp, x11Screen) / DisplayWidthMM(x11_disp, x11Screen) * 25.4;
		float ydpi = (float)DisplayHeight(x11_disp, x11Screen) / DisplayHeightMM(x11_disp, x11Screen) * 25.4;
		screen_dpi = std::max(xdpi, ydpi);

		int depth = CopyFromParent;

		XVisualInfo* x11Visual = nullptr;
		Colormap     x11Colormap = 0;
#if !defined(GLES)

		if (!theGLContext.ChooseVisual(x11_disp, &x11Visual, &depth))
			exit(1);
		x11Colormap = XCreateColormap(x11_disp, RootWindow(x11_disp, x11Screen), x11Visual->visual, AllocNone);
#else
		int i32Depth = DefaultDepth(x11_disp, x11Screen);
		x11Visual = new XVisualInfo;
		if (!XMatchVisualInfo(x11_disp, x11Screen, i32Depth, TrueColor, x11Visual))
		{
			ERROR_LOG(RENDERER, "Error: Unable to acquire visual");
			delete x11Visual;
			return;
		}
		// Gets the window parameters
		Window sRootWindow = RootWindow(x11_disp, x11Screen);
		x11Colormap = XCreateColormap(x11_disp, sRootWindow, x11Visual->visual, AllocNone);
#endif
		XSetWindowAttributes sWA;
		sWA.colormap = x11Colormap;

		// Add to these for handling other events
		sWA.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
		sWA.event_mask |= PointerMotionMask | FocusChangeMask;
		unsigned long ui32Mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

		x11_width = cfgLoadInt("window", "width", 0);
		if (x11_width == 0)
			x11_width = cfgLoadInt("x11", "width", DEFAULT_WINDOW_WIDTH);
		x11_height = cfgLoadInt("window", "height", 0);
		x11_fullscreen = cfgLoadBool("window", "fullscreen", DEFAULT_FULLSCREEN);
		if (x11_height == 0)
		{
			x11_height = cfgLoadInt("x11", "height", DEFAULT_WINDOW_HEIGHT);
			x11_fullscreen = cfgLoadBool("x11", "fullscreen", DEFAULT_FULLSCREEN);
		}

		if (x11_width < 0 || x11_height < 0)
		{
			x11_width = XDisplayWidth(x11_disp, x11Screen);
			x11_height = XDisplayHeight(x11_disp, x11Screen);
		}

		// Creates the X11 window
		x11_win = XCreateWindow(x11_disp, RootWindow(x11_disp, x11Screen), 0, 0, x11_width, x11_height,
			0, depth, InputOutput, x11Visual->visual, ui32Mask, &sWA);
#if !defined(GLES)
		XFree(x11Visual);
#else
		delete x11Visual;
#endif

		XSetWindowBackground(x11_disp, x11_win, 0);

		Atom net_wm_icon = XInternAtom(x11_disp, "_NET_WM_ICON", False);
		Atom cardinal = XInternAtom(x11_disp, "CARDINAL", False);
		XChangeProperty(x11_disp, x11_win, net_wm_icon, cardinal, 32, PropModeReplace,
				(const unsigned char*)reicast_icon, sizeof(reicast_icon) / sizeof(*reicast_icon));

		// Capture the close window event
		wmDeleteMessage = XInternAtom(x11_disp, "WM_DELETE_WINDOW", False);
		XSetWMProtocols(x11_disp, x11_win, &wmDeleteMessage, 1);

		if (x11_fullscreen)
		{
			Atom wmState = XInternAtom(x11_disp, "_NET_WM_STATE", False);
			Atom wmFullscreen = XInternAtom(x11_disp, "_NET_WM_STATE_FULLSCREEN", False);
			XChangeProperty(x11_disp, x11_win, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)&wmFullscreen, 1);

			XMapRaised(x11_disp, x11_win);
		}
		else
		{
			XMapWindow(x11_disp, x11_win);
		}
		theGLContext.SetDisplayAndWindow(x11_disp, x11_win);
#ifdef USE_VULKAN
		theVulkanContext.SetWindow((void *)x11_win, (void *)x11_disp);
#endif
		InitRenderApi();

		XFlush(x11_disp);

		x11_window_set_text("Flycast");
	}
	else
	{
		INFO_LOG(RENDERER, "Not creating X11 window ..");
	}
}

void x11_window_set_text(const char* text)
{
	if (x11_win)
	{
		XStoreName(x11_disp, x11_win, text);
		XSetIconName(x11_disp, x11_win, text);

		XClassHint hint = { (char *)"WM_CLASS", (char *)text };
		XSetClassHint(x11_disp, x11_win, &hint);
	}
}

void x11_window_destroy()
{
	destroy_empty_cursor();
	TermRenderApi();

	// close XWindow
	if (x11_win)
	{
		if (!x11_fullscreen)
		{
			cfgSaveInt("window", "width", x11_width);
			cfgSaveInt("window", "height", x11_height);
		}
		cfgSaveBool("window", "fullscreen", x11_fullscreen);
		XDestroyWindow(x11_disp, x11_win);
		x11_win = (Window)0;
	}
	if (x11_disp)
	{
		XCloseDisplay(x11_disp);
		x11_disp = nullptr;
	}
}
#endif
