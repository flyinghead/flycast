//
//  Created by Lounge Katt on 8/25/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import "PadViewController.h"
#import "EmulatorView.h"

@interface PadViewController () {
	UITouch *joyTouch;
	CGPoint joyBias;
}

@end

@implementation PadViewController

- (void)showController:(UIView *)parentView
{
	[parentView addSubview:self.view];
}

- (void)hideController
{
	[self.view removeFromSuperview];
}

- (BOOL)isControllerVisible {
	return self.view.window != nil;
}

- (IBAction)keycodeDown:(id)sender
{
	[self.handler handleKeyDown:(enum IOSButton)((UIButton *)sender).tag];
}

- (IBAction)keycodeUp:(id)sender
{
	[self.handler handleKeyUp:(enum IOSButton)((UIButton *)sender).tag];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch == nil) {
		for (UITouch *touch in touches) {
			CGPoint loc = [touch locationInView:[self joystickBackground]];
			if ([self.joystickBackground pointInside:loc withEvent:event]) {
				joyTouch = touch;
				joyBias = loc;
				joyx[0] = 0;
				joyy[0] = 0;
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
				joyTouch = nil;
				self.joyXConstraint.constant = 0;
				self.joyYConstraint.constant = 0;
				joyx[0] = 0;
				joyy[0] = 0;
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
				// 10% dead zone
				if (pos.x * pos.x + pos.y * pos.y < 2.5 * 2.5) {
					pos.x = 0;
					pos.y = 0;
				}
				self.joyXConstraint.constant = pos.x;
				self.joyYConstraint.constant = pos.y;
				joyx[0] = (s8)round(pos.x * 127.0 / 25.0);
				joyy[0] = (s8)round(pos.y * 127.0 / 25.0);
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
				joyTouch = nil;
				self.joyXConstraint.constant = 0;
				self.joyYConstraint.constant = 0;
				joyx[0] = 0;
				joyy[0] = 0;
				break;
			}
		}
	}
	[super touchesCancelled:touches withEvent:event];
}
@end
