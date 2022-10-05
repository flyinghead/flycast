/*
	Copyright 2022 flyinghead

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
#include "input/keyboard_device.h"

const u8 AndroidKeycodes[] {
        /** Unknown key code. */
        0,
        /** Soft Left key.
         * Usually situated below the display on phones and used as a multi-function
         * feature key for selecting a software defined function shown on the bottom left
         * of the display. */
        0,
        /** Soft Right key.
         * Usually situated below the display on phones and used as a multi-function
         * feature key for selecting a software defined function shown on the bottom right
         * of the display. */
        0,
        /** Home key.
         * This key is handled by the framework and is never delivered to applications. */
        0,
        /** Back key. */
        0,
        /** Call key. */
        0,
        /** End Call key. */
        0,
        /** '0' key. */
        39,
        /** '1' key. */
        30,
        /** '2' key. */
        31,
        /** '3' key. */
        32,
        /** '4' key. */
        33,
        /** '5' key. */
        34,
        /** '6' key. */
        35,
        /** '7' key. */
        36,
        /** '8' key. */
        37,
        /** '9' key. */
        38,
        /** '*' key. */
        85, // Keypad *
        /** '#' key. */
        0,
        /** Directional Pad Up key.
         * May also be synthesized from trackball motions. */
        82,
        /** Directional Pad Down key.
         * May also be synthesized from trackball motions. */
        81,
        /** Directional Pad Left key.
         * May also be synthesized from trackball motions. */
        80,
        /** Directional Pad Right key.
         * May also be synthesized from trackball motions. */
        79,
        /** Directional Pad Center key.
         * May also be synthesized from trackball motions. */
        0,
        /** Volume Up key.
         * Adjusts the speaker volume up. */
        0,
        /** Volume Down key.
         * Adjusts the speaker volume down. */
        0,
        /** Power key. */
        0,
        /** Camera key.
         * Used to launch a camera application or take pictures. */
        0,
        /** Clear key. */
        0,
        /** 'A' key. */
        4,
        /** 'B' key. */
        5,
        /** 'C' key. */
        6,
        /** 'D' key. */
        7,
        /** 'E' key. */
        8,
        /** 'F' key. */
        9,
        /** 'G' key. */
        10,
        /** 'H' key. */
        11,
        /** 'I' key. */
        12,
        /** 'J' key. */
        13,
        /** 'K' key. */
        14,
        /** 'L' key. */
        15,
        /** 'M' key. */
        16,
        /** 'N' key. */
        17,
        /** 'O' key. */
        18,
        /** 'P' key. */
        19,
        /** 'Q' key. */
        20,
        /** 'R' key. */
        21,
        /** 'S' key. */
        22,
        /** 'T' key. */
        23,
        /** 'U' key. */
        24,
        /** 'V' key. */
        25,
        /** 'W' key. */
        26,
        /** 'X' key. */
        27,
        /** 'Y' key. */
        28,
        /** 'Z' key. */
        29,
        /** ',' key. */
        54,
        /** '.' key. */
        55,
        /** Left Alt modifier key. */
        226,
        /** Right Alt modifier key. */
        230,
        /** Left Shift modifier key. */
        225,
        /** Right Shift modifier key. */
        229,
        /** Tab key. */
        43,
        /** Space key. */
        44,
        /** Symbol modifier key.
         * Used to enter alternate symbols. */
        0,
        /** Explorer special function key.
         * Used to launch a browser application. */
        0,
        /** Envelope special function key.
         * Used to launch a mail application. */
        0,
        /** Enter key. */
        40,
        /** Backspace key.
         * Deletes characters before the insertion point, unlike {@link AKEYCODE_FORWARD_DEL}. */
        42,
        /** '`' (backtick) key. */
        53,
        /** '-'. */
        45,
        /** '=' key. */
        46,
        /** '[' key. */
        47,
        /** ']' key. */
        48,
        /** '\' key. */
        49,
        /** ';' key. */
        51,
        /** ''' (apostrophe) key. */
        52,
        /** '/' key. */
        56,
        /** '@' key. */
        0,
        /** Number modifier key.
         * Used to enter numeric symbols.
         * This key is not {@link AKEYCODE_NUM_LOCK}; it is more like {@link AKEYCODE_ALT_LEFT}. */
        83,
        /** Headset Hook key.
         * Used to hang up calls and stop media. */
        0,
        /** Camera Focus key.
         * Used to focus the camera. */
        0,
        /** '+' key. */
        87,	// Keypad +
        /** Menu key. */
        118,
        /** Notification key. */
        0,
        /** Search key. */
        0,
        /** Play/Pause media key. */
        0,
        /** Stop media key. */
        0,
        /** Play Next media key. */
        0,
        /** Play Previous media key. */
        0,
        /** Rewind media key. */
        0,
        /** Fast Forward media key. */
        0,
        /** Mute key.
         * Mutes the microphone, unlike {@link AKEYCODE_VOLUME_MUTE}. */
        0,
        /** Page Up key. */
        75,
        /** Page Down key. */
        78,
        /** Picture Symbols modifier key.
         * Used to switch symbol sets (Emoji, Kao-moji). */
        0,
        /** Switch Charset modifier key.
         * Used to switch character sets (Kanji, Katakana). */
        0,
        /** A Button key.
         * On a game controller, the A button should be either the button labeled A
         * or the first button on the bottom row of controller buttons. */
        0,
        /** B Button key.
         * On a game controller, the B button should be either the button labeled B
         * or the second button on the bottom row of controller buttons. */
        0,
        /** C Button key.
         * On a game controller, the C button should be either the button labeled C
         * or the third button on the bottom row of controller buttons. */
        0,
        /** X Button key.
         * On a game controller, the X button should be either the button labeled X
         * or the first button on the upper row of controller buttons. */
        0,
        /** Y Button key.
         * On a game controller, the Y button should be either the button labeled Y
         * or the second button on the upper row of controller buttons. */
        0,
        /** Z Button key.
         * On a game controller, the Z button should be either the button labeled Z
         * or the third button on the upper row of controller buttons. */
        0,
        /** L1 Button key.
         * On a game controller, the L1 button should be either the button labeled L1 (or L)
         * or the top left trigger button. */
        0,
        /** R1 Button key.
         * On a game controller, the R1 button should be either the button labeled R1 (or R)
         * or the top right trigger button. */
        0,
        /** L2 Button key.
         * On a game controller, the L2 button should be either the button labeled L2
         * or the bottom left trigger button. */
        0,
        /** R2 Button key.
         * On a game controller, the R2 button should be either the button labeled R2
         * or the bottom right trigger button. */
        0,
        /** Left Thumb Button key.
         * On a game controller, the left thumb button indicates that the left (or only)
         * joystick is pressed. */
        0,
        /** Right Thumb Button key.
         * On a game controller, the right thumb button indicates that the right
         * joystick is pressed. */
        0,
        /** Start Button key.
         * On a game controller, the button labeled Start. */
        0,
        /** Select Button key.
         * On a game controller, the button labeled Select. */
        0,
        /** Mode Button key.
         * On a game controller, the button labeled Mode. */
        0,
        /** Escape key. */
        41,
        /** Forward Delete key.
         * Deletes characters ahead of the insertion point, unlike {@link AKEYCODE_DEL}. */
        76,
        /** Left Control modifier key. */
        224,
        /** Right Control modifier key. */
        228,
        /** Caps Lock key. */
        57,
        /** Scroll Lock key. */
        71,
        /** Left Meta modifier key. */
        228,
        /** Right Meta modifier key. */
        231,
        /** Function modifier key. */
        0,
        /** System Request / Print Screen key. */
        154,
        /** Break / Pause key. */
        72,
        /** Home Movement key.
         * Used for scrolling or moving the cursor around to the start of a line
         * or to the top of a list. */
        74,
        /** End Movement key.
         * Used for scrolling or moving the cursor around to the end of a line
         * or to the bottom of a list. */
        77,
        /** Insert key.
         * Toggles insert / overwrite edit mode. */
        73,
        /** Forward key.
         * Navigates forward in the history stack.  Complement of {@link AKEYCODE_BACK}. */
        0,
        /** Play media key. */
        0,
        /** Pause media key. */
        0,
        /** Close media key.
         * May be used to close a CD tray, for example. */
        0,
        /** Eject media key.
         * May be used to eject a CD tray, for example. */
        0,
        /** Record media key. */
        0,
        /** F1 key. */
        58,
        /** F2 key. */
        59,
        /** F3 key. */
        60,
        /** F4 key. */
        61,
        /** F5 key. */
        62,
        /** F6 key. */
        63,
        /** F7 key. */
        64,
        /** F8 key. */
        65,
        /** F9 key. */
        66,
        /** F10 key. */
        67,
        /** F11 key. */
        68,
        /** F12 key. */
        69,
        /** Num Lock key.
         * This is the Num Lock key; it is different from {@link AKEYCODE_NUM}.
         * This key alters the behavior of other keys on the numeric keypad. */
        83,
        /** Numeric keypad '0' key. */
        98,
        /** Numeric keypad '1' key. */
        89,
        /** Numeric keypad '2' key. */
        90,
        /** Numeric keypad '3' key. */
        91,
        /** Numeric keypad '4' key. */
        92,
        /** Numeric keypad '5' key. */
        93,
        /** Numeric keypad '6' key. */
        94,
        /** Numeric keypad '7' key. */
        95,
        /** Numeric keypad '8' key. */
        96,
        /** Numeric keypad '9' key. */
        97,
        /** Numeric keypad '/' key (for division). */
        84,
        /** Numeric keypad '*' key (for multiplication). */
        85,
        /** Numeric keypad '-' key (for subtraction). */
        86,
        /** Numeric keypad '+' key (for addition). */
        87,
        /** Numeric keypad '.' key (for decimals or digit grouping). */
        99, // Keypad period
        /** Numeric keypad ',' key (for decimals or digit grouping). */
        0,
        /** Numeric keypad Enter key. */
        88,
        /** Numeric keypad '=' key. */
        0,
        /** Numeric keypad '(' key. */
        0,
        /** Numeric keypad ')' key. */
        0,
        /** Volume Mute key.
         * Mutes the speaker, unlike {@link AKEYCODE_MUTE}.
         * This key should normally be implemented as a toggle such that the first press
         * mutes the speaker and the second press restores the original volume. */
        0,
        /** Info key.
         * Common on TV remotes to show additional information related to what is
         * currently being viewed. */
        0,
        /** Channel up key.
         * On TV remotes, increments the television channel. */
        0,
        /** Channel down key.
         * On TV remotes, decrements the television channel. */
        0,
        /** Zoom in key. */
        0,
        /** Zoom out key. */
        0,
        /** TV key.
         * On TV remotes, switches to viewing live TV. */
        0,
        /** Window key.
         * On TV remotes, toggles picture-in-picture mode or other windowing functions. */
        0,
        /** Guide key.
         * On TV remotes, shows a programming guide. */
        0,
        /** DVR key.
         * On some TV remotes, switches to a DVR mode for recorded shows. */
        0,
        /** Bookmark key.
         * On some TV remotes, bookmarks content or web pages. */
        0,
        /** Toggle captions key.
         * Switches the mode for closed-captioning text, for example during television shows. */
        0,
        /** Settings key.
         * Starts the system settings activity. */
        0,
        /** TV power key.
         * On TV remotes, toggles the power on a television screen. */
        0,
        /** TV input key.
         * On TV remotes, switches the input on a television screen. */
        0,
        /** Set-top-box power key.
         * On TV remotes, toggles the power on an external Set-top-box. */
        0,
        /** Set-top-box input key.
         * On TV remotes, switches the input mode on an external Set-top-box. */
        0,
        /** A/V Receiver power key.
         * On TV remotes, toggles the power on an external A/V Receiver. */
        0,
        /** A/V Receiver input key.
         * On TV remotes, switches the input mode on an external A/V Receiver. */
        0,
        /** Red "programmable" key.
         * On TV remotes, acts as a contextual/programmable key. */
        0,
        /** Green "programmable" key.
         * On TV remotes, actsas a contextual/programmable key. */
        0,
        /** Yellow "programmable" key.
         * On TV remotes, acts as a contextual/programmable key. */
        0,
        /** Blue "programmable" key.
         * On TV remotes, acts as a contextual/programmable key. */
        0,
        /** App switch key.
         * Should bring up the application switcher dialog. */
        0,
        /** Generic Game Pad Button #1.*/
        0,
        /** Generic Game Pad Button #2.*/
        0,
        /** Generic Game Pad Button #3.*/
        0,
        /** Generic Game Pad Button #4.*/
        0,
        /** Generic Game Pad Button #5.*/
        0,
        /** Generic Game Pad Button #6.*/
        0,
        /** Generic Game Pad Button #7.*/
        0,
        /** Generic Game Pad Button #8.*/
        0,
        /** Generic Game Pad Button #9.*/
        0,
        /** Generic Game Pad Button #10.*/
        0,
        /** Generic Game Pad Button #11.*/
        0,
        /** Generic Game Pad Button #12.*/
        0,
        /** Generic Game Pad Button #13.*/
        0,
        /** Generic Game Pad Button #14.*/
        0,
        /** Generic Game Pad Button #15.*/
        0,
        /** Generic Game Pad Button #16.*/
        0,
        /** Language Switch key.
         * Toggles the current input language such as switching between English and Japanese on
         * a QWERTY keyboard.  On some devices, the same function may be performed by
         * pressing Shift+Spacebar. */
        0,
        /** Manner Mode key.
         * Toggles silent or vibrate mode on and off to make the device behave more politely
         * in certain settings such as on a crowded train.  On some devices, the key may only
         * operate when long-pressed. */
        0,
        /** 3D Mode key.
         * Toggles the display between 2D and 3D mode. */
        0,
        /** Contacts special function key.
         * Used to launch an address book application. */
        0,
        /** Calendar special function key.
         * Used to launch a calendar application. */
        0,
        /** Music special function key.
         * Used to launch a music player application. */
        0,
        /** Calculator special function key.
         * Used to launch a calculator application. */
        0,
        /** Japanese full-width / half-width key. */
        0,
        /** Japanese alphanumeric key. */
        0,
        /** Japanese non-conversion key. */
        0,
        /** Japanese conversion key. */
        0,
        /** Japanese katakana / hiragana key. */
        146,
        /** Japanese Yen key. */
        137,
        /** Japanese Ro key. */
        0,
        /** Japanese kana key. */
        0,
};

class AndroidKeyboard : public KeyboardDeviceTemplate<int>
{
public:
    AndroidKeyboard(int maple_port = 0) : KeyboardDeviceTemplate(maple_port, "Android")
    {
        _unique_id = "android_keyboard";
        if (!find_mapping())
            input_mapper = getDefaultMapping();
    }

protected:
    u8 convert_keycode(int keycode) override
    {
        if (keycode < 0 || keycode >= ARRAY_SIZE(AndroidKeycodes))
            return 0;
        else
            return AndroidKeycodes[keycode];
    }
};
