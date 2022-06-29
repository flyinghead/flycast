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
#import <AVFoundation/AVFoundation.h>

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

static bool recordingAVSession;
static void updateAudioSession(Event event, void *)
{
	if (event == Event::Resume)
	{
		AVAudioSession *session = [AVAudioSession sharedInstance];
		NSError *error = nil;
		bool hasMicrophone = false;
		for (int bus = 0; bus < 4 && !hasMicrophone; bus++)
		{
			switch (config::MapleMainDevices[bus])
			{
				case MDT_SegaController:
					for (int port = 0; port < 2; port++)
						if (config::MapleExpansionDevices[bus][port] == MDT_Microphone)
							hasMicrophone = true;
					break;
				case MDT_LightGun:
				case MDT_AsciiStick:
				case MDT_TwinStick:
					if (config::MapleExpansionDevices[bus][0] == MDT_Microphone)
						hasMicrophone = true;
					break;
				default:
					break;
			}
		}
		bool configChanged = false;
		if (recordingAVSession && !hasMicrophone)
		{
			recordingAVSession = false;
			configChanged = true;
			// Allow playback only
			[session setCategory:AVAudioSessionCategoryAmbient
					 withOptions:AVAudioSessionCategoryOptionMixWithOthers | AVAudioSessionCategoryOptionDefaultToSpeaker
				| AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionAllowBluetoothA2DP
				| AVAudioSessionCategoryOptionAllowAirPlay
						   error:&error];
			NSLog(@"AVAudioSession set to Playback only");
		}
		else if (!recordingAVSession && hasMicrophone)
		{
			// TODO delay until actual recording starts?
			recordingAVSession = true;
			configChanged = true;
			// Allow audio playing AND recording
			[session setCategory:AVAudioSessionCategoryPlayAndRecord
					 withOptions:AVAudioSessionCategoryOptionMixWithOthers | AVAudioSessionCategoryOptionDefaultToSpeaker
				| AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionAllowBluetoothA2DP
				| AVAudioSessionCategoryOptionAllowAirPlay
						   error:&error];
			NSLog(@"AVAudioSession set to Play and Record");
		}
		if (configChanged)
		{
			if (error != nil)
				NSLog(@"AVAudioSession.setCategory:  %@", error);
			[session setActive:YES error:&error];
			if (error != nil)
				NSLog(@"AVAudioSession.setActive:  %@", error);
		}
	}
}

@interface FlycastViewController () <UIDocumentPickerDelegate, UITextFieldDelegate>

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

@property (nonatomic, assign, getter=isKeyboardVisible) BOOL keyboardVisible;
@property (nonatomic, assign) CGRect textInputRect;
@property (nonatomic, assign) int keyboardHeight;

@end

@implementation FlycastViewController {
	UITextField *textField;
	BOOL showingKeyboard;
	NSString *changeText;
	NSString *obligateForBackspace;
}

- (id)initWithCoder:(NSCoder *)coder
{
	self = [super initWithCoder:coder];
	if (self) {
		flycastViewController = self;
		[self initKeyboard];
		showingKeyboard = NO;
	}
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
	EventManager::listen(Event::Resume, updateAudioSession);

	settings.display.width = roundf([[UIScreen mainScreen] nativeBounds].size.width);
    settings.display.height = roundf([[UIScreen mainScreen] nativeBounds].size.height);
	if (settings.display.width < settings.display.height)
		std::swap(settings.display.width, settings.display.height);
	float scale = 1;
	if ([[UIScreen mainScreen] respondsToSelector:@selector(scale)]) {
	  scale = [[UIScreen mainScreen] scale];
	}
	settings.display.dpi = 160.f * scale;
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
	[self deinitKeyboard];
	EventManager::unlisten(Event::Resume, updatePointerLock);
	EventManager::unlisten(Event::Pause, updatePointerLock);
	EventManager::unlisten(Event::Terminate, updatePointerLock);
	EventManager::unlisten(Event::Resume, updateAudioSession);
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

#pragma mark On-screen keyboard
// borrowed from SDL uikit view controller code

@synthesize textInputRect;
@synthesize keyboardHeight;
@synthesize keyboardVisible;

// Set ourselves up as a UITextFieldDelegate
- (void)initKeyboard
{
	changeText = nil;
	obligateForBackspace = @"                                                                "; // 64 spaces
	textField = [[UITextField alloc] initWithFrame:CGRectZero];
	textField.delegate = self;
	// placeholder so there is something to delete!
	textField.text = obligateForBackspace;

	// set UITextInputTrait properties, mostly to defaults
	textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
	textField.autocorrectionType = UITextAutocorrectionTypeNo;
	textField.enablesReturnKeyAutomatically = NO;
	textField.keyboardAppearance = UIKeyboardAppearanceDefault;
	textField.keyboardType = UIKeyboardTypeDefault;
	textField.returnKeyType = UIReturnKeyDefault;
	textField.secureTextEntry = NO;

	textField.hidden = YES;
	keyboardVisible = NO;

	NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#if !TARGET_OS_TV
	[center addObserver:self selector:@selector(keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
	[center addObserver:self selector:@selector(keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];
#endif
	[center addObserver:self selector:@selector(textFieldTextDidChange:) name:UITextFieldTextDidChangeNotification object:nil];
	gui_setOnScreenKeyboardCallback([](bool show) {
		if (show != flycastViewController.keyboardVisible)
		{
			if (show)
				[flycastViewController showKeyboard];
			else
				[flycastViewController hideKeyboard];
		}
	});
}

- (void)deinitKeyboard
{
	gui_setOnScreenKeyboardCallback(nullptr);
	NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#if !TARGET_OS_TV
	[center removeObserver:self name:UIKeyboardWillShowNotification object:nil];
	[center removeObserver:self name:UIKeyboardWillHideNotification object:nil];
#endif
	[center removeObserver:self name:UITextFieldTextDidChangeNotification object:nil];
}

- (void)setView:(UIView *)view
{
	[super setView:view];

	[view addSubview:textField];

	if (keyboardVisible) {
		[self showKeyboard];
	}
}

// reveal onscreen virtual keyboard
- (void)showKeyboard
{
	keyboardVisible = YES;
	if (textField.window) {
		showingKeyboard = YES;
		[textField becomeFirstResponder];
		showingKeyboard = NO;
	}
}

// hide onscreen virtual keyboard
- (void)hideKeyboard
{
	keyboardVisible = NO;
	[textField resignFirstResponder];
}

- (void)keyboardWillShow:(NSNotification *)notification
{
#if !TARGET_OS_TV
	CGRect kbrect = [[notification userInfo][UIKeyboardFrameEndUserInfoKey] CGRectValue];

	// The keyboard rect is in the coordinate space of the screen/window, but we
	// want its height in the coordinate space of the view.
	kbrect = [self.view convertRect:kbrect fromView:nil];

	[self setKeyboardHeight:(int)kbrect.size.height];
#endif
}

- (void)keyboardWillHide:(NSNotification *)notification
{
	[self setKeyboardHeight:0];
}

- (void)textFieldTextDidChange:(NSNotification *)notification
{
	if (changeText!=nil && textField.markedTextRange == nil)
	{
		NSUInteger len = changeText.length;
		if (len > 0)
			gui_keyboard_inputUTF8([changeText UTF8String]);
		changeText = nil;
	}
}

- (void)updateKeyboard
{
	CGAffineTransform t = self.view.transform;
	CGPoint offset = CGPointMake(0.0, 0.0);
	CGRect frame = self.view.window.screen.bounds; // FIXME UIKit_ComputeViewFrame(window, self.view.window.screen);

	if (self.keyboardHeight) {
		int rectbottom = self.textInputRect.origin.y + self.textInputRect.size.height;
		int keybottom = self.view.bounds.size.height - self.keyboardHeight;
		if (keybottom < rectbottom) {
			offset.y = keybottom - rectbottom;
		}
	}

	// Apply this view's transform (except any translation) to the offset, in
	// order to orient it correctly relative to the frame's coordinate space.
	t.tx = 0.0;
	t.ty = 0.0;
	offset = CGPointApplyAffineTransform(offset, t);

	// Apply the updated offset to the view's frame.
	frame.origin.x += offset.x;
	frame.origin.y += offset.y;

	self.view.frame = frame;
}

- (void)setKeyboardHeight:(int)height
{
	keyboardVisible = height > 0;
	keyboardHeight = height;
	[self updateKeyboard];
}

// UITextFieldDelegate method.  Invoked when user types something.
- (BOOL)textField:(UITextField *)_textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
	NSUInteger len = string.length;
	if (len == 0) {
		changeText = nil;
		if (textField.markedTextRange == nil) {
			// it wants to replace text with nothing, ie a delete
			gui_keyboard_key(0x2A, true, 0); // backspace
			gui_keyboard_key(0x2A, false, 0);
		}
		if (textField.text.length < 16) {
			textField.text = obligateForBackspace;
		}
	} else {
		changeText = string;
	}
	return YES;
}

// Terminates the editing session
- (BOOL)textFieldShouldReturn:(UITextField*)_textField
{
	gui_keyboard_key(0x28, true, 0); // Return
	gui_keyboard_key(0x28, false, 0);
	return YES;
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
