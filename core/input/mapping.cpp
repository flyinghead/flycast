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
#include <cmath>
#include "mapping.h"
#include "cfg/ini.h"
#include "stdclass.h"

static struct
{
	DreamcastKey id;
	std::string section;
	std::string option;
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
	{ EMU_BTN_FFORWARD, "emulator", "btn_fforward" },
	{ EMU_BTN_TRIGGER_LEFT, "compat", "btn_trigger_left" },
	{ EMU_BTN_TRIGGER_RIGHT, "compat", "btn_trigger_right" },
	{ EMU_BTN_ANA_UP, "compat", "btn_analog_up" },
	{ EMU_BTN_ANA_DOWN, "compat", "btn_analog_down" },
	{ EMU_BTN_ANA_LEFT, "compat", "btn_analog_left" },
	{ EMU_BTN_ANA_RIGHT, "compat", "btn_analog_right" },
	{ DC_BTN_RELOAD, "dreamcast", "reload" },
};

static struct
{
	DreamcastKey id;
	std::string section;
	std::string option;
	std::string section_inverted;
	std::string option_inverted;
}
axis_list[] =
{
	{ DC_AXIS_X, "dreamcast", "axis_x", "compat", "axis_x_inverted" },
	{ DC_AXIS_Y, "dreamcast", "axis_y", "compat", "axis_y_inverted" },
	{ DC_AXIS_LT, "dreamcast", "axis_trigger_left",  "compat", "axis_trigger_left_inverted" },
	{ DC_AXIS_RT, "dreamcast", "axis_trigger_right", "compat", "axis_trigger_right_inverted" },
	{ DC_AXIS_X2, "dreamcast", "axis_right_x", "compat", "axis_right_x_inverted" },
	{ DC_AXIS_Y2, "dreamcast", "axis_right_y", "compat", "axis_right_y_inverted" },
	{ EMU_AXIS_DPAD1_X, "compat", "axis_dpad1_x", "compat", "axis_dpad1_x_inverted" },
	{ EMU_AXIS_DPAD1_Y, "compat", "axis_dpad1_y", "compat", "axis_dpad1_y_inverted" },
	{ EMU_AXIS_DPAD2_X, "compat", "axis_dpad2_x", "compat", "axis_dpad2_x_inverted" },
	{ EMU_AXIS_DPAD2_Y, "compat", "axis_dpad2_y", "compat", "axis_dpad2_y_inverted" },
	{ EMU_AXIS_BTN_A, "compat", "axis_btn_a", "compat", "axis_btn_a_inverted" },
	{ EMU_AXIS_BTN_B, "compat", "axis_btn_b", "compat", "axis_btn_b_inverted" },
	{ EMU_AXIS_BTN_C, "compat", "axis_btn_c", "compat", "axis_btn_c_inverted" },
	{ EMU_AXIS_BTN_D, "compat", "axis_btn_d", "compat", "axis_btn_d_inverted" },
	{ EMU_AXIS_BTN_X, "compat", "axis_btn_x", "compat", "axis_btn_x_inverted" },
	{ EMU_AXIS_BTN_Y, "compat", "axis_btn_y", "compat", "axis_btn_y_inverted" },
	{ EMU_AXIS_BTN_Z, "compat", "axis_btn_z", "compat", "axis_btn_z_inverted" },
	{ EMU_AXIS_BTN_START, "compat", "axis_btn_start", "compat", "axis_btn_start_inverted" },
	{ EMU_AXIS_DPAD_LEFT, "compat", "axis_dpad1_left", "compat", "axis_dpad1_left_inverted" },
	{ EMU_AXIS_DPAD_RIGHT, "compat", "axis_dpad1_right", "compat", "axis_dpad1_right_inverted" },
	{ EMU_AXIS_DPAD_UP, "compat", "axis_dpad1_up", "compat", "axis_dpad1_up_inverted" },
	{ EMU_AXIS_DPAD_DOWN, "compat", "axis_dpad1_down", "compat", "axis_dpad1_down_inverted" },
	{ EMU_AXIS_DPAD2_LEFT, "compat", "axis_dpad2_left", "compat", "axis_dpad2_left_inverted" },
	{ EMU_AXIS_DPAD2_RIGHT, "compat", "axis_dpad2_right", "compat", "axis_dpad2_right_inverted" },
	{ EMU_AXIS_DPAD2_UP, "compat", "axis_dpad2_up", "compat", "axis_dpad2_up_inverted" },
	{ EMU_AXIS_DPAD2_DOWN, "compat", "axis_dpad2_down", "compat", "axis_dpad2_down_inverted" }
};

std::map<std::string, std::shared_ptr<InputMapping>> InputMapping::loaded_mappings;

void InputMapping::set_button(u32 port, DreamcastKey id, u32 code)
{
	if (id != EMU_BTN_NONE)
	{
		while (true)
		{
			u32 code = get_button_code(port, id);
			if (code == (u32)-1)
				break;
			buttons[port][code] = EMU_BTN_NONE;
		}
		buttons[port][code] = id;
		dirty = true;
	}
}

void InputMapping::set_axis(u32 port, DreamcastKey id, u32 code, bool is_inverted)
{
	if (id != EMU_AXIS_NONE)
	{
		while (true)
		{
			u32 code = get_axis_code(port, id);
			if (code == (u32)-1)
				break;
			axes[port][code] = EMU_AXIS_NONE;
		}
		axes[port][code] = id;
		axes_inverted[port][code] = is_inverted;
		dirty = true;
	}
}

using namespace emucfg;

void InputMapping::load(FILE* fp)
{
	ConfigFile mf;
	mf.parse(fp);

	this->name = mf.get("emulator", "mapping_name", "<Unknown>");

	int dz = mf.get_int("emulator", "dead_zone", 10);
	dz = std::min(dz, 100);
	dz = std::max(dz, 0);

	this->dead_zone = (float)dz / 100.f;

	for (int port = 0; port < 4; port++)
	{
		for (u32 i = 0; i < ARRAY_SIZE(button_list); i++)
		{
			std::string option;
			if (port == 0)
				option = button_list[i].option;
			else
				option = button_list[i].option + std::to_string(port);
			int button_code = mf.get_int(button_list[i].section, option, -1);
			if (button_code >= 0)
				this->set_button(port, button_list[i].id, button_code);
		}

		for (u32 i = 0; i < ARRAY_SIZE(axis_list); i++)
		{
			std::string option;
			if (port == 0)
				option = axis_list[i].option;
			else
				option = axis_list[i].option + std::to_string(port);
			int axis_code = mf.get_int(axis_list[i].section, option, -1);
			if (axis_code >= 0)
			{
				if (port == 0)
					option = axis_list[i].option_inverted;
				else
					option = axis_list[i].option_inverted + std::to_string(port);
				this->set_axis(port, axis_list[i].id, axis_code, mf.get_bool(axis_list[i].section_inverted, option, false));
			}
		}
	}
	dirty = false;
}

u32 InputMapping::get_button_code(u32 port, DreamcastKey key)
{
	for (auto& it : buttons[port])
	{
		if (it.second == key)
			return it.first;
	}
	return -1;
}

u32 InputMapping::get_axis_code(u32 port, DreamcastKey key)
{
	for (auto& it : axes[port])
	{
		if (it.second == key)
			return it.first;
	}
	return -1;
}

std::shared_ptr<InputMapping> InputMapping::LoadMapping(const char *name)
{
	auto it = loaded_mappings.find(name);
	if (it != loaded_mappings.end())
		return it->second;

	std::string path = get_readonly_config_path((std::string("mappings/") + name).c_str());
	FILE *fp = fopen(path.c_str(), "r");
	if (fp == NULL)
		return NULL;
	std::shared_ptr<InputMapping> mapping = std::make_shared<InputMapping>();
	mapping->load(fp);
	fclose(fp);
	loaded_mappings[name] = mapping;

	return mapping;
}

bool InputMapping::save(const char *name)
{
	if (!dirty)
		return true;

	std::string path = get_writable_config_path("mappings/");
	make_directory(path);
	path = get_writable_config_path((std::string("mappings/") + name).c_str());
	FILE *fp = fopen(path.c_str(), "w");
	if (fp == NULL)
	{
		WARN_LOG(INPUT, "Cannot save controller mappings into %s", path.c_str());
		return false;
	}
	ConfigFile mf;

	mf.set("emulator", "mapping_name", this->name);
	mf.set_int("emulator", "dead_zone", (int)std::round(this->dead_zone * 100.f));

	for (int port = 0; port < 4; port++)
	{
		for (u32 i = 0; i < ARRAY_SIZE(button_list); i++)
		{
			std::string option;
			if (port == 0)
				option = button_list[i].option;
			else
				option = button_list[i].option + std::to_string(port);

			for (auto& it : buttons[port])
			{
				if (it.second == button_list[i].id)
					mf.set_int(button_list[i].section, option, (int)it.first);
			}
		}

		for (u32 i = 0; i < ARRAY_SIZE(axis_list); i++)
		{
			std::string option;
			if (port == 0)
				option = axis_list[i].option;
			else
				option = axis_list[i].option + std::to_string(port);
			std::string option_inverted;
			if (port == 0)
				option_inverted = axis_list[i].option_inverted;
			else
				option_inverted = axis_list[i].option_inverted + std::to_string(port);

			for (auto& it : axes[port])
			{
				if (it.second == axis_list[i].id)
				{
					mf.set_int(axis_list[i].section, option, (int)it.first);
					mf.set_bool(axis_list[i].section_inverted, option_inverted, axes_inverted[port][it.first]);
				}
			}
		}
	}
	mf.save(fp);
	dirty = false;
	fclose(fp);

	return true;
}

void InputMapping::SaveMapping(const char *name, std::shared_ptr<InputMapping> mapping)
{
	mapping->save(name);
	InputMapping::loaded_mappings[name] = mapping;
}
