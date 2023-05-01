#include "gdxsv_emu_hooks.h"

#include <fstream>
#include <regex>

#include "cfg/cfg.h"
#include "gdxsv.h"
#include "hw/maple/maple_if.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "json.hpp"
#include "log/LogManager.h"
#include "nowide/fstream.hpp"
#include "oslib/oslib.h"
#include "rend/gui_util.h"
#include "stdclass.h"
#include "version.h"
#include "xxhash.h"

using namespace nlohmann;

#ifdef _WIN32
#define CHAR_PATH_SEPARATOR '\\'
#else
#define CHAR_PATH_SEPARATOR '/'
#endif

static void gdxsv_update_popup();
static void wireless_warning_popup();
static void gdxsv_latest_version_check();
static bool gdxsv_update_available = false;
static std::string gdxsv_latest_version_tag;
static std::string gdxsv_latest_version_download_url;

void gdxsv_emu_flycast_init() { config::GGPOEnable = false; }

void gdxsv_emu_start() {
	gdxsv.Reset();

	if (gdxsv.Enabled()) {
		auto replay = cfgLoadStr("gdxsv", "replay", "");
		if (!replay.empty()) {
			dc_loadstate(99);
		} else if (!cfgLoadStr("gdxsv", "rbk_test", "").empty()) {
			dc_loadstate(99);
		} else {
			gdxsv.StartPingTest();
		}
	}
}

void gdxsv_emu_reset() { gdxsv.Reset(); }

void gdxsv_emu_vblank() {
	if (gdxsv.Enabled()) {
		gdxsv.HookVBlank();
	}
}

void gdxsv_emu_mainui_loop() {
	if (gdxsv.Enabled()) {
		gdxsv.HookMainUiLoop();
	}
}

void gdxsv_emu_rpc() {
	if (gdxsv.Enabled()) {
		gdxsv.HandleRPC();
	}
}

void gdxsv_emu_savestate(int slot) {
	if (gdxsv.Enabled()) {
		gdxsv.RestoreOnlinePatch();
	}
}

void gdxsv_emu_loadstate(int slot) {
	if (gdxsv.Enabled()) {
		auto replay = cfgLoadStr("gdxsv", "replay", "");
		if (!replay.empty() && slot == 99) {
			gdxsv.StartReplayFile(replay.c_str());
		}

		auto rbk_test = cfgLoadStr("gdxsv", "rbk_test", "");
		if (!rbk_test.empty() && slot == 99) {
			gdxsv.StartRollbackTest(rbk_test.c_str());
		}
	}
}

bool gdxsv_ingame() { return gdxsv.InGame(); }

bool gdxsv_emu_menu_open() {
	if (gdxsv.Enabled()) {
		return gdxsv.HookOpenMenu();
	}
	return true;
}

bool gdxsv_widescreen_hack_enabled() { return gdxsv.Enabled() && config::WidescreenGameHacks; }

void gdxsv_emu_gui_display() {
	if (gui_state == GuiState::Main) {
		gdxsv_update_popup();
		wireless_warning_popup();
	}
}

static void gui_header(const char* title) {
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));	// Left
	ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
}

void gdxsv_emu_gui_settings() {
	gui_header("gdxsv Settings");

	ImGui::Columns(5, "gdxlang", false);
	ImGui::Text("Language mod:");
	ImGui::SameLine();
	ShowHelpMarker("Patch game language and texture, for DX only");
	ImGui::NextColumn();
	OptionRadioButton("Japanese", config::GdxLanguage, 0);
	ImGui::NextColumn();
	OptionRadioButton("Cantonese", config::GdxLanguage, 1);
	ImGui::NextColumn();
	OptionRadioButton("English", config::GdxLanguage, 2);
	ImGui::NextColumn();
	OptionRadioButton("Disabled", config::GdxLanguage, 3);
	ImGui::Columns(1, nullptr, false);

	if (ImGui::Button(" Apply Recommended Settings ", ImVec2(0, 40))) {
		// Frame Limit
		config::LimitFPS = false;
		config::VSync = true;
		config::FixedFrequency = 2;
		// Controls
		config::MapleMainDevices[0].set(MapleDeviceType::MDT_SegaController);
		config::MapleExpansionDevices[0][0].set(MDT_SegaVMU);
		// Video
		config::PerStripSorting = false;
		config::DelayFrameSwapping = false;
#if defined(_WIN32)
		config::RendererType.set(RenderType::DirectX11);
#else
		config::RendererType.set(RenderType::OpenGL);
#endif
		config::RenderResolution = 960;
		config::SkipFrame = 0;
		config::AutoSkipFrame = 0;
		// Audio
		config::DSPEnabled = false;
		config::AudioVolume.set(50);
		config::AudioVolume.calcDbPower();
		config::AudioBufferSize = 706 * 4;
		// Others
		config::DynarecEnabled = true;
		config::DynarecIdleSkip = true;
		config::ThreadedRendering = false;
		// Network
		config::EnableUPnP = true;
		if (config::GdxLocalPort == 0) {
			config::GdxLocalPort = get_random_port_number();
		}
		config::GdxMinDelay = 2;

		maple_ReconnectDevices();
	}
	ImGui::SameLine();
	ShowHelpMarker(R"(Use gdxsv recommended settings:
    Frame Limit Method:
      VSync:Yes
        DupeFrames: No
      CPU Sleep (59.94Hz)

    Control:
      Device A: Sega Controller / Sega VMU
    
    Video:
      Transparent Sorting: Per Triangle
      Automatic Frame Skipping: Disabled
      Delay Frame Swapping: No
      Renderer: DirectX 11 (Windows) / OpenGL (OtherOS)
      Internal Resolution: 1280x960 (x2)
      Frame Skipping: 0
    
    Audio:
      Enable DSP: No
      Volume Level: 50%
      Buffer: 64ms
    
    Advanced:
      CPU Mode: Dynarec
      Dynarec Idle Skip: Yes
      Multi-threaded emulation: No

    Network:
      Enable UPnP
      Gdx Minimum Delay: 2)");

	bool widescreen = config::Widescreen.get() && config::WidescreenGameHacks.get();
	bool pressed = ImGui::Checkbox("Enable 16:9 Widescreen Hack", &widescreen);
	if (pressed) {
		config::Widescreen.set(widescreen);
		config::SuperWidescreen.set(widescreen);
		config::WidescreenGameHacks.set(widescreen);
	}
	ImGui::SameLine();
	ShowHelpMarker(R"(Use the following rendering options:
    rend.WideScreen=true
    rend.SuperWideScreen=true
    rend.WidescreenGameHacks=true)");

	ImGui::Text("Frame Limit Method:");
	ImGui::SameLine();
	ShowHelpMarker("You must select one or more methods to limit game frame rate");

	OptionCheckbox("AudioSync", config::LimitFPS, "Limit frame rate by audio. Minimize audio glitch");
	OptionCheckbox("VSync", config::VSync, "Limit frame rate by VSync. Minimize video glitch");
	ImGui::Indent();
	{
		DisabledScope scope(!config::VSync);

		OptionCheckbox("Duplicate frames", config::DupeFrames, "Duplicate frames on high refresh rate monitors (120 Hz and higher)");
	}
	ImGui::Unindent();

	bool fixedFrequency = config::FixedFrequency != 0;
	ImGui::Checkbox("CPU Sleep", &fixedFrequency);
	ImGui::SameLine();
	ShowHelpMarker("Limit frame rate by CPU Sleep and Busy-Wait. Minimize input glitch (Experimental)");
	if (fixedFrequency) {
		if (!config::FixedFrequency) config::FixedFrequency = 2;

		ImGui::Columns(3, "fixed_frequency", false);
		OptionRadioButton("Auto", config::FixedFrequency, 1, "Automatically sets frequency by Cable & Broadcast type");
		ImGui::NextColumn();
		OptionRadioButton("59.94 Hz", config::FixedFrequency, 2, "Native NTSC/VGA frequency");
		ImGui::NextColumn();
		OptionRadioButton("60 Hz", config::FixedFrequency, 3, "Approximate NTSC/VGA frequency");
		ImGui::NextColumn();
		OptionRadioButton("50 Hz", config::FixedFrequency, 4, "Native PAL frequency");
		ImGui::NextColumn();
		OptionRadioButton("30 Hz", config::FixedFrequency, 5, "Half NTSC/VGA frequency");
		ImGui::Columns(1, nullptr, false);
	} else {
		config::FixedFrequency = 0;
	}

	if (!config::LimitFPS && !config::VSync && !config::FixedFrequency) {
		config::LimitFPS = true;
	}

	gui_header("Network Settings (P2P Lobby Only)");
	OptionCheckbox("Enable UPnP", config::EnableUPnP, "Automatically configure your network router for netplay");
	ImGui::InputInt("Gdx UDP Port", &config::GdxLocalPort.get());
	ImGui::SameLine();
	ShowHelpMarker("UDP port number used for P2P communication. Cannot use the same number as another application.");

	if (config::GdxLocalPort == 0) {
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	static std::string upnp_result;
	static std::future<std::string> upnp_future;
	if (upnp_future.valid() && upnp_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
		upnp_result = upnp_future.get();
	}

	const auto buttonWidth = ImGui::CalcTextSize("Test The Port").x + ImGui::GetStyle().ItemSpacing.x * 2;
	if (ImGui::Button("UPnP Now", ImVec2(buttonWidth, 0)) && !upnp_future.valid()) {
		upnp_result = "Please wait...";
		int port = config::GdxLocalPort;
		upnp_future = std::async(std::launch::async, [port]() -> std::string {
			auto& upnp = gdxsv.UPnP();
			std::string result = upnp.Init() && upnp.AddPortMapping(port, false) ? "Success" : upnp.getLastError();
			return result;
		});
	}
	ImGui::SameLine();
	ShowHelpMarker("Open the port using UPnP");
	ImGui::SameLine();
	ImGui::Text(upnp_result.c_str());

	static std::string udp_test_result;
	static std::future<std::string> udp_test_future;
	if (udp_test_future.valid() && udp_test_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
		udp_test_result = udp_test_future.get();
	}
	if (ImGui::Button("Test The Port", ImVec2(buttonWidth, 0)) && !udp_test_future.valid()) {
		udp_test_result = "Please wait...";
		udp_test_future = test_udp_port_connectivity(config::GdxLocalPort);
	}
	ImGui::SameLine();
	ShowHelpMarker("Test receiving data using this UDP port");
	ImGui::SameLine();
	ImGui::Text(udp_test_result.c_str());

	if (config::GdxLocalPort == 0) {
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	OptionArrowButtons(
		"Gdx Minimum Delay", config::GdxMinDelay, 2, 6,
		"Minimum frame of input delay used for rollback communication.\nSmaller value reduces latency, but uses more CPU and "
		"introduces glitches.");
	
	OptionCheckbox("Display Network Statistics", config::NetworkStats,
			"Display network statistics on screen by default.\nUse Flycast Menu button to show/hide.");
}

void gdxsv_gui_display_osd() { gdxsv.DisplayOSD(); }

void gdxsv_crash_append_log(FILE* f) {
	if (gdxsv.Enabled()) {
		fprintf(f, "[gdxsv]disk: %d\n", gdxsv.Disk());
		fprintf(f, "[gdxsv]user_id: %s\n", gdxsv.UserId().c_str());
		fprintf(f, "[gdxsv]netmode: %s\n", gdxsv.NetModeString());
	}
}

static bool trim_prefix(const std::string& s, const std::string& prefix, std::string& out) {
	auto size = prefix.size();
	if (s.size() < size) return false;
	if (std::equal(std::begin(prefix), std::end(prefix), std::begin(s))) {
		out = s.substr(prefix.size());
		return true;
	}
	return false;
}

void gdxsv_crash_append_tag(const std::string& logfile, std::vector<http::PostField>& post_fields) {
	if (file_exists(logfile)) {
		nowide::ifstream ifs(logfile);
		if (ifs.is_open()) {
			std::string line;
			std::string f_disk, f_user_id, f_netmode;

			while (std::getline(ifs, line)) {
				trim_prefix(line, "[gdxsv]disk: ", f_disk);
				trim_prefix(line, "[gdxsv]user_id: ", f_user_id);
				trim_prefix(line, "[gdxsv]netmode: ", f_netmode);
			}

			if (!f_disk.empty()) post_fields.emplace_back("sentry[tags][disk]", f_disk);
			if (!f_user_id.empty()) post_fields.emplace_back("sentry[tags][user_id]", f_user_id);
			if (!f_netmode.empty()) post_fields.emplace_back("sentry[tags][netmode]", f_netmode);
		}
	}

	const std::string machine_id = os_GetMachineID();
	if (machine_id.length()) {
		const auto digest = XXH64(machine_id.c_str(), machine_id.size(), 37);
		std::stringstream ss;
		ss << std::hex << digest;
		post_fields.emplace_back("sentry[tags][machine_id]", ss.str().c_str());
	}
}

static void gdxsv_update_popup() {
	gdxsv_latest_version_check();
	bool no_popup_opened = !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
	if (gdxsv_update_available && no_popup_opened) {
		ImGui::OpenPopup("New version");
		gdxsv_update_available = false;
	}
	if (ImGui::BeginPopupModal("New version", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * settings.display.uiScale);
		ImGui::TextWrapped("  %s is available for download!  ", gdxsv_latest_version_tag.c_str());
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * settings.display.uiScale, 3 * settings.display.uiScale));
		float currentwidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x -
							 -55.f * settings.display.uiScale);
		if (ImGui::Button("Download", ImVec2(100.f * settings.display.uiScale, 0.f))) {
			gdxsv_update_available = false;
			if (!gdxsv_latest_version_download_url.empty()) {
				os_LaunchFromURL(gdxsv_latest_version_download_url);
			} else {
				os_LaunchFromURL("https://github.com/inada-s/flycast/releases/latest/");
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x +
							 -55.f * settings.display.uiScale);
		if (ImGui::Button("Cancel", ImVec2(100.f * settings.display.uiScale, 0.f))) {
			gdxsv_update_available = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}
}

static void wireless_warning_popup() {
	static bool show_wireless_warning = true;
	static std::string connection_medium = os_GetConnectionMedium();
	const bool no_popup_opened = !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

	if (show_wireless_warning && no_popup_opened && connection_medium == "Wireless") {
		ImGui::OpenPopup("Wireless connection detected");
		show_wireless_warning = false;
	}

	if (ImGui::BeginPopupModal("Wireless connection detected", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * settings.display.uiScale);
		ImGui::TextWrapped("  Please use LAN cable for the best gameplay experience!  ");
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * settings.display.uiScale, 3 * settings.display.uiScale));
		float currentwidth = ImGui::GetContentRegionAvail().x;

		ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x);
		if (ImGui::Button("OK", ImVec2(100.f * settings.display.uiScale, 0.f))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}
}

static void gdxsv_handle_release_json(const std::string& json_string) {
	const std::regex tag_name_regex(R"|#|("tag_name":"(.*?)")|#|");
	const std::string version_prefix = "gdxsv-";
	const std::regex semver_regex(R"|#|(^([0-9]+)\.([0-9]+)\.([0-9]+)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+)?$)|#|");

	std::string latest_tag_name;
	std::string latest_download_url;
	std::string expected_name;

#if HOST_CPU == CPU_X64
#if defined(_WIN32)
	expected_name = "flycast-gdxsv-windows-msvc.zip";
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
	expected_name = "flycast-gdxsv-macos-x86_64.zip";
#elif defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
	expected_name = "flycast-gdxsv-linux-x86_64.zip";
#endif
#endif

	try {
		json v = json::parse(json_string);
		latest_tag_name = v.at("tag_name");
		for (auto e : v.at("assets")) {
			std::string name = e.at("name");
			if (name == expected_name) {
				latest_download_url = e.at("browser_download_url");
			}
		}
	} catch (const json::exception& e) {
		WARN_LOG(COMMON, "json parse failure: %s", e.what());
	}

	if (latest_tag_name.empty()) return;

	auto trim_prefix = [&version_prefix](const std::string& s) {
		if (s.size() < version_prefix.size()) return s;
		if (version_prefix == s.substr(0, version_prefix.size())) return s.substr(version_prefix.size());
		return s;
	};

	std::smatch match;

	auto current_version_str = trim_prefix(std::string(GIT_VERSION));
	if (!std::regex_match(current_version_str, match, semver_regex)) return;
	if (match.size() < 4) return;
	auto current_version = std::tuple<int, int, int>(std::stoi(match.str(1)), std::stoi(match.str(2)), std::stoi(match.str(3)));

	auto latest_version_str = trim_prefix(latest_tag_name);
	if (!std::regex_match(latest_version_str, match, semver_regex)) return;
	if (match.size() < 4) return;
	auto latest_version = std::tuple<int, int, int>(std::stoi(match.str(1)), std::stoi(match.str(2)), std::stoi(match.str(3)));

	gdxsv_latest_version_tag = latest_tag_name;
	gdxsv_latest_version_download_url = latest_download_url;

	if (current_version < latest_version) {
		gdxsv_update_available = true;
	}
}

static void gdxsv_latest_version_check() {
	static std::once_flag once;
	std::call_once(once, [] {
		std::thread([]() {
			const std::string json = os_FetchStringFromURL("https://api.github.com/repos/inada-s/flycast/releases/latest");
			if (json.empty()) return;
			gdxsv_handle_release_json(json);
		}).detach();
	});
}

static void UnpackAccumulativeOffsetsIntoRanges(int base_codepoint, const short* accumulative_offsets, int accumulative_offsets_count, ImWchar* out_ranges)
{
	for (int n = 0; n < accumulative_offsets_count; n++, out_ranges += 2)
	{
		out_ranges[0] = out_ranges[1] = (ImWchar)(base_codepoint + accumulative_offsets[n]);
		base_codepoint += accumulative_offsets[n];
	}
	out_ranges[0] = 0;
}

const ImWchar* gdxsv_get_glyph_ranges_shiftjis()
{
	// 6577 ideograms code points for Shift_JIS
	static const short accumulative_offsets_from_0x4E00[] =
	{
		-19055,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,8,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,56,15,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,7103,5,1,2,1,3,1,3,1,4,1,10,2,1,8,200,40,101,1,
		1,1,63,2,44,2,1,4,1,3,7,8,3,1,2,7,1,1,1,1,1,8,1,8,21,14,1,5,1,3,1,23,1,3,1,30,109,494,1,1,1,9,3,1,3,1,3,1,3,1,1,3,3,1,1,3,3,1,3,1,3,1,3,1,3,
		1,3,3,9,85,1,17,1,9,1,9,1,4,3,1,32,22,1,58,2,40,3,2,10129,1,2,4,1,1,1,1,2,1,2,1,3,1,1,1,1,1,5,3,5,4,3,4,1,4,2,1,2,1,3,3,1,2,6,2,1,1,6,1,1,1,
		1,4,1,1,3,15,2,11,2,2,3,1,2,1,1,1,1,2,3,1,2,1,3,1,2,1,2,1,1,1,1,2,1,1,2,3,1,1,1,2,3,3,4,6,1,1,2,2,1,3,1,2,1,1,5,1,1,1,1,1,4,1,1,4,1,1,8,1,2,
		2,4,1,4,6,8,1,3,1,1,1,1,9,2,1,18,1,4,2,2,2,2,1,6,3,1,6,1,1,1,1,2,2,2,2,1,1,1,1,1,11,6,1,3,2,1,5,1,3,4,3,2,3,2,2,2,5,2,3,2,3,1,10,2,1,1,6,1,9,
		3,1,1,6,4,2,1,3,3,1,2,1,2,2,2,2,1,1,9,1,4,2,1,2,2,4,7,1,3,2,2,2,2,1,2,2,3,1,5,2,2,1,1,1,2,1,1,1,1,1,9,3,10,4,1,1,6,1,5,1,4,2,9,7,6,2,1,1,2,5,
		3,5,8,4,7,1,1,18,1,5,1,1,1,2,7,4,3,4,1,3,2,2,4,1,4,4,5,2,2,6,1,7,4,2,5,1,1,2,5,9,2,1,1,2,2,5,2,9,8,5,3,1,1,3,1,1,2,1,1,1,1,1,1,2,1,1,1,2,2,2,
		6,2,6,3,3,1,1,1,1,1,1,3,4,1,1,1,4,4,2,3,1,3,1,2,1,2,1,1,1,1,2,1,1,2,7,2,2,1,1,2,1,1,1,1,4,1,1,1,1,1,1,1,6,7,1,1,3,2,1,1,9,5,1,1,3,1,5,1,2,1,
		3,3,1,4,1,2,1,1,3,1,2,3,1,2,1,1,2,1,3,3,3,3,6,7,1,2,2,1,4,2,3,3,1,1,1,1,1,8,1,3,3,1,1,1,2,5,2,5,3,5,1,1,4,1,5,1,1,1,1,1,1,8,2,4,4,1,1,4,4,1,
		2,7,4,1,3,6,1,1,1,1,4,3,1,4,3,2,3,2,2,2,2,4,5,3,2,1,1,4,1,1,1,2,1,1,2,1,11,1,2,3,1,1,4,1,2,1,3,1,2,5,2,1,5,1,1,2,1,3,3,1,2,7,5,2,2,5,1,1,1,4,
		1,1,2,2,1,1,1,1,1,3,4,1,1,1,3,1,2,2,2,2,6,3,5,1,1,1,2,1,1,2,1,3,4,3,2,18,2,2,5,1,5,1,2,1,4,1,2,3,3,5,7,1,5,1,1,1,1,1,1,6,2,1,2,2,4,2,1,1,1,1,
		3,1,1,1,1,1,1,1,1,1,1,1,3,1,1,2,7,2,1,4,1,1,1,1,1,1,1,1,1,10,2,2,1,6,3,2,1,1,1,8,2,1,2,1,1,1,2,2,4,2,1,1,4,3,14,9,2,6,1,2,2,1,1,4,1,1,3,4,2,
		5,1,2,1,1,2,16,2,1,3,3,1,3,3,1,5,4,1,1,2,1,1,2,3,1,1,15,9,1,3,1,2,1,4,1,4,8,3,7,2,1,8,1,4,2,24,1,2,2,5,1,5,2,4,1,1,6,3,4,3,1,5,1,6,24,1,2,2,
		3,1,3,2,1,1,13,1,1,2,1,1,1,8,1,1,1,1,1,2,2,6,14,1,2,13,6,2,3,4,1,19,2,4,1,8,3,11,2,1,1,3,14,6,2,1,2,2,2,10,10,2,2,11,9,4,2,1,1,8,4,2,6,6,1,3,
		5,5,12,2,3,9,6,2,6,4,1,1,1,5,6,3,2,4,1,2,1,3,2,3,11,2,2,1,6,1,3,2,1,3,1,4,1,2,2,2,3,1,3,2,4,3,7,1,1,5,3,7,1,3,5,2,5,3,4,1,1,1,16,3,2,3,1,21,
		3,6,1,2,8,13,2,1,1,6,6,3,13,3,3,5,3,4,1,1,2,6,3,1,3,17,3,2,1,2,4,2,3,1,4,1,10,4,4,4,3,6,5,1,1,3,1,5,3,3,1,9,1,6,1,2,3,1,1,1,4,4,7,2,5,2,3,4,
		5,5,2,14,4,5,3,9,3,3,5,5,1,1,1,3,3,4,2,3,2,5,2,2,2,1,1,3,2,1,5,1,6,1,2,1,1,1,1,5,2,1,1,1,1,5,7,1,5,1,5,1,2,1,1,1,1,6,3,2,2,1,1,1,1,1,3,1,5,1,
		6,6,3,1,1,5,1,1,1,3,1,2,1,2,6,2,3,2,1,1,1,2,2,5,1,4,5,4,1,1,1,6,3,6,3,3,2,2,6,2,3,4,6,7,2,3,8,3,2,5,1,2,1,5,1,2,9,1,2,2,1,11,5,4,2,2,6,8,7,2,
		2,3,1,5,4,6,6,1,6,4,1,5,3,17,8,4,4,2,19,19,8,1,33,1,1,3,1,7,2,1,4,6,1,10,2,3,3,17,1,14,2,1,10,12,8,2,4,2,4,8,2,3,2,11,1,3,1,2,1,2,1,1,1,2,4,
		1,1,1,3,2,5,1,2,2,3,2,6,3,2,2,1,1,2,1,1,2,6,2,1,1,1,1,1,1,2,3,1,1,1,1,8,2,3,1,1,1,2,1,6,3,1,1,1,1,1,2,3,4,2,1,1,7,2,1,1,2,1,2,1,1,1,1,1,2,3,
		2,3,2,1,2,2,4,1,2,1,2,1,1,1,1,1,1,1,2,1,1,2,2,3,4,6,2,2,4,5,4,7,1,1,1,1,1,1,1,1,1,4,1,2,2,1,2,1,1,1,1,2,2,9,2,1,3,1,7,2,1,2,5,3,19,4,1,3,13,
		7,1,2,1,5,2,3,1,1,3,1,2,7,2,18,7,1,7,1,1,3,2,1,6,4,1,2,10,4,3,3,3,1,1,1,1,1,1,1,4,3,7,34,1,2,2,2,10,13,3,3,4,3,12,2,3,4,1,4,13,5,10,2,9,3,2,
		1,12,3,1,5,1,3,5,2,1,3,2,2,1,1,1,3,3,3,1,1,1,1,2,4,2,1,4,1,3,5,1,5,5,3,1,1,2,8,6,2,2,1,3,3,1,1,5,3,3,1,1,2,5,2,6,1,2,8,2,1,1,1,14,1,1,1,1,2,
		1,1,1,1,1,1,1,2,2,1,3,3,5,6,1,1,3,2,4,6,1,4,2,8,1,1,1,9,1,1,5,1,1,5,1,3,3,4,1,2,2,1,1,1,1,5,1,3,4,1,2,1,2,1,1,2,1,1,2,1,2,2,1,5,1,1,1,1,2,1,
		1,2,1,1,1,1,1,3,4,6,1,1,2,4,2,2,4,2,1,4,2,3,7,2,2,2,3,2,3,1,2,3,1,4,1,4,3,1,1,1,1,3,1,2,4,2,3,3,1,1,1,1,1,1,2,1,2,1,1,4,1,1,1,4,1,1,5,2,1,7,
		1,1,3,1,5,1,5,3,1,6,2,7,1,9,1,1,1,3,1,3,4,7,5,1,4,3,3,2,2,15,1,1,2,3,1,3,2,1,1,3,1,4,1,1,1,1,1,1,4,2,9,7,1,1,3,4,1,2,3,2,3,4,1,5,1,2,1,1,1,3,
		1,1,1,1,1,2,1,5,2,10,2,1,5,2,1,1,5,2,2,1,3,1,4,1,3,3,1,2,1,8,1,1,1,1,2,4,1,8,1,1,10,2,5,2,2,3,1,1,2,4,1,8,1,2,1,2,1,2,1,1,5,1,2,3,2,1,4,1,1,
		6,5,1,4,2,6,1,4,8,8,1,1,1,3,2,3,1,2,1,1,1,1,5,2,3,1,1,3,2,3,1,2,2,1,3,3,1,1,1,2,1,1,1,1,7,4,5,3,4,2,1,3,2,3,1,10,3,2,2,1,2,4,4,4,4,5,3,1,1,1,
		1,1,1,1,3,19,3,12,2,2,1,1,2,2,1,1,1,1,8,1,1,2,1,1,2,1,1,2,2,4,1,2,1,1,2,5,4,4,1,1,2,1,1,4,3,4,1,1,6,1,1,2,2,1,5,2,3,3,3,2,3,5,6,3,5,3,3,2,1,
		1,2,1,1,5,1,7,1,1,1,1,1,1,1,3,1,2,13,1,5,4,4,2,1,1,5,3,1,1,1,1,1,2,1,2,1,1,1,1,1,3,1,1,2,1,1,3,1,11,1,1,1,2,2,2,1,1,7,1,2,1,5,1,1,3,5,8,6,8,
		1,3,4,11,3,1,1,10,3,1,2,1,5,2,5,11,1,1,2,3,4,4,1,3,1,5,3,5,1,3,2,1,3,4,2,3,4,1,1,1,1,2,2,1,1,1,1,1,6,2,1,6,3,2,3,1,2,3,6,1,2,4,4,1,6,2,6,5,6,
		2,4,12,7,2,4,3,1,5,10,2,4,1,7,2,4,4,4,12,10,15,2,6,7,2,2,9,5,10,1,2,5,4,6,1,4,2,2,1,2,2,7,2,1,5,1,3,2,6,5,2,4,2,6,1,1,1,3,1,5,3,2,1,2,2,4,3,
		1,2,5,19,4,1,6,1,6,1,1,3,5,1,1,1,1,1,2,3,1,6,3,5,2,2,4,1,1,1,1,4,1,4,1,3,6,4,2,2,1,2,1,10,1,4,1,1,3,2,2,1,6,2,2,1,3,2,3,1,2,4,1,1,2,1,7,2,3,
		1,4,2,1,1,1,5,1,3,3,5,2,2,5,1,1,3,1,1,1,1,3,1,4,9,1,7,1,3,1,3,2,2,1,4,1,8,3,1,5,2,1,5,2,5,1,1,6,3,2,1,1,1,5,2,4,3,11,1,1,3,2,2,1,1,1,5,1,1,4,
		2,4,7,2,1,3,1,1,5,3,5,1,1,5,5,4,5,3,6,4,1,3,2,3,3,3,2,13,3,1,2,1,3,6,3,7,2,1,1,1,2,1,1,3,1,1,1,1,3,5,1,2,2,2,5,1,1,1,4,2,1,1,7,1,1,2,1,1,1,1,
		3,3,2,1,1,2,3,2,2,5,3,5,1,1,1,2,3,3,3,2,1,1,1,1,1,1,1,5,3,2,1,1,1,1,2,2,5,2,1,6,2,2,2,1,4,5,2,3,2,1,3,1,1,4,3,6,4,1,2,1,1,1,8,3,2,4,4,1,1,1,
		2,1,4,2,3,1,4,2,3,2,3,2,1,2,2,1,1,6,3,1,3,1,1,15,3,1,7,3,1,7,1,1,7,2,4,1,3,1,3,1,1,1,3,2,5,1,2,1,2,1,5,3,1,2,4,4,13,2,1,7,1,2,2,2,8,2,4,1,3,
		4,2,2,1,2,4,1,1,5,2,1,1,2,2,1,3,1,2,8,2,3,1,1,2,5,2,1,2,1,2,5,1,1,2,4,7,1,3,7,1,6,1,3,1,3,3,1,1,1,1,3,7,1,1,1,5,1,1,2,1,2,2,6,4,2,3,4,2,11,9,
		1,1,4,1,2,1,1,2,1,1,8,1,2,1,1,4,1,1,2,1,1,3,1,1,3,1,8,4,3,3,1,6,1,4,7,7,3,1,2,7,3,1,2,2,4,3,1,1,1,1,2,3,5,1,4,1,9,1,3,2,5,7,2,2,2,3,3,5,1,1,
		6,1,1,3,2,2,3,1,2,1,1,6,1,1,3,7,1,2,1,1,3,7,3,1,3,13,1,6,2,1,4,12,6,7,1,4,9,1,2,7,5,4,2,1,7,2,2,5,5,3,6,1,1,14,2,7,1,3,1,9,2,16,1,9,1,5,8,4,
		7,2,1,1,2,4,11,5,1,1,1,3,1,5,4,2,3,2,2,3,1,5,2,4,2,1,1,1,2,3,1,5,4,1,4,1,2,4,1,3,1,2,3,1,1,2,8,1,2,6,6,6,1,4,2,1,1,1,1,2,3,1,1,3,1,5,1,5,2,1,
		4,1,1,4,3,4,12,1,3,4,21,7,2,2,1,3,4,8,1,16,3,1,6,2,1,1,12,2,5,2,3,3,1,1,1,2,6,2,6,2,1,7,3,1,3,1,1,5,4,1,3,2,1,3,3,1,1,6,1,9,3,3,2,5,1,1,1,1,
		1,1,1,2,3,1,4,1,2,7,2,2,2,2,2,4,1,1,2,3,2,5,1,1,2,24,1,6,5,2,2,3,1,6,4,1,1,7,1,2,1,2,3,2,1,3,3,1,20,1,2,7,1,2,3,1,2,2,6,3,1,1,12,3,4,2,5,2,4,
		2,1,19,3,3,3,4,4,5,1,1,4,1,5,1,3,3,1,5,3,3,2,2,2,1,1,2,3,2,2,2,2,1,1,10,2,1,1,1,1,8,2,4,1,1,2,2,1,1,2,1,1,1,3,2,1,1,1,1,9,2,4,5,7,3,1,8,2,3,
		4,8,4,3,1,3,4,8,1,1,2,10,3,1,6,2,4,1,2,3,3,5,5,3,4,1,3,3,5,2,1,4,2,1,5,2,1,1,8,1,14,3,3,2,3,1,6,1,2,1,4,3,6,2,2,1,1,11,2,1,8,1,5,1,6,6,1,2,4,
		15,4,3,1,3,5,2,7,1,1,1,4,4,2,2,4,1,1,2,2,8,3,6,10,2,1,6,7,2,6,7,1,1,1,3,14,1,3,3,4,1,1,3,7,1,2,1,2,2,3,4,4,3,8,2,4,2,4,3,2,1,2,1,1,7,1,5,4,2,
		12,14,5,7,11,8,4,1,8,4,1,12,1,4,4,7,19,1,1,1,4,5,1,1,14,3,4,10,4,2,18,8,4,12,11,5,1,2,5,5,16,6,13,3,2,7,1,3,9,2,1,1,1,2,3,2,15,7,4,2,5,5,1,4,
		6,9,4,5,8,5,5,5,1,5,2,2,2,1,2,8,1,5,1,1,5,1,1,7,4,2,1,3,7,7,3,11,13,2,2,1,3,2,3,1,4,1,1,1,1,1,1,6,1,1,3,1,6,6,1,2,2,2,2,1,5,2,9,2,5,4,1,2,1,
		1,5,11,4,10,2,5,5,3,3,4,3,9,1,1,2,8,2,2,5,2,2,5,1,1,7,3,1,10,1,1,3,1,13,12,1,4,1,1,2,6,4,1,1,3,1,5,2,1,7,1,5,1,9,1,8,12,5,2,6,2,3,3,2,1,9,3,
		2,2,11,19,9,1,8,5,2,6,2,3,1,16,2,5,5,3,1,3,7,6,5,2,1,3,25,3,13,1,1,1,1,4,5,2,20,4,1,1,1,2,1,1,3,1,5,1,5,1,3,3,8,5,8,19,4,5,9,13,13,5,5,8,4,2,
		1,3,1,2,5,2,1,1,4,1,1,11,1,1,7,1,1,3,2,2,3,2,2,2,1,4,2,1,2,3,1,4,1,1,1,4,1,2,1,1,8,2,3,1,1,1,1,2,2,3,5,1,1,1,1,3,2,2,1,1,1,2,1,1,2,3,3,1,2,1,
		1,7,3,4,1,2,1,1,3,1,2,3,6,3,6,2,6,6,1,1,2,3,1,3,1,1,4,1,2,2,3,3,5,2,1,3,1,2,3,4,1,6,7,2,1,1,6,2,2,1,2,8,2,2,18,1,1,1,2,3,9,4,7,7,4,1,1,4,6,4,
		2,4,5,1,5,1,1,1,2,4,2,4,2,2,1,1,1,1,2,3,1,2,1,1,3,3,2,3,3,3,1,20,2,4,3,1,1,1,5,3,1,3,2,2,3,5,4,1,4,1,2,1,2,2,1,1,2,3,4,4,2,4,3,3,3,3,3,1,1,2,
		1,15,3,1,1,4,1,1,3,14,1,2,2,4,7,19,1,6,2,2,1,2,3,14,5,1,12,3,3,13,2,5,7,1,3,3,3,3,2,1,1,2,8,6,10,3,1,1,6,1,2,2,2,4,1,1,4,9,6,10,6,2,1,11,5,1,
		1,11,2,6,5,6,24,14,1,3,3,2,8,5,5,1,5,1,1,3,2,2,2,3,9,4,2,1,5,6,5,2,2,3,4,1,4,1,5,1,3,6,13,1,4,3,5,9,4,6,7,3,1,7,13,4,1,1,14,2,2,2,1,6,1,1,7,
		3,2,1,1,3,3,1,1,1,2,3,3,5,10,3,5,1,1,3,1,5,3,1,1,14,9,1,3,4,2,3,6,1,3,1,1,1,1,8,2,6,1,3,3,7,2,2,1,2,1,2,3,4,11,5,8,3,2,1,6,3,1,1,1,2,3,1,14,
		3,1,5,4,1,1,1,1,1,2,1,3,3,4,1,1,1,7,10,1,1,6,2,5,4,2,3,1,3,2,2,2,1,4,10,1,2,1,1,1,7,10,1,4,1,1,6,4,5,4,1,1,2,1,2,1,2,2,3,1,2,4,1,2,1,2,1,1,1,
		1,2,1,4,3,2,1,6,2,1,1,5,3,2,2,2,2,1,4,2,7,1,2,3,2,5,1,2,4,7,2,1,3,12,1,2,1,2,1,1,2,1,1,1,2,2,7,8,2,5,2,2,1,3,1,5,12,1,4,2,2,3,2,1,2,1,1,1,2,
		1,2,2,9,3,4,3,4,8,1,3,2,1,2,2,1,3,14,3,4,1,1,3,4,6,3,1,9,7,6,4,1,1,1,3,8,2,2,4,3,1,12,1,5,1,1,1,3,9,1,2,1,4,2,2,7,1,4,1,7,7,2,9,3,1,1,6,2,2,
		3,2,3,2,1,3,4,2,1,2,1,2,1,3,2,1,3,1,1,6,4,1,2,2,3,5,4,4,4,2,4,2,1,1,2,5,8,3,2,2,2,2,1,3,2,2,2,1,1,4,1,2,3,4,1,2,1,1,1,1,1,4,1,1,9,1,2,1,1,2,
		1,2,4,1,5,3,1,1,1,1,5,1,2,1,1,6,5,3,3,1,1,3,2,6,3,1,1,2,1,3,4,12,6,4,6,1,1,1,3,3,1,8,1,1,1,1,1,1,1,2,1,3,2,1,2,1,1,8,3,1,4,2,1,3,3,2,2,1,1,2,
		1,3,4,1,3,3,3,2,7,6,3,1,4,1,1,7,9,3,1,2,1,1,3,5,3,3,1,3,2,2,1,1,2,1,3,2,3,4,1,2,7,1,1,3,1,3,1,8,1,2,1,3,3,9,2,1,1,2,3,1,5,1,3,2,1,1,2,1,1,2,
		5,1,154,2,2,11,7,1,1,2,1,3,1,3,7,1,7,1,1,1,1,3,2,2,3,2,1,1,9,1,2,1,1,1,2,2,2,6,6,3,1,5,1,4,1,5,1,3,4,2,1,4,4,4,1,4,2,6,2,1,11,1,5,3,2,5,3,6,
		2,1,4,1,2,1,1,1,5,1,4,2,3,2,1,1,3,5,7,11,3,5,2,2,7,4,8,4,2,2,4,1,2,1,6,7,1,2,1,1,2,1,2,4,1,1,5,1,1,1,2,2,1,7,3,2,2,1,2,4,1,3,4,1,2,1,2,2,1,2,
		6,9,1,2,6,2,8,3,1,1,2,1,3,3,1,10,2,3,4,4,1,5,3,1,1,1,1,1,16,1,8,6,6,2,2,6,5,8,5,3,2,1,2,1,1,10,6,1,5,3,2,1,3,4,1,1,5,1,2,1,5,2,5,4,2,5,2,1,3,
		3,1,4,1,7,3,2,3,2,3,1,1,1,2,4,2,1,4,4,2,5,1,1,5,1,3,2,2,1,2,3,6,1,6,1,1,2,3,1,3,2,1,1,1,2,1,1,2,2,4,1,1,3,1,2,1,10,1,1,1,2,5,2,1,1,1,1,7,7,17,
		1,1,3,2,3,2,2,2,2,1,3,1,2,1,4,1,1,6,13,2,5,7,4,2,6,1,5,1,1,2,2,2,5,1,2,2,8,12,1,1,1,3,2,2,1,2,1,2,2,2,1,1,4,6,2,1,5,1,1,7,1,1,1,1,3,5,8,1,1,
		4,7,5,3,4,2,1,2,1,1,2,1,7,5,4,1,5,1,2,6,27,2,2,4,1,9,2,2,1,4,5,3,4,4,1,1,2,6,2,1,6,4,8,4,4,5,2,2,5,3,2,4,3,1,9,2,4,1,1,1,2,3,4,2,6,1,3,4,1,1,
		1,5,13,2,7,1,2,5,4,3,4,1,9,3,5,9,4,6,1,1,3,3,2,1,1,1,1,2,4,2,2,1,8,2,7,5,4,5,3,3,2,12,5,6,1,2,1,3,2,3,2,3,1,1,2,3,1,5,3,1,18,2,6,8,3,1,6,11,
		2,1,1,2,1,2,5,2,5,6,1,8,8,3,1,2,5,1,1,1,3,7,2,1,2,9,5,1,1,3,10,7,2,4,3,1,2,6,1,3,3,2,2,1,8,2,2,1,1,1,1,2,1,10,1,7,8,4,2,1,5,7,1,7,1,4,1,13,2,
		1,1,3,4,4,1,3,1,8,3,1,3,8,11,2,1,15,14,1,2,4,1,5,2,2,1,8,4,6,8,2,15,1,1,7,2,14,1,5,1,1,4,1,6,14,2,1,2,2,2,1,6,5,2,3,1,5,5,3,1,1,1,9,1,2,3,2,
		2,1,3,2,1,1,3,4,1,6,2,2,9,4,11,3,4,4,10,2,1,2,5,2,2,2,6,1,3,3,2,2,4,6,2,2,7,3,11,18,3,9,4,4,7,1,2,3,4,2,1,4,5,2,14,15,3,4,1,2,2,3,7,8,1,1,2,
		4,1,11,1,1,4,10,5,3,2,5,2,2,2,6,1,5,1,4,2,2,2,1,3,1,1,5,3,5,2,3,2,6,1,1,2,1,6,3,2,5,4,1,2,5,4,2,7,3,2,1,3,1,2,8,2,1,1,1,1,10,5,1,3,1,3,1,2,2,
		7,10,1,1,4,1,3,1,1,4,2,3,3,2,4,2,1,10,1,7,5,1,11,2,3,6,2,1,8,1,9,8,1,1,4,2,4,3,2,3,2,3,7,1,2,2,3,1,1,2,1,4,4,3,1,1,2,7,5,1,2,1,2,3,3,1,7,3,2,
		1,18,8,2,1,6,3,1,13,4,8,2,1,5,2,2,2,2,3,3,1,1,3,2,2,4,2,5,3,4,2,1,2,4,1,1,5,5,2,8,1,1,2,1,6,3,2,2,1,3,3,2,1,1,1,2,1,1,1,9,6,2,1,2,2,3,1,4,2,
		2,3,2,4,2,1,2,1,1,1,2,4,1,3,2,7,3,3,3,2,3,4,1,1,2,3,4,4,1,1,2,2,1,1,2,2,1,4,2,1,3,2,4,1,2,4,2,1,1,2,3,5,2,2,4,2,1,2,1,1,2,3,1,5,2,3,1,7,6,5,
		3,8,3,2,1,7,2,2,1,1,3,7,5,1,3,1,1,1,2,3,6,3,3,7,2,2,2,2,1,3,3,1,157,3,5,2,5,2,2,2,2,2,5,5,7,1,8,1,1,12,1,1,2,6,3,4,1,2,1,1,6,4,5,1,2,1,1,5,1,
		1,1,1,1,1,1,1,1,2,1,1,2,1,1,3,1,1,2,1,1,1,1,1,3,1,2,3,1,3,2,7,1,1,2,2,2,1,1,2,4,3,13,1,1,1,7,1,2,1,2,1,2,2,1,3,1,2,78,2,1,4,2,3,1,2,1,3,10,4,
		5,15,10,5,11,7,4,4,9,1,3,7,4,1,2,2,2,2,5,2,1,4,4,2,7,3,9,1,1,5,1,13,1,1,11,6,4,1,13,2,3,1,1,1,2,4,5,4,6,1,3,1,14,2,2,6,5,3,1,2,3,1,2,4,2,1,5,
		8,9,1,1,3,1,1,13,7,1,2,2,1,1,1,5,9,4,3,1,8,13,3,1,1,1,5,2,4,1,2,6,1,1,1,4,2,1,1,2,7,3,1,5,4,5,1,2,3,1,3,2,1,1,3,3,1,1,9,5,3,2,1,1,1,55,1,2,1,
		4,4,1,5,1,1,1,1,1,5,3,1,1,3,3,2,1,9,3,3,6,8,3,1,3,1,1,2,2,1,4,3,1,1,1,3,3,1,2,2,1,5,2,1,1,1,1,2,1,1,1,1,2,1,3,1,1,1,1,1,1,4,7,3,1,3,1,2,1,3,
		2,3,1,3,2,2,1,1,2,1,1,1,1,1,1,1,1,1,2,1,3,2,2,1,2,2,3,1,4,1,1,3,3,1,1,1,2,2,1,2,1,1,1,1,1,3,2,1,5,2,18,3,2,2,5,2,4,3,9,9,4,13,6,1,2,4,5,8,2,
		6,5,16,7,20,3,2,23,1,1,1,1,1,4,2,2,2,10,1,2,4,1,2,6,1,2,2,1,10,5,2,2,2,3,2,5,5,6,2,6,1,4,5,1,3,2,6,1,5,1,1,1,2,1,1,1,1,1,1,5,2,3,1,1,2,2,2,3,
		1,14,1,6,3,14,1,3,3,1,9,11,3,8,3,8,5,1,3,1,2,5,7,3,1,3,4,2,2,11,13,2,3,2,12,2,2,1,2,2,1,1,17,10,2,22,3,18,5,1,3,5,1,5,2,2,10,9,1,8,1,1,6,2,1,
		3,2,3,1,2,1,3,3,5,1,9,7,2,7,2,5,1,4,12,2,7,7,2,14,8,2,1,3,13,5,1,1,2,9,10,5,8,1,5,1,1,5,4,3,1,3,27,4,9,3,1,4,1,1,7,10,10,1,2,2,7,3,13,1,1,7,
		1,3,2,2,8,6,5,2,5,1,1,1,2,246,9,2,1,4,2,1,1,4,2,2,1,2,2,1,7,2,1,1,1,2,1,5,5,7,2,1,2,5,4,3,2,1,7,1,1,2,4,5,1,3,55,5,7,2,4,1,3,9,4,1,2,2,7,1,1,
		2,1,11,1,1,1,1,3,1,1,1,1,4,2,4,2,1,2,1,1,1,2,3,8,1,2,2,1,2,1,1,5,1,2,1,1,2,1,4,3,4,1,2,6,1,1,2,2,1,1,1,2,1,4,1,3,1,1,1,2,2,1,1,1,3,4,1,3,2,1,
		6,1,5,2,1,5,2,4,1,2,2,5,4,2,1,1,2,3,1,1,2,2,3,3,3,2,6,3,3,6,2,6,1,4,1,4,2,2,2,1,9,4,3,3,2,2,1,1,2,2,2,1,2,2,4,3,5,1,2,5,3,1,1,5,2,2,1,8,4,4,
		3,3,2,3,2,6,1,15,3,2,3,8,9,17,1,4,1,2,1,5,4,2,1,1,2,1,2,4,3,1,1,1,1,4,1,2,7,3,8,1,7,3,1,3,1,1,9,5,1,1,1,1,5,1,3,3,3,9,4,4,1,1,2,1,52,2,5,2,5,
		13,1,2,21,1,3,3,7,2,2,1,1,3,2,8,1,1,5,2,4,1,2,4,2,1,1,4,5,1,2,1,3,4,4,2,15,1,4,3,4,2,1,4,1,1,3,2,63,1,1,1,12,3,4,1,1,5,1,8,5,3,1,1,2,8,1,1,3,
		3,3,2,2,3,11,1,3,1,6,3,4,2,4,9,1,3,1,6,15,3,5,7,7,2,2,1,2,8,8,2,3,1,4,3,2,1,4,1,1,61,5,3,8,4,4,4,11,2,2,1,4,6,1,3,1,3,4,1,2,1,1,2,3,3,4,11,18,
		2,5,3,1,2,2,1,1,1,4,1,2,1,9,1,5,1,1,1,1,8,1,1,2,3,4,2,21,5,15,11,3,1,1,3,1,8,1,8,2,1,2,1,6,5,7,6,3,1,5,2,1,2,2,5,6,1,1,1,4,8,1,1,3,15,2,2,1,
		1,2,1,3,2,1,1,1,6,6,3,1,8,1,1,1,2,7,1,1,3,8,1,1,10,5,3,6,7,15,2,109,2,2,2,1,4,3,1,2,13,3,1,1,1,5,4,3,6,4,4,3,2,2,1,1,15,3,1,2,3,2,2,8,1,8,3,
		1,1,2,1,3,8,3,3,8,13,2,6,11,10,5,2,4,3,2,4,2,1,6,1,2,2,9,4,6,13,7,2,3,6,1,1,3,29,1,3,87,3,1,4,2,2,7,3,1,5,1,1,2,2,6,2,6,1,3,1,3,11,1,1,1,1,2,
		1,5,8,1,1,1,1,2,2,4,1,2,1,1,1,2,5,3,7,5,2,1,2,2,1,1,10,1,6,5,2,11,1,11,15,3,12,1,3,1,3,2,11,1,1,1,1,3,1,3,2,6,4,1,22,8,7,1,3,
	};
	static ImWchar base_ranges[] = // not zero-terminated
	{
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
		0x31F0, 0x31FF, // Katakana Phonetic Extensions
		0xFF00, 0xFFEF, // Half-width characters
		0xFFFD, 0xFFFD  // Invalid
	};
	static ImWchar full_ranges[IM_ARRAYSIZE(base_ranges) + IM_ARRAYSIZE(accumulative_offsets_from_0x4E00)*2 + 1] = { 0 };
	if (!full_ranges[0])
	{
		memcpy(full_ranges, base_ranges, sizeof(base_ranges));
		UnpackAccumulativeOffsetsIntoRanges(0x4E00, accumulative_offsets_from_0x4E00, IM_ARRAYSIZE(accumulative_offsets_from_0x4E00), full_ranges + IM_ARRAYSIZE(base_ranges));
	}
	return &full_ranges[0];
}

#undef CHAR_PATH_SEPARATOR
