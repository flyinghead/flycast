#pragma once
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "x11.h"

class X11KeyboardDevice : public KeyboardDeviceTemplate<int>
{
public:
	X11KeyboardDevice(int maple_port) : KeyboardDeviceTemplate(maple_port)
	{
		//04-1D Letter keys A-Z (in alphabetic order)
		kb_map[KEY_A] = 0x04;
		kb_map[KEY_B] = 0x05;
		kb_map[KEY_C] = 0x06;
		kb_map[KEY_D] = 0x07;
		kb_map[KEY_E] = 0x08;
		kb_map[KEY_F] = 0x09;
		kb_map[KEY_G] = 0x0A;
		kb_map[KEY_H] = 0x0B;
		kb_map[KEY_I] = 0x0C;
		kb_map[KEY_J] = 0x0D;
		kb_map[KEY_K] = 0x0E;
		kb_map[KEY_L] = 0x0F;
		kb_map[KEY_M] = 0x10;
		kb_map[KEY_N] = 0x11;
		kb_map[KEY_O] = 0x12;
		kb_map[KEY_P] = 0x13;
		kb_map[KEY_Q] = 0x14;
		kb_map[KEY_R] = 0x15;
		kb_map[KEY_S] = 0x16;
		kb_map[KEY_T] = 0x17;
		kb_map[KEY_U] = 0x18;
		kb_map[KEY_V] = 0x19;
		kb_map[KEY_W] = 0x1A;
		kb_map[KEY_X] = 0x1B;
		kb_map[KEY_Y] = 0x1C;
		kb_map[KEY_Z] = 0x1D;

		//1E-27 Number keys 1-0
		kb_map[KEY_1] = 0x1E;
		kb_map[KEY_2] = 0x1F;
		kb_map[KEY_3] = 0x20;
		kb_map[KEY_4] = 0x21;
		kb_map[KEY_5] = 0x22;
		kb_map[KEY_6] = 0x23;
		kb_map[KEY_7] = 0x24;
		kb_map[KEY_8] = 0x25;
		kb_map[KEY_9] = 0x26;
		kb_map[KEY_0] = 0x27;

		kb_map[KEY_RETURN] = 0x28;
		kb_map[KEY_ESC] = 0x29;
		kb_map[KEY_BACKSPACE] = 0x2A;
		kb_map[KEY_TAB] = 0x2B;
		kb_map[KEY_SPACE] = 0x2C;

		kb_map[20] = 0x2D;	// -
		kb_map[21] = 0x2E;	// =
		kb_map[34] = 0x2F;	// [
		kb_map[35] = 0x30;	// ]

		kb_map[94] = 0x31;	// \ (US) unsure of keycode

		//32-34 "]", ";" and ":" (the 3 keys right of L)
		kb_map[51] = 0x32;	// ~ (non-US) *,µ in FR layout
		kb_map[47] = 0x33;	// ;
		kb_map[48] = 0x34;	// '

		//35 hankaku/zenkaku / kanji (top left)
		kb_map[49] = 0x35;	// `~ (US)

		//36-38 ",", "." and "/" (the 3 keys right of M)
		kb_map[59] = 0x36;
		kb_map[60] = 0x37;
		kb_map[61] = 0x38;

		// CAPSLOCK
		kb_map[66] = 0x39;

		//3A-45 Function keys F1-F12
		for (int i = 0;i < 10; i++)
			kb_map[KEY_F1 + i] = 0x3A + i;
		kb_map[KEY_F11] = 0x44;
		kb_map[KEY_F12] = 0x45;

		//46-4E Control keys above cursor keys
		kb_map[107] = 0x46;		// Print Screen
		kb_map[78] = 0x47;		// Scroll Lock
		kb_map[127] = 0x48;		// Pause
		kb_map[KEY_INS] = 0x49;
		kb_map[KEY_HOME] = 0x4A;
		kb_map[KEY_PGUP] = 0x4B;
		kb_map[KEY_DEL] = 0x4C;
		kb_map[KEY_END] = 0x4D;
		kb_map[KEY_PGDOWN] = 0x4E;

		//4F-52 Cursor keys
		kb_map[KEY_RIGHT] = 0x4F;
		kb_map[KEY_LEFT] = 0x50;
		kb_map[KEY_DOWN] = 0x51;
		kb_map[KEY_UP] = 0x52;

		//53 Num Lock (Numeric keypad)
		kb_map[77] = 0x53;
		//54 "/" (Numeric keypad)
		kb_map[106] = 0x54;
		//55 "*" (Numeric keypad)
		kb_map[63] = 0x55;
		//56 "-" (Numeric keypad)
		kb_map[82] = 0x56;
		//57 "+" (Numeric keypad)
		kb_map[86] = 0x57;
		//58 Enter (Numeric keypad)
		kb_map[104] = 0x58;
		//59-62 Number keys 1-0 (Numeric keypad)
		kb_map[87] = 0x59;
		kb_map[88] = 0x5A;
		kb_map[89] = 0x5B;
		kb_map[83] = 0x5C;
		kb_map[84] = 0x5D;
		kb_map[85] = 0x5E;
		kb_map[79] = 0x5F;
		kb_map[80] = 0x60;
		kb_map[81] = 0x61;
		kb_map[90] = 0x62;
		//63 "." (Numeric keypad)
		kb_map[91] = 0x63;
		//64 #| (non-US)
		//kb_map[94] = 0x64;
		//65 S3 key
		//66-A4 Not used
		//A5-DF Reserved
		kb_map[KEY_LCTRL] = 0xE0;  // Left Control
		kb_map[KEY_LSHIFT] = 0xE1; // Left Shift
		//E2 Left Alt
		//E3 Left S1
		kb_map[KEY_RCTRL] = 0xE4;  // Right Control
		kb_map[KEY_RSHIFT] = 0xE5; // Right Shift
		//E6 Right Alt
		//E7 Right S3
		//E8-FF Reserved
	}
	virtual const char* name() override { return "X11 Keyboard"; }

protected:
	virtual u8 convert_keycode(int keycode) override
	{
		return kb_map[keycode];
	}

private:
	std::map<int, u8> kb_map;
};

class KbInputMapping : public InputMapping
{
public:
	KbInputMapping()
	{
		name = "X11 Keyboard";
		set_button(DC_BTN_A, KEY_X);
		set_button(DC_BTN_B, KEY_C);
		set_button(DC_BTN_X, KEY_S);
		set_button(DC_BTN_Y, KEY_D);
		set_button(DC_DPAD_UP, KEY_UP);
		set_button(DC_DPAD_DOWN, KEY_DOWN);
		set_button(DC_DPAD_LEFT, KEY_LEFT);
		set_button(DC_DPAD_RIGHT, KEY_RIGHT);
		set_button(DC_BTN_START, KEY_RETURN);
		set_button(EMU_BTN_TRIGGER_LEFT, KEY_F);
		set_button(EMU_BTN_TRIGGER_RIGHT, KEY_V);
		set_button(EMU_BTN_MENU, KEY_TAB);
		set_button(EMU_BTN_FFORWARD, KEY_SPACE);

		dirty = false;
	}
};

class X11KbGamepadDevice : public GamepadDevice
{
public:
	X11KbGamepadDevice(int maple_port) : GamepadDevice(maple_port, "X11")
	{
		_name = "Keyboard";
		_unique_id = "x11_keyboard";
		if (!find_mapping())
			input_mapper = std::make_shared<KbInputMapping>();
	}

	virtual const char *get_button_name(u32 code) override
	{
		switch (code)
		{
		case KEY_A:
			return "A";
		case KEY_B:
			return "B";
		case KEY_C:
			return "C";
		case KEY_D:
			return "D";
		case KEY_E:
			return "E";
		case KEY_F:
			return "F";
		case KEY_G:
			return "G";
		case KEY_H:
			return "H";
		case KEY_I:
			return "I";
		case KEY_J:
			return "J";
		case KEY_K:
			return "K";
		case KEY_L:
			return "L";
		case KEY_M:
			return "M";
		case KEY_N:
			return "N";
		case KEY_O:
			return "O";
		case KEY_P:
			return "P";
		case KEY_Q:
			return "Q";
		case KEY_R:
			return "R";
		case KEY_S:
			return "S";
		case KEY_T:
			return "T";
		case KEY_U:
			return "U";
		case KEY_V:
			return "V";
		case KEY_W:
			return "W";
		case KEY_X:
			return "X";
		case KEY_Y:
			return "Y";
		case KEY_Z:
			return "Z";

		case KEY_0:
			return "0";
		case KEY_1:
			return "1";
		case KEY_2:
			return "2";
		case KEY_3:
			return "3";
		case KEY_4:
			return "4";
		case KEY_5:
			return "5";
		case KEY_6:
			return "6";
		case KEY_7:
			return "7";
		case KEY_8:
			return "8";
		case KEY_9:
			return "9";

		case KEY_BACKSPACE:
			return "Backspace";
		case KEY_DEL:
			return "Delete";
		case KEY_DOWN:
			return "Down";
		case KEY_UP:
			return "Up";
		case KEY_LEFT:
			return "Left";
		case KEY_RIGHT:
			return "Right";
		case KEY_END:
			return "End";
		case KEY_ESC:
			return "Escape";

		case KEY_F1:
			return "F1";
		case KEY_F2:
			return "F2";
		case KEY_F3:
			return "F3";
		case KEY_F4:
			return "F4";
		case KEY_F5:
			return "F5";
		case KEY_F6:
			return "F6";
		case KEY_F7:
			return "F7";
		case KEY_F8:
			return "F8";
		case KEY_F9:
			return "F9";
		case KEY_F10:
			return "F10";
		case KEY_F11:
			return "F11";
		case KEY_F12:
			return "F12";

		case KEY_HOME:
			return "Home";
		case KEY_INS:
			return "Insert";
		case KEY_RETURN:
			return "Return";
		case KEY_LALT:
			return "Left ALT";
		case KEY_LCTRL:
			return "Left CTRL";
		case KEY_LSHIFT:
			return "Left SHIFT";
		case KEY_RCTRL:
			return "Right CTRL";
		case KEY_RSHIFT:
			return "Right Shift";
		case 108:
			return "Right ALT";
		case KEY_PGDOWN:
			return "Page Down";
		case KEY_PGUP:
			return "Page Up";
		case KEY_SPACE:
			return "Space";
		case KEY_TAB:
			return "Tab";

		case 87:
			return "Num 1";
		case 88:
			return "Num 2";
		case 89:
			return "Num 3";
		case 83:
			return "Num 4";
		case 84:
			return "Num 5";
		case 85:
			return "Num 6";
		case 79:
			return "Num 7";
		case 80:
			return "Num 8";
		case 81:
			return "Num 9";
		case 90:
			return "Num 0";
		case 91:
			return "Num .";
		case 106:
			return "Num /";
		case 63:
			return "Num *";
		case 82:
			return "Num -";
		case 86:
			return "Num +";
		case 104:
			return "Num Enter";

		case 20:
			return "-";
		case 21:
			return "=";
		case 34:
			return "[";
		case 35:
			return "]";
		case 47:
			return ";";
		case 48:
			return "'";
		case 49:
			return "`";
		case 51:
			return "\\";
		case 59:
			return ",";
		case 60:
			return ".";
		case 61:
			return "/";
		case 94:
			return "102nd";

		default:
			return nullptr;
		}
	}
};
