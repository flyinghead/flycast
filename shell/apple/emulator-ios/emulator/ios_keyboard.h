//
//  ios_keyboard.h
//  flycast
//
//  Created by Cameron Bates on 9/6/21.
//
#pragma once
#import <GameController/GameController.h>
#include "input/keyboard_device.h"

class API_AVAILABLE(ios(14.0)) IOSKeyboard : public KeyboardDeviceTemplate<UInt16>
{
public:
    IOSKeyboard(int port, GCKeyboard *keyboard) : KeyboardDeviceTemplate(port, "iOS", false), gcKeyboard(keyboard)
    {
        set_maple_port(port);
        loadMapping();

        kb_map[GCKeyCodeKeyA] = 0x04;
        kb_map[GCKeyCodeKeyB] = 0x05;
        kb_map[GCKeyCodeKeyC] = 0x06;
        kb_map[GCKeyCodeKeyD] = 0x07;
        kb_map[GCKeyCodeKeyE] = 0x08;
        kb_map[GCKeyCodeKeyF] = 0x09;
        kb_map[GCKeyCodeKeyG] = 0x0A;
        kb_map[GCKeyCodeKeyH] = 0x0B;
        kb_map[GCKeyCodeKeyI] = 0x0C;
        kb_map[GCKeyCodeKeyJ] = 0x0D;
        kb_map[GCKeyCodeKeyK] = 0x0E;
        kb_map[GCKeyCodeKeyL] = 0x0F;
        kb_map[GCKeyCodeKeyM] = 0x10;
        kb_map[GCKeyCodeKeyN] = 0x11;
        kb_map[GCKeyCodeKeyO] = 0x12;
        kb_map[GCKeyCodeKeyP] = 0x13;
        kb_map[GCKeyCodeKeyQ] = 0x14;
        kb_map[GCKeyCodeKeyR] = 0x15;
        kb_map[GCKeyCodeKeyS] = 0x16;
        kb_map[GCKeyCodeKeyT] = 0x17;
        kb_map[GCKeyCodeKeyU] = 0x18;
        kb_map[GCKeyCodeKeyV] = 0x19;
        kb_map[GCKeyCodeKeyW] = 0x1A;
        kb_map[GCKeyCodeKeyX] = 0x1B;
        kb_map[GCKeyCodeKeyY] = 0x1C;
        kb_map[GCKeyCodeKeyZ] = 0x1D;
        
        // Number keys 1-0
        kb_map[GCKeyCodeOne] = 0x1E;
        kb_map[GCKeyCodeTwo] = 0x1F;
        kb_map[GCKeyCodeThree] = 0x20;
        kb_map[GCKeyCodeFour] = 0x21;
        kb_map[GCKeyCodeFive] = 0x22;
        kb_map[GCKeyCodeSix] = 0x23;
        kb_map[GCKeyCodeSeven] = 0x24;
        kb_map[GCKeyCodeEight] = 0x25;
        kb_map[GCKeyCodeNine] = 0x26;
        kb_map[GCKeyCodeZero] = 0x27;

        kb_map[GCKeyCodeReturnOrEnter] = 0x28;
        kb_map[GCKeyCodeEscape] = 0x29;
        kb_map[GCKeyCodeDeleteOrBackspace] = 0x2A;
        kb_map[GCKeyCodeTab] = 0x2B;
        kb_map[GCKeyCodeSpacebar] = 0x2C;

        kb_map[GCKeyCodeHyphen] = 0x2D;      // -
        kb_map[GCKeyCodeEqualSign] = 0x2E;     // =
        kb_map[GCKeyCodeOpenBracket] = 0x2F;        // [
        kb_map[GCKeyCodeCloseBracket] = 0x30;       // ]
        kb_map[GCKeyCodeBackslash] = 0x31;  // "\"

        // "]", ";" and ":" (the 3 keys right of L)
        // kb_map[?] = 0x32;   // ~ (non-US) *,µ in FR layout
        kb_map[GCKeyCodeSemicolon] = 0x33;  // ;
        kb_map[GCKeyCodeQuote] = 0x34;      // '
        kb_map[GCKeyCodeGraveAccentAndTilde] = 0x35;  // `~ (US)

        // ",", "." and "/" (the 3 keys right of M)
        kb_map[GCKeyCodeComma] = 0x36;
        kb_map[GCKeyCodePeriod] = 0x37;
        kb_map[GCKeyCodeSlash] = 0x38;

        // Capslock
        kb_map[GCKeyCodeCapsLock] = 0x39;

        // Function keys F1-F12
        kb_map[GCKeyCodeF1] = 0x3A;
        kb_map[GCKeyCodeF2] = 0x3B;
        kb_map[GCKeyCodeF3] = 0x3C;
        kb_map[GCKeyCodeF4] = 0x3D;
        kb_map[GCKeyCodeF5] = 0x3E;
        kb_map[GCKeyCodeF6] = 0x3F;
        kb_map[GCKeyCodeF7] = 0x40;
        kb_map[GCKeyCodeF8] = 0x41;
        kb_map[GCKeyCodeF9] = 0x42;
        kb_map[GCKeyCodeF10] = 0x43;
        kb_map[GCKeyCodeF11] = 0x44;
        kb_map[GCKeyCodeF12] = 0x45;

        // Control keys above cursor keys
        kb_map[GCKeyCodePrintScreen] = 0x46;         // Print Screen
        kb_map[GCKeyCodeScrollLock] = 0x47;         // Scroll Lock
        kb_map[GCKeyCodePause] = 0x48;         // Pause
        kb_map[GCKeyCodeInsert] = 0x49;        // Insert
        kb_map[GCKeyCodeHome] = 0x4A;
        kb_map[GCKeyCodePageUp] = 0x4B;
        kb_map[GCKeyCodeDeleteForward] = 0x4C;
        kb_map[GCKeyCodeEnd] = 0x4D;
        kb_map[GCKeyCodePageDown] = 0x4E;

        // Cursor keys
        kb_map[GCKeyCodeRightArrow] = 0x4F;
        kb_map[GCKeyCodeLeftArrow] = 0x50;
        kb_map[GCKeyCodeDownArrow] = 0x51;
        kb_map[GCKeyCodeUpArrow] = 0x52;

        // Keypad
        kb_map[GCKeyCodeKeypadNumLock] = 0x53; // Num Lock
        kb_map[GCKeyCodeKeypadSlash] = 0x54; // "/"
        kb_map[GCKeyCodeKeypadAsterisk] = 0x55; // "*"
        kb_map[GCKeyCodeKeypadHyphen] = 0x56; // "-"
        kb_map[GCKeyCodeKeypadPlus] = 0x57; // "+"
        kb_map[GCKeyCodeKeypadEnter] = 0x58; // Enter
        kb_map[GCKeyCodeKeypad1] = 0x59;
        kb_map[GCKeyCodeKeypad2] = 0x5A;
        kb_map[GCKeyCodeKeypad3] = 0x5B;
        kb_map[GCKeyCodeKeypad4] = 0x5C;
        kb_map[GCKeyCodeKeypad5] = 0x5D;
        kb_map[GCKeyCodeKeypad6] = 0x5E;
        kb_map[GCKeyCodeKeypad7] = 0x5F;
        kb_map[GCKeyCodeKeypad8] = 0x60;
        kb_map[GCKeyCodeKeypad9] = 0x61;
        kb_map[GCKeyCodeKeypad0] = 0x62;
        kb_map[GCKeyCodeKeypadPeriod] = 0x63; // "."

        //64 #| (non-US)
        //kb_map[94] = 0x64;
        //65 S3 key
        //66-A4 Not used
        //A5-DF Reserved

        kb_map[GCKeyCodeLeftControl] = 0xE0;
        kb_map[GCKeyCodeLeftShift] = 0xE1;
        kb_map[GCKeyCodeLeftAlt] = 0xE2;    // Left Alt
        kb_map[GCKeyCodeLeftGUI] = 0xE3; // Left Command/Meta
        kb_map[GCKeyCodeRightControl] = 0xE4;
        kb_map[GCKeyCodeRightShift] = 0xE5;
        kb_map[GCKeyCodeRightAlt] = 0xE6;    // Right Alt
        kb_map[GCKeyCodeRightGUI] = 0xE7;    // Right Command/Meta
        
        // International keys
        kb_map[GCKeyCodeInternational1] = 0x87;
        kb_map[GCKeyCodeInternational2] = 0x88;
        kb_map[GCKeyCodeInternational3] = 0x89; // Yen
        kb_map[GCKeyCodeInternational4] = 0x8A;
        kb_map[GCKeyCodeInternational5] = 0x8B;
        kb_map[GCKeyCodeInternational6] = 0x8C;
        kb_map[GCKeyCodeInternational7] = 0x8D;
        kb_map[GCKeyCodeInternational8] = 0x8E;
        kb_map[GCKeyCodeInternational9] = 0x8F;
        
        // Language keys
        kb_map[GCKeyCodeLANG1] = 0x90; // Hangul
        kb_map[GCKeyCodeLANG2] = 0x91; // Hanja
        kb_map[GCKeyCodeLANG3] = 0x92; // Katakana
        kb_map[GCKeyCodeLANG4] = 0x93; // Hiragana
        kb_map[GCKeyCodeLANG5] = 0x94; // Zekaku/Hankaku
        kb_map[GCKeyCodeLANG6] = 0x95;
        kb_map[GCKeyCodeLANG7] = 0x96;
        kb_map[GCKeyCodeLANG8] = 0x97;
        kb_map[GCKeyCodeLANG9] = 0x98;

        [gcKeyboard.keyboardInput setKeyChangedHandler:^(GCKeyboardInput *keyboard, GCDeviceButtonInput *key, GCKeyCode keyCode, BOOL pressed) {
            if (pressed) {
                if (keyCode == GCKeyCodeLeftAlt || keyCode == GCKeyCodeLeftGUI) {
                    GCKeyCode otherModifierCode = keyCode == GCKeyCodeLeftAlt ? GCKeyCodeLeftGUI : GCKeyCodeLeftAlt;
                
                    GCControllerButtonInput *otherModifier = [keyboard buttonForKeyCode:otherModifierCode];
                
                    if (otherModifier.isPressed && !gui_is_open()) {
                        gui_open_settings();
                    }
                }
            }

            keyboard_input(keyCode, pressed);
        }];
    }
    
    void set_maple_port(int port) override
    {
        KeyboardDevice::set_maple_port(port);
    }
    
    static void addKeyboard(GCKeyboard *keyboard)
    {
        if (keyboards.count(keyboard) > 0)
            return;

        int port = std::min((int)keyboards.size(), 3);
        keyboards[keyboard] = std::make_shared<IOSKeyboard>(port, keyboard);
        KeyboardDevice::Register(keyboards[keyboard]);
    }

    static void removeKeyboard(GCKeyboard *keyboard)
    {
        auto it = keyboards.find(keyboard);
        if (it == keyboards.end())
            return;
        KeyboardDevice::Unregister(it->second);
        keyboards.erase(it);
    }

protected:
    u8 convert_keycode(UInt16 keycode) override
    {
        return kb_map[keycode];
    }

private:
    GCKeyboard * __weak gcKeyboard = nullptr;
    static std::map<GCKeyboard *, std::shared_ptr<IOSKeyboard>> keyboards;
    std::map<UInt16, u8> kb_map;
};
