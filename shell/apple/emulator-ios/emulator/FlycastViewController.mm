/*
	Copyright 2021 flyinghead
	Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.

	This file is part of Flycast.

	Flycast is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	Flycast is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#import "FlycastViewController.h"
#import <Network/Network.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#import <OpenGLES/EAGL.h>

#import "PadViewController.h"
#import "EmulatorView.h"

#include "types.h"
#include "wsi/context.h"
#include "rend/mainui.h"
#include "emulator.h"
#include "log/LogManager.h"
#include "stdclass.h"
#include "cfg/option.h"
#include "ios_gamepad.h"
#include "ios_keyboard.h"
#include "ios_mouse.h"

//@import AltKit;
#import "AltKit-Swift.h"

std::string iosJitStatus;
static bool iosJitAuthorized;
static __unsafe_unretained FlycastViewController *flycastViewController;

std::map<GCController *, std::shared_ptr<IOSGamepad>> IOSGamepad::controllers;
std::map<GCKeyboard *, std::shared_ptr<IOSKeyboard>> IOSKeyboard::keyboards;
std::map<GCMouse *, std::shared_ptr<IOSMouse>> IOSMouse::mice;

void common_linux_setup();

static bool lockedPointer;
static void updatePointerLock(Event event, void *)
{
    if (@available(iOS 14.0, *)) {
        bool hasChanged = NO;
        switch (event) {
            case Event::Resume:
                lockedPointer = YES;
                hasChanged = YES;
                break;
            case Event::Pause:
            case Event::Terminate:
                lockedPointer = NO;
                hasChanged = YES;
                break;
            default:
                break;
        }
        
        if (hasChanged) {
            [flycastViewController setNeedsUpdateOfPrefersPointerLocked];
        }
    }
}

@interface FlycastViewController () <UIDocumentPickerDelegate>

@property (strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) PadViewController *padController;

@property (nonatomic) iCadeReaderView* iCadeReader;
@property (nonatomic, strong) id gamePadConnectObserver;
@property (nonatomic, strong) id gamePadDisconnectObserver;
@property (nonatomic, strong) id keyboardConnectObserver;
@property (nonatomic, strong) id keyboardDisconnectObserver;
@property (nonatomic, strong) id mouseConnectObserver;
@property (nonatomic, strong) id mouseDisconnectObserver;

@property (nonatomic, strong) nw_path_monitor_t monitor;
@property (nonatomic, strong) dispatch_queue_t monitorQueue;

@end

extern int screen_dpi;

@implementation FlycastViewController

- (id)initWithCoder:(NSCoder *)coder
{
	self = [super initWithCoder:coder];
	if (self)
		flycastViewController = self;
	return self;

}

- (void)viewDidLoad
{
    [super viewDidLoad];

	LogManager::Init();

	std::string homedir = [[[[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] objectAtIndex:0] path] UTF8String];
	homedir += "/";
	set_user_config_dir(homedir);
	set_user_data_dir(homedir);

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

	common_linux_setup();

	flycast_init(0, nullptr);
	config::ContentPath.get().clear();
	config::ContentPath.get().push_back(homedir);

#if !TARGET_OS_TV
	self.padController = [[PadViewController alloc] initWithNibName:@"PadViewController" bundle:nil];
#endif
	
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
	if (!self.context) {
		self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
		if (!self.context) {
			NSLog(@"Failed to create ES context");
		}
	}
    
    EmulatorView *emuView = (EmulatorView *)self.view;
    emuView.context = self.context;

	// Set preferred refresh rate
	[self setPreferredFramesPerSecond:60];
	[EAGLContext setCurrentContext:self.context];

    if (@available(iOS 14.0, *)) {
        self.keyboardConnectObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:GCKeyboardDidConnectNotification object:nil queue:[NSOperationQueue mainQueue]
            usingBlock:^(NSNotification *note) {
            GCKeyboard *keyboard = note.object;
            IOSKeyboard::addKeyboard(keyboard);
        }];
        
        self.keyboardDisconnectObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:GCKeyboardDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue]
            usingBlock:^(NSNotification *note) {
            GCKeyboard *keyboard = note.object;
            IOSKeyboard::removeKeyboard(keyboard);
        }];
        
        self.mouseConnectObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:GCMouseDidConnectNotification object:nil queue:[NSOperationQueue mainQueue]
            usingBlock:^(NSNotification *note) {
            GCMouse *mouse = note.object;
            IOSMouse::addMouse(mouse);
        }];
        
        self.mouseDisconnectObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:GCMouseDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue]
            usingBlock:^(NSNotification *note) {
            GCMouse *mouse = note.object;
            IOSMouse::removeMouse(mouse);
        }];

        EventManager::listen(Event::Resume, updatePointerLock);
        EventManager::listen(Event::Pause, updatePointerLock);
        EventManager::listen(Event::Terminate, updatePointerLock);
    }

    self.gamePadConnectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
		GCController *controller = note.object;
		IOSGamepad::addController(controller);
#if !TARGET_OS_TV
		if (IOSGamepad::controllerConnected())
			[self.padController hideController];
#endif
    }];
    self.gamePadDisconnectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
		GCController *controller = note.object;
		IOSGamepad::removeController(controller);
#if !TARGET_OS_TV
		if (!IOSGamepad::controllerConnected())
			[self.padController showController:self.view];
#endif
    }];

	for (GCController *controller in [GCController controllers])
		IOSGamepad::addController(controller);

#if !TARGET_OS_TV
	[self addChildViewController:self.padController];
	self.padController.view.frame = self.view.bounds;
	self.padController.view.translatesAutoresizingMaskIntoConstraints = YES;
	self.padController.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
	if (IOSGamepad::controllerConnected())
		[self.padController hideController];
#endif

    self.iCadeReader = [[iCadeReaderView alloc] init];
    [self.view addSubview:self.iCadeReader];
    self.iCadeReader.delegate = self;
    self.iCadeReader.active = YES;
	// TODO iCade handlers
	
	settings.display.width = roundf([[UIScreen mainScreen] nativeBounds].size.width);
    settings.display.height = roundf([[UIScreen mainScreen] nativeBounds].size.height);
	if (settings.display.width < settings.display.height)
		std::swap(settings.display.width, settings.display.height);
	float scale = 1;
	if ([[UIScreen mainScreen] respondsToSelector:@selector(scale)]) {
	  scale = [[UIScreen mainScreen] scale];
	}
	screen_dpi = roundf(160 * scale);
	initRenderApi();
	mainui_init();

	[self altKitStart];
}

- (BOOL)prefersStatusBarHidden
{
	return YES;
}

- (BOOL)prefersPointerLocked
{
    return lockedPointer;
}

- (UIStatusBarStyle)preferredStatusBarStyle
{
	return UIStatusBarStyleLightContent;
}

-(UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
	return UIRectEdgeAll;
}

- (void)dealloc
{
    if ([EAGLContext currentContext] == self.context) {
        [EAGLContext setCurrentContext:nil];
    }
}

- (void)altKitStart
{
	NSLog(@"Starting AltKit discovery");

	[[ALTServerManager sharedManager] autoconnectWithCompletionHandler:^(ALTServerConnection *connection, NSError *error) {
		if (error)
		{
			dispatch_async(dispatch_get_main_queue(), ^(void) {
				iosJitStatus = "Failed: " + std::string([error.description UTF8String]);
			});
			return NSLog(@"Could not auto-connect to server. %@", error);
		}
		
		[connection enableUnsignedCodeExecutionWithCompletionHandler:^(BOOL success, NSError *error) {
			iosJitAuthorized = success;
			if (success)
			{
				NSLog(@"Successfully enabled JIT compilation!");
				dispatch_async(dispatch_get_main_queue(), ^(void) {
					iosJitStatus = "OK";
					nw_path_monitor_cancel(self.monitor);
				});
				[[ALTServerManager sharedManager] stopDiscovering];
			}
			else
			{
				dispatch_async(dispatch_get_main_queue(), ^(void) {
					iosJitStatus = "Failed: " + std::string([error.description UTF8String]);
				});
				NSLog(@"Could not enable JIT compilation. %@", error);
			}
			
			[connection disconnect];
		}];
	}];

	dispatch_queue_attr_t attrs = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, DISPATCH_QUEUE_PRIORITY_DEFAULT);
	self.monitorQueue = dispatch_queue_create("com.flycast.network-monitor", attrs);
	self.monitor = nw_path_monitor_create();
	nw_path_monitor_set_queue(self.monitor, self.monitorQueue);
	nw_path_monitor_set_update_handler(self.monitor, ^(nw_path_t _Nonnull path) {
		nw_path_status_t status = nw_path_get_status(path);
		if (!iosJitAuthorized && (status == nw_path_status_satisfied || status == nw_path_status_satisfiable)) {
			dispatch_async(dispatch_get_main_queue(), ^(void) {
				[[ALTServerManager sharedManager] stopDiscovering];
				iosJitStatus = "Connecting...";
				[[ALTServerManager sharedManager] startDiscovering];
			});
		}
	});
	nw_path_monitor_start(self.monitor);
}

- (void)viewSafeAreaInsetsDidChange
{
	[super viewSafeAreaInsetsDidChange];
	float scale = self.view.contentScaleFactor;
	gui_set_insets(self.view.safeAreaInsets.left * scale, self.view.safeAreaInsets.right * scale,
				   self.view.safeAreaInsets.top * scale, self.view.safeAreaInsets.bottom * scale);
}

#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{

}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
#if !TARGET_OS_TV
	if (emu.running() != [self.padController isControllerVisible] && !IOSGamepad::controllerConnected())
	{
		if (emu.running())
			[self.padController showController:self.view];
		else
			[self.padController hideController];
	}
#endif
	mainui_rend_frame();
}
/*
- (void)pickIosFile
{
	UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:@[@"com.flyinghead.flycast.disk-image", @"com.pkware.zip-archive"] inMode:UIDocumentPickerModeOpen];
	picker.delegate = self;
	picker.allowsMultipleSelection = true;
	
	[self presentViewController:picker animated:YES completion:nil];
}

- (void)pickIosFolder
{
	if (@available(iOS 14.0, *)) {
		UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeFolder]];
		picker.delegate = self;
		
		[self presentViewController:picker animated:YES completion:nil];
	}
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
	for (NSURL *url in urls) {
		if (url.fileURL)
		{
			[url startAccessingSecurityScopedResource];
			gui_add_content_path(url.fileSystemRepresentation);
		}
	}
}
*/

@end

void pickIosFolder()
{
//	[flycastViewController pickIosFolder];
}

void pickIosFile()
{
//	[flycastViewController pickIosFile];
}
