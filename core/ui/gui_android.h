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

	_Count
};

#ifdef __ANDROID__

void setPosition(ControlId id, float x, float y, float w, float h);
void show();
void hide();
void draw();
void startEditing();
void stopEditing(bool canceled);
void resetEditing();
void displayCommands();

#else

void setPosition(ControlId id, float x, float y, float w, float h) {}
void show() {}
void hide() {}
void draw() {}
void startEditing() {}
void stopEditing(bool canceled) {}
void resetEditing() {}
void displayCommands() {}

#endif
}	// namespace vgamepad
