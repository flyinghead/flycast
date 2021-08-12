//
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import "EmulatorView.h"

#include "types.h"
#include "rend/gui.h"

@implementation EmulatorView

- (void)touchLocation:(UITouch*)touch;
{
	float scale = self.contentScaleFactor;
	CGPoint location = [touch locationInView:touch.view];
	_mouse->setAbsPos(location.x * scale, location.y * scale, self.bounds.size.width * scale, self.bounds.size.height * scale);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	UITouch *touch = [touches anyObject];
	[self touchLocation:touch];
	if (gui_is_open())
		_mouse->setButton(Mouse::LEFT_BUTTON, true);
	[super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	UITouch *touch = [touches anyObject];
	[self touchLocation:touch];
	if (gui_is_open())
		_mouse->setButton(Mouse::LEFT_BUTTON, false);
	[super touchesEnded:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	UITouch *touch = [touches anyObject];
	[self touchLocation:touch];
	[super touchesMoved:touches withEvent:event];
}

@end
