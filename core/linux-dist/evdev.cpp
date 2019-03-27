#if defined(USE_EVDEV)

#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#ifdef USE_UDEV
#include <libudev.h>
#endif
#include "evdev.h"
#include "evdev_gamepad.h"

#define EVDEV_DEVICE_STRING "/dev/input/event%d"

static int maple_port = 0;

static void input_evdev_add_device(const char *devnode)
{
	int fd = open(devnode, O_RDWR);
	if (fd >= 0)
	{
		std::shared_ptr<EvdevGamepadDevice> gamepad = std::make_shared<EvdevGamepadDevice>(maple_port, devnode, fd);
		if (maple_port < 3)
			maple_port++;
		EvdevGamepadDevice::AddDevice(gamepad);
	}
}

static void input_evdev_remove_device(const char *devnode)
{
	std::shared_ptr<EvdevGamepadDevice> gamepad = EvdevGamepadDevice::GetControllerForDevnode(devnode);
	if (gamepad != NULL)
	{
		maple_port = gamepad->maple_port();	// Reuse the maple port for the next device connected
		EvdevGamepadDevice::RemoveDevice(gamepad);
	}
}

#ifdef USE_UDEV
static struct udev* udev;
static struct udev_monitor* udev_monitor;

static bool is_joystick(struct udev_device *udev_device)
{
	const char* devnode = udev_device_get_devnode(udev_device);
	if (devnode == NULL || strncmp("/dev/input/event", devnode, 16))
		return false;

	if (udev_device_get_property_value(udev_device, "ID_INPUT_JOYSTICK"))
		return true;
	if (udev_device_get_property_value(udev_device, "ID_INPUT_ACCELEROMETER")
		|| udev_device_get_property_value(udev_device, "ID_INPUT_KEY")
		|| udev_device_get_property_value(udev_device, "ID_INPUT_KEYBOARD")
		|| udev_device_get_property_value(udev_device, "ID_INPUT_MOUSE")
		|| udev_device_get_property_value(udev_device, "ID_INPUT_TABLET")
		|| udev_device_get_property_value(udev_device, "ID_INPUT_TOUCHPAD")
		|| udev_device_get_property_value(udev_device, "ID_INPUT_TOUCHSCREEN"))
		return false;

	// On some platforms (older udev), ID_INPUT_ properties are not present, instead
	// the system makes use of the ID_CLASS property to identify the device class
	const char* id_class = udev_device_get_property_value(udev_device, "ID_CLASS");
	if (id_class == NULL)
		return false;
	if (strstr(id_class, "joystick") != NULL)
		return true;
	if (strstr(id_class, "accelerometer") != NULL
			|| strstr(id_class, "key") != NULL
			|| strstr(id_class, "keyboard") != NULL
			|| strstr(id_class, "mouse") != NULL
			|| strstr(id_class, "tablet") != NULL
			|| strstr(id_class, "touchpad") != NULL
			|| strstr(id_class, "touchscreen") != NULL)
		return false;

	return false;	// Not sure here. Could it still be a joystick after all?
}

static void get_udev_events()
{
	if (udev == NULL)
	{
		udev = udev_new();
		// Create the monitor before doing the enumeration
		udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
		if (udev_monitor == NULL)
		{
			perror("Controller hot-plugging disabled");
		}
		else
		{
			udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "input", NULL);
			udev_monitor_enable_receiving(udev_monitor);
		}
		// Enumerate all joystick devices
		struct udev_enumerate* enumerator = udev_enumerate_new(udev);
		if (udev_enumerate_add_match_subsystem(enumerator, "input") != 0) {
			perror("udev_enumerate_add_match_subsystem");
			udev_monitor_unref(udev_monitor);
			udev_unref(udev);
			udev = NULL;
			return;
		}
		if (udev_enumerate_scan_devices(enumerator) != 0) {
			perror("udev_enumerate_scan_devices");
			udev_monitor_unref(udev_monitor);
			udev_enumerate_unref(enumerator);
			udev_unref(udev);
			udev = NULL;
			return;
		}
		udev_list_entry* devices = udev_enumerate_get_list_entry(enumerator);
		udev_list_entry* device;
		udev_list_entry_foreach(device, devices) {
			const char* syspath = udev_list_entry_get_name(device);
			udev_device* udev_device = udev_device_new_from_syspath(udev, syspath);
			if (udev_device != NULL)
			{
				if (is_joystick(udev_device))
				{
					const char* devnode = udev_device_get_devnode(udev_device);
					input_evdev_add_device(devnode);
				}
				udev_device_unref(udev_device);
			}
		}
		udev_enumerate_unref(enumerator);
	}
	if (udev_monitor != NULL)
	{
		int monitor_fd = udev_monitor_get_fd(udev_monitor);
		fd_set set;
		FD_ZERO(&set);
		FD_SET(monitor_fd, &set);
		timeval timeout = {0, 0};

		if (select(monitor_fd + 1, &set, NULL, NULL, &timeout) > 0 && FD_ISSET(monitor_fd, &set))
		{
			// event detected
			udev_device* udev_device = udev_monitor_receive_device(udev_monitor);
			if (udev_device != NULL)
			{
				if (is_joystick(udev_device))
				{
					const char* devnode = udev_device_get_devnode(udev_device);
					const char* action = udev_device_get_action(udev_device);
					if (action != NULL && devnode != NULL)
					{
						if (strstr(action, "add") != NULL)
						{
							//printf("udev monitor: device added %s\n", devnode);
							input_evdev_add_device(devnode);
						}
						else if (strstr(action, "remove") != NULL)
						{
							//printf("udev monitor: device removed %s\n", devnode);
							input_evdev_remove_device(devnode);
						}
					}
				}
				udev_device_unref(udev_device);
			}
		}
	}
}

static void udev_term()
{
	if (udev_monitor != NULL)
	{
		udev_monitor_unref(udev_monitor);
		udev_monitor = NULL;
	}
	if (udev != NULL)
	{
		udev_unref(udev);
		udev = NULL;
	}
}
#endif	// USE_UDEV

void input_evdev_init()
{
	maple_port = 0;
#ifdef USE_UDEV
	get_udev_events();
#else
	char buf[32];
	for (int port = 0; port < 100; port++)
	{
		sprintf(buf, EVDEV_DEVICE_STRING, port);
		input_evdev_add_device(buf);
	}
#endif
}

void input_evdev_close()
{
#ifdef USE_UDEV
	udev_term();
#endif
	EvdevGamepadDevice::CloseDevices();
}

// FIXME this shouldn't be done by port. Need something like: handle_events() then get_port(0), get_port(2), ...
bool input_evdev_handle(u32 port)
{
#ifdef USE_UDEV
	get_udev_events();
#endif
	EvdevGamepadDevice::PollDevices();
	return true;
}

#endif	// USE_EVDEV

