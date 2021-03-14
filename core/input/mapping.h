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
#pragma once

#include "types.h"
#include "gamepad.h"

#include <map>
#include <memory>

class InputMapping
{
public:
	InputMapping() = default;
	InputMapping(const InputMapping& other) {
		name = other.name;
		dead_zone = other.dead_zone;
		for (int port = 0; port < 4; port++)
		{
			buttons[port] = other.buttons[port];
			axes[port] = other.axes[port];
			axes_inverted[port] = other.axes_inverted[port];
		}
	}

	std::string name;
	float dead_zone = 0.1f;

	DreamcastKey get_button_id(u32 port, u32 code)
	{
		auto it = buttons[port].find(code);
		if (it != buttons[port].end())
			return it->second;
		else
			return EMU_BTN_NONE;
	}
	void set_button(u32 port, DreamcastKey id, u32 code);
	void set_button(DreamcastKey id, u32 code) { set_button(0, id, code); }
	u32 get_button_code(u32 port, DreamcastKey key);

	DreamcastKey get_axis_id(u32 port, u32 code)
	{
		auto it = axes[port].find(code);
		if (it != axes[port].end())
			return it->second;
		else
			return EMU_AXIS_NONE;
	}
	bool get_axis_inverted(u32 port, u32 code)
	{
		auto it = axes_inverted[port].find(code);
		if (it != axes_inverted[port].end())
			return it->second;
		else
			return false;
	}
	u32 get_axis_code(u32 port, DreamcastKey key);
	void set_axis(u32 port, DreamcastKey id, u32 code, bool inverted);
	void set_axis(DreamcastKey id, u32 code, bool inverted) { set_axis(0, id, code, inverted); }

	void load(FILE* fp);
	bool save(const char *name);

	bool is_dirty() { return dirty; }

	static std::shared_ptr<InputMapping> LoadMapping(const char *name);
	static void SaveMapping(const char *name, const std::shared_ptr<InputMapping>& mapping);

protected:
	bool dirty = false;

private:
	std::map<u32, DreamcastKey> buttons[4];
	std::map<u32, DreamcastKey> axes[4];
	std::map<u32, bool> axes_inverted[4];

	static std::map<std::string, std::shared_ptr<InputMapping>> loaded_mappings;
};

class IdentityInputMapping : public InputMapping
{
public:
	IdentityInputMapping() {
		name = "Default";
		dead_zone = 0.1f;
		
		for (int i = 0; i < 32; i++)
			set_button(0, (DreamcastKey)(1 << i), 1 << i);
		set_axis(0, DC_AXIS_X, DC_AXIS_X, false);
		set_axis(0, DC_AXIS_Y, DC_AXIS_Y, false);
		set_axis(0, DC_AXIS_LT, DC_AXIS_LT, false);
		set_axis(0, DC_AXIS_RT, DC_AXIS_RT, false);
		set_axis(0, DC_AXIS_X2, DC_AXIS_X2, false);
		set_axis(0, DC_AXIS_Y2, DC_AXIS_Y2, false);
	}
};
