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
#import "PadViewController.h"
#include "ios_gamepad.h"

@interface PadViewController () {
	UITouch *joyTouch;
	CGPoint joyBias;
	std::shared_ptr<IOSVirtualGamepad> virtualGamepad;
}

@end

@implementation PadViewController

- (void)viewDidLoad
{
	[super viewDidLoad];
	virtualGamepad = std::make_shared<IOSVirtualGamepad>();
	GamepadDevice::Register(virtualGamepad);
}

- (void)showController:(UIView *)parentView
{
	[parentView addSubview:self.view];
}

- (void)hideController
{
	[self resetTouch];
	[self.view removeFromSuperview];
}

- (BOOL)isControllerVisible {
	return self.view.window != nil;
}

- (IBAction)keycodeDown:(id)sender
{
	virtualGamepad->gamepad_btn_input((u32)((UIButton *)sender).tag, true);
}

- (IBAction)keycodeUp:(id)sender
{
	virtualGamepad->gamepad_btn_input((u32)((UIButton *)sender).tag, false);
}

- (void)resetTouch
{
	joyTouch = nil;
	self.joyXConstraint.constant = 0;
	self.joyYConstraint.constant = 0;
	virtualGamepad->gamepad_axis_input(IOS_AXIS_LX, 0);
	virtualGamepad->gamepad_axis_input(IOS_AXIS_LY, 0);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch == nil) {
		for (UITouch *touch in touches) {
			CGPoint loc = [touch locationInView:[self joystickBackground]];
			if ([self.joystickBackground pointInside:loc withEvent:event]) {
				joyTouch = touch;
				joyBias = loc;
				virtualGamepad->gamepad_axis_input(IOS_AXIS_LX, 0);
				virtualGamepad->gamepad_axis_input(IOS_AXIS_LY, 0);
				break;
			}
		}
	}
	[super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch != nil) {
		for (UITouch *touch in touches) {
			if (touch == joyTouch) {
				[self resetTouch];
				break;
			}
		}
	}
	[super touchesEnded:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch != nil) {
		for (UITouch *touch in touches) {
			if (touch == joyTouch) {
				CGPoint pos = [touch locationInView:[self joystickBackground]];
				pos.x -= joyBias.x;
				pos.y -= joyBias.y;
				pos.x = std::max<CGFloat>(std::min<CGFloat>(25.0, pos.x), -25.0);
				pos.y = std::max<CGFloat>(std::min<CGFloat>(25.0, pos.y), -25.0);
				self.joyXConstraint.constant = pos.x;
				self.joyYConstraint.constant = pos.y;
				virtualGamepad->gamepad_axis_input(IOS_AXIS_LX, (s8)std::round(pos.x * 32767.0 / 25.0));
				virtualGamepad->gamepad_axis_input(IOS_AXIS_LY, (s8)std::round(pos.y * 32767.0 / 25.0));
				break;
			}
		}
	}
	[super touchesMoved:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch != nil) {
		for (UITouch *touch in touches) {
			if (touch == joyTouch) {
				[self resetTouch];
				break;
			}
		}
	}
	[super touchesCancelled:touches withEvent:event];
}
@end
