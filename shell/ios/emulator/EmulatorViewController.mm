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
    

    //This looks like the right place, rite?
    char text[2]="";
    
    char* prms[2];
    prms[0]=text;
    
    reicast_main(1, prms);
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (!self.context) {
        NSLog(@"Failed to create ES context");
    }
    
    GLKView *view = (GLKView *)self.view;
    view.context = self.context;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
	
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


- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    screen_width = view.drawableWidth;
    screen_height = view.drawableHeight;

    glClearColor(0.65f, 0.65f, 0.65f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    while(!rend_single_frame()) ;
}


@end
