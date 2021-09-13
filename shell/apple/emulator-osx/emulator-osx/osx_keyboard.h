//
//  osx_keyboard.h
//  reicast-osx
//
//  Created by flyinghead on 26/02/2019.
//  Copyright © 2019 reicast. All rights reserved.
//
#pragma once
#include "input/keyboard_device.h"
#include "oslib/oslib.h"

// Rumbling Taptic Engine by Private MultitouchSupport.framework
extern "C" {
typedef void *MTDeviceRef;
bool MTDeviceIsAvailable(void);
MTDeviceRef MTDeviceCreateDefault(void);
OSStatus MTDeviceGetDeviceID(MTDeviceRef, uint64_t*) __attribute__ ((weak_import));
CFTypeRef MTActuatorCreateFromDeviceID(UInt64 deviceID);
IOReturn MTActuatorOpen(CFTypeRef actuatorRef);
IOReturn MTActuatorClose(CFTypeRef actuatorRef);
IOReturn MTActuatorActuate(CFTypeRef actuatorRef, SInt32 actuationID, UInt32 unknown1, Float32 unknown2, Float32 unknown3);
bool MTActuatorIsOpen(CFTypeRef actuatorRef);
}

class OSXKeyboard : public KeyboardDeviceTemplate<UInt16>
{
public:
	OSXKeyboard(int maple_port) : KeyboardDeviceTemplate(maple_port, "OSX")
	{
		_name = "Keyboard";
		_unique_id = "osx_keyboard";
		loadMapping();

		//04-1D Letter keys A-Z (in alphabetic order)
		kb_map[kVK_ANSI_A] = 0x04;
		kb_map[kVK_ANSI_B] = 0x05;
		kb_map[kVK_ANSI_C] = 0x06;
		kb_map[kVK_ANSI_D] = 0x07;
		kb_map[kVK_ANSI_E] = 0x08;
		kb_map[kVK_ANSI_F] = 0x09;
		kb_map[kVK_ANSI_G] = 0x0A;
		kb_map[kVK_ANSI_H] = 0x0B;
		kb_map[kVK_ANSI_I] = 0x0C;
		kb_map[kVK_ANSI_J] = 0x0D;
		kb_map[kVK_ANSI_K] = 0x0E;
		kb_map[kVK_ANSI_L] = 0x0F;
		kb_map[kVK_ANSI_M] = 0x10;
		kb_map[kVK_ANSI_N] = 0x11;
		kb_map[kVK_ANSI_O] = 0x12;
		kb_map[kVK_ANSI_P] = 0x13;
		kb_map[kVK_ANSI_Q] = 0x14;
		kb_map[kVK_ANSI_R] = 0x15;
		kb_map[kVK_ANSI_S] = 0x16;
		kb_map[kVK_ANSI_T] = 0x17;
		kb_map[kVK_ANSI_U] = 0x18;
		kb_map[kVK_ANSI_V] = 0x19;
		kb_map[kVK_ANSI_W] = 0x1A;
		kb_map[kVK_ANSI_X] = 0x1B;
		kb_map[kVK_ANSI_Y] = 0x1C;
		kb_map[kVK_ANSI_Z] = 0x1D;
		
		//1E-27 Number keys 1-0
		kb_map[kVK_ANSI_1] = 0x1E;
		kb_map[kVK_ANSI_2] = 0x1F;
		kb_map[kVK_ANSI_3] = 0x20;
		kb_map[kVK_ANSI_4] = 0x21;
		kb_map[kVK_ANSI_5] = 0x22;
		kb_map[kVK_ANSI_6] = 0x23;
		kb_map[kVK_ANSI_7] = 0x24;
		kb_map[kVK_ANSI_8] = 0x25;
		kb_map[kVK_ANSI_9] = 0x26;
		kb_map[kVK_ANSI_0] = 0x27;
		
		kb_map[kVK_Return] = 0x28;
		kb_map[kVK_Escape] = 0x29;
		kb_map[kVK_Delete] = 0x2A;
		kb_map[kVK_Tab] = 0x2B;
		kb_map[kVK_Space] = 0x2C;
		
		kb_map[kVK_ANSI_Minus] = 0x2D;      // -
		kb_map[kVK_ANSI_Equal] = 0x2E;     // =
		kb_map[kVK_ANSI_LeftBracket] = 0x2F;        // [
		kb_map[kVK_ANSI_RightBracket] = 0x30;       // ]
		
		kb_map[kVK_ANSI_Backslash] = 0x31;  // \ (US) unsure of keycode
		
		//32-34 "]", ";" and ":" (the 3 keys right of L)
		//kb_map[?] = 0x32;   // ~ (non-US) *,µ in FR layout
		kb_map[kVK_ANSI_Semicolon] = 0x33;  // ;
		kb_map[kVK_ANSI_Quote] = 0x34;      // '
		
		//35 hankaku/zenkaku / kanji (top left)
		kb_map[kVK_ANSI_Grave] = 0x35;  // `~ (US)
		
		//36-38 ",", "." and "/" (the 3 keys right of M)
		kb_map[kVK_ANSI_Comma] = 0x36;
		kb_map[kVK_ANSI_Period] = 0x37;
		kb_map[kVK_ANSI_Slash] = 0x38;
		
		// CAPSLOCK
		kb_map[kVK_CapsLock] = 0x39;
		
		//3A-45 Function keys F1-F12
		kb_map[kVK_F1] = 0x3A;
		kb_map[kVK_F2] = 0x3B;
		kb_map[kVK_F3] = 0x3C;
		kb_map[kVK_F4] = 0x3D;
		kb_map[kVK_F5] = 0x3E;
		kb_map[kVK_F6] = 0x3F;
		kb_map[kVK_F7] = 0x40;
		kb_map[kVK_F8] = 0x41;
		kb_map[kVK_F9] = 0x42;
		kb_map[kVK_F10] = 0x43;
		kb_map[kVK_F11] = 0x44;
		kb_map[kVK_F12] = 0x45;
		
		//46-4E Control keys above cursor keys
		kb_map[kVK_F13] = 0x46;         // Print Screen
		kb_map[kVK_F14] = 0x47;         // Scroll Lock
		kb_map[kVK_F15] = 0x48;         // Pause
		kb_map[kVK_Help] = 0x49;		// Insert
		kb_map[kVK_Home] = 0x4A;
		kb_map[kVK_PageUp] = 0x4B;
		kb_map[kVK_ForwardDelete] = 0x4C;
		kb_map[kVK_End] = 0x4D;
		kb_map[kVK_PageDown] = 0x4E;
		
		//4F-52 Cursor keys
		kb_map[kVK_RightArrow] = 0x4F;
		kb_map[kVK_LeftArrow] = 0x50;
		kb_map[kVK_DownArrow] = 0x51;
		kb_map[kVK_UpArrow] = 0x52;
		
		//53 Num Lock (Numeric keypad)
		kb_map[kVK_ANSI_KeypadClear] = 0x53;
		//54 "/" (Numeric keypad)
		kb_map[kVK_ANSI_KeypadDivide] = 0x54;
		//55 "*" (Numeric keypad)
		kb_map[kVK_ANSI_KeypadMultiply] = 0x55;
		//56 "-" (Numeric keypad)
		kb_map[kVK_ANSI_KeypadMinus] = 0x56;
		//57 "+" (Numeric keypad)
		kb_map[kVK_ANSI_KeypadPlus] = 0x57;
		//58 Enter (Numeric keypad)
		kb_map[kVK_ANSI_KeypadEnter] = 0x58;
		//59-62 Number keys 1-0 (Numeric keypad)
		kb_map[kVK_ANSI_Keypad1] = 0x59;
		kb_map[kVK_ANSI_Keypad2] = 0x5A;
		kb_map[kVK_ANSI_Keypad3] = 0x5B;
		kb_map[kVK_ANSI_Keypad4] = 0x5C;
		kb_map[kVK_ANSI_Keypad5] = 0x5D;
		kb_map[kVK_ANSI_Keypad6] = 0x5E;
		kb_map[kVK_ANSI_Keypad7] = 0x5F;
		kb_map[kVK_ANSI_Keypad8] = 0x60;
		kb_map[kVK_ANSI_Keypad9] = 0x61;
		kb_map[kVK_ANSI_Keypad0] = 0x62;
		//63 "." (Numeric keypad)
		kb_map[kVK_ANSI_KeypadDecimal] = 0x63;
		//64 #| (non-US)
		//kb_map[94] = 0x64;
		//65 S3 key
		//66-A4 Not used
		//A5-DF Reserved
		kb_map[kVK_Control] = 0xE0;
		kb_map[kVK_Shift] = 0xE1;
		kb_map[kVK_Option] = 0xE2;	// Left Alt
		//E3 Left S1
		kb_map[kVK_RightControl] = 0xE4;
		kb_map[kVK_RightShift] = 0xE5;
		kb_map[kVK_RightOption] = 0xE6;	// Right Alt
		//E7 Right S3
		//E8-FF Reserved

		kb_map[kVK_ISO_Section] = 0x32;	// #, Tilde

		// Japanese keyboards
		kb_map[kVK_JIS_Underscore] = 0x87;	// I18n keyboard 1
		kb_map[kVK_JIS_Yen] = 0x89;     	// I18n keyboard 3
	}
	
	void rumble(float power, float inclination, u32 duration_ms) override
	{
		NOTICE_LOG(INPUT, "rumble %.1f inc %f duration %d", power, inclination, duration_ms);

		uint64_t deviceID;
		if ( MTDeviceIsAvailable() && MTDeviceGetDeviceID(MTDeviceCreateDefault(), &deviceID) == 0 )
		{
			if ( power == 0 && vib_timer )
			{
				dispatch_source_cancel(vib_timer);
				return;
			}

			__block CFTypeRef actuatorRef = MTActuatorCreateFromDeviceID(deviceID);
			if (!actuatorRef) return;
			MTActuatorOpen(actuatorRef);

			__block double vib_stop_time = os_GetSeconds() + duration_ms / 1000.0;
			vib_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0));
			// Vibration interval: Power 1.0 = 10ms, Power 0.1 = 100ms
			dispatch_source_set_timer(vib_timer, DISPATCH_TIME_NOW, 10 + (1.0-power)*100 * NSEC_PER_MSEC, 0);

			dispatch_source_set_event_handler(vib_timer, ^{
				if ( vib_stop_time - os_GetSeconds() < 0 )
				{
					dispatch_source_cancel(vib_timer);
					return;
				}
				MTActuatorActuate(actuatorRef, 6, 0, 0.0, 0.0);
			});

			dispatch_source_set_cancel_handler(vib_timer, ^{
				MTActuatorClose(actuatorRef);
			});

			dispatch_resume(vib_timer);
		}
	}

protected:
	u8 convert_keycode(UInt16 keycode) override
	{
		return kb_map[keycode];
	}
	
private:
	std::map<UInt16, u8> kb_map;
	dispatch_source_t vib_timer;
};

