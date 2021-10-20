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
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

#include <cmath>
#include "input/gamepad_device.h"
#include "input/mouse.h"
#include "rend/gui.h"

enum IOSButton {
	IOS_BTN_A = 1,
	IOS_BTN_B,
	IOS_BTN_X,
	IOS_BTN_Y,
	IOS_BTN_UP,
	IOS_BTN_DOWN,
	IOS_BTN_LEFT,
	IOS_BTN_RIGHT,
	IOS_BTN_MENU,		// aka Start
	IOS_BTN_OPTIONS,	// aka Back (xbox), Select (dualshock)
	IOS_BTN_HOME,
	IOS_BTN_L1,
	IOS_BTN_R1,
	IOS_BTN_L2,
	IOS_BTN_R2,
	IOS_BTN_L3,
	IOS_BTN_R3,
	IOS_BTN_SHARE,
	IOS_BTN_PADDLE1,
	IOS_BTN_PADDLE2,
	IOS_BTN_PADDLE3,
	IOS_BTN_PADDLE4,
	IOS_BTN_TOUCHPAD,

	IOS_BTN_MAX
};
enum IOSAxis {
	IOS_AXIS_L1 = 1,
	IOS_AXIS_R1,
	IOS_AXIS_L2,
	IOS_AXIS_R2,
	IOS_AXIS_LX,
	IOS_AXIS_LY,
	IOS_AXIS_RX,
	IOS_AXIS_RY,
};

static NSString *GCInputXboxShareButton = @"Button Share";

template<bool Arcade = false, bool Gamepad = false>
class DefaultIOSMapping : public InputMapping
{
public:
	DefaultIOSMapping()
	{
		name = Arcade ? Gamepad ? "Arcade Gamepad" : "Arcade Hitbox" : "Default";
		if (Arcade)
		{
			if (Gamepad)
			{
				// 1  2  3  4  5  6
				// A  B  X  Y  L  R
				set_button(DC_BTN_A, IOS_BTN_A);
				set_button(DC_BTN_B, IOS_BTN_B);
				set_button(DC_BTN_C, IOS_BTN_X);
				set_button(DC_BTN_X, IOS_BTN_Y);
				set_button(DC_BTN_Y, IOS_BTN_L1);
				set_button(DC_BTN_Z, IOS_BTN_R1);
				set_axis(DC_AXIS_LT, IOS_AXIS_L2, true);
				set_axis(DC_AXIS_RT, IOS_AXIS_R2, true);
			}
			else
			{
				// Hitbox
				// 1  2  3  4  5  6  7  8
				// X  Y  R1 A  B  R2 L1 L2
				set_button(DC_BTN_A, IOS_BTN_X);
				set_button(DC_BTN_B, IOS_BTN_Y);
				set_button(DC_BTN_C, IOS_BTN_R1);
				set_button(DC_BTN_X, IOS_BTN_A);
				set_button(DC_BTN_Y, IOS_BTN_B);
				set_axis(DC_BTN_Z, IOS_AXIS_R2, true);
				set_button(DC_DPAD2_LEFT, IOS_BTN_L1);			// L1 (Naomi button 7)
				set_axis(DC_DPAD2_RIGHT, IOS_AXIS_L2, true);	// L2 (Naomi button 8)
			}
		}
		else
		{
			set_button(DC_BTN_A, IOS_BTN_A);
			set_button(DC_BTN_B, IOS_BTN_B);
			set_button(DC_BTN_X, IOS_BTN_X);
			set_button(DC_BTN_Y, IOS_BTN_Y);
			set_axis(DC_AXIS_RT, IOS_AXIS_R2, true);
			set_button(DC_BTN_C, IOS_BTN_R1);
			set_axis(DC_AXIS_LT, IOS_AXIS_L2, true);
			set_button(DC_BTN_Z, IOS_BTN_L1);
		}
		set_button(DC_DPAD_UP, IOS_BTN_UP);
		set_button(DC_DPAD_DOWN, IOS_BTN_DOWN);
		set_button(DC_DPAD_LEFT, IOS_BTN_LEFT);
		set_button(DC_DPAD_RIGHT, IOS_BTN_RIGHT);
		set_button(DC_BTN_START, IOS_BTN_MENU);
		set_button(EMU_BTN_MENU, IOS_BTN_OPTIONS);

		set_axis(DC_AXIS_LEFT, IOS_AXIS_LX, false);
		set_axis(DC_AXIS_RIGHT, IOS_AXIS_LX, true);
		set_axis(DC_AXIS_UP, IOS_AXIS_LY, false);
		set_axis(DC_AXIS_DOWN, IOS_AXIS_LY, true);
		set_axis(DC_AXIS2_LEFT, IOS_AXIS_RX, false);
		set_axis(DC_AXIS2_RIGHT, IOS_AXIS_RX, true);
		set_axis(DC_AXIS2_UP, IOS_AXIS_RY, false);
		set_axis(DC_AXIS2_DOWN, IOS_AXIS_RY, true);
		dirty = false;
	}
};

class IOSGamepad : public GamepadDevice
{
public:
	IOSGamepad(int port, GCController *controller) : GamepadDevice(port, "iOS"), gcController(controller)
	{
		set_maple_port(port);
		if (gcController.vendorName != nullptr)
			_name = [gcController.vendorName UTF8String];
		else
			_name = "MFi Gamepad";
		//_unique_id = ?
		INFO_LOG(INPUT, "iOS: Opened joystick %d: '%s'", port, _name.c_str());
		loadMapping();
		
		if (gcController.extendedGamepad != nullptr)
		{
			[gcController.extendedGamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_A, pressed);
			}];
			[gcController.extendedGamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_B, pressed);
			}];
			[gcController.extendedGamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_X, pressed);
			}];
			[gcController.extendedGamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_Y, pressed);
			}];
			[gcController.extendedGamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
				gamepad_btn_input(IOS_BTN_RIGHT, dpad.right.isPressed);
				gamepad_btn_input(IOS_BTN_LEFT, dpad.left.isPressed);
				gamepad_btn_input(IOS_BTN_UP, dpad.up.isPressed);
				gamepad_btn_input(IOS_BTN_DOWN, dpad.down.isPressed);
			}];
			if (@available(iOS 12.1, *)) {
				[gcController.extendedGamepad.rightThumbstickButton setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_R3, pressed);
				}];
				[gcController.extendedGamepad.leftThumbstickButton setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_L3, pressed);
				}];
			}
			if (@available(iOS 13.0, *)) {
				[gcController.extendedGamepad.buttonOptions setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_OPTIONS, pressed);
				}];
				[gcController.extendedGamepad.buttonMenu setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_MENU, pressed);
				}];
				[gcController.extendedGamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_L1, pressed);
				}];
				[gcController.extendedGamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_R1, pressed);
				}];
			}
			else
			{
				// Left shoulder for options/menu
				[gcController.extendedGamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_OPTIONS, pressed);
				}];
				// Right shoulder for menu/start
				[gcController.extendedGamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_MENU, pressed);
				}];
			}
			if (@available(iOS 14.0, *)) {
				[gcController.extendedGamepad.buttonHome setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_HOME, pressed);
				}];
			}
			
			[gcController.extendedGamepad.rightTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_R2, pressed);
				gamepad_axis_input(IOS_AXIS_R2, (int)std::roundf(32767.f * value));
			}];
			[gcController.extendedGamepad.leftTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_L2, pressed);
				gamepad_axis_input(IOS_AXIS_L2, (int)std::roundf(32767.f * value));
			}];
			[gcController.extendedGamepad.leftThumbstick.xAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value) {
				gamepad_axis_input(IOS_AXIS_LX, (int)std::roundf(32767.f * value));
			}];
			[gcController.extendedGamepad.leftThumbstick.yAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value) {
				gamepad_axis_input(IOS_AXIS_LY, (int)std::roundf(-32767.f * value));
			}];
			[gcController.extendedGamepad.rightThumbstick.xAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value) {
				gamepad_axis_input(IOS_AXIS_RX, (int)std::roundf(32767.f * value));
			}];
			[gcController.extendedGamepad.rightThumbstick.yAxis setValueChangedHandler:^(GCControllerAxisInput *axis, float value) {
				gamepad_axis_input(IOS_AXIS_RY, (int)std::roundf(-32767.f * value));
			}];

		}
		else
		{
			// Legacy gamepad
			[gcController.gamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_A, pressed);
			}];
			[gcController.gamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_B, pressed);
			}];
			[gcController.gamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_X, pressed);
			}];
			[gcController.gamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				gamepad_btn_input(IOS_BTN_Y, pressed);
			}];
			[gcController.gamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
				gamepad_btn_input(IOS_BTN_RIGHT, dpad.right.isPressed);
				gamepad_btn_input(IOS_BTN_LEFT, dpad.left.isPressed);
				gamepad_btn_input(IOS_BTN_UP, dpad.up.isPressed);
				gamepad_btn_input(IOS_BTN_DOWN, dpad.down.isPressed);
			}];
			[gcController.gamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed) {
					if (gcController != nil && gcController.gamepad.leftShoulder.pressed)
						gamepad_btn_input(IOS_BTN_MENU, true);
					else
						gamepad_btn_input(IOS_BTN_R2, true);
				}
				else {
					gamepad_btn_input(IOS_BTN_R2, false);
					gamepad_btn_input(IOS_BTN_MENU, false);
				}
			}];
			[gcController.gamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
				if (pressed) {
					if (gcController != nil && gcController.gamepad.rightShoulder.pressed)
						gamepad_btn_input(IOS_BTN_MENU, true);
					else
						gamepad_btn_input(IOS_BTN_L2, true);
				}
				else {
					gamepad_btn_input(IOS_BTN_L2, false);
					gamepad_btn_input(IOS_BTN_MENU, false);
				}
			}];
		}
		
		if (@available(iOS 14.0, *)) {
			if (gcController.physicalInputProfile.buttons[GCInputXboxPaddleOne] != nil) {
				[gcController.physicalInputProfile.buttons[GCInputXboxPaddleOne] setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_PADDLE1, pressed);
				}];
			}
			if (gcController.physicalInputProfile.buttons[GCInputXboxPaddleTwo] != nil) {
				[gcController.physicalInputProfile.buttons[GCInputXboxPaddleTwo] setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_PADDLE2, pressed);
				}];
			}
			if (gcController.physicalInputProfile.buttons[GCInputXboxPaddleThree] != nil) {
				[gcController.physicalInputProfile.buttons[GCInputXboxPaddleThree] setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_PADDLE3, pressed);
				}];
			}
			if (gcController.physicalInputProfile.buttons[GCInputXboxPaddleFour] != nil) {
				[gcController.physicalInputProfile.buttons[GCInputXboxPaddleFour] setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_PADDLE4, pressed);
				}];
			}
			if (gcController.physicalInputProfile.buttons[GCInputXboxShareButton] != nil) {
				[gcController.physicalInputProfile.buttons[GCInputXboxShareButton] setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_SHARE, pressed);
				}];
			}
			if (gcController.physicalInputProfile.buttons[GCInputDualShockTouchpadButton] != nil) {
				[gcController.physicalInputProfile.buttons[GCInputDualShockTouchpadButton] setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
					gamepad_btn_input(IOS_BTN_TOUCHPAD, pressed);
				}];
			}
			if (gcController.haptics != nullptr)
			{
				CHHapticEngine *hapticEngine = [gcController.haptics createEngineWithLocality:GCHapticsLocalityDefault];
				NSError *error = nil;
				[hapticEngine startAndReturnError:&error];
				if (error != nullptr)
					NSLog(@"Haptic engine error: \(error)");
				else {
					this->hapticEngine = hapticEngine;
					_rumble_enabled = true;
				}
			}
		}
	}
	
	~IOSGamepad() {
		if (hapticEngine != nullptr)
			[hapticEngine stopWithCompletionHandler:^(NSError * _Nullable error) {}];
	}
	
	void set_maple_port(int port) override
	{
		GamepadDevice::set_maple_port(port);
		if (port <= 3 && gcController != nil)
			gcController.playerIndex = (GCControllerPlayerIndex)port;
	}

	const char *get_button_name(u32 code) override
	{
		switch ((IOSButton)code) {
			case IOS_BTN_A:
				return "A";
			case IOS_BTN_B:
				return "B";
			case IOS_BTN_X:
				return "X";
			case IOS_BTN_Y:
				return "Y";
			case IOS_BTN_UP:
				return "DPad Up";
			case IOS_BTN_DOWN:
				return "DPad Down";
			case IOS_BTN_LEFT:
				return "DPad Left";
			case IOS_BTN_RIGHT:
				return "DPad Right";
			case IOS_BTN_MENU:
				return "Menu";
			case IOS_BTN_OPTIONS:
				return "Options";
			case IOS_BTN_HOME:
				return "Home";
			case IOS_BTN_L1:
				return "L Shoulder";
			case IOS_BTN_R1:
				return "R Shoulder";
			case IOS_BTN_L2:
				return "L Trigger";
			case IOS_BTN_R2:
				return "R Trigger";
			case IOS_BTN_L3:
				return "L Thumbstick";
			case IOS_BTN_R3:
				return "R Thumbstick";
			case IOS_BTN_SHARE:
				return "Share";
			case IOS_BTN_PADDLE1:
				return "Paddle 1";
			case IOS_BTN_PADDLE2:
				return "Paddle 2";
			case IOS_BTN_PADDLE3:
				return "Paddle 3";
			case IOS_BTN_PADDLE4:
				return "Paddle 4";
			case IOS_BTN_TOUCHPAD:
				return "Touchpad";
			default:
				return nullptr;
		}
	}
	const char *get_axis_name(u32 code) override
	{
		switch ((IOSAxis)code) {
			case IOS_AXIS_L1:
				return "L Shoulder";
			case IOS_AXIS_R1:
				return "R Shoulder";
			case IOS_AXIS_L2:
				return "L Trigger";
			case IOS_AXIS_R2:
				return "R Trigger";
			case IOS_AXIS_LX:
				return "L Stick X";
			case IOS_AXIS_LY:
				return "L Stick Y";
			case IOS_AXIS_RX:
				return "R Stick X";
			case IOS_AXIS_RY:
				return "R Stick Y";
			default:
				return nullptr;
		}
	}
	
	std::shared_ptr<InputMapping> getDefaultMapping() override {
		return std::make_shared<DefaultIOSMapping<>>();
	}

	void rumble(float power, float inclination, u32 duration_ms) override
	{
		NOTICE_LOG(INPUT, "rumble %.1f inc %f duration %d", power, inclination, duration_ms);
		if (@available(iOS 13.0, *)) {
			CHHapticEvent *event = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
																 parameters:@[
																	 [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:power]]
															   relativeTime:0.0
																   duration:duration_ms / 1000.0];
			NSError *error = nil;
			CHHapticPattern *pattern = [[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
			if (error != nil)
				return;
			if (hapticPlayer != nil)
				[hapticPlayer stopAtTime:0 error:&error];
			hapticPlayer = [hapticEngine createPlayerWithPattern:pattern error:&error];
			if (error != nil)
				return;
			[hapticPlayer startAtTime:0 error:&error];
		}
	}

	void resetMappingToDefault(bool arcade, bool gamepad) override
	{
		NOTICE_LOG(INPUT, "Resetting iOS gamepad to default: %d %d", arcade, gamepad);
		if (arcade)
		{
			if (gamepad)
				input_mapper = std::make_shared<DefaultIOSMapping<true, true>>();
			else
				input_mapper = std::make_shared<DefaultIOSMapping<true, false>>();
		}
		else
			input_mapper = std::make_shared<DefaultIOSMapping<false, false>>();
	}

	static void addController(GCController *controller)
	{
		if (controllers.count(controller) > 0)
			return;
		if (controller.extendedGamepad == nullptr && controller.gamepad == nullptr)
			return;
		int port = std::min((int)controllers.size(), 3);
		controllers[controller] = std::make_shared<IOSGamepad>(port, controller);
		GamepadDevice::Register(controllers[controller]);
	}

	static void removeController(GCController *controller)
	{
		auto it = controllers.find(controller);
		if (it == controllers.end())
			return;
		GamepadDevice::Unregister(it->second);
		controllers.erase(it);
	}
	
	static bool controllerConnected() {
		return !controllers.empty();
	}

private:
	GCController * __weak gcController = nullptr;
	CHHapticEngine *hapticEngine = nullptr;
	id<CHHapticPatternPlayer> hapticPlayer;
	static std::map<GCController *, std::shared_ptr<IOSGamepad>> controllers;
};

class IOSVirtualGamepad : public GamepadDevice
{
public:
	IOSVirtualGamepad() : GamepadDevice(0, "iOS", false) {
		_name = "Virtual Gamepad";
		_unique_id = "ios-virtual-gamepad";
		input_mapper = getDefaultMapping();
	}

	bool is_virtual_gamepad() override { return true; }

	std::shared_ptr<InputMapping> getDefaultMapping() override {
		return std::make_shared<DefaultIOSMapping<>>();
	}

	bool gamepad_btn_input(u32 code, bool pressed) override
	{
		if (pressed)
			buttonState |= 1 << code;
		else
			buttonState &= ~(1 << code);
		switch (code)
		{
			case IOS_BTN_L2:
				gamepad_axis_input(IOS_AXIS_L2, pressed ? 0x7fff : 0);
				if (settings.platform.system != DC_PLATFORM_DREAMCAST)
					GamepadDevice::gamepad_btn_input(IOS_BTN_L1, pressed);	// Z, btn5
				return true;
			case IOS_BTN_R2:
				if (!pressed && maple_port() >= 0 && maple_port() <= 3)
					kcode[maple_port()] |= DC_DPAD2_UP | DC_BTN_D | DC_DPAD2_DOWN;
				gamepad_axis_input(IOS_AXIS_R2, pressed ? 0x7fff : 0);
				if (settings.platform.system != DC_PLATFORM_DREAMCAST)
					GamepadDevice::gamepad_btn_input(IOS_BTN_Y, pressed);	// Y, btn4
				return true;
			default:
				if ((buttonState & ((1 << IOS_BTN_UP) | (1 << IOS_BTN_DOWN))) == ((1 << IOS_BTN_UP) | (1 << IOS_BTN_DOWN))
					|| (buttonState & ((1 << IOS_BTN_LEFT) | (1 << IOS_BTN_RIGHT))) == ((1 << IOS_BTN_LEFT) | (1 << IOS_BTN_RIGHT)))
				{
					GamepadDevice::gamepad_btn_input(IOS_BTN_UP, false);
					GamepadDevice::gamepad_btn_input(IOS_BTN_DOWN, false);
					GamepadDevice::gamepad_btn_input(IOS_BTN_LEFT, false);
					GamepadDevice::gamepad_btn_input(IOS_BTN_RIGHT, false);
					buttonState = 0;
					gui_open_settings();
					return true;
				}
				if (settings.platform.system != DC_PLATFORM_DREAMCAST && maple_port() >= 0 && maple_port() <= 3)
				{
					u32& keycode = kcode[maple_port()];
					if ((buttonState & (1 << IOS_BTN_R2)) != 0)
					{
						switch (code) {
							case IOS_BTN_A:
								// RT + A -> D (coin)
								keycode = pressed ? keycode & ~DC_BTN_D : keycode | DC_BTN_D;
								break;
							case IOS_BTN_B:
								// RT + B -> Service
								keycode = pressed ? keycode & ~DC_DPAD2_UP : keycode | DC_DPAD2_UP;
								break;
							case IOS_BTN_X:
								// RT + X -> Test
								keycode = pressed ? keycode & ~DC_DPAD2_DOWN : keycode | DC_DPAD2_DOWN;
								break;
							default:
								break;
						}
					}
					// arcade mapping: X -> btn2, Y -> btn3
					if (code == IOS_BTN_X)
						code = IOS_BTN_R1; // C, btn2
					if (code == IOS_BTN_Y)
						code = IOS_BTN_X;  // btn3
				}

				return GamepadDevice::gamepad_btn_input(code, pressed);
		}
	}

private:
	u32 buttonState = 0;
};

class IOSTouchMouse : public SystemMouse
{
public:
    IOSTouchMouse() : SystemMouse("iOS")
    {
        _unique_id = "ios_mouse";
        _name = "Touchscreen (Mouse)";
        loadMapping();
    }
};
