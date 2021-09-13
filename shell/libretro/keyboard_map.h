/*
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
#ifndef _KEYBOARD_MAP_H
#define _KEYBOARD_MAP_H
#include "libretro.h"

static u8 kb_map[RETROK_LAST];

static void init_kb_map()
{
	 //04-1D Letter keys A-Z (in alphabetic order)
	 kb_map[RETROK_a] = 0x04;
	 kb_map[RETROK_b] = 0x05;
	 kb_map[RETROK_c] = 0x06;
	 kb_map[RETROK_d] = 0x07;
	 kb_map[RETROK_e] = 0x08;
	 kb_map[RETROK_f] = 0x09;
	 kb_map[RETROK_g] = 0x0A;
	 kb_map[RETROK_h] = 0x0B;
	 kb_map[RETROK_i] = 0x0C;
	 kb_map[RETROK_j] = 0x0D;
	 kb_map[RETROK_k] = 0x0E;
	 kb_map[RETROK_l] = 0x0F;
	 kb_map[RETROK_m] = 0x10;
	 kb_map[RETROK_n] = 0x11;
	 kb_map[RETROK_o] = 0x12;
	 kb_map[RETROK_p] = 0x13;
	 kb_map[RETROK_q] = 0x14;
	 kb_map[RETROK_r] = 0x15;
	 kb_map[RETROK_s] = 0x16;
	 kb_map[RETROK_t] = 0x17;
	 kb_map[RETROK_u] = 0x18;
	 kb_map[RETROK_v] = 0x19;
	 kb_map[RETROK_w] = 0x1A;
	 kb_map[RETROK_x] = 0x1B;
	 kb_map[RETROK_y] = 0x1C;
	 kb_map[RETROK_z] = 0x1D;

	 //1E-27 Number keys 1-0
	 kb_map[RETROK_1] = 0x1E;	kb_map[RETROK_EXCLAIM] = 0x1E;
	 kb_map[RETROK_2] = 0x1F;	kb_map[RETROK_AT] = 0x1F;
	 kb_map[RETROK_3] = 0x20;	kb_map[RETROK_HASH] = 0x20;
	 kb_map[RETROK_4] = 0x21;	kb_map[RETROK_DOLLAR] = 0x21;
	 kb_map[RETROK_5] = 0x22;	kb_map[37] = 0x22;	// missing RETROK_PERCENT
	 kb_map[RETROK_6] = 0x23;	kb_map[RETROK_CARET] = 0x23;
	 kb_map[RETROK_7] = 0x24;	kb_map[RETROK_AMPERSAND] = 0x24;
	 kb_map[RETROK_8] = 0x25;	kb_map[RETROK_ASTERISK] = 0x25;
	 kb_map[RETROK_9] = 0x26;	kb_map[RETROK_LEFTPAREN] = 0x26;
	 kb_map[RETROK_0] = 0x27;	kb_map[RETROK_RIGHTPAREN] = 0x27;

	 kb_map[RETROK_RETURN] = 0x28;
	 kb_map[RETROK_ESCAPE] = 0x29;
	 kb_map[RETROK_BACKSPACE] = 0x2A;
	 kb_map[RETROK_TAB] = 0x2B;
	 kb_map[RETROK_SPACE] = 0x2C;

	 kb_map[RETROK_MINUS] = 0x2D;			kb_map[RETROK_UNDERSCORE] = 0x2D;	// - _
	 kb_map[RETROK_EQUALS] = 0x2E;			kb_map[RETROK_PLUS] = 0x2E;			// = +
	 kb_map[RETROK_LEFTBRACKET] = 0x2F;		kb_map[RETROK_LEFTBRACE] = 0x2F;	// [ {
	 kb_map[RETROK_RIGHTBRACKET] = 0x30;	kb_map[RETROK_RIGHTBRACE] = 0x30;	// ] }

	 kb_map[RETROK_BACKSLASH] = 0x31;		kb_map[RETROK_BAR] = 0x31;			// \ |

	 //32 (non-US keyboards only)
	 //33-34 ";" and ":" (the 2 keys right of L)
	 kb_map[RETROK_SEMICOLON] = 0x33;		kb_map[RETROK_COLON] = 0x33;		// ; :
	 kb_map[RETROK_QUOTE] = 0x34;			kb_map[RETROK_QUOTEDBL] = 0x34;		// ' "

	 //35 ` ~ (top left)
	 kb_map[RETROK_BACKQUOTE] = 0x35;		kb_map[RETROK_TILDE] = 0x35;		// ` ~

	 //36-38 ",", "." and "/" (the 3 keys right of M)
	 kb_map[RETROK_COMMA] = 0x36;			kb_map[RETROK_LESS] = 0x36;
	 kb_map[RETROK_PERIOD] = 0x37;			kb_map[RETROK_GREATER] = 0x37;
	 kb_map[RETROK_SLASH] = 0x38;			kb_map[RETROK_QUESTION] = 0x38;

	 // CAPSLOCK
	 kb_map[RETROK_CAPSLOCK] = 0x39;

	 //3A-45 Function keys F1-F12
	 for (int i = 0;i < 12; i++)
		 kb_map[RETROK_F1 + i] = 0x3A + i;

	 //46-4E Control keys above cursor keys
	 kb_map[RETROK_PRINT] = 0x46;		// Print Screen
	 kb_map[RETROK_SCROLLOCK] = 0x47;	// Scroll Lock
	 kb_map[RETROK_PAUSE] = 0x48;		// Pause
	 kb_map[RETROK_INSERT] = 0x49;
	 kb_map[RETROK_HOME] = 0x4A;
	 kb_map[RETROK_PAGEUP] = 0x4B;
	 kb_map[RETROK_DELETE] = 0x4C;
	 kb_map[RETROK_END] = 0x4D;
	 kb_map[RETROK_PAGEDOWN] = 0x4E;

	 //4F-52 Cursor keys
	 kb_map[RETROK_RIGHT] = 0x4F;
	 kb_map[RETROK_LEFT] = 0x50;
	 kb_map[RETROK_DOWN] = 0x51;
	 kb_map[RETROK_UP] = 0x52;

	 //53 Num Lock (Numeric keypad)
	 kb_map[RETROK_NUMLOCK] = 0x53;
	 //54 "/" (Numeric keypad)
	 kb_map[RETROK_KP_DIVIDE] = 0x54;
	 //55 "*" (Numeric keypad)
	 kb_map[RETROK_KP_MULTIPLY] = 0x55;
	 //56 "-" (Numeric keypad)
	 kb_map[RETROK_KP_MINUS] = 0x56;
	 //57 "+" (Numeric keypad)
	 kb_map[RETROK_KP_PLUS] = 0x57;
	 //58 Enter (Numeric keypad)
	 kb_map[RETROK_KP_ENTER] = 0x58;
	 //59-62 Number keys 1-0 (Numeric keypad)
	 kb_map[RETROK_KP1] = 0x59;
	 kb_map[RETROK_KP2] = 0x51;
	 kb_map[RETROK_KP3] = 0x5B;
	 kb_map[RETROK_KP4] = 0x50;
	 kb_map[RETROK_KP5] = 0x5D;
	 kb_map[RETROK_KP6] = 0x4F;
	 kb_map[RETROK_KP7] = 0x5F;
	 kb_map[RETROK_KP8] = 0x52;
	 kb_map[RETROK_KP9] = 0x61;
	 kb_map[RETROK_KP0] = 0x62;
	 //63 "." (Numeric keypad)
	 kb_map[RETROK_KP_PERIOD] = 0x63;
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

#endif
