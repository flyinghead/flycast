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

@interface ViewController : GLKViewController <iCadeEventDelegate>

@property NSString* diskImage;
@property (nonatomic) iCadeReaderView* iCadeReader;
@property (nonatomic) GCController *gController __attribute__((weak_import));
@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

@property (nonatomic, strong) IBOutlet UIView *controllerView;

@property (nonatomic, strong) IBOutlet UIButton* img_dpad_l;
@property (nonatomic, strong) IBOutlet UIButton* img_dpad_r;
@property (nonatomic, strong) IBOutlet UIButton* img_dpad_u;
@property (nonatomic, strong) IBOutlet UIButton* img_dpad_d;
@property (nonatomic, strong) IBOutlet UIButton* img_abxy_a;
@property (nonatomic, strong) IBOutlet UIButton* img_abxy_b;
@property (nonatomic, strong) IBOutlet UIButton* img_abxy_x;
@property (nonatomic, strong) IBOutlet UIButton* img_abxy_y;
@property (nonatomic, strong) IBOutlet UIButton* img_vjoy;
@property (nonatomic, strong) IBOutlet UIButton* img_lt;
@property (nonatomic, strong) IBOutlet UIButton* img_rt;
@property (nonatomic, strong) IBOutlet UIButton* img_start;

@end
