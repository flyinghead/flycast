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
#include "rawinput.h"
#ifndef TARGET_UWP
#include <hidusage.h>
#include <map>
#include "hw/maple/maple_devs.h"

#ifndef CALLBACK
#define CALLBACK
#endif

HWND getNativeHwnd();

namespace rawinput {

static std::map<HANDLE, std::shared_ptr<RawMouse>> mice;
static std::map<HANDLE, std::shared_ptr<RawKeyboard>> keyboards;
static HWND hWnd;

const u8 Ps2toUsb[0x80] {
		// 00
		0xff,
		0x29,	// Esc
		0x1e,	// 1
		0x1f,	// 2
		0x20,	// 3
		0x21,	// 4
		0x22,	// 5
		0x23,	// 6
		0x24,	// 7
		0x25,	// 8
		0x26,	// 9
		0x27,	// 0
		0x2d,	// - _
		0x2e,	// = +
		0x2a,	// Backspace
		0x2b,	// Tab
		// 10
		0x14,	// Q
		0x1a,	// W
		0x08,	// E
		0x15,	// R
		0x17,	// T
		0x1c, 	// Y
		0x18,	// U
		0x0c,	// I
		0x12,	// O
		0x13,	// P
		0x2f,	// [ {
		0x30,	// ] }
		0x28,	// Return
		0xe0,	// Left Control
		0x04,	// A
		0x16,	// S
		// 20
		0x07,	// D
		0x09,	// F
		0x0a,	// G
		0x0b,	// H
		0x0d,	// J
		0x0e, 	// K
		0x0f,	// L
		0x33,	// ; :
		0x34,	// ' "
		0x35,	// ` ~
		0xe1,	// Left Shift
		0x31,	// \ | (US)
		0x1d,	// Z
		0x1b,	// X
		0x06,	// C
		0x19,	// V
		// 30
		0x05,	// B
		0x11,	// N
		0x10,	// M
		0x36,	// , <
		0x37,	// . >
		0x38,	// / ?
		0xe5,	// Right Shift
		0x55,	// Keypad *
		0xe2,	// Left Alt
		0x2c,	// Space
		0x39,	// Caps Lock
		0x3a,	// F1
		0x3b,	// F2
		0x3c,	// F3
		0x3d,	// F4
		0x3e,	// F5
		// 40
		0x3f,	// F6
		0x40,	// F7
		0x41,	// F8
		0x42,	// F9
		0x43,	// F10
		0x53,	// Num Lock
		0x47,	// Scroll Lock
		0x5f,	// Keypad 7
		0x60,	// Keypad 8
		0x61,	// Keypad 9
		0x56,	// Keypad -
		0x5c,	// Keypad 4
		0x5d,	// Keypad 5
		0x5e,	// Keypad 6
		0x57,	// Keypad +
		0x59,	// Keypad 1
		// 50
		0x5a,	// Keypad 2
		0x5b,	// Keypad 3
		0x62,	// Keypad 0
		0x63,	// Keypad .
		0xff,
		0xff,
		0x64,	// (Europe2)
		0x44,	// F11
		0x45,	// F12
		0x67,	// Keypad =
		0xff,
		0xff,
		0x8c,	// Int'l 6
		0xff,
		0xff,
		0xff,
		// 60
		0xff,
		0xff,
		0xff,
		0xff,
		0x68,	// F13
		0x69,	// F14
		0x6a,	// F15
		0x6b,	// F16
		0x6c,	// F17
		0x6d,	// F18
		0x6e,	// F19
		0x6f,	// F20
		0x70,	// F21
		0x71,	// F22
		0x72,	// F23
		0xff,
		// 70
		0x88,	// Int'l 2 (Katakana/Hiragana)
		0xff,
		0xff,
		0x87,	// Int'l 1 (Ro)
		0xff,
		0xff,
		0x73,	// F24
		0x93,	// Lang 4 Hiragana
		0x92,	// Lang 3 Katakana
		0x8a,	// Int'l 4 (Henkan)
		0xff,
		0x8b,	// Int'l 5 (Muhenkan)
		0xff,
		0x89,	// Int'l 3 (Yen)
		0x85,	// Keypad ,
		0xff,
};

const u8 Ps2toUsbE0[][2] {
	{ 0x1c, 0x58 },	// Keypad Enter
	{ 0x1d, 0xe4 },	// Right Control
	{ 0x35, 0x54 },	// Keypad /
	{ 0x37, 0x46 },	// Print Screen
	{ 0x38, 0xe6 },	// Right Alt
	{ 0x46, 0x48 },	// Break
	{ 0x47, 0x4a },	// Home
	{ 0x48, 0x52 },	// Up
	{ 0x49, 0x4b },	// Page Up
	{ 0x4b, 0x50 },	// Left
	{ 0x4d, 0x4f },	// Right
	{ 0x4f, 0x4d },	// End
	{ 0x50, 0x51 },	// Down
	{ 0x51, 0x4e },	// Page Down
	{ 0x52, 0x49 },	// Insert
	{ 0x53, 0x4c },	// Delete
	{ 0x5b, 0xe3 },	// Left GUI
	{ 0x5c, 0xe7 },	// Right GUI
	{ 0x5d, 0x65 },	// App

};

RawMouse::RawMouse(int maple_port, const std::string& name, const std::string& uniqueId, HANDLE handle) :
		Mouse("RAW", maple_port), handle(handle)
{
	this->_name = name;
	this->_unique_id = uniqueId;
	std::replace(this->_unique_id.begin(), this->_unique_id.end(), '=', '_');
	std::replace(this->_unique_id.begin(), this->_unique_id.end(), '[', '_');
	std::replace(this->_unique_id.begin(), this->_unique_id.end(), ']', '_');
	loadMapping();

	setAbsPos(settings.display.width / 2, settings.display.height / 2, settings.display.width, settings.display.height);
}

void RawMouse::buttonInput(Button button, u16 flags, u16 downFlag, u16 upFlag)
{
	if (flags & (downFlag | upFlag))
		setButton(button, flags & downFlag);
}

void RawMouse::updateState(RAWMOUSE* state)
{
	if (state->usFlags & MOUSE_MOVE_ABSOLUTE)
	{
		bool isVirtualDesktop = (state->usFlags & MOUSE_VIRTUAL_DESKTOP) == MOUSE_VIRTUAL_DESKTOP;
		int width = GetSystemMetrics(isVirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
		int height = GetSystemMetrics(isVirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

		POINT pt { long(state->lLastX / 65535.0f * width), long(state->lLastY / 65535.0f * height) };
		ScreenToClient(getNativeHwnd(), &pt);
		setAbsPos(pt.x, pt.y, settings.display.width, settings.display.height);
	}
	else if (state->lLastX != 0 || state->lLastY != 0)
		setRelPos(state->lLastX, state->lLastY);
	buttonInput(LEFT_BUTTON, state->usButtonFlags, RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP);
	buttonInput(MIDDLE_BUTTON, state->usButtonFlags, RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP);
	buttonInput(RIGHT_BUTTON, state->usButtonFlags, RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP);
	buttonInput(BUTTON_4, state->usButtonFlags, RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP);
	buttonInput(BUTTON_5, state->usButtonFlags, RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP);
	if ((state->usButtonFlags & RI_MOUSE_WHEEL))
		setWheel(-(short)state->usButtonData / WHEEL_DELTA);
}

static LRESULT CALLBACK rawWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg != WM_INPUT)
		return DefWindowProcA(hWnd, msg, wParam, lParam);

	RAWINPUT ri;
	UINT size = sizeof(ri);
	if (GET_RAWINPUT_CODE_WPARAM(wParam) != RIM_INPUT  // app isn't in the foreground
			|| GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
	{
		DefWindowProcA(hWnd, msg, wParam, lParam);
		return 0;
	}

	switch (ri.header.dwType)
	{
	case RIM_TYPEKEYBOARD:
		{
			if ((ri.data.keyboard.Flags & RI_KEY_E1) != 0)
				break;
			auto it = keyboards.find(ri.header.hDevice);
			if (it == keyboards.end())
				break;

			u16 scancode = ri.data.keyboard.MakeCode;
			bool pressed = (ri.data.keyboard.Flags & RI_KEY_BREAK) == 0;
			u8 keycode = 0xff;
			if ((ri.data.keyboard.Flags & RI_KEY_E0) != 0)
			{
				for (u32 i = 0; i < ARRAY_SIZE(Ps2toUsbE0); i++)
					if (Ps2toUsbE0[i][0] == scancode)
					{
						keycode = Ps2toUsbE0[i][1];
						DEBUG_LOG(INPUT, "[%d] E0 key %x -> %x", it->second->maple_port(), scancode, keycode);
						break;
					}
			}
			else
			{
				keycode = Ps2toUsb[scancode];
				DEBUG_LOG(INPUT, "[%d] key %x -> %x", it->second->maple_port(), scancode, keycode);
			}
			if (keycode != 0xff)
				it->second->keyboard_input(keycode, pressed);
		}
		break;

	case RIM_TYPEMOUSE:
		{
			auto it = mice.find(ri.header.hDevice);
			if (it != mice.end())
				it->second->updateState(&ri.data.mouse);
		}
		break;
	}

	DefWindowProcA(hWnd, msg, wParam, lParam);
	return 0;
}

static void createWindow()
{
	WNDCLASSA wndClass {};
	wndClass.hInstance = GetModuleHandleA(nullptr);
	if (!wndClass.hInstance)
		return;
	wndClass.lpfnWndProc = rawWindowProc;
	wndClass.lpszClassName = "flycastRawInput";
	if (RegisterClassA(&wndClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		return;

	hWnd = CreateWindowExA(0, wndClass.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
	if (hWnd == nullptr)
		UnregisterClassA(wndClass.lpszClassName, nullptr);
}

static void destroyWindow()
{
	if (hWnd == nullptr)
		return;

	DestroyWindow(hWnd);
	hWnd = nullptr;
	UnregisterClassA("flycastRawInput", nullptr);
}

static void findDevices()
{
	u32 numDevices;
	GetRawInputDeviceList(NULL, &numDevices, sizeof(RAWINPUTDEVICELIST));
	if (numDevices == 0)
		return;

	RAWINPUTDEVICELIST *deviceList = new RAWINPUTDEVICELIST[numDevices];
	GetRawInputDeviceList(deviceList, &numDevices, sizeof(RAWINPUTDEVICELIST));
	for (u32 i = 0; i < numDevices; ++i)
	{
		RAWINPUTDEVICELIST& device = deviceList[i];
		if (device.dwType == RIM_TYPEMOUSE || device.dwType == RIM_TYPEKEYBOARD)
		{
			// Get the device name
			std::string name;
			std::string uniqueId;
			u32 size;
			GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, nullptr, &size);
			if (size > 0)
			{
				std::vector<char> deviceNameData(size);
				u32 res = GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, &deviceNameData[0], &size);
				if (res != (u32)-1)
				{
					std::string deviceName(&deviceNameData[0], std::strlen(&deviceNameData[0]));
					if (deviceName.substr(0, 8) == "\\\\?\\HID#")
						deviceName = deviceName.substr(8);
					uniqueId = (device.dwType == RIM_TYPEMOUSE ? "raw_mouse_" : "raw_keyboard_") + deviceName;
					if (deviceName.length() > 17 && deviceName.substr(0, 4) == "VID_" && deviceName.substr(8, 5) == "&PID_")
						deviceName = deviceName.substr(0, 17);
					name = (device.dwType == RIM_TYPEMOUSE ? "Mouse " : "Keyboard ") + deviceName;
				}
			}
			uintptr_t handle = (uintptr_t)device.hDevice;
			if (name.empty())
				name = (device.dwType == RIM_TYPEMOUSE ? "Mouse " : "Keyboard ") + std::to_string(handle);
			if (uniqueId.empty())
				uniqueId = (device.dwType == RIM_TYPEMOUSE ? "raw_mouse_" : "raw_keyboard_") + std::to_string(handle);
			NOTICE_LOG(INPUT, "Found RawInput %s name \"%s\" id %s", device.dwType == RIM_TYPEMOUSE ? "mouse" : "keyboard", name.c_str(), uniqueId.c_str());

			if (device.dwType == RIM_TYPEMOUSE)
			{
				auto ptr = std::make_shared<RawMouse>(mice.size() >= 4 ? 3 : mice.size(), name, uniqueId, device.hDevice);
				mice[device.hDevice] = ptr;
				GamepadDevice::Register(ptr);
			}
			else
			{
				auto ptr = std::make_shared<RawKeyboard>(keyboards.size() >= 4 ? 3 : keyboards.size(), name, uniqueId, device.hDevice);
				keyboards[device.hDevice] = ptr;
				GamepadDevice::Register(ptr);
			}
		}
	}
	delete [] deviceList;
}

void init()
{
	createWindow();
	verify(hWnd != NULL);
	findDevices();

	RAWINPUTDEVICE rid[2];
	rid[0].dwFlags = 0;
	rid[0].hwndTarget = hWnd;
	rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
	rid[1].dwFlags = 0;
	rid[1].hwndTarget = hWnd;
	rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
}

void term()
{
	RAWINPUTDEVICE rid[2];
	rid[0].dwFlags = RIDEV_REMOVE;
	rid[0].hwndTarget = nullptr;
	rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
	rid[1].dwFlags = RIDEV_REMOVE;
	rid[1].hwndTarget = nullptr;
	rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));

	destroyWindow();
	for (auto& mouse : mice)
		GamepadDevice::Unregister(mouse.second);
	mice.clear();
	for (auto& keyboard : keyboards)
		GamepadDevice::Unregister(keyboard.second);
	keyboards.clear();
}

}
#endif
