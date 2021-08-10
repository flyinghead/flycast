//
//  Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.
//
#import "FlycastViewController.h"

#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#import <OpenGLES/EAGL.h>

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
static std::shared_ptr<IOSMouse> mouse;

void common_linux_setup();

@interface FlycastViewController () {
}

@property (strong, nonatomic) EAGLContext *context;

@end

extern int screen_width,screen_height;
extern int screen_dpi;

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>

static void move_pthread_to_realtime_scheduling_class(pthread_t pthread)
{
	mach_timebase_info_data_t timebase_info;
	mach_timebase_info(&timebase_info);

	const uint64_t NANOS_PER_MSEC = 1000000ULL;
	double clock2abs = ((double)timebase_info.denom / (double)timebase_info.numer) * NANOS_PER_MSEC;

	thread_time_constraint_policy_data_t policy;
	policy.period      = 0;
	policy.computation = (uint32_t)(5 * clock2abs); // 5 ms of work
	policy.constraint  = (uint32_t)(10 * clock2abs);
	policy.preemptible = FALSE;

	int kr = thread_policy_set(pthread_mach_thread_np(pthread_self()),
							   THREAD_TIME_CONSTRAINT_POLICY,
							   (thread_policy_t)&policy,
							   THREAD_TIME_CONSTRAINT_POLICY_COUNT);
	if (kr != KERN_SUCCESS) {
		mach_error("thread_policy_set:", kr);
		exit(1);
	}
}

// TODO use this for emu thread?
static void MakeCurrentThreadRealTime()
{
	move_pthread_to_realtime_scheduling_class(pthread_self());
}

@implementation FlycastViewController

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

	[self.padController setControlOutput:emuView];
    
    self.connectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        if ( GCController.controllers.count ){
            [self toggleHardwareController:YES];
        }
    }];
    self.disconnectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
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
	[self.view addSubview:self.padController.view];
	[self.padController didMoveToParentViewController:self];
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
	
	// Swipe right to open the menu in-game
	UISwipeGestureRecognizer *swipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(handleSwipe:)];
	[swipe setDirection:UISwipeGestureRecognizerDirectionRight];
	[self.view addGestureRecognizer:swipe];
	
	[self altKitStart];
}

- (void)handleSwipe:(UISwipeGestureRecognizer *)swipe {
	gui_open_settings();
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
	iosJitStatus = "Connecting...";
	[[ALTServerManager sharedManager] startDiscovering];

	[[ALTServerManager sharedManager] autoconnectWithCompletionHandler:^(ALTServerConnection *connection, NSError *error) {
		if (error)
		{
			iosJitStatus = "Failed: " + std::string([error.description UTF8String]);
			return NSLog(@"Could not auto-connect to server. %@", error);
		}
		
		[connection enableUnsignedCodeExecutionWithCompletionHandler:^(BOOL success, NSError *error) {
			if (success)
			{
				NSLog(@"Successfully enabled JIT compilation!");
				iosJitStatus = "OK";
				[[ALTServerManager sharedManager] stopDiscovering];
			}
			else
			{
				iosJitStatus = "Failed: " + std::string([error.description UTF8String]);
				NSLog(@"Could not enable JIT compilation. %@", error);
			}
			
			[connection disconnect];
		}];
	}];
}

#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{

}

- (void)toggleHardwareController:(BOOL)useHardware {
    if (useHardware) {

		[self.padController hideController];

#if TARGET_OS_TV
		for(GCController*c in GCController.controllers) {
			if ((c.gamepad != nil || c.extendedGamepad != nil) && (c != _gController)) {

				self.gController = c;
				break;
			}
		}
#else
		self.gController = [GCController controllers].firstObject;
#endif
		// TODO: Add multi player  using gController.playerIndex and iterate all controllers

		if (self.gController.extendedGamepad) {
			[self.gController.extendedGamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~DC_BTN_A;
				} else {
					kcode[0] |= DC_BTN_A;
				}
			}];
			[self.gController.extendedGamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~DC_BTN_B;
				} else {
					kcode[0] |= DC_BTN_B;
				}
			}];
			[self.gController.extendedGamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~DC_BTN_X;
				} else {
					kcode[0] |= DC_BTN_X;
				}
			}];
			[self.gController.extendedGamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~DC_BTN_Y;
				} else {
					kcode[0] |= DC_BTN_Y;
				}
			}];
			[self.gController.extendedGamepad.rightTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                                 if (pressed) {
					 rt[0] = 255;
				 } else {
					 rt[0] = 0;
				 }
                        }];
			[self.gController.extendedGamepad.leftTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                                 if (pressed) {
					 lt[0] = 255;
				 } else {
					 lt[0] = 0;
				 }
                        }];
			
			// Either trigger for start
			[self.gController.extendedGamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~DC_BTN_START;
				} else {
					kcode[0] |= DC_BTN_START;
				}
			}];
			[self.gController.extendedGamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~DC_BTN_START;
				} else {
					kcode[0] |= DC_BTN_START;
				}
			}];
			[self.gController.extendedGamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue){
				if (dpad.right.isPressed) {
					kcode[0] &= ~DC_DPAD_RIGHT;
				} else {
					kcode[0] |= DC_DPAD_RIGHT;
				}
				if (dpad.left.isPressed) {
					kcode[0] &= ~DC_DPAD_LEFT;
				} else {
					kcode[0] |= DC_DPAD_LEFT;
				}
				if (dpad.up.isPressed) {
					kcode[0] &= ~DC_DPAD_UP;
				} else {
					kcode[0] |= DC_DPAD_UP;
				}
				if (dpad.down.isPressed) {
					kcode[0] &= ~DC_DPAD_DOWN;
				} else {
					kcode[0] |= DC_DPAD_DOWN;
				}
			}];
			[self.gController.extendedGamepad.leftThumbstick.xAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value){
				s8 v=(s8)(value*127); //-127 ... + 127 range

				//NSLog(@"Joy X: %i", v);
				joyx[0] = v;
			}];
			[self.gController.extendedGamepad.leftThumbstick.yAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value){
				s8 v=(s8)(value*127 * - 1); //-127 ... + 127 range

				//NSLog(@"Joy Y: %i", v);
				joyy[0] = v;
			}];
			[self.gController.extendedGamepad.rightThumbstick.xAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value){
				s8 v=(s8)(value*127); //-127 ... + 127 range

				//NSLog(@"RJoy X: %i", v);
				joyrx[0] = v;
			}];
			[self.gController.extendedGamepad.rightThumbstick.yAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value){
				s8 v=(s8)(value*127 * - 1); //-127 ... + 127 range

				//NSLog(@"RJoy Y: %i", v);
				joyry[0] = v;
			}];
		}
        else if (self.gController.gamepad) {
			EmulatorView *emuView = (EmulatorView *)self.view;
            [self.gController.gamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					[emuView handleKeyDown:self.padController.img_abxy_a];
				} else {
					[emuView handleKeyUp:self.padController.img_abxy_a];
				}
            }];
            [self.gController.gamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					[emuView handleKeyDown:self.padController.img_abxy_b];
				} else {
					[emuView handleKeyUp:self.padController.img_abxy_b];
				}
            }];
            [self.gController.gamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					[emuView handleKeyDown:self.padController.img_abxy_x];
				} else {
					[emuView handleKeyUp:self.padController.img_abxy_x];
				}
            }];
            [self.gController.gamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					[emuView handleKeyDown:self.padController.img_abxy_y];
				} else {
					[emuView handleKeyUp:self.padController.img_abxy_y];
				}
            }];
            [self.gController.gamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue){
				if (dpad.right.isPressed) {
					[emuView handleKeyDown:self.padController.img_dpad_r];
				} else {
					[emuView handleKeyUp:self.padController.img_dpad_r];
				}
				if (dpad.left.isPressed) {
					[emuView handleKeyDown:self.padController.img_dpad_l];
				} else {
					[emuView handleKeyUp:self.padController.img_dpad_l];
				}
				if (dpad.up.isPressed) {
					[emuView handleKeyDown:self.padController.img_dpad_u];
				} else {
					[emuView handleKeyUp:self.padController.img_dpad_u];
				}
				if (dpad.down.isPressed) {
					[emuView handleKeyDown:self.padController.img_dpad_d];
				} else {
					[emuView handleKeyUp:self.padController.img_dpad_d];
				}
            }];

			[self.gController.gamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed) {
					[emuView handleKeyDown:self.padController.img_rt];
				} else {
					[emuView handleKeyUp:self.padController.img_rt];
				}
			}];

			[self.gController.gamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed) {
					[emuView handleKeyDown:self.padController.img_lt];
				} else {
					[emuView handleKeyUp:self.padController.img_lt];
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

@end

void os_SetupInput()
{
	mouse = std::make_shared<IOSMouse>();
	GamepadDevice::Register(mouse);
}
