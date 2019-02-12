//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//
#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>

#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "rend/gui.h"
#include <sys/stat.h>

#include <OpenGL/gl3.h>

static void init_kb_map();

int msgboxf(const wchar* text,unsigned int type,...)
{
    va_list args;

    wchar temp[2048];
    va_start(args, type);
    vsprintf(temp, text, args);
    va_end(args);

    puts(temp);
    return 0;
}

int darw_printf(const wchar* text,...) {
    va_list args;

    wchar temp[2048];
    va_start(args, text);
    vsprintf(temp, text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

u16 kcode[4] = { 0xFFFF };
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];
extern u8 kb_key[6];            // normal keys pressed
extern u8 kb_shift;             // shift keys pressed (bitmask)
static int kb_used;
static u8 kb_map[256];

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {

}


void UpdateInputState(u32 port) {

}

void UpdateVibration(u32 port, u32 value) {

}

void os_CreateWindow() {

}

void os_SetupInput() {
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	mcfg_CreateDevices();
#endif
	init_kb_map();
}

void* libPvr_GetRenderTarget() {
    return 0;
}

void* libPvr_GetRenderSurface() {
    return 0;

}

bool gl_init(void*, void*) {
    return true;
}

void gl_term() {

}

void gl_swap() {

}

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();
void dc_term();
void dc_stop();

bool has_init = false;
void* emuthread(void*) {
    settings.profile.run_counts=0;
    common_linux_setup();
    char* argv[] = { "reicast" };

    dc_init(1,argv);

    has_init = true;

    dc_run();

    has_init = false;

    dc_term();
	[[NSApplication sharedApplication] terminate:NULL];
	
    return 0;
}

extern "C" void emu_dc_stop()
{
    dc_stop();
}

pthread_t emu_thread;
extern "C" void emu_main() {
    pthread_create(&emu_thread, 0, &emuthread, 0);
}

extern int screen_width,screen_height;
bool rend_single_frame();
bool rend_framePending();
bool gles_init();

extern "C" bool emu_frame_pending()
{
	return rend_framePending() || gui_is_open();
}

extern "C" int emu_single_frame(int w, int h) {
    if (!has_init)
        return true;
    if (!emu_frame_pending())
        return 0;
    screen_width = w;
    screen_height = h;

    return rend_single_frame();
}

extern "C" void emu_gles_init() {
    char *home = getenv("HOME");
    if (home != NULL)
    {
        string config_dir = string(home) + "/.reicast";
        mkdir(config_dir.c_str(), 0755); // create the directory if missing
        set_user_config_dir(config_dir);
        set_user_data_dir(config_dir);
    }
    else
    {
        set_user_config_dir(".");
        set_user_data_dir(".");
    }
    // Add bundle resources path
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
        add_system_data_dir(string(path));
    CFRelease(resourcesURL);
    CFRelease(mainBundle);

    gles_init();
}

enum DCPad {
    Btn_C		= 1,
    Btn_B		= 1<<1,
    Btn_A		= 1<<2,
    Btn_Start	= 1<<3,
    DPad_Up		= 1<<4,
    DPad_Down	= 1<<5,
    DPad_Left	= 1<<6,
    DPad_Right	= 1<<7,
    Btn_Z		= 1<<8,
    Btn_Y		= 1<<9,
    Btn_X		= 1<<10,
    Btn_D		= 1<<11,
    DPad2_Up	= 1<<12,
    DPad2_Down	= 1<<13,
    DPad2_Left	= 1<<14,
    DPad2_Right	= 1<<15,

    Axis_LT= 0x10000,
    Axis_RT= 0x10001,
    Axis_X= 0x20000,
    Axis_Y= 0x20001,
};

static void handle_key(int dckey, int state) {
    if (state)
        kcode[0] &= ~dckey;
    else
        kcode[0] |= dckey;
}

static void handle_trig(u8* dckey, int state) {
    if (state)
        dckey[0] = 255;
    else
        dckey[0] = 0;
}

bool dc_loadstate(void);
bool dc_savestate(void);

extern "C" void emu_key_input(UInt16 keyCode, int state, UInt modifierFlags) {
    switch(keyCode) {
        // S
        case kVK_ANSI_S:     handle_key(Btn_X, state); break;
        // D
        case kVK_ANSI_D:     handle_key(Btn_Y, state); break;
        // C
        case kVK_ANSI_C:     handle_key(Btn_B, state); break;
        // X
        case kVK_ANSI_X:     handle_key(Btn_A, state); break;

        // F
        case kVK_ANSI_F:     handle_trig(lt, state); break;
        // V
        case kVK_ANSI_V:     handle_trig(rt, state); break;

        // Left arrow
        case kVK_LeftArrow: handle_key(DPad_Left, state); break;
        // Down arrow
        case kVK_DownArrow: handle_key(DPad_Down, state); break;
        // Right arrow
        case kVK_RightArrow: handle_key(DPad_Right, state); break;
        // Up arrow
        case kVK_UpArrow:	handle_key(DPad_Up, state); break;
        // Enter
        case kVK_Return:    handle_key(Btn_Start, state); break;
		// F2
		case kVK_F2:     	dc_savestate(); break;
		// F4
		case kVK_F4:     	dc_loadstate(); break;
		// Tab
		case kVK_Tab:     	gui_open_settings(); break;
    }
	u8 dc_keycode = kb_map[keyCode & 0xff];
	if (dc_keycode != 0)
	{
		if (state == 1)
		{
			if (kb_used < 6)
			{
				bool found = false;
				for (int i = 0; !found && i < 6; i++)
				{
					if (kb_key[i] == dc_keycode)
						found = true;
				}
				if (!found)
				{
					kb_key[kb_used] = dc_keycode;
					kb_used++;
				}
			}
		}
		else
		{
			if (kb_used > 0)
			{
				for (int i = 0; i < 6; i++)
				{
					if (kb_key[i] == dc_keycode)
					{
						kb_used--;
						for (int j = i; j < 5; j++)
							kb_key[j] = kb_key[j + 1];
						kb_key[5] = 0;
					}
				}
			}
		}
	}
	if (modifierFlags & NSEventModifierFlagShift)
	{
		if (state == 0)
			kb_shift &= ~(0x02 | 0x20);
		else
			kb_shift |= 0x02 | 0x20;
	}
	if (modifierFlags & NSEventModifierFlagControl)
	{
		if (state == 0)
			kb_shift &= ~(0x01 | 0x10);
		else
			kb_shift |= 0x01 | 0x10;
	}
}

static void init_kb_map()
{
	//04-1D Letter keys A-Z (in alphabetic order)
	kb_map[kVK_ANSI_A] = 0x04;
	kb_map[kVK_ANSI_B] = 0x05;
	kb_map[kVK_ANSI_C] = 0x06;
	kb_map[kVK_ANSI_D] = 0x07;
	kb_map[kVK_ANSI_E] = 0x08;
	kb_map[kVK_ANSI_F] = 0x09;
	kb_map[kVK_ANSI_G] = 0x0A;
	kb_map[kVK_ANSI_H] = 0x0B;
	kb_map[kVK_ANSI_I] = 0x0C;
	kb_map[kVK_ANSI_J] = 0x0D;
	kb_map[kVK_ANSI_K] = 0x0E;
	kb_map[kVK_ANSI_L] = 0x0F;
	kb_map[kVK_ANSI_M] = 0x10;
	kb_map[kVK_ANSI_N] = 0x11;
	kb_map[kVK_ANSI_O] = 0x12;
	kb_map[kVK_ANSI_P] = 0x13;
	kb_map[kVK_ANSI_Q] = 0x14;
	kb_map[kVK_ANSI_R] = 0x15;
	kb_map[kVK_ANSI_S] = 0x16;
	kb_map[kVK_ANSI_T] = 0x17;
	kb_map[kVK_ANSI_U] = 0x18;
	kb_map[kVK_ANSI_V] = 0x19;
	kb_map[kVK_ANSI_W] = 0x1A;
	kb_map[kVK_ANSI_X] = 0x1B;
	kb_map[kVK_ANSI_Y] = 0x1C;
	kb_map[kVK_ANSI_Z] = 0x1D;
	
	//1E-27 Number keys 1-0
	kb_map[kVK_ANSI_1] = 0x1E;
	kb_map[kVK_ANSI_2] = 0x1F;
	kb_map[kVK_ANSI_3] = 0x20;
	kb_map[kVK_ANSI_4] = 0x21;
	kb_map[kVK_ANSI_5] = 0x22;
	kb_map[kVK_ANSI_6] = 0x23;
	kb_map[kVK_ANSI_7] = 0x24;
	kb_map[kVK_ANSI_8] = 0x25;
	kb_map[kVK_ANSI_9] = 0x26;
	kb_map[kVK_ANSI_0] = 0x27;
	
	kb_map[kVK_Return] = 0x28;
	kb_map[kVK_Escape] = 0x29;
	kb_map[kVK_Delete] = 0x2A;
	kb_map[kVK_Tab] = 0x2B;
	kb_map[kVK_Space] = 0x2C;
	
	kb_map[kVK_ANSI_Minus] = 0x2D;      // -
	kb_map[kVK_ANSI_Equal] = 0x2E;     // =
	kb_map[kVK_ANSI_LeftBracket] = 0x2F;        // [
	kb_map[kVK_ANSI_RightBracket] = 0x30;       // ]
	
	kb_map[kVK_ANSI_Backslash] = 0x31;  // \ (US) unsure of keycode
	
	//32-34 "]", ";" and ":" (the 3 keys right of L)
	//kb_map[?] = 0x32;   // ~ (non-US) *,µ in FR layout
	kb_map[kVK_ANSI_Semicolon] = 0x33;  // ;
	kb_map[kVK_ANSI_Quote] = 0x34;      // '
	
	//35 hankaku/zenkaku / kanji (top left)
	kb_map[kVK_ANSI_Grave] = 0x35;  // `~ (US)
	
	//36-38 ",", "." and "/" (the 3 keys right of M)
	kb_map[kVK_ANSI_Comma] = 0x36;
	kb_map[kVK_ANSI_Period] = 0x37;
	kb_map[kVK_ANSI_Slash] = 0x38;
	
	// CAPSLOCK
	kb_map[kVK_CapsLock] = 0x39;
	
	//3A-45 Function keys F1-F12
	kb_map[kVK_F1] = 0x3A;
	kb_map[kVK_F2] = 0x3B;
	kb_map[kVK_F3] = 0x3C;
	kb_map[kVK_F4] = 0x3D;
	kb_map[kVK_F5] = 0x3E;
	kb_map[kVK_F6] = 0x3F;
	kb_map[kVK_F7] = 0x40;
	kb_map[kVK_F8] = 0x41;
	kb_map[kVK_F9] = 0x42;
	kb_map[kVK_F10] = 0x43;
	kb_map[kVK_F11] = 0x44;
	kb_map[kVK_F12] = 0x45;
	
	//46-4E Control keys above cursor keys
	kb_map[kVK_F13] = 0x46;         // Print Screen
	kb_map[kVK_F14] = 0x47;         // Scroll Lock
	kb_map[kVK_F15] = 0x48;         // Pause
	kb_map[kVK_Help] = 0x49;		// Insert
	kb_map[kVK_Home] = 0x4A;
	kb_map[kVK_PageUp] = 0x4B;
	kb_map[kVK_ForwardDelete] = 0x4C;
	kb_map[kVK_End] = 0x4D;
	kb_map[kVK_PageDown] = 0x4E;
	
	//4F-52 Cursor keys
	kb_map[kVK_RightArrow] = 0x4F;
	kb_map[kVK_LeftArrow] = 0x50;
	kb_map[kVK_DownArrow] = 0x51;
	kb_map[kVK_UpArrow] = 0x52;
	
	//53 Num Lock (Numeric keypad)
	kb_map[kVK_ANSI_KeypadClear] = 0x53;
	//54 "/" (Numeric keypad)
	kb_map[kVK_ANSI_KeypadDivide] = 0x54;
	//55 "*" (Numeric keypad)
	kb_map[kVK_ANSI_KeypadMultiply] = 0x55;
	//56 "-" (Numeric keypad)
	kb_map[kVK_ANSI_KeypadMinus] = 0x56;
	//57 "+" (Numeric keypad)
	kb_map[kVK_ANSI_KeypadPlus] = 0x57;
	//58 Enter (Numeric keypad)
	kb_map[kVK_ANSI_KeypadEnter] = 0x58;
	//59-62 Number keys 1-0 (Numeric keypad)
	kb_map[kVK_ANSI_Keypad1] = 0x59;
	kb_map[kVK_ANSI_Keypad2] = 0x5A;
	kb_map[kVK_ANSI_Keypad3] = 0x5B;
	kb_map[kVK_ANSI_Keypad4] = 0x5C;
	kb_map[kVK_ANSI_Keypad5] = 0x5D;
	kb_map[kVK_ANSI_Keypad6] = 0x5E;
	kb_map[kVK_ANSI_Keypad7] = 0x5F;
	kb_map[kVK_ANSI_Keypad8] = 0x60;
	kb_map[kVK_ANSI_Keypad9] = 0x61;
	kb_map[kVK_ANSI_Keypad0] = 0x62;
	//63 "." (Numeric keypad)
	kb_map[kVK_ANSI_KeypadDecimal] = 0x63;
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
