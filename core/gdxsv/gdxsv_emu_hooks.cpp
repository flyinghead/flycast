#include "gdxsv_emu_hooks.h"

#include <regex>
#include <sstream>

#include "cfg/cfg.h"
#include "gdxsv.h"
#include "gdxsv_replay_util.h"
#include "gdxsv_update.h"
#include "hw/maple/maple_if.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "json.hpp"
#include "nowide/fstream.hpp"
#include "oslib/directory.h"
#include "oslib/oslib.h"
#include "rend/boxart/http_client.h"
#include "rend/gui_util.h"
#include "stdclass.h"
#include "xxhash.h"

using namespace nlohmann;

static void gdxsv_update_popup();
static void wireless_warning_popup();

bool gdxsv_enabled() { return gdxsv.Enabled(); }

bool gdxsv_is_ingame() { return gdxsv.InGame(); }

bool gdxsv_is_online() { return gdxsv.IsOnline(); }

bool gdxsv_is_savestate_allowed() { return gdxsv.IsSaveStateAllowed(); }

bool gdxsv_is_replaying() { return gdxsv.IsReplaying(); }

void gdxsv_stop_replay() { gdxsv.StopReplay(); }

void gdxsv_emu_flycast_init() { config::GGPOEnable = false; }

void gdxsv_emu_start() {
	gdxsv.Reset();

	if (gdxsv.Enabled()) {
		auto replay = cfgLoadStr("gdxsv", "replay", "");
		if (!replay.empty()) {
			dc_savestate(90);
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
			auto replay_pov = cfgLoadInt("gdxsv", "ReplayPOV", 1);
			gdxsv.StartReplayFile(replay.c_str(), replay_pov - 1);
		}

		auto rbk_test = cfgLoadStr("gdxsv", "rbk_test", "");
		if (!rbk_test.empty() && slot == 99) {
			gdxsv.StartRollbackTest(rbk_test.c_str());
		}
	}
}

bool gdxsv_emu_menu_open() {
	if (gdxsv.Enabled()) {
		return gdxsv.HookOpenMenu();
	}
	return true;
}

bool gdxsv_widescreen_hack_enabled() { return gdxsv.Enabled() && config::WidescreenGameHacks; }

static void gui_header(const char* title) {
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ScaledVec2(0.f, 0.5f));	// Left
	ImGui::ButtonEx(title, ScaledVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
}

void gdxsv_emu_gui_display() {
	if (gui_state == GuiState::Main) {
		gdxsv_update_popup();
		wireless_warning_popup();
	}

	if (gui_state == GuiState::GdxsvReplay) {
		gdxsv_replay_select_dialog();
	}
}

void gdxsv_emu_gui_settings() {
	gui_header("gdxsv Settings");

	if (config::ThreadedRendering.get()) {
		ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1), "WARNING: Multi-threaded emulation is enabled. Disable is strongly recommended.");
		ImGui::SameLine();
		if (ImGui::Button("Set Disable")) {
			config::ThreadedRendering = false;
		}
	}

	if (config::PerStripSorting.get()) {
		ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1),
						   "WARNING: Transparent Sorting is not Per Triangle. Per Triangle is strongly recommended.");
		ImGui::SameLine();
		if (ImGui::Button("Set Per Triangle")) {
			config::PerStripSorting = false;
		}
	}

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

	if (ImGui::Button(" Apply Recommended Settings ", ScaledVec2(0, 40))) {
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
      Enable UPnP: Yes
      Gdx Minimum Delay: 2

    Gdxsv:
      SaveReplay: yes
      UploadReplay: yes)");

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
	if (ImGui::Button("UPnP Now", ScaledVec2(buttonWidth, 0)) && !upnp_future.valid()) {
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
	if (ImGui::Button("Test The Port", ScaledVec2(buttonWidth, 0)) && !udp_test_future.valid()) {
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

static void textCentered(const std::string& text) {
	const auto windowWidth = ImGui::GetWindowSize().x;
	const auto textWidth = ImGui::CalcTextSize(text.c_str()).x;
	ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
	ImGui::Text(text.c_str());
}

static void textCentered(const ImVec4& color, const std::string& text) {
	const auto windowWidth = ImGui::GetWindowSize().x;
	const auto textWidth = ImGui::CalcTextSize(text.c_str()).x;
	ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
	ImGui::TextColored(color, text.c_str());
}

static void gdxsv_update_popup() {
	static bool update_popup_shown = false;
	static std::shared_future<bool> self_update_result;
	bool no_popup_opened = !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

	if (gdxsv_update.IsUpdateAvailable() && !update_popup_shown && no_popup_opened) {
		ImGui::OpenPopup("New version");
	}

	if (ImGui::BeginPopupModal("New version", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * settings.display.uiScale);
		ImGui::TextWrapped("  %s is available for download!  ", gdxsv_update.GetLatestVersionTag().c_str());
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * settings.display.uiScale, 3 * settings.display.uiScale));
		float currentwidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x -
							 -55.f * settings.display.uiScale);
		if (GdxsvUpdate::IsSupportSelfUpdate()) {
			if (ImGui::Button("Update", ImVec2(100.f * settings.display.uiScale, 0.f))) {
				self_update_result = gdxsv_update.StartSelfUpdate();
				update_popup_shown = true;
				ImGui::CloseCurrentPopup();
			}
		} else {
			if (ImGui::Button("Download", ImVec2(100.f * settings.display.uiScale, 0.f))) {
				os_LaunchFromURL(GdxsvUpdate::DownloadPageURL());
				update_popup_shown = true;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x +
							 -55.f * settings.display.uiScale);
		if (ImGui::Button("Cancel", ImVec2(100.f * settings.display.uiScale, 0.f))) {
			update_popup_shown = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}

	if (self_update_result.valid() && no_popup_opened) {
		ImGui::OpenPopup("Update");
	}

	ImGui::SetNextWindowSize(ScaledVec2(340, 0));
	centerNextWindow();
	ImVec2 padding = ScaledVec2(20, 20);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	if (ImGui::BeginPopupModal("Update", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * settings.display.uiScale, 3 * settings.display.uiScale));

		if (self_update_result.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
			if (self_update_result.get()) {
				textCentered(ImVec4(0, 0.8, 0, 1), "Download Completed");
				textCentered("Please restart the emulator");

				if (ImGui::Button("Exit", ScaledVec2(300, 30))) {
					self_update_result = {};
					ImGui::CloseCurrentPopup();
					dc_exit();
				}
			} else {
				textCentered(ImVec4(0.8, 0, 0, 1), "Download Failed");
				textCentered("Please download the latest version manually");

				if (ImGui::Button("Download", ScaledVec2(300, 30))) {
					self_update_result = {};
					os_LaunchFromURL(GdxsvUpdate::DownloadPageURL());
					ImGui::CloseCurrentPopup();
				}
			}
		} else {
			ImGui::Text("Updating...");
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
			ImGui::ProgressBar(gdxsv_update.SelfUpdateProgress(), ImVec2(-1, 20.f * settings.display.uiScale));
			ImGui::PopStyleColor();
		}

		ImGui::SetItemDefaultFocus();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
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
