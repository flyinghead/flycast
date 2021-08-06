//
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import <GLKit/GLKit.h>
#include "ios_mouse.h"

@interface EmulatorView : GLKView

- (void)handleKeyDown:(UIButton*)button;
- (void)handleKeyUp:(UIButton*)button;
- (void)touchLocation:(UITouch*)touch;

@property (nonatomic, strong) UIViewController *controllerView;
@property IOSMouse *mouse;

@end
