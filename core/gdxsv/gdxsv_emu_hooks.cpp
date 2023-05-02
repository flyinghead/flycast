#include "gdxsv_emu_hooks.h"

#include <regex>
#include <sstream>

#include "cfg/cfg.h"
#include "gdxsv.h"
#include "hw/maple/maple_if.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "json.hpp"
#include "log/LogManager.h"
#include "nowide/fstream.hpp"
#include "oslib/oslib.h"
#include "rend/boxart/http_client.h"
#include "rend/gui_util.h"
#include "stdclass.h"
#include "version.h"
#include "xxhash.h"

using namespace nlohmann;

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

	OptionCheckbox("Save Replay", config::GdxSaveReplay, "Save replay file to replays directory");
	{
		DisabledScope scope(!config::GdxSaveReplay);
		ImGui::SameLine();
		OptionCheckbox("Upload Replay", config::GdxUploadReplay, "Automatically upload the replay file after save");
	}

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
