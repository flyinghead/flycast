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
#include "cfg/cfg.h"
#include "ui/vgamepad.h"

@interface PadViewController () {
	UITouch *joyTouch;
	CGPoint joyBias;
	std::shared_ptr<IOSVirtualGamepad> virtualGamepad;
	NSMutableDictionary *touchToButton;
	NSTimer *hideTimer;
}

@end

@implementation PadViewController

- (void)viewDidLoad
{
	[super viewDidLoad];
	virtualGamepad = std::make_shared<IOSVirtualGamepad>();
	GamepadDevice::Register(virtualGamepad);
	touchToButton = [[NSMutableDictionary alloc] init];
}

- (void)showController:(UIView *)parentView
{
	if (!cfgLoadBool("help", "PauseGameTip", false))
	{
		UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Help Tip"
								   message:@"To pause the game, press Up+Down or Left+Right on the virtual DPad."
								   preferredStyle:UIAlertControllerStyleAlert];
		[self presentViewController:alert animated:YES completion:nil];
		UIAlertAction* defaultAction = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault
									   handler:^(UIAlertAction * action) {
			cfgSaveBool("help", "PauseGameTip", true);
		}];
		[alert addAction:defaultAction];
	}
	[parentView addSubview:self.view];
	[self startHideTimer];
}

- (void)hideController
{
	[self resetAnalog];
	[hideTimer invalidate];
	[self.view removeFromSuperview];
}

- (BOOL)isControllerVisible {
	return self.view.window != nil;
}

-(void)startHideTimer
{
	[hideTimer invalidate];
	hideTimer = [NSTimer scheduledTimerWithTimeInterval:10
												 target:self
											   selector:@selector(hideTimer)
											   userInfo:nil
												repeats:NO];
	vgamepad::show();
}

-(void)hideTimer {
	vgamepad::hide();
}

- (void)resetAnalog
{
	joyTouch = nil;
	virtualGamepad->joystickInput(0, 0);
}

static CGPoint translateCoords(const CGPoint& pos, const CGSize& size)
{
	CGFloat hscale = 480.0 / size.height;
	CGPoint p;
	p.y = pos.y * hscale;
	p.x = (pos.x - (size.width - 640.0 / hscale) / 2.0) * hscale;
	return p;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	bool forwardEvent = true;
	[self startHideTimer];
	for (UITouch *touch in touches)
	{
		CGPoint point = [touch locationInView:self.view];
		point = translateCoords(point, self.view.bounds.size);
		vgamepad::ControlId control = vgamepad::hitTest(point.x, point.y);
		if (joyTouch == nil && (control == vgamepad::AnalogArea || control == vgamepad::AnalogStick))
		{
			[self resetAnalog];
			joyTouch = touch;
			joyBias = point;
			forwardEvent = false;
			continue;
		}
		NSValue *key = [NSValue valueWithPointer:(const void *)touch];
		if (control != vgamepad::None && control != vgamepad::AnalogArea
				&& control != vgamepad::AnalogStick && touchToButton[key] == nil)
		{
			touchToButton[key] = [NSNumber numberWithInt:control];
			// button down
			virtualGamepad->buttonInput(control, true);
			forwardEvent = false;
		}
	}
	if (forwardEvent)
		[super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	bool forwardEvent = true;
	for (UITouch *touch in touches)
	{
		if (touch == joyTouch) {
			[self resetAnalog];
			forwardEvent = false;
			continue;
		}
		NSValue *key = [NSValue valueWithPointer:(const void *)touch];
		NSNumber *control = touchToButton[key];
		if (control != nil) {
			[touchToButton removeObjectForKey:key];
			// button up
			virtualGamepad->buttonInput(static_cast<vgamepad::ControlId>(control.intValue), false);
			forwardEvent = false;
		}
	}
	if (forwardEvent)
		[super touchesEnded:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	bool forwardEvent = true;
	[self startHideTimer];
	for (UITouch *touch in touches)
	{
		CGPoint point = [touch locationInView:self.view];
		point = translateCoords(point, self.view.bounds.size);
		if (touch == joyTouch)
		{
			point.x -= joyBias.x;
			point.y -= joyBias.y;
			double sz = vgamepad::getControlWidth(vgamepad::AnalogStick);
			point.x = std::max<CGFloat>(std::min<CGFloat>(1.0, point.x / sz), -1.0);
			point.y = std::max<CGFloat>(std::min<CGFloat>(1.0, point.y / sz), -1.0);
			virtualGamepad->joystickInput(point.x, point.y);
			forwardEvent = false;
			continue;
		}
		vgamepad::ControlId control = vgamepad::hitTest(point.x, point.y);
		NSValue *key = [NSValue valueWithPointer:(const void *)touch];
		NSNumber *prevControl = touchToButton[key];
		if (prevControl.intValue == control)
			continue;
		if (prevControl != nil && prevControl.intValue != vgamepad::None && prevControl.intValue != vgamepad::AnalogArea) {
			// button up
			virtualGamepad->buttonInput(static_cast<vgamepad::ControlId>(prevControl.intValue), false);
		}
		// button down
		virtualGamepad->buttonInput(control, true);
		touchToButton[key] = [NSNumber numberWithInt:control];
		forwardEvent = false;
	}
	if (forwardEvent)
		[super touchesMoved:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	bool forwardEvent = true;
	for (UITouch *touch in touches) {
		if (touch == joyTouch) {
			[self resetAnalog];
			forwardEvent = false;
			continue;
		}
		NSValue *key = [NSValue valueWithPointer:(const void *)touch];
		NSNumber *control = touchToButton[key];
		if (control != nil) {
			[touchToButton removeObjectForKey:key];
			// button up
			virtualGamepad->buttonInput(static_cast<vgamepad::ControlId>(control.intValue), false);
			forwardEvent = false;
		}
	}
	if (forwardEvent)
		[super touchesCancelled:touches withEvent:event];
}
@end
