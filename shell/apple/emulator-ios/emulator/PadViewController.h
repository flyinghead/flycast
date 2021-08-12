//
//  Created by Lounge Katt on 8/25/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "FlycastViewController.h"

@interface PadViewController : UIViewController

@property (weak, nonatomic) IBOutlet UIImageView *joystick;
@property (weak, nonatomic) IBOutlet UIImageView *joystickBackground;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint *joyXConstraint;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint *joyYConstraint;

@property (nonatomic, strong) FlycastViewController *handler;

- (void) showController:(UIView *)parentView;
- (void) hideController;
- (BOOL) isControllerVisible;

@end
