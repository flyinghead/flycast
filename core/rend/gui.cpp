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
#include "network/net_handshake.h"
#include "network/ggpo.h"
#include "wsi/context.h"
#include "input/gamepad_device.h"
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

static bool game_started;

extern u8 kb_shift[MAPLE_PORTS]; // shift keys pressed (bitmask)
extern u8 kb_key[MAPLE_PORTS][6];		// normal keys pressed

int screen_dpi = 96;
int insetLeft, insetRight, insetTop, insetBottom;

static bool inited = false;
float scaling = 1;
GuiState gui_state = GuiState::Main;
static bool commandLineStart;
static u32 mouseButtons;
static int mouseX, mouseY;
static float mouseWheel;
static std::string error_msg;
static std::string osd_message;
static double osd_message_end;
static std::mutex osd_message_mutex;

static int map_system = 0;
static void display_vmus();
static void reset_vmus();
static void term_vmus();
static void displayCrosshairs();

GameScanner scanner;

static void emuEventCallback(Event event)
{
	switch (event)
	{
	case Event::Resume:
		game_started = true;
		break;
	case Event::Start:
		GamepadDevice::load_system_mappings();
		if (config::AutoLoadState && settings.imgread.ImagePath[0] != '\0')
			// TODO don't load state if using naomi networking
			dc_loadstate(config::SavestateSlot);
		break;
	case Event::Terminate:
		if (config::AutoSaveState && settings.imgread.ImagePath[0] != '\0')
			dc_savestate(config::SavestateSlot);
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
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
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
#if !(defined(_WIN32) || defined(__APPLE__) || defined(__SWITCH__)) || defined(TARGET_IPHONE)
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
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
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
#elif defined(__ANDROID__)
    if (getenv("FLYCAST_LOCALE") != nullptr)
    {
    	const ImWchar *glyphRanges = nullptr;
    	std::string locale = getenv("FLYCAST_LOCALE");
        if (locale.find("ja") == 0)				// Japanese
        	glyphRanges = io.Fonts->GetGlyphRangesJapanese();
        else if (locale.find("ko") == 0)		// Korean
        	glyphRanges = io.Fonts->GetGlyphRangesKorean();
        else if (locale.find("zh_TW") == 0
        		|| locale.find("zh_HK") == 0)	// Traditional Chinese
        	glyphRanges = GetGlyphRangesChineseTraditionalOfficial();
        else if (locale.find("zh_CN") == 0)		// Simplified Chinese
        	glyphRanges = GetGlyphRangesChineseSimplifiedOfficial();

        if (glyphRanges != nullptr)
        	io.Fonts->AddFontFromFileTTF("/system/fonts/NotoSansCJK-Regular.ttc", 17.f * scaling, &font_cfg, glyphRanges);
    }

    // TODO Linux, iOS, ...
#endif
    INFO_LOG(RENDERER, "Screen DPI is %d, size %d x %d. Scaling by %.2f", screen_dpi, screen_width, screen_height, scaling);

    EventManager::listen(Event::Resume, emuEventCallback);
    EventManager::listen(Event::Start, emuEventCallback);
    EventManager::listen(Event::Terminate, emuEventCallback);
}

void gui_keyboard_input(u16 wc)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard)
		io.AddInputCharacter(wc);
}

void gui_keyboard_inputUTF8(const std::string& s)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard)
		io.AddInputCharactersUTF8(s.c_str());
}

void gui_set_mouse_position(int x, int y)
{
	mouseX = x;
	mouseY = y;
}

void gui_set_mouse_button(int button, bool pressed)
{
	if (pressed)
		mouseButtons |= 1 << button;
	else
		mouseButtons &= ~(1 << button);
}

void gui_set_mouse_wheel(float delta)
{
	mouseWheel += delta;
}

static void ImGui_Impl_NewFrame()
{
	if (config::RendererType.isOpenGL())
		ImGui_ImplOpenGL3_NewFrame();
#ifdef _WIN32
	else if (config::RendererType.isDirectX())
		ImGui_ImplDX9_NewFrame();
#endif
	ImGui::GetIO().DisplaySize.x = screen_width;
	ImGui::GetIO().DisplaySize.y = screen_height;

	ImGuiIO& io = ImGui::GetIO();

	// Read keyboard modifiers inputs
	io.KeyCtrl = 0;
	io.KeyShift = 0;
	io.KeyAlt = false;
	io.KeySuper = false;
	memset(&io.KeysDown[0], 0, sizeof(io.KeysDown));
	for (int port = 0; port < 4; port++)
	{
		io.KeyCtrl |= (kb_shift[port] & (0x01 | 0x10)) != 0;
		io.KeyShift |= (kb_shift[port] & (0x02 | 0x20)) != 0;

		for (int i = 0; i < IM_ARRAYSIZE(kb_key[0]); i++)
			if (kb_key[port][i] != 0)
				io.KeysDown[kb_key[port][i]] = true;
	}
	if (mouseX < 0 || mouseX >= screen_width || mouseY < 0 || mouseY >= screen_height)
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	else
		io.MousePos = ImVec2(mouseX, mouseY);
	static bool delayTouch;
#if defined(__ANDROID__) || defined(TARGET_IPHONE)
	// Delay touch by one frame to allow widgets to be hovered before click
	// This is required for widgets using ImGuiButtonFlags_AllowItemOverlap such as TabItem's
	if (!delayTouch && (mouseButtons & (1 << 0)) != 0 && !io.MouseDown[ImGuiMouseButton_Left])
		delayTouch = true;
	else
		delayTouch = false;
#endif
	if (io.WantCaptureMouse)
	{
		io.MouseWheel = -mouseWheel / 16;
		mouseWheel = 0;
	}
	if (!delayTouch)
		io.MouseDown[ImGuiMouseButton_Left] = (mouseButtons & (1 << 0)) != 0;
	io.MouseDown[ImGuiMouseButton_Right] = (mouseButtons & (1 << 1)) != 0;
	io.MouseDown[ImGuiMouseButton_Middle] = (mouseButtons & (1 << 2)) != 0;
	io.MouseDown[3] = (mouseButtons & (1 << 3)) != 0;

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

	ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
}

void gui_set_insets(int left, int right, int top, int bottom)
{
	insetLeft = left;
	insetRight = right;
	insetTop = top;
	insetBottom = bottom;
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
		GamepadDevice::load_system_mappings();
		dc_resume();
	}
}

void gui_start_game(const std::string& path)
{
	dc_term_game();
	reset_vmus();

	scanner.stop();
	gui_state = GuiState::Loading;
	static std::string path_copy;
	path_copy = path;	// path may be a local var

	dc_load_game(path.empty() ? NULL : path_copy.c_str());
}

void gui_stop_game(const std::string& message)
{
	if (!commandLineStart)
	{
		// Exit to main menu
		dc_term_game();
		gui_state = GuiState::Main;
		game_started = false;
		settings.imgread.ImagePath[0] = '\0';
		reset_vmus();
		if (!message.empty())
			error_msg = "Flycast has stopped.\n\n" + message;
	}
	else
	{
		// Exit emulator
		dc_exit();
	}
}

static void gui_display_commands()
{
	if (dc_is_running())
		dc_stop();

   	display_vmus();

    centerNextWindow();
    ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

    ImGui::Begin("##commands", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    bool loadSaveStateDisabled = settings.imgread.ImagePath[0] == '\0' || settings.online;
	if (loadSaveStateDisabled)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	// Load State
	if (ImGui::Button("Load State", ImVec2(110 * scaling, 50 * scaling)) && !loadSaveStateDisabled)
	{
		gui_state = GuiState::Closed;
		dc_loadstate(config::SavestateSlot);
	}
	ImGui::SameLine();

	// Slot #
	std::string slot = "Slot " + std::to_string((int)config::SavestateSlot + 1);
	if (ImGui::Button(slot.c_str(), ImVec2(80 * scaling - ImGui::GetStyle().FramePadding.x, 50 * scaling)))
		ImGui::OpenPopup("slot_select_popup");
    if (ImGui::BeginPopup("slot_select_popup"))
    {
        for (int i = 0; i < 10; i++)
            if (ImGui::Selectable(std::to_string(i + 1).c_str(), config::SavestateSlot == i, 0,
            		ImVec2(ImGui::CalcTextSize("Slot 8").x, 0))) {
                config::SavestateSlot = i;
                SaveSettings();
            }
        ImGui::EndPopup();
    }
	ImGui::SameLine();

	// Save State
	if (ImGui::Button("Save State", ImVec2(110 * scaling, 50 * scaling)) && !loadSaveStateDisabled)
	{
		gui_state = GuiState::Closed;
		dc_savestate(config::SavestateSlot);
	}
	if (loadSaveStateDisabled)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

	ImGui::Columns(2, "buttons", false);

	// Settings
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Resume", ImVec2(150 * scaling, 50 * scaling)))
	{
		GamepadDevice::load_system_mappings();
		gui_state = GuiState::Closed;
	}

	ImGui::NextColumn();

	// Insert/Eject Disk
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

	// Cheats
	if (settings.online)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if (ImGui::Button("Cheats", ImVec2(150 * scaling, 50 * scaling)) && !settings.online)
	{
		gui_state = GuiState::Cheats;
	}
	if (settings.online)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}
	ImGui::Columns(1, nullptr, false);

	// Exit
	if (ImGui::Button("Exit", ImVec2(300 * scaling + ImGui::GetStyle().ColumnsMinSpacing + ImGui::GetStyle().FramePadding.x * 2 - 1,
			50 * scaling)))
	{
		gui_stop_game();
	}

	ImGui::End();
}

inline static void header(const char *title)
{
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
	ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
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

struct Mapping {
	DreamcastKey key;
	const char *name;
};

const Mapping dcButtons[] = {
	{ EMU_BTN_NONE, "Directions" },
	{ DC_DPAD_UP, "Up" },
	{ DC_DPAD_DOWN, "Down" },
	{ DC_DPAD_LEFT, "Left" },
	{ DC_DPAD_RIGHT, "Right" },

	{ DC_AXIS_UP, "Thumbstick Up" },
	{ DC_AXIS_DOWN, "Thumbstick Down" },
	{ DC_AXIS_LEFT, "Thumbstick Left" },
	{ DC_AXIS_RIGHT, "Thumbstick Right" },

	{ DC_DPAD2_UP, "DPad2 Up" },
	{ DC_DPAD2_DOWN, "DPad2 Down" },
	{ DC_DPAD2_LEFT, "DPad2 Left" },
	{ DC_DPAD2_RIGHT, "DPad2 Right" },

	{ EMU_BTN_NONE, "Buttons" },
	{ DC_BTN_A, "A" },
	{ DC_BTN_B, "B" },
	{ DC_BTN_X, "X" },
	{ DC_BTN_Y, "Y" },
	{ DC_BTN_C, "C" },
	{ DC_BTN_D, "D" },
	{ DC_BTN_Z, "Z" },

	{ EMU_BTN_NONE, "Triggers" },
	{ DC_AXIS_LT, "Left Trigger" },
	{ DC_AXIS_RT, "Right Trigger" },

	{ EMU_BTN_NONE, "System Buttons" },
	{ DC_BTN_START, "Start" },
	{ DC_BTN_RELOAD, "Reload" },

	{ EMU_BTN_NONE, "Emulator" },
	{ EMU_BTN_MENU, "Menu" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },

	{ EMU_BTN_NONE, nullptr }
};

const Mapping arcadeButtons[] = {
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

	{ EMU_BTN_NONE, "System Buttons" },
	{ DC_BTN_START, "Start" },
	{ DC_BTN_RELOAD, "Reload" },
	{ DC_BTN_D, "Coin" },
	{ DC_DPAD2_UP, "Service" },
	{ DC_DPAD2_DOWN, "Test" },

	{ EMU_BTN_NONE, "Emulator" },
	{ EMU_BTN_MENU, "Menu" },
	{ EMU_BTN_ESCAPE, "Exit" },
	{ EMU_BTN_FFORWARD, "Fast-forward" },

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

static std::shared_ptr<GamepadDevice> mapped_device;
static u32 mapped_code;
static bool analogAxis;
static bool positiveDirection;
static double map_start_time;
static bool arcade_button_mode;
static u32 gamepad_port;

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
	default:
		return EMU_BTN_NONE;
	}
}
static void detect_input_popup(const Mapping *mapping)
{
	ImVec2 padding = ImVec2(20 * scaling, 20 * scaling);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	if (ImGui::BeginPopupModal("Map Control", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("Waiting for control '%s'...", mapping->name);
		double now = os_GetSeconds();
		ImGui::Text("Time out in %d s", (int)(5 - (now - map_start_time)));
		if (mapped_code != (u32)-1)
		{
			std::shared_ptr<InputMapping> input_mapping = mapped_device->get_input_mapping();
			if (input_mapping != NULL)
			{
				unmapControl(input_mapping, gamepad_port, mapping->key);
				if (analogAxis)
				{
					input_mapping->set_axis(gamepad_port, mapping->key, mapped_code, positiveDirection);
					DreamcastKey opposite = getOppositeDirectionKey(mapping->key);
					// Map the axis opposite direction to the corresponding opposite dc button or axis,
					// but only if the opposite direction axis isn't used and the dc button or axis isn't mapped.
					if (opposite != EMU_BTN_NONE
							&& input_mapping->get_axis_id(gamepad_port, mapped_code, !positiveDirection) == EMU_BTN_NONE
							&& input_mapping->get_axis_code(gamepad_port, opposite).first == (u32)-1
							&& input_mapping->get_button_code(gamepad_port, opposite) == (u32)-1)
						input_mapping->set_axis(gamepad_port, opposite, mapped_code, !positiveDirection);
				}
				else
					input_mapping->set_button(gamepad_port, mapping->key, mapped_code);
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

static void displayLabelOrCode(const char *label, u32 code, const char *suffix = "")
{
	if (label != nullptr)
		ImGui::Text("%s%s", label, suffix);
	else
		ImGui::Text("[%d]%s", code, suffix);
}

static void displayMappedControl(const std::shared_ptr<GamepadDevice>& gamepad, DreamcastKey key)
{
	std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
	u32 code = input_mapping->get_button_code(gamepad_port, key);
	if (code != (u32)-1)
	{
		displayLabelOrCode(gamepad->get_button_name(code), code);
		return;
	}
	std::pair<u32, bool> pair = input_mapping->get_axis_code(gamepad_port, key);
	code = pair.first;
	if (code != (u32)-1)
	{
		displayLabelOrCode(gamepad->get_axis_name(code), code, pair.second ? "+" : "-");
		return;
	}
}

static void controller_mapping_popup(const std::shared_ptr<GamepadDevice>& gamepad)
{
	fullScreenWindow(true);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("Controller Mapping", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float width = ImGui::GetIO().DisplaySize.x - insetLeft - insetRight - (style.WindowBorderSize + style.WindowPadding.x) * 2;
		const float col_width = (width - style.GrabMinSize - style.ItemSpacing.x
				- (ImGui::CalcTextSize("Map").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x)
				- (ImGui::CalcTextSize("Unmap").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x)) / 2;

		std::shared_ptr<InputMapping> input_mapping = gamepad->get_input_mapping();
		if (input_mapping == NULL || ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
		{
			ImGui::CloseCurrentPopup();
			gamepad->save_mapping(map_system);
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
		float comboWidth = ImGui::CalcTextSize("Dreamcast Controls").x + ImGui::GetStyle().ItemSpacing.x * 3.0f + ImGui::GetFontSize();
		ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - comboWidth - ImGui::GetStyle().ItemSpacing.x - 100 * scaling * 2);

		ImGui::AlignTextToFramePadding();

		if (ImGui::Button("Reset...", ImVec2(100 * scaling, 30 * scaling)))
			ImGui::OpenPopup("Confirm Reset");

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20 * scaling, 20 * scaling));
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
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20 * scaling, ImGui::GetStyle().ItemSpacing.y));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * scaling, 10 * scaling));
			if (ImGui::Button("Yes"))
			{
				gamepad->resetMappingToDefault(arcade_button_mode, !hitbox);
				gamepad->save_mapping(map_system);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No"))
				ImGui::CloseCurrentPopup();
			ImGui::PopStyleVar(2);;

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(1);;

		ImGui::SameLine();

		const char* items[] = { "Dreamcast Controls", "Arcade Controls" };
		static int item_current_map_idx = 0;
		static int last_item_current_map_idx = 2;

		// Here our selection data is an index.

		ImGui::PushItemWidth(comboWidth);
		// Make the combo height the same as the Done and Reset buttons
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, (30 * scaling - ImGui::GetFontSize()) / 2));
		ImGui::Combo("##arcadeMode", &item_current_map_idx, items, IM_ARRAYSIZE(items));
		ImGui::PopStyleVar();
		ImGui::PopItemWidth();
		if (item_current_map_idx != last_item_current_map_idx)
		{
			gamepad->save_mapping(map_system);
		}
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
			gamepad->find_mapping(map_system);
			input_mapping = gamepad->get_input_mapping();

			last_item_current_map_idx = item_current_map_idx;
		}

		char key_id[32];

		ImGui::BeginChildFrame(ImGui::GetID("buttons"), ImVec2(0, 0), ImGuiWindowFlags_None);

		gamepad->find_mapping(map_system);
		for (; systemMapping->name != nullptr; systemMapping++)
		{
			if (systemMapping->key == EMU_BTN_NONE)
			{
				ImGui::Columns(1, nullptr, false);
				header(systemMapping->name);
				ImGui::Columns(3, "bindings", false);
				ImGui::SetColumnWidth(0, col_width);
				ImGui::SetColumnWidth(1, col_width);
				continue;
			}
			sprintf(key_id, "key_id%d", systemMapping->key);
			ImGui::PushID(key_id);

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
			if (ImGui::Button("Map"))
			{
				map_start_time = os_GetSeconds();
				ImGui::OpenPopup("Map Control");
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detectButtonOrAxisInput([](u32 code, bool analog, bool positive)
						{
							mapped_code = code;
							analogAxis = analog;
							positiveDirection = positive;
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
			ImGui::PopID();
		}
		ImGui::Columns(1, nullptr, false);
	    scrollWhenDraggingOnVoid();
	    windowDragScroll();

		ImGui::EndChildFrame();
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

static void error_popup()
{
	if (!error_msg.empty())
	{
		ImVec2 padding = ImVec2(20 * scaling, 20 * scaling);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
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
			ImGui::PopTextWrapPos();
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
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
        select_file_popup("Select Directory", [](bool cancelled, std::string selection)
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

static void gui_display_settings()
{
	static bool maple_devices_changed;

	fullScreenWindow(false);
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
				loadGameSpecificSettings();
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

#if !defined(TARGET_IPHONE)
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
                select_file_popup("Select Directory", [](bool cancelled, std::string selection)
                		{
                			if (!cancelled)
                			{
                				scanner.stop();
                				config::ContentPath.get().push_back(selection);
                				scanner.refresh();
                			}
                		});
                ImGui::PopStyleVar();
                scrollWhenDraggingOnVoid();

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

#if defined(__linux__) && !defined(__ANDROID__)
            if (ImGui::ListBoxHeader("Data Directory", 1))
            {
            	ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", get_writable_data_path("").c_str());
                ImGui::ListBoxFooter();
            }
            ImGui::SameLine();
            ShowHelpMarker("The directory containing BIOS files, as well as saved VMUs and states");
#else
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
#endif // !linux
#endif // !TARGET_IPHONE

			if (OptionCheckbox("Hide Legacy Naomi Roms", config::HideLegacyNaomiRoms,
					"Hide .bin, .dat and .lst files from the content browser"))
				scanner.refresh();
	    	ImGui::Text("Automatic State:");
			OptionCheckbox("Load", config::AutoLoadState,
					"Load the last saved state of the game when starting");
			ImGui::SameLine();
			OptionCheckbox("Save", config::AutoSaveState,
					"Save the state of the game when stopping");

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Controls"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			header("Physical Devices");
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
						gamepad->verify_or_create_system_mappings();
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
#ifdef _WIN32
	    	OptionCheckbox("Use Raw Input", config::UseRawInput, "Supports multiple pointing devices (mice, light guns) and keyboards");
#endif

			ImGui::Spacing();
			header("Dreamcast Devices");
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
		    }

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Video"))
		{
			int renderApi;
			bool perPixel;
			switch (config::RendererType)
			{
			default:
			case RenderType::OpenGL:
				renderApi = 0;
				perPixel = false;
				break;
			case RenderType::OpenGL_OIT:
				renderApi = 0;
				perPixel = true;
				break;
			case RenderType::Vulkan:
				renderApi = 1;
				perPixel = false;
				break;
			case RenderType::Vulkan_OIT:
				renderApi = 1;
				perPixel = true;
				break;
			case RenderType::DirectX9:
				renderApi = 2;
				perPixel = false;
				break;
			}

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
#if !defined(__APPLE__)
			bool has_per_pixel = false;
			if (renderApi == 0)
				has_per_pixel = !theGLContext.IsGLES() && theGLContext.GetMajorVersion() >= 4;
#ifdef USE_VULKAN
			else
				has_per_pixel = VulkanContext::Instance()->SupportsFragmentShaderStoresAndAtomics();
#endif
#else
			bool has_per_pixel = false;
#endif
		    header("Transparent Sorting");
		    {
		    	int renderer = perPixel ? 2 : config::PerStripSorting ? 1 : 0;
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
		    		perPixel = false;
		    		config::PerStripSorting.set(false);
		    		break;
		    	case 1:
		    		perPixel = false;
		    		config::PerStripSorting.set(true);
		    		break;
		    	case 2:
		    		perPixel = true;
		    		break;
		    	}
		    }
	    	ImGui::Spacing();
		    header("Rendering Options");
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
		    	if (!config::Widescreen)
		    	{
			        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		    	}
		    	ImGui::Indent();
		    	OptionCheckbox("Super Widescreen", config::SuperWidescreen,
		    			"Use the full width of the screen or window when its aspect ratio is greater than 16:9");
		    	ImGui::Unindent();
		    	if (!config::Widescreen)
		    	{
			        ImGui::PopItemFlag();
			        ImGui::PopStyleVar();
		    	}
		    	OptionCheckbox("Widescreen Game Cheats", config::WidescreenGameHacks,
		    			"Modify the game so that it displays in 16:9 anamorphic format and use horizontal screen stretching. Only some games are supported.");
#ifndef TARGET_IPHONE
		    	OptionCheckbox("VSync", config::VSync, "Synchronizes the frame rate with the screen refresh rate. Recommended");
#endif
		    	OptionCheckbox("Show FPS Counter", config::ShowFPS, "Show on-screen frame/sec counter");
		    	OptionCheckbox("Show VMU In-game", config::FloatVMUs, "Show the VMU LCD screens while in-game");
		    	OptionCheckbox("Rotate Screen 90°", config::Rotate90, "Rotate the screen 90° counterclockwise");
		    	OptionCheckbox("Delay Frame Swapping", config::DelayFrameSwapping,
		    			"Useful to avoid flashing screen or glitchy videos. Not recommended on slow platforms");
#if defined(USE_VULKAN) || defined(_WIN32)
		    	ImGui::Text("Graphics API:");
#if defined(USE_VULKAN) && defined(_WIN32)
	            constexpr u32 columns = 3;
#else
	            constexpr u32 columns = 2;
#endif
	            ImGui::Columns(columns, "renderApi", false);
		    	ImGui::RadioButton("Open GL", &renderApi, 0);
            	ImGui::NextColumn();
#ifdef USE_VULKAN
		    	ImGui::RadioButton("Vulkan", &renderApi, 1);
            	ImGui::NextColumn();
#endif
#ifdef _WIN32
		    	ImGui::RadioButton("DirectX", &renderApi, 2);
            	ImGui::NextColumn();
#endif
		    	ImGui::Columns(1, NULL, false);
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

                ImGuiStyle& style = ImGui::GetStyle();
                float innerSpacing = style.ItemInnerSpacing.x;
                ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
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
                ImGui::SameLine(0, innerSpacing);
                
                if (ImGui::ArrowButton("##Decrease Res", ImGuiDir_Left))
                {
                    if (selected > 0)
                    	config::RenderResolution = vres[selected - 1];
                }
                ImGui::SameLine(0, innerSpacing);
                if (ImGui::ArrowButton("##Increase Res", ImGuiDir_Right))
                {
                    if (selected < vres.size() - 1)
                    	config::RenderResolution = vres[selected + 1];
                }
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                
                ImGui::Text("Internal Resolution");
                ImGui::SameLine();
                ShowHelpMarker("Internal render resolution. Higher is better but more demanding");

		    	OptionSlider("Horizontal Stretching", config::ScreenStretching, 100, 150,
		    			"Stretch the screen horizontally");
		    	OptionArrowButtons("Frame Skipping", config::SkipFrame, 0, 6,
		    			"Number of frames to skip between two actually rendered frames");
		    }
	    	ImGui::Spacing();
		    header("Render to Texture");
		    {
		    	OptionCheckbox("Copy to VRAM", config::RenderToTextureBuffer,
		    			"Copy rendered-to textures back to VRAM. Slower but accurate");
		    }
	    	ImGui::Spacing();
		    header("Texture Upscaling");
		    {
#ifndef TARGET_NO_OPENMP
		    	OptionArrowButtons("Texture Upscaling", config::TextureUpscale, 1, 8,
		    			"Upscale textures with the xBRZ algorithm. Only on fast platforms and for certain 2D games");
		    	OptionSlider("Texture Max Size", config::MaxFilteredTextureSize, 8, 1024,
		    			"Textures larger than this dimension squared will not be upscaled");
		    	OptionArrowButtons("Max Threads", config::MaxThreads, 1, 8,
		    			"Maximum number of threads to use for texture upscaling. Recommended: number of physical cores minus one");
#endif
		    	OptionCheckbox("Load Custom Textures", config::CustomTextures,
		    			"Load custom/high-res textures from data/textures/<game id>");
		    }
			ImGui::PopStyleVar();
			ImGui::EndTabItem();

		    switch (renderApi)
		    {
		    case 0:
		    	config::RendererType = perPixel ? RenderType::OpenGL_OIT : RenderType::OpenGL;
		    	break;
		    case 1:
		    	config::RendererType = perPixel ? RenderType::Vulkan_OIT : RenderType::Vulkan;
		    	break;
		    case 2:
		    	config::RendererType = RenderType::DirectX9;
		    	break;
		    }
		}
		if (ImGui::BeginTabItem("Audio"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
			OptionCheckbox("Enable DSP", config::DSPEnabled,
					"Enable the Dreamcast Digital Sound Processor. Only recommended on fast platforms");
			if (OptionSlider("Volume Level", config::AudioVolume, 0, 100, "Adjust the emulator's audio level"))
			{
				config::AudioVolume.calcDbPower();
			};
#ifdef __ANDROID__
			if (config::AudioBackend.get() == "auto" || config::AudioBackend.get() == "android")
				OptionCheckbox("Automatic Latency", config::AutoLatency,
						"Automatically set audio latency. Recommended");
#endif
            if (!config::AutoLatency
            		|| (config::AudioBackend.get() != "auto" && config::AudioBackend.get() != "android"))
            {
				int latency = (int)roundf(config::AudioBufferSize * 1000.f / 44100.f);
				ImGui::SliderInt("Latency", &latency, 12, 512, "%d ms");
				config::AudioBufferSize = (int)roundf(latency * 44100.f / 1000.f);
				ImGui::SameLine();
				ShowHelpMarker("Sets the maximum audio latency. Not supported by all audio drivers.");
            }

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
		    header("CPU Mode");
		    {
				ImGui::Columns(2, "cpu_modes", false);
				OptionRadioButton("Dynarec", config::DynarecEnabled, true,
					"Use the dynamic recompiler. Recommended in most cases");
				ImGui::NextColumn();
				OptionRadioButton("Interpreter", config::DynarecEnabled, false,
					"Use the interpreter. Very slow but may help in case of a dynarec problem");
				ImGui::Columns(1, NULL, false);
		    }
		    if (config::DynarecEnabled)
		    {
		    	ImGui::Spacing();
		    	header("Dynarec Options");
		    	OptionCheckbox("Safe Mode", config::DynarecSafeMode,
		    			"Do not optimize integer division. Not recommended");
		    	OptionCheckbox("Idle Skip", config::DynarecIdleSkip, "Skip wait loops. Recommended");
		    }
	    	ImGui::Spacing();
		    header("Network");
		    {
		    	OptionCheckbox("Broadband Adapter Emulation", config::EmulateBBA,
		    			"Emulate the Ethernet Broadband Adapter (BBA) instead of the Modem");
		    	OptionCheckbox("Enable GGPO Networking", config::GGPOEnable,
		    			"Enable networking using GGPO");
		    	OptionCheckbox("Enable Naomi Networking", config::NetworkEnable,
		    			"Enable networking for supported Naomi games");
		    	if (config::GGPOEnable)
		    	{
					OptionCheckbox("Play as Player 1", config::ActAsServer,
							"Deselect to play as player 2");
					char server_name[256];
					strcpy(server_name, config::NetworkServer.get().c_str());
					ImGui::InputText("Peer", server_name, sizeof(server_name), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("Your peer IP address and optional port");
					config::NetworkServer.set(server_name);
					OptionSlider("Frame Delay", config::GGPODelay, 0, 20,
						"Sets Frame Delay, advisable for sessions with ping >100 ms");

					ImGui::Text("Left Thumbstick:");
					OptionRadioButton<int>("Disabled", config::GGPOAnalogAxes, 0, "Left thumbstick not used");
					ImGui::SameLine();
					OptionRadioButton<int>("Horizontal", config::GGPOAnalogAxes, 1, "Use the left thumbstick horizontal axis only");
					ImGui::SameLine();
					OptionRadioButton<int>("Full", config::GGPOAnalogAxes, 2, "Use the left thumbstick horizontal and vertical axes");

					OptionCheckbox("Network Statistics", config::NetworkStats,
			    			"Display network statistics on screen");
		    	}
		    	else if (config::NetworkEnable)
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
	    	ImGui::Spacing();
		    header("Other");
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
		    header("Flycast");
		    {
				ImGui::Text("Version: %s", GIT_VERSION);
				ImGui::Text("Git Hash: %s", GIT_HASH);
				ImGui::Text("Build Date: %s", BUILD_DATE);
		    }
	    	ImGui::Spacing();
		    header("Platform");
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
					"macOS"
#endif
#elif defined(_WIN32)
					"Windows"
#elif defined(__SWITCH__)
					"Switch"
#else
					"Unknown"
#endif
						);
#ifdef TARGET_IPHONE
				extern std::string iosJitStatus;
				ImGui::Text("JIT Status: %s", iosJitStatus.c_str());
#endif
		    }
	    	ImGui::Spacing();
	    	if (config::RendererType.isOpenGL())
	    	{
				header("Open GL");
	    		ImGui::Text("Renderer: %s", (const char *)glGetString(GL_RENDERER));
	    		ImGui::Text("Version: %s", (const char *)glGetString(GL_VERSION));
	    	}
#ifdef USE_VULKAN
	    	else if (config::RendererType.isVulkan())
	    	{
				header("Vulkan");
				std::string name = VulkanContext::Instance()->GetDriverName();
				ImGui::Text("Driver Name: %s", name.c_str());
				std::string version = VulkanContext::Instance()->GetDriverVersion();
				ImGui::Text("Version: %s", version.c_str());
	    	}
#endif
#ifdef _WIN32
	    	else if (config::RendererType.isDirectX())
	    	{
				if (ImGui::CollapsingHeader("DirectX", ImGuiTreeNodeFlags_DefaultOpen))
				{
		    		std::string name = theDXContext.getDriverName();
		    		ImGui::Text("Driver Name: %s", name.c_str());
		    		std::string version = theDXContext.getDriverVersion();
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

    scrollWhenDraggingOnVoid();
    windowDragScroll();
    ImGui::End();
    ImGui::PopStyleVar();
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
	ImGui::ShowDemoWindow();
}

static void gui_display_content()
{
	fullScreenWindow(false);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::Begin("##main", NULL, ImGuiWindowFlags_NoDecoration);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 8 * scaling));		// from 8, 4
    ImGui::AlignTextToFramePadding();
    ImGui::Indent(10 * scaling);
    ImGui::Text("GAMES");
    ImGui::Unindent(10 * scaling);

    static ImGuiTextFilter filter;
#if !defined(__ANDROID__) && !defined(TARGET_IPHONE)
	ImGui::SameLine(0, 32 * scaling);
	filter.Draw("Filter");
#endif
    if (gui_state != GuiState::SelectDisk)
    {
		ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f);
		if (ImGui::Button("Settings"))
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
							try {
								DiscSwap();
								gui_state = GuiState::Closed;
							} catch (const FlycastException& e) {
								error_msg = e.what();
							}
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
    windowDragScroll();
	ImGui::EndChild();
	ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();

	error_popup();
    contentpath_warning_popup();
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
	ImGui::OpenPopup("Select System Directory");
	select_file_popup("Select System Directory", &systemdir_selected_callback);
}

static std::future<bool> networkStatus;

static void gui_network_start()
{
	centerNextWindow();
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
		if (NetworkHandshake::instance->canStartNow())
			ImGui::Text("Press Start to start the game now.");
	}
	ImGui::Text("%s", get_notification().c_str());

	float currentwidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
	ImGui::SetCursorPosY(126.f * scaling);
	if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f)))
	{
		NetworkHandshake::instance->stop();
		networkStatus.get();
		gui_state = GuiState::Main;
		settings.imgread.ImagePath[0] = '\0';
	}
	ImGui::PopStyleVar();

	ImGui::End();

	if ((kcode[0] & DC_BTN_START) == 0)
		NetworkHandshake::instance->startNow();
}

static void gui_display_loadscreen()
{
	centerNextWindow();
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 180 * scaling));

    ImGui::Begin("##loading", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 10 * scaling));
    ImGui::AlignTextToFramePadding();
    ImGui::SetCursorPosX(20.f * scaling);
	if (dc_is_load_done())
	{
		try {
			dc_get_load_status();
			if (NetworkHandshake::instance != nullptr)
			{
				networkStatus = NetworkHandshake::instance->start();
				gui_state = GuiState::NetworkStart;
			}
			else
			{
				gui_state = GuiState::Closed;
				ImGui::Text("STARTING...");
			}
		} catch (const FlycastException& ex) {
			ERROR_LOG(BOOT, "%s", ex.what());
			error_msg = ex.what();
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
}

void gui_display_ui()
{
	if (gui_state == GuiState::Closed || gui_state == GuiState::VJoyEdit)
		return;
	if (gui_state == GuiState::Main)
	{
		std::string game_file = settings.imgread.ImagePath;
		if (!game_file.empty())
		{
#ifndef __ANDROID__
			commandLineStart = true;
#endif
			gui_start_game(game_file);
			return;
		}
	}

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

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
		gui_display_content();
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
		gui_display_vjoy_commands(scaling);
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
	case GuiState::Cheats:
		gui_cheats();
		break;
	default:
		die("Unknown UI state");
		break;
	}
    ImGui::Render();
    ImGui_impl_RenderDrawData(ImGui::GetDrawData());

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

	if (!message.empty() || config::FloatVMUs || crosshairsNeeded() || (ggpo::active() && config::NetworkStats))
	{
		ImGui_Impl_NewFrame();
		ImGui::NewFrame();

		if (!message.empty())
		{
			ImGui::SetNextWindowBgAlpha(0);
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always, ImVec2(0.f, 1.f));	// Lower left corner
			ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 0));

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
		if (ggpo::active() && config::NetworkStats)
			ggpo::displayStats();

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
static ImTextureID vmu_lcd_tex_ids[8];

static ImTextureID crosshairTexId;

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
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

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
	    	pos.x = ImGui::GetIO().DisplaySize.x - VMU_WIDTH - VMU_PADDING;
	    if (y == 0)
	    {
	    	pos.y = VMU_PADDING;
	    	if (i & 1)
	    		pos.y += VMU_HEIGHT + VMU_PADDING;
	    }
	    else
	    {
	    	pos.y = ImGui::GetIO().DisplaySize.y - VMU_HEIGHT - VMU_PADDING;
	    	if (i & 1)
	    		pos.y -= VMU_HEIGHT + VMU_PADDING;
	    }
	    ImVec2 pos_b(pos.x + VMU_WIDTH, pos.y + VMU_HEIGHT);
		ImGui::GetWindowDrawList()->AddImage(vmu_lcd_tex_ids[i], pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), 0xC0ffffff);
	}
    ImGui::End();
}

std::pair<float, float> getCrosshairPosition(int playerNum)
{
	float fx = mo_x_abs[playerNum];
	float fy = mo_y_abs[playerNum];
	float width = 640.f;
	float height = 480.f;

	if (config::Rotate90)
	{
		float t = fy;
		fy = 639.f - fx;
		fx = t;
		std::swap(width, height);
	}
	float scale = height / screen_height;
	fy /= scale;
	scale /= config::ScreenStretching / 100.f;
	fx = fx / scale + (screen_width - width / scale) / 2.f;

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
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

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
		pos.x -= (XHAIR_WIDTH * scaling) / 2.f;
		pos.y += (XHAIR_WIDTH * scaling) / 2.f;
		ImVec2 pos_b(pos.x + XHAIR_WIDTH * scaling, pos.y - XHAIR_HEIGHT * scaling);

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
