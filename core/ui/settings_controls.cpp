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
#include "settings.h"
#include "gui.h"
#include "IconsFontAwesome6.h"
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "input/mouse.h"
#include "hw/maple/maple_devs.h"
#include "vgamepad.h"
#include "oslib/storage.h"

#if defined(USE_SDL)
#include "sdl/dreamlink.h" // For USE_DREAMCASTCONTROLLER
#endif

static float calcComboWidth(const char *biggestLabel) {
	return ImGui::CalcTextSize(biggestLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
}

static const char *maple_device_types[] =
{
	"None",
	"Sega Controller",
	"Light Gun",
	"Keyboard",
	"Mouse",
	"Twin Stick",
	"Arcade/Ascii Stick",
	"Maracas Controller",
	"Fishing Controller",
	"Pop'n Music controller",
	"Racing Controller",
	"Densha de Go! Controller",
	"Panther DC/Full Controller",
//	"Dreameye",
};

static const char *maple_expansion_device_types[] =
{
	"None",
	"Sega VMU",
	"Vibration Pack",
	"Microphone",
};

static const char *maple_device_name(MapleDeviceType type)
{
	switch (type)
	{
	case MDT_SegaController:
		return maple_device_types[1];
	case MDT_LightGun:
		return maple_device_types[2];
	case MDT_Keyboard:
		return maple_device_types[3];
	case MDT_Mouse:
		return maple_device_types[4];
	case MDT_TwinStick:
		return maple_device_types[5];
	case MDT_AsciiStick:
		return maple_device_types[6];
	case MDT_MaracasController:
		return maple_device_types[7];
	case MDT_FishingController:
		return maple_device_types[8];
	case MDT_PopnMusicController:
		return maple_device_types[9];
	case MDT_RacingController:
		return maple_device_types[10];
	case MDT_DenshaDeGoController:
		return maple_device_types[11];
	case MDT_SegaControllerXL:
		return maple_device_types[12];
	case MDT_Dreameye:
//		return maple_device_types[13];
	case MDT_None:
	default:
		return maple_device_types[0];
	}
}

static MapleDeviceType maple_device_type_from_index(int idx)
{
	switch (idx)
	{
	case 1:
		return MDT_SegaController;
	case 2:
		return MDT_LightGun;
	case 3:
		return MDT_Keyboard;
	case 4:
		return MDT_Mouse;
	case 5:
		return MDT_TwinStick;
	case 6:
		return MDT_AsciiStick;
	case 7:
		return MDT_MaracasController;
	case 8:
		return MDT_FishingController;
	case 9:
		return MDT_PopnMusicController;
	case 10:
		return MDT_RacingController;
	case 11:
		return MDT_DenshaDeGoController;
	case 12:
		return MDT_SegaControllerXL;
	case 13:
		return MDT_Dreameye;
	case 0:
	default:
		return MDT_None;
	}
}

static const char *maple_expansion_device_name(MapleDeviceType type)
{
	switch (type)
	{
	case MDT_SegaVMU:
		return maple_expansion_device_types[1];
	case MDT_PurupuruPack:
		return maple_expansion_device_types[2];
	case MDT_Microphone:
		return maple_expansion_device_types[3];
	case MDT_None:
	default:
		return maple_expansion_device_types[0];
	}
}

static const char *maple_ports[] = { "None", "A", "B", "C", "D", "All" };

struct Mapping {
	DreamcastKey key;
	const char *name;
};

static const Mapping dcButtons[] = {
	{ EMU_BTN_NONE, "Directions" },
	{ DC_DPAD_UP, "Up" },
	{ DC_DPAD_DOWN, "Down" },
	{ DC_DPAD_LEFT, "Left" },
	{ DC_DPAD_RIGHT, "Right" },

	{ DC_AXIS_UP, "Thumbstick Up" },
	{ DC_AXIS_DOWN, "Thumbstick Down" },
	{ DC_AXIS_LEFT, "Thumbstick Left" },
	{ DC_AXIS_RIGHT, "Thumbstick Right" },

	{ DC_AXIS2_UP, "R.Thumbstick Up" },
	{ DC_AXIS2_DOWN, "R.Thumbstick Down" },
	{ DC_AXIS2_LEFT, "R.Thumbstick Left" },
	{ DC_AXIS2_RIGHT, "R.Thumbstick Right" },

	{ DC_AXIS3_UP,    "Axis 3 Up"    },
	{ DC_AXIS3_DOWN,  "Axis 3 Down"  },
	{ DC_AXIS3_LEFT,  "Axis 3 Left"  },
	{ DC_AXIS3_RIGHT, "Axis 3 Right" },

	{ DC_DPAD2_UP,    "DPad2 Up"    },
	{ DC_DPAD2_DOWN,  "DPad2 Down"  },
	{ DC_DPAD2_LEFT,  "DPad2 Left"  },
	{ DC_DPAD2_RIGHT, "DPad2 Right" },

	{ EMU_BTN_NONE, "Buttons" },
	{ DC_BTN_A, "A" },
	{ DC_BTN_B, "B" },
	{ DC_BTN_X, "X" },
	{ DC_BTN_Y, "Y" },
	{ DC_BTN_C, "C" },
	{ DC_BTN_D, "D" },
	{ DC_BTN_Z, "Z" },

	{ EMU_BTN_NONE, "Triggers"      },
	{ DC_AXIS_LT,   "Left Trigger"  },
	{ DC_AXIS_RT,   "Right Trigger" },
	{ DC_AXIS_LT2,   "Left Trigger 2" },
	{ DC_AXIS_RT2,   "Right Trigger 2" },

	{ EMU_BTN_NONE, "System Buttons" },
	{ DC_BTN_START, "Start" },
	{ DC_BTN_RELOAD, "Reload" },

	{ EMU_BTN_NONE, "Emulator" },
	{ EMU_BTN_MENU, "Menu" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },
	{ EMU_BTN_LOADSTATE, "Load State" },
	{ EMU_BTN_SAVESTATE, "Save State" },
	{ EMU_BTN_BYPASS_KB, "Bypass Emulated Keyboard" },
	{ EMU_BTN_SCREENSHOT, "Save Screenshot" },

	{ EMU_BTN_NONE, nullptr }
};

static const Mapping arcadeButtons[] = {
	{ EMU_BTN_NONE, "Directions" },
	{ DC_DPAD_UP, "Up" },
	{ DC_DPAD_DOWN, "Down" },
	{ DC_DPAD_LEFT, "Left" },
	{ DC_DPAD_RIGHT, "Right" },

	{ DC_AXIS_UP, "Thumbstick Up" },
	{ DC_AXIS_DOWN, "Thumbstick Down" },
	{ DC_AXIS_LEFT, "Thumbstick Left" },
	{ DC_AXIS_RIGHT, "Thumbstick Right" },

	{ DC_AXIS2_UP, "R.Thumbstick Up" },
	{ DC_AXIS2_DOWN, "R.Thumbstick Down" },
	{ DC_AXIS2_LEFT, "R.Thumbstick Left" },
	{ DC_AXIS2_RIGHT, "R.Thumbstick Right" },

	{ EMU_BTN_NONE, "Buttons" },
	{ DC_BTN_A, "Button 1" },
	{ DC_BTN_B, "Button 2" },
	{ DC_BTN_C, "Button 3" },
	{ DC_BTN_X, "Button 4" },
	{ DC_BTN_Y, "Button 5" },
	{ DC_BTN_Z, "Button 6" },
	{ DC_DPAD2_LEFT, "Button 7" },
	{ DC_DPAD2_RIGHT, "Button 8" },
//	{ DC_DPAD2_RIGHT, "Button 9" }, // TODO

	{ EMU_BTN_NONE, "Triggers" },
	{ DC_AXIS_LT, "Left Trigger" },
	{ DC_AXIS_RT, "Right Trigger" },
	{ DC_AXIS_LT2,   "Left Trigger 2" },
	{ DC_AXIS_RT2,   "Right Trigger 2" },

	{ EMU_BTN_NONE, "System Buttons" },
	{ DC_BTN_START, "Start" },
	{ DC_BTN_RELOAD, "Reload" },
	{ DC_BTN_D, "Coin" },
	{ DC_DPAD2_UP, "Service" },
	{ DC_DPAD2_DOWN, "Test" },
	{ DC_BTN_INSERT_CARD, "Insert Card" },

	{ EMU_BTN_NONE, "Emulator" },
	{ EMU_BTN_MENU, "Menu" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },
	{ EMU_BTN_LOADSTATE, "Load State" },
	{ EMU_BTN_SAVESTATE, "Save State" },
	{ EMU_BTN_BYPASS_KB, "Bypass Emulated Keyboard" },
	{ EMU_BTN_SCREENSHOT, "Save Screenshot" },

	{ EMU_BTN_NONE, nullptr }
};

static MapleDeviceType maple_expansion_device_type_from_index(int idx)
{
	switch (idx)
	{
	case 1:
		return MDT_SegaVMU;
	case 2:
		return MDT_PurupuruPack;
	case 3:
		return MDT_Microphone;
	case 0:
	default:
		return MDT_None;
	}
}

static std::shared_ptr<GamepadDevice> currentGamepad;
static InputMapping::InputSet mapped_codes;  // Stores multiple buttons in the order they were entered
static u64 map_start_time;
static bool arcade_button_mode;
static u32 gamepad_port;
static std::unordered_set<DreamcastKey> buttonState;

static void unmapControl(const std::shared_ptr<InputMapping>& mapping, u32 gamepad_port, DreamcastKey key)
{
	mapping->clear_button(gamepad_port, key);
	mapping->clear_axis(gamepad_port, key);
}

static DreamcastKey getOppositeDirectionKey(DreamcastKey key)
{
	switch (key)
	{
	case DC_DPAD_UP:
		return DC_DPAD_DOWN;
	case DC_DPAD_DOWN:
		return DC_DPAD_UP;
	case DC_DPAD_LEFT:
		return DC_DPAD_RIGHT;
	case DC_DPAD_RIGHT:
		return DC_DPAD_LEFT;
	case DC_DPAD2_UP:
		return DC_DPAD2_DOWN;
	case DC_DPAD2_DOWN:
		return DC_DPAD2_UP;
	case DC_DPAD2_LEFT:
		return DC_DPAD2_RIGHT;
	case DC_DPAD2_RIGHT:
		return DC_DPAD2_LEFT;
	case DC_AXIS_UP:
		return DC_AXIS_DOWN;
	case DC_AXIS_DOWN:
		return DC_AXIS_UP;
	case DC_AXIS_LEFT:
		return DC_AXIS_RIGHT;
	case DC_AXIS_RIGHT:
		return DC_AXIS_LEFT;
	case DC_AXIS2_UP:
		return DC_AXIS2_DOWN;
	case DC_AXIS2_DOWN:
		return DC_AXIS2_UP;
	case DC_AXIS2_LEFT:
		return DC_AXIS2_RIGHT;
	case DC_AXIS2_RIGHT:
		return DC_AXIS2_LEFT;
	case DC_AXIS3_UP:
		return DC_AXIS3_DOWN;
	case DC_AXIS3_DOWN:
		return DC_AXIS3_UP;
	case DC_AXIS3_LEFT:
		return DC_AXIS3_RIGHT;
	case DC_AXIS3_RIGHT:
		return DC_AXIS3_LEFT;
	default:
		return EMU_BTN_NONE;
	}
}

static void displayLabelOrCode(const char *label, u32 code, const char *suffix = "")
{
	if (label != nullptr)
		ImGui::Text("%s%s", label, suffix);
	else
		ImGui::Text("[%d]%s", code, suffix);
}

static void detect_input_popup(const Mapping *mapping)
{
	ImVec2 padding = ScaledVec2(20, 20);
	ImguiStyleVar _(ImGuiStyleVar_WindowPadding, padding);
	ImguiStyleVar _1(ImGuiStyleVar_ItemSpacing, padding);
	if (ImGui::BeginPopupModal("Map Control", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("Waiting for control '%s'...", mapping->name);
		u64 now = getTimeMs();

		// Check if we're still in the initial delay period
		if (now >= map_start_time)
		{
			// Check if device is still detecting input (might have been cancelled by button release)
			bool still_detecting = currentGamepad != nullptr && currentGamepad->is_input_detecting();

			// If detection was cancelled by button release, close popup immediately
			int remaining = still_detecting ? (int)(5 - (now - map_start_time) / 1000) : 0;

			if (remaining < 0)
				remaining = 5;

			if (still_detecting)
				ImGui::Text("Time out in %d s", remaining);

			// Display currently detected buttons during the countdown
			if (!mapped_codes.empty())
			{
				ImGui::Text("Current inputs: ");
				ImGui::SameLine();
				bool first = true;
				for (const InputMapping::InputDef& inputDef : mapped_codes)
				{
					if (!first)
					{
						ImGui::SameLine();
						ImGui::Text("&");
						ImGui::SameLine();
					}

					const char* name = nullptr;
					if (inputDef.is_button())
						name = currentGamepad->get_button_name(inputDef.code);
					else
						name = currentGamepad->get_axis_name(inputDef.code);

					displayLabelOrCode(name, inputDef.code);

					first = false;
				}

				// Allow early completion with Confirm button if at least one button is detected
				if (ImGui::Button("Confirm"))
					remaining = 0;
			}

			// Wait for the countdown to complete before mapping
			if (remaining <= 0)
			{
				std::shared_ptr<InputMapping> input_mapping = currentGamepad->get_input_mapping();
				if (input_mapping != NULL && !mapped_codes.empty())
				{
					unmapControl(input_mapping, gamepad_port, mapping->key);
					if (mapped_codes.size() == 1 && mapped_codes.front().is_axis())
					{
						// Single axis mapping
						const InputMapping::InputDef& axisInputDef = mapped_codes.front();
						const bool positive = (axisInputDef.type == InputMapping::InputDef::InputType::AXIS_POS);
						input_mapping->set_axis(gamepad_port, mapping->key, axisInputDef.code, positive);
						DreamcastKey opposite = getOppositeDirectionKey(mapping->key);
						// Map the axis opposite direction to the corresponding opposite dc button or axis,
						// but only if the opposite direction axis isn't used and the dc button or axis isn't mapped.
						if (opposite != EMU_BTN_NONE
								&& input_mapping->get_axis_id(gamepad_port, axisInputDef.code, !positive) == EMU_BTN_NONE
								&& input_mapping->get_axis_code(gamepad_port, opposite).first == (u32)-1
								&& input_mapping->get_button_code(gamepad_port, opposite) == (u32)-1)
							input_mapping->set_axis(gamepad_port, opposite, axisInputDef.code, !positive);
					}
					else
					{
						input_mapping->set_button(gamepad_port, mapping->key, InputMapping::ButtonCombo{mapped_codes, true});
					}
				}

				// Make sure to cancel input detection to prevent collecting more inputs
				if (currentGamepad)
					currentGamepad->cancel_detect_input();

				mapped_codes.clear();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
}

static void displayMappedControl(const std::shared_ptr<GamepadDevice>& gamepad, DreamcastKey key)
{
	std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
	InputMapping::ButtonCombo combo = input_mapping->get_button_combo(gamepad_port, key);

	if (combo.inputs.empty())
	{
		// Try axis
		std::pair<u32, bool> pair = input_mapping->get_axis_code(gamepad_port, key);
		const InputMapping::InputDef inputDef = InputMapping::InputDef::from_axis(pair.first, pair.second);
		if (inputDef.is_valid())
			displayLabelOrCode(gamepad->get_axis_name(inputDef.code), inputDef.code, inputDef.get_suffix());
	}
	else
	{
		// Display button combination in "Button1 & Button2 & ..." format
		bool first = true;
		for (const InputMapping::InputDef& inputDef : combo.inputs)
		{
			if (!first)
			{
				ImGui::SameLine();
				ImGui::Text("&");
				ImGui::SameLine();
			}

			const char* name = nullptr;
			if (inputDef.is_button())
				name = gamepad->get_button_name(inputDef.code);
			else if (inputDef.is_axis())
				name = gamepad->get_axis_name(inputDef.code);

			displayLabelOrCode(name, inputDef.code, inputDef.get_suffix());
			first = false;
		}

		if (combo.inputs.size() > 1)
		{
			if (ImGui::Checkbox("Sequential", &(combo.sequential)))
				// Update mapping with updated combo settings
				input_mapping->set_button(gamepad_port, key, combo);
			ImGui::SameLine();
			ShowHelpMarker(
				"When checked, this combo will only activate when all keys are pressed in the given sequence.\n"
				"When not checked, the combo will activate when all keys are pressed in any order.");
		}
	}
}

static float getAxisValue(const std::shared_ptr<GamepadDevice>& gamepad, DreamcastKey axis)
{
	int port = gamepad->maple_port();
	if (port == -1)
		return 0.f;
	if (port == 4)
		port = gamepad_port;
	float v;
	switch (axis)
	{
	case DC_AXIS_UP: v = -joyy[port] / 32768.f; break;
	case DC_AXIS_DOWN: v = joyy[port] / 32767.f; break;
	case DC_AXIS_LEFT: v = -joyx[port] / 32768.f; break;
	case DC_AXIS_RIGHT: v = joyx[port] / 32767.f; break;
	case DC_AXIS2_UP: v = -joyry[port] / 32768.f; break;
	case DC_AXIS2_DOWN: v = joyry[port] / 32767.f; break;
	case DC_AXIS2_LEFT: v = -joyrx[port] / 32768.f; break;
	case DC_AXIS2_RIGHT: v = joyrx[port] / 32767.f; break;
	case DC_AXIS3_UP: v = -joy3y[port] / 32768.f; break;
	case DC_AXIS3_DOWN: v = joy3y[port] / 32767.f; break;
	case DC_AXIS3_LEFT: v = -joy3x[port] / 32768.f; break;
	case DC_AXIS3_RIGHT: v = joy3x[port] / 32767.f; break;
	case DC_AXIS_LT: v = lt[port] / 65535.f; break;
	case DC_AXIS_RT: v = rt[port] / 65535.f; break;
	case DC_AXIS_LT2: v = lt2[port] / 65535.f; break;
	case DC_AXIS_RT2: v = rt2[port] / 65535.f; break;
	default: v = 0.f;
	}
	return std::clamp(v, 0.f, 1.f);
}

static void buttonListener(int port, DreamcastKey key, bool pressed)
{
	if (currentGamepad == nullptr || port == -1)
		return;
	if (currentGamepad->maple_port() == 4 && port != (int)gamepad_port)
		return;
	if (pressed)
		buttonState.insert(key);
	else
		buttonState.erase(key);
}

static bool getButtonState(const std::shared_ptr<GamepadDevice>& gamepad, DreamcastKey btn) {
	return buttonState.count(btn) != 0;
}

static void controller_mapping_popup(const std::shared_ptr<GamepadDevice>& gamepad)
{
	fullScreenWindow(true);
	ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("Controller Mapping", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float winWidth = ImGui::GetIO().DisplaySize.x - insetLeft - insetRight - (style.WindowBorderSize + style.WindowPadding.x) * 2;
		const float col_width = (winWidth - style.GrabMinSize - style.ItemSpacing.x
				- (ImGui::CalcTextSize("Map").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x)
				- (ImGui::CalcTextSize("Unmap").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x)) / 3;

		static int map_system;
		static int item_current_map_idx = 0;
		static int last_item_current_map_idx = 2;
		if (currentGamepad == nullptr) {
			gamepad->listenButtons(buttonListener);
			currentGamepad = gamepad;
		}

		std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
		if (input_mapping == NULL || ImGui::Button("Done", ScaledVec2(100, 30)))
		{
			ImGui::CloseCurrentPopup();
			gamepad->save_mapping(map_system);
			last_item_current_map_idx = 2;
			ImGui::EndPopup();
			gamepad->unlistenButtons(buttonListener);
			currentGamepad = nullptr;
			return;
		}
		ImGui::SetItemDefaultFocus();

		float portWidth = 0;
		if (gamepad->maple_port() == MAPLE_PORTS)
		{
			ImGui::SameLine();
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (uiScaled(30) - ImGui::GetFontSize()) / 2));
			portWidth = ImGui::CalcTextSize("AA").x + ImGui::GetStyle().ItemSpacing.x * 2.0f + ImGui::GetFontSize();
			ImGui::SetNextItemWidth(portWidth);
			if (ImGui::BeginCombo("Port", maple_ports[gamepad_port + 1]))
			{
				for (u32 j = 0; j < MAPLE_PORTS; j++)
				{
					bool is_selected = gamepad_port == j;
					if (ImGui::Selectable(maple_ports[j + 1], &is_selected))
						gamepad_port = j;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			portWidth += ImGui::CalcTextSize("Port").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x;
		}
		float comboWidth = ImGui::CalcTextSize("Dreamcast Controls").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 4;
		float gameConfigWidth = 0;
		if (!settings.content.gameId.empty())
			gameConfigWidth = ImGui::CalcTextSize(gamepad->isPerGameMapping() ? "Delete Game Config" : "Make Game Config").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2;
		ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - comboWidth - gameConfigWidth - ImGui::GetStyle().ItemSpacing.x - uiScaled(100) * 2 - portWidth);

		ImGui::AlignTextToFramePadding();

		if (!settings.content.gameId.empty())
		{
			if (gamepad->isPerGameMapping())
			{
				if (ImGui::Button("Delete Game Config", ScaledVec2(0, 30)))
				{
					gamepad->setPerGameMapping(false);
					if (!gamepad->find_mapping(map_system))
						gamepad->resetMappingToDefault(arcade_button_mode, true);
				}
			}
			else
			{
				if (ImGui::Button("Make Game Config", ScaledVec2(0, 30)))
					gamepad->setPerGameMapping(true);
			}
			ImGui::SameLine();
		}
		if (ImGui::Button("Reset...", ScaledVec2(100, 30)))
			ImGui::OpenPopup("Confirm Reset");

		{
			ImguiStyleVar _(ImGuiStyleVar_WindowPadding, ScaledVec2(20, 20));
			if (ImGui::BeginPopupModal("Confirm Reset", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
			{
				ImGui::Text("Are you sure you want to reset the mappings to default?");
				static bool hitbox;
				if (arcade_button_mode)
				{
					ImGui::Text("Controller Type:");
					if (ImGui::RadioButton("Gamepad", !hitbox))
						hitbox = false;
					ImGui::SameLine();
					if (ImGui::RadioButton("Arcade / Hit Box", hitbox))
						hitbox = true;
				}
				ImGui::NewLine();
				{
	 				ImguiStyleVar _(ImGuiStyleVar_ItemSpacing, ImVec2(uiScaled(20), ImGui::GetStyle().ItemSpacing.y));
					ImguiStyleVar _1(ImGuiStyleVar_FramePadding, ScaledVec2(10, 10));
					if (ImGui::Button("Yes"))
					{
						gamepad->resetMappingToDefault(arcade_button_mode, !hitbox);
						gamepad->save_mapping(map_system);
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button("No"))
						ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		ImGui::SameLine();

		const char* items[] = { "Dreamcast Controls", "Arcade Controls" };

		if (last_item_current_map_idx == 2 && game_started)
			// Select the right mappings for the current game
			item_current_map_idx = settings.platform.isArcade() ? 1 : 0;

		// Here our selection data is an index.

		ImGui::SetNextItemWidth(comboWidth);
		// Make the combo height the same as the Done and Reset buttons
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (uiScaled(30) - ImGui::GetFontSize()) / 2));
		ImGui::Combo("##arcadeMode", &item_current_map_idx, items, IM_ARRAYSIZE(items));
		ImGui::PopStyleVar();
		if (last_item_current_map_idx != 2 && item_current_map_idx != last_item_current_map_idx)
			gamepad->save_mapping(map_system);

		const Mapping *systemMapping = dcButtons;
		if (item_current_map_idx == 0)
		{
			arcade_button_mode = false;
			map_system = DC_PLATFORM_DREAMCAST;
			systemMapping = dcButtons;
		}
		else if (item_current_map_idx == 1)
		{
			arcade_button_mode = true;
			map_system = DC_PLATFORM_NAOMI;
			systemMapping = arcadeButtons;
		}

		if (item_current_map_idx != last_item_current_map_idx)
		{
			if (!gamepad->find_mapping(map_system))
				if (map_system == DC_PLATFORM_DREAMCAST || !gamepad->find_mapping(DC_PLATFORM_DREAMCAST))
					gamepad->resetMappingToDefault(arcade_button_mode, true);
			input_mapping = gamepad->get_input_mapping();

			last_item_current_map_idx = item_current_map_idx;
		}

		char key_id[32];

		ImGui::BeginChild(ImGui::GetID("buttons"), ImVec2(0, 0), ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NavFlattened);

		for (; systemMapping->name != nullptr; systemMapping++)
		{
			if (systemMapping->key == EMU_BTN_NONE)
			{
				ImGui::Columns(1, nullptr, false);
				header(systemMapping->name);
				ImGui::Columns(4, "bindings", false);
				ImGui::SetColumnWidth(0, col_width);
				ImGui::SetColumnWidth(1, col_width);
				ImGui::SetColumnWidth(2, col_width);
				continue;
			}
			snprintf(key_id, sizeof(key_id), "key_id%d", systemMapping->key);
			ImguiID _(key_id);

			const char *game_btn_name = nullptr;
			if (arcade_button_mode)
			{
				game_btn_name = GetCurrentGameButtonName(systemMapping->key);
				if (game_btn_name == nullptr)
					game_btn_name = GetCurrentGameAxisName(systemMapping->key);
			}
			if (game_btn_name != nullptr && game_btn_name[0] != '\0')
				ImGui::Text("%s - %s", systemMapping->name, game_btn_name);
			else
				ImGui::Text("%s", systemMapping->name);

			ImGui::NextColumn();
			displayMappedControl(gamepad, systemMapping->key);

			ImGui::NextColumn();
			if (dynamic_cast<KeyboardDevice*>(gamepad.get()) == nullptr
					&& dynamic_cast<Mouse*>(gamepad.get()) == nullptr)
			{
				if ((systemMapping->key & DC_BTN_GROUP_MASK) == DC_AXIS_STICKS
						|| (systemMapping->key & DC_BTN_GROUP_MASK) == DC_AXIS_TRIGGERS)
				{
					ImguiStyleColor _(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
					float v = getAxisValue(gamepad, systemMapping->key);
					char s[32];
					snprintf(s, sizeof(s), "%.0f%%", v * 100.f);
					ImGui::ProgressBar(v, ImVec2(-1, 0), s);
				}
				else if (getButtonState(gamepad, systemMapping->key)) {
					ImGui::Text(ICON_FA_CIRCLE_DOT);
				}
			}

			ImGui::NextColumn();
			if (ImGui::Button("Map"))
			{
				// Set a small delay to avoid capturing the button press used to click "Map"
				map_start_time = getTimeMs() + 300; // 300ms delay before starting the countdown
				ImGui::OpenPopup("Map Control");
				mapped_codes.clear();  // Clear previous button codes

				// Detect combos only for EMU_BUTTONS
				const auto buttonGroup = (systemMapping->key & DC_BTN_GROUP_MASK);
				const bool detectCombo = (buttonGroup == EMU_BUTTONS);

				// Setup a callback to collect button/axes presses
				gamepad->detectInput(true, true, detectCombo, [](u32 code, bool analog, bool positive)
				{
					if (analog)
						mapped_codes.insert_back(InputMapping::InputDef::from_axis(code, positive));
					else
						mapped_codes.insert_back(InputMapping::InputDef::from_button(code));
				});
			}
			detect_input_popup(systemMapping);
			ImGui::SameLine();
			if (ImGui::Button("Unmap"))
			{
				input_mapping = gamepad->get_input_mapping();
				unmapControl(input_mapping, gamepad_port, systemMapping->key);
			}
			ImGui::NextColumn();
		}
		ImGui::Columns(1, nullptr, false);
	    scrollWhenDraggingOnVoid();
	    windowDragScroll();

		ImGui::EndChild();
		error_popup();
		ImGui::EndPopup();
	}
}

static void gamepadPngFileSelected(bool cancelled, std::string path)
{
	if (!cancelled)
		gui_runOnUiThread([path]() {
			vgamepad::loadImage(path);
		});
}

static void gamepadSettingsPopup(const std::shared_ptr<GamepadDevice>& gamepad)
{
	centerNextWindow();
	ImGui::SetNextWindowSize(min(ImGui::GetIO().DisplaySize, ScaledVec2(450.f, 300.f)));

	ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("Gamepad Settings", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_DragScrolling))
	{
		if (ImGui::Button("Done", ScaledVec2(100, 30)))
		{
			gamepad->save_mapping();
			// Update both console and arcade profile/mapping
			int rumblePower = gamepad->get_rumble_power();
			float deadzone = gamepad->get_dead_zone();
			float saturation = gamepad->get_saturation();
			int otherPlatform = settings.platform.isConsole() ? DC_PLATFORM_NAOMI : DC_PLATFORM_DREAMCAST;
			if (!gamepad->find_mapping(otherPlatform))
				if (otherPlatform == DC_PLATFORM_DREAMCAST || !gamepad->find_mapping(DC_PLATFORM_DREAMCAST))
					gamepad->resetMappingToDefault(otherPlatform != DC_PLATFORM_DREAMCAST, true);
			std::shared_ptr<InputMapping> mapping = gamepad->get_input_mapping();
			if (mapping != nullptr)
			{
				if (gamepad->is_rumble_enabled() && rumblePower != mapping->rumblePower) {
					mapping->rumblePower = rumblePower;
					mapping->set_dirty();
				}
				if (gamepad->has_analog_stick())
				{
					if (deadzone != mapping->dead_zone) {
						mapping->dead_zone = deadzone;
						mapping->set_dirty();
					}
					if (saturation != mapping->saturation) {
						mapping->saturation = saturation;
						mapping->set_dirty();
					}
				}
				if (mapping->is_dirty())
					gamepad->save_mapping(otherPlatform);
			}
			gamepad->find_mapping();

			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}
		ImGui::NewLine();
		if (gamepad->is_virtual_gamepad())
		{
			if (gamepad->is_rumble_enabled()) {
				header("Haptic");
				OptionSlider("Power", config::VirtualGamepadVibration, 0, 100, "Haptic feedback power", "%d%%");
			}
			header("View");
			OptionSlider("Transparency", config::VirtualGamepadTransparency, 0, 100, "Virtual gamepad buttons transparency", "%d%%");

#if defined(__ANDROID__) || defined(TARGET_IPHONE)
			vgamepad::ImguiVGamepadTexture tex;
			ImGui::Image(tex.getId(), ScaledVec2(300.f, 150.f), ImVec2(0, 1), ImVec2(1, 0));
#endif
			const char *gamepadPngTitle = "Select a PNG file";
			if (ImGui::Button("Choose Image...", ScaledVec2(150, 30)))
#ifdef __ANDROID__
			{
				if (!hostfs::addStorage(false, false, gamepadPngTitle, gamepadPngFileSelected, "image/png"))
					ImGui::OpenPopup(gamepadPngTitle);
			}
#else
			{
				ImGui::OpenPopup(gamepadPngTitle);
			}
#endif
			ImGui::SameLine();
			if (ImGui::Button("Use Default", ScaledVec2(150, 30)))
				vgamepad::loadImage("");

			select_file_popup(gamepadPngTitle, [](bool cancelled, std::string selection)
				{
					gamepadPngFileSelected(cancelled, selection);
					return true;
				}, true, "png");
		}
		else if (gamepad->is_rumble_enabled())
		{
			header("Rumble");
			int power = gamepad->get_rumble_power();
			ImGui::SetNextItemWidth(uiScaled(300));
			if (ImGui::SliderInt("Power", &power, 0, 100, "%d%%"))
				gamepad->set_rumble_power(power);
			ImGui::SameLine();
			ShowHelpMarker("Rumble power");
		}
		if (gamepad->has_analog_stick())
		{
			header("Thumbsticks");
			int deadzone = std::round(gamepad->get_dead_zone() * 100.f);
			ImGui::SetNextItemWidth(uiScaled(300));
			if (ImGui::SliderInt("Dead zone", &deadzone, 0, 100, "%d%%"))
				gamepad->set_dead_zone(deadzone / 100.f);
			ImGui::SameLine();
			ShowHelpMarker("Minimum deflection to register as input");
			int saturation = std::round(gamepad->get_saturation() * 100.f);
			ImGui::SetNextItemWidth(uiScaled(300));
			if (ImGui::SliderInt("Saturation", &saturation, 50, 200, "%d%%"))
				gamepad->set_saturation(saturation / 100.f);
			ImGui::SameLine();
			ShowHelpMarker("Value sent to the game at 100% thumbstick deflection. "
					"Values greater than 100% will saturate before full deflection of the thumbstick.");
		}
	    scrollWhenDraggingOnVoid();
	    windowDragScroll();
		ImGui::EndPopup();
	}
}

void gui_settings_controls(bool& maple_devices_changed)
{
	header("Physical Devices");
    {
		if (ImGui::BeginTable("physicalDevices", 5, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableSetupColumn("System", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Port", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

			const float portComboWidth = calcComboWidth("None");
			const ImVec4 gray{ 0.5f, 0.5f, 0.5f, 1.f };

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextColored(gray, "System");

			ImGui::TableSetColumnIndex(1);
			ImGui::TextColored(gray, "Name");

			ImGui::TableSetColumnIndex(3);
			ImGui::TextColored(gray, "Port");

			for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
			{
				std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
				if (!gamepad)
					continue;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", gamepad->api_name().c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s", gamepad->name().c_str());

#if defined(USE_DREAMLINK_DEVICES)
				ImGui::TableSetColumnIndex(2);
				DreamLinkGamepad* dreamLinkGamepad = dynamic_cast<DreamLinkGamepad*>(gamepad.get());
				if (dreamLinkGamepad != nullptr) {
					ImGui::Text("DreamLink status: %s", dreamLinkGamepad->dreamLinkStatus());
				}
#endif

				ImGui::TableSetColumnIndex(3);
				char port_name[32];
				snprintf(port_name, sizeof(port_name), "##mapleport%d", i);
				ImguiID _(port_name);
				ImGui::SetNextItemWidth(portComboWidth);
				if (ImGui::BeginCombo(port_name, maple_ports[gamepad->maple_port() + 1]))
				{
					for (int j = -1; j < (int)std::size(maple_ports) - 1; j++)
					{
						bool is_selected = gamepad->maple_port() == j;
						if (ImGui::Selectable(maple_ports[j + 1], &is_selected)) {
							gamepad->set_maple_port(j);
#if defined(USE_DREAMLINK_DEVICES)
							if (dreamLinkGamepad != nullptr) {
								maple_devices_changed = true;
							}
#endif
						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndCombo();
				}

				ImGui::TableSetColumnIndex(4);
				ImGui::SameLine(0, uiScaled(8));
				if (gamepad->remappable() && ImGui::Button("Map"))
				{
					gamepad_port = 0;
					ImGui::OpenPopup("Controller Mapping");
				}

				controller_mapping_popup(gamepad);

#if defined(__ANDROID__) || defined(TARGET_IPHONE)
				if (gamepad->is_virtual_gamepad())
				{
					if (ImGui::Button("Edit Layout"))
					{
						vgamepad::startEditing();
						gui_setState(GuiState::VJoyEdit);
					}
				}
#endif
				if (gamepad->is_rumble_enabled() || gamepad->has_analog_stick()
					|| gamepad->is_virtual_gamepad())
				{
					ImGui::SameLine(0, uiScaled(16));
					if (ImGui::Button("Settings"))
						ImGui::OpenPopup("Gamepad Settings");
					gamepadSettingsPopup(gamepad);
				}
			}
			ImGui::EndTable();
		}
    }

	ImGui::Spacing();
	OptionSlider("Mouse sensitivity", config::MouseSensitivity, 1, 500);
#if defined(_WIN32) && !defined(TARGET_UWP)
	OptionCheckbox("Use Raw Input", config::UseRawInput, "Supports multiple pointing devices (mice, light guns) and keyboards");
#endif
#ifdef USE_DREAMLINK_DEVICES
	{
		DisabledScope scope(game_started);
		OptionCheckbox("Use Physical VMU Memory", config::UsePhysicalVmuMemory,
			"Enables direct read/write access to physical VMU memory via DreamPicoPort/DreamConn. "
			"This is not compatible with load state events.");
	}
#endif

	ImGui::Spacing();
	header("Dreamcast Devices");
    {
		bool is_there_any_xhair = false;
		if (ImGui::BeginTable("dreamcastDevices", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings,
				ImVec2(0, 0), uiScaled(8)))
		{
			const float mainComboWidth = calcComboWidth(maple_device_types[11]); 			// densha de go! controller
			const float expComboWidth = calcComboWidth(maple_expansion_device_types[2]);	// vibration pack

			for (int bus = 0; bus < MAPLE_PORTS; bus++)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Port %c", bus + 'A');

				ImGui::TableSetColumnIndex(1);
				char device_name[32];
				snprintf(device_name, sizeof(device_name), "##device%d", bus);
				float w = ImGui::CalcItemWidth() / 3;
				ImGui::PushItemWidth(w);
				ImGui::SetNextItemWidth(mainComboWidth);
				if (ImGui::BeginCombo(device_name, maple_device_name(config::MapleMainDevices[bus]), ImGuiComboFlags_None))
				{
					for (int i = 0; i < IM_ARRAYSIZE(maple_device_types); i++)
					{
						bool is_selected = config::MapleMainDevices[bus] == maple_device_type_from_index(i);
						if (ImGui::Selectable(maple_device_types[i], &is_selected))
						{
							config::MapleMainDevices[bus] = maple_device_type_from_index(i);
							maple_devices_changed = true;
						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				int port_count = 0;
				switch (config::MapleMainDevices[bus]) {
					case MDT_SegaController:
					case MDT_SegaControllerXL:
						port_count = 2;
						break;
					case MDT_LightGun:
					case MDT_TwinStick:
					case MDT_AsciiStick:
					case MDT_RacingController:
						port_count = 1;
						break;
					default: break;
				}
				for (int port = 0; port < port_count; port++)
				{
					ImGui::TableSetColumnIndex(2 + port);
					snprintf(device_name, sizeof(device_name), "##device%d.%d", bus, port + 1);
					ImguiID _(device_name);
					ImGui::SetNextItemWidth(expComboWidth);
					if (ImGui::BeginCombo(device_name, maple_expansion_device_name(config::MapleExpansionDevices[bus][port]), ImGuiComboFlags_None))
					{
						for (int i = 0; i < IM_ARRAYSIZE(maple_expansion_device_types); i++)
						{
							bool is_selected = config::MapleExpansionDevices[bus][port] == maple_expansion_device_type_from_index(i);
							if (ImGui::Selectable(maple_expansion_device_types[i], &is_selected))
							{
								config::MapleExpansionDevices[bus][port] = maple_expansion_device_type_from_index(i);
								maple_devices_changed = true;
							}
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
				if (config::MapleMainDevices[bus] == MDT_LightGun)
				{
					ImGui::TableSetColumnIndex(3);
					snprintf(device_name, sizeof(device_name), "##device%d.xhair", bus);
					ImguiID _(device_name);
					u32 color = config::CrosshairColor[bus];
					float xhairColor[4] {
						(color & 0xff) / 255.f,
						((color >> 8) & 0xff) / 255.f,
						((color >> 16) & 0xff) / 255.f,
						((color >> 24) & 0xff) / 255.f
					};
					bool colorChanged = ImGui::ColorEdit4("Crosshair color", xhairColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf
							| ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel);
					ImGui::SameLine();
					bool enabled = color != 0;
					if (ImGui::Checkbox("Crosshair", &enabled) || colorChanged)
					{
						if (enabled)
						{
							config::CrosshairColor[bus] = (u8)(std::round(xhairColor[0] * 255.f))
									| ((u8)(std::round(xhairColor[1] * 255.f)) << 8)
									| ((u8)(std::round(xhairColor[2] * 255.f)) << 16)
									| ((u8)(std::round(xhairColor[3] * 255.f)) << 24);
							if (config::CrosshairColor[bus] == 0)
								config::CrosshairColor[bus] = 0xC0FFFFFF;
						}
						else
						{
							config::CrosshairColor[bus] = 0;
						}
					}
					is_there_any_xhair |= enabled;
				}

#ifdef USE_DREAMLINK_DEVICES
				if (port_count > 0)
				{
					ImGui::SameLine();
					ImGui::PushID(bus);
					bool pressed = OptionCheckbox("Use Network Expansion Devices", config::UseNetworkExpansionDevices[bus],
						"Connect to expansion devices such as VMUs over local TCP.");
					ImGui::PopID();

					if (pressed)
						maple_devices_changed = true;
				}
#endif

				ImGui::PopItemWidth();
			}
			ImGui::EndTable();
		}
		{
			DisabledScope scope(!is_there_any_xhair);
			OptionSlider("Crosshair Size", config::CrosshairSize, 10, 100);
		}
		OptionCheckbox("Per Game VMU A1", config::PerGameVmu, "When enabled, each game has its own VMU on port 1 of controller A.");
    }
}
