#include "evdev.h"
#include "input/gamepad_device.h"
#include "oslib/oslib.h"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <climits>

class DefaultEvdevInputMapping : public InputMapping
{
public:
	DefaultEvdevInputMapping() {
		name = "Default";
		set_button(DC_BTN_START, BTN_START);
		set_button(DC_BTN_A, BTN_SOUTH);
		set_button(DC_BTN_B, BTN_EAST);
		set_button(DC_BTN_X, BTN_WEST);
		set_button(DC_BTN_Y, BTN_NORTH);
		set_button(DC_BTN_C, BTN_C);
		set_button(DC_BTN_Z, BTN_Z);
		set_button(DC_DPAD_UP, BTN_DPAD_UP);
		set_button(DC_DPAD_DOWN, BTN_DPAD_DOWN);
		set_button(DC_DPAD_LEFT, BTN_DPAD_LEFT);
		set_button(DC_DPAD_RIGHT, BTN_DPAD_RIGHT);
		set_button(EMU_BTN_MENU, BTN_SELECT);

		set_axis(DC_AXIS_LEFT, ABS_X, false);
		set_axis(DC_AXIS_RIGHT, ABS_X, true);
		set_axis(DC_AXIS_UP, ABS_Y, false);
		set_axis(DC_AXIS_DOWN, ABS_Y, true);
		set_axis(DC_AXIS_LT, ABS_Z, true);
		set_axis(DC_AXIS_RT, ABS_RZ, true);
		set_axis(DC_AXIS2_LEFT, ABS_RX, false);
		set_axis(DC_AXIS2_RIGHT, ABS_RX, true);
		set_axis(DC_AXIS_UP, ABS_RY, false);
		set_axis(DC_AXIS_DOWN, ABS_RY, true);
	}
};

class EvdevGamepadDevice : public GamepadDevice
{
public:
	EvdevGamepadDevice(int maple_port, const char *devnode, int fd, const char *mapping_file = NULL)
	: GamepadDevice(maple_port, "evdev"), _fd(fd), _devnode(devnode), _rumble_effect_id(-1)
	{
		fcntl(fd, F_SETFL, O_NONBLOCK);
		char buf[256] = "Unknown";
		if (ioctl(fd, EVIOCGNAME(sizeof(buf) - 1), buf) < 0)
			perror("evdev: ioctl(EVIOCGNAME)");
		else
			INFO_LOG(INPUT, "evdev: Opened device '%s'", buf);
		_name = buf;
		buf[0] = 0;
		if (ioctl(fd, EVIOCGUNIQ(sizeof(buf) - 1), buf) == 0)
			_unique_id = buf;
		if (_unique_id.empty())
			_unique_id = devnode;

		if (!find_mapping())
		{
#if defined(TARGET_PANDORA)
			mapping_file = "controller_pandora.cfg";
#else
			if (_name == "Microsoft X-Box 360 pad"
				|| _name == "Xbox 360 Wireless Receiver"
				|| _name == "Xbox 360 Wireless Receiver (XBOX)")
			{
				mapping_file = "controller_xpad.cfg";
			}
			else if (_name.find("Xbox Gamepad (userspace driver)") != std::string::npos)
			{
				mapping_file = "controller_xboxdrv.cfg";
			}
			else if (_name.find("keyboard") != std::string::npos
						|| _name.find("Keyboard") != std::string::npos)
			{
				mapping_file = "keyboard.cfg";
			}
			else
			{
				mapping_file = "controller_generic.cfg";
			}
#endif
			if (find_mapping())
			{
				INFO_LOG(INPUT, "using default mapping '%s'", input_mapper->name.c_str());
				input_mapper = std::make_shared<InputMapping>(*input_mapper);
			}
			else
				input_mapper = getDefaultMapping();
			input_mapper->name = _name + " mapping";
			save_mapping();
		}
		else
			INFO_LOG(INPUT, "using custom mapping '%s'", input_mapper->name.c_str());
	}
	~EvdevGamepadDevice() override
	{
		INFO_LOG(INPUT, "evdev: Device '%s' on port %d disconnected", _name.c_str(), maple_port());
		close(_fd);
	}

	std::shared_ptr<InputMapping> getDefaultMapping() override {
		return std::make_shared<DefaultEvdevInputMapping>();
	}

	void rumble(float power, float inclination, u32 duration_ms) override
	{
		vib_inclination = inclination * power;
		vib_stop_time = os_GetSeconds() + duration_ms / 1000.0;

		do_rumble(power, duration_ms);
	}
	void update_rumble() override
	{
		if (vib_inclination > 0)
		{
			int rem_time = (vib_stop_time - os_GetSeconds()) * 1000;
			if (rem_time <= 0)
				vib_inclination = 0;
			else
				do_rumble(vib_inclination * rem_time, rem_time);
		}
	}

	const char *get_button_name(u32 code) override
	{
		switch (code)
		{
		case BTN_START:
			return "Start";
		case BTN_SELECT:
			return "Select";
		case BTN_MODE:
			return "Mode";
		case BTN_NORTH:
			return "North";
		case BTN_SOUTH:
			return "South";
		case BTN_EAST:
			return "East";
		case BTN_WEST:
			return "West";
		case BTN_C:
			return "C";
		case BTN_Z:
			return "Z";
		case BTN_DPAD_UP:
			return "DPad Up";
		case BTN_DPAD_DOWN:
			return "DPad Down";
		case BTN_DPAD_LEFT:
			return "DPad Left";
		case BTN_DPAD_RIGHT:
			return "DPad Right";
		case BTN_TL:
			return "Trigger L";
		case BTN_TR:
			return "Trigger R";
		case BTN_TL2:
			return "Trigger L2";
		case BTN_TR2:
			return "Trigger R2";
		case BTN_THUMBL:
			return "Thumb L";
		case BTN_THUMBR:
			return "Thumb R";
		default:
			return nullptr;
		}
	}
	const char *get_axis_name(u32 code) override
	{
		switch (code)
		{
		case ABS_X:
			return "Abs X";
		case ABS_Y:
			return "Abs Y";
		case ABS_Z:
			return "Abs Z";
		case ABS_RX:
			return "Abs RX";
		case ABS_RY:
			return "Abs RY";
		case ABS_RZ:
			return "Abs RZ";
		default:
			return nullptr;
		}
	}

	static std::shared_ptr<EvdevGamepadDevice> GetControllerForPort(int port)
	{
		for (auto& pair : evdev_gamepads)
			if (pair.second->maple_port() == port)
				return pair.second;
		return NULL;
	}

	static std::shared_ptr<EvdevGamepadDevice> GetControllerForDevnode(const char *devnode)
	{
		auto it = evdev_gamepads.find(devnode);
		if (it == evdev_gamepads.end())
			return NULL;
		return it->second;
	}

	static void PollDevices()
	{
		for (auto& pair : evdev_gamepads)
			pair.second->read_input();
	}

	static void CloseDevices()
	{
		while (!evdev_gamepads.empty())
			RemoveDevice(evdev_gamepads.begin()->second);
	}

	static void AddDevice(std::shared_ptr<EvdevGamepadDevice> gamepad)
	{
		evdev_gamepads[gamepad->_devnode] = gamepad;
		GamepadDevice::Register(gamepad);
	}
	static void RemoveDevice(std::shared_ptr<EvdevGamepadDevice> gamepad)
	{
		evdev_gamepads.erase(gamepad->_devnode);
		GamepadDevice::Unregister(gamepad);
	}

private:
	int get_axis_min_value(u32 axis)
	{
		auto it = axis_min_values.find(axis);
		if (it == axis_min_values.end()) {
			load_axis_min_max(axis);
			it = axis_min_values.find(axis);
			if (it == axis_min_values.end())
				return INT_MIN;
		}
		return it->second;
	}

	unsigned int get_axis_range(u32 axis)
	{
		auto it = axis_ranges.find(axis);
		if (it == axis_ranges.end()) {
			load_axis_min_max(axis);
			it = axis_ranges.find(axis);
			if (it == axis_ranges.end())
				return UINT_MAX;
		}
		return it->second;
	}

	void load_axis_min_max(u32 axis)
	{
		struct input_absinfo abs;
		if (ioctl(_fd, EVIOCGABS(axis), &abs))
		{
			perror("evdev: ioctl(EVIOCGABS)");
			axis_ranges[axis] = 255;
			axis_min_values[axis] = 0;
			return;
		}
		axis_min_values[axis] = abs.minimum;
		axis_ranges[axis] = abs.maximum - abs.minimum;
		DEBUG_LOG(INPUT, "evdev: range of axis %d is from %d to %d", axis, axis_min_values[axis], axis_min_values[axis] + axis_ranges[axis]);
	}

	void read_input()
	{
		update_rumble();
		input_event ie;

		while (read(_fd, &ie, sizeof(ie)) == sizeof(ie))
		{
			switch (ie.type)
			{
				case EV_KEY:
					gamepad_btn_input(ie.code, ie.value != 0);
					break;

				case EV_ABS:
					{
						// TODO no way to distinguish between half and full axes
						int min = get_axis_min_value(ie.code);
						unsigned range = get_axis_range(ie.code);
						gamepad_axis_input(ie.code, (ie.value - min) * 65535 / range - 32768);
					}
					break;
			}
		}

	}
	void do_rumble(float power, u32 duration_ms)
	{
		// Remove previous effect
		if (_rumble_effect_id != -1)
			ioctl(_fd, EVIOCRMFF, _rumble_effect_id);

		// Upload new effect
		struct ff_effect effect;
		effect.type = FF_RUMBLE;
		effect.id = -1;		// Let the driver assign one
		effect.direction = 0;
		effect.replay.length = (u16)duration_ms;
		effect.replay.delay = 0;
		effect.u.rumble.strong_magnitude = (s16)(power * 32767);
		effect.u.rumble.weak_magnitude = (s16)(power * 32767);
		if (ioctl(_fd, EVIOCSFF, &effect) == -1)
		{
			perror("evdev: Force feedback error");
			_rumble_effect_id = -1;
		}
		else
		{
			_rumble_effect_id = effect.id;

			// Let's play the effect
			input_event play;
			play.type = EV_FF;
			play.code = effect.id;
			play.value = 1;
			if (write(_fd, (const void*) &play, sizeof(play)) == -1)
			{
				perror("evdev: Force feedback error");
			}
		}
	}

	int _fd;
	std::string _devnode;
	int _rumble_effect_id = -1;
	float vib_inclination = 0;
	double vib_stop_time = 0;
	std::map<u32, int> axis_min_values;
	std::map<u32, unsigned int> axis_ranges;
	static std::map<std::string, std::shared_ptr<EvdevGamepadDevice>> evdev_gamepads;
};

std::map<std::string, std::shared_ptr<EvdevGamepadDevice>> EvdevGamepadDevice::evdev_gamepads;
