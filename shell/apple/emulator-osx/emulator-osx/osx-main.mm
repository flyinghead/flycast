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
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>

#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "log/LogManager.h"
#include "rend/gui.h"
#include "osx_keyboard.h"
#include "osx_gamepad.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif
#include "stdclass.h"
#include "wsi/context.h"
#include "emulator.h"

OSXKeyboardDevice keyboard(0);
static std::shared_ptr<OSXKbGamepadDevice> kb_gamepad(0);
static std::shared_ptr<OSXMouseGamepadDevice> mouse_gamepad(0);

int darw_printf(const char* text, ...)
{
    va_list args;

    char temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

int get_mic_data(u8* buffer) { return 0; }

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
#if 0
    int ret = task_set_exception_ports(
                                       mach_task_self(),
                                       EXC_MASK_BAD_ACCESS,
                                       MACH_PORT_NULL,
                                       EXCEPTION_DEFAULT,
                                       0);
    
    if (ret != KERN_SUCCESS) {
        printf("task_set_exception_ports: %s\n", mach_error_string(ret));
    }
#endif
}

void os_SetupInput()
{
#if defined(USE_SDL)
	input_sdl_init();
#endif

	kb_gamepad = std::make_shared<OSXKbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
	mouse_gamepad = std::make_shared<OSXMouseGamepadDevice>(0);
	GamepadDevice::Register(mouse_gamepad);
}

void common_linux_setup();
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
        std::string config_dir = std::string(home) + "/.reicast";
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
        add_system_data_dir(std::string(path));
    CFRelease(resourcesURL);
    CFRelease(mainBundle);

	// Calculate screen DPI
	NSScreen *screen = [NSScreen mainScreen];
	NSDictionary *description = [screen deviceDescription];
    CGDirectDisplayID displayID = [[description objectForKey:@"NSScreenNumber"] unsignedIntValue];
	CGSize displayPhysicalSize = CGDisplayScreenSize(displayID);
    
    //Neither CGDisplayScreenSize(description's NSScreenNumber) nor [NSScreen backingScaleFactor] can calculate the correct dpi in macOS. E.g. backingScaleFactor is always 2 in all display modes for rMBP 16"
    NSSize displayNativeSize;
    CFArrayRef allDisplayModes = CGDisplayCopyAllDisplayModes(displayID, NULL);
    CFIndex n = CFArrayGetCount(allDisplayModes);
    for(int i = 0; i < n; ++i)
    {
        CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(allDisplayModes, i);
        if(CGDisplayModeGetIOFlags(m) & kDisplayModeNativeFlag)
        {
            displayNativeSize.width = CGDisplayModeGetPixelWidth(m);
            displayNativeSize.height = CGDisplayModeGetPixelHeight(m);
            break;
        }
    }
    CFRelease(allDisplayModes);
    
	screen_dpi = (int)(displayNativeSize.width / displayPhysicalSize.width * 25.4f);
	screen_width = width;
	screen_height = height;

	InitRenderApi();
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
