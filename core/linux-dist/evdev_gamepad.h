#include "../input/gamepad_device.h"
#include "evdev.h"
#include "rend/gui.h"

class EvdevGamepadDevice : public GamepadDevice
{
public:
	EvdevGamepadDevice(int maple_port, const char *devnode, int fd, const char *mapping_file = NULL)
	: GamepadDevice(maple_port, "evdev"), _fd(fd), _rumble_effect_id(-1), _devnode(devnode)
	{
		fcntl(fd, F_SETFL, O_NONBLOCK);
		char name[256] = "Unknown";
		if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
			perror("evdev: ioctl(EVIOCGNAME)");
		else
			printf("evdev: Opened device '%s' ", name);
		_name = name;
		if (!find_mapping(mapping_file))
		{
#if defined(TARGET_PANDORA)
			mapping_file = "controller_pandora.cfg";
#elif defined(TARGET_GCW0)
			mapping_file = "controller_gcwz.cfg";
#else
			if (!strcmp(name, "Microsoft X-Box 360 pad")
				|| !strcmp(name, "Xbox 360 Wireless Receiver")
				|| !strcmp(name, "Xbox 360 Wireless Receiver (XBOX)"))
			{
				mapping_file = "controller_xpad.cfg";
			}
			else if (strstr(name, "Xbox Gamepad (userspace driver)") != NULL)
			{
				mapping_file = "controller_xboxdrv.cfg";
			}
			else if (strstr(name, "keyboard") != NULL ||
					 strstr(name, "Keyboard") != NULL)
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
		auto it = evdev_gamepads.find(_devnode);
		if (it != evdev_gamepads.end())
			delete it->second;
		evdev_gamepads[_devnode] = this;
	}
	virtual ~EvdevGamepadDevice() override
	{
		printf("evdev: Device '%s' on port %d disconnected\n", _name.c_str(), maple_port());
		close(_fd);
		evdev_gamepads.erase(_devnode);
	}

	// FIXME add to base class
	void Rumble(u16 pow_strong, u16 pow_weak)
	{
		printf("RUMBLE: %u / %u (%d)\n", pow_strong, pow_weak, _rumble_effect_id);
		struct ff_effect effect;
		effect.type = FF_RUMBLE;
		effect.id = _rumble_effect_id;
		effect.u.rumble.strong_magnitude = pow_strong;
		effect.u.rumble.weak_magnitude = pow_weak;
		effect.replay.length = 0;
		effect.replay.delay = 0;
		if (ioctl(_fd, EVIOCSFF, &effect) == -1)
		{
			perror("evdev: Force feedback error");
			_rumble_effect_id = -2;
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
				_rumble_effect_id = -2;
			}
		}
	}

	static EvdevGamepadDevice *GetControllerForPort(int port)
	{
		for (auto pair : evdev_gamepads)
			if (pair.second->maple_port() == port)
				return pair.second;
		return NULL;
	}

	static EvdevGamepadDevice *GetControllerForDevnode(const char *devnode)
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
			delete evdev_gamepads.begin()->second;
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
		printf("evdev: range of axis %d is from %d to %d\n", axis, axis_min_values[axis], axis_min_values[axis] + axis_ranges[axis]);
	}

private:
	void read_input()
	{
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

	int _fd;
	std::string _devnode;
	int _rumble_effect_id;
	static std::map<std::string, EvdevGamepadDevice*> evdev_gamepads;
};

std::map<std::string, EvdevGamepadDevice*> EvdevGamepadDevice::evdev_gamepads;
