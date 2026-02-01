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
#pragma once
#include "gamepad_device.h"
#include "oslib/i18n.h"
#include <mutex>

// Mouse position and buttons
extern u8 mo_buttons[4];
extern s32 mo_x_abs[4];
extern s32 mo_y_abs[4];
extern float mo_x_delta[4];
extern float mo_y_delta[4];
extern float mo_wheel_delta[4];
extern std::mutex relPosMutex;

extern s32 mo_x_prev[4];
extern s32 mo_y_prev[4];

void SetMousePosition(int x, int y, int width, int height, u32 mouseId = 0);
void SetRelativeMousePosition(float xrel, float yrel, u32 mouseId = 0);

class MouseInputMapping : public InputMapping
{
public:
	MouseInputMapping()
	{
		name = "Mouse";
		set_button(DC_BTN_A, 2);		// Left
		set_button(DC_BTN_B, 1);		// Right
		set_button(DC_BTN_START, 3);	// Middle

		dirty = false;
	}
};

class Mouse : public GamepadDevice
{
protected:
	Mouse(const char *apiName, int maplePort = 0) : GamepadDevice(maplePort, apiName) {
		this->_name = i18n::Ts("Mouse");
	}

	std::shared_ptr<InputMapping> getDefaultMapping() override {
		return std::make_shared<MouseInputMapping>();
	}

public:
	enum Button {
		LEFT_BUTTON = 2,
		RIGHT_BUTTON = 1,
		MIDDLE_BUTTON = 3,
		BUTTON_4 = 4,
		BUTTON_5 = 5
	};

	const char *get_button_name(u32 code) override
	{
		switch((Button)code)
		{
		case LEFT_BUTTON:
			return i18n::T("Left Button");
		case RIGHT_BUTTON:
			return i18n::T("Right Button");
		case MIDDLE_BUTTON:
			return i18n::T("Middle Button");
		case BUTTON_4:
			return i18n::T("Button 4");
		case BUTTON_5:
			return i18n::T("Button 5");
		default:
			return nullptr;
		}
	}

	void setAbsPos(int x, int y, int width, int height);
	void setRelPos(float deltax, float deltay);
	void setButton(Button button, bool pressed);
	void setWheel(int delta);
};

class SystemMouse : public Mouse
{
	bool touchMouse;

protected:
	SystemMouse(const char *apiName, int maplePort = 0, bool touchMouse = false)
		: Mouse(apiName, maplePort), touchMouse(touchMouse)
	{}

public:
	void setAbsPos(int x, int y, int width, int height);
	void setButton(Button button, bool pressed);
	void setWheel(int delta);
};

class TouchMouse : public SystemMouse
{
public:
	TouchMouse() : SystemMouse("Flycast", -1, true)
	{
		_name = i18n::Ts("Touch Mouse");
		_unique_id = "touch_mouse";
		loadMapping();
	}
};

