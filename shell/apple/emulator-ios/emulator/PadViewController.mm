//
//  Created by Lounge Katt on 8/25/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import "PadViewController.h"
#import "EmulatorView.h"

@interface PadViewController ()

@end

@implementation PadViewController

- (void)viewDidLoad {
    [super viewDidLoad];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)showController:(UIView *)parentView
{
	[parentView addSubview:self.view];
}

- (void)hideController
{
	[self.view removeFromSuperview];
}

- (BOOL)isControllerVisible {
	if (self.view.window != nil) {
		return YES;
	}
	return NO;
}

- (void)setControlOutput:(EmulatorView *)output
{
	self.handler = output;
}

- (IBAction)keycodeDown:(id)sender
{
	[self.handler handleKeyDown:(UIButton*)sender];
}

- (IBAction)keycodeUp:(id)sender
{
	[self.handler handleKeyUp:(UIButton*)sender];
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
	self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
	if (self) {
		// Custom initialization
	}
	return self;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch != nil)
		return;
	for (UITouch *touch in touches) {
		CGPoint loc = [touch locationInView:[self joystickBackground]];
		if ([self.joystickBackground pointInside:loc withEvent:event]) {
			joyTouch = touch;
			joyBias = loc;
			joyx[0] = 0;
			joyy[0] = 0;
			// don't let any gesture recognizer steal our touch
			for (UIGestureRecognizer *gesture in touch.gestureRecognizers)
				[gesture ignoreTouch:touch forEvent:event];
			break;
		}
	}
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch == nil)
		return;
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

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch == nil)
		return;
	for (UITouch *touch in touches) {
		if (touch == joyTouch) {
			CGPoint pos = [touch locationInView:[self joystickBackground]];
			pos.x -= joyBias.x;
			pos.y -= joyBias.y;
			pos.x = std::max<CGFloat>(std::min<CGFloat>(25.0, pos.x), -25.0);
			pos.y = std::max<CGFloat>(std::min<CGFloat>(25.0, pos.y), -25.0);
			self.joyXConstraint.constant = pos.x;
			self.joyYConstraint.constant = pos.y;
			joyx[0] = (s8)round(pos.x * 127.0 / 25.0);
			joyy[0] = (s8)round(pos.y * 127.0 / 25.0);
			break;
		}
	}
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	if (joyTouch == nil)
		return;
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
@end
