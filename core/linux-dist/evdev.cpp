#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include "linux-dist/evdev.h"
#include "linux-dist/main.h"
#include "cfg/ini.h"
#include <vector>
#include <map>

#if defined(USE_EVDEV)
	std::map<std::string, ControllerMapping> loaded_mappings;

	int load_keycode(ConfigFile* cfg, string section, string dc_key)
	{
		int code;
		string keycode = cfg->get(section, dc_key, "-1");
		if (strstr(keycode.c_str(), "KEY_") != NULL ||
			strstr(keycode.c_str(), "BTN_") != NULL ||
			strstr(keycode.c_str(), "ABS_") != NULL)
		{
			if(evdev_keycodes.count(keycode.c_str()) == 1)
			{
				code = evdev_keycodes[keycode.c_str()];
				printf("%s = %s (%d)\n", dc_key.c_str(), keycode.c_str(), code);
			}
			else
			{
				code = -1;
				printf("evdev: failed to find keycode for '%s'", keycode.c_str());
			}
		}
		else
		{
			code = cfg->get_int(section, dc_key, -1);
			printf("%s = %d\n", dc_key.c_str(), code);
		}
		return code;
	}

	ControllerMapping load_mapping(FILE* fd)
	{
		ConfigFile mf;
		mf.parse(fd);

		ControllerMapping mapping = {
			mf.get("emulator", "mapping_name", "<Unknown>").c_str(),
			load_keycode(&mf, "dreamcast", "btn_a"),
			load_keycode(&mf, "dreamcast", "btn_b"),
			load_keycode(&mf, "dreamcast", "btn_c"),
			load_keycode(&mf, "dreamcast", "btn_d"),
			load_keycode(&mf, "dreamcast", "btn_x"),
			load_keycode(&mf, "dreamcast", "btn_y"),
			load_keycode(&mf, "dreamcast", "btn_z"),
			load_keycode(&mf, "dreamcast", "btn_start"),
			load_keycode(&mf, "emulator",  "btn_escape"),
			load_keycode(&mf, "dreamcast", "dpad_left"),
			load_keycode(&mf, "dreamcast", "dpad_right"),
			load_keycode(&mf, "dreamcast", "dpad_up"),
			load_keycode(&mf, "dreamcast", "dpad_down"),
			load_keycode(&mf, "dreamcast", "dpad2_left"),
			load_keycode(&mf, "dreamcast", "dpad2_right"),
			load_keycode(&mf, "dreamcast", "dpad2_up"),
			load_keycode(&mf, "dreamcast", "dpad2_down"),
			load_keycode(&mf, "compat",    "btn_trigger_left"),
			load_keycode(&mf, "compat",    "btn_trigger_right"),
			load_keycode(&mf, "compat",    "axis_dpad_x"),
			load_keycode(&mf, "compat",    "axis_dpad_y"),
			load_keycode(&mf, "compat",    "axis_dpad2_x"),
			load_keycode(&mf, "compat",    "axis_dpad2_y"),
			load_keycode(&mf, "dreamcast", "axis_x"),
			load_keycode(&mf, "dreamcast", "axis_y"),
			load_keycode(&mf, "dreamcast", "axis_trigger_left"),
			load_keycode(&mf, "dreamcast", "axis_trigger_right")
		};
		return mapping;
	}

	int input_evdev_init(Controller* controller, const char* device, const char* custom_mapping_fname = NULL)
	{
		char name[256] = "Unknown";

		printf("evdev: Trying to open device at '%s'\n", device);

		int fd = open(device, O_RDONLY);

		if (fd >= 0)
		{
			fcntl(fd, F_SETFL, O_NONBLOCK);
			if(ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
			{
				perror("evdev: ioctl");
				return -2;
			}
			else
			{
				printf("evdev: Found '%s' at '%s'\n", name, device);

				controller->fd = fd;

				const char* mapping_fname;

				if(custom_mapping_fname != NULL)
				{
					mapping_fname = custom_mapping_fname;
				}
				else
				{
					#if defined(TARGET_PANDORA)
						mapping_fname = "controller_pandora.cfg";
					#elif defined(TARGET_GCW0)
						mapping_fname = "controller_gcwz.cfg";
					#else
						if (strcmp(name, "Microsoft X-Box 360 pad") == 0 ||
							strcmp(name, "Xbox 360 Wireless Receiver") == 0 ||
							strcmp(name, "Xbox 360 Wireless Receiver (XBOX)") == 0)
						{
							mapping_fname = "controller_xpad.cfg";
						}
						else if (strstr(name, "Xbox Gamepad (userspace driver)") != NULL)
						{
							mapping_fname = "controller_xboxdrv.cfg";
						}
						else if (strstr(name, "keyboard") != NULL ||
								 strstr(name, "Keyboard") != NULL)
						{
							mapping_fname = "keyboard.cfg";
						}
						else
						{
							mapping_fname = "controller_generic.cfg";
						}
					#endif
				}
				if(loaded_mappings.count(string(mapping_fname)) == 0)
				{
					size_t size_needed = snprintf(NULL, 0, EVDEV_MAPPING_PATH, mapping_fname) + 1;
					char* mapping_path = (char*)malloc(size_needed);
					sprintf(mapping_path, EVDEV_MAPPING_PATH, mapping_fname);
					FILE* mapping_fd = fopen(GetPath(mapping_path).c_str(), "r");
					free(mapping_path);
					
					if(mapping_fd != NULL)
					{
						printf("evdev: reading mapping file: '%s'\n", mapping_fname);
						loaded_mappings.insert(std::make_pair(string(mapping_fname), load_mapping(mapping_fd)));
						fclose(mapping_fd);
					}
					else
					{
						printf("evdev: unable to open mapping file '%s'\n", mapping_fname);
						return -3;
					}
				}
				controller->mapping = &loaded_mappings[string(mapping_fname)];
				printf("evdev: Using '%s' mapping\n", controller->mapping->name);
				return 0;
			}
		}
		else
		{
			perror("evdev: open");
			return -1;
		}
	}

	bool input_evdev_handle(Controller* controller, u32 port)
	{
		#define SET_FLAG(field, mask, expr) field =((expr) ? (field & ~mask) : (field | mask))
		if (controller->fd < 0 || controller->mapping == NULL)
		{
			return false;
		}

		input_event ie;

		while(read(controller->fd, &ie, sizeof(ie)) == sizeof(ie))
		{
			if(ie.type != EV_SYN && ie.type != EV_MSC)
			{
				printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
			}
			switch(ie.type)
			{
				case EV_KEY:
					if (ie.code == controller->mapping->Btn_A) {
						SET_FLAG(kcode[port], DC_BTN_A, ie.value);
					} else if (ie.code == controller->mapping->Btn_B) {
						SET_FLAG(kcode[port], DC_BTN_B, ie.value);
					} else if (ie.code == controller->mapping->Btn_C) {
						SET_FLAG(kcode[port], DC_BTN_C, ie.value);
					} else if (ie.code == controller->mapping->Btn_D) {
						SET_FLAG(kcode[port], DC_BTN_D, ie.value);
					} else if (ie.code == controller->mapping->Btn_X) {
						SET_FLAG(kcode[port], DC_BTN_X, ie.value);
					} else if (ie.code == controller->mapping->Btn_Y) {
						SET_FLAG(kcode[port], DC_BTN_Y, ie.value);
					} else if (ie.code == controller->mapping->Btn_Z) {
						SET_FLAG(kcode[port], DC_BTN_Z, ie.value);
					} else if (ie.code == controller->mapping->Btn_Start) {
						SET_FLAG(kcode[port], DC_BTN_START, ie.value);
					} else if (ie.code == controller->mapping->Btn_Escape) {
						die("death by escape key");
					} else if (ie.code == controller->mapping->Btn_DPad_Left) {
						SET_FLAG(kcode[port], DC_DPAD_LEFT, ie.value);
					} else if (ie.code == controller->mapping->Btn_DPad_Right) {
						SET_FLAG(kcode[port], DC_DPAD_RIGHT, ie.value);
					} else if (ie.code == controller->mapping->Btn_DPad_Up) {
						SET_FLAG(kcode[port], DC_DPAD_UP, ie.value);
					} else if (ie.code == controller->mapping->Btn_DPad_Down) {
						SET_FLAG(kcode[port], DC_DPAD_DOWN, ie.value);
					} else if (ie.code == controller->mapping->Btn_DPad2_Left) {
						SET_FLAG(kcode[port], DC_DPAD2_LEFT, ie.value);
					} else if (ie.code == controller->mapping->Btn_DPad2_Right) {
						SET_FLAG(kcode[port], DC_DPAD2_RIGHT, ie.value);
					} else if (ie.code == controller->mapping->Btn_DPad2_Up) {
						SET_FLAG(kcode[port], DC_DPAD2_UP, ie.value);
					} else if (ie.code == controller->mapping->Btn_DPad2_Down) {
						SET_FLAG(kcode[port], DC_DPAD2_DOWN, ie.value);
					} else if (ie.code == controller->mapping->Btn_Trigger_Left) {
						lt[port] = (ie.value ? 255 : 0);
					} else if (ie.code == controller->mapping->Btn_Trigger_Right) {
						rt[port] = (ie.value ? 255 : 0);
					}
					break;
				case EV_ABS:
					if (ie.code == controller->mapping->Axis_DPad_X)
					{
						switch(ie.value)
						{
							case -1:
								SET_FLAG(kcode[port], DC_DPAD_LEFT,  1);
								SET_FLAG(kcode[port], DC_DPAD_RIGHT, 0);
								break;
							case 0:
								SET_FLAG(kcode[port], DC_DPAD_LEFT,  0);
								SET_FLAG(kcode[port], DC_DPAD_RIGHT, 0);
								break;
							case 1:
								SET_FLAG(kcode[port], DC_DPAD_LEFT,  0);
								SET_FLAG(kcode[port], DC_DPAD_RIGHT, 1);
								break;
						}
					}
					else if (ie.code == controller->mapping->Axis_DPad_Y)
					{
						switch(ie.value)
						{
							case -1:
								SET_FLAG(kcode[port], DC_DPAD_UP,   1);
								SET_FLAG(kcode[port], DC_DPAD_DOWN, 0);
								break;
							case 0:
								SET_FLAG(kcode[port], DC_DPAD_UP,  0);
								SET_FLAG(kcode[port], DC_DPAD_DOWN, 0);
								break;
							case 1:
								SET_FLAG(kcode[port], DC_DPAD_UP,  0);
								SET_FLAG(kcode[port], DC_DPAD_DOWN, 1);
								break;
						}
					}
					else if (ie.code == controller->mapping->Axis_DPad2_X)
					{
						switch(ie.value)
						{
							case -1:
								SET_FLAG(kcode[port], DC_DPAD2_LEFT,  1);
								SET_FLAG(kcode[port], DC_DPAD2_RIGHT, 0);
								break;
							case 0:
								SET_FLAG(kcode[port], DC_DPAD2_LEFT,  0);
								SET_FLAG(kcode[port], DC_DPAD2_RIGHT, 0);
								break;
							case 1:
								SET_FLAG(kcode[port], DC_DPAD2_LEFT,  0);
								SET_FLAG(kcode[port], DC_DPAD2_RIGHT, 1);
								break;
						}
					}
					else if (ie.code == controller->mapping->Axis_DPad2_X)
					{
						switch(ie.value)
						{
							case -1:
								SET_FLAG(kcode[port], DC_DPAD2_UP,   1);
								SET_FLAG(kcode[port], DC_DPAD2_DOWN, 0);
								break;
							case 0:
								SET_FLAG(kcode[port], DC_DPAD2_UP,  0);
								SET_FLAG(kcode[port], DC_DPAD2_DOWN, 0);
								break;
							case 1:
								SET_FLAG(kcode[port], DC_DPAD2_UP,  0);
								SET_FLAG(kcode[port], DC_DPAD2_DOWN, 1);
								break;
						}
					}
					else if (ie.code == controller->mapping->Axis_Analog_X)
					{
						printf("%d", ie.value);
						joyx[port] = (s8)(ie.value/256);
					}
					else if (ie.code == controller->mapping->Axis_Analog_Y)
					{
						joyy[port] = (s8)(ie.value/256);
					}
					else if (ie.code == controller->mapping->Axis_Trigger_Left)
					{
						lt[port] = (s8)ie.value;
					}
					else if (ie.code == controller->mapping->Axis_Trigger_Right)
					{
						rt[port] = (s8)ie.value;
					}
					break;
			}
		}
	}
#endif









