/*
	Copyright 2021 flyinghead
	Copyright (c) 2015 reicast. All rights reserved.

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
//
//  Created by Lounge Katt on 8/25/15.
//
#import <UIKit/UIKit.h>

@interface PadViewController : UIViewController

@property (weak, nonatomic) IBOutlet UIImageView *joystick;
@property (weak, nonatomic) IBOutlet UIImageView *joystickBackground;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint *joyXConstraint;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint *joyYConstraint;

- (void) showController:(UIView *)parentView;
- (void) hideController;
- (BOOL) isControllerVisible;

@end
