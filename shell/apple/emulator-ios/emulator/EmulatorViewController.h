//
//  EmulatorViewController.h
//  emulator
//
//  Created by Karen Tsai (angelXwind) on 2014/3/5.
//  Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.
//

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#import <GameController/GameController.h>
#import "iCadeReaderView.h"
#import "PadViewController.h"

@interface ViewController : GLKViewController <iCadeEventDelegate>

@property NSString* diskImage;
@property (nonatomic) iCadeReaderView* iCadeReader;
@property (nonatomic) GCController *gController __attribute__((weak_import));
@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

@property (nonatomic, strong) PadViewController *controllerView;

- (void)handleKeycode:(UIButton*)button;

@end
