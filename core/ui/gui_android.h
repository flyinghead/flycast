/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "types.h"

namespace vgamepad
{
enum ControlId
{
	None = -1,
	Left,
	Up,
	Right,
	Down,
	X,
	Y,
	B,
	A,
	Start,
	LeftTrigger,
	RightTrigger,
	AnalogArea,
	AnalogStick,
	FastForward,

	LeftUp,
	RightUp,
	LeftDown,
	RightDown,
	
	_Count,
	_VisibleCount = FastForward + 1,
};

enum Element
{
	Elem_None = -1,
	Elem_DPad,
	Elem_Buttons,
	Elem_Start,
	Elem_LT,
	Elem_RT,
	Elem_Analog,
	Elem_FForward,
};

#if defined(__ANDROID__) || defined(TARGET_IPHONE)

void setPosition(ControlId id, float x, float y, float w = 0.f, float h = 0.f);	// Legacy android
void show();
void hide();
void draw();
void startEditing();
void pauseEditing();
void stopEditing(bool canceled);
void resetEditing();
void displayCommands();

ControlId hitTest(float x, float y);
u32 controlToDcKey(ControlId control);
void setAnalogStick(float x, float y);
float getControlWidth(ControlId);

void applyUiScale();
Element layoutHitTest(float x, float y);
void translateElement(Element element, float dx, float dy);
void scaleElement(Element element, float factor);

#else

void show() {}
void hide() {}
void draw() {}
void startEditing() {}
void pauseEditing() {}
void displayCommands() {}
void applyUiScale() {}

#endif
}	// namespace vgamepad
