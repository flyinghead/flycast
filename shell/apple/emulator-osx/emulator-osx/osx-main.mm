//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//
#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>
#include <sys/stat.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>

#include "types.h"
#include "log/LogManager.h"
#include "rend/gui.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif
#include "stdclass.h"
#include "oslib/oslib.h"
#include "emulator.h"
#include "rend/mainui.h"

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

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {
}

void UpdateInputState() {
#if defined(USE_SDL)
	input_sdl_handle();
#endif
}

void os_CreateWindow() {
#ifdef DEBUG
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
	sdl_window_create();
}

void os_SetupInput()
{
#if defined(USE_SDL)
	input_sdl_init();
#endif
}

void common_linux_setup();
static int emu_flycast_init();

static void emu_flycast_term()
{
	flycast_term();
	LogManager::Shutdown();
}

extern "C" int SDL_main(int argc, char *argv[])
{
    char *home = getenv("HOME");
    if (home != NULL)
    {
        std::string config_dir = std::string(home) + "/.reicast/";
        if (!file_exists(config_dir))
            config_dir = std::string(home) + "/.flycast/";
		if (!file_exists(config_dir))
			config_dir = std::string(home) + "/Library/Application Support/Flycast/";
        mkdir(config_dir.c_str(), 0755); // create the directory if missing
        set_user_config_dir(config_dir);
        add_system_data_dir(config_dir);
        config_dir += "data/";
        mkdir(config_dir.c_str(), 0755);
        set_user_data_dir(config_dir);
    }
    else
    {
        set_user_config_dir("./");
        set_user_data_dir("./");
    }
    // Add bundle resources path
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
        add_system_data_dir(std::string(path) + "/");
    CFRelease(resourcesURL);
    CFRelease(mainBundle);

    // Calculate screen DPI
    /*
    NSScreen *screen = [NSScreen mainScreen];
    NSDictionary *description = [screen deviceDescription];
    CGDirectDisplayID displayID = [[description objectForKey:@"NSScreenNumber"] unsignedIntValue];
	CGSize displayPhysicalSize = CGDisplayScreenSize(displayID);

	//Neither CGDisplayScreenSize(description's NSScreenNumber) nor [NSScreen backingScaleFactor] can calculate the correct dpi in macOS. E.g. backingScaleFactor is always 2 in all display modes for rMBP 16"
	NSSize displayNativeSize;
	CFStringRef dmKeys[1] = { kCGDisplayShowDuplicateLowResolutionModes };
	CFBooleanRef dmValues[1] = { kCFBooleanTrue };
	CFDictionaryRef dmOptions = CFDictionaryCreate(kCFAllocatorDefault, (const void**) dmKeys, (const void**) dmValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	CFArrayRef allDisplayModes = CGDisplayCopyAllDisplayModes(displayID, dmOptions);
	CFIndex n = CFArrayGetCount(allDisplayModes);
	for (CFIndex i = 0; i < n; ++i)
	{
		CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(allDisplayModes, i);
		CGFloat width = CGDisplayModeGetPixelWidth(m);
		CGFloat height = CGDisplayModeGetPixelHeight(m);
		CGFloat modeWidth = CGDisplayModeGetWidth(m);

		//Only check 1x mode
		if (width == modeWidth)
		{
			if (CGDisplayModeGetIOFlags(m) & kDisplayModeNativeFlag)
			{
				displayNativeSize.width = width;
				displayNativeSize.height = height;
				break;
			}
			//Get the largest size even if kDisplayModeNativeFlag is not present e.g. iMac 27-Inch with 5K Retina
			if (width > displayNativeSize.width)
			{
				displayNativeSize.width = width;
				displayNativeSize.height = height;
			}
		}

	}
	CFRelease(allDisplayModes);
	CFRelease(dmOptions);

	screen_dpi = (int)(displayNativeSize.width / displayPhysicalSize.width * 25.4f);
	NSSize displayResolution;
	displayResolution.width = CGDisplayPixelsWide(displayID);
	displayResolution.height = CGDisplayPixelsHigh(displayID);
	scaling = displayNativeSize.width / displayResolution.width;
	*/

	emu_flycast_init();

	mainui_loop();

	emu_flycast_term();
	os_UninstallFaultHandler();
	sdl_window_destroy();

	return 0;
}

static int emu_flycast_init()
{
	LogManager::Init();
	common_linux_setup();
	NSArray *arguments = [[NSProcessInfo processInfo] arguments];
	unsigned long argc = [arguments count];
	char **argv = (char **)malloc(argc * sizeof(char*));
	int paramCount = 0;
	for (unsigned long i = 0; i < argc; i++)
	{
		const char *arg = [[arguments objectAtIndex:i] UTF8String];
		if (!strncmp(arg, "-psn_", 5))
			// ignore Process Serial Number argument on first launch
			continue;
		argv[paramCount++] = strdup(arg);
	}
	
	int rc = flycast_init(paramCount, argv);
	
	for (unsigned long i = 0; i < paramCount; i++)
		free(argv[i]);
	free(argv);
	
	return rc;
}

std::string os_Locale(){
    return [[[NSLocale preferredLanguages] objectAtIndex:0] UTF8String];
}

std::string os_PrecomposedString(std::string string){
    return [[[NSString stringWithUTF8String:string.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}
