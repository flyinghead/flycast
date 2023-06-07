#pragma once
#include "input/keyboard_device.h"

#include <windows.h>

// Used to differentiate between main enter key and num keypad one
#define VK_NUMPAD_RETURN 0x0E

class Win32KeyboardDevice : public KeyboardDevice
{
public:
	Win32KeyboardDevice(int maple_port) : KeyboardDevice(maple_port, "win32")
	{
		kb_map['A'] = 0x04;
		kb_map['B'] = 0x05;
		kb_map['C'] = 0x06;
		kb_map['D'] = 0x07;
		kb_map['E'] = 0x08;
		kb_map['F'] = 0x09;
		kb_map['G'] = 0x0A;
		kb_map['H'] = 0x0B;
		kb_map['I'] = 0x0C;
		kb_map['J'] = 0x0D;
		kb_map['K'] = 0x0E;
		kb_map['L'] = 0x0F;
		kb_map['M'] = 0x10;
		kb_map['N'] = 0x11;
		kb_map['O'] = 0x12;
		kb_map['P'] = 0x13;
		kb_map['Q'] = 0x14;
		kb_map['R'] = 0x15;
		kb_map['S'] = 0x16;
		kb_map['T'] = 0x17;
		kb_map['U'] = 0x18;
		kb_map['V'] = 0x19;
		kb_map['W'] = 0x1A;
		kb_map['X'] = 0x1B;
		kb_map['Y'] = 0x1C;
		kb_map['Z'] = 0x1D;

		//1E-27 Number keys 1-0
		kb_map['1'] = 0x1E;
		kb_map['2'] = 0x1F;
		kb_map['3'] = 0x20;
		kb_map['4'] = 0x21;
		kb_map['5'] = 0x22;
		kb_map['6'] = 0x23;
		kb_map['7'] = 0x24;
		kb_map['8'] = 0x25;
		kb_map['9'] = 0x26;
		kb_map['0'] = 0x27;

		kb_map[VK_RETURN] = 0x28;
		kb_map[VK_ESCAPE] = 0x29;
		kb_map[VK_BACK] = 0x2A;
		kb_map[VK_TAB] = 0x2B;
		kb_map[VK_SPACE] = 0x2C;

		kb_map[VK_OEM_MINUS] = 0x2D;	// -
		kb_map[VK_OEM_PLUS] = 0x2E;	// =
		kb_map[VK_OEM_4] = 0x2F;	// [
		kb_map[VK_OEM_6] = 0x30;	// ]

		kb_map[VK_OEM_5] = 0x31;	// \ (US) unsure of keycode

		//32-34 "]", ";" and ":" (the 3 keys right of L)
		kb_map[VK_OEM_8] = 0x32;	// ~ (non-US) *,µ in FR layout
		kb_map[VK_OEM_1] = 0x33;	// ;
		kb_map[VK_OEM_7] = 0x34;	// '

		//35 hankaku/zenkaku / kanji (top left)
		kb_map[VK_OEM_3] = 0x35;	// `~ (US)

		//36-38 ",", "." and "/" (the 3 keys right of M)
		kb_map[VK_OEM_COMMA] = 0x36;
		kb_map[VK_OEM_PERIOD] = 0x37;
		kb_map[VK_OEM_2] = 0x38;

		// CAPSLOCK
		kb_map[VK_CAPITAL] = 0x39;

		//3A-45 Function keys F1-F12
		for (int i = 0;i < 12; i++)
			kb_map[VK_F1 + i] = 0x3A + i;

		//46-4E Control keys above cursor keys
		kb_map[VK_SNAPSHOT] = 0x46;		// Print Screen
		kb_map[VK_SCROLL] = 0x47;		// Scroll Lock
		kb_map[VK_PAUSE] = 0x48;		// Pause
		kb_map[VK_INSERT] = 0x49;
		kb_map[VK_HOME] = 0x4A;
		kb_map[VK_PRIOR] = 0x4B;
		kb_map[VK_DELETE] = 0x4C;
		kb_map[VK_END] = 0x4D;
		kb_map[VK_NEXT] = 0x4E;

		//4F-52 Cursor keys
		kb_map[VK_RIGHT] = 0x4F;
		kb_map[VK_LEFT] = 0x50;
		kb_map[VK_DOWN] = 0x51;
		kb_map[VK_UP] = 0x52;

		//53 Num Lock (Numeric keypad)
		kb_map[VK_NUMLOCK] = 0x53;
		//54 "/" (Numeric keypad)
		kb_map[VK_DIVIDE] = 0x54;
		//55 "*" (Numeric keypad)
		kb_map[VK_MULTIPLY] = 0x55;
		//56 "-" (Numeric keypad)
		kb_map[VK_SUBTRACT] = 0x56;
		//57 "+" (Numeric keypad)
		kb_map[VK_ADD] = 0x57;
		//58 Enter (Numeric keypad)
		kb_map[VK_NUMPAD_RETURN] = 0x58;
		//59-62 Number keys 1-0 (Numeric keypad)
		kb_map[VK_NUMPAD1] = 0x59;
		kb_map[VK_NUMPAD2] = 0x5A;
		kb_map[VK_NUMPAD3] = 0x5B;
		kb_map[VK_NUMPAD4] = 0x5C;
		kb_map[VK_NUMPAD5] = 0x5D;
		kb_map[VK_NUMPAD6] = 0x5E;
		kb_map[VK_NUMPAD7] = 0x5F;
		kb_map[VK_NUMPAD8] = 0x60;
		kb_map[VK_NUMPAD9] = 0x61;
		kb_map[VK_NUMPAD0] = 0x62;
		//63 "." (Numeric keypad)
		kb_map[VK_DECIMAL] = 0x63;
		//64 #| (non-US)
		//kb_map[94] = 0x64;
		//65 S3 key
		//66-A4 Not used
		//A5-DF Reserved
		kb_map[VK_CONTROL] = 0xE0;	// Left Control
		kb_map[VK_SHIFT] = 0xE1;	// Left Shift
		//E2 Left Alt
		//E3 Left S1
		//E4 Right Control
		//E5 Right Shift
		//E6 Right Alt
		//E7 Right S3
		//E8-FF Reserved

	}

	void input(u8 keycode, bool pressed) {
		KeyboardDevice::input(kb_map[keycode], pressed, 0);
	}

private:
	std::map<u8, u8> kb_map;
};
