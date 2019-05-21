#include "../input/gamepad_device.h"
#include "oslib/oslib.h"
#include "evdev.h"

class EvdevGamepadDevice : public GamepadDevice
{
public:
	EvdevGamepadDevice(int maple_port, const char *devnode, int fd, const char *mapping_file = NULL)
	: GamepadDevice(maple_port, "evdev"), _fd(fd), _rumble_effect_id(-1), _devnode(devnode)
	{
		fcntl(fd, F_SETFL, O_NONBLOCK);
		char buf[256] = "Unknown";
		if (ioctl(fd, EVIOCGNAME(sizeof(buf) - 1), buf) < 0)
			perror("evdev: ioctl(EVIOCGNAME)");
		else
			printf("evdev: Opened device '%s' ", buf);
		_name = buf;
		buf[0] = 0;
		if (ioctl(fd, EVIOCGUNIQ(sizeof(buf) - 1), buf) == 0)
			_unique_id = buf;
		if (_unique_id.empty())
			_unique_id = devnode;

		if (!find_mapping(mapping_file))
		{
#if defined(TARGET_PANDORA)
			mapping_file = "controller_pandora.cfg";
#elif defined(TARGET_GCW0)
			mapping_file = "controller_gcwz.cfg";
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
			if (find_mapping(mapping_file))
			{
				printf("using default mapping '%s'\n", input_mapper->name.c_str());
				input_mapper = new InputMapping(*input_mapper);
			}
			else
				input_mapper = new IdentityInputMapping();
			input_mapper->name = _name + " mapping";
			save_mapping();
		}
		else
			printf("using custom mapping '%s'\n", input_mapper->name.c_str());
	}
	virtual ~EvdevGamepadDevice() override
	{
		printf("evdev: Device '%s' on port %d disconnected\n", _name.c_str(), maple_port());
		close(_fd);
	}

	virtual void rumble(float power, float inclination, u32 duration_ms) override
	{
		vib_inclination = inclination * power;
		vib_stop_time = os_GetSeconds() + duration_ms / 1000.0;

		do_rumble(power, duration_ms);
	}
	virtual void update_rumble() override
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

	static std::shared_ptr<EvdevGamepadDevice> GetControllerForPort(int port)
	{
		for (auto pair : evdev_gamepads)
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
		for (auto pair : evdev_gamepads)
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

protected:
	virtual void load_axis_min_max(u32 axis) override
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
		//printf("evdev: range of axis %d is from %d to %d\n", axis, axis_min_values[axis], axis_min_values[axis] + axis_ranges[axis]);
	}

private:
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
					gamepad_axis_input(ie.code, ie.value);
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
	static std::map<std::string, std::shared_ptr<EvdevGamepadDevice>> evdev_gamepads;
};

std::map<std::string, std::shared_ptr<EvdevGamepadDevice>> EvdevGamepadDevice::evdev_gamepads;
