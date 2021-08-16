#pragma once
#import <GameController/GameController.h>

#include "input/gamepad_device.h"

enum IOSButton {
	IOS_BTN_A = 1,
	IOS_BTN_B = 2,
	IOS_BTN_X = 3,
	IOS_BTN_Y = 4,
	IOS_BTN_UP = 5,
	IOS_BTN_DOWN = 6,
	IOS_BTN_LEFT = 7,
	IOS_BTN_RIGHT = 8,
	IOS_BTN_MENU = 9,
	IOS_BTN_OPTIONS = 10,
	IOS_BTN_HOME = 11,
	IOS_BTN_L1 = 12,
	IOS_BTN_R1 = 13,
	IOS_BTN_L3 = 14,
	IOS_BTN_R3 = 15,
	IOS_BTN_L2 = 16,
	IOS_BTN_R2 = 17,

	IOS_BTN_MAX
};

class IOSGamePad : public GamepadDevice
{
public:
	IOSGamePad(int port, GCController *controller) : GamepadDevice(port, "iOS"), gcController(controller)
	{
		gcController.playerIndex = (GCControllerPlayerIndex)port;
		if (gcController.vendorName != nullptr)
			_name = [gcController.vendorName UTF8String];
		//_unique_id = ?
		INFO_LOG(INPUT, "iOS: Opened joystick %d: '%s' unique_id=%s", port, _name.c_str(), _unique_id.c_str());
		loadMapping();
	}

	static void addController(GCController *controller)
	{
		if (controllers.count(controller) > 0)
			return;
		if (controller.extendedGamepad == nil && controller.gamepad == nil)
			return;
		int port = std::min((int)controllers.size(), 3);
		controllers[controller] = std::make_shared<IOSGamePad>(port, controller);
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

private:
	GCController *gcController = nullptr;
	static std::map<GCController *, std::shared_ptr<IOSGamePad>> controllers;
};

class IOSMouse : public SystemMouse
{
public:
	IOSMouse() : SystemMouse("iOS")
	{
		_unique_id = "ios_mouse";
		loadMapping();
	}
};
