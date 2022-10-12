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

#import <SystemConfiguration/SystemConfiguration.h>

int darw_printf(const char* text, ...)
{
    va_list args;

    char temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    
    NSString* log = [NSString stringWithCString:temp encoding: NSUTF8StringEncoding];
    if (getenv("TERM") == NULL) //Xcode console does not support colors
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
	//For settings.dreamcast.ContentPath.emplace_back("./"), Since macOS app bundle cwd is at "/"
	chdir([[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] cStringUsingEncoding:NSUTF8StringEncoding]);
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
        std::string config_dir = std::string(home) + "/.reicast/";
        if (!file_exists(config_dir))
            config_dir = std::string(home) + "/.flycast/";
		if (!file_exists(config_dir))
			config_dir = std::string(home) + "/Library/Application Support/Flycast/";

        /* Different config folder for multiple instances */
        int instanceNumber = (int)[[NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.flyinghead.Flycast"] count];
		if (instanceNumber > 1)
		{
			config_dir += std::to_string(instanceNumber) + "/";
			[[NSApp dockTile] setBadgeLabel:@(instanceNumber).stringValue];
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

void os_LaunchFromURL(const std::string& url) {
    NSString *urlString = [NSString stringWithUTF8String:url.c_str()];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:urlString]];
}

std::string os_FetchStringFromURL(const std::string& url) {
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block std::string result;

    NSURL *URL = [NSURL URLWithString:[[NSString alloc] initWithCString:url.c_str() encoding:NSASCIIStringEncoding]];
    NSURLRequest *request = [NSURLRequest requestWithURL:URL];

    NSURLSession *session = [NSURLSession sharedSession];
    NSURLSessionDataTask *task = [session dataTaskWithRequest:request
                                            completionHandler:
                                  ^(NSData *data, NSURLResponse *response, NSError *error) {
        if(error == nil) {
            NSString* str = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            result = std::string([str UTF8String], [str lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
        }
        dispatch_semaphore_signal(sem);
    }];

    [task resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

    return result;
}

std::string os_GetMachineID(){
    io_service_t    platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,IOServiceMatching("IOPlatformExpertDevice"));
    CFStringRef serialNumberAsCFString = NULL;

    if (platformExpert) {
        serialNumberAsCFString =
        (CFStringRef)IORegistryEntryCreateCFProperty(platformExpert,
                                        CFSTR(kIOPlatformSerialNumberKey),
                                        kCFAllocatorDefault, 0);
        IOObjectRelease(platformExpert);
        if (serialNumberAsCFString) {
            CFIndex bufferSize = CFStringGetLength(serialNumberAsCFString);
            bufferSize = CFStringGetMaximumSizeForEncoding(bufferSize, kCFStringEncodingUTF8);
            char value[bufferSize+1];

            if (CFStringGetCString(serialNumberAsCFString, value, bufferSize, kCFStringEncodingUTF8))
            {
                std::string s;
                s += value;
                return s;
            }
        }
    }
    return "";
}

NSString* runCommand(NSString* commandToRun) {
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:@"/bin/sh"];

    NSArray *arguments = [NSArray arrayWithObjects:
                          @"-c" ,
                          [NSString stringWithFormat:@"%@", commandToRun],
                          nil];
    [task setArguments:arguments];

    NSPipe *pipe = [NSPipe pipe];
    [task setStandardOutput:pipe];

    NSFileHandle *file = [pipe fileHandleForReading];

    [task launch];

    NSData *data = [file readDataToEndOfFile];

    NSString *output = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return output;
}

std::string os_GetConnectionMedium() {
    NSString* bestInterface = runCommand(@"route get 8.8.8.8 | grep interface | awk '{split($0,a,\": \"); printf \"%s\", a[2]}'");

	if ( bestInterface == nil || [bestInterface isEqual:[NSNull null]] || ([bestInterface respondsToSelector:@selector(length)] && [bestInterface length] == 0)) {
		return "Unknown";
	}
	
    CFArrayRef ref = SCNetworkInterfaceCopyAll();
    NSArray* networkInterfaces = (__bridge NSArray *)(ref);
    NSString* interfaceType;
    for(int i = 0; i < networkInterfaces.count; i++) {
        SCNetworkInterfaceRef interface = (__bridge SCNetworkInterfaceRef)(networkInterfaces[i]);
        NSString* bsdName = (NSString*) SCNetworkInterfaceGetBSDName(interface);

        if([bestInterface isEqualToString:bsdName]){
            interfaceType = ((NSString *)SCNetworkInterfaceGetInterfaceType(interface)) ;
            break;
        }
    }

    if ([interfaceType isEqualToString:@"IEEE80211"] || [interfaceType isEqualToString:@"Bluetooth"] || [interfaceType isEqualToString:@"IrDA"]) {
        return "Wireless";
    } else {
        return "Wired";
    }
}