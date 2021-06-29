#include "input/gamepad_device.h"
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
		set_button(EMU_BTN_TRIGGER_LEFT, XINPUT_GAMEPAD_LEFT_SHOULDER);
		set_button(EMU_BTN_TRIGGER_RIGHT, XINPUT_GAMEPAD_RIGHT_SHOULDER);
		set_button(EMU_BTN_MENU, XINPUT_GAMEPAD_BACK);
		set_axis(DC_AXIS_LT, 0, false);
		set_axis(DC_AXIS_RT, 1, false);
		set_axis(DC_AXIS_X, 2, false);
		set_axis(DC_AXIS_Y, 3, true);
		set_axis(DC_AXIS_X2, 4, false);
		set_axis(DC_AXIS_Y2, 5, true);
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
				gamepad_axis_input(0, state.Gamepad.bLeftTrigger);
				last_left_trigger = state.Gamepad.bLeftTrigger;
			}
			if (state.Gamepad.bRightTrigger != last_right_trigger)
			{
				gamepad_axis_input(1, state.Gamepad.bRightTrigger);
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
			int rem_time = (vib_stop_time - os_GetSeconds()) * 1000;
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
		if (!find_mapping())
		{
			input_mapper = std::make_shared<XInputMapping>();
			input_mapper->name = _name + " mapping";
			save_mapping();
			INFO_LOG(INPUT, "using default mapping");
		}
		else
			INFO_LOG(INPUT, "using custom mapping '%s'n", input_mapper->name.c_str());
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
	void load_axis_min_max(u32 axis) override
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
	static std::vector<std::shared_ptr<XInputGamepadDevice>> xinput_gamepads;
};

std::vector<std::shared_ptr<XInputGamepadDevice>> XInputGamepadDevice::xinput_gamepads(XUSER_MAX_COUNT);

class KbInputMapping : public InputMapping
{
public:
	KbInputMapping()
	{
		name = "Windows Keyboard";
		set_button(DC_BTN_A, 'X');
		set_button(DC_BTN_B, 'C');
		set_button(DC_BTN_X, 'S');
		set_button(DC_BTN_Y, 'D');
		set_button(DC_DPAD_UP, VK_UP);
		set_button(DC_DPAD_DOWN, VK_DOWN);
		set_button(DC_DPAD_LEFT, VK_LEFT);
		set_button(DC_DPAD_RIGHT, VK_RIGHT);
		set_button(DC_BTN_START, VK_RETURN);
		set_button(EMU_BTN_TRIGGER_LEFT, 'F');
		set_button(EMU_BTN_TRIGGER_RIGHT, 'V');
		set_button(EMU_BTN_MENU, VK_TAB);
		set_button(EMU_BTN_FFORWARD, VK_SPACE);

		dirty = false;
	}
};

class WinKbGamepadDevice : public GamepadDevice
{
public:
	WinKbGamepadDevice(int maple_port) : GamepadDevice(maple_port, "win32")
	{
		_name = "Keyboard";
		_unique_id = "win_keyboard";
		if (!find_mapping())
			input_mapper = std::make_shared<KbInputMapping>();
	}
	~WinKbGamepadDevice() override = default;
};

class MouseInputMapping : public InputMapping
{
public:
	MouseInputMapping()
	{
		name = "Mouse";
		set_button(DC_BTN_A, 0);	// Left
		set_button(DC_BTN_B, 2);	// Right
		set_button(DC_BTN_START, 1);// Middle

		dirty = false;
	}
};

class WinMouseGamepadDevice : public GamepadDevice
{
public:
	WinMouseGamepadDevice(int maple_port) : GamepadDevice(maple_port, "win32")
	{
		_name = "Mouse";
		_unique_id = "win_mouse";
		if (!find_mapping())
			input_mapper = std::make_shared<MouseInputMapping>();
	}
	~WinMouseGamepadDevice() override = default;

	bool gamepad_btn_input(u32 code, bool pressed) override
	{
		if (gui_is_open() && !is_detecting_input())
			// Don't register mouse clicks as gamepad presses when gui is open
			// This makes the gamepad presses to be handled first and the mouse position to be ignored
			// TODO Make this generic
			return false;
		else
			return GamepadDevice::gamepad_btn_input(code, pressed);
	}

	const char *get_button_name(u32 code) override
	{
		switch (code)
		{
		case 0:
			return "Left Button";
		case 2:
			return "Right Button";
		case 1:
			return "Middle Button";
		default:
			return nullptr;
		}
	}
};

