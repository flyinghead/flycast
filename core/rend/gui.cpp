/*
	Copyright 2019 flyinghead

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

#include <mutex>
#include "gui.h"
#include "osd.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_if.h"
#include "hw/maple/maple_devs.h"
#include "hw/naomi/naomi_cart.h"
#include "imgui/imgui.h"
#include "gles/imgui_impl_opengl3.h"
#include "imgui/roboto_medium.h"
#include "network/naomi_network.h"
#include "wsi/context.h"
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "gui_util.h"
#include "gui_android.h"
#include "game_scanner.h"
#include "version.h"
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "imgread/common.h"
#include "log/LogManager.h"
#include "emulator.h"
#include "rend/mainui.h"

extern void UpdateInputState();
static bool game_started;

extern int screen_width, screen_height;
extern u8 kb_shift; 		// shift keys pressed (bitmask)
extern u8 kb_key[6];		// normal keys pressed

int screen_dpi = 96;

static bool inited = false;
float scaling = 1;
GuiState gui_state = GuiState::Main;
static bool settings_opening;
#ifdef __ANDROID__
static bool touch_up;
#endif
static std::string error_msg;
static std::string osd_message;
static double osd_message_end;
static std::mutex osd_message_mutex;

static void display_vmus();
static void reset_vmus();
static void term_vmus();
static void displayCrosshairs();

GameScanner scanner;

float gui_get_scaling()
{
	return scaling;
}

static void emuEventCallback(Event event)
{
	switch (event)
	{
	case Event::Resume:
		game_started = true;
		break;
	case Event::Start:
		if (config::AutoSavestate && settings.imgread.ImagePath[0] != '\0')
			dc_loadstate();
		break;
	case Event::Terminate:
		if (config::AutoSavestate && settings.imgread.ImagePath[0] != '\0')
			dc_savestate();
		break;
	default:
		break;
	}
}

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
#ifdef __ANDROID__
    ImGui::GetStyle().GrabMinSize = 20.0f;				// from 10
    ImGui::GetStyle().ScrollbarSize = 24.0f;			// from 16
    ImGui::GetStyle().TouchExtraPadding = ImVec2(1, 1);	// from 0,0
#endif

    // Setup Platform/Renderer bindings
    if (config::RendererType.isOpenGL())
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
#if !(defined(_WIN32) || defined(__APPLE__))
    scaling = std::max(1.f, screen_dpi / 100.f * 0.75f);
#endif
    if (scaling > 1)
		ImGui::GetStyle().ScaleAllSizes(scaling);

    static const ImWchar ranges[] =
    {
    	0x0020, 0xFFFF, // All chars
        0,
    };

    io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 17.f * scaling, nullptr, ranges);
    ImFontConfig font_cfg;
    font_cfg.MergeMode = true;
#ifdef _WIN32
    u32 cp = GetACP();
    std::string fontDir = std::string(nowide::getenv("SYSTEMROOT")) + "\\Fonts\\";
    switch (cp)
    {
    case 932:	// Japanese
		{
			font_cfg.FontNo = 2;	// UIGothic
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "msgothic.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
			font_cfg.FontNo = 2;	// Meiryo UI
			if (font == nullptr)
				io.Fonts->AddFontFromFileTTF((fontDir + "Meiryo.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
		}
		break;
    case 949:	// Korean
		{
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Malgun.ttf").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
			if (font == nullptr)
			{
				font_cfg.FontNo = 2;	// Dotum
				io.Fonts->AddFontFromFileTTF((fontDir + "Gulim.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
			}
		}
    	break;
    case 950:	// Traditional Chinese
		{
			font_cfg.FontNo = 1; // Microsoft JhengHei UI Regular
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Msjh.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
			font_cfg.FontNo = 0;
			if (font == nullptr)
				io.Fonts->AddFontFromFileTTF((fontDir + "MSJH.ttf").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
		}
    	break;
    case 936:	// Simplified Chinese
		io.Fonts->AddFontFromFileTTF((fontDir + "Simsun.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseSimplifiedOfficial());
    	break;
    default:
    	break;
    }
#elif __APPLE__
    std::string fontDir = std::string("/System/Library/Fonts/");
    
    extern std::string os_Locale();
    std::string locale = os_Locale();
    
    if (locale.find("ja") == 0)             // Japanese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "ヒラギノ角ゴシック W4.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesJapanese());
    }
    else if (locale.find("ko") == 0)       // Korean
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "AppleSDGothicNeo.ttc").c_str(), 17.f * scaling, &font_cfg, io.Fonts->GetGlyphRangesKorean());
    }
    else if (locale.find("zh-Hant") == 0)  // Traditional Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseTraditionalOfficial());
    }
    else if (locale.find("zh-Hans") == 0)  // Simplified Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), 17.f * scaling, &font_cfg, GetGlyphRangesChineseSimplifiedOfficial());
    }
    // TODO linux, Android...
#endif
    INFO_LOG(RENDERER, "Screen DPI is %d, size %d x %d. Scaling by %.2f", screen_dpi, screen_width, screen_height, scaling);

    EventManager::listen(Event::Resume, emuEventCallback);
    EventManager::listen(Event::Start, emuEventCallback);
    EventManager::listen(Event::Terminate, emuEventCallback);
}

void ImGui_Impl_NewFrame()
{
	if (config::RendererType.isOpenGL())
		ImGui_ImplOpenGL3_NewFrame();
	ImGui::GetIO().DisplaySize.x = screen_width;
	ImGui::GetIO().DisplaySize.y = screen_height;

	ImGuiIO& io = ImGui::GetIO();

	UpdateInputState();

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
	if (mo_x_phy < 0 || mo_x_phy >= screen_width || mo_y_phy < 0 || mo_y_phy >= screen_height)
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	else
		io.MousePos = ImVec2(mo_x_phy, mo_y_phy);
#ifdef __ANDROID__
	// Put the "mouse" outside the screen one frame after a touch up
	// This avoids buttons and the like to stay selected
	if ((mo_buttons[0] & 0xf) == 0xf)
	{
		if (touch_up)
			io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
		else if (io.MouseDown[0])
			touch_up = true;
	}
	else
		touch_up = false;
#endif
	if (io.WantCaptureMouse)
	{
		io.MouseWheel = -mo_wheel_delta[0] / 16;
		// Reset all relative mouse positions
		mo_x_delta[0] = 0;
		mo_y_delta[0] = 0;
		mo_wheel_delta[0] = 0;
	}
	io.MouseDown[0] = (mo_buttons[0] & (1 << 2)) == 0;
	io.MouseDown[1] = (mo_buttons[0] & (1 << 1)) == 0;
	io.MouseDown[2] = (mo_buttons[0] & (1 << 3)) == 0;
	io.MouseDown[3] = (mo_buttons[0] & (1 << 0)) == 0;

	io.NavInputs[ImGuiNavInput_Activate] = (kcode[0] & DC_BTN_A) == 0;
	io.NavInputs[ImGuiNavInput_Cancel] = (kcode[0] & DC_BTN_B) == 0;
	io.NavInputs[ImGuiNavInput_Input] = (kcode[0] & DC_BTN_X) == 0;
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
		const std::string input_text = KeyboardDevice::GetInstance()->get_character_input();
		if (io.WantCaptureKeyboard)
		{
			for (const u8 b : input_text)
				// Cheap ISO Latin-1 to UTF-8 conversion
			    if (b < 0x80)
			    	io.AddInputCharacter(b);
			    else
			    	io.AddInputCharacter((0xc2 + (b > 0xbf)) | ((b & 0x3f) + 0x80) << 8);
		}
	}
}

#if 0
#include "oslib/timeseries.h"
TimeSeries renderTimes;
TimeSeries vblankTimes;

void gui_plot_render_time(int width, int height)
{
	std::vector<float> v = renderTimes.data();
	ImGui::PlotLines("Render Times", v.data(), v.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));
	ImGui::Text("StdDev: %.1f%%", renderTimes.stddev() * 100.f / 0.01666666667f);
	v = vblankTimes.data();
	ImGui::PlotLines("VBlank", v.data(), v.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));
	ImGui::Text("StdDev: %.1f%%", vblankTimes.stddev() * 100.f / 0.01666666667f);
}
#endif

void gui_open_settings()
{
	if (gui_state == GuiState::Closed)
	{
		gui_state = GuiState::Commands;
		settings_opening = true;
		HideOSD();
	}
	else if (gui_state == GuiState::VJoyEdit)
	{
		gui_state = GuiState::VJoyEditCommands;
	}
	else if (gui_state == GuiState::Loading)
	{
		dc_cancel_load();
		gui_state = GuiState::Main;
	}
	else if (gui_state == GuiState::Commands)
	{
		gui_state = GuiState::Closed;
		dc_resume();
	}
}

static void gui_start_game(const std::string& path)
{
	scanner.stop();
	gui_state = GuiState::Loading;
	static std::string path_copy;
	path_copy = path;	// path may be a local var

	dc_load_game(path.empty() ? NULL : path_copy.c_str());
}

static void gui_display_commands()
{
	dc_stop();

	ImGui_Impl_NewFrame();
    ImGui::NewFrame();
    if (!settings_opening && config::RendererType.isOpenGL())
    	ImGui_ImplOpenGL3_DrawBackground();

    if (!config::FloatVMUs)
    	// If floating VMUs, they are already visible on the background
    	display_vmus();

    ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

    ImGui::Begin("##commands", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Columns(2, "buttons", false);
	if (settings.imgread.ImagePath[0] == '\0')
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if (ImGui::Button("Load State", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Closed;
		dc_loadstate();
	}
	ImGui::NextColumn();
	if (ImGui::Button("Save State", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Closed;
		dc_savestate();
	}
	if (settings.imgread.ImagePath[0] == '\0')
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

	ImGui::NextColumn();
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Resume", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Closed;
	}

	ImGui::NextColumn();
	const char *disk_label = libGDR_GetDiscType() == Open ? "Insert Disk" : "Eject Disk";
	if (ImGui::Button(disk_label, ImVec2(150 * scaling, 50 * scaling)))
	{
		if (libGDR_GetDiscType() == Open)
		{
			gui_state = GuiState::SelectDisk;
		}
		else
		{
			DiscOpenLid();
			gui_state = GuiState::Closed;
		}
	}
	ImGui::NextColumn();
	if (ImGui::Button("Exit", ImVec2(150 * scaling, 50 * scaling)))
	{
		// Exit to main menu
		dc_term_game();
		gui_state = GuiState::Main;
		game_started = false;
		settings.imgread.ImagePath[0] = '\0';
	}

	ImGui::End();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), settings_opening);
    settings_opening = false;
}

const char *maple_device_types[] = { "None", "Sega Controller", "Light Gun", "Keyboard", "Mouse", "Twin Stick", "Ascii Stick" };
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
	case MDT_TwinStick:
		return maple_device_types[5];
	case MDT_AsciiStick:
		return maple_device_types[6];
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

const char *maple_ports[] = { "None", "A", "B", "C", "D", "All" };
const DreamcastKey button_keys[] = {
		DC_BTN_START, DC_BTN_A, DC_BTN_B, DC_BTN_X, DC_BTN_Y, DC_DPAD_UP, DC_DPAD_DOWN, DC_DPAD_LEFT, DC_DPAD_RIGHT,
		EMU_BTN_MENU, EMU_BTN_ESCAPE, EMU_BTN_FFORWARD, EMU_BTN_TRIGGER_LEFT, EMU_BTN_TRIGGER_RIGHT,
		DC_BTN_C, DC_BTN_D, DC_BTN_Z, DC_DPAD2_UP, DC_DPAD2_DOWN, DC_DPAD2_LEFT, DC_DPAD2_RIGHT,
		DC_BTN_RELOAD,
		EMU_BTN_ANA_UP, EMU_BTN_ANA_DOWN, EMU_BTN_ANA_LEFT, EMU_BTN_ANA_RIGHT
};
const char *button_names[] = {
		"Start", "A", "B", "X", "Y", "DPad Up", "DPad Down", "DPad Left", "DPad Right",
		"Menu", "Exit", "Fast-forward", "Left Trigger", "Right Trigger",
		"C", "D", "Z", "Right Dpad Up", "Right DPad Down", "Right DPad Left", "Right DPad Right",
		"Reload",
		"Left Stick Up", "Left Stick Down", "Left Stick Left", "Left Stick Right"
};
const char *arcade_button_names[] = {
		"Start", "Button 1", "Button 2", "Button 3", "Button 4", "Up", "Down", "Left", "Right",
		"Menu", "Exit", "Fast-forward", "N/A", "N/A",
		"Service", "Coin", "Test", "Button 5", "Button 6", "Button 7", "Button 8",
		"Reload",
		"N/A", "N/A", "N/A", "N/A"
};
const DreamcastKey axis_keys[] = {
		DC_AXIS_X, DC_AXIS_Y, DC_AXIS_LT, DC_AXIS_RT, DC_AXIS_X2, DC_AXIS_Y2, EMU_AXIS_DPAD1_X, EMU_AXIS_DPAD1_Y,
		EMU_AXIS_DPAD2_X, EMU_AXIS_DPAD2_Y, EMU_AXIS_BTN_START, EMU_AXIS_BTN_A, EMU_AXIS_BTN_B, EMU_AXIS_BTN_X, EMU_AXIS_BTN_Y,
		EMU_AXIS_BTN_C, EMU_AXIS_BTN_D, EMU_AXIS_BTN_Z, EMU_AXIS_DPAD2_UP, EMU_AXIS_DPAD2_DOWN, EMU_AXIS_DPAD2_LEFT, EMU_AXIS_DPAD2_RIGHT
};
const char *axis_names[] = {
		"Left Stick X", "Left Stick Y", "Left Trigger", "Right Trigger", "Right Stick X", "Right Stick Y", "DPad X", "DPad Y",
		"Right DPad X", "Right DPad Y", "Start", "A", "B", "X", "Y",
		"C", "D", "Z", "N/A", "N/A", "N/A", "N/A"
};
const char *arcade_axis_names[] = {
		"Left Stick X", "Left Stick Y", "Left Trigger", "Right Trigger", "Right Stick X", "Right Stick Y", "DPad X", "DPad Y",
		"Right DPad X", "Right DPad Y", "Start", "Button 1", "Button 2", "Button 3", "Button 4",
		"Service", "Coin", "Test", "Button 5", "Button 6", "Button 7", "Button 8"
};
static_assert(ARRAY_SIZE(button_keys) == ARRAY_SIZE(button_names), "invalid size");
static_assert(ARRAY_SIZE(button_keys) == ARRAY_SIZE(arcade_button_names), "invalid size");
static_assert(ARRAY_SIZE(axis_keys) == ARRAY_SIZE(axis_names), "invalid size");
static_assert(ARRAY_SIZE(axis_keys) == ARRAY_SIZE(arcade_axis_names), "invalid size");

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
static bool arcade_button_mode;
static u32 gamepad_port;

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
		ImGui::Text("Waiting for %s '%s'...", analog ? "axis" : "button",
				analog ? arcade_button_mode ? arcade_axis_names[index] : axis_names[index]
						: arcade_button_mode ? arcade_button_names[index] : button_names[index]);
		double now = os_GetSeconds();
		ImGui::Text("Time out in %d s", (int)(5 - (now - map_start_time)));
		if (mapped_code != (u32)-1)
		{
			std::shared_ptr<InputMapping> input_mapping = mapped_device->get_input_mapping();
			if (input_mapping != NULL)
			{
				if (analog)
				{
					u32 previous_mapping = input_mapping->get_axis_code(gamepad_port, axis_keys[index]);
					bool inverted = false;
					if (previous_mapping != (u32)-1)
						inverted = input_mapping->get_axis_inverted(gamepad_port, previous_mapping);
					// FIXME Allow inverted to be set
					input_mapping->set_axis(gamepad_port, axis_keys[index], mapped_code, inverted);
				}
				else
					input_mapping->set_button(gamepad_port, button_keys[index], mapped_code);
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

static void controller_mapping_popup(const std::shared_ptr<GamepadDevice>& gamepad)
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("Controller Mapping", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		const float width = screen_width / 2;
		const float col_width = (width
				- ImGui::GetStyle().GrabMinSize
				- (0 + ImGui::GetStyle().ItemSpacing.x)
				- (ImGui::CalcTextSize("Map").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x)) / 2;

		std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
		if (input_mapping == NULL || ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
		{
			ImGui::CloseCurrentPopup();
			gamepad->save_mapping();
		}
		ImGui::SetItemDefaultFocus();

		if (gamepad->maple_port() == MAPLE_PORTS)
		{
			ImGui::SameLine();
			float w = ImGui::CalcItemWidth();
			ImGui::PushItemWidth(w / 2);
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
			ImGui::PopItemWidth();
		}
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Arcade button names").x
				- ImGui::GetStyle().FramePadding.x * 3.0f - ImGui::GetStyle().ItemSpacing.x);
		ImGui::Checkbox("Arcade button names", &arcade_button_mode);

		char key_id[32];
		ImGui::BeginGroup();
		ImGui::Text("  Buttons  ");

		ImGui::BeginChildFrame(ImGui::GetID("buttons"), ImVec2(width, 0), ImGuiWindowFlags_None);
		ImGui::Columns(3, "bindings", false);
		ImGui::SetColumnWidth(0, col_width);
		ImGui::SetColumnWidth(1, col_width);
		for (u32 j = 0; j < ARRAY_SIZE(button_keys); j++)
		{
			sprintf(key_id, "key_id%d", j);
			ImGui::PushID(key_id);

			const char *btn_name = arcade_button_mode ? arcade_button_names[j] : button_names[j];
			const char *game_btn_name = GetCurrentGameButtonName(button_keys[j]);
			if (game_btn_name != nullptr)
				ImGui::Text("%s - %s", btn_name, game_btn_name);
			else
				ImGui::Text("%s", btn_name);

			ImGui::NextColumn();
			u32 code = input_mapping->get_button_code(gamepad_port, button_keys[j]);
			if (code != (u32)-1)
			{
				const char *label = gamepad->get_button_name(code);
				if (label != nullptr)
					ImGui::Text("%s", label);
				else
					ImGui::Text("[%d]", code);
			}
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
		ImGui::SetColumnWidth(0, col_width);
		ImGui::SetColumnWidth(1, col_width);

		for (u32 j = 0; j < ARRAY_SIZE(axis_keys); j++)
		{
			sprintf(key_id, "axis_id%d", j);
			ImGui::PushID(key_id);

			const char *axis_name = arcade_button_mode ? arcade_axis_names[j] : axis_names[j];
			const char *game_axis_name = GetCurrentGameAxisName(axis_keys[j]);
			if (game_axis_name != nullptr)
				ImGui::Text("%s - %s", axis_name, game_axis_name);
			else
				ImGui::Text("%s", axis_name);

			ImGui::NextColumn();
			u32 code = input_mapping->get_axis_code(gamepad_port, axis_keys[j]);
			if (code != (u32)-1)
			{
				const char *label = gamepad->get_axis_name(code);
				if (label != nullptr)
					ImGui::Text("%s", label);
				else
					ImGui::Text("[%d]", code);
			}
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

static void error_popup()
{
	if (!error_msg.empty())
	{
		ImGui::OpenPopup("Error");
		if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
			ImGui::TextWrapped("%s", error_msg.c_str());
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
			float currentwidth = ImGui::GetContentRegionAvail().x;
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
	}
}

static void contentpath_warning_popup()
{
    static bool show_contentpath_selection;

    if (scanner.content_path_looks_incorrect)
    {
        ImGui::OpenPopup("Incorrect Content Location?");
        if (ImGui::BeginPopupModal("Incorrect Content Location?", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
            ImGui::TextWrapped("  Scanned %d folders but no game can be found!  ", scanner.empty_folders_scanned);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
            float currentwidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x - 55.f * scaling);
            if (ImGui::Button("Reselect", ImVec2(100.f * scaling, 0.f)))
            {
            	scanner.content_path_looks_incorrect = false;
                ImGui::CloseCurrentPopup();
                show_contentpath_selection = true;
            }
            
            ImGui::SameLine();
            ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x + 55.f * scaling);
            if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f)))
            {
            	scanner.content_path_looks_incorrect = false;
                ImGui::CloseCurrentPopup();
                scanner.stop();
                config::ContentPath.get().clear();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::PopStyleVar();
            ImGui::EndPopup();
        }
    }
    if (show_contentpath_selection)
    {
        scanner.stop();
        ImGui::OpenPopup("Select Directory");
        select_directory_popup("Select Directory", scaling, [](bool cancelled, std::string selection)
        {
            show_contentpath_selection = false;
            if (!cancelled)
            {
            	config::ContentPath.get().clear();
                config::ContentPath.get().push_back(selection);
            }
            scanner.refresh();
        });
    }
}

void directory_selected_callback(bool cancelled, std::string selection)
{
	if (!cancelled)
	{
		scanner.stop();
		config::ContentPath.get().push_back(selection);
		scanner.refresh();
	}
}

static void gui_display_settings()
{
	static bool maple_devices_changed;

	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	RenderType pvr_rend = config::RendererType;
	bool vulkan = !config::RendererType.isOpenGL();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Settings", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

    if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
    {
    	if (game_started)
    		gui_state = GuiState::Commands;
    	else
    		gui_state = GuiState::Main;
    	if (maple_devices_changed)
    	{
    		maple_devices_changed = false;
    		if (game_started && settings.platform.system == DC_PLATFORM_DREAMCAST)
    		{
    			maple_ReconnectDevices();
    			reset_vmus();
    		}
    	}
       	SaveSettings();
    }
	if (game_started)
	{
	    ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, normal_padding.y));
		if (config::Settings::instance().hasPerGameConfig())
		{
			if (ImGui::Button("Delete Game Config", ImVec2(0, 30 * scaling)))
			{
				config::Settings::instance().setPerGameConfig(false);
				config::Settings::instance().load(false);
				LoadGameSpecificSettings();
			}
		}
		else
		{
			if (ImGui::Button("Make Game Config", ImVec2(0, 30 * scaling)))
				config::Settings::instance().setPerGameConfig(true);
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
			OptionComboBox("Language", config::Language, languages, ARRAY_SIZE(languages),
				"The language as configured in the Dreamcast BIOS");

			const char *broadcast[] = { "NTSC", "PAL", "PAL/M", "PAL/N", "Default" };
			OptionComboBox("Broadcast", config::Broadcast, broadcast, ARRAY_SIZE(broadcast),
					"TV broadcasting standard for non-VGA modes");

			const char *region[] = { "Japan", "USA", "Europe", "Default" };
			OptionComboBox("Region", config::Region, region, ARRAY_SIZE(region),
						"BIOS region");

			const char *cable[] = { "VGA", "RGB Component", "TV Composite" };
			if (config::Cable.isReadOnly())
			{
		        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::BeginCombo("Cable", cable[config::Cable == 0 ? 0 : config::Cable - 1], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(cable); i++)
				{
					bool is_selected = i == 0 ? config::Cable <= 1 : config::Cable - 1 == i;
					if (ImGui::Selectable(cable[i], &is_selected))
						config::Cable = i == 0 ? 0 : i + 1;
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (config::Cable.isReadOnly())
			{
		        ImGui::PopItemFlag();
		        ImGui::PopStyleVar();
			}
            ImGui::SameLine();
            ShowHelpMarker("Video connection type");

            ImVec2 size;
            size.x = 0.0f;
            size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f)
            				* (config::ContentPath.get().size() + 1) ;//+ ImGui::GetStyle().FramePadding.y * 2.f;

            if (ImGui::ListBoxHeader("Content Location", size))
            {
            	int to_delete = -1;
                for (u32 i = 0; i < config::ContentPath.get().size(); i++)
                {
                	ImGui::PushID(config::ContentPath.get()[i].c_str());
                    ImGui::AlignTextToFramePadding();
                	ImGui::Text("%s", config::ContentPath.get()[i].c_str());
                	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("X").x - ImGui::GetStyle().FramePadding.x);
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
            		scanner.stop();
            		config::ContentPath.get().erase(config::ContentPath.get().begin() + to_delete);
        			scanner.refresh();
            	}
            }
            ImGui::SameLine();
            ShowHelpMarker("The directories where your games are stored");

            if (ImGui::ListBoxHeader("Home Directory", 1))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_config_path("").c_str());
#ifdef __ANDROID__
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Change").x - ImGui::GetStyle().FramePadding.x);
                if (ImGui::Button("Change"))
                	gui_state = GuiState::Onboarding;
#endif
                ImGui::ListBoxFooter();
            }
            ImGui::SameLine();
            ShowHelpMarker("The directory where Flycast saves configuration files and VMUs. BIOS files should be in a subfolder named \"data\"");
			if (OptionCheckbox("Hide Legacy Naomi Roms", config::HideLegacyNaomiRoms,
					"Hide .bin, .dat and .lst files from the content browser"))
				scanner.refresh();
			OptionCheckbox("Auto load/save state", config::AutoSavestate,
					"Automatically save the state of the game when stopping and load it at start up.");

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Controls"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    if (ImGui::CollapsingHeader("Dreamcast Devices", ImGuiTreeNodeFlags_DefaultOpen))
		    {
				for (int bus = 0; bus < MAPLE_PORTS; bus++)
				{
					ImGui::Text("Device %c", bus + 'A');
					ImGui::SameLine();
					char device_name[32];
					sprintf(device_name, "##device%d", bus);
					float w = ImGui::CalcItemWidth() / 3;
					ImGui::PushItemWidth(w);
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
					int port_count = config::MapleMainDevices[bus] == MDT_SegaController ? 2
							: config::MapleMainDevices[bus] == MDT_LightGun || config::MapleMainDevices[bus] == MDT_TwinStick || config::MapleMainDevices[bus] == MDT_AsciiStick ? 1
							: 0;
					for (int port = 0; port < port_count; port++)
					{
						ImGui::SameLine();
						sprintf(device_name, "##device%d.%d", bus, port + 1);
						ImGui::PushID(device_name);
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
						ImGui::PopID();
					}
					if (config::MapleMainDevices[bus] == MDT_LightGun)
					{
						ImGui::SameLine();
						sprintf(device_name, "##device%d.xhair", bus);
						ImGui::PushID(device_name);
						u32 color = config::CrosshairColor[bus];
						float xhairColor[4] {
							(color & 0xff) / 255.f,
							((color >> 8) & 0xff) / 255.f,
							((color >> 16) & 0xff) / 255.f,
							((color >> 24) & 0xff) / 255.f
						};
						ImGui::ColorEdit4("Crosshair color", xhairColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf
								| ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel);
						ImGui::SameLine();
						bool enabled = color != 0;
						ImGui::Checkbox("Crosshair", &enabled);
						if (enabled)
						{
							config::CrosshairColor[bus] = (u8)(xhairColor[0] * 255.f)
									| ((u8)(xhairColor[1] * 255.f) << 8)
									| ((u8)(xhairColor[2] * 255.f) << 16)
									| ((u8)(xhairColor[3] * 255.f) << 24);
							if (config::CrosshairColor[bus] == 0)
								config::CrosshairColor[bus] = 0xC0FFFFFF;
						}
						else
						{
							config::CrosshairColor[bus] = 0;
						}
						ImGui::PopID();
					}
					ImGui::PopItemWidth();
				}
				ImGui::Spacing();
		    }
		    if (ImGui::CollapsingHeader("Physical Devices", ImGuiTreeNodeFlags_DefaultOpen))
		    {
				ImGui::Columns(4, "physicalDevices", false);
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
						for (int j = -1; j < (int)ARRAY_SIZE(maple_ports) - 1; j++)
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
					{
						gamepad_port = 0;
						ImGui::OpenPopup("Controller Mapping");
					}

					controller_mapping_popup(gamepad);

#ifdef __ANDROID__
					if (gamepad->is_virtual_gamepad())
					{
						if (ImGui::Button("Edit"))
						{
							vjoy_start_editing();
							gui_state = GuiState::VJoyEdit;
						}
						ImGui::SameLine();
						OptionSlider("Haptic", config::VirtualGamepadVibration, 0, 60);
					}
#endif
					ImGui::NextColumn();
					ImGui::PopID();
				}
		    }
	    	ImGui::Columns(1, NULL, false);

	    	ImGui::Spacing();
	    	OptionSlider("Mouse sensitivity", config::MouseSensitivity, 1, 500);

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Video"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
#if !defined(__APPLE__)
			bool has_per_pixel = false;
			if (!vulkan)
				has_per_pixel = !theGLContext.IsGLES() && theGLContext.GetMajorVersion() >= 4;
#ifdef USE_VULKAN
			else
				has_per_pixel = VulkanContext::Instance()->SupportsFragmentShaderStoresAndAtomics();
#endif
#else
			bool has_per_pixel = false;
#endif
		    if (ImGui::CollapsingHeader("Transparent Sorting", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	int renderer = (pvr_rend == RenderType::OpenGL_OIT || pvr_rend == RenderType::Vulkan_OIT) ? 2 : config::PerStripSorting ? 1 : 0;
		    	ImGui::Columns(has_per_pixel ? 3 : 2, "renderers", false);
		    	ImGui::RadioButton("Per Triangle", &renderer, 0);
	            ImGui::SameLine();
	            ShowHelpMarker("Sort transparent polygons per triangle. Fast but may produce graphical glitches");
            	ImGui::NextColumn();
		    	ImGui::RadioButton("Per Strip", &renderer, 1);
	            ImGui::SameLine();
	            ShowHelpMarker("Sort transparent polygons per strip. Faster but may produce graphical glitches");
	            if (has_per_pixel)
	            {
	            	ImGui::NextColumn();
	            	ImGui::RadioButton("Per Pixel", &renderer, 2);
	            	ImGui::SameLine();
	            	ShowHelpMarker("Sort transparent polygons per pixel. Slower but accurate");
	            }
		    	ImGui::Columns(1, NULL, false);
		    	switch (renderer)
		    	{
		    	case 0:
		    		if (!vulkan)
		    			pvr_rend = RenderType::OpenGL;	// regular Open GL
		    		else
		    			pvr_rend = RenderType::Vulkan;	// regular Vulkan
		    		config::PerStripSorting.set(false);
		    		break;
		    	case 1:
		    		if (!vulkan)
		    			pvr_rend = RenderType::OpenGL;
		    		else
		    			pvr_rend = RenderType::Vulkan;
		    		config::PerStripSorting.set(true);
		    		break;
		    	case 2:
		    		if (!vulkan)
		    			pvr_rend = RenderType::OpenGL_OIT;
		    		else
		    			pvr_rend = RenderType::Vulkan_OIT;
		    		break;
		    	}
		    }
		    if (ImGui::CollapsingHeader("Rendering Options", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	ImGui::Text("Automatic Frame Skipping:");
		    	ImGui::Columns(3, "autoskip", false);
		    	OptionRadioButton("Disabled", config::AutoSkipFrame, 0, "No frame skipping");
            	ImGui::NextColumn();
		    	OptionRadioButton("Normal", config::AutoSkipFrame, 1, "Skip a frame when the GPU and CPU are both running slow");
            	ImGui::NextColumn();
		    	OptionRadioButton("Maximum", config::AutoSkipFrame, 2, "Skip a frame when the GPU is running slow");
		    	ImGui::Columns(1, nullptr, false);

		    	OptionCheckbox("Shadows", config::ModifierVolumes,
		    			"Enable modifier volumes, usually used for shadows");
		    	OptionCheckbox("Fog", config::Fog, "Enable fog effects");
		    	OptionCheckbox("Widescreen", config::Widescreen,
		    			"Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas");
		    	OptionCheckbox("Widescreen Game Cheats", config::WidescreenGameHacks,
		    			"Modify the game so that it displays in 16:9 anamorphic format and use horizontal screen stretching. Only some games are supported.");
		    	OptionCheckbox("Show FPS Counter", config::ShowFPS, "Show on-screen frame/sec counter");
		    	OptionCheckbox("Show VMU In-game", config::FloatVMUs, "Show the VMU LCD screens while in-game");
		    	OptionCheckbox("Rotate Screen 90°", config::Rotate90, "Rotate the screen 90° counterclockwise");
		    	OptionCheckbox("Delay Frame Swapping", config::DelayFrameSwapping,
		    			"Useful to avoid flashing screen or glitchy videos. Not recommended on slow platforms");
#ifdef USE_VULKAN
				ImGui::Checkbox("Use Vulkan Renderer", &vulkan);
	            ImGui::SameLine();
	            ShowHelpMarker("Use Vulkan instead of Open GL/GLES");
#endif
	            const std::array<float, 9> scalings{ 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 4.5f, 5.f };
	            const std::array<std::string, 9> scalingsText{ "Half", "Native", "x1.5", "x2", "x2.5", "x3", "x4", "x4.5", "x5" };
	            std::array<int, scalings.size()> vres;
	            std::array<std::string, scalings.size()> resLabels;
	            u32 selected = 0;
	            for (u32 i = 0; i < scalings.size(); i++)
	            {
	            	vres[i] = scalings[i] * 480;
	            	if (vres[i] == config::RenderResolution)
	            		selected = i;
	            	if (!config::Widescreen)
	            		resLabels[i] = std::to_string((int)(scalings[i] * 640)) + "x" + std::to_string((int)(scalings[i] * 480));
	            	else
	            		resLabels[i] = std::to_string((int)(scalings[i] * 480 * 16 / 9)) + "x" + std::to_string((int)(scalings[i] * 480));
	            	resLabels[i] += " (" + scalingsText[i] + ")";
	            }

                ImGuiStyle& scaling_style = ImGui::GetStyle();
                float scaling_spacing = scaling_style.ItemInnerSpacing.x;
                ImGui::PushItemWidth(ImGui::CalcItemWidth() - scaling_spacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
                if (ImGui::BeginCombo("##Resolution", resLabels[selected].c_str(), ImGuiComboFlags_NoArrowButton))
                {
                	for (u32 i = 0; i < scalings.size(); i++)
                    {
                        bool is_selected = vres[i] == config::RenderResolution;
                        if (ImGui::Selectable(resLabels[i].c_str(), is_selected))
                        	config::RenderResolution = vres[i];
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::SameLine(0, scaling_spacing);
                
                if (ImGui::ArrowButton("##Decrease Res", ImGuiDir_Left))
                {
                    if (selected > 0)
                    	config::RenderResolution = vres[selected - 1];
                }
                ImGui::SameLine(0, scaling_spacing);
                if (ImGui::ArrowButton("##Increase Res", ImGuiDir_Right))
                {
                    if (selected < vres.size() - 1)
                    	config::RenderResolution = vres[selected + 1];
                }
                ImGui::SameLine(0, scaling_style.ItemInnerSpacing.x);
                
                ImGui::Text("Internal Resolution");
                ImGui::SameLine();
                ShowHelpMarker("Internal render resolution. Higher is better but more demanding");

		    	OptionSlider("Horizontal Stretching", config::ScreenStretching, 100, 150,
		    			"Stretch the screen horizontally");
		    	OptionSlider("Frame Skipping", config::SkipFrame, 0, 6,
		    			"Number of frames to skip between two actually rendered frames");
		    }
		    if (ImGui::CollapsingHeader("Render to Texture", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	OptionCheckbox("Copy to VRAM", config::RenderToTextureBuffer,
		    			"Copy rendered-to textures back to VRAM. Slower but accurate");
		    	OptionSlider("Render to Texture Upscaling", config::RenderToTextureUpscale, 1, 8,
		    			"Upscale rendered-to textures. Should be the same as the screen or window upscale ratio, or lower for slow platforms");
		    }
		    if (ImGui::CollapsingHeader("Texture Upscaling", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	OptionSlider("Texture Upscaling", config::TextureUpscale, 1, 8,
		    			"Upscale textures with the xBRZ algorithm. Only on fast platforms and for certain 2D games");
		    	OptionSlider("Upscaled Texture Max Size", config::MaxFilteredTextureSize, 8, 1024,
		    			"Textures larger than this dimension squared will not be upscaled");
		    	OptionSlider("Max Threads", config::MaxThreads, 1, 8,
		    			"Maximum number of threads to use for texture upscaling. Recommended: number of physical cores minus one");
		    	OptionCheckbox("Load Custom Textures", config::CustomTextures,
		    			"Load custom/high-res textures from data/textures/<game id>");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Audio"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			OptionCheckbox("Disable Sound", config::DisableSound, "Disable the emulator sound output");
			OptionCheckbox("Enable DSP", config::DSPEnabled,
					"Enable the Dreamcast Digital Sound Processor. Only recommended on fast platforms");
#if !defined(_WIN32)
#ifdef __ANDROID__
            OptionCheckbox("Automatic Latency", config::AutoLatency,
            		"Automatically set audio latency. Recommended");
#endif
            if (!config::AutoLatency)
            {
				int latency = (int)roundf(config::AudioBufferSize * 1000.f / 44100.f);
				ImGui::SliderInt("Latency", &latency, 12, 512, "%d ms");
				config::AudioBufferSize = (int)roundf(latency * 44100.f / 1000.f);
				ImGui::SameLine();
				ShowHelpMarker("Sets the maximum audio latency. Not supported by all audio drivers.");
            }
#endif

			audiobackend_t* backend = nullptr;
			std::string backend_name = config::AudioBackend;
			if (backend_name != "auto")
			{
				backend = GetAudioBackend(config::AudioBackend);
				if (backend != NULL)
					backend_name = backend->slug;
			}

			audiobackend_t* current_backend = backend;
			if (ImGui::BeginCombo("Audio Driver", backend_name.c_str(), ImGuiComboFlags_None))
			{
				bool is_selected = (config::AudioBackend.get() == "auto");
				if (ImGui::Selectable("auto - Automatic driver selection", &is_selected))
					config::AudioBackend.set("auto");

				for (u32 i = 0; i < GetAudioBackendCount(); i++)
				{
					backend = GetAudioBackend(i);
					is_selected = (config::AudioBackend.get() == backend->slug);

					if (is_selected)
						current_backend = backend;

					if (ImGui::Selectable((backend->slug + " - " + backend->name).c_str(), &is_selected))
						config::AudioBackend.set(backend->slug);
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ShowHelpMarker("The audio driver to use");

			if (current_backend != NULL && current_backend->get_options != NULL)
			{
				// get backend specific options
				int option_count;
				audio_option_t* options = current_backend->get_options(&option_count);

				for (int o = 0; o < option_count; o++)
				{
					std::string value = cfgLoadStr(current_backend->slug, options->cfg_name, "");

					if (options->type == integer)
					{
						int val = stoi(value);
						if (ImGui::SliderInt(options->caption.c_str(), &val, options->min_value, options->max_value))
						{
							std::string s = std::to_string(val);
							cfgSaveStr(current_backend->slug, options->cfg_name, s);
						}
					}
					else if (options->type == checkbox)
					{
						bool check = value == "1";
						if (ImGui::Checkbox(options->caption.c_str(), &check))
							cfgSaveStr(current_backend->slug, options->cfg_name,
									check ? "1" : "0");
					}
					else if (options->type == ::list)
					{
						if (ImGui::BeginCombo(options->caption.c_str(), value.c_str(), ImGuiComboFlags_None))
						{
							bool is_selected = false;
							for (const auto& cur : options->list_callback())
							{
								is_selected = value == cur;
								if (ImGui::Selectable(cur.c_str(), &is_selected))
									cfgSaveStr(current_backend->slug, options->cfg_name, cur);

								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
					else {
						WARN_LOG(RENDERER, "Unknown option");
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
				OptionRadioButton("Dynarec", config::DynarecEnabled, true,
					"Use the dynamic recompiler. Recommended in most cases");
				ImGui::NextColumn();
				OptionRadioButton("Interpreter", config::DynarecEnabled, false,
					"Use the interpreter. Very slow but may help in case of a dynarec problem");
				ImGui::Columns(1, NULL, false);
		    }
		    if (config::DynarecEnabled && ImGui::CollapsingHeader("Dynarec Options", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	OptionCheckbox("Safe Mode", config::DynarecSafeMode,
		    			"Do not optimize integer division. Not recommended");
#if HOST_CPU == CPU_ARM
		    	OptionCheckbox("Unstable Optimizations", config::DynarecUnstableOpt,
		    			"Enable unsafe optimizations. Will cause crash or environmental disaster");
#endif
		    	OptionCheckbox("Idle Skip", config::DynarecIdleSkip, "Skip wait loops. Recommended");
		    }
		    if (ImGui::CollapsingHeader("Network", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	OptionCheckbox("Broadband Adapter Emulation", config::EmulateBBA,
		    			"Emulate the Ethernet Broadband Adapter (BBA) instead of the Modem");
		    	OptionCheckbox("Enable Naomi Networking", config::NetworkEnable,
		    			"Enable networking for supported Naomi games");
		    	if (config::NetworkEnable)
		    	{
					OptionCheckbox("Act as Server", config::ActAsServer,
							"Create a local server for Naomi network games");
					if (!config::ActAsServer)
					{
						char server_name[256];
						strcpy(server_name, config::NetworkServer.get().c_str());
						ImGui::InputText("Server", server_name, sizeof(server_name), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
						ImGui::SameLine();
						ShowHelpMarker("The server to connect to. Leave blank to find a server automatically");
						config::NetworkServer.set(server_name);
					}
		    	}
		    }
		    if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
		    {
		    	OptionCheckbox("HLE BIOS", config::UseReios, "Force high-level BIOS emulation");
	            OptionCheckbox("Force Windows CE", config::ForceWindowsCE,
	            		"Enable full MMU emulation and other Windows CE settings. Do not enable unless necessary");
#ifndef __ANDROID
	            OptionCheckbox("Serial Console", config::SerialConsole,
	            		"Dump the Dreamcast serial console to stdout");
#endif
	            OptionCheckbox("Dump Textures", config::DumpTextures,
	            		"Dump all textures into data/texdump/<game id>");

	            bool logToFile = cfgLoadBool("log", "LogToFile", false);
	            bool newLogToFile = logToFile;
				ImGui::Checkbox("Log to File", &newLogToFile);
				if (logToFile != newLogToFile)
				{
					cfgSaveBool("log", "LogToFile", newLogToFile);
					LogManager::Shutdown();
					LogManager::Init();
				}
	            ImGui::SameLine();
	            ShowHelpMarker("Log debug information to flycast.log");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("About"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		    if (ImGui::CollapsingHeader("Flycast", ImGuiTreeNodeFlags_DefaultOpen))
		    {
				ImGui::Text("Version: %s", REICAST_VERSION);
				ImGui::Text("Git Hash: %s", GIT_HASH);
				ImGui::Text("Build Date: %s", BUILD_DATE);
				ImGui::Text("Target: %s",
					settings.platform.system == DC_PLATFORM_DREAMCAST ?
						"Dreamcast"
					: settings.platform.system == DC_PLATFORM_NAOMI ?
						"Naomi"
					: settings.platform.system == DC_PLATFORM_ATOMISWAVE ?
						"Atomiswave"
					:
						"Unknown"
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
#ifdef __ANDROID__
					"Android"
#elif defined(__unix__)
					"Linux"
#elif defined(__APPLE__)
#ifdef TARGET_IPHONE
		    		"iOS"
#else
					"OSX"
#endif
#elif defined(_WIN32)
					"Windows"
#else
					"Unknown"
#endif
						);
		    }
	    	if (config::RendererType.isOpenGL())
	    	{
				if (ImGui::CollapsingHeader("Open GL", ImGuiTreeNodeFlags_DefaultOpen))
				{
		    		ImGui::Text("Renderer: %s", (const char *)glGetString(GL_RENDERER));
		    		ImGui::Text("Version: %s", (const char *)glGetString(GL_VERSION));
		    	}
	    	}
#ifdef USE_VULKAN
	    	else
	    	{
				if (ImGui::CollapsingHeader("Vulkan", ImGuiTreeNodeFlags_DefaultOpen))
				{
		    		std::string name = VulkanContext::Instance()->GetDriverName();
		    		ImGui::Text("Driver Name: %s", name.c_str());
		    		std::string version = VulkanContext::Instance()->GetDriverVersion();
		    		ImGui::Text("Version: %s", version.c_str());
				}
	    	}
#endif

#ifdef __ANDROID__
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

    ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
    ScrollWhenDraggingOnVoid(ImVec2(0.0f, -mouse_delta.y), ImGuiMouseButton_Left);
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData(), false);

    if (vulkan != !config::RendererType.isOpenGL())
        pvr_rend = !vulkan ? RenderType::OpenGL
        		: config::RendererType == RenderType::OpenGL_OIT ? RenderType::Vulkan_OIT : RenderType::Vulkan;
    config::RendererType = pvr_rend;
}

void gui_display_notification(const char *msg, int duration)
{
	std::lock_guard<std::mutex> lock(osd_message_mutex);
	osd_message = msg;
	osd_message_end = os_GetSeconds() + (double)duration / 1000.0;
}

static std::string get_notification()
{
	std::lock_guard<std::mutex> lock(osd_message_mutex);
	if (!osd_message.empty() && os_GetSeconds() >= osd_message_end)
		osd_message.clear();
	return osd_message;
}

inline static void gui_display_demo()
{
	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	ImGui::ShowDemoWindow();
	ImGui::Render();
	ImGui_impl_RenderDrawData(ImGui::GetDrawData(), false);
}

static void gui_display_content()
{
	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

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
    if (gui_state != GuiState::SelectDisk)
    {
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f /*+ ImGui::GetStyle().ItemSpacing.x*/);
		if (ImGui::Button("Settings"))//, ImVec2(0, 30 * scaling)))
			gui_state = GuiState::Settings;
    }
    ImGui::PopStyleVar();

    scanner.fetch_game_list();

	// Only if Filter and Settings aren't focused... ImGui::SetNextWindowFocus();
	ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, 0), true);
    {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scaling, 20 * scaling));		// from 8, 4

		ImGui::PushID("bios");
		if (ImGui::Selectable("Dreamcast BIOS"))
		{
			gui_state = GuiState::Closed;
			gui_start_game("");
		}
		ImGui::PopID();

		{
			scanner.get_mutex().lock();
			for (const auto& game : scanner.get_game_list())
			{
				if (gui_state == GuiState::SelectDisk)
				{
					std::string extension = get_file_extension(game.path);
					if (extension != "gdi" && extension != "chd"
							&& extension != "cdi" && extension != "cue")
						// Only dreamcast disks
						continue;
				}
				if (filter.PassFilter(game.name.c_str()))
				{
					ImGui::PushID(game.path.c_str());
					if (ImGui::Selectable(game.name.c_str()))
					{
						if (gui_state == GuiState::SelectDisk)
						{
							strcpy(settings.imgread.ImagePath, game.path.c_str());
							DiscSwap();
							gui_state = GuiState::Closed;
						}
						else
						{
							std::string gamePath(game.path);
							scanner.get_mutex().unlock();
							gui_state = GuiState::Closed;
							gui_start_game(gamePath);
							scanner.get_mutex().lock();
							ImGui::PopID();
							break;
						}
					}
					ImGui::PopID();
				}
			}
			scanner.get_mutex().unlock();
		}
        ImGui::PopStyleVar();
    }
	ImGui::EndChild();
	ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

	error_popup();
    contentpath_warning_popup();

	ImGui::Render();
	ImGui_impl_RenderDrawData(ImGui::GetDrawData(), false);
}

static void systemdir_selected_callback(bool cancelled, std::string selection)
{
	if (!cancelled)
	{
		selection += "/";
		set_user_config_dir(selection);
		add_system_data_dir(selection);

		std::string data_path = selection + "data/";
		set_user_data_dir(data_path);
		if (!file_exists(data_path))
		{
			if (!make_directory(data_path))
			{
				WARN_LOG(BOOT, "Cannot create 'data' directory");
				set_user_data_dir(selection);
			}
		}

		if (cfgOpen())
		{
			config::Settings::instance().load(false);
			// Make sure the renderer type doesn't change mid-flight
			config::RendererType = RenderType::OpenGL;
			gui_state = GuiState::Main;
			if (config::ContentPath.get().empty())
			{
				scanner.stop();
				config::ContentPath.get().push_back(selection);
			}
			SaveSettings();
		}
	}
}

static void gui_display_onboarding()
{
	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	ImGui::OpenPopup("Select System Directory");
	select_directory_popup("Select System Directory", scaling, &systemdir_selected_callback);

	ImGui::Render();
	ImGui_impl_RenderDrawData(ImGui::GetDrawData(), false);
}

static std::future<bool> networkStatus;

static void start_network()
{
	networkStatus = naomiNetwork.startNetworkAsync();
	gui_state = GuiState::NetworkStart;
}

static void gui_network_start()
{
	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2, screen_height / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 180 * scaling));

	ImGui::Begin("##network", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 10 * scaling));
	ImGui::AlignTextToFramePadding();
	ImGui::SetCursorPosX(20.f * scaling);

	if (networkStatus.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		if (networkStatus.get())
		{
			gui_state = GuiState::Closed;
			ImGui::Text("STARTING...");
		}
		else
		{
			gui_state = GuiState::Main;
			settings.imgread.ImagePath[0] = '\0';
		}
	}
	else
	{
		ImGui::Text("STARTING NETWORK...");
		if (config::ActAsServer)
			ImGui::Text("Press Start to start the game now.");
	}
	ImGui::Text("%s", get_notification().c_str());

	float currentwidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
	ImGui::SetCursorPosY(126.f * scaling);
	if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f)))
	{
		naomiNetwork.terminate();
		networkStatus.get();
		gui_state = GuiState::Main;
		settings.imgread.ImagePath[0] = '\0';
	}
	ImGui::PopStyleVar();

	ImGui::End();

	ImGui::Render();
	ImGui_impl_RenderDrawData(ImGui::GetDrawData(), false);

	if ((kcode[0] & DC_BTN_START) == 0)
		naomiNetwork.startNow();
}

static void gui_display_loadscreen()
{
	ImGui_Impl_NewFrame();
	ImGui::NewFrame();
	if (config::RendererType.isOpenGL())
		ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(screen_width / 2, screen_height / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 180 * scaling));

    ImGui::Begin("##loading", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 10 * scaling));
    ImGui::AlignTextToFramePadding();
    ImGui::SetCursorPosX(20.f * scaling);
	if (dc_is_load_done())
	{
		try {
			dc_get_load_status();
			if (NaomiNetworkSupported())
			{
				start_network();
			}
			else
			{
				gui_state = GuiState::Closed;
				ImGui::Text("STARTING...");
			}
		} catch (const ReicastException& ex) {
			ERROR_LOG(BOOT, "%s", ex.reason.c_str());
			error_msg = ex.reason;
#ifdef TEST_AUTOMATION
			die("Game load failed");
#endif
			gui_state = GuiState::Main;
			settings.imgread.ImagePath[0] = '\0';
		}
	}
	else
	{
		ImGui::Text("LOADING... ");
		ImGui::SameLine();
		ImGui::Text("%s", get_notification().c_str());

		float currentwidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
		ImGui::SetCursorPosY(126.f * scaling);
		if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f)))
		{
			dc_cancel_load();
			gui_state = GuiState::Main;
		}
	}
	ImGui::PopStyleVar();

    ImGui::End();

    ImGui::Render();
	ImGui_impl_RenderDrawData(ImGui::GetDrawData(), false);
}

void gui_display_ui()
{
	switch (gui_state)
	{
	case GuiState::Settings:
		gui_display_settings();
		break;
	case GuiState::Commands:
		gui_display_commands();
		break;
	case GuiState::Main:
		//gui_display_demo();
		{
			std::string game_file = settings.imgread.ImagePath;
			if (!game_file.empty())
				gui_start_game(game_file);
			else
				gui_display_content();
		}
		break;
	case GuiState::Closed:
		break;
	case GuiState::Onboarding:
		gui_display_onboarding();
		break;
	case GuiState::VJoyEdit:
		break;
	case GuiState::VJoyEditCommands:
#ifdef __ANDROID__
		gui_display_vjoy_commands(screen_width, screen_height, scaling);
#endif
		break;
	case GuiState::SelectDisk:
		gui_display_content();
		break;
	case GuiState::Loading:
		gui_display_loadscreen();
		break;
	case GuiState::NetworkStart:
		gui_network_start();
		break;
	default:
		die("Unknown UI state");
		break;
	}

	if (gui_state == GuiState::Closed)
		dc_resume();
}

static float LastFPSTime;
static int lastFrameCount = 0;
static float fps = -1;

static std::string getFPSNotification()
{
	if (config::ShowFPS)
	{
		double now = os_GetSeconds();
		if (now - LastFPSTime >= 1.0) {
			fps = (MainFrameCount - lastFrameCount) / (now - LastFPSTime);
			LastFPSTime = now;
			lastFrameCount = MainFrameCount;
		}
		if (fps >= 0.f && fps < 9999.f) {
			char text[32];
			snprintf(text, sizeof(text), "F:%.1f%s", fps, settings.input.fastForwardMode ? " >>" : "");

			return std::string(text);
		}
	}
	return std::string(settings.input.fastForwardMode ? ">>" : "");
}

void gui_display_osd()
{
	if (gui_state == GuiState::VJoyEdit)
		return;
	std::string message = get_notification();
	if (message.empty())
		message = getFPSNotification();

	if (!message.empty() || config::FloatVMUs || crosshairsNeeded())
	{
		ImGui_Impl_NewFrame();
		ImGui::NewFrame();

		if (!message.empty())
		{
			ImGui::SetNextWindowBgAlpha(0);
			ImGui::SetNextWindowPos(ImVec2(0, screen_height), ImGuiCond_Always, ImVec2(0.f, 1.f));	// Lower left corner
			ImGui::SetNextWindowSize(ImVec2(screen_width, 0));

			ImGui::Begin("##osd", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav
					| ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
			ImGui::SetWindowFontScale(1.5);
			ImGui::TextColored(ImVec4(1, 1, 0, 0.7), "%s", message.c_str());
			ImGui::End();
		}
		displayCrosshairs();
		if (config::FloatVMUs)
			display_vmus();
//		gui_plot_render_time(screen_width, screen_height);

		ImGui::Render();
		ImGui_impl_RenderDrawData(ImGui::GetDrawData());
	}
}

void gui_open_onboarding()
{
	gui_state = GuiState::Onboarding;
}

void gui_term()
{
	if (inited)
	{
		inited = false;
		term_vmus();
		if (config::RendererType.isOpenGL())
			ImGui_ImplOpenGL3_Shutdown();
		ImGui::DestroyContext();
	    EventManager::unlisten(Event::Resume, emuEventCallback);
	}
}

int msgboxf(const char* text, unsigned int type, ...) {
    va_list args;

    char temp[2048];
    va_start(args, type);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    ERROR_LOG(COMMON, "%s", temp);

    gui_display_notification(temp, 2000);

    return 1;
}

extern bool subfolders_read;

void gui_refresh_files()
{
	scanner.refresh();
	subfolders_read = false;
}

#define VMU_WIDTH (70 * 48 * scaling / 32)
#define VMU_HEIGHT (70 * scaling)
#define VMU_PADDING (8 * scaling)
u32 vmu_lcd_data[8][48 * 32];
bool vmu_lcd_status[8];
bool vmu_lcd_changed[8];
static ImTextureID vmu_lcd_tex_ids[8];

static ImTextureID crosshairTexId;

void push_vmu_screen(int bus_id, int bus_port, u8* buffer)
{
	int vmu_id = bus_id * 2 + bus_port;
	if (vmu_id < 0 || vmu_id >= (int)ARRAY_SIZE(vmu_lcd_data))
		return;
	u32 *p = &vmu_lcd_data[vmu_id][0];
	for (int i = 0; i < (int)ARRAY_SIZE(vmu_lcd_data[vmu_id]); i++, buffer++)
		*p++ = *buffer != 0 ? 0xFFFFFFFFu : 0xFF000000u;
	vmu_lcd_status[vmu_id] = true;
	vmu_lcd_changed[vmu_id] = true;
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
	if (!config::RendererType.isOpenGL())
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
			ImGui_ImplOpenGL3_DeleteTexture(vmu_lcd_tex_ids[i]);
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

static const int lightgunCrosshairData[16 * 16] =
{
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	-1,-1,-1,-1,-1,-1, 0, 0, 0, 0,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1, 0, 0, 0, 0,-1,-1,-1,-1,-1,-1,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
};

const u32 *getCrosshairTextureData()
{
	return (u32 *)lightgunCrosshairData;
}

std::pair<float, float> getCrosshairPosition(int playerNum)
{
	float fx = mo_x_abs[playerNum];
	float fy = mo_y_abs[playerNum];
	int width = screen_width;
	int height = screen_height;
	if (config::Rotate90)
	{
		float t = fy;
		fy = width - fx;
		fx = t;
		std::swap(width, height);
	}
	if ((float)width / height >= 640.f / 480.f)
	{
		float scale = 480.f / height;
		fy /= scale;
		scale *= config::ScreenStretching / 100.f;
		fx = fx / scale + (width - 640.f / scale) / 2.f;
	}
	else
	{
		float scale = 640.f / width;
		fx /= scale;
		scale *= config::ScreenStretching / 100.f;
		fy = fy / scale + (height - 480.f / scale) / 2.f;
	}
	return std::make_pair(fx, fy);
}

static void displayCrosshairs()
{
	if (!game_started)
		return;
	if (!config::RendererType.isOpenGL())
		return;
	if (!crosshairsNeeded())
		return;

	if (crosshairTexId == ImTextureID())
		crosshairTexId = ImGui_ImplOpenGL3_CreateCrosshairTexture(getCrosshairTextureData());
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));

    ImGui::Begin("xhair-window", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs
    		| ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
	for (u32 i = 0; i < config::CrosshairColor.size(); i++)
	{
		if (config::CrosshairColor[i] == 0)
			continue;
		if (settings.platform.system == DC_PLATFORM_DREAMCAST && config::MapleMainDevices[i] != MDT_LightGun)
			continue;

		ImVec2 pos;
		std::tie(pos.x, pos.y) = getCrosshairPosition(i);
		pos.x -= XHAIR_WIDTH / 2.f;
		pos.y += XHAIR_WIDTH / 2.f;
		ImVec2 pos_b(pos.x + XHAIR_WIDTH, pos.y - XHAIR_HEIGHT);

		ImGui::GetWindowDrawList()->AddImage(crosshairTexId, pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), config::CrosshairColor[i]);
	}
	ImGui::End();
}

static void reset_vmus()
{
	for (u32 i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
		vmu_lcd_status[i] = false;
}

static void term_vmus()
{
	if (!config::RendererType.isOpenGL())
		return;
	for (u32 i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
	{
		if (vmu_lcd_tex_ids[i] != ImTextureID())
		{
			ImGui_ImplOpenGL3_DeleteTexture(vmu_lcd_tex_ids[i]);
			vmu_lcd_tex_ids[i] = ImTextureID();
		}
	}
	if (crosshairTexId != ImTextureID())
	{
		ImGui_ImplOpenGL3_DeleteTexture(crosshairTexId);
		crosshairTexId = ImTextureID();
	}
}
