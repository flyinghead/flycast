#include "gdxsv_emu_hooks.h"

#include <regex>

#include "gdxsv.h"
#include "hw/maple/maple_if.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "oslib/oslib.h"
#include "rend/gui_util.h"
#include "version.h"
#include <fstream>

#ifdef _WIN32
#define CHAR_PATH_SEPARATOR '\\'
#else
#define CHAR_PATH_SEPARATOR '/'
#endif

void gdxsv_latest_version_check();
static bool gdxsv_update_available = false;
static std::string gdxsv_latest_version_tag;

void gdxsv_flycast_init() {
	std::ifstream fs;
	fs.open(get_writable_data_path("crash_dmp_list.txt"));
	std::vector<std::string> unhandled_dmp = {};
	if (fs.is_open()) {
		std::string line;
		
		while (std::getline(fs, line)) {
			std::map<std::string, std::string> files = {};
			files["upload_file_minidump"] = line;
			
			std::string log = line;
			if (log.find(".dmp") != -1) {
				log.replace(log.find(".dmp"), sizeof(".dmp") - 1, ".log");
				files["flycast_log"] = log;
			}
			files["emu_cfg"] = get_writable_config_path("emu.cfg");
			
			int result = os_UploadFilesToURL("https://o4503934635540480.ingest.sentry.io/api/4503936127270912/minidump/?sentry_key=3da24b2132294f8d9e02a11a5e3db8ec", files);
			NOTICE_LOG(COMMON, "Upload status: %d, %s", result, line.c_str());
			if (result != 200) {
				unhandled_dmp.push_back(line);
			}
		}
	} else {
		return;
	}
	
	FILE* fp = nowide::fopen(get_writable_data_path("crash_dmp_list.txt").c_str(), "w");
	if (fp == nullptr) {
		NOTICE_LOG(COMMON, "fopen failed");
	} else {
		if (unhandled_dmp.size()) {
			for (auto dmp : unhandled_dmp) {
				fprintf(fp, "%s\n", dmp.c_str());
			}
		}
		fclose(fp);
	}
}

void gdxsv_prepare_crashlog(const char* dump_dir, const char* minidump_id) {
	FILE* fp = nowide::fopen(get_writable_data_path("crash_dmp_list.txt").c_str(), "at");
	if (fp == nullptr) {
		NOTICE_LOG(COMMON, "fopen failed");
	} else {
		fprintf(fp, "%s%c%s.dmp\n", dump_dir, CHAR_PATH_SEPARATOR, minidump_id);
		fclose(fp);
	}
	
	size_t size = strlen(dump_dir) + strlen(minidump_id) + 6;
	char* slog_path = new char[size];
	std::snprintf(slog_path, size, "%s%c%s.log", dump_dir, CHAR_PATH_SEPARATOR, minidump_id);
	FILE* slog_fp = nowide::fopen(slog_path, "w");
	delete[] slog_path;
	if (slog_fp == nullptr) return;
	
	std::ifstream fs;
	fs.open(get_writable_data_path("flycast.log"));
	
	if (fs.is_open()) {
		fs.seekg(-10000, std::ios_base::end);
		std::string line;
		while(std::getline(fs, line)){
			fprintf(slog_fp, "%s\n", line.c_str());
		}
	}
	fclose(slog_fp);
}

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

void gdxsv_emu_update() {
	if (gdxsv.Enabled()) {
		gdxsv.Update();
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

bool gdxsv_emu_enabled() { return gdxsv.Enabled(); }

bool gdxsv_emu_ingame() { return gdxsv.InGame(); }

void gdxsv_update_popup() {
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
							 55.f * settings.display.uiScale);
		if (ImGui::Button("Download", ImVec2(100.f * settings.display.uiScale, 0.f))) {
			gdxsv_update_available = false;
			os_LaunchFromURL("https://github.com/inada-s/flycast/releases/latest/");
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		ImGui::SetCursorPosX((currentwidth - 100.f * settings.display.uiScale) / 2.f + ImGui::GetStyle().WindowPadding.x +
							 55.f * settings.display.uiScale);
		if (ImGui::Button("Cancel", ImVec2(100.f * settings.display.uiScale, 0.f))) {
			gdxsv_update_available = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}
}

inline static void gui_header(const char *title) {
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));	// Left
	ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
}

void gdxsv_emu_settings() {
	gui_header("gdxsv Settings");

	ImGui::Columns(5, "gdxlang", false);
	ImGui::SetColumnWidth(0, 135.0f * settings.display.uiScale);
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

	if (ImGui::Button("Apply Recommended Settings", ImVec2(0, 40))) {
		// Controls
		config::MapleMainDevices[0].set(MapleDeviceType::MDT_SegaController);
		config::MapleExpansionDevices[0][0].set(MDT_SegaVMU);
		// Video
		config::PerStripSorting = false;
		config::AutoSkipFrame = 2;
		config::VSync = true;
		config::DelayFrameSwapping = false;
#if defined(_WIN32)
		config::RendererType.set(RenderType::DirectX11);
#else
		config::RendererType.set(RenderType::OpenGL);
#endif
		config::RenderResolution = 960;
		config::SkipFrame = 0;
		// Audio
		config::DSPEnabled = false;
		config::AudioVolume.set(50);
		config::AudioVolume.calcDbPower();
		config::AudioBufferSize = 706;
		// Others
		config::DynarecEnabled = true;
		config::DynarecIdleSkip = true;
		config::ThreadedRendering = false;

		// Network
		config::EnableUPnP = true;
		config::GdxLocalPort = 0;
		config::GdxMinDelay = 2;

		maple_ReconnectDevices();
	}
	ImGui::SameLine();
	ShowHelpMarker(R"(Use gdxsv recommended settings:
    Control:
      Device A: Sega Controller / Sega VMU
    
    Video:
      Transparent Sorting: Per Triangle
      Automatic Frame Skipping: Maximum
      VSync: Yes
      Delay Frame Swapping: No
      Renderer: DirectX 11 (Windows) / OpenGL (OtherOS)
      Internal Resolution: 1280x960 (x2)
      Frame Skipping: 0
    
    Audio:
      Enable DSP: No
      Volume Level: 50%
      Latency: 16ms
    
    Advanced:
      CPU Mode: Dynarec
      Dynarec Idle Skip: Yes
      Multi-threaded emulation: No

    Network:
      Enable UPnP
      Gdx Local UDP Port: 0
      Gdx Minimum Delay: 2)");

	gui_header("Network Settings (P2P Lobby Only)");
	OptionCheckbox("Enable UPnP", config::EnableUPnP, "Automatically configure your network router for netplay");

	char local_port[256];
	sprintf(local_port, "%d", (int)config::GdxLocalPort);
	ImGui::InputText("Gdx UDP Port", local_port, sizeof(local_port), ImGuiInputTextFlags_CharsDecimal, nullptr, nullptr);
	ImGui::SameLine();
	ShowHelpMarker("The local UDP Port. Set to 0 to automatically configure next time");
	config::GdxLocalPort.set(atoi(local_port));

	static char upnp_result[256];
	if (config::GdxLocalPort == 0) {
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if (ImGui::Button("UPnP Now")) {
		auto upnp = gdxsv.UPnP();
		if (upnp.Init() && upnp.AddPortMapping(config::GdxLocalPort, false))
			strcpy(upnp_result, "Success");
		else
			strcpy(upnp_result, upnp.getLastError());
	}
	if (config::GdxLocalPort == 0) {
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
	ImGui::SameLine();
	ImGui::Text(upnp_result);

	OptionSlider("Gdx Minimum Delay", config::GdxMinDelay, 2, 6,
				 "Minimum frame of input delay used for rollback communication.\nSmaller value reduces latency, but uses more CPU and "
				 "introduces glitches.");

	ImGui::NewLine();
	gui_header("Flycast Settings");
}

void gdxsv_handle_release_json(const std::string &json) {
	const std::string version_prefix = "gdxsv-";
	const std::regex tag_name_regex(R"|#|("tag_name":"(.*?)")|#|");
	const std::regex semver_regex(R"|#|(^([0-9]+)\.([0-9]+)\.([0-9]+)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+)?$)|#|");

	auto trim_prefix = [&version_prefix](const std::string &s) {
		if (s.size() < version_prefix.size()) return s;
		if (version_prefix == s.substr(0, version_prefix.size())) return s.substr(version_prefix.size());
		return s;
	};

	std::smatch match;

	auto current_version_str = trim_prefix(std::string(GIT_VERSION));
	if (!std::regex_match(current_version_str, match, semver_regex)) return;

	if (match.size() < 4) return;
	auto current_version = std::tuple<int, int, int>(std::stoi(match.str(1)), std::stoi(match.str(2)), std::stoi(match.str(3)));

	if (!std::regex_search(json, match, tag_name_regex)) return;
	if (match.size() < 2) return;
	auto tag_name = match.str(1);
	auto latest_version_str = trim_prefix(tag_name);

	if (!std::regex_match(latest_version_str, match, semver_regex)) return;
	if (match.size() < 4) return;
	auto latest_version = std::tuple<int, int, int>(std::stoi(match.str(1)), std::stoi(match.str(2)), std::stoi(match.str(3)));

	gdxsv_latest_version_tag = tag_name;

	if (current_version < latest_version) {
		gdxsv_update_available = true;
	}
}

void gdxsv_latest_version_check() {
	static std::once_flag once;
	std::call_once(once, [] {
		std::thread([]() {
			const std::string json = os_FetchStringFromURL("https://api.github.com/repos/inada-s/flycast/releases/latest");
			if (json.empty()) return;
			gdxsv_handle_release_json(json);
		}).detach();
	});
}

void gdxsv_mainui_loop() { gdxsv.HookMainUiLoop(); }

void gdxsv_gui_display_osd() { gdxsv.DisplayOSD(); }

#undef CHAR_PATH_SEPARATOR
