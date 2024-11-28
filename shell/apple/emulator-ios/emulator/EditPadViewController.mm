/*
	Copyright 2024 flyinghead

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
#import "EditPadViewController.h"
#include "types.h"
#include "ui/gui.h"
#include "ui/vgamepad.h"
#include "cfg/cfg.h"

@interface EditPadViewController () {
	vgamepad::Element currentControl;
}

@end

@implementation EditPadViewController

- (void)viewDidLoad
{
	[super viewDidLoad];
	currentControl = vgamepad::Elem_None;
}

- (void)showController:(UIView *)parentView
{
	if (!cfgLoadBool("help", "EditPadTip", false))
	{
		UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Help Tip"
								   message:@"Double tap to exit."
								   preferredStyle:UIAlertControllerStyleAlert];
		[self presentViewController:alert animated:YES completion:nil];
		UIAlertAction* defaultAction = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault
									   handler:^(UIAlertAction * action) {
			cfgSaveBool("help", "EditPadTip", true);
		}];
		[alert addAction:defaultAction];
	}
	[parentView addSubview:self.view];
}

- (void)hideController
{
	[self.view removeFromSuperview];
}

- (BOOL)isControllerVisible {
	return self.view.window != nil;
}

static void normalize(CGPoint& pos, const CGSize& size) {
	pos.x /= size.width;
	pos.y /= size.height;
}

- (IBAction)handlePan:(UIPanGestureRecognizer *)recognizer
{
	CGPoint loc = [recognizer locationInView:self.view];
	normalize(loc, self.view.bounds.size);
	switch (recognizer.state)
	{
		case UIGestureRecognizerStateBegan:
			currentControl = vgamepad::layoutHitTest(loc.x, loc.y);
			break;
		case UIGestureRecognizerStateEnded:
			currentControl = vgamepad::Elem_None;
			break;
		case UIGestureRecognizerStateChanged:
			if (currentControl != vgamepad::Elem_None)
			{
				CGPoint translation = [recognizer translationInView:self.view];
				[recognizer setTranslation:CGPointMake(0, 0) inView:self.view];
				normalize(translation, self.view.bounds.size);
				vgamepad::translateElement(currentControl, translation.x, translation.y);
			}
			break;
		case UIGestureRecognizerStateCancelled:
			currentControl = vgamepad::Elem_None;
			break;
		default:
			break;
	}
}

- (IBAction)handlePinch:(UIPinchGestureRecognizer *)recognizer
{
	CGPoint loc = [recognizer locationInView:self.view];
	normalize(loc, self.view.bounds.size);
	switch (recognizer.state)
	{
		case UIGestureRecognizerStateBegan:
			currentControl = vgamepad::layoutHitTest(loc.x, loc.y);
			break;
		case UIGestureRecognizerStateChanged:
			if (currentControl != vgamepad::Elem_None)
				vgamepad::scaleElement(currentControl, recognizer.scale);
			break;
		case UIGestureRecognizerStateEnded:
			currentControl = vgamepad::Elem_None;
			break;
		case UIGestureRecognizerStateCancelled:
			currentControl = vgamepad::Elem_None;
			break;
		default:
			break;
	}
	recognizer.scale = 1;
}

- (IBAction)handleTap:(UITapGestureRecognizer *)recognizer
{
	if (recognizer.state == UIGestureRecognizerStateRecognized)
		gui_open_settings();
}

@end

