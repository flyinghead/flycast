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
#import "EmulatorView.h"

#include "types.h"
#include "rend/gui.h"
#include "ios_gamepad.h"

@implementation EmulatorView {
	std::shared_ptr<IOSTouchMouse> mouse;
}

- (void)didMoveToSuperview
{
	[super didMoveToSuperview];
	mouse = std::make_shared<IOSTouchMouse>();
	GamepadDevice::Register(mouse);
}

- (void)touchLocation:(UITouch*)touch;
{
	float scale = self.contentScaleFactor;
	CGPoint location = [touch locationInView:touch.view];
	mouse->setAbsPos(location.x * scale, location.y * scale, self.bounds.size.width * scale, self.bounds.size.height * scale);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	UITouch *touch = [touches anyObject];
	[self touchLocation:touch];
	if (gui_is_open())
		mouse->setButton(Mouse::LEFT_BUTTON, true);
	[super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	UITouch *touch = [touches anyObject];
	[self touchLocation:touch];
	if (gui_is_open())
		mouse->setButton(Mouse::LEFT_BUTTON, false);
	[super touchesEnded:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
{
	UITouch *touch = [touches anyObject];
	[self touchLocation:touch];
	[super touchesMoved:touches withEvent:event];
}

@end
