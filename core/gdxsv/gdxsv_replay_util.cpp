#include "gdxsv_replay_util.h"

#include <sys/stat.h>
#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif

#include "dirent.h"
#include "gdxsv.h"
#include "oslib/directory.h"
#include "oslib/oslib.h"
#include "rend/boxart/http_client.h"
#include "rend/gui_util.h"
#include "stdclass.h"

static bool read_dir = false;
static std::vector<std::pair<std::string, uint64_t>> files;
static std::string selected_replay_file;
static std::string battle_log_file_name;
static proto::BattleLogFile battle_log;
static int pov_index = 0;

static bool download_replay_savestate(int disk, const std::string& save_path) {
	std::string content_type;
	http::init();
	std::vector<u8> downloaded;
	std::string url = "https://storage.googleapis.com/gdxsv/misc/gdx-disc2_99.state";
	if (disk == 1) {
		std::string url = "https://storage.googleapis.com/gdxsv/misc/gdx-disc1_99.state";
	}
	int rc = http::get(url, downloaded, content_type);
	if (rc != 200) {
		ERROR_LOG(COMMON, "replay savestate download failure: %s", url.c_str());
		return false;
	}

	FILE* fp = nowide::fopen(save_path.c_str(), "wb");
	if (fp == nullptr) {
		ERROR_LOG(COMMON, "replay savestate save failure: %s", url.c_str());
		return false;
	}

	auto written = fwrite(downloaded.data(), 1, downloaded.size(), fp);
	std::fclose(fp);
	return written == downloaded.size();
}

void gdxsv_start_replay(const std::string& replay_file, int pov) {
	if (gdxsv.IsSaveStateAllowed()) {
		dc_savestate(90);
	}

	bool ok = true;
	auto savestate_path = hostfs::getSavestatePath(99, false);
	if (!file_exists(savestate_path)) {
		ok = false;
		if (download_replay_savestate(2, savestate_path)) {
			ok = true;
		}
	}

	if (ok) {
		dc_loadstate(99);
		gui_state = GuiState::Closed;
		gdxsv.StartReplayFile(replay_file.c_str(), pov);
	}
}

void gdxsv_end_replay() {
	dc_loadstate(90);
	if (!selected_replay_file.empty()) {
		gui_state = GuiState::GdxsvReplay;
	}
}

void gdxsv_replay_select_dialog() {
	auto replay_dir = get_writable_data_path("replays");

	centerNextWindow();
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

	ImGui::Begin("##gdxsv_emu_replay_menu", nullptr,
				 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
	ImGui::AlignTextToFramePadding();
	ImGui::Indent(10 * settings.display.uiScale);

	ImGui::SameLine();
	if (ImGui::Button("Close")) {
		gui_state = GuiState::Commands;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload")) {
		read_dir = false;
	}

	if (!read_dir) {
		files.clear();
		read_dir = true;
		DIR* dir = flycast::opendir(replay_dir.c_str());

		while (true) {
			struct dirent* entry = flycast::readdir(dir);
			if (entry == NULL) break;
			std::string name(entry->d_name);
#ifdef __APPLE__
			extern std::string os_PrecomposedString(std::string string);
			name = os_PrecomposedString(name);
#endif
			if (name == ".") continue;
			std::string extension = get_file_extension(name);
			if (extension == "pb") {
				struct stat result;
				if (stat((replay_dir + "/" + name).c_str(), &result) == 0) {
					files.emplace_back(name, result.st_mtime);
				}
			}
		}
		flycast::closedir(dir);
	}

	ImGui::Unindent(10 * settings.display.uiScale);
	ImGui::PopStyleVar();

	ImGui::BeginChild(ImGui::GetID("gdxsv_replay_file_list"), ImVec2(330, 0), true, ImGuiWindowFlags_DragScrolling);
	{
		if (files.empty()) {
			ImGui::Text("(No replay found)");
		} else {
			for (int i = 0; i < files.size(); ++i) {
				ImGui::PushID(i);
				if (ImGui::Selectable(files[i].first.c_str(), files[i].first == selected_replay_file, 0, ImVec2(0, 0))) {
					selected_replay_file = files[i].first;
				}
				ImGui::SameLine();

				time_t t = files[i].second;
				char buf[128] = {0};
				std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
				ImGui::Text(buf);
				ImGui::PopID();
			}
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild(ImGui::GetID("gdxsv_replay_file_detail"), ImVec2(0, 0), true, ImGuiWindowFlags_DragScrolling);
	{
		if (!selected_replay_file.empty()) {
			auto replay_file_path = replay_dir + "/" + selected_replay_file;
			if (battle_log_file_name != selected_replay_file) {
				battle_log_file_name = selected_replay_file;
				FILE* fp = nowide::fopen(replay_file_path.c_str(), "rb");
				if (fp != nullptr) {
					battle_log.ParseFromFileDescriptor(fileno(fp));
					std::fclose(fp);
				}
				pov_index = 0;
			}

			bool playable = "dc" + std::to_string(gdxsv.Disk()) == battle_log.game_disk();

			ImGui::Text("BattleCode: %s", battle_log.battle_code().c_str());
			ImGui::Text("Game: %s", battle_log.game_disk().c_str());
			ImGui::Text("Players: %d", battle_log.users_size());

			char buf[128] = {0};
			time_t time = battle_log.start_at();
			std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
			ImGui::Text("StartAt: %s", buf);
			time = battle_log.end_at();
			std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
			ImGui::Text("EndAt: %s", buf);

			auto textCentered = [](const std::string& text) {
				auto windowWidth = ImGui::GetWindowSize().x;
				auto textWidth = ImGui::CalcTextSize(text.c_str()).x;
				ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
				ImGui::Text(text.c_str());
			};

			std::vector<int> renpo_index, zeon_index;
			for (int i = 0; i < battle_log.users_size(); i++) {
				if (battle_log.users(i).team() == 1) renpo_index.push_back(i);
				if (battle_log.users(i).team() == 2) zeon_index.push_back(i);
			}

			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.2f, .1f, .6f, 1));
			for (int i : renpo_index) {
				if (i != renpo_index.front()) ImGui::SameLine();
				ImGui::BeginChild(ImGui::GetID(("gdxsv_replay_file_detail_renpo_" + std::to_string(i)).c_str()), ScaledVec2(180, 80), true,
								  ImGuiWindowFlags_NoScrollbar);
				textCentered("ID: " + battle_log.users(i).user_id());
				textCentered("HN: " + battle_log.users(i).user_name());
				textCentered("PN: " + battle_log.users(i).pilot_name());
				ImGui::EndChild();
			}
			ImGui::PopStyleColor();

			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.6f, .1f, .2f, 1));
			for (int i : zeon_index) {
				if (i != zeon_index.front()) ImGui::SameLine();
				ImGui::BeginChild(ImGui::GetID(("gdxsv_replay_file_detail_zeon_" + std::to_string(i)).c_str()), ScaledVec2(180, 80), true,
								  ImGuiWindowFlags_NoScrollbar);
				textCentered("ID: " + battle_log.users(i).user_id());
				textCentered("HN: " + battle_log.users(i).user_name());
				textCentered("PN: " + battle_log.users(i).pilot_name());
				ImGui::EndChild();
			}
			ImGui::PopStyleColor();

			auto povString = [](int i) -> std::string {
				const auto& u = battle_log.users(i);
				char buf[128] = {};
				snprintf(buf, sizeof(buf), "%dP\tID: %s\tHN: %s\tPN: %s", i + 1, u.user_id().c_str(), u.user_name().c_str(),
						 u.pilot_name().c_str());
				return buf;
			};

			if (ImGui::BeginCombo("POV", povString(pov_index).c_str())) {
				for (int i = 0; i < battle_log.users_size(); i++) {
					bool selected = pov_index == i;
					if (ImGui::Selectable(povString(i).c_str(), selected, 0, ImVec2(0, 0))) {
						pov_index = i;
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::ButtonEx("Play", ScaledVec2(240, 50), playable ? 0 : ImGuiButtonFlags_Disabled)) {
				gdxsv_start_replay(replay_dir + "/" + selected_replay_file, pov_index);
			}
		}
	}
	ImGui::EndChild();

	ImGui::End();
}
