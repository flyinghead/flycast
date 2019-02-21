#include "../input/gamepad_device.h"
#include <Xinput.h>

class XInputMapping : public InputMapping
{
public:
	XInputMapping()
	{
		name = "XInput";
		set_button(DC_BTN_A, XINPUT_GAMEPAD_A);
		set_button(DC_BTN_B, XINPUT_GAMEPAD_B);
		set_button(DC_BTN_X, XINPUT_GAMEPAD_X);
		set_button(DC_BTN_Y, XINPUT_GAMEPAD_Y);
		set_button(DC_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP);
		set_button(DC_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN);
		set_button(DC_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT);
		set_button(DC_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT);
		set_button(DC_BTN_START, XINPUT_GAMEPAD_START);
		set_button(EMU_BTN_TRIGGER_LEFT, XINPUT_GAMEPAD_LEFT_SHOULDER);
		set_button(EMU_BTN_TRIGGER_RIGHT, XINPUT_GAMEPAD_RIGHT_SHOULDER);
		set_button(EMU_BTN_MENU, XINPUT_GAMEPAD_BACK);
		set_axis(DC_AXIS_LT, 0, false);
		set_axis(DC_AXIS_RT, 1, false);
		set_axis(DC_AXIS_X, 2, false);
		set_axis(DC_AXIS_Y, 3, false);
		set_axis(DC_AXIS_X2, 4, false);
		set_axis(DC_AXIS_Y2, 5, false);
		dirty = false;
	}
};

class XInputGamepadDevice : public GamepadDevice
{
public:
	XInputGamepadDevice(int maple_port, int xinput_port)
	: GamepadDevice(maple_port, "xinput"), _xinput_port(xinput_port)
	{
	}

	void ReadInput()
	{
		XINPUT_STATE state;

		if (XInputGetState(_xinput_port, &state) == 0)
		{
			if (input_mapper == NULL)
				Open();
			u32 xbutton = state.Gamepad.wButtons;

			for (int i = 0; i < 16; i++)
			{
				gamepad_btn_input(1 << i, (xbutton & (1 << i)) != 0);
			}
			gamepad_axis_input(0, state.Gamepad.bLeftTrigger);
			gamepad_axis_input(1, state.Gamepad.bRightTrigger);
			gamepad_axis_input(2, state.Gamepad.sThumbLX);
			gamepad_axis_input(3, state.Gamepad.sThumbLY);
			gamepad_axis_input(4, state.Gamepad.sThumbRX);
			gamepad_axis_input(5, state.Gamepad.sThumbRY);
		}
		else if (input_mapper != NULL)
		{
			printf("xinput: Controller '%s' on port %d disconnected\n", _name.c_str(), _xinput_port);
			GamepadDevice::Unregister(xinput_gamepads[_xinput_port]);
			input_mapper = NULL;
		}
	}

	void Open()
	{
		JOYCAPS joycaps;
		int rc = joyGetDevCaps(_xinput_port, &joycaps, sizeof(joycaps));
		if (rc != 0)
			_name = "xinput" + std::to_string(_xinput_port);
		else
			_name = joycaps.szPname;
		printf("xinput: Opened controller '%s' on port %d ", _name.c_str(), _xinput_port);
		if (!find_mapping())
		{
			input_mapper = new XInputMapping();
			input_mapper->name = _name + " mapping";
			save_mapping();
			printf("using default mapping\n");
		}
		else
			printf("using custom mapping '%s'\n", input_mapper->name.c_str());
		GamepadDevice::Register(xinput_gamepads[_xinput_port]);
	}

	static void CreateDevices()
	{
		for (int port = 0; port < XUSER_MAX_COUNT; port++)
			xinput_gamepads[port] = std::make_shared<XInputGamepadDevice>(port, port);
	}
	static void CloseDevices()
	{
		for (int port = 0; port < XUSER_MAX_COUNT; port++)
			GamepadDevice::Unregister(xinput_gamepads[port]);
	}

	static std::shared_ptr<XInputGamepadDevice> GetXInputDevice(int port)
	{
		return xinput_gamepads[port];
	}

protected:
	virtual void load_axis_min_max(u32 axis) override
	{
		if (axis == 0 || axis == 1)
		{
			axis_ranges[axis] = 255;
			axis_min_values[axis] = 0;
		}
		else
		{
			axis_ranges[axis] = 65535;
			axis_min_values[axis] = -32768;
		}
	}

private:

	const int _xinput_port;
	static std::vector<std::shared_ptr<XInputGamepadDevice>> xinput_gamepads;
};

std::vector<std::shared_ptr<XInputGamepadDevice>> XInputGamepadDevice::xinput_gamepads(XUSER_MAX_COUNT);
