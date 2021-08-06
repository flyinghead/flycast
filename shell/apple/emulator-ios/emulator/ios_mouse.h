#pragma once
#include "input/gamepad_device.h"

class IOSMouse : public SystemMouse
{
public:
	IOSMouse() : SystemMouse("iOS")
	{
		_unique_id = "ios_mouse";
		loadMapping();
	}
};


