#include "input/gamepad_device.h"
#include "input/mouse.h"
#include "rend/gui.h"

#include <windows.h>
#include <xinput.h>

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
		set_button(DC_AXIS_LT, XINPUT_GAMEPAD_LEFT_SHOULDER);
		set_button(DC_AXIS_RT, XINPUT_GAMEPAD_RIGHT_SHOULDER);
		set_button(EMU_BTN_MENU, XINPUT_GAMEPAD_BACK);
		set_axis(DC_AXIS_LT, 0, false);
		set_axis(DC_AXIS_RT, 1, false);
		set_axis(DC_AXIS_LEFT, 2, false);
		set_axis(DC_AXIS_RIGHT, 2, true);
		set_axis(DC_AXIS_UP, 3, true);
		set_axis(DC_AXIS_DOWN, 3, false);
		set_axis(DC_AXIS2_LEFT, 4, false);
		set_axis(DC_AXIS2_RIGHT, 4, true);
		set_axis(DC_AXIS2_UP, 5, true);
		set_axis(DC_AXIS2_DOWN, 5, false);
		dirty = false;
	}
};

class XInputGamepadDevice : public GamepadDevice
{
public:
	XInputGamepadDevice(int maple_port, int xinput_port)
	: GamepadDevice(maple_port, "xinput"), _xinput_port(xinput_port)
	{
		char buf[32];
		sprintf(buf, "xinput-%d", xinput_port + 1);
		_unique_id = buf;
	}

	virtual std::shared_ptr<InputMapping> getDefaultMapping() override {
		return std::make_shared<XInputMapping>();
	}

	void ReadInput()
	{
		update_rumble();

		XINPUT_STATE state;

		if (XInputGetState(_xinput_port, &state) == 0)
		{
			if (!input_mapper)
				Open();
			u32 xbutton = state.Gamepad.wButtons;
			u32 changes = xbutton ^ last_buttons_state;

			for (int i = 0; i < 16; i++)
				if ((changes & (1 << i)) != 0)
					gamepad_btn_input(1 << i, (xbutton & (1 << i)) != 0);
			last_buttons_state = xbutton;

			if (state.Gamepad.bLeftTrigger != last_left_trigger)
			{
				gamepad_axis_input(0, (state.Gamepad.bLeftTrigger << 7) + (state.Gamepad.bLeftTrigger >> 1));
				last_left_trigger = state.Gamepad.bLeftTrigger;
			}
			if (state.Gamepad.bRightTrigger != last_right_trigger)
			{
				gamepad_axis_input(1, (state.Gamepad.bRightTrigger << 7) + (state.Gamepad.bRightTrigger >> 1));
				last_right_trigger = state.Gamepad.bRightTrigger;
			}
			if (state.Gamepad.sThumbLX != last_left_thumb_x)
			{
				gamepad_axis_input(2, state.Gamepad.sThumbLX);
				last_left_thumb_x = state.Gamepad.sThumbLX;
			}
			if (state.Gamepad.sThumbLY != last_left_thumb_y)
			{
				gamepad_axis_input(3, state.Gamepad.sThumbLY);
				last_left_thumb_y = state.Gamepad.sThumbLY;
			}
			if (state.Gamepad.sThumbRX != last_right_thumb_x)
			{
				gamepad_axis_input(4, state.Gamepad.sThumbRX);
				last_right_thumb_x = state.Gamepad.sThumbRX;
			}
			if (state.Gamepad.sThumbRY != last_right_thumb_y)
			{
				gamepad_axis_input(5, state.Gamepad.sThumbRY);
				last_right_thumb_y = state.Gamepad.sThumbRY;
			}
		}
		else if (input_mapper)
		{
			INFO_LOG(INPUT, "xinput: Controller '%s' on port %d disconnected", _name.c_str(), _xinput_port);
			GamepadDevice::Unregister(xinput_gamepads[_xinput_port]);
			input_mapper.reset();
			last_buttons_state = 0;
			last_left_trigger = 0;
			last_right_trigger = 0;
			last_left_thumb_x = 0;
			last_left_thumb_y = 0;
			last_right_thumb_x = 0;
			last_right_thumb_y = 0;
		}
	}
	void rumble(float power, float inclination, u32 duration_ms) override
	{
		vib_inclination = inclination * power;
		vib_stop_time = os_GetSeconds() + duration_ms / 1000.0;

		do_rumble(power);
	}
	void update_rumble() override
	{
		if (vib_stop_time > 0)
		{
			int rem_time = (int)((vib_stop_time - os_GetSeconds()) * 1000.0);
			if (rem_time <= 0)
			{
				vib_stop_time = 0;
				do_rumble(0);
			}
			else if (vib_inclination > 0)
				do_rumble(vib_inclination * rem_time);
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
		INFO_LOG(INPUT, "xinput: Opened controller '%s' on port %d", _name.c_str(), _xinput_port);
		loadMapping();

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
	void load_axis_min_max(u32 axis)
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
	void do_rumble(float power)
	{
		XINPUT_VIBRATION vib;

		vib.wLeftMotorSpeed = (u16)(65535 * power);
		vib.wRightMotorSpeed = (u16)(65535 * power);

		XInputSetState(_xinput_port, &vib);
	}

	const int _xinput_port;
	u32 last_buttons_state = 0;
	u8 last_left_trigger = 0;
	u8 last_right_trigger = 0;
	s16 last_left_thumb_x = 0;
	s16 last_left_thumb_y = 0;
	s16 last_right_thumb_x = 0;
	s16 last_right_thumb_y = 0;
	double vib_stop_time;
	float vib_inclination;
	std::map<u32, int> axis_min_values;
	std::map<u32, unsigned int> axis_ranges;
	static std::vector<std::shared_ptr<XInputGamepadDevice>> xinput_gamepads;
};

std::vector<std::shared_ptr<XInputGamepadDevice>> XInputGamepadDevice::xinput_gamepads(XUSER_MAX_COUNT);

class WinMouse : public Mouse
{
public:
	WinMouse() : Mouse("win32")
	{
		_unique_id = "win_mouse";
		loadMapping();
	}
};

