/*
	Copyright 2021 flyinghead

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
#include "mouse.h"
#include "rend/gui.h"

// Mouse buttons
// bit 0: Button C
// bit 1: Right button (B)
// bit 2: Left button (A)
// bit 3: Wheel button
u8 mo_buttons[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
// Relative mouse coordinates [-512:511]
f32 mo_x_delta[4];
f32 mo_y_delta[4];
f32 mo_wheel_delta[4];
// Absolute mouse coordinates
// Range [0:639] [0:479]
// but may be outside this range if the pointer is offscreen or outside the 4:3 window.
s32 mo_x_abs[4];
s32 mo_y_abs[4];
// previous mouse coordinates for relative motion
s32 mo_x_prev[4] = { -1, -1, -1, -1 };
s32 mo_y_prev[4] = { -1, -1, -1, -1 };
// last known screen/window size
static s32 mo_width;
static s32 mo_height;

void Mouse::setAbsPos(int x, int y, int width, int height) {
	SetMousePosition(x, y, width, height, maple_port());
}

void Mouse::setRelPos(float deltax, float deltay) {
	SetRelativeMousePosition(deltax, deltay, maple_port());
}

void Mouse::setWheel(int delta) {
	if (maple_port() >= 0 && maple_port() < (int)ARRAY_SIZE(mo_wheel_delta))
		mo_wheel_delta[maple_port()] += delta;
}

void Mouse::setButton(Button button, bool pressed)
{
	if (maple_port() >= 0 && maple_port() < (int)ARRAY_SIZE(mo_buttons))
	{
		if (pressed)
			mo_buttons[maple_port()] &= ~(1 << (int)button);
		else
			mo_buttons[maple_port()] |= 1 << (int)button;
	}
	if ((gui_is_open() || gui_mouse_captured()) && !is_detecting_input())
		// Don't register mouse clicks as gamepad presses when gui is open
		// This makes the gamepad presses to be handled first and the mouse position to be ignored
		return;
	gamepad_btn_input(button, pressed);
}


void SystemMouse::setAbsPos(int x, int y, int width, int height) {
	gui_set_mouse_position(x, y);
	Mouse::setAbsPos(x, y, width, height);
}

void SystemMouse::setButton(Button button, bool pressed) {
	int uiBtn = (int)button - 1;
	if (uiBtn < 2)
		uiBtn ^= 1;
	gui_set_mouse_button(uiBtn, pressed);
	Mouse::setButton(button, pressed);
}

void SystemMouse::setWheel(int delta) {
	gui_set_mouse_wheel(delta * 35);
	Mouse::setWheel(delta);
}


static void screenToNative(int& x, int& y, int width, int height)
{
	float fx, fy;
	if (!config::Rotate90)
	{
		float scale = 480.f / height;
		fy = y * scale;
		scale /= config::ScreenStretching / 100.f;
		fx = (x - (width - 640.f / scale) / 2.f) * scale;
	}
	else
	{
		float scale = 640.f / width;
		fx = x * scale;
		scale /= config::ScreenStretching / 100.f;
		fy = (y - (height - 480.f / scale) / 2.f) * scale;
	}
	x = (int)std::round(fx);
	y = (int)std::round(fy);
}

void SetMousePosition(int x, int y, int width, int height, u32 mouseId)
{
	if (mouseId >= ARRAY_SIZE(mo_x_abs))
		return;
	mo_width = width;
	mo_height = height;

	if (config::Rotate90)
	{
		int t = y;
		y = x;
		x = height - 1 - t;
		std::swap(width, height);
	}
	screenToNative(x, y, width, height);
	mo_x_abs[mouseId] = x;
	mo_y_abs[mouseId] = y;

	if (mo_x_prev[mouseId] != -1)
	{
		mo_x_delta[mouseId] += (f32)(x - mo_x_prev[mouseId]) * config::MouseSensitivity / 100.f;
		mo_y_delta[mouseId] += (f32)(y - mo_y_prev[mouseId]) * config::MouseSensitivity / 100.f;
	}
	mo_x_prev[mouseId] = x;
	mo_y_prev[mouseId] = y;
}

void SetRelativeMousePosition(float xrel, float yrel, u32 mouseId)
{
	if (mouseId >= ARRAY_SIZE(mo_x_delta))
		return;
	int width = mo_width;
	int height = mo_height;
	if (config::Rotate90)
	{
		std::swap(xrel, yrel);
		xrel = -xrel;
		std::swap(width, height);
	}
	float dx = xrel * config::MouseSensitivity / 100.f;
	float dy = yrel * config::MouseSensitivity / 100.f;
	mo_x_delta[mouseId] += dx;
	mo_y_delta[mouseId] += dy;
	int minX = -width / 32;
	int minY = -height / 32;
	int maxX = width + width / 32;
	int maxY = height + height / 32;
	screenToNative(minX, minY, width, height);
	screenToNative(maxX, maxY, width, height);
	mo_x_abs[mouseId] += (int)std::round(dx);
	mo_y_abs[mouseId] += (int)std::round(dy);
	mo_x_abs[mouseId] = std::min(std::max(mo_x_abs[mouseId], minX), maxX);
	mo_y_abs[mouseId] = std::min(std::max(mo_y_abs[mouseId], minY), maxY);
}

