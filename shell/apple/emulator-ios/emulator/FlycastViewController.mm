//
//  Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.
//
#import "FlycastViewController.h"
#import <GameController/GameController.h>
#import <Network/Network.h>
//#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#import <OpenGLES/EAGL.h>

#import "PadViewController.h"
#import "EmulatorView.h"

#include "types.h"
#include "input/gamepad_device.h"
#include "wsi/context.h"
#include "rend/mainui.h"
#include "emulator.h"
#include "log/LogManager.h"
#include "stdclass.h"
#include "cfg/option.h"
#include "ios_mouse.h"
#include "rend/gui.h"

//@import AltKit;
#import "AltKit/AltKit-Swift.h"

std::string iosJitStatus;
static bool iosJitAuthorized;
static std::shared_ptr<IOSMouse> mouse;
static __unsafe_unretained FlycastViewController *flycastViewController;

void common_linux_setup();

@interface FlycastViewController () <UIDocumentPickerDelegate>

@property (strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) PadViewController *padController;

@property (nonatomic) iCadeReaderView* iCadeReader;
@property (nonatomic) GCController *gController __attribute__((weak_import));
@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

@property (nonatomic, strong) nw_path_monitor_t monitor;
@property (nonatomic, strong) dispatch_queue_t monitorQueue;

@end

extern int screen_width,screen_height;
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

    self.connectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
		// GCController *controller = (GCController *)note.object;
		// IOSGame::addController(controller);
        if (GCController.controllers.count > 0) {
			[self toggleHardwareController:YES];
        }
    }];
    self.disconnectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
		// GCController *controller = (GCController *)note.object;
		// IOSGame::removeController(controller);
        if (GCController.controllers.count == 0) {
            [self toggleHardwareController:NO];
        }
    }];
    
    if ([[GCController controllers] count]) {
        [self toggleHardwareController:YES];
	}

#if !TARGET_OS_TV
	[self addChildViewController:self.padController];
	self.padController.view.frame = self.view.bounds;
	self.padController.view.translatesAutoresizingMaskIntoConstraints = YES;
	self.padController.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleWidth;
	self.padController.handler = self;
	[self.padController hideController];
#endif

    self.iCadeReader = [[iCadeReaderView alloc] init];
    [self.view addSubview:self.iCadeReader];
    self.iCadeReader.delegate = self;
    self.iCadeReader.active = YES;
	// TODO iCade handlers
	
	screen_width = roundf([[UIScreen mainScreen] nativeBounds].size.width);
    screen_height = roundf([[UIScreen mainScreen] nativeBounds].size.height);
	if (screen_width < screen_height)
		std::swap(screen_width, screen_height);
	float scale = 1;
	if ([[UIScreen mainScreen] respondsToSelector:@selector(scale)]) {
	  scale = [[UIScreen mainScreen] scale];
	}
	screen_dpi = roundf(160 * scale);
	InitRenderApi();
	mainui_init();
	mainui_enabled = true;
	
	emuView.mouse = ::mouse.get();
	[self altKitStart];
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

#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{

}

- (void)toggleHardwareController:(BOOL)useHardware
{
    if (useHardware)
	{
		[self.padController hideController];

#if TARGET_OS_TV
		for (GCController*c in GCController.controllers) {
			if ((c.gamepad != nil || c.extendedGamepad != nil) && c != _gController) {
				self.gController = c;
				break;
			}
		}
#else
		self.gController = [GCController controllers].firstObject;
#endif
		// TODO: Add multi player  using gController.playerIndex and iterate all controllers

		if (self.gController.extendedGamepad)
		{
			[self.gController.extendedGamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed)
					[self handleKeyDown:IOS_BTN_A];
				else
					[self handleKeyUp:IOS_BTN_A];
			}];
			[self.gController.extendedGamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed)
					[self handleKeyDown:IOS_BTN_B];
				else
					[self handleKeyUp:IOS_BTN_B];
			}];
			[self.gController.extendedGamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed)
					[self handleKeyDown:IOS_BTN_X];
				else
					[self handleKeyUp:IOS_BTN_X];
			}];
			[self.gController.extendedGamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed)
					[self handleKeyDown:IOS_BTN_Y];
				else
					[self handleKeyUp:IOS_BTN_Y];
			}];
			[self.gController.extendedGamepad.rightTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				rt[0] = (int)std::roundf(255.f * value);
			}];
			[self.gController.extendedGamepad.leftTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				lt[0] = (int)std::roundf(255.f * value);
			}];
			
			if (@available(iOS 13.0, *)) {
				[self.gController.extendedGamepad.buttonOptions setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					if (pressed)
						[self handleKeyDown:IOS_BTN_OPTIONS];
					else
						[self handleKeyUp:IOS_BTN_OPTIONS];
				}];
				[self.gController.extendedGamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					if (pressed)
						[self handleKeyDown:IOS_BTN_L1];
					else
						[self handleKeyUp:IOS_BTN_L1];
				}];
			} else {
				// Left shoulder for options/menu
				[self.gController.extendedGamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					if (pressed)
						[self handleKeyDown:IOS_BTN_OPTIONS];
					else
						[self handleKeyUp:IOS_BTN_OPTIONS];
				}];
			}
			if (@available(iOS 13.0, *)) {
				[self.gController.extendedGamepad.buttonMenu setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					if (pressed)
						[self handleKeyDown:IOS_BTN_MENU];
					else
						[self handleKeyUp:IOS_BTN_MENU];
				}];
				[self.gController.extendedGamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					if (pressed)
						[self handleKeyDown:IOS_BTN_R1];
					else
						[self handleKeyUp:IOS_BTN_R1];
				}];
			} else {
				// Right shoulder for menu/start
				[self.gController.extendedGamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					if (pressed)
						[self handleKeyDown:IOS_BTN_MENU];
					else
						[self handleKeyUp:IOS_BTN_MENU];
				}];
			}

			[self.gController.extendedGamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
				if (dpad.right.isPressed)
					[self handleKeyDown:IOS_BTN_RIGHT];
				else
					[self handleKeyUp:IOS_BTN_RIGHT];
				if (dpad.left.isPressed)
					[self handleKeyDown:IOS_BTN_LEFT];
				else
					[self handleKeyUp:IOS_BTN_LEFT];
				if (dpad.up.isPressed)
					[self handleKeyDown:IOS_BTN_UP];
				else
					[self handleKeyUp:IOS_BTN_UP];
				if (dpad.down.isPressed)
					[self handleKeyDown:IOS_BTN_DOWN];
				else
					[self handleKeyUp:IOS_BTN_DOWN];
			}];
			[self.gController.extendedGamepad.leftThumbstick.xAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value) {
				s8 v = (s8)(value * 127); //-127 ... + 127 range

				joyx[0] = v;
			}];
			[self.gController.extendedGamepad.leftThumbstick.yAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value) {
				s8 v = (s8)(value * 127 * -1); //-127 ... + 127 range

				joyy[0] = v;
			}];
			[self.gController.extendedGamepad.rightThumbstick.xAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value) {
				s8 v = (s8)(value * 127); //-127 ... + 127 range

				joyrx[0] = v;
			}];
			[self.gController.extendedGamepad.rightThumbstick.yAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value) {
				s8 v = (s8)(value * 127 * -1); //-127 ... + 127 range

				joyry[0] = v;
			}];
		}
        else if (self.gController.gamepad) {
            [self.gController.gamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed)
					[self handleKeyDown:IOS_BTN_A];
				else
					[self handleKeyUp:IOS_BTN_A];
            }];
            [self.gController.gamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed)
					[self handleKeyDown:IOS_BTN_B];
				else
					[self handleKeyUp:IOS_BTN_B];
            }];
            [self.gController.gamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed)
					[self handleKeyDown:IOS_BTN_X];
				else
					[self handleKeyUp:IOS_BTN_X];
            }];
            [self.gController.gamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed)
					[self handleKeyDown:IOS_BTN_Y];
				else
					[self handleKeyUp:IOS_BTN_Y];
            }];
            [self.gController.gamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue){
				if (dpad.right.isPressed)
					[self handleKeyDown:IOS_BTN_RIGHT];
				else
					[self handleKeyUp:IOS_BTN_RIGHT];
				if (dpad.left.isPressed)
					[self handleKeyDown:IOS_BTN_LEFT];
				else
					[self handleKeyUp:IOS_BTN_LEFT];
				if (dpad.up.isPressed)
					[self handleKeyDown:IOS_BTN_UP];
				else
					[self handleKeyUp:IOS_BTN_UP];
				if (dpad.down.isPressed)
					[self handleKeyDown:IOS_BTN_DOWN];
				else
					[self handleKeyUp:IOS_BTN_DOWN];
            }];

			[self.gController.gamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed) {
					if (self.gController.gamepad.leftShoulder.pressed)
						[self handleKeyDown:IOS_BTN_MENU];
					else
						[self handleKeyDown:IOS_BTN_R2];
				}
				else {
					[self handleKeyUp:IOS_BTN_R2];
					[self handleKeyUp:IOS_BTN_MENU];
				}
			}];
			[self.gController.gamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed) {
					if (self.gController.gamepad.rightShoulder.pressed)
						[self handleKeyDown:IOS_BTN_MENU];
					else
						[self handleKeyDown:IOS_BTN_L2];
				}
				else {
					[self handleKeyUp:IOS_BTN_L2];
					[self handleKeyUp:IOS_BTN_MENU];
				}
			}];

            //Add controller pause handler here
        }
    } else {
        self.gController = nil;
		[self.padController showController:self.view];
    }
}


- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
	if (dc_is_running() != [self.padController isControllerVisible] && self.gController == nil)
	{
		if (dc_is_running())
			[self.padController showController:self.view];
		else
			[self.padController hideController];
	}
	mainui_rend_frame();
}

static DreamcastKey IosToDCKey[IOS_BTN_MAX] {
	EMU_BTN_NONE,	// none
	DC_BTN_A,
	DC_BTN_B,
	DC_BTN_X,
	DC_BTN_Y,
	DC_DPAD_UP,
	DC_DPAD_DOWN,
	DC_DPAD_LEFT,
	DC_DPAD_RIGHT,
	DC_BTN_START,	// menu
	EMU_BTN_MENU,	// options
	EMU_BTN_NONE,	// home
	EMU_BTN_NONE,	// L1
	EMU_BTN_NONE,	// R1
	EMU_BTN_NONE,	// L3
	EMU_BTN_NONE,	// R3
	EMU_BTN_TRIGGER_LEFT,	// L2
	EMU_BTN_TRIGGER_RIGHT,	// R2
};

- (void)handleKeyDown:(enum IOSButton)button;
{
	DreamcastKey dcKey = IosToDCKey[button];
	switch (dcKey) {
		case EMU_BTN_NONE:
			break;
		case EMU_BTN_MENU:
			gui_open_settings();
			break;
		case EMU_BTN_TRIGGER_LEFT:
			lt[0] = 0xff;
			break;
		case EMU_BTN_TRIGGER_RIGHT:
			rt[0] = 0xff;
			break;
		default:
			if (dcKey < EMU_BTN_TRIGGER_LEFT)
				kcode[0] &= ~dcKey;
			break;
	}
	// Open menu with UP + DOWN or LEFT + RIGHT
	if ((kcode[0] & (DC_DPAD_UP | DC_DPAD_DOWN)) == 0
		|| (kcode[0] & (DC_DPAD_LEFT | DC_DPAD_RIGHT)) == 0) {
		kcode[0] = ~0;
		gui_open_settings();
	}
	// Arcade shortcuts
	if (rt[0] > 0)
	{
		if ((kcode[0] & DC_BTN_A) == 0)
			// RT + A -> D (coin)
			kcode[0] &= ~DC_BTN_D;
		if ((kcode[0] & DC_BTN_B) == 0)
			// RT + B -> C (service)
			kcode[0] &= ~DC_BTN_C;
		if ((kcode[0] & DC_BTN_X) == 0)
			// RT + X -> Z (test)
			kcode[0] &= ~DC_BTN_Z;
	}
}

- (void)handleKeyUp:(enum IOSButton)button;
{
	DreamcastKey dcKey = IosToDCKey[button];
	switch (dcKey) {
		case EMU_BTN_NONE:
			break;
		case EMU_BTN_TRIGGER_LEFT:
			lt[0] = 0;
			break;
		case EMU_BTN_TRIGGER_RIGHT:
			rt[0] = 0;
			break;
		default:
			if (dcKey < EMU_BTN_TRIGGER_LEFT)
				kcode[0] |= dcKey;
			break;
	}
	if (rt[0] == 0)
		kcode[0] |= DC_BTN_D | DC_BTN_C | DC_BTN_Z;
	else
	{
		if ((kcode[0] & DC_BTN_A) != 0)
			kcode[0] |= DC_BTN_D;
		if ((kcode[0] & DC_BTN_B) != 0)
			kcode[0] |= DC_BTN_C;
		if ((kcode[0] & DC_BTN_X) != 0)
			kcode[0] |= DC_BTN_Z;
	}
}
/*
- (void)pickIosFolder
{
	if (@available(iOS 14.0, *)) {
		UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeFolder]];
		picker.delegate = self;
		
		[self presentViewController:picker animated:YES completion:nil];
	} else {
		// Fallback on earlier versions
		NSLog(@"UIDocumentPickerViewController no iOS 14 :(");
	}
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
	for (NSURL *url in urls) {
		std::string path { url.absoluteString.UTF8String };
		if (path.substr(0, 8) == "file:///")
			config::ContentPath.get().push_back(path.substr(7));
	}
}
*/

@end

void os_SetupInput()
{
	mouse = std::make_shared<IOSMouse>();
	GamepadDevice::Register(mouse);
}

void pickIosFolder()
{
//	[flycastViewController pickIosFolder];
}
