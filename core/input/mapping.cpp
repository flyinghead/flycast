/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "mapping.h"
#include "cfg/ini.h"

static struct
{
	DreamcastKey id;
	string section;
	string option;
}
button_list[] =
{
	{ DC_BTN_A, "dreamcast", "btn_a" },
	{ DC_BTN_B, "dreamcast", "btn_b" },
	{ DC_BTN_C, "dreamcast", "btn_c" },
	{ DC_BTN_D, "dreamcast", "btn_d" },
	{ DC_BTN_X, "dreamcast", "btn_x" },
	{ DC_BTN_Y, "dreamcast", "btn_y" },
	{ DC_BTN_Z, "dreamcast", "btn_z" },
	{ DC_BTN_START, "dreamcast", "btn_start" },
	{ DC_DPAD_LEFT, "dreamcast", "btn_dpad1_left" },
	{ DC_DPAD_RIGHT, "dreamcast", "btn_dpad1_right" },
	{ DC_DPAD_UP, "dreamcast", "btn_dpad1_up" },
	{ DC_DPAD_DOWN, "dreamcast", "btn_dpad1_down" },
	{ DC_DPAD2_LEFT, "dreamcast", "btn_dpad2_left" },
	{ DC_DPAD2_RIGHT, "dreamcast", "btn_dpad2_right" },
	{ DC_DPAD2_UP, "dreamcast", "btn_dpad2_up" },
	{ DC_DPAD2_DOWN, "dreamcast", "btn_dpad2_down" },
	{ EMU_BTN_ESCAPE, "emulator", "btn_escape" },
	{ EMU_BTN_MENU, "emulator", "btn_menu" },
	{ EMU_BTN_TRIGGER_LEFT, "compat", "btn_trigger_left" },
	{ EMU_BTN_TRIGGER_RIGHT, "compat", "btn_trigger_right" }
};

static struct
{
	DreamcastKey id;
	string section;
	string option;
	string section_inverted;
	string option_inverted;
}
axis_list[] =
{
	{ DC_AXIS_X, "dreamcast", "axis_x", "compat", "axis_x_inverted" },
	{ DC_AXIS_Y, "dreamcast", "axis_y", "compat", "axis_y_inverted" },
	{ DC_AXIS_LT,  "dreamcast", "axis_trigger_left",  "compat", "axis_trigger_left_inverted" },
	{ DC_AXIS_RT, "dreamcast", "axis_trigger_right", "compat", "axis_trigger_right_inverted" },
	{ EMU_AXIS_DPAD1_X, "compat", "axis_dpad1_x", "compat", "axis_dpad1_x_inverted" },
	{ EMU_AXIS_DPAD1_Y, "compat", "axis_dpad1_y", "compat", "axis_dpad1_y_inverted" },
	{ EMU_AXIS_DPAD2_X, "compat", "axis_dpad2_x", "compat", "axis_dpad2_x_inverted" },
	{ EMU_AXIS_DPAD2_Y, "compat", "axis_dpad2_y", "compat", "axis_dpad2_y_inverted" }
};

std::map<std::string, InputMapping *> InputMapping::loaded_mappings;

void InputMapping::set_button(DreamcastKey id, u32 code)
{
	if (id != EMU_BTN_NONE)
	{
		while (true)
		{
			u32 code = get_button_code(id);
			if (code == -1)
				break;
			buttons[code] = EMU_BTN_NONE;
		}
		buttons[code] = id;
		dirty = true;
	}
}

void InputMapping::set_axis(DreamcastKey id, u32 code, bool is_inverted)
{
	if (id != EMU_AXIS_NONE)
	{
		while (true)
		{
			u32 code = get_axis_code(id);
			if (code == -1)
				break;
			axes[code] = EMU_AXIS_NONE;
		}
		axes[code] = id;
		axes_inverted[code] = is_inverted;
		dirty = true;
	}
}

using namespace emucfg;

void InputMapping::load(FILE* fp)
{
	ConfigFile mf;
	mf.parse(fp);

	this->name = mf.get("emulator", "mapping_name", "<Unknown>");

	for (int i = 0; i < ARRAY_SIZE(button_list); i++)
	{
		int button_code = mf.get_int(button_list[i].section, button_list[i].option, -1);
		if (button_code >= 0)
		{
			this->set_button(button_list[i].id, button_code);
		}
	}

	for (int i = 0; i < ARRAY_SIZE(axis_list); i++)
	{
		int axis_code = mf.get_int(axis_list[i].section, axis_list[i].option, -1);
		if (axis_code >= 0)
		{
			this->set_axis(axis_list[i].id, axis_code, mf.get_bool(axis_list[i].section_inverted, axis_list[i].option_inverted, false));
		}
	}
	dirty = false;
}

u32 InputMapping::get_button_code(DreamcastKey key)
{
	for (auto it : buttons)
	{
		if (it.second == key)
			return it.first;
	}
	return -1;
}

u32 InputMapping::get_axis_code(DreamcastKey key)
{
	for (auto it : axes)
	{
		if (it.second == key)
			return it.first;
	}
	return -1;
}

InputMapping *InputMapping::LoadMapping(const char *name)
{
	auto it = loaded_mappings.find(name);
	if (it != loaded_mappings.end())
		return it->second;

	std::string path = get_writable_config_path((std::string("/mappings/") + name).c_str());
	FILE *fp = fopen(path.c_str(), "r");
	if (fp == NULL)
		return NULL;
	InputMapping *mapping = new InputMapping();
	mapping->load(fp);
	fclose(fp);
	loaded_mappings[name] = mapping;

	return mapping;
}

bool InputMapping::save(const char *name)
{
	loaded_mappings[name] = this;
	if (!dirty)
		return true;

	std::string path = get_writable_config_path("/mappings/");
	make_directory(path);
	path = get_writable_config_path((std::string("/mappings/") + name).c_str());
	FILE *fp = fopen(path.c_str(), "w");
	if (fp == NULL)
	{
		printf("Cannot save controller mappings into %s\n", path.c_str());
		return false;
	}
	ConfigFile mf;

	mf.set("emulator", "mapping_name", this->name);

	for (int i = 0; i < ARRAY_SIZE(button_list); i++)
	{
		for (auto it : buttons)
		{
			if (it.second == button_list[i].id)
				mf.set_int(button_list[i].section, button_list[i].option, (int)it.first);
		}
	}

	for (int i = 0; i < ARRAY_SIZE(axis_list); i++)
	{
		for (auto it : axes)
		{
			if (it.second == axis_list[i].id)
			{
				mf.set_int(axis_list[i].section, axis_list[i].option, (int)it.first);
				mf.set_bool(axis_list[i].section_inverted, axis_list[i].option_inverted, axes_inverted[it.first]);
			}
		}
	}
	mf.save(fp);
	dirty = false;
	fclose(fp);

	return true;
}
