//
//  EmulatorViewController.m
//  emulator
//
//  Created by Karen Tsai (angelXwind) on 2014/3/5.
//  Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.
//

#import "EmulatorViewController.h"
#import <OpenGLES/ES2/glext.h>

#include "types.h"
#include "profiler/profiler.h"
#include "cfg/cfg.h"
#include "rend/TexCache.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"

extern u16 kcode[4];
extern u32 vks[4];
extern s8 joyx[4],joyy[4];
extern u8 rt[4],lt[4];

#define DC_BTN_C		(1)
#define DC_BTN_B		(1<<1)
#define DC_BTN_A		(1<<2)
#define DC_BTN_START	(1<<3)
#define DC_DPAD_UP		(1<<4)
#define DC_DPAD_DOWN	(1<<5)
#define DC_DPAD_LEFT	(1<<6)
#define DC_DPAD_RIGHT	(1<<7)
#define DC_BTN_Z		(1<<8)
#define DC_BTN_Y		(1<<9)
#define DC_BTN_X		(1<<10)
#define DC_BTN_D		(1<<11)
#define DC_DPAD2_UP		(1<<12)
#define DC_DPAD2_DOWN	(1<<13)
#define DC_DPAD2_LEFT	(1<<14)
#define DC_DPAD2_RIGHT	(1<<15)

#define DC_AXIS_LT		(0X10000)
#define DC_AXIS_RT		(0X10001)
#define DC_AXIS_X		(0X20000)
#define DC_AXIS_Y		(0X20001)

@interface ViewController () {
}

@property (strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) GLKBaseEffect *effect;

- (void)setupGL;
- (void)tearDownGL;
- (void)emuThread;

@end

//who has time for headers
extern int screen_width,screen_height;
bool rend_single_frame();
bool gles_init();
extern "C" int reicast_main(int argc, char* argv[]);


@implementation ViewController

-(void)emuThread
{
    install_prof_handler(1);

	char *Args[3];
	const char *P;

	P = (const char *)[self.diskImage UTF8String];
	Args[0] = "dc";
	Args[1] = "-config";
	Args[2] = P&&P[0]? (char *)malloc(strlen(P)+32):0;

	if(Args[2])
	{
		strcpy(Args[2],"config:image=");
		strcat(Args[2],P);
	}

	reicast_main(Args[2]? 3:1,Args);
}

- (void)viewDidLoad
{
    [super viewDidLoad];
	
	self.controllerView = [[PadViewController alloc] initWithNibName:@"PadViewController" bundle:nil];
    
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (!self.context) {
        NSLog(@"Failed to create ES context");
    }
    
    GLKView *view = (GLKView *)self.view;
    view.context = self.context;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
    
    self.connectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        if ([[GCController controllers] count] == 1) {
            [self toggleHardwareController:YES];
        }
    }];
    self.disconnectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        if (![[GCController controllers] count]) {
            [self toggleHardwareController:NO];
        }
    }];
    
    if ([[GCController controllers] count]) {
        [self toggleHardwareController:YES];
	}
	[self.controllerView showController:self.view];
		
    self.iCadeReader = [[iCadeReaderView alloc] init];
    [self.view addSubview:self.iCadeReader];
    self.iCadeReader.delegate = self;
    self.iCadeReader.active = YES;
	
    [self setupGL];
    
    if (!gles_init())
        die("OPENGL FAILED");
    
    NSThread* myThread = [[NSThread alloc] initWithTarget:self
                                                 selector:@selector(emuThread)
                                                   object:nil];
    [myThread start];  // Actually create the thread
}

- (void)dealloc
{
    [self tearDownGL];
    
    if ([EAGLContext currentContext] == self.context) {
        [EAGLContext setCurrentContext:nil];
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];

    if ([self isViewLoaded] && ([[self view] window] == nil)) {
        self.view = nil;
        
        [self tearDownGL];
        
        if ([EAGLContext currentContext] == self.context) {
            [EAGLContext setCurrentContext:nil];
        }
        self.context = nil;
    }

    // Dispose of any resources that can be recreated.
}

- (void)setupGL
{
    [EAGLContext setCurrentContext:self.context];
    
}

- (void)tearDownGL
{
    [EAGLContext setCurrentContext:self.context];
    
}

#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{

}

- (void)handleKeycode:(UIButton*)button
{
	if (button == self.controllerView.img_dpad_l) {
		kcode[0] &= ~(DC_DPAD_LEFT);
	} else {
		kcode[0] |= ~(DC_DPAD_LEFT);
	}
	if (button == self.controllerView.img_dpad_r) {
		kcode[0] &= ~(DC_DPAD_RIGHT);
	} else {
		kcode[0] |= ~(DC_DPAD_RIGHT);
	}
	if (button == self.controllerView.img_dpad_u) {
		kcode[0] &= ~(DC_DPAD_UP);
	} else {
		kcode[0] |= ~(DC_DPAD_UP);
	}
	if (button == self.controllerView.img_dpad_d) {
		kcode[0] &= ~(DC_DPAD_DOWN);
	} else {
		kcode[0] |= ~(DC_DPAD_DOWN);
	}
	if (button == self.controllerView.img_abxy_a) {
		kcode[0] &= ~(DC_BTN_A);
	} else {
		kcode[0] |= (DC_BTN_A);
	}
	if (button == self.controllerView.img_abxy_b) {
		kcode[0] &= ~(DC_BTN_B);
	} else {
		kcode[0] |= (DC_BTN_B);
	}
	if (button == self.controllerView.img_abxy_x) {
		kcode[0] &= ~(DC_BTN_X);
	} else {
		kcode[0] |= (DC_BTN_X);
	}
	if (button == self.controllerView.img_abxy_y) {
		kcode[0] &= ~(DC_BTN_Y);
	} else {
		kcode[0] |= (DC_BTN_Y);
	}
	if (button == self.controllerView.img_lt) {
		kcode[0] &= ~(DC_AXIS_LT);
	} else {
		kcode[0] |= (DC_AXIS_LT);
	}
	if (button == self.controllerView.img_rt) {
		kcode[0] &= ~(DC_AXIS_RT);
	} else {
		kcode[0] |= (DC_AXIS_RT);
	}
	if (button == self.controllerView.img_start) {
		kcode[0] &= ~(DC_BTN_START);
	} else {
		kcode[0] |= (DC_BTN_START);
	}
}

- (void)toggleHardwareController:(BOOL)useHardware {
    if (useHardware) {
//		[self.controllerView hideController];
        self.gController = [GCController controllers][0];
        if (self.gController.gamepad) {
            [self.gController.gamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~(DC_BTN_A);
				} else {
					kcode[0] |= (DC_BTN_A);
				}
            }];
            [self.gController.gamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~(DC_BTN_B);
				} else {
					kcode[0] |= (DC_BTN_B);
				}
            }];
            [self.gController.gamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~(DC_BTN_X);
				} else {
					kcode[0] |= (DC_BTN_X);
				}
            }];
            [self.gController.gamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~(DC_BTN_Y);
				} else {
					kcode[0] |= (DC_BTN_Y);
				}
            }];
            [self.gController.gamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue){
				if (xValue >= 0.1) {
					kcode[0] &= ~(DC_DPAD_RIGHT);
				} else {
					kcode[0] |= ~(DC_DPAD_RIGHT);
				}
				if (xValue <= -0.1) {
					kcode[0] &= ~(DC_DPAD_LEFT);
				} else {
					kcode[0] |= ~(DC_DPAD_LEFT);
				}
				if (yValue >= 0.1) {
					kcode[0] &= ~(DC_DPAD_UP);
				} else {
					kcode[0] |= ~(DC_DPAD_UP);
				}
				if (yValue <= -0.1) {
					kcode[0] &= ~(DC_DPAD_DOWN);
				} else {
					kcode[0] |= ~(DC_DPAD_DOWN);
				}
            }];
            //Add controller pause handler here
        }
        if (self.gController.extendedGamepad) {
            [self.gController.extendedGamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~(DC_BTN_A);
				} else {
					kcode[0] |= (DC_BTN_A);
				}
            }];
            [self.gController.extendedGamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~(DC_BTN_B);
				} else {
					kcode[0] |= (DC_BTN_B);
				}
            }];
            [self.gController.extendedGamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~(DC_BTN_X);
				} else {
					kcode[0] |= (DC_BTN_X);
				}
            }];
            [self.gController.extendedGamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed && value >= 0.1) {
					kcode[0] &= ~(DC_BTN_Y);
				} else {
					kcode[0] |= (DC_BTN_Y);
				}
            }];
            [self.gController.extendedGamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue){
				if (xValue >= 0.1) {
					 kcode[0] &= ~(DC_DPAD_RIGHT);
				} else {
					kcode[0] |= ~(DC_DPAD_RIGHT);
				}
				if (xValue <= -0.1) {
					kcode[0] &= ~(DC_DPAD_LEFT);
				} else {
					kcode[0] |= ~(DC_DPAD_LEFT);
				}
				if (yValue >= 0.1) {
					kcode[0] &= ~(DC_DPAD_UP);
				} else {
					kcode[0] |= ~(DC_DPAD_UP);
				}
				if (yValue <= -0.1) {
					kcode[0] &= ~(DC_DPAD_DOWN);
				} else {
					kcode[0] |= ~(DC_DPAD_DOWN);
				}
            }];
            [self.gController.extendedGamepad.leftThumbstick.xAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value){
                
            }];
            [self.gController.extendedGamepad.leftThumbstick.yAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value){
                
            }];
        }
    } else {
        self.gController = nil;
//		[self.controllerView showController:self.view];
    }
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    screen_width = view.drawableWidth;
    screen_height = view.drawableHeight;

    glClearColor(0.65f, 0.65f, 0.65f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    while(!rend_single_frame()) ;
}


@end
