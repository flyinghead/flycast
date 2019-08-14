//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//
#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>
#include <OpenGL/gl3.h>
#include <sys/stat.h>

#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "log/LogManager.h"
#include "rend/gui.h"
#include "osx_keyboard.h"
#include "osx_gamepad.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif

OSXKeyboardDevice keyboard(0);
static std::shared_ptr<OSXKbGamepadDevice> kb_gamepad(0);
static std::shared_ptr<OSXMouseGamepadDevice> mouse_gamepad(0);

int darw_printf(const wchar* text, ...)
{
    va_list args;

    wchar temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

u16 kcode[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {
}

void UpdateInputState(u32 port) {
#if defined(USE_SDL)
	input_sdl_handle(port);
#endif
}

void os_CreateWindow() {
}

void os_SetupInput()
{
#if defined(USE_SDL)
	if (SDL_Init(0) != 0)
	{
		die("SDL: Initialization failed!");
	}
	input_sdl_init();
#endif

	kb_gamepad = std::make_shared<OSXKbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
	mouse_gamepad = std::make_shared<OSXMouseGamepadDevice>(0);
	GamepadDevice::Register(mouse_gamepad);
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

void common_linux_setup();
int reicast_init(int argc, char* argv[]);
void dc_exit();
void dc_resume();
void rend_init_renderer();

extern "C" void emu_dc_exit()
{
    dc_exit();
}

extern "C" void emu_dc_resume()
{
	dc_resume();
}

extern int screen_width,screen_height;
bool rend_single_frame();
bool rend_framePending();

extern "C" bool emu_frame_pending()
{
	return rend_framePending() || gui_is_open();
}

extern "C" int emu_single_frame(int w, int h) {
    if (!emu_frame_pending())
        return 0;
    screen_width = w;
    screen_height = h;

    return rend_single_frame();
}

extern "C" void emu_gles_init(int width, int height) {
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

	// Calculate screen DPI
	NSScreen *screen = [NSScreen mainScreen];
	NSDictionary *description = [screen deviceDescription];
	NSSize displayPixelSize = [[description objectForKey:NSDeviceSize] sizeValue];
	CGSize displayPhysicalSize = CGDisplayScreenSize([[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);
	screen_dpi = (int)(displayPixelSize.width / displayPhysicalSize.width) * 25.4f;
	screen_width = width;
	screen_height = height;

	rend_init_renderer();
}

extern "C" int emu_reicast_init()
{
	LogManager::Init();
	common_linux_setup();
	NSArray *arguments = [[NSProcessInfo processInfo] arguments];
	unsigned long argc = [arguments count];
	char **argv = (char **)malloc(argc * sizeof(char*));
	for (unsigned long i = 0; i < argc; i++)
		argv[i] = strdup([[arguments objectAtIndex:i] UTF8String]);
	
	int rc = reicast_init((int)argc, argv);
	
	for (unsigned long i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
	
	return rc;
}

extern "C" void emu_key_input(UInt16 keyCode, bool pressed, UInt modifierFlags) {
	keyboard.keyboard_input(keyCode, pressed, keyboard.convert_modifier_keys(modifierFlags));
	if ((modifierFlags
		 & (NSEventModifierFlagShift | NSEventModifierFlagControl | NSEventModifierFlagOption | NSEventModifierFlagCommand)) == 0)
		kb_gamepad->gamepad_btn_input(keyCode, pressed);
}
extern "C" void emu_character_input(const char *characters) {
	if (characters != NULL)
		while (*characters != '\0')
			keyboard.keyboard_character(*characters++);
}

extern "C" void emu_mouse_buttons(int button, bool pressed)
{
	mouse_gamepad->gamepad_btn_input(button, pressed);
}
