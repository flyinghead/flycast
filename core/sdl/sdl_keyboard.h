#pragma once
#include "input/keyboard_device.h"
#include "sdl.h"

class SDLKeyboardDevice : public KeyboardDeviceTemplate<SDL_Keycode>
{
public:
	SDLKeyboardDevice(int maple_port) : KeyboardDeviceTemplate(maple_port)
	{
		//04-1D Letter keys A-Z (in alphabetic order)
		kb_map[SDLK_a] = 0x04;
		kb_map[SDLK_b] = 0x05;
		kb_map[SDLK_c] = 0x06;
		kb_map[SDLK_d] = 0x07;
		kb_map[SDLK_e] = 0x08;
		kb_map[SDLK_f] = 0x09;
		kb_map[SDLK_g] = 0x0A;
		kb_map[SDLK_h] = 0x0B;
		kb_map[SDLK_i] = 0x0C;
		kb_map[SDLK_j] = 0x0D;
		kb_map[SDLK_k] = 0x0E;
		kb_map[SDLK_l] = 0x0F;
		kb_map[SDLK_m] = 0x10;
		kb_map[SDLK_n] = 0x11;
		kb_map[SDLK_o] = 0x12;
		kb_map[SDLK_p] = 0x13;
		kb_map[SDLK_q] = 0x14;
		kb_map[SDLK_r] = 0x15;
		kb_map[SDLK_s] = 0x16;
		kb_map[SDLK_t] = 0x17;
		kb_map[SDLK_u] = 0x18;
		kb_map[SDLK_v] = 0x19;
		kb_map[SDLK_w] = 0x1A;
		kb_map[SDLK_x] = 0x1B;
		kb_map[SDLK_y] = 0x1C;
		kb_map[SDLK_z] = 0x1D;

		//1E-27 Number keys 1-0
		kb_map[SDLK_1] = 0x1E;
		kb_map[SDLK_2] = 0x1F;
		kb_map[SDLK_3] = 0x20;
		kb_map[SDLK_4] = 0x21;
		kb_map[SDLK_5] = 0x22;
		kb_map[SDLK_6] = 0x23;
		kb_map[SDLK_7] = 0x24;
		kb_map[SDLK_8] = 0x25;
		kb_map[SDLK_9] = 0x26;
		kb_map[SDLK_0] = 0x27;

		kb_map[SDLK_RETURN] = 0x28;
		kb_map[SDLK_ESCAPE] = 0x29;
		kb_map[SDLK_BACKSPACE] = 0x2A;
		kb_map[SDLK_TAB] = 0x2B;
		kb_map[SDLK_SPACE] = 0x2C;

		kb_map[SDLK_MINUS] = 0x2D;	// -
		kb_map[SDLK_EQUALS] = 0x2E;	// =
		kb_map[SDLK_LEFTBRACKET] = 0x2F;	// [
		kb_map[SDLK_RIGHTBRACKET] = 0x30;	// ]

		kb_map[SDLK_BACKSLASH] = 0x31;	// \ (US) unsure of keycode

		//32-34 "]", ";" and ":" (the 3 keys right of L)
		kb_map[SDLK_ASTERISK] = 0x32;	// ~ (non-US) *,µ in FR layout
		kb_map[SDLK_SEMICOLON] = 0x33;	// ;
		kb_map[SDLK_QUOTE] = 0x34;	// '

		//35 hankaku/zenkaku / kanji (top left)
		kb_map[SDLK_BACKQUOTE] = 0x35;	// `~ (US)

		//36-38 ",", "." and "/" (the 3 keys right of M)
		kb_map[SDLK_COMMA] = 0x36;
		kb_map[SDLK_PERIOD] = 0x37;
		kb_map[SDLK_SLASH] = 0x38;

		// CAPSLOCK
		kb_map[SDLK_CAPSLOCK] = 0x39;

		//3A-45 Function keys F1-F12
		for (int i = 0;i < 10; i++)
			kb_map[SDLK_F1 + i] = 0x3A + i;
		kb_map[SDLK_F11] = 0x44;
		kb_map[SDLK_F12] = 0x45;

		//46-4E Control keys above cursor keys
		kb_map[SDLK_PRINTSCREEN] = 0x46;		// Print Screen
		kb_map[SDLK_SCROLLLOCK] = 0x47;		// Scroll Lock
		kb_map[SDLK_PAUSE] = 0x48;		// Pause
		kb_map[SDLK_INSERT] = 0x49;
		kb_map[SDLK_HOME] = 0x4A;
		kb_map[SDLK_PAGEUP] = 0x4B;
		kb_map[SDLK_DELETE] = 0x4C;
		kb_map[SDLK_END] = 0x4D;
		kb_map[SDLK_PAGEDOWN] = 0x4E;

		//4F-52 Cursor keys
		kb_map[SDLK_RIGHT] = 0x4F;
		kb_map[SDLK_LEFT] = 0x50;
		kb_map[SDLK_DOWN] = 0x51;
		kb_map[SDLK_UP] = 0x52;

		//53 Num Lock (Numeric keypad)
		kb_map[SDLK_NUMLOCKCLEAR] = 0x53;
		//54 "/" (Numeric keypad)
		kb_map[SDLK_KP_DIVIDE] = 0x54;
		//55 "*" (Numeric keypad)
		kb_map[SDLK_KP_MULTIPLY] = 0x55;
		//56 "-" (Numeric keypad)
		kb_map[SDLK_KP_MINUS] = 0x56;
		//57 "+" (Numeric keypad)
		kb_map[SDLK_KP_PLUS] = 0x57;
		//58 Enter (Numeric keypad)
		kb_map[SDLK_KP_ENTER] = 0x58;
		//59-62 Number keys 1-0 (Numeric keypad)
		kb_map[SDLK_KP_1] = 0x59;
		kb_map[SDLK_KP_2] = 0x5A;
		kb_map[SDLK_KP_3] = 0x5B;
		kb_map[SDLK_KP_4] = 0x5C;
		kb_map[SDLK_KP_5] = 0x5D;
		kb_map[SDLK_KP_6] = 0x5E;
		kb_map[SDLK_KP_7] = 0x5F;
		kb_map[SDLK_KP_8] = 0x60;
		kb_map[SDLK_KP_9] = 0x61;
		kb_map[SDLK_KP_0] = 0x62;
		//63 "." (Numeric keypad)
		kb_map[SDLK_KP_PERIOD] = 0x63;
		//64 #| (non-US)
		//kb_map[94] = 0x64;
		//65 S3 key
		//66-A4 Not used
		//A5-DF Reserved
		//E0 Left Control
		//E1 Left Shift
		//E2 Left Alt
		//E3 Left S1
		//E4 Right Control
		//E5 Right Shift
		//E6 Right Alt
		//E7 Right S3
		//E8-FF Reserved
	}
	virtual ~SDLKeyboardDevice() {}
	virtual const char* name() override { return "SDL Keyboard"; }

protected:
	virtual u8 convert_keycode(SDL_Keycode keycode) override
	{
		return kb_map[keycode];
	}

private:
	std::map<SDL_Keycode, u8> kb_map;
};
