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
#include "gui.h"
#include "rend/osd.h"
#include "cfg/cfg.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "network/net_handshake.h"
#include "network/ice.h"
#include "input/gamepad_device.h"
#include "gui_util.h"
#include "imgread/common.h"
#include "emulator.h"
#include "mainui.h"
#include "lua/lua.h"
#include "gui_chat.h"
#include "imgui_driver.h"
#if FC_PROFILER
#include "implot.h"
#endif
#include "boxart/boxart.h"
#include "profiler/fc_profiler.h"
#include "hw/naomi/card_reader.h"
#include "oslib/resources.h"
#include "achievements/achievements.h"
#include "gui_achievements.h"
#include "IconsFontAwesome6.h"
#include <stb_image_write.h>
#include "hw/pvr/Renderer_if.h"
#include "rend/CustomTexture.h"
#include "hw/mem/addrspace.h"
#include "hw/maple/maple_if.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif
#include "vgamepad.h"
#include "settings.h"
#include "oslib/i18n.h"
using namespace i18n;

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <mutex>
#include <algorithm>

bool game_started;

int insetLeft, insetRight, insetTop, insetBottom;
std::unique_ptr<ImGuiDriver> imguiDriver;

static bool inited = false;
GuiState gui_state = GuiState::Main;
static bool commandLineStart;
std::string launchOnExitUri;
static u32 mouseButtons;
static int mouseX, mouseY;
static float mouseWheel;
static bool mouseTouchscreen;
static std::string error_msg;
static bool error_msg_shown;
static std::string osd_message;
static u64 osd_message_end;
static std::mutex osd_message_mutex;
static void (*showOnScreenKeyboard)(bool show);
static bool keysUpNextFrame[512];
bool uiUserScaleUpdated;
static bool clearActiveIdNextFrame;

GameScanner scanner;
static BackgroundGameLoader gameLoader;
static Boxart boxart;
static Chat chat;
static std::recursive_mutex guiMutex;
using LockGuard = std::lock_guard<std::recursive_mutex>;

ImFont *largeFont;
static Toast toast;
static ThreadRunner uiThreadRunner;

static void emuEventCallback(Event event, void *)
{
	switch (event)
	{
	case Event::Resume:
		game_started = true;
		vgamepad::startGame();
		break;
	case Event::Start:
		GamepadDevice::load_system_mappings();
		break;
	case Event::Terminate:
		GamepadDevice::load_system_mappings();
		game_started = false;
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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
#if FC_PROFILER
	ImPlot::CreateContext();
#endif
	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

	io.IniFilename = NULL;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    EventManager::listen(Event::Resume, emuEventCallback);
    EventManager::listen(Event::Start, emuEventCallback);
	EventManager::listen(Event::Terminate, emuEventCallback);
    ggpo::receiveChatMessages([](int playerNum, const std::string& msg) { chat.receive(playerNum, msg); });

#ifdef TARGET_UWP
	{
		// Detect when the on-screen keyboard is hidden and clear the text input widget id to validate the edit.
		// Otherwise the user will cancel the edit if he presses B, and must press A in the case of multi-line inputs.
		using namespace Windows::UI::ViewManagement;
		InputPane^ inputPane = InputPane::GetForCurrentView();
		if (inputPane)
		{
			inputPane->Hiding += ref new Windows::Foundation::TypedEventHandler<InputPane^, InputPaneVisibilityEventArgs^>(
				[](InputPane^, InputPaneVisibilityEventArgs^)
				{
					clearActiveIdNextFrame = true;
				});
		}
	}
#endif
}

static ImGuiKey keycodeToImGuiKey(u8 keycode)
{
	switch (keycode)
	{
		case 0x2B: return ImGuiKey_Tab;
		case 0x50: return ImGuiKey_LeftArrow;
		case 0x4F: return ImGuiKey_RightArrow;
		case 0x52: return ImGuiKey_UpArrow;
		case 0x51: return ImGuiKey_DownArrow;
		case 0x4B: return ImGuiKey_PageUp;
		case 0x4E: return ImGuiKey_PageDown;
		case 0x4A: return ImGuiKey_Home;
		case 0x4D: return ImGuiKey_End;
		case 0x49: return ImGuiKey_Insert;
		case 0x4C: return ImGuiKey_Delete;
		case 0x2A: return ImGuiKey_Backspace;
		case 0x2C: return ImGuiKey_Space;
		case 0x28: return ImGuiKey_Enter;
		case 0x29: return ImGuiKey_Escape;
		case 0x04: return ImGuiKey_A;
		case 0x06: return ImGuiKey_C;
		case 0x19: return ImGuiKey_V;
		case 0x1B: return ImGuiKey_X;
		case 0x1C: return ImGuiKey_Y;
		case 0x1D: return ImGuiKey_Z;
		case 0xE0:
		case 0xE4:
			return ImGuiMod_Ctrl;
		case 0xE1:
		case 0xE5:
			return ImGuiMod_Shift;
		case 0xE3:
		case 0xE7:
			return ImGuiMod_Super;
		default: return ImGuiKey_None;
	}
}

static bool addFont(const char *path, float size, ImFontConfig& fontConfig, const ImWchar *glyphRanges) {
	ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size, &fontConfig, glyphRanges);
	return font != nullptr;
}

static void addFont(const char *path[], float size, ImFontConfig& fontConfig, const ImWchar *glyphRanges)
{
	while (*path != nullptr)
		if (addFont(*path++, size, fontConfig, glyphRanges))
			break;
}

void gui_initFonts()
{
	static float uiScale;

	verify(inited);
	uiThreadRunner.init();

#if !defined(TARGET_UWP) && !defined(__SWITCH__)
	settings.display.uiScale = std::max(1.f, settings.display.dpi / 100.f * 0.75f);
   	// Limit scaling on small low-res screens
    if (settings.display.width <= 640 || settings.display.height <= 480)
    	settings.display.uiScale = std::min(1.2f, settings.display.uiScale);
#endif
    settings.display.uiScale *= config::UIScaling / 100.f;
	if (settings.display.uiScale == uiScale && ImGui::GetIO().Fonts->IsBuilt())
		return;
	uiScale = settings.display.uiScale;

    // Setup Dear ImGui style
	ImGui::GetStyle() = ImGuiStyle{};

    // Apply the current theme
    applyCurrentTheme();

    ImGui::GetStyle().TabRounding = 5.0f;
    ImGui::GetStyle().FrameRounding = 3.0f;
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 8);		// from 8,4
    ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 6);	// from 4,4
#if defined(__ANDROID__) || defined(TARGET_IPHONE) || defined(__SWITCH__)
    ImGui::GetStyle().TouchExtraPadding = ImVec2(1, 1);	// from 0,0
#endif
	if (settings.display.uiScale > 1)
		ImGui::GetStyle().ScaleAllSizes(settings.display.uiScale);

    static const ImWchar ranges[] =
    {
    	0x0020, 0xFFFF, // All chars
        0,
    };

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();

	// Regular font
	const float fontSize = uiScaled(17.f);
	size_t dataSize;
	std::unique_ptr<u8[]> data = resource::load("fonts/Roboto-Medium.ttf", dataSize);
	verify(data != nullptr);
	ImFont *regularFont = io.Fonts->AddFontFromMemoryTTF(data.release(), dataSize, fontSize, nullptr, ranges);
    ImFontConfig fontConfig;
    fontConfig.MergeMode = true;
    fontConfig.DstFont = regularFont;
	// Font Awesome symbols (added to default font)
	data = resource::load("fonts/" FONT_ICON_FILE_NAME_FAS, dataSize);
	verify(data != nullptr);
    fontConfig.FontNo = 0;
	static ImWchar faRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	io.Fonts->AddFontFromMemoryTTF(data.release(), dataSize, fontSize, &fontConfig, faRanges);

	// Large font
    const float largeFontSize = uiScaled(21.f);
	data = resource::load("fonts/Roboto-Regular.ttf", dataSize);
	verify(data != nullptr);
	largeFont = io.Fonts->AddFontFromMemoryTTF(data.release(), dataSize, largeFontSize, nullptr, ranges);
	ImFontConfig largeFontConfig;
	largeFontConfig.MergeMode = true;
	largeFontConfig.DstFont = largeFont;

#ifdef _WIN32
    u32 cp = GetACP();
    std::string fontDir = std::string(nowide::getenv("SYSTEMROOT")) + "\\Fonts\\";
    switch (cp)
    {
    case 932:	// Japanese
		{
			fontConfig.FontNo = 2;	// UIGothic
			largeFontConfig.FontNo = 2;
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "msgothic.ttc").c_str(), fontSize, &fontConfig, io.Fonts->GetGlyphRangesJapanese());
			io.Fonts->AddFontFromFileTTF((fontDir + "msgothic.ttc").c_str(), largeFontSize, &largeFontConfig, io.Fonts->GetGlyphRangesJapanese());
			fontConfig.FontNo = 2;	// Meiryo UI
			largeFontConfig.FontNo = 2;
			if (font == nullptr) {
				io.Fonts->AddFontFromFileTTF((fontDir + "Meiryo.ttc").c_str(), fontSize, &fontConfig, io.Fonts->GetGlyphRangesJapanese());
				io.Fonts->AddFontFromFileTTF((fontDir + "Meiryo.ttc").c_str(), largeFontSize, &largeFontConfig, io.Fonts->GetGlyphRangesJapanese());
			}
		}
		break;
    case 949:	// Korean
		{
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Malgun.ttf").c_str(), fontSize, &fontConfig, io.Fonts->GetGlyphRangesKorean());
			io.Fonts->AddFontFromFileTTF((fontDir + "Malgun.ttf").c_str(), largeFontSize, &largeFontConfig, io.Fonts->GetGlyphRangesKorean());
			if (font == nullptr)
			{
				fontConfig.FontNo = 2;	// Dotum
				io.Fonts->AddFontFromFileTTF((fontDir + "Gulim.ttc").c_str(), fontSize, &fontConfig, io.Fonts->GetGlyphRangesKorean());
				largeFontConfig.FontNo = 2;	// Dotum
				io.Fonts->AddFontFromFileTTF((fontDir + "Gulim.ttc").c_str(), largeFontSize, &largeFontConfig, io.Fonts->GetGlyphRangesKorean());
			}
		}
    	break;
    case 950:	// Traditional Chinese
		{
			fontConfig.FontNo = 1; // Microsoft JhengHei UI Regular
			ImFont* font = io.Fonts->AddFontFromFileTTF((fontDir + "Msjh.ttc").c_str(), fontSize, &fontConfig, GetGlyphRangesChineseTraditionalOfficial());
			largeFontConfig.FontNo = 1;
			io.Fonts->AddFontFromFileTTF((fontDir + "Msjh.ttc").c_str(), largeFontSize, &largeFontConfig, GetGlyphRangesChineseTraditionalOfficial());
			if (font == nullptr)
			{
				fontConfig.FontNo = 0;
				io.Fonts->AddFontFromFileTTF((fontDir + "MSJH.ttf").c_str(), fontSize, &fontConfig, GetGlyphRangesChineseTraditionalOfficial());
				largeFontConfig.FontNo = 0;
				io.Fonts->AddFontFromFileTTF((fontDir + "MSJH.ttf").c_str(), largeFontSize, &largeFontConfig, GetGlyphRangesChineseTraditionalOfficial());
			}
		}
    	break;
    case 936:	// Simplified Chinese
		io.Fonts->AddFontFromFileTTF((fontDir + "Simsun.ttc").c_str(), fontSize, &fontConfig, GetGlyphRangesChineseSimplifiedOfficial());
		io.Fonts->AddFontFromFileTTF((fontDir + "Simsun.ttc").c_str(), largeFontSize, &largeFontConfig, GetGlyphRangesChineseSimplifiedOfficial());
    	break;
    default:
    	break;
    }
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
    std::string fontDir = std::string("/System/Library/Fonts/");
    std::string locale = i18n::getCurrentLocale();

    if (locale.find("ja") == 0)             // Japanese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "ヒラギノ角ゴシック W4.ttc").c_str(), fontSize, &fontConfig, io.Fonts->GetGlyphRangesJapanese());
        io.Fonts->AddFontFromFileTTF((fontDir + "ヒラギノ角ゴシック W4.ttc").c_str(), largeFontSize, &largeFontConfig, io.Fonts->GetGlyphRangesJapanese());
    }
    else if (locale.find("ko") == 0)       // Korean
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "AppleSDGothicNeo.ttc").c_str(), fontSize, &fontConfig, io.Fonts->GetGlyphRangesKorean());
        io.Fonts->AddFontFromFileTTF((fontDir + "AppleSDGothicNeo.ttc").c_str(), largeFontSize, &largeFontConfig, io.Fonts->GetGlyphRangesKorean());
    }
    else if (locale.find("zh-Hant") == 0)  // Traditional Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), fontSize, &fontConfig, GetGlyphRangesChineseTraditionalOfficial());
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), largeFontSize, &largeFontConfig, GetGlyphRangesChineseTraditionalOfficial());
    }
    else if (locale.find("zh-Hans") == 0)  // Simplified Chinese
    {
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), fontSize, &fontConfig, GetGlyphRangesChineseSimplifiedOfficial());
        io.Fonts->AddFontFromFileTTF((fontDir + "PingFang.ttc").c_str(), largeFontSize, &largeFontConfig, GetGlyphRangesChineseSimplifiedOfficial());
    }
#elif defined(__ANDROID__)
    {
    	const ImWchar *glyphRanges = nullptr;
        std::string locale = i18n::getCurrentLocale();
        if (locale.find("ja") == 0)				// Japanese
        	glyphRanges = io.Fonts->GetGlyphRangesJapanese();
        else if (locale.find("ko") == 0)		// Korean
        	glyphRanges = io.Fonts->GetGlyphRangesKorean();
        else if (locale.find("zh_TW") == 0
        		|| locale.find("zh_HK") == 0)	// Traditional Chinese
        	glyphRanges = GetGlyphRangesChineseTraditionalOfficial();
        else if (locale.find("zh_CN") == 0)		// Simplified Chinese
        	glyphRanges = GetGlyphRangesChineseSimplifiedOfficial();

        if (glyphRanges != nullptr) {
        	io.Fonts->AddFontFromFileTTF("/system/fonts/NotoSansCJK-Regular.ttc", fontSize, &fontConfig, glyphRanges);
        	io.Fonts->AddFontFromFileTTF("/system/fonts/NotoSansCJK-Regular.ttc", largeFontSize, &largeFontConfig, glyphRanges);
        }
    }

#elif defined(__linux__)
	std::string locale = i18n::getCurrentLocale();
	if (locale.find("ja_") == 0)			// Japanese
	{
		const char *fonts[] = {
				"/usr/share/fonts/opentype/ipafont-gothic/ipagp.ttf",
				"/usr/share/fonts/ipa-pgothic-fonts/ipagp.ttf",	// redhat
				"/usr/share/fonts/truetype/takao-gothic/TakaoPGothic.ttf",
				"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
				"/usr/share/fonts/adobe-source-han-sans-jp-fonts/SourceHanSansJP-Regular.otf", // redhat
				"/usr/share/fonts/vl-gothic-fonts/VL-Gothic-Regular.ttf", // redhat
				"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
				"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc", // redhat
				nullptr
		};
		const char *largeFonts[] = {
				"/usr/share/fonts/opentype/ipafont-gothic/ipagp.ttf",
				"/usr/share/fonts/ipa-pgothic-fonts/ipagp.ttf",	// redhat
				"/usr/share/fonts/truetype/takao-gothic/TakaoPGothic.ttf",
				"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
				"/usr/share/fonts/adobe-source-han-sans-jp-fonts/SourceHanSansJP-Bold.otf", // redhat
				"/usr/share/fonts/vl-gothic-fonts/VL-Gothic-Regular.ttf", // redhat
				"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
				"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc", // redhat
				nullptr
		};
		addFont(fonts, fontSize, fontConfig, io.Fonts->GetGlyphRangesJapanese());
		addFont(largeFonts, largeFontSize, largeFontConfig, io.Fonts->GetGlyphRangesJapanese());
	}
	else if (locale.find("ko_") == 0)		// Korean
	{
		const char *fonts[] = {
				"/usr/share/fonts/truetype/unfonts-core/UnDotum.ttf",
				"/usr/share/fonts-droid-fallback/truetype/DroidSansFallback.ttf",
				"/usr/share/fonts/baekmuk-dotum-fonts/dotum.ttf", // redhat
				"/usr/share/fonts/adobe-source-han-sans-kr-fonts/SourceHanSansKR-Regular.otf", // redhat
				"/usr/share/fonts/naver-nanum-gothic-coding-fonts/NanumGothic_Coding.ttf", // redhat
				"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
				"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc", // redhat
				nullptr
		};
		const char *largeFonts[] = {
				"/usr/share/fonts/truetype/unfonts-core/UnDotumBold.ttf",
				"/usr/share/fonts-droid-fallback/truetype/DroidSansFallback.ttf",
				"/usr/share/fonts/adobe-source-han-sans-kr-fonts/SourceHanSansKR-Bold.otf", // redhat
				"/usr/share/fonts/naver-nanum-gothic-coding-fonts/NanumGothic_Coding_Bold.ttf", // redhat
				"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
				"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc", // redhat
				nullptr
		};
		addFont(fonts, fontSize, fontConfig, io.Fonts->GetGlyphRangesKorean());
		addFont(largeFonts, largeFontSize, largeFontConfig, io.Fonts->GetGlyphRangesKorean());
	}
	else if (locale.find("zh_") == 0)		// Chinese
	{
		const ImWchar *glyphRanges = GetGlyphRangesChineseSimplifiedOfficial();
		if (locale.find("zh_TW") == 0 || locale.find("zh_HK") == 0)
			glyphRanges = GetGlyphRangesChineseTraditionalOfficial();
		const char *fonts[] = {
				"/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
				"/usr/share/fonts/wqy-zenhei-fonts/wqy-zenhei.ttc", // redhat
				"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
				"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
				"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc", // redhat
		};
		const char *largeFonts[] = {
				"/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
				"/usr/share/fonts/wqy-zenhei-fonts/wqy-zenhei.ttc", // redhat
				"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
				"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
				"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc", // redhat
		};
		addFont(fonts, fontSize, fontConfig, glyphRanges);
		addFont(largeFonts, largeFontSize, largeFontConfig, glyphRanges);
	}

	// TODO BSD, iOS, ...
#endif
    NOTICE_LOG(RENDERER, "Screen DPI is %.0f, size %d x %d. Scaling by %.2f", settings.display.dpi, settings.display.width, settings.display.height, settings.display.uiScale);
	vgamepad::applyUiScale();
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

void gui_keyboard_key(u8 keyCode, bool pressed)
{
	if (!inited)
		return;
	ImGuiKey key = keycodeToImGuiKey(keyCode);
	if (key == ImGuiKey_None)
		return;
	if (!pressed && ImGui::IsKeyDown(key))
	{
		keysUpNextFrame[keyCode] = true;
		return;
	}
	ImGuiIO& io = ImGui::GetIO();
	io.AddKeyEvent(key, pressed);
}

bool gui_keyboard_captured() {
	ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard;
}

bool gui_mouse_captured() {
	ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureMouse;
}

void gui_set_mouse_position(int x, int y, bool touchscreen)
{
	mouseX = std::round(x * settings.display.pointScale);
	mouseY = std::round(y * settings.display.pointScale);
	mouseTouchscreen = touchscreen;
}

void gui_set_mouse_button(int button, bool pressed, bool touchscreen)
{
	if (pressed)
		mouseButtons |= 1 << button;
	else
		mouseButtons &= ~(1 << button);
	mouseTouchscreen = touchscreen;
}

void gui_set_mouse_wheel(float delta) {
	mouseWheel += delta;
	mouseTouchscreen = false;
}

static void gui_newFrame()
{
	imguiDriver->newFrame();
	ImGui::GetIO().DisplaySize.x = settings.display.width;
	ImGui::GetIO().DisplaySize.y = settings.display.height;

	ImGuiIO& io = ImGui::GetIO();

	io.AddMouseSourceEvent(mouseTouchscreen ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
	if (mouseX < 0 || mouseX >= settings.display.width || mouseY < 0 || mouseY >= settings.display.height)
		io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
	else
		io.AddMousePosEvent(mouseX, mouseY);
	if (io.WantCaptureMouse)
	{
		io.AddMouseWheelEvent(0, -mouseWheel / 16);
		mouseWheel = 0;
	}
	io.AddMouseButtonEvent(ImGuiMouseButton_Left, (mouseButtons & (1 << 0)) != 0);
	io.AddMouseButtonEvent(ImGuiMouseButton_Right, (mouseButtons & (1 << 1)) != 0);
	io.AddMouseButtonEvent(ImGuiMouseButton_Middle, (mouseButtons & (1 << 2)) != 0);
	io.AddMouseButtonEvent(3, (mouseButtons & (1 << 3)) != 0);

	// shows a popup navigation window even in game because of the OSD
	//io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, ((kcode[0] & DC_BTN_X) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadFaceRight, ((kcode[0] & DC_BTN_B) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadFaceUp, ((kcode[0] & DC_BTN_Y) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadFaceDown, ((kcode[0] & DC_BTN_A) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, ((kcode[0] & DC_DPAD_LEFT) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadDpadRight, ((kcode[0] & DC_DPAD_RIGHT) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadDpadUp, ((kcode[0] & DC_DPAD_UP) == 0));
	io.AddKeyEvent(ImGuiKey_GamepadDpadDown, ((kcode[0] & DC_DPAD_DOWN) == 0));

	float analog;
	analog = joyx[0] < 0 ? -(float)joyx[0] / 32768.f : 0.f;
	io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, analog > 0.1f, analog);
	analog = joyx[0] > 0 ? (float)joyx[0] / 32768.f : 0.f;
	io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, analog > 0.1f, analog);
	analog = joyy[0] < 0 ? -(float)joyy[0] / 32768.f : 0.f;
	io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, analog > 0.1f, analog);
	analog = joyy[0] > 0 ? (float)joyy[0] / 32768.f : 0.f;
	io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, analog > 0.1f, analog);

	ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);

	if (showOnScreenKeyboard != nullptr)
		showOnScreenKeyboard(io.WantTextInput);
	if (clearActiveIdNextFrame && io.WantTextInput)
	{
		ImGui::ClearActiveID();
		clearActiveIdNextFrame = false;
	}
}

// SDL on-screen keyboard: Delay keys up by one frame to allow quick key presses.
static void delayedKeysUp()
{
	ImGuiIO& io = ImGui::GetIO();
	for (u32 i = 0; i < std::size(keysUpNextFrame); i++)
		if (keysUpNextFrame[i])
			io.AddKeyEvent(keycodeToImGuiKey(i), false);
	memset(keysUpNextFrame, 0, sizeof(keysUpNextFrame));
}

static void gui_endFrame(bool gui_open) {
    imguiDriver->renderDrawData(ImGui::GetDrawData(), gui_open);
    delayedKeysUp();
}

void gui_setOnScreenKeyboardCallback(void (*callback)(bool show)) {
	showOnScreenKeyboard = callback;
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
#include <vector>
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
	const LockGuard lock(guiMutex);
	if (gui_state == GuiState::Closed && !settings.naomi.slave)
	{
		if (!ggpo::active())
		{
			if (achievements::canPause())
			{
				vgamepad::hide();
				try {
					emu.stop();
					gui_setState(GuiState::Commands);
				} catch (const FlycastException& e) {
					gui_stop_game(e.what());
				}
			}
		}
		else
		{
			chat.toggle();
		}
	}
	else if (gui_state == GuiState::VJoyEdit)
	{
		vgamepad::pauseEditing();
		// iOS: force a touch up event to make up for the one eaten by the tap gesture recognizer
		mouseButtons &= ~1;
		gui_setState(GuiState::VJoyEditCommands);
	}
	else if (gui_state == GuiState::Loading)
	{
		gameLoader.cancel();
	}
	else if (gui_state == GuiState::Commands)
	{
		gui_setState(GuiState::Closed);
		GamepadDevice::load_system_mappings();
		emu.start();
	}
}

void gui_start_game(const std::string& path)
{
	const LockGuard lock(guiMutex);
	if (gui_state != GuiState::Main && gui_state != GuiState::Closed && gui_state != GuiState::Commands)
		return;
	emu.unloadGame();
	reset_vmus();
    chat.reset();

	scanner.stop();
	gui_setState(GuiState::Loading);
	gameLoader.load(path);
}

void gui_stop_game(const std::string& message)
{
	const LockGuard lock(guiMutex);
	if (!commandLineStart)
	{
		// Exit to main menu
		emu.unloadGame();
		gui_setState(GuiState::Main);
		reset_vmus();
		if (!message.empty())
			gui_error(Ts("Flycast has stopped.") + "\n\n" + message);
	}
	else
	{
		if (!message.empty())
			ERROR_LOG(COMMON, "Flycast has stopped: %s", message.c_str());
		// Exit emulator
		dc_exit();
	}
}

static void appendVectorData(void *context, void *data, int size)
{
	std::vector<u8>& v = *(std::vector<u8> *)context;
	const u8 *bytes = (const u8 *)data;
	v.insert(v.end(), bytes, bytes + size);
}

static void getScreenshot(std::vector<u8>& data, int width = 0)
{
	data.clear();
	std::vector<u8> rawData;
	int height = 0;
	if (renderer == nullptr || !renderer->GetLastFrame(rawData, width, height))
		return;
	stbi_flip_vertically_on_write(0);
	stbi_write_png_to_func(appendVectorData, &data, width, height, 3, &rawData[0], 0);
}

static void savestate()
{
	// TODO save state async: png compression, savestate file compression/write
	std::vector<u8> pngData;
	getScreenshot(pngData, 640);
	dc_savestate(config::SavestateSlot, pngData.empty() ? nullptr : &pngData[0], pngData.size());
	ImguiStateTexture savestatePic;
	savestatePic.invalidate();
}

static void gui_display_commands()
{
	fullScreenWindow(false);
	ImGui::SetNextWindowBgAlpha(0.8f);
	ImguiStyleVar _{ImGuiStyleVar_WindowBorderSize, 0};

	ImGui::Begin("##commands", NULL, ImGuiWindowFlags_NoDecoration);
	{
		ImguiStyleVar _{ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)};	// left aligned

		float columnWidth = std::min(200.f,
				(ImGui::GetContentRegionAvail().x - uiScaled(100 + 150) - ImGui::GetStyle().FramePadding.x * 2)
				/ 2 / uiScaled(1));
		float buttonWidth = 150.f;	// not scaled
		bool lowWidth = ImGui::GetContentRegionAvail().x < uiScaled(100 + buttonWidth * 3)
				+ ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x * 2;
		if (lowWidth)
			buttonWidth = std::min(150.f,
					(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ItemSpacing.x * 2)
					/ 3 / uiScaled(1));
		bool lowHeight = ImGui::GetContentRegionAvail().y < uiScaled(100 + 50 * 2 + buttonWidth * 3 / 4) + ImGui::GetTextLineHeightWithSpacing() * 2
				+ ImGui::GetStyle().ItemSpacing.y * 2 + ImGui::GetStyle().WindowPadding.y;

		GameMedia game;
		game.path = settings.content.path;
		game.fileName = settings.content.fileName;
		GameBoxart art = boxart.getBoxart(game);
		ImguiFileTexture tex(art.boxartPath);
		// TODO use placeholder image if not available
		tex.draw(ScaledVec2(100, 100));

		ImGui::SameLine();
		if (!lowHeight)
		{
			ImGui::BeginChild("game_info", ScaledVec2(0, 100.f), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
			ImGui::PushFont(largeFont);
			ImGui::Text("%s", art.name.c_str());
			ImGui::PopFont();
			{
				ImguiStyleColor _(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.f));
				ImGui::TextWrapped("%s", art.fileName.c_str());
			}
			ImGui::EndChild();
		}

		if (lowWidth) {
			ImGui::Columns(3, "buttons", false);
		}
		else
		{
			ImGui::Columns(4, "buttons", false);
			ImGui::SetColumnWidth(0, uiScaled(100.f)  + ImGui::GetStyle().ItemSpacing.x);
			ImGui::SetColumnWidth(1, uiScaled(columnWidth));
			ImGui::SetColumnWidth(2, uiScaled(columnWidth));
			const ImVec2 vmuPos = ImGui::GetStyle().WindowPadding + ScaledVec2(0.f, 100.f)
					+ ImVec2(insetLeft, ImGui::GetStyle().ItemSpacing.y);
			ImguiVmuTexture::displayVmus(vmuPos);
			ImGui::NextColumn();
		}
		ImguiStyleVar _1{ImGuiStyleVar_FramePadding, ScaledVec2(12.f, 3.f)};

		// Resume
		if (IconButton(ICON_FA_PLAY, T("Resume"), ScaledVec2(buttonWidth, 50)).realize())
		{
			GamepadDevice::load_system_mappings();
			gui_setState(GuiState::Closed);
		}
		// Cheats
		{
			DisabledScope _{settings.network.online || settings.raHardcoreMode};

			if (IconButton(ICON_FA_MASK, T("Cheats"), ScaledVec2(buttonWidth, 50)).realize() && !settings.network.online)
				gui_setState(GuiState::Cheats);
		}
		// Achievements
		{
			DisabledScope _{!achievements::isActive()};

			if (IconButton(ICON_FA_TROPHY, T("Achievements"), ScaledVec2(buttonWidth, 50)).realize() && achievements::isActive())
				gui_setState(GuiState::Achievements);
		}
		// Barcode
		if (card_reader::barcodeAvailable())
		{
			ImGui::Text("%s", T("Barcode Card"));
			char cardBuf[64] {};
			strncpy(cardBuf, card_reader::barcodeGetCard().c_str(), sizeof(cardBuf) - 1);
			ImGui::SetNextItemWidth(uiScaled(buttonWidth));
			if (InputText("##barcode", cardBuf, sizeof(cardBuf), ImGuiInputTextFlags_None))
				card_reader::barcodeSetCard(cardBuf);
		}

		ImGui::NextColumn();

		// Insert/Eject Disk
		std::string disk_label = gdr::isOpen() ? T("Insert Disk") : T("Eject Disk");
		if (IconButton(ICON_FA_COMPACT_DISC, disk_label, ScaledVec2(buttonWidth, 50)).realize())
		{
			if (gdr::isOpen()) {
				gui_setState(GuiState::SelectDisk);
			}
			else {
				emu.openGdrom();
				gui_setState(GuiState::Closed);
			}
		}
		// Settings
		if (IconButton(ICON_FA_GEAR, T("Settings"), ScaledVec2(buttonWidth, 50)).realize())
			gui_setState(GuiState::Settings);

		// Exit
		if (IconButton(ICON_FA_POWER_OFF, commandLineStart ?  T("Exit") : T("Close Game"), ScaledVec2(buttonWidth, 50)).realize())
			gui_stop_game();

		ImGui::NextColumn();
		{
			DisabledScope _{!dc_savestateAllowed()};
			ImguiStateTexture savestatePic;
			time_t savestateDate = dc_getStateCreationDate(config::SavestateSlot);

			// Load State
			{
				DisabledScope _{settings.raHardcoreMode || savestateDate == 0};
				if (IconButton(ICON_FA_CLOCK_ROTATE_LEFT, T("Load State"), ScaledVec2(buttonWidth, 50)).realize() && dc_savestateAllowed())
				{
					gui_setState(GuiState::Closed);
					dc_loadstate(config::SavestateSlot);
				}
			}

			// Save State
			if (IconButton(ICON_FA_DOWNLOAD, T("Save State"), ScaledVec2(buttonWidth, 50)).realize() && dc_savestateAllowed())
			{
				gui_setState(GuiState::Closed);
				savestate();
			}

			// Slot #
			if (ImGui::ArrowButton("##prev-slot", ImGuiDir_Left))
			{
				if (config::SavestateSlot == 0)
					config::SavestateSlot = 9;
				else
					config::SavestateSlot--;
				SaveSettings();
			}
			std::string slot = strprintf(T("Slot %d"), (int)config::SavestateSlot + 1);
			float spacingW = (uiScaled(buttonWidth) - ImGui::GetFrameHeight() * 2 - ImGui::CalcTextSize(slot.c_str()).x) / 2;
			ImGui::SameLine(0, spacingW);
			ImGui::Text("%s", slot.c_str());
			ImGui::SameLine(0, spacingW);
			if (ImGui::ArrowButton("##next-slot", ImGuiDir_Right))
			{
				if (config::SavestateSlot == 9)
					config::SavestateSlot = 0;
				else
					config::SavestateSlot++;
				SaveSettings();
			}
			{
				ImVec4 gray(0.75f, 0.75f, 0.75f, 1.f);
				if (savestateDate == 0)
					ImGui::TextColored(gray, "%s", T("Empty"));
				else
					ImGui::TextColored(gray, "%s", timeToShortDateTimeString(savestateDate).c_str());
			}
			savestatePic.draw(ScaledVec2(buttonWidth, 0.f));
		}

		ImGui::Columns(1, nullptr, false);
	}
	ImGui::End();
}

void error_popup()
{
	if (!error_msg_shown && !error_msg.empty())
	{
		ImVec2 padding = ScaledVec2(20, 20);
		ImguiStyleVar _(ImGuiStyleVar_WindowPadding, padding);
		ImguiStyleVar _1(ImGuiStyleVar_ItemSpacing, padding);
		ImGui::OpenPopup("Error");
		if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + uiScaled(400.f));
			ImGui::TextWrapped("%s", error_msg.c_str());
			{
				ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(16, 3));
				float currentwidth = ImGui::GetContentRegionAvail().x;
				ImGui::SetCursorPosX((currentwidth - uiScaled(80.f)) / 2.f + ImGui::GetStyle().WindowPadding.x);
				if (ImGui::Button(T("OK"), ScaledVec2(80.f, 0)))
				{
					error_msg.clear();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SetItemDefaultFocus();
			ImGui::PopTextWrapPos();
			ImGui::EndPopup();
		}
		error_msg_shown = true;
	}
}

static void contentpath_warning_popup()
{
    static bool show_contentpath_selection;

    if (scanner.content_path_looks_incorrect)
    {
        ImGui::OpenPopup(T("Incorrect Content Location?"));
        if (ImGui::BeginPopupModal(T("Incorrect Content Location?"), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + uiScaled(400.f));
            ImGui::TextWrapped((std::string("  ") + T("Scanned %d folders but no game can be found!") + std::string("  ")).c_str(), scanner.empty_folders_scanned);
			{
				ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(16, 3));
				float currentwidth = ImGui::GetContentRegionAvail().x;
				ImGui::SetCursorPosX((currentwidth - uiScaled(100.f)) / 2.f + ImGui::GetStyle().WindowPadding.x - uiScaled(55.f));
				if (ImGui::Button(T("Reselect"), ScaledVec2(100.f, 0)))
				{
					scanner.content_path_looks_incorrect = false;
					ImGui::CloseCurrentPopup();
					show_contentpath_selection = true;
				}

				ImGui::SameLine();
				ImGui::SetCursorPosX((currentwidth - uiScaled(100.f)) / 2.f + ImGui::GetStyle().WindowPadding.x + uiScaled(55.f));
				if (ImGui::Button(T("Cancel"), ScaledVec2(100.f, 0)))
				{
					scanner.content_path_looks_incorrect = false;
					ImGui::CloseCurrentPopup();
					scanner.stop();
					config::ContentPath.get().clear();
				}
			}
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
    }
    if (show_contentpath_selection)
    {
        scanner.stop();
        const char *title = T("Select a Content Folder");
        ImGui::OpenPopup(title);
        select_file_popup(title, [](bool cancelled, std::string selection)
        {
            show_contentpath_selection = false;
            if (!cancelled)
            {
            	config::ContentPath.get().clear();
                config::ContentPath.get().push_back(selection);
            }
            scanner.refresh();
            return true;
        });
    }
}

void os_notify(const char *msg, int durationMs, const char *details)
{
	if (gui_state != GuiState::Closed)
	{
		std::lock_guard<std::mutex> _{osd_message_mutex};
		osd_message = msg;
		osd_message_end = getTimeMs() + durationMs;
	}
	else {
		toast.show(msg, details != nullptr ? details : "", durationMs);
	}
}

static std::string get_notification()
{
	std::lock_guard<std::mutex> lock(osd_message_mutex);
	if (!osd_message.empty() && getTimeMs() >= osd_message_end)
		osd_message.clear();
	return osd_message;
}

inline static void gui_display_demo() {
	ImGui::ShowDemoWindow();
}

static void gameTooltip(const std::string& tip)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
        ImGui::TextUnformatted(tip.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static bool gameImageButton(ImguiTexture& texture, const std::string& tooltip, ImVec2 size, const std::string& gameName)
{
	bool pressed = texture.button("##imagebutton", size, gameName);
	gameTooltip(tooltip);

    return pressed;
}

#ifdef TARGET_UWP
void gui_load_game()
{
	using namespace Windows::Storage;
	using namespace Concurrency;

	auto picker = ref new Pickers::FileOpenPicker();
	picker->ViewMode = Pickers::PickerViewMode::List;

	picker->FileTypeFilter->Append(".chd");
	picker->FileTypeFilter->Append(".gdi");
	picker->FileTypeFilter->Append(".cue");
	picker->FileTypeFilter->Append(".cdi");
	picker->FileTypeFilter->Append(".zip");
	picker->FileTypeFilter->Append(".7z");
	picker->FileTypeFilter->Append(".elf");
	if (!config::HideLegacyNaomiRoms)
	{
		picker->FileTypeFilter->Append(".bin");
		picker->FileTypeFilter->Append(".lst");
		picker->FileTypeFilter->Append(".dat");
	}
	picker->SuggestedStartLocation = Pickers::PickerLocationId::DocumentsLibrary;

	create_task(picker->PickSingleFileAsync()).then([](StorageFile ^file) {
		if (file)
		{
			NOTICE_LOG(COMMON, "Picked file: %S", file->Path->Data());
			nowide::stackstring path;
			if (path.convert(file->Path->Data()))
				gui_start_game(path.get());
		}
	});
}
#endif

static void gui_display_content()
{
	fullScreenWindow(false);
	ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
	ImguiStyleVar _1(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::Begin("##main", NULL, ImGuiWindowFlags_NoDecoration);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
    ImGui::AlignTextToFramePadding();
    ImGui::Indent(uiScaled(10));
    ImGui::Text("%s", T("GAMES"));
    ImGui::Unindent(uiScaled(10));

    static ImGuiTextFilter filter;
    IconButton settingsBtn(ICON_FA_GEAR, T("Settings"));
#if !defined(__ANDROID__) && !defined(TARGET_IPHONE) && !defined(TARGET_UWP) && !defined(__SWITCH__)
	ImGui::SameLine(0, uiScaled(32));
	filter.Draw(T("Filter"), ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x
			- settingsBtn.width() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize(T("Filter")).x);
#endif
    if (gui_state != GuiState::SelectDisk)
    {
#ifdef TARGET_UWP
		ImGui::SameLine(ImGui::GetContentRegionMax().x - settingsBtn.width()
				- ImGui::GetStyle().FramePadding.x * 2.0f  - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize(T("Load...")).x);
		if (ImGui::Button(T("Load...")))
			gui_load_game();
		ImGui::SameLine();
#elif defined(__SWITCH__)
		IconButton exitBtn(ICON_FA_POWER_OFF, T("Exit"));
		ImGui::SameLine(ImGui::GetContentRegionMax().x - settingsBtn.width()
				- ImGui::GetStyle().ItemSpacing.x - exitBtn.width());
		if (exitBtn.realize())
			dc_exit();
		ImGui::SameLine();
#else
		ImGui::SameLine(ImGui::GetContentRegionMax().x - settingsBtn.width());
#endif
		if (settingsBtn.realize())
			gui_setState(GuiState::Settings);
    }
    else
    {
    	IconButton cancelBtn(T("Cancel"));
		ImGui::SameLine(ImGui::GetContentRegionMax().x - cancelBtn.width());
		if (cancelBtn.realize())
			gui_setState(GuiState::Commands);
    }
    ImGui::PopStyleVar();

    scanner.fetch_game_list();

	// Only if Filter and Settings aren't focused... ImGui::SetNextWindowFocus();
	ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_DragScrolling);
    {
		const float totalWidth = ImGui::GetContentRegionMax().x - (!ImGui::GetCurrentWindow()->ScrollbarY ? ImGui::GetStyle().ScrollbarSize : 0);
		const int itemsPerLine = std::max<int>(totalWidth / (uiScaled(150) + ImGui::GetStyle().ItemSpacing.x), 1);
		const float responsiveBoxSize = totalWidth / itemsPerLine - ImGui::GetStyle().FramePadding.x * 2;
		const ImVec2 responsiveBoxVec2 = ImVec2(responsiveBoxSize, responsiveBoxSize);

		if (config::BoxartDisplayMode)
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
		else
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ScaledVec2(8, 20));

		int counter = 0;
		bool gameListEmpty = false;
		{
			scanner.get_mutex().lock();
			gameListEmpty = scanner.get_game_list().empty();
			for (const auto& game : scanner.get_game_list())
			{
				if (gui_state == GuiState::SelectDisk)
				{
					std::string extension = get_file_extension(game.path);
					if (!game.device && extension != "gdi" && extension != "chd"
							&& extension != "cdi" && extension != "cue")
						// Only dreamcast disks
						continue;
					if (game.path.empty())
						// Dreamcast BIOS isn't a disk
						continue;
				}
				std::string gameName = game.name;
				GameBoxart art;
				if (config::BoxartDisplayMode && !game.device)
				{
					art = boxart.getBoxartAndLoad(game);
					gameName = art.name;
				}
				if (filter.PassFilter(gameName.c_str()))
				{
					ImguiID _(game.path.empty() ? "bios" : game.path);
					bool pressed = false;
					if (config::BoxartDisplayMode)
					{
						if (counter % itemsPerLine != 0)
							ImGui::SameLine();
						counter++;
						// Put the image inside a child window so we can detect when it's fully clipped and doesn't need to be loaded
						if (ImGui::BeginChild("img", ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_NavFlattened))
						{
							ImguiFileTexture tex(art.boxartPath);
							pressed = gameImageButton(tex, game.name, responsiveBoxVec2, gameName);
						}
						ImGui::EndChild();
					}
					else
					{
						pressed = ImGui::Selectable(gameName.c_str());
					}
					if (pressed)
					{
						if (!config::BoxartDisplayMode)
							art = boxart.getBoxart(game);
						settings.content.title = art.name;
						if (settings.content.title.empty() || settings.content.title == game.fileName)
							settings.content.title = get_file_basename(game.fileName);
						if (gui_state == GuiState::SelectDisk)
						{
							try {
								emu.insertGdrom(game.path);
								gui_setState(GuiState::Closed);
							} catch (const FlycastException& e) {
								gui_error(e.what());
							}
						}
						else
						{
							std::string gamePath(game.path);
							scanner.get_mutex().unlock();
							gui_start_game(gamePath);
							scanner.get_mutex().lock();
							break;
						}
					}
				}
			}
			scanner.get_mutex().unlock();
		}
		bool addContent = false;
#if !defined(TARGET_IPHONE)
		if (gameListEmpty && gui_state != GuiState::SelectDisk)
		{
			const char *label = T("Your game list is empty");
			// center horizontally
			const float w = largeFont->CalcTextSizeA(largeFont->LegacySize, FLT_MAX, -1.f, label).x + ImGui::GetStyle().FramePadding.x * 2;
			ImGui::SameLine((ImGui::GetContentRegionMax().x - w) / 2);
			if (ImGui::BeginChild("empty", ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_NavFlattened))
			{
				ImGui::PushFont(largeFont);
				ImGui::NewLine();
				ImGui::Text("%s", label);
				ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
				addContent = ImGui::Button(T("Add Game Folder"));
				ImGui::PopFont();
			}
			ImGui::EndChild();
		}
#endif
        ImGui::PopStyleVar();
        addContentPath(addContent);
    }
    scrollWhenDraggingOnVoid();
    windowDragScroll();
	ImGui::EndChild();
	ImGui::End();

    contentpath_warning_popup();
}

static bool systemdir_selected_callback(bool cancelled, std::string selection)
{
	if (cancelled)
	{
		gui_setState(GuiState::Main);
		return true;
	}
	selection += "/";

	std::string data_path = selection + "data/";
	if (!file_exists(data_path))
	{
		if (!make_directory(data_path))
		{
			WARN_LOG(BOOT, "Cannot create 'data' directory: %s", data_path.c_str());
			gui_error(Ts("Invalid selection:") + '\n' + Ts("Flycast cannot write to this folder."));
			return false;
		}
	}
	// We might be able to create a directory but not a file. Because ... android
	// So let's test to be sure.
	std::string testPath = data_path + "writetest.txt";
	FILE *file = fopen(testPath.c_str(), "w");
	if (file == nullptr)
	{
		WARN_LOG(BOOT, "Cannot write in the 'data' directory");
		gui_error(Ts("Invalid selection:") + '\n' + Ts("Flycast cannot write to this folder."));
		return false;
	}
	fclose(file);
	unlink(testPath.c_str());

	set_user_config_dir(selection);
	add_system_data_dir(selection);
	set_user_data_dir(data_path);

	if (config::open())
	{
		config::Settings::instance().load(false);
		// Make sure the renderer type doesn't change mid-flight
		config::RendererType = RenderType::OpenGL;
		gui_setState(GuiState::Main);
		if (config::ContentPath.get().empty())
		{
			scanner.stop();
			config::ContentPath.get().push_back(selection);
		}
		SaveSettings();
	}
	return true;
}

static void gui_display_onboarding()
{
	const char *title = T("Select Flycast Home Folder");
	ImGui::OpenPopup(title);
	select_file_popup(title, &systemdir_selected_callback);
}

static void drawBoxartBackground()
{
	GameMedia game;
	game.path = settings.content.path;
	game.fileName = settings.content.fileName;
	GameBoxart art = boxart.getBoxart(game);
	ImguiFileTexture tex(art.boxartPath);
	ImDrawList *dl = ImGui::GetBackgroundDrawList();
	tex.draw(dl, ImVec2(0, 0), ImVec2(settings.display.width, settings.display.height), 1.f);
}

static std::future<bool> networkStatus;

static void gui_network_start()
{
	drawBoxartBackground();
	centerNextWindow();
	ImGui::SetNextWindowSize(ScaledVec2(330, 0));
	ImGui::SetNextWindowBgAlpha(0.8f);
	ImguiStyleVar _1(ImGuiStyleVar_WindowPadding, ScaledVec2(20, 20));

	if (ImGui::Begin("##network", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 10));
		ImGui::AlignTextToFramePadding();
		ImGui::SetCursorPosX(uiScaled(20.f));

		if (networkStatus.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
		{
			ImGui::Text("%s", T("Starting..."));
			try {
				if (networkStatus.get())
					gui_setState(GuiState::Closed);
				else
					gui_stop_game();
			} catch (const FlycastException& e) {
				gui_stop_game(e.what());
			}
		}
		else
		{
			ImGui::Text("%s", T("Starting Network..."));
			if (NetworkHandshake::instance->canStartNow())
				ImGui::TextWrapped("%s", T("Press Start to start the game now."));
		}
		ImGui::Text("%s", get_notification().c_str());

		float currentwidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((currentwidth - uiScaled(100.f)) / 2.f + ImGui::GetStyle().WindowPadding.x);
		if (ImGui::Button(T("Cancel"), ScaledVec2(100.f, 0)) && NetworkHandshake::instance != nullptr)
		{
			NetworkHandshake::instance->stop();
			try {
				networkStatus.get();
			}
			catch (const FlycastException&) {
			}
			gui_stop_game();
		}
	}
	ImGui::End();

	if ((kcode[0] & DC_BTN_START) == 0 && NetworkHandshake::instance != nullptr)
		NetworkHandshake::instance->startNow();
}

#ifdef TARGET_UWP
#include "oslib/http_client.h"

static bool checkUWPProtocolActivation()
{
	// Check for UWP protocol-activated ROM path
	static int checkCount = 90; // Try many times - OnAppActivated may not be called yet
	if (checkCount == 0)
		return false;
	checkCount--;
	char* activationUri = SDL_WinRTGetProtocolActivationURI();
	if (activationUri == nullptr)
		return false;

	std::string uri(activationUri);
	SDL_free(activationUri);
	INFO_LOG(BOOT, "Protocol activation URI: %s", uri.c_str());
	size_t qpos = uri.find('?');
	if (qpos != std::string::npos)
	{
		uri = uri.substr(qpos + 1);
		// Parse launchOnExit parameter
		size_t exitPos = uri.find("launchOnExit=");
		if (exitPos != std::string::npos) {
			exitPos += 13; // Skip "launchOnExit="
			size_t exitEnd = uri.find('&', exitPos);
			if (exitEnd == std::string::npos)
				exitEnd = uri.size();
			std::string exitUri = uri.substr(exitPos, exitEnd - exitPos);
			launchOnExitUri = http::urlDecode(exitUri);
			INFO_LOG(BOOT, "LaunchOnExit URI: %s", launchOnExitUri.c_str());
			// SDL WinRT will automatically handle launchOnExit from the protocol URI
		}

		uri = http::urlDecode(uri);

		// Parse ROM path (first quoted string)
		size_t s = uri.find('"');
		if (s != std::string::npos)
		{
			size_t e = uri.find('"', s + 1);
			if (e != std::string::npos)
			{
				std::string romPath = uri.substr(s + 1, e - (s + 1));
				commandLineStart = true;
				gui_start_game(romPath);
				return true;
			}
		}
	}
	return false;
}
#endif

static void gui_display_loadscreen()
{
	drawBoxartBackground();
	centerNextWindow();
	ImGui::SetNextWindowSize(ScaledVec2(330, 0));
	ImGui::SetNextWindowBgAlpha(0.8f);
	ImguiStyleVar _(ImGuiStyleVar_WindowPadding, ScaledVec2(20, 20));

    if (ImGui::Begin("##loading", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize))
    {
		ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 10));
		ImGui::AlignTextToFramePadding();
		ImGui::SetCursorPosX(uiScaled(20.f));
		try {
			const char *label = gameLoader.getProgress().label;
			if (label == nullptr)
			{
				if (gameLoader.ready())
					label = T("Starting...");
				else
					label = T("Loading...");
			}
			
			const bool customTexPreloading = custom_texture.isPreloading();

			if (gameLoader.ready() && !customTexPreloading)
			{
				if (NetworkHandshake::instance != nullptr)
				{
					networkStatus = NetworkHandshake::instance->start();
					gui_setState(GuiState::NetworkStart);
				}
				else
				{
					gui_setState(GuiState::Closed);
					ImGui::Text("%s", label);
				}
			}
			else
			{
				int texLoaded = 0;
				int texTotal = 0;
				size_t loaded_size_b = 0;
				custom_texture.getPreloadProgress(texLoaded, texTotal, loaded_size_b);
				
				ImGui::Text("%s", label);
				float progress = 0;
				char overlay[64] = "";
				
				if (!gameLoader.ready())
				{
					progress = gameLoader.getProgress().progress;
				}
				else if (customTexPreloading)
				{
					ImGui::Spacing();
					ImGui::Text("%s", T("Preloading custom textures"));
					progress = (texTotal == -1 || texTotal == 0) ? 0.f : (float)texLoaded / (float)texTotal;
					if (texTotal == -1)
						snprintf(overlay, sizeof(overlay), "%s", T("Preparing..."));
					else
					{
						float loaded_size_mb = (float)loaded_size_b / (1024 * 1024);
						snprintf(overlay, sizeof(overlay), "%d / %d (%.1f MB)", texLoaded, texTotal, loaded_size_mb);
					}
				}
				
				ImguiStyleColor _(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
				ImGui::ProgressBar(progress, ImVec2(-1, uiScaled(20.f)), overlay);

				float currentwidth = ImGui::GetContentRegionAvail().x;
				ImGui::SetCursorPosX((currentwidth - uiScaled(100.f)) / 2.f + ImGui::GetStyle().WindowPadding.x);
				if (ImGui::Button(T("Cancel"), ScaledVec2(100.f, 0)))
					gameLoader.cancel();
			}
		} catch (const FlycastException& ex) {
			ERROR_LOG(BOOT, "%s", ex.what());
#ifdef TEST_AUTOMATION
			die("Game load failed");
#endif
			gui_stop_game(ex.what());
		}
    }
    ImGui::End();
}

void gui_display_ui()
{
	FC_PROFILE_SCOPE;
	const LockGuard lock(guiMutex);

	if (gui_state == GuiState::Closed)
		return;
	if (gui_state == GuiState::Main)
	{
#ifdef TARGET_UWP
		if (checkUWPProtocolActivation())
			return;
#endif
		if (!settings.content.path.empty() || settings.naomi.slave)
		{
#ifndef __ANDROID__
			commandLineStart = true;
#endif
			if (settings.content.path.substr(0, 7) == "dc_bios")
				gui_start_game("");
			else
				gui_start_game(settings.content.path);
			return;
		}
	}

	gui_newFrame();
	ImGui::NewFrame();
	error_msg_shown = false;
	bool gui_open = gui_is_open();

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
		vgamepad::draw();
		break;
	case GuiState::VJoyEditCommands:
		vgamepad::displayCommands();
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
	case GuiState::Achievements:
#ifdef USE_RACHIEVEMENTS
		achievements::achievementList();
		break;
#endif
	default:
		die("Unknown UI state");
		break;
	}
	error_popup();
    ImGui::Render();
	gui_endFrame(gui_open);
	uiThreadRunner.execTasks();
	ImguiFileTexture::resetLoadCount();

	if (gui_state == GuiState::Closed)
		emu.start();
}

static u64 LastFPSTime;
static int lastFrameCount = 0;
static float fps = -1;

static std::string getFPSNotification()
{
	if (config::ShowFPS)
	{
		u64 now = getTimeMs();
		if (now - LastFPSTime >= 1000) {
			fps = ((float)MainFrameCount - lastFrameCount) * 1000.f / (now - LastFPSTime);
			LastFPSTime = now;
			lastFrameCount = MainFrameCount;
		}
		if (fps >= 0.f && fps < 9999.f) {
			char text[32];
			snprintf(text, sizeof(text), "F:%4.1f%s", fps, settings.input.fastForwardMode ? " >>" : "");

			return std::string(text);
		}
	}
	return std::string(settings.input.fastForwardMode ? ">>" : "");
}

void gui_draw_osd()
{
	gui_newFrame();
	ImGui::NewFrame();

#ifdef USE_RACHIEVEMENTS
	if (!achievements::notifier.draw())
#endif
		if (!toast.draw())
		{
			std::string message = getFPSNotification();
			if (!message.empty())
			{
				const float maxW = uiScaled(640.f);
				ImDrawList *dl = ImGui::GetForegroundDrawList();
				const ScaledVec2 padding(5.f, 5.f);
				const ImVec2 size = largeFont->CalcTextSizeA(largeFont->LegacySize, FLT_MAX, maxW, &message.front(), &message.back() + 1)
						+ padding * 2.f;
				ImVec2 pos(insetLeft, ImGui::GetIO().DisplaySize.y - size.y);
				constexpr float alpha = 0.7f;
				const ImU32 bg_col = alphaOverride(0x00202020, alpha / 2.f);
				dl->AddRectFilled(pos, pos + size, bg_col, 0.f);
				pos += padding;
				const ImU32 col = alphaOverride(0x0000FFFF, alpha);
				dl->AddText(largeFont, largeFont->LegacySize, pos, col, &message.front(), &message.back() + 1, maxW);
			}
		}

	if (ggpo::active())
	{
		if (config::NetworkStats)
			ggpo::displayStats();
		chat.display();
	}
	else if (config::NetworkStats) {
		ice::displayStats();
	}
	if (!settings.raHardcoreMode)
		lua::overlay();
	vgamepad::draw();
    ImGui::Render();
	uiThreadRunner.execTasks();
}

void gui_display_osd() {
	gui_draw_osd();
	gui_endFrame(gui_is_open());
}

void gui_display_profiler()
{
#if FC_PROFILER
	gui_newFrame();
	ImGui::NewFrame();

	ImGui::Begin("Profiler", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);

	{
		ImguiStyleColor _(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

		std::unique_lock<std::recursive_mutex> lock(fc_profiler::ProfileThread::s_allThreadsLock);

		for(const fc_profiler::ProfileThread* profileThread : fc_profiler::ProfileThread::s_allThreads)
		{
			char text[256];
			std::snprintf(text, 256, "%.3f : Thread %s", (float)profileThread->cachedTime, profileThread->threadName.c_str());
			ImGui::TreeNode(text);

			ImGui::Indent();
			fc_profiler::drawGUI(profileThread->cachedResultTree);
			ImGui::Unindent();
		}
	}

	for (const fc_profiler::ProfileThread* profileThread : fc_profiler::ProfileThread::s_allThreads)
	{
		fc_profiler::drawGraph(*profileThread);
	}

	ImGui::End();
    ImGui::Render();
	gui_endFrame(true);
#endif
}

void gui_open_onboarding() {
	gui_setState(GuiState::Onboarding);
}

void gui_cancel_load() {
	gameLoader.cancel();
}

void gui_term()
{
	if (inited)
	{
		inited = false;
		scanner.stop();
		ImGui::DestroyContext();
	    EventManager::unlisten(Event::Resume, emuEventCallback);
	    EventManager::unlisten(Event::Start, emuEventCallback);
	    EventManager::unlisten(Event::Terminate, emuEventCallback);
	    boxart.term();
	}
}

void fatal_error(const char* text, ...)
{
    va_list args;

    char temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    ERROR_LOG(COMMON, "%s", temp);

    os_notify("Fatal Error", 20000, temp);
}

extern bool subfolders_read;

void gui_refresh_files() {
	scanner.refresh();
	subfolders_read = false;
}

void reset_vmus() {
	for (u32 i = 0; i < std::size(vmu_lcd_status); i++)
		vmu_lcd_status[i] = false;
}

void gui_error(const std::string& what) {
	error_msg = what;
}

void gui_loadState()
{
	const LockGuard lock(guiMutex);
	if (gui_state == GuiState::Closed && dc_savestateAllowed())
	{
		try {
			emu.stop();
			dc_loadstate(config::SavestateSlot);
			emu.start();
		} catch (const FlycastException& e) {
			gui_stop_game(e.what());
		}
	}
}

void gui_saveState(bool stopRestart)
{
	const LockGuard lock(guiMutex);
	if ((gui_state == GuiState::Closed || !stopRestart) && dc_savestateAllowed())
	{
		try {
			if (stopRestart)
				emu.stop();
			savestate();
			if (stopRestart)
				emu.start();
		} catch (const FlycastException& e) {
			if (stopRestart)
				gui_stop_game(e.what());
			else
				WARN_LOG(COMMON, "gui_saveState: %s", e.what());
		}
	}
}

void gui_setState(GuiState newState)
{
	gui_state = newState;
	if (newState == GuiState::Closed)
	{
		// If the game isn't rendering any frame, these flags won't be updated and keyboard/mouse input will be ignored.
		// So we force them false here. They will be set in the next ImGUI::NewFrame() anyway
		ImGuiIO& io = ImGui::GetIO();
		io.WantCaptureKeyboard = false;
		io.WantCaptureMouse = false;
	}
}

std::string gui_getCurGameBoxartUrl()
{
	GameMedia game;
	game.fileName = settings.content.fileName;
	game.path = settings.content.path;
	GameBoxart art = boxart.getBoxart(game);
	return art.boxartUrl;
}

void gui_runOnUiThread(std::function<void()> function) {
	uiThreadRunner.runOnThread(function);
}

void gui_takeScreenshot()
{
	if (!game_started)
		return;
	gui_runOnUiThread([]() {
		std::string date = timeToISO8601(time(nullptr));
		std::replace(date.begin(), date.end(), '/', '-');
		std::replace(date.begin(), date.end(), ':', '-');
		std::string name = "Flycast-" + date + ".png";

		std::vector<u8> data;
		getScreenshot(data);
		if (data.empty()) {
			os_notify(T("No screenshot available"), 2000);
		}
		else
		{
			try {
				hostfs::saveScreenshot(name, data);
				os_notify(T("Screenshot saved"), 2000, name.c_str());
			} catch (const FlycastException& e) {
				os_notify(T("Error saving screenshot"), 5000, e.what());
			}
		}
	});
}

#ifdef TARGET_UWP
// Ugly but a good workaround for MS stupidity
// UWP doesn't allow the UI thread to wait on a thread/task. When an std::future is ready, it is possible
// that the task has not yet completed. Calling std::future::get() at this point will throw an exception
// AND destroy the std::future at the same time, rendering it invalid and discarding the future result.
bool __cdecl Concurrency::details::_Task_impl_base::_IsNonBlockingThread() {
	return false;
}
#endif
