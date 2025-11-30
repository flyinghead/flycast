/*
	Copyright 2025 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "nswitch.h"
#include "imgui.h"
#include <string>

//
// Use the synchronous Switch keyboard applet to edit some text
//
bool switchEditText(char *value, size_t capacity, ImGuiInputTextFlags flags, bool multiline)
{
	SwkbdConfig kbdConfig;
	Result rc = swkbdCreate(&kbdConfig, 0);
	if (!R_SUCCEEDED(rc))
		return false;

	// Select a Preset to use
	if (flags & ImGuiInputTextFlags_Password)
		swkbdConfigMakePresetPassword(&kbdConfig);
	else
		swkbdConfigMakePresetDefault(&kbdConfig);
	if (flags & ImGuiInputTextFlags_CharsDecimal) {
		swkbdConfigSetType(&kbdConfig, SwkbdType_NumPad);
	}
	else
	{
		swkbdConfigSetType(&kbdConfig, SwkbdType_Normal);
		if (flags & ImGuiInputTextFlags_CharsNoBlank)
			swkbdConfigSetKeySetDisableBitmask(&kbdConfig, SwkbdKeyDisableBitmask_Space);
		swkbdConfigSetReturnButtonFlag(&kbdConfig, multiline);
	}

	swkbdConfigSetOkButtonText(&kbdConfig, "Submit");
	//swkbdConfigSetGuideText(&kbdConfig, "Hint");	// Text displayed in gray when field is empty TODO?
	//swkbdConfigSetTextCheckCallback(&kbdConfig, validate_text);

	// Set the initial string value
	swkbdConfigSetInitialText(&kbdConfig, value);

	std::string backupValue = value;
	rc = swkbdShow(&kbdConfig, value, capacity);
	swkbdClose(&kbdConfig);

	if (R_SUCCEEDED(rc))
		return true;

	// Restore the initial value
	strncpy(value, backupValue.c_str(), capacity);

	return false;
}
