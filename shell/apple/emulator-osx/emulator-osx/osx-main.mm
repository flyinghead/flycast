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
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif
#include "stdclass.h"
#include "oslib/oslib.h"
#include "emulator.h"
#include "rend/mainui.h"
#include <future>

int darw_printf(const char* text, ...)
{
    va_list args;

    char temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    
    NSString* log = [NSString stringWithCString:temp encoding: NSUTF8StringEncoding];
    static bool isXcode = [[[NSProcessInfo processInfo] environment][@"OS_ACTIVITY_DT_MODE"] boolValue];
    if (isXcode) // Xcode console does not support colors
    {
        log = [log stringByReplacingOccurrencesOfString:@"\x1b[0m" withString:@""];
        log = [log stringByReplacingOccurrencesOfString:@"\x1b[92m" withString:@"ℹ️ "];
        log = [log stringByReplacingOccurrencesOfString:@"\x1b[91m" withString:@"⚠️ "];
        log = [log stringByReplacingOccurrencesOfString:@"\x1b[93m" withString:@"🛑 "];
    }
    NSLog(@"%@", log);

    return 0;
}

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {
#if defined(USE_SDL)
	NSMenuItem *editMenuItem = [[NSApp mainMenu] itemAtIndex:1];
	[editMenuItem setEnabled:SDL_IsTextInputActive()];
#endif
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

void os_TermInput()
{
#if defined(USE_SDL)
	input_sdl_quit();
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
        std::string config_dir = std::string(home) + "/.flycast/";
		if (!file_exists(config_dir))
			config_dir = std::string(home) + "/Library/Application Support/Flycast/";
		
		/* Different config folder for multiple instances */
		if (getppid() == 1)
		{
			int instanceNumber = (int)[[NSRunningApplication runningApplicationsWithBundleIdentifier:[[NSBundle mainBundle] bundleIdentifier]] count];
			if (instanceNumber > 1)
			{
				config_dir += std::to_string(instanceNumber) + "/";
				[[NSApp dockTile] setBadgeLabel:@(instanceNumber).stringValue];
			}
		}

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

	emu_flycast_init();
#ifdef USE_BREAKPAD
	auto async = std::async(std::launch::async, uploadCrashes, "/tmp");
#endif

	mainui_loop();

	sdl_window_destroy();
	emu_flycast_term();
	os_UninstallFaultHandler();

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

void os_RunInstance(int argc, const char *argv[])
{
	if (fork() == 0)
	{
		std::vector<char *> localArgs;
		NSArray *arguments = [[NSProcessInfo processInfo] arguments];
		const char *selfPath = [[arguments objectAtIndex:0] UTF8String];
		localArgs.push_back((char *)selfPath);
		for (int i = 0; i < argc; i++)
			localArgs.push_back((char *)argv[i]);
		localArgs.push_back(nullptr);
		execv(selfPath, &localArgs[0]);
		ERROR_LOG(BOOT, "Error %d launching Flycast instance %s", errno, selfPath);
		die("execv failed");
	}
}

#import <Syphon/Syphon.h>
#import <cfg/cfg.h>
#include "rend/vulkan/vulkan.h"
static SyphonOpenGLServer* syphonGLServer;
static SyphonMetalServer* syphonMtlServer;

void os_VideoRoutingInitSyphonWithGLContext(void* glContext)
{
	int boardID = cfgLoadInt("naomi", "BoardId", 0);
	syphonGLServer = [[SyphonOpenGLServer alloc] initWithName:[NSString stringWithFormat:(boardID == 0 ? @"Video Content" : @"Video Content - %d"), boardID] context:[(__bridge NSOpenGLContext*)glContext CGLContextObj] options:nil];
}

void os_VideoRoutingPublishFrameTexture(GLuint texID, GLuint texTarget, float w, float h)
{
	CGLLockContext([syphonGLServer context]);
	[syphonGLServer publishFrameTexture:texID textureTarget:texTarget imageRegion:NSMakeRect(0, 0, w, h) textureDimensions:NSMakeSize(w, h) flipped:NO];
	CGLUnlockContext([syphonGLServer context]);
}

void os_VideoRoutingTermGL()
{
	[syphonGLServer stop];
	[syphonGLServer release];
	syphonGLServer = NULL;
}

void os_VideoRoutingInitSyphonWithVkDevice(const vk::UniqueDevice& device)
{
	vk::ExportMetalDeviceInfoEXT deviceInfo;
	auto objectsInfo = vk::ExportMetalObjectsInfoEXT(&deviceInfo);
	device->exportMetalObjectsEXT(&objectsInfo);
	
	int boardID = cfgLoadInt("naomi", "BoardId", 0);
	syphonMtlServer = [[SyphonMetalServer alloc] initWithName:[NSString stringWithFormat:(boardID == 0 ? @"Video Content" : @"Video Content - %d"), boardID] device:deviceInfo.mtlDevice options:nil];
}

void os_VideoRoutingPublishFrameTexture(const vk::Device& device, const vk::Image& image, const vk::Queue& queue, float x, float y, float w, float h)
{
	auto textureInfo = vk::ExportMetalTextureInfoEXT(image);
	auto commandInfo = vk::ExportMetalCommandQueueInfoEXT(queue);
	commandInfo.pNext = &textureInfo;
	auto objectsInfo = vk::ExportMetalObjectsInfoEXT(&commandInfo);
	device.exportMetalObjectsEXT(&objectsInfo);
	
	auto commandBuffer = [commandInfo.mtlCommandQueue commandBufferWithUnretainedReferences];
	[syphonMtlServer publishFrameTexture:textureInfo.mtlTexture onCommandBuffer:commandBuffer imageRegion:NSMakeRect(x, y, w, h) flipped:YES];
	[commandBuffer commit];
}

void os_VideoRoutingTermVk()
{
	[syphonMtlServer stop];
	[syphonMtlServer release];
	syphonMtlServer = NULL;
}
