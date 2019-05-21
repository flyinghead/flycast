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
#include <algorithm>
#include <math.h>
#ifdef _MSC_VER
#include "dirent/dirent.h"
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <dirent.h>
#endif
#include <sys/stat.h>

#include "gui.h"
#include "oslib/oslib.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_if.h"
#include "imgui/imgui.h"
#include "gles/imgui_impl_opengl3.h"
#include "imgui/roboto_medium.h"
#include "gles/gles.h"
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "linux-dist/main.h"	// FIXME for kcode[]
#include "gui_util.h"
#include "gui_android.h"

#include "version.h"
#include "oslib/audiostream.h"


extern void dc_loadstate();
extern void dc_savestate();
extern void dc_stop();
extern void dc_reset();
extern void dc_resume();
extern int dc_start_game(const char *path);
extern void UpdateInputState(u32 port);
extern bool game_started;

extern int screen_width, screen_height;
extern u8 kb_shift; 		// shift keys pressed (bitmask)
extern u8 kb_key[6];		// normal keys pressed
extern s32 mo_x_abs;
extern s32 mo_y_abs;
extern u32 mo_buttons;
extern f32 mo_x_delta;
extern f32 mo_y_delta;
extern f32 mo_wheel_delta;
extern bool renderer_changed;

int screen_dpi = 96;

static bool inited = false;
static float scaling = 1;
GuiState gui_state = Main;
static bool settings_opening;
static bool touch_up;

static void display_vmus();
static void reset_vmus();
static void term_vmus();

void gui_init()
{
	if (inited)
		return;
	inited = true;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.IniFilename = NULL;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

	io.KeyMap[ImGuiKey_Tab] = 0x2B;
	io.KeyMap[ImGuiKey_LeftArrow] = 0x50;
	io.KeyMap[ImGuiKey_RightArrow] = 0x4F;
	io.KeyMap[ImGuiKey_UpArrow] = 0x52;
	io.KeyMap[ImGuiKey_DownArrow] = 0x51;
	io.KeyMap[ImGuiKey_PageUp] = 0x4B;
	io.KeyMap[ImGuiKey_PageDown] = 0x4E;
	io.KeyMap[ImGuiKey_Home] = 0x4A;
	io.KeyMap[ImGuiKey_End] = 0x4D;
	io.KeyMap[ImGuiKey_Insert] = 0x49;
	io.KeyMap[ImGuiKey_Delete] = 0x4C;
	io.KeyMap[ImGuiKey_Backspace] = 0x2A;
	io.KeyMap[ImGuiKey_Space] = 0x2C;
	io.KeyMap[ImGuiKey_Enter] = 0x28;
	io.KeyMap[ImGuiKey_Escape] = 0x29;
	io.KeyMap[ImGuiKey_A] = 0x04;
	io.KeyMap[ImGuiKey_C] = 0x06;
	io.KeyMap[ImGuiKey_V] = 0x19;
	io.KeyMap[ImGuiKey_X] = 0x1B;
	io.KeyMap[ImGuiKey_Y] = 0x1C;
	io.KeyMap[ImGuiKey_Z] = 0x1D;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    ImGui::GetStyle().TabRounding = 0;
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 8);		// from 8,4
    ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 6);	// from 4,4
    //ImGui::GetStyle().WindowRounding = 0;
#ifdef _ANDROID
    ImGui::GetStyle().GrabMinSize = 20.0f;				// from 10
    ImGui::GetStyle().ScrollbarSize = 24.0f;			// from 16
    ImGui::GetStyle().TouchExtraPadding = ImVec2(1, 1);	// from 0,0
#endif

    // Setup Platform/Renderer bindings
    ImGui_ImplOpenGL3_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    scaling = max(1.f, screen_dpi / 100.f * 0.75f);
    if (scaling > 1)
		ImGui::GetStyle().ScaleAllSizes(scaling);

    io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 17 * scaling);
    printf("Screen DPI is %d, size %d x %d. Scaling by %.2f\n", screen_dpi, screen_width, screen_height, scaling);
}

void ImGui_Impl_NewFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::GetIO().DisplaySize.x = screen_width;
	ImGui::GetIO().DisplaySize.y = screen_height;

	ImGuiIO& io = ImGui::GetIO();

	UpdateInputState(0);

	// Read keyboard modifiers inputs
	io.KeyCtrl = (kb_shift & (0x01 | 0x10)) != 0;
	io.KeyShift = (kb_shift & (0x02 | 0x20)) != 0;
	io.KeyAlt = false;
	io.KeySuper = false;

	memset(&io.KeysDown[0], 0, sizeof(io.KeysDown));
	for (int i = 0; i < IM_ARRAYSIZE(kb_key); i++)
		if (kb_key[i] != 0)
			io.KeysDown[kb_key[i]] = true;
		else
			break;
	float scale = screen_height / 480.0f;
	float x_offset = (screen_width - 640.0f * scale) / 2;
	int real_x = mo_x_abs * scale + x_offset;
	int real_y = mo_y_abs * scale;
	if (real_x < 0 || real_x >= screen_width || real_y < 0 || real_y >= screen_height)
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	else
		io.MousePos = ImVec2(real_x, real_y);
#ifdef _ANDROID
	// Put the "mouse" outside the screen one frame after a touch up
	// This avoids buttons and the like to stay selected
	if ((mo_buttons & 0xf) == 0xf)
	{
		if (touch_up)
		{
			io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
			touch_up = false;
		}
		else if (io.MouseDown[0])
			touch_up = true;
	}
#endif
	if (io.WantCaptureMouse)
	{
		io.MouseWheel = -mo_wheel_delta / 16;
		// Reset all relative mouse positions
		mo_x_delta = 0;
		mo_y_delta = 0;
		mo_wheel_delta = 0;
	}
	io.MouseDown[0] = (mo_buttons & (1 << 2)) == 0;
	io.MouseDown[1] = (mo_buttons & (1 << 1)) == 0;
	io.MouseDown[2] = (mo_buttons & (1 << 3)) == 0;
	io.MouseDown[3] = (mo_buttons & (1 << 0)) == 0;

	io.NavInputs[ImGuiNavInput_Activate] = (kcode[0] & DC_BTN_A) == 0;
	io.NavInputs[ImGuiNavInput_Cancel] = (kcode[0] & DC_BTN_B) == 0;
	io.NavInputs[ImGuiNavInput_Input] = (kcode[0] & DC_BTN_X) == 0;
	//io.NavInputs[ImGuiNavInput_Menu] = (kcode[0] & DC_BTN_Y) == 0;
	io.NavInputs[ImGuiNavInput_DpadLeft] = (kcode[0] & DC_DPAD_LEFT) == 0;
	io.NavInputs[ImGuiNavInput_DpadRight] = (kcode[0] & DC_DPAD_RIGHT) == 0;
	io.NavInputs[ImGuiNavInput_DpadUp] = (kcode[0] & DC_DPAD_UP) == 0;
	io.NavInputs[ImGuiNavInput_DpadDown] = (kcode[0] & DC_DPAD_DOWN) == 0;
	io.NavInputs[ImGuiNavInput_LStickLeft] = joyx[0] < 0 ? -(float)joyx[0] / 128 : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickLeft] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickLeft] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickRight] = joyx[0] > 0 ? (float)joyx[0] / 128 : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickRight] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickRight] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickUp] = joyy[0] < 0 ? -(float)joyy[0] / 128.f : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickUp] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickUp] = 0.f;
	io.NavInputs[ImGuiNavInput_LStickDown] = joyy[0] > 0 ? (float)joyy[0] / 128.f : 0.f;
	if (io.NavInputs[ImGuiNavInput_LStickDown] < 0.1f)
		io.NavInputs[ImGuiNavInput_LStickDown] = 0.f;

	if (KeyboardDevice::GetInstance() != NULL)
	{
		std:string input_text = KeyboardDevice::GetInstance()->get_character_input();
		if (io.WantCaptureKeyboard)
			io.AddInputCharactersUTF8(input_text.c_str());
	}
}

static double last_render;
std::vector<float> render_times;

void gui_dosmth(int width, int height)
{
	if (last_render == 0)
	{
		last_render = os_GetSeconds();
		return;
	}
	double new_time = os_GetSeconds();
	render_times.push_back((float)(new_time - last_render));
	if (render_times.size() > 100)
		render_times.erase(render_times.begin());
	last_render = new_time;

	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

    ImGui::PlotLines("Render Times", &render_times[0], render_times.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));

    // Render dear imgui into screen
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
static void ShowHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void gui_open_settings()
{
	if (gui_state == Closed)
	{
		gui_state = Commands;
		settings_opening = true;
		HideOSD();
	}
	else if (gui_state == VJoyEdit)
	{
		gui_state = VJoyEditCommands;
	}
}

static void gui_display_commands()
{
	dc_stop();

	ImGui_Impl_NewFrame();
    ImGui::NewFrame();
    if (!settings_opening)
    	ImGui_ImplOpenGL3_DrawBackground();

    if (!settings.rend.FloatVMUs)
    	// If floating VMUs, they are already visible on the background
    	display_vmus();

    ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

    ImGui::Begin("Reicast", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Columns(2, "buttons", false);
	if (ImGui::Button("Load State", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = ClosedNoResume;
		dc_loadstate();
	}
	ImGui::NextColumn();
	if (ImGui::Button("Save State", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = ClosedNoResume;
		dc_savestate();
	}

	ImGui::NextColumn();
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Resume", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = Closed;
	}

	ImGui::NextColumn();
	if (ImGui::Button("Restart", ImVec2(150 * scaling, 50 * scaling)))
	{
		dc_reset();
		gui_state = Closed;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Exit", ImVec2(150 * scaling, 50 * scaling)))
	{
		// Exit to main menu
		gui_state = Main;
		game_started = false;
		cfgSetVirtual("config", "image", "");
	}

#if 0
	ImGui::NextColumn();
	if (ImGui::Button("RenderDone Int", ImVec2(150 * scaling, 50 * scaling)))
	{
		asic_RaiseInterrupt(holly_RENDER_DONE);
		gui_state = Closed;
	}
#endif
	ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), settings_opening);
    settings_opening = false;
}

const char *maple_device_types[] = { "None", "Sega Controller", "Light Gun", "Keyboard", "Mouse" };
const char *maple_expansion_device_types[] = { "None", "Sega VMU", "Purupuru", "Microphone" };

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

const char *maple_ports[] = { "None", "A", "B", "C", "D" };
const DreamcastKey button_keys[] = { DC_BTN_START, DC_BTN_A, DC_BTN_B, DC_BTN_X, DC_BTN_Y, DC_DPAD_UP, DC_DPAD_DOWN, DC_DPAD_LEFT, DC_DPAD_RIGHT,
		EMU_BTN_MENU, EMU_BTN_ESCAPE, EMU_BTN_TRIGGER_LEFT, EMU_BTN_TRIGGER_RIGHT,
		DC_BTN_C, DC_BTN_D, DC_BTN_Z, DC_DPAD2_UP, DC_DPAD2_DOWN, DC_DPAD2_LEFT, DC_DPAD2_RIGHT };
const char *button_names[] = { "Start", "A", "B", "X", "Y", "DPad Up", "DPad Down", "DPad Left", "DPad Right",
		"Menu", "Exit", "Left Trigger", "Right Trigger",
		"C", "D", "Z", "Right Dpad Up", "Right DPad Down", "Right DPad Left", "Right DPad Right" };
const DreamcastKey axis_keys[] = { DC_AXIS_X, DC_AXIS_Y, DC_AXIS_LT, DC_AXIS_RT, EMU_AXIS_DPAD1_X, EMU_AXIS_DPAD1_Y, EMU_AXIS_DPAD2_X, EMU_AXIS_DPAD2_Y };
const char *axis_names[] = { "Stick X", "Stick Y", "Left Trigger", "Right Trigger", "DPad X", "DPad Y", "Right DPad X", "Right DPad Y" };

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

static std::shared_ptr<GamepadDevice> mapped_device;
static u32 mapped_code;
static double map_start_time;

static void input_detected(u32 code)
{
	mapped_code = code;
}

static void detect_input_popup(int index, bool analog)
{
	ImVec2 padding = ImVec2(20 * scaling, 20 * scaling);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	if (ImGui::BeginPopupModal(analog ? "Map Axis" : "Map Button", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("Waiting for %s '%s'...", analog ? "axis" : "button", analog ? axis_names[index] : button_names[index]);
		double now = os_GetSeconds();
		ImGui::Text("Time out in %d s", (int)(5 - (now - map_start_time)));
		if (mapped_code != -1)
		{
			InputMapping *input_mapping = mapped_device->get_input_mapping();
			if (input_mapping != NULL)
			{
				if (analog)
				{
					u32 previous_mapping = input_mapping->get_axis_code(axis_keys[index]);
					bool inverted = false;
					if (previous_mapping != -1)
						inverted = input_mapping->get_axis_inverted(previous_mapping);
					// FIXME Allow inverted to be set
					input_mapping->set_axis(axis_keys[index], mapped_code, inverted);
				}
				else
					input_mapping->set_button(button_keys[index], mapped_code);
			}
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
		}
		else if (now - map_start_time >= 5)
		{
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
}

static void controller_mapping_popup(std::shared_ptr<GamepadDevice> gamepad)
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("Controller Mapping", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		const float width = screen_width / 2;
		const float col0_width = ImGui::CalcTextSize("Right DPad Downxxx").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
		const float col1_width = width
				- ImGui::GetStyle().GrabMinSize
				- (col0_width + ImGui::GetStyle().ItemSpacing.x)
				- (ImGui::CalcTextSize("Map").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);

		InputMapping *input_mapping = gamepad->get_input_mapping();
		if (input_mapping == NULL || ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
		{
			ImGui::CloseCurrentPopup();
			gamepad->save_mapping();
		}
		ImGui::SetItemDefaultFocus();

		char key_id[32];
		ImGui::BeginGroup();
		ImGui::Text("  Buttons  ");

		ImGui::BeginChildFrame(ImGui::GetID("buttons"), ImVec2(width, 0), ImGuiWindowFlags_None);
		ImGui::Columns(3, "bindings", false);
		ImGui::SetColumnWidth(0, col0_width);
		ImGui::SetColumnWidth(1, col1_width);
		for (int j = 0; j < ARRAY_SIZE(button_keys); j++)
		{
			sprintf(key_id, "key_id%d", j);
			ImGui::PushID(key_id);
			ImGui::Text("%s", button_names[j]);
			ImGui::NextColumn();
			u32 code = input_mapping->get_button_code(button_keys[j]);
			if (code != -1)
				ImGui::Text("%d", code);
			ImGui::NextColumn();
			if (ImGui::Button("Map"))
			{
				map_start_time = os_GetSeconds();
				ImGui::OpenPopup("Map Button");
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detect_btn_input(&input_detected);
			}
			detect_input_popup(j, false);
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::EndChildFrame();
		ImGui::EndGroup();

		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("  Analog Axes  ");
		ImGui::BeginChildFrame(ImGui::GetID("analog"), ImVec2(width, 0), ImGuiWindowFlags_None);
		ImGui::Columns(3, "bindings", false);
		ImGui::SetColumnWidth(0, col0_width);
		ImGui::SetColumnWidth(1, col1_width);

		for (int j = 0; j < ARRAY_SIZE(axis_keys); j++)
		{
			sprintf(key_id, "axis_id%d", j);
			ImGui::PushID(key_id);
			ImGui::Text("%s", axis_names[j]);
			ImGui::NextColumn();
			u32 code = input_mapping->get_axis_code(axis_keys[j]);
			if (code != -1)
				ImGui::Text("%d", code);
			ImGui::NextColumn();
			if (ImGui::Button("Map"))
			{
				map_start_time = os_GetSeconds();
				ImGui::OpenPopup("Map Axis");
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detect_axis_input(&input_detected);
			}
			detect_input_popup(j, true);
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::EndChildFrame();
		ImGui::EndGroup();
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

static std::string error_msg;

static void error_popup()
{
	if (!error_msg.empty())
	{
		if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
			ImGui::TextWrapped("%s", error_msg.c_str());
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
			float currentwidth = ImGui::GetContentRegionAvailWidth();
			ImGui::SetCursorPosX((currentwidth - 80.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
			if (ImGui::Button("OK", ImVec2(80.f * scaling, 0.f)))
			{
				error_msg.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::PopStyleVar();
			ImGui::EndPopup();
		}
		ImGui::OpenPopup("Error");
	}
}

static bool game_list_done;		// Set to false to refresh the game list

void directory_selected_callback(bool cancelled, std::string selection)
{
	if (!cancelled)
	{
		settings.dreamcast.ContentPath.push_back(selection);
		game_list_done = false;
	}
}

static void gui_display_settings()
{
	static bool maple_devices_changed;

	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	int dynarec_enabled = settings.dynarec.Enable;
	u32 renderer = settings.pvr.rend;

    if (!settings_opening)
    	ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Settings", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

    if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
    {
    	if (game_started)
    		gui_state = Commands;
    	else
    		gui_state = Main;
    	if (maple_devices_changed)
    	{
    		maple_devices_changed = false;
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
    		maple_ReconnectDevices();
    		reset_vmus();
#endif
    	}
       	SaveSettings();
    }
	if (game_started)
	{
	    ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, normal_padding.y));
		if (cfgHasGameSpecificConfig())
		{
			if (ImGui::Button("Delete Game Config", ImVec2(0, 30 * scaling)))
			{
				cfgDeleteGameSpecificConfig();
				InitSettings();
				LoadSettings(false);
			}
		}
		else
		{
			if (ImGui::Button("Make Game Config", ImVec2(0, 30 * scaling)))
				cfgMakeGameSpecificConfig();
		}
	    ImGui::PopStyleVar();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 6 * scaling));		// from 4, 3

    if (ImGui::BeginTabBar("settings", ImGuiTabBarFlags_NoTooltip))
    {
		if (ImGui::BeginTabItem("General"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			const char *languages[] = { "Japanese", "English", "German", "French", "Spanish", "Italian", "Default" };
			if (ImGui::BeginCombo("Language", languages[settings.dreamcast.language], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(languages); i++)
				{
					bool is_selected = settings.dreamcast.language == i;
					if (ImGui::Selectable(languages[i], &is_selected))
						settings.dreamcast.language = i;
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
            ImGui::SameLine();
            ShowHelpMarker("The language as configured in the Dreamcast BIOS");

			const char *broadcast[] = { "NTSC", "PAL", "PAL/M", "PAL/N", "Default" };
			if (ImGui::BeginCombo("Broadcast", broadcast[settings.dreamcast.broadcast], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(broadcast); i++)
				{
					bool is_selected = settings.dreamcast.broadcast == i;
					if (ImGui::Selectable(broadcast[i], &is_selected))
						settings.dreamcast.broadcast = i;
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
            ImGui::SameLine();
            ShowHelpMarker("TV broadcasting standard for non-VGA modes");

			const char *region[] = { "Japan", "USA", "Europe", "Default" };
			if (ImGui::BeginCombo("Region", region[settings.dreamcast.region], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(region); i++)
				{
					bool is_selected = settings.dreamcast.region == i;
					if (ImGui::Selectable(region[i], &is_selected))
						settings.dreamcast.region = i;
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
            ImGui::SameLine();
            ShowHelpMarker("BIOS region");

			const char *cable[] = { "VGA", "RGB Component", "TV Composite" };
			if (ImGui::BeginCombo("Cable", cable[settings.dreamcast.cable == 0 ? 0 : settings.dreamcast.cable - 1], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(cable); i++)
				{
					bool is_selected = i == 0 ? settings.dreamcast.cable <= 1 : settings.dreamcast.cable - 1 == i;
					if (ImGui::Selectable(cable[i], &is_selected))
						settings.dreamcast.cable = i == 0 ? 0 : i + 1;
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
            ImGui::SameLine();
            ShowHelpMarker("Video connection type");

            static int current_item;
            std::vector<const char *> paths;
            for (auto path : settings.dreamcast.ContentPath)
            	paths.push_back(path.c_str());

            ImVec2 size;
            size.x = 0.0f;
            size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f)
            				* (settings.dreamcast.ContentPath.size() + 1) ;//+ ImGui::GetStyle().FramePadding.y * 2.f;

            if (ImGui::ListBoxHeader("Content Location", size))
            {
            	int to_delete = -1;
                for (int i = 0; i < settings.dreamcast.ContentPath.size(); i++)
                {
                	ImGui::PushID(settings.dreamcast.ContentPath[i].c_str());
                    ImGui::AlignTextToFramePadding();
                	ImGui::Text("%s", settings.dreamcast.ContentPath[i].c_str());
                	ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("X").x - ImGui::GetStyle().FramePadding.x);
                	if (ImGui::Button("X"))
                		to_delete = i;
                	ImGui::PopID();
                }
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24 * scaling, 3 * scaling));
            	if (ImGui::Button("Add"))
            		ImGui::OpenPopup("Select Directory");
        		select_directory_popup("Select Directory", scaling, &directory_selected_callback);
        		ImGui::PopStyleVar();

        		ImGui::ListBoxFooter();
            	if (to_delete >= 0)
            	{
            		settings.dreamcast.ContentPath.erase(settings.dreamcast.ContentPath.begin() + to_delete);
        			game_list_done = false;
            	}
            }
            ImGui::SameLine();
            ShowHelpMarker("The directories where your games are stored");

            if (ImGui::ListBoxHeader("Home Directory", 1))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_config_path("").c_str());
#ifdef _ANDROID
                ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Change").x - ImGui::GetStyle().FramePadding.x);
                if (ImGui::Button("Change"))
                	gui_state = Onboarding;
#endif
                ImGui::ListBoxFooter();
            }
            ImGui::SameLine();
            ShowHelpMarker("The directory where reicast saves configuration files and VMUs. BIOS files should be in the data subdirectory");

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Controls"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST || DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
		    if (ImGui::CollapsingHeader("Dreamcast Devices", ImGuiTreeNodeFlags_DefaultOpen))
		    {
				for (int bus = 0; bus < MAPLE_PORTS; bus++)
				{
					ImGui::Text("Device %c", bus + 'A');
					ImGui::SameLine();
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
					char device_name[32];
					sprintf(device_name, "##device%d", bus);
					float w = ImGui::CalcItemWidth() / 3;
					ImGui::PushItemWidth(w);
					if (ImGui::BeginCombo(device_name, maple_device_name((MapleDeviceType)settings.input.maple_devices[bus]), ImGuiComboFlags_None))
					{
						for (int i = 0; i < IM_ARRAYSIZE(maple_device_types); i++)
						{
							bool is_selected = settings.input.maple_devices[bus] == maple_device_type_from_index(i);
							if (ImGui::Selectable(maple_device_types[i], &is_selected))
							{
								settings.input.maple_devices[bus] = maple_device_type_from_index(i);
								maple_devices_changed = true;
							}
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					int port_count = settings.input.maple_devices[bus] == MDT_SegaController ? 2 : settings.input.maple_devices[bus] == MDT_LightGun ? 1 : 0;
					for (int port = 0; port < port_count; port++)
					{
						ImGui::SameLine();
						sprintf(device_name, "##device%d.%d", bus, port + 1);
						ImGui::PushID(device_name);
						if (ImGui::BeginCombo(device_name, maple_expansion_device_name((MapleDeviceType)settings.input.maple_expansion_devices[bus][port]), ImGuiComboFlags_None))
						{
							for (int i = 0; i < IM_ARRAYSIZE(maple_expansion_device_types); i++)
							{
								bool is_selected = settings.input.maple_expansion_devices[bus][port] == maple_expansion_device_type_from_index(i);
								if (ImGui::Selectable(maple_expansion_device_types[i], &is_selected))
								{
									settings.input.maple_expansion_devices[bus][port] = maple_expansion_device_type_from_index(i);
									maple_devices_changed = true;
								}
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						ImGui::PopID();
					}
					ImGui::PopItemWidth();
#elif DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
					if (MapleDevices[bus][5] != NULL)
						ImGui::Text("%s", maple_device_name(MapleDevices[bus][5]->get_device_type()));
#endif
				}
				ImGui::Spacing();
		    }
#endif
		    if (ImGui::CollapsingHeader("Physical Devices", ImGuiTreeNodeFlags_DefaultOpen))
		    {
				ImGui::Columns(4, "renderers", false);
				ImGui::Text("System");
				ImGui::SetColumnWidth(-1, ImGui::CalcTextSize("System").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);
				ImGui::NextColumn();
				ImGui::Text("Name");
				ImGui::NextColumn();
				ImGui::Text("Port");
				ImGui::SetColumnWidth(-1, ImGui::CalcTextSize("None").x * 1.6f + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight()
					+ ImGui::GetStyle().ItemInnerSpacing.x	+ ImGui::GetStyle().ItemSpacing.x);
				ImGui::NextColumn();
				ImGui::NextColumn();
				for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
				{
					std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
					if (!gamepad)
						continue;
					ImGui::Text("%s", gamepad->api_name().c_str());
					ImGui::NextColumn();
					ImGui::Text("%s", gamepad->name().c_str());
					ImGui::NextColumn();
					char port_name[32];
					sprintf(port_name, "##mapleport%d", i);
					ImGui::PushID(port_name);
					if (ImGui::BeginCombo(port_name, maple_ports[gamepad->maple_port() + 1]))
					{
						for (int j = -1; j < MAPLE_PORTS; j++)
						{
							bool is_selected = gamepad->maple_port() == j;
							if (ImGui::Selectable(maple_ports[j + 1], &is_selected))
								gamepad->set_maple_port(j);
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}

						ImGui::EndCombo();
					}
					ImGui::NextColumn();
					if (gamepad->remappable() && ImGui::Button("Map"))
						ImGui::OpenPopup("Controller Mapping");

					controller_mapping_popup(gamepad);

#ifdef _ANDROID
					if (gamepad->is_virtual_gamepad())
					{
						if (ImGui::Button("Edit"))
						{
							vjoy_start_editing();
							gui_state = VJoyEdit;
						}
						ImGui::SameLine();
						ImGui::SliderInt("Haptic", &settings.input.VirtualGamepadVibration, 0, 60);
					}
#endif
					ImGui::NextColumn();
					ImGui::PopID();
				}
		    }
	    	ImGui::Columns(1, NULL, false);

	    	ImGui::Spacing();
			ImGui::SliderInt("Mouse sensitivity", (int *)&settings.input.MouseSensitivity, 1, 500);

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Video"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
#if !defined(GLES) && HOST_OS != OS_DARWIN
		    if (!gl.is_gles && gl.gl_major >= 4 && ImGui::CollapsingHeader("Transparent Sorting", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	ImGui::Columns(2, "renderers", false);
		    	ImGui::RadioButton("Per Triangle", (int *)&settings.pvr.rend, 0);
	            ImGui::SameLine();
	            ShowHelpMarker("Sort transparent polygons per triangle. Fast but may produce graphical glitches");
		    	ImGui::NextColumn();
		    	ImGui::RadioButton("Per Pixel", (int *)&settings.pvr.rend, 3);
	            ImGui::SameLine();
	            ShowHelpMarker("Sort transparent polygons per pixel. Slower but accurate");
		    	ImGui::Columns(1, NULL, false);
		    }
#endif
		    if (ImGui::CollapsingHeader("Rendering Options", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	ImGui::Checkbox("Synchronous Rendering", &settings.pvr.SynchronousRender);
	            ImGui::SameLine();
	            ShowHelpMarker("Reduce frame skipping by pausing the CPU when possible. Recommended for most platforms");
		    	ImGui::Checkbox("Clipping", &settings.rend.Clipping);
	            ImGui::SameLine();
	            ShowHelpMarker("Enable clipping. May produce graphical errors when disabled");
		    	ImGui::Checkbox("Shadows", &settings.rend.ModifierVolumes);
	            ImGui::SameLine();
	            ShowHelpMarker("Enable modifier volumes, usually used for shadows");
		    	ImGui::Checkbox("Fog", &settings.rend.Fog);
	            ImGui::SameLine();
	            ShowHelpMarker("Enable fog effects");
		    	ImGui::Checkbox("Widescreen", &settings.rend.WideScreen);
	            ImGui::SameLine();
	            ShowHelpMarker("Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas");
		    	ImGui::Checkbox("Show FPS Counter", &settings.rend.ShowFPS);
	            ImGui::SameLine();
	            ShowHelpMarker("Show on-screen frame/sec counter");
		    	ImGui::Checkbox("Show VMU in game", &settings.rend.FloatVMUs);
	            ImGui::SameLine();
	            ShowHelpMarker("Show the VMU LCD screens while in game");
		    	ImGui::Checkbox("Rotate screen 90°", &settings.rend.Rotate90);
	            ImGui::SameLine();
	            ShowHelpMarker("Rotate the screen 90° counterclockwise");
		    	ImGui::SliderInt("Scaling", (int *)&settings.rend.ScreenScaling, 1, 100);
	            ImGui::SameLine();
	            ShowHelpMarker("Downscaling factor relative to native screen resolution. Higher is better");
		    	ImGui::SliderInt("Horizontal Stretching", (int *)&settings.rend.ScreenStretching, 100, 150);
	            ImGui::SameLine();
	            ShowHelpMarker("Stretch the screen horizontally");
		    	ImGui::SliderInt("Frame Skipping", (int *)&settings.pvr.ta_skip, 0, 6);
	            ImGui::SameLine();
	            ShowHelpMarker("Number of frames to skip between two actually rendered frames");
		    }
		    if (ImGui::CollapsingHeader("Render to Texture", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	ImGui::Checkbox("Copy to VRAM", &settings.rend.RenderToTextureBuffer);
	            ImGui::SameLine();
	            ShowHelpMarker("Copy rendered-to textures back to VRAM. Slower but accurate");
		    	ImGui::SliderInt("Render to Texture Upscaling", (int *)&settings.rend.RenderToTextureUpscale, 1, 8);
	            ImGui::SameLine();
	            ShowHelpMarker("Upscale rendered-to textures. Should be the same as the screen or window upscale ratio, or lower for slow platforms");
		    }
		    if (ImGui::CollapsingHeader("Texture Upscaling", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	ImGui::SliderInt("Texture Upscaling", (int *)&settings.rend.TextureUpscale, 1, 8);
	            ImGui::SameLine();
	            ShowHelpMarker("Upscale textures with the xBRZ algorithm. Only on fast platforms and for certain games");
		    	ImGui::SliderInt("Upscaled Texture Max Size", (int *)&settings.rend.MaxFilteredTextureSize, 8, 1024);
	            ImGui::SameLine();
	            ShowHelpMarker("Textures larger than this dimension squared will not be upscaled");
		    	ImGui::SliderInt("Max Threads", (int *)&settings.pvr.MaxThreads, 1, 8);
	            ImGui::SameLine();
	            ShowHelpMarker("Maximum number of threads to use for texture upscaling. Recommended: number of physical cores minus one");
		    	ImGui::Checkbox("Load Custom Textures", &settings.rend.CustomTextures);
	            ImGui::SameLine();
	            ShowHelpMarker("Load custom/high-res textures from data/textures/<game id>");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Audio"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			ImGui::Checkbox("Disable Sound", &settings.aica.NoSound);
            ImGui::SameLine();
            ShowHelpMarker("Disable the emulator sound output");

			ImGui::Checkbox("Enable DSP", &settings.aica.NoBatch);
            ImGui::SameLine();
            ShowHelpMarker("Enable the Dreamcast Digital Sound Processor. Only recommended on fast and arm64 platforms");
			ImGui::Checkbox("Limit FPS", &settings.aica.LimitFPS);
            ImGui::SameLine();
            ShowHelpMarker("Use the sound output to limit the speed of the emulator. Recommended in most cases");

			audiobackend_t* backend = NULL;;
			std::string backend_name = settings.audio.backend;
			if (backend_name != "auto" && backend_name != "none")
			{
				backend = GetAudioBackend(settings.audio.backend);
				if (backend != NULL)
					backend_name = backend->slug;
			}

			SortAudioBackends();

			audiobackend_t* current_backend = backend;
			if (ImGui::BeginCombo("Audio Backend", backend_name.c_str(), ImGuiComboFlags_None))
			{
				bool is_selected = (settings.audio.backend == "auto");
				if (ImGui::Selectable("auto", &is_selected))
					settings.audio.backend = "auto";
				ImGui::SameLine(); ImGui::Text("-");
				ImGui::SameLine(); ImGui::Text("Autoselect audio backend");

				is_selected = (settings.audio.backend == "none");
				if (ImGui::Selectable("none", &is_selected))
					settings.audio.backend = "none";
				ImGui::SameLine(); ImGui::Text("-");
				ImGui::SameLine(); ImGui::Text("No audio backend");

				for (int i = 0; i < GetAudioBackendCount(); i++)
				{
					backend = GetAudioBackend(i);
					is_selected = (settings.audio.backend == backend->slug);

					if (is_selected)
						current_backend = backend;

					if (ImGui::Selectable(backend->slug.c_str(), &is_selected))
						settings.audio.backend = backend->slug;
					ImGui::SameLine(); ImGui::Text("-");
					ImGui::SameLine(); ImGui::Text(backend->name.c_str());
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
            ImGui::SameLine();
            ShowHelpMarker("The audio backend to use");

			if (current_backend != NULL && current_backend->get_options != NULL)
			{
				// get backend specific options
				int option_count;
				audio_option_t* options = current_backend->get_options(&option_count);

				// initialize options if not already done
				std::map<std::string, std::string>* cfg_entries = &settings.audio.options[current_backend->slug];
				bool populate_entries = (cfg_entries->size() == 0);

				for (int o = 0; o < option_count; o++)
				{
					std::string value;
					if (populate_entries)
					{
						value = cfgLoadStr(current_backend->slug.c_str(), options->cfg_name.c_str(), "");
						(*cfg_entries)[options->cfg_name] = value;
					}
					value = (*cfg_entries)[options->cfg_name];

					if (options->type == integer)
					{
						int val = stoi(value);
						ImGui::SliderInt(options->caption.c_str(), &val, options->min_value, options->max_value);
						(*cfg_entries)[options->cfg_name] = to_string(val);
					}
					else if (options->type == checkbox)
					{
						bool check = (value == "1");
						ImGui::Checkbox(options->caption.c_str(), &check);
						std::string cur = check ? "1" : "0";
						(*cfg_entries)[options->cfg_name] = cur;
					}
					else if (options->type == ::list)
					{
						if (ImGui::BeginCombo(options->caption.c_str(), value.c_str(), ImGuiComboFlags_None))
						{
							bool is_selected = false;
							std::vector<std::string> list_items = options->list_callback();
							for (std::vector<std::string>::iterator it = list_items.begin() ; it != list_items.end(); ++it)
							{
								std::string cur = (std::string)*it;
								is_selected = (value == cur);
								if (ImGui::Selectable(cur.c_str(), &is_selected))
								{
									(*cfg_entries)[options->cfg_name] = cur;
								}

								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
					else {
						printf("Unknown option\n");
					}

					options++;
				}
			}

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Advanced"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    if (ImGui::CollapsingHeader("CPU Mode", ImGuiTreeNodeFlags_DefaultOpen))
		    {
				ImGui::Columns(2, "cpu_modes", false);
				ImGui::RadioButton("Dynarec", &dynarec_enabled, 1);
	            ImGui::SameLine();
	            ShowHelpMarker("Use the dynamic recompiler. Recommended in most cases");
				ImGui::NextColumn();
				ImGui::RadioButton("Interpreter", &dynarec_enabled, 0);
	            ImGui::SameLine();
	            ShowHelpMarker("Use the interpreter. Very slow but may help in case of a dynarec problem");
				ImGui::Columns(1, NULL, false);
		    }
		    if (ImGui::CollapsingHeader("Dynarec Options", dynarec_enabled ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
		    {
		    	ImGui::Checkbox("Safe Mode", &settings.dynarec.safemode);
	            ImGui::SameLine();
	            ShowHelpMarker("Do not optimize integer division. Recommended");
		    	ImGui::Checkbox("Unstable Optimizations", &settings.dynarec.unstable_opt);
	            ImGui::SameLine();
	            ShowHelpMarker("Enable unsafe optimizations. Will cause crash or environmental disaster");
		    	ImGui::Checkbox("Idle Skip", &settings.dynarec.idleskip);
	            ImGui::SameLine();
	            ShowHelpMarker("Skip wait loops. Recommended");
				ImGui::PushItemWidth(ImGui::CalcTextSize("Largeenough").x);
	            const char *preview = settings.dynarec.SmcCheckLevel == NoCheck ? "Faster" : settings.dynarec.SmcCheckLevel == FastCheck ? "Fast" : "Full";
				if (ImGui::BeginCombo("SMC Checks", preview	, ImGuiComboFlags_None))
				{
					bool is_selected = settings.dynarec.SmcCheckLevel == NoCheck;
					if (ImGui::Selectable("Faster", &is_selected))
						settings.dynarec.SmcCheckLevel = NoCheck;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
					is_selected = settings.dynarec.SmcCheckLevel == FastCheck;
					if (ImGui::Selectable("Fast", &is_selected))
						settings.dynarec.SmcCheckLevel = FastCheck;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
					is_selected = settings.dynarec.SmcCheckLevel == FullCheck;
					if (ImGui::Selectable("Full", &is_selected))
						settings.dynarec.SmcCheckLevel = FullCheck;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
					ImGui::EndCombo();
				}
	            ImGui::SameLine();
	            ShowHelpMarker("How to detect self-modifying code. Full check recommended");
		    }
		    if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
		    {
#ifndef _ANDROID
				ImGui::Checkbox("Serial Console", &settings.debug.SerialConsole);
	            ImGui::SameLine();
	            ShowHelpMarker("Dump the Dreamcast serial console to stdout");
#endif
				ImGui::Checkbox("Dump Textures", &settings.rend.DumpTextures);
	            ImGui::SameLine();
	            ShowHelpMarker("Dump all textures into data/texdump/<game id>");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("About"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    if (ImGui::CollapsingHeader("Reicast", ImGuiTreeNodeFlags_DefaultOpen))
		    {
				ImGui::Text("Version: %s", REICAST_VERSION);
				ImGui::Text("Git Hash: %s", GIT_HASH);
				ImGui::Text("Build Date: %s", BUILD_DATE);
				ImGui::Text("Target: %s",
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
						"Dreamcast"
#elif DC_PLATFORM == DC_PLATFORM_NAOMI
						"Naomi"
#elif DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
						"Atomiswave"
#else
						"Unknown"
#endif
						);
		    }
		    if (ImGui::CollapsingHeader("Platform", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	ImGui::Text("CPU: %s",
#if HOST_CPU == CPU_X86
					"x86"
#elif HOST_CPU == CPU_ARM
					"ARM"
#elif HOST_CPU == CPU_MIPS
					"MIPS"
#elif HOST_CPU == CPU_X64
					"x86/64"
#elif HOST_CPU == CPU_GENERIC
					"Generic"
#elif HOST_CPU == CPU_ARM64
					"ARM64"
#else
					"Unknown"
#endif
						);
		    	ImGui::Text("Operating System: %s",
#ifdef _ANDROID
					"Android"
#elif HOST_OS == OS_LINUX
					"Linux"
#elif HOST_OS == OS_DARWIN
#if TARGET_IPHONE
		    		"iOS"
#else
					"OSX"
#endif
#elif HOST_OS == OS_WINDOWS
					"Windows"
#else
					"Unknown"
#endif
						);
		    }
		    if (ImGui::CollapsingHeader("Open GL", ImGuiTreeNodeFlags_DefaultOpen))
		    {
				ImGui::Text("Renderer: %s", (const char *)glGetString(GL_RENDERER));
				ImGui::Text("Version: %s", (const char *)glGetString(GL_VERSION));
		    }
#ifdef _ANDROID
		    ImGui::Separator();
		    if (ImGui::Button("Send Logs")) {
		    	void android_send_logs();
		    	android_send_logs();
		    }
#endif
			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);

   	if (renderer != settings.pvr.rend)
   		renderer_changed = true;
   	settings.dynarec.Enable = (bool)dynarec_enabled;
}

#ifdef _ANDROID
static std::string current_library_path("/storage/emulated/0/Download");
#else
static std::string current_library_path("/home/raph/RetroPie/roms/dreamcast/");
#endif
struct GameMedia {
	std::string name;
	std::string path;
};

static bool operator<(const GameMedia &left, const GameMedia &right)
{
	return left.name < right.name;
}

static void add_game_directory(const std::string& path, std::vector<GameMedia>& game_list)
{
	//printf("Exploring %s\n", path.c_str());
	DIR *dir = opendir(path.c_str());
	if (dir == NULL)
		return;
	while (true)
	{
		struct dirent *entry = readdir(dir);
		if (entry == NULL)
			break;
		std:string name(entry->d_name);
		if (name == "." || name == "..")
			continue;
		std::string child_path = path + "/" + name;
		bool is_dir = false;
#ifndef _WIN32
		if (entry->d_type == DT_DIR)
			is_dir = true;
		if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
#endif
		{
			struct stat st;
			if (stat(child_path.c_str(), &st) != 0)
				continue;
			if (S_ISDIR(st.st_mode))
				is_dir = true;
		}
		if (is_dir)
		{
			add_game_directory(child_path, game_list);
		}
		else
		{
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
			if (name.size() >= 4)
			{
				std::string extension = name.substr(name.size() - 4).c_str();
				//printf("  found game %s ext %s\n", entry->d_name, extension.c_str());
				if (stricmp(extension.c_str(), ".cdi") && stricmp(extension.c_str(), ".gdi") && stricmp(extension.c_str(), ".chd") && stricmp(extension.c_str(), ".cue"))
					continue;
				game_list.push_back({ name, child_path });
			}
#else
			std::string::size_type dotpos = name.find_last_of(".");
			if (dotpos == std::string::npos || dotpos == name.size() - 1)
				continue;
			std::string extension = name.substr(dotpos);
			if (stricmp(extension.c_str(), ".zip") && stricmp(extension.c_str(), ".7z") && stricmp(extension.c_str(), ".bin")
					 && stricmp(extension.c_str(), ".lst") && stricmp(extension.c_str(), ".dat"))
				continue;
			game_list.push_back({ name, child_path });
#endif
		}
	}
	closedir(dir);
}

static std::vector<GameMedia> game_list;

static void fetch_game_list()
{
	if (game_list_done)
		return;
	game_list.clear();
	for (auto path : settings.dreamcast.ContentPath)
		add_game_directory(path, game_list);
	std::stable_sort(game_list.begin(), game_list.end());
	game_list_done = true;
}

static void gui_display_demo()
{
	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	ImGui::ShowDemoWindow();
	ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);
}

static void gui_start_game(const std::string& path)
{
	int rc = dc_start_game(path.empty() ? NULL : path.c_str());
	if (rc != 0)
	{
		gui_state = Main;
		game_started = false;
		cfgSetVirtual("config", "image", "");
		switch (rc) {
		case -3:
			error_msg = "Audio/video initialization failed";
			break;
		case -5:
			error_msg = "Cannot find BIOS files";
			break;
		case -6:
			error_msg = "Cannot load NAOMI rom or BIOS";
			break;
		default:
			break;
		}
	}
}

static void gui_display_content()
{
	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("##main", NULL, ImGuiWindowFlags_NoDecoration);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 8 * scaling));		// from 8, 4
    ImGui::AlignTextToFramePadding();
    ImGui::Text("GAMES");

    static ImGuiTextFilter filter;
    if (KeyboardDevice::GetInstance() != NULL)
    {
        ImGui::SameLine(0, 32 * scaling);
    	filter.Draw("Filter");
    }

    ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f /*+ ImGui::GetStyle().ItemSpacing.x*/);
    if (ImGui::Button("Settings"))//, ImVec2(0, 30 * scaling)))
    	gui_state = Settings;
    ImGui::PopStyleVar();

    fetch_game_list();

	// Only if Filter and Settings aren't focused... ImGui::SetNextWindowFocus();
	ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, 0), true);
    {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scaling, 20 * scaling));		// from 8, 4

#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
		ImGui::PushID("bios");
		if (ImGui::Selectable("Dreamcast BIOS"))
		{
			gui_state = ClosedNoResume;
			cfgSetVirtual("config", "image", "");
			gui_start_game("");
		}
		ImGui::PopID();
#endif

        for (auto game : game_list)
        	if (filter.PassFilter(game.name.c_str()))
        	{
    			ImGui::PushID(game.path.c_str());
				if (ImGui::Selectable(game.name.c_str()))
				{
					gui_state = ClosedNoResume;
					gui_start_game(game.path);
				}
				ImGui::PopID();
        	}
        ImGui::PopStyleVar();
    }
	ImGui::EndChild();
	ImGui::End();
    ImGui::PopStyleVar();

	error_popup();

	ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);
}

void systemdir_selected_callback(bool cancelled, std::string selection)
{
	if (!cancelled)
	{
		set_user_config_dir(selection);
		set_user_data_dir(selection);
		if (cfgOpen())
		{
			LoadSettings(false);
			gui_state = Main;
			if (settings.dreamcast.ContentPath.empty())
				settings.dreamcast.ContentPath.push_back(selection);
			SaveSettings();
		}
	}
}

void gui_display_onboarding()
{
	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	ImGui::OpenPopup("Select System Directory");
	select_directory_popup("Select System Directory", scaling, &systemdir_selected_callback);

	ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);
}

void gui_display_ui()
{
	switch (gui_state)
	{
	case Settings:
		gui_display_settings();
		break;
	case Commands:
		gui_display_commands();
		break;
	case Main:
		//gui_display_demo();
		{
			std::string game_file = cfgLoadStr("config", "image", "");
			if (!game_file.empty())
			{
				gui_state = ClosedNoResume;
				gui_start_game(game_file);
			}
			else
				gui_display_content();
		}
		break;
	case Closed:
	case ClosedNoResume:
		break;
	case Onboarding:
		gui_display_onboarding();
		break;
	case VJoyEdit:
		break;
	case VJoyEditCommands:
#ifdef _ANDROID
		gui_display_vjoy_commands(screen_width, screen_height, scaling);
#endif
		break;
	}

	if (gui_state == Closed)
		dc_resume();
	else if (gui_state == ClosedNoResume)
		gui_state = Closed;
}

static float LastFPSTime;
static int lastFrameCount = 0;
static float fps = -1;

static std::string osd_message;
static double osd_message_end;

void gui_display_notification(const char *msg, int duration)
{
	osd_message = msg;
	osd_message_end = os_GetSeconds() + (double)duration / 1000.0;
}

static std::string getFPSNotification()
{
	if (settings.rend.ShowFPS)
	{
		double now = os_GetSeconds();
		if (now - LastFPSTime >= 1.0) {
			fps = (FrameCount - lastFrameCount) / (now - LastFPSTime);
			LastFPSTime = now;
			lastFrameCount = FrameCount;
		}
		if (fps >= 0.f && fps < 9999.f) {
			char text[32];
			snprintf(text, sizeof(text), "F:%.1f", fps);

			return std::string(text);
		}
	}
	return std::string("");
}

void gui_display_osd()
{
	if (gui_state == VJoyEdit)
		return;
	double now = os_GetSeconds();
	if (!osd_message.empty())
	{
		if (now >= osd_message_end)
			osd_message.clear();
	}
	std::string message;
	if (osd_message.empty())
	{
		message = getFPSNotification();
	}
	else
		message = osd_message;

	if (!message.empty() || settings.rend.FloatVMUs)
	{
		ImGui_Impl_NewFrame();
		ImGui::NewFrame();

		if (!message.empty())
		{
			ImGui::SetNextWindowBgAlpha(0);
			ImGui::SetNextWindowPos(ImVec2(0, screen_height), ImGuiCond_Always, ImVec2(0.f, 1.f));	// Lower left corner

			ImGui::Begin("##osd", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav
					| ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
			ImGui::SetWindowFontScale(1.5);
			ImGui::TextColored(ImVec4(1, 1, 0, 0.7), "%s", message.c_str());
			ImGui::End();
		}
		if (settings.rend.FloatVMUs)
			display_vmus();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}

void gui_open_onboarding()
{
	gui_state = Onboarding;
}

void gui_term()
{
	inited = false;
	term_vmus();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui::DestroyContext();
}

int msgboxf(const wchar* text, unsigned int type, ...) {
    va_list args;

    wchar temp[2048];
    va_start(args, type);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    printf("%s\n", temp);

    gui_display_notification(temp, 2000);

    return 1;
}

extern bool subfolders_read;

void gui_refresh_files()
{
	game_list_done = false;
	subfolders_read = false;
}

#define VMU_WIDTH (70 * 48 * scaling / 32)
#define VMU_HEIGHT (70 * scaling)
#define VMU_PADDING (8 * scaling)
static u32 vmu_lcd_data[8][48 * 32];
static bool vmu_lcd_status[8];
static ImTextureID vmu_lcd_tex_ids[8];

void push_vmu_screen(int bus_id, int bus_port, u8* buffer)
{
	int vmu_id = bus_id * 2 + bus_port;
	if (vmu_id < 0 || vmu_id >= ARRAY_SIZE(vmu_lcd_data))
		return;
	u32 *p = &vmu_lcd_data[vmu_id][0];
	for (int i = 0; i < ARRAY_SIZE(vmu_lcd_data[vmu_id]); i++, buffer++)
		*p++ = *buffer != 0 ? 0xFFFFFFFFu : 0xFF000000u;
	vmu_lcd_status[vmu_id] = true;
}

static const int vmu_coords[8][2] = {
		{ 0 , 0 },
		{ 0 , 0 },
		{ 1 , 0 },
		{ 1 , 0 },
		{ 0 , 1 },
		{ 0 , 1 },
		{ 1 , 1 },
		{ 1 , 1 },
};

static void display_vmus()
{
	if (!game_started)
		return;
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));

    ImGui::Begin("vmu-window", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs
    		| ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
	for (int i = 0; i < 8; i++)
	{
		if (!vmu_lcd_status[i])
			continue;

		if (vmu_lcd_tex_ids[i] != (ImTextureID)0)
			ImGui_ImplOpenGL3_DeleteVmuTexture(vmu_lcd_tex_ids[i]);
		vmu_lcd_tex_ids[i] = ImGui_ImplOpenGL3_CreateVmuTexture(vmu_lcd_data[i]);

	    int x = vmu_coords[i][0];
	    int y = vmu_coords[i][1];
	    ImVec2 pos;
	    if (x == 0)
	    	pos.x = VMU_PADDING;
	    else
	    	pos.x = screen_width - VMU_WIDTH - VMU_PADDING;
	    if (y == 0)
	    {
	    	pos.y = VMU_PADDING;
	    	if (i & 1)
	    		pos.y += VMU_HEIGHT + VMU_PADDING;
	    }
	    else
	    {
	    	pos.y = screen_height - VMU_HEIGHT - VMU_PADDING;
	    	if (i & 1)
	    		pos.y -= VMU_HEIGHT + VMU_PADDING;
	    }
	    ImVec2 pos_b(pos.x + VMU_WIDTH, pos.y + VMU_HEIGHT);
		ImGui::GetWindowDrawList()->AddImage(vmu_lcd_tex_ids[i], pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), 0xC0ffffff);
	}
    ImGui::End();
}

static void reset_vmus()
{
	for (int i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
		vmu_lcd_status[i] = false;
}

static void term_vmus()
{
	for (int i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
	{
		if (vmu_lcd_tex_ids[i] != (ImTextureID)0)
		{
			ImGui_ImplOpenGL3_DeleteVmuTexture(vmu_lcd_tex_ids[i]);
			vmu_lcd_tex_ids[i] = (ImTextureID)0;
		}
	}
}
