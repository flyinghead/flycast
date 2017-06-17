//
//  EmulatorView.h
//  emulator
//
//  Created by admin on 1/18/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import <GLKit/GLKit.h>

@interface EmulatorView : GLKView

- (void)handleKeyDown:(UIButton*)button;
- (void)handleKeyUp:(UIButton*)button;

@property (nonatomic, strong) UIViewController *controllerView;

@end
