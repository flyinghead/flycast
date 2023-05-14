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
#include "json.hpp"

struct ReplayEntry {
	std::string disk;
	std::vector<proto::BattleLogUser> users;
	int round = 0;
	int renpo_win = 0;
	int zeon_win = 0;
	time_t start_unix;
	std::string replay_url;
};

static bool read_dir = false;
static std::vector<std::pair<std::string, uint64_t>> files;
static std::string selected_replay_file;
static std::string battle_log_file_name;
static proto::BattleLogFile battle_log;

static std::string search_user_id;
static std::shared_future<std::vector<ReplayEntry>> fetch_replay_entry_future_;
static int selected_replay_entry_index = -1;

static int entry_paging = 0;

static int pov_index = -1;

static ImVec2 normal_padding;
static float scaling;

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
	settings.input.fastForwardMode = false;

	emu.start();
	emu.render();
	emu.stop();
	if (!selected_replay_file.empty() || selected_replay_entry_index != -1) {
		gui_state = GuiState::GdxsvReplay;
	} else {
		// Replay from command-line, resume game when end replaying
		emu.start();
	}
}

auto textCentered = [](const std::string& text) {
	auto windowWidth = ImGui::GetWindowSize().x;
	auto textWidth = ImGui::CalcTextSize(text.c_str()).x;
	ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
	ImGui::Text(text.c_str());
};

void gdxsv_replay_draw_forces(const bool is_renpo, const std::vector<int>& force_index, int& user_index, const std::vector<proto::BattleLogUser>& users) {
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f * scaling);

	if (is_renpo) {
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.42f, .79f, .99f, 1));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.055f, .122f, .227f, .3f));
	} else {
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.97f, .23f, .35f, 1));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.196f, .07f, .05f, .3f));
	}
	
	for (int i : force_index) {
		if (i != force_index.front()) ImGui::SameLine();
		auto pos = ImGui::GetCursorPos();
		if (ImGui::Selectable(("##pov_" + std::to_string(user_index)).c_str(), (pov_index == user_index), 0, ScaledVec2(180, 90))) {
			if (pov_index == user_index) {
				pov_index = -1;
			} else {
				pov_index = user_index;
			}
		}
		ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
		ImGui::BeginChild(
			ImGui::GetID(("gdxsv_replay_file_detail_renpo_" + std::to_string(i)).c_str()), ScaledVec2(180, 90), true,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
		textCentered("ID: " + users[user_index].user_id());
		textCentered("HN: " + users[user_index].user_name());
		textCentered("PN: " + users[user_index].pilot_name());
		ImGui::EndChild();
		user_index++;
	}
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	
	ImGui::PopStyleVar();
}

void gdxsv_replay_draw_players(const std::vector<proto::BattleLogUser>& users) {
	std::vector<int> renpo_index, zeon_index;
	for (int i = 0; i < users.size(); i++) {
		if (users[i].team() == 1) renpo_index.push_back(i);
		if (users[i].team() == 2) zeon_index.push_back(i);
	}
	int user_index = 0;

	gdxsv_replay_draw_forces(true, renpo_index, user_index, users);
	gdxsv_replay_draw_forces(false, zeon_index, user_index, users);
}

void gdxsv_replay_local_tab() {
	const auto replay_dir = get_writable_data_path("replays");
	
	if (!read_dir) {
		files.clear();
		read_dir = true;

		if (file_exists(replay_dir)) {
			DIR* dir = flycast::opendir(replay_dir.c_str());

			while (true) {
				struct dirent* entry = flycast::readdir(dir);
				if (entry == nullptr) break;
				std::string name(entry->d_name);
#ifdef __APPLE__
				extern std::string os_PrecomposedString(std::string string);
				name = os_PrecomposedString(name);
#endif
				if (name == ".") continue;
				std::string extension = get_file_extension(name);
				if (extension == "pb") {
					struct stat result;
					if (flycast::stat((replay_dir + "/" + name).c_str(), &result) == 0) {
						files.emplace_back(name, result.st_mtime);
					}
				}
			}
			std::sort(files.begin(), files.end(), std::greater<>());

			flycast::closedir(dir);
		}
	}
	
	if (ImGui::Button("Reload")) {
		read_dir = false;
	}
	ImGui::SameLine();
#if defined(__APPLE__) && TARGET_OS_OSX
	if (ImGui::Button("Reveal in Finder")) {
		char temp[512];
		sprintf(temp, "open \"%s\"", get_writable_data_path("replays").c_str());
		system(temp);
	}
#elif defined(_WIN32) && !defined(TARGET_UWP)
	if (ImGui::Button("Open folder")) {
	}
#endif
	ImGui::SameLine();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	ImVec2 size;
	size.x = ImGui::GetContentRegionAvail().x;
	size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f);
	if (ImGui::ListBoxHeader("##Replay File Directory", size))
	{
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", get_writable_data_path("replays").c_str());
		
		ImGui::ListBoxFooter();
	}
	ImGui::PopStyleVar();
	
	ImGui::BeginChild(ImGui::GetID("gdxsv_replay_file_list"), ImVec2(330, 0) * scaling, true, ImGuiWindowFlags_DragScrolling);
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
			const auto replay_file_path = replay_dir + "/" + selected_replay_file;
			if (battle_log_file_name != selected_replay_file) {
				battle_log_file_name = selected_replay_file;
				battle_log.Clear();
				FILE* fp = nowide::fopen(replay_file_path.c_str(), "rb");
				if (fp != nullptr) {
					battle_log.ParseFromFileDescriptor(fileno(fp));
					std::fclose(fp);
				}
				pov_index = -1;
			}

			const bool playable = "dc" + std::to_string(gdxsv.Disk()) == battle_log.game_disk();

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
			OptionCheckbox("Hide name", config::GdxReplayHideName, "Replace player names with generic names");
			ImGui::NewLine();

			gdxsv_replay_draw_players(std::vector<proto::BattleLogUser>(battle_log.users().begin(), battle_log.users().end()));
			
			ImGui::NewLine();

			bool pov_selected = (pov_index == -1);
			DisabledScope scope(pov_selected);

			if (ImGui::ButtonEx(pov_selected ? "Select a player" : "Replay", ScaledVec2(240, 50),
								playable ? 0 : ImGuiButtonFlags_Disabled) &&
				!scope.isDisabled()) {
				gdxsv_start_replay(replay_dir + "/" + selected_replay_file, pov_index);
			}
		}
	}
	ImGui::EndChild();
}

void HandleReplayJSON(const std::string& json_string, std::vector<ReplayEntry>& out) {
	try {
		nlohmann::json j = nlohmann::json::parse(json_string);
		
		for(auto item : j) {
			ReplayEntry entry;
			entry.disk = item.at("disk");
			
			for(auto u : item.at("users")) {
				auto user = proto::BattleLogUser();
				user.set_user_id(u.at("user_id"));
				user.set_user_name(u.at("user_name"));
				user.set_pilot_name(u.at("pilot_name"));
				user.set_team(u.at("team"));
				user.set_pos(u.at("pos"));
				entry.users.push_back(user);
			}
			
			entry.round = item.value("round", 0);
			entry.renpo_win = item.value("renpo_win", 0);
			entry.zeon_win = item.value("zeon_win", 0);
			entry.start_unix = item.at("start_unix");
			entry.replay_url = item.at("replay_url");
			
			out.push_back(entry);
		}
	} catch (const nlohmann::json::exception& e) {
		WARN_LOG(COMMON, "json parse failure: %s", e.what());
	}
}



template <typename T>
static bool future_is_ready(const T& future) {
	return future.valid() && future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

void FetchReplayJSON() {
	if (fetch_replay_entry_future_.valid()) {
		return;
	}

	const auto future_fn = []() -> std::vector<ReplayEntry> {
		std::vector<ReplayEntry> entries{};
		std::vector<u8> dl;
		std::string content_type;
		http::init();
		std::string url = "https://asia-northeast1-gdxsv-274515.cloudfunctions.net/lbsapi/replay?";
		url += "page=" + http::urlEncode(std::to_string(entry_paging));
		if (search_user_id != "") {
			url += "&user_id=" + http::urlEncode(search_user_id);
		}
		const int rc = http::get(url, dl, content_type);
		if (rc != 200) {
			ERROR_LOG(COMMON, "version check failure: %s", url.c_str());
			return entries;
		}

		HandleReplayJSON(std::string(dl.begin(), dl.end()), entries);
		return entries;
	};

	fetch_replay_entry_future_ = std::async(std::launch::async, future_fn).share();
}

void FetchNewResults(){
	selected_replay_entry_index = -1;
	fetch_replay_entry_future_ = std::shared_future<std::vector<ReplayEntry>>();
}

void gdxsv_replay_server_tab() {
	ImGui::AlignTextToFramePadding();
	
	ImGui::Text("User ID");
	ImGui::SameLine();
	static char user_id_buf[7];
	ImGui::InputText("##user_id_input", user_id_buf, IM_ARRAYSIZE(user_id_buf));
	ImGui::SameLine();
	if (ImGui::Button("Search")) {
		search_user_id = std::string(user_id_buf);
		entry_paging = 0;
		FetchNewResults();
	}
	
	ImGui::BeginChild(ImGui::GetID("gdxsv_replay_server_list_paging"), ScaledVec2(330, 0), false, ImGuiWindowFlags_NoDecoration);
	{
		ImGui::BeginChild(ImGui::GetID("gdxsv_replay_server_list"), ImVec2(0, ImGui::GetContentRegionAvail().y - 40.f * scaling), true, ImGuiWindowFlags_DragScrolling);
		{
			if (!fetch_replay_entry_future_.valid()) {
				FetchReplayJSON();
			} else if (!future_is_ready(fetch_replay_entry_future_)) {
				ImGui::Text("Loading...");
			} else {
				const auto& entries = fetch_replay_entry_future_.get();
				for (int i = 0; i < entries.size(); ++i) {
					ImGui::PushID(i);
					
					time_t t = entries[i].start_unix;
					char buf[128] = {0};
					std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
					
					if (ImGui::Selectable(buf, i == selected_replay_entry_index, 0, ImVec2(0, 0))) {
						selected_replay_entry_index = i;
						pov_index = -1;
					}
					
					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();
		{
			DisabledScope scope(entry_paging == 0);
			if (ImGui::Button("Prev Page") && !scope.isDisabled()){
				entry_paging--;
				FetchNewResults();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Next Page")) {
			entry_paging++;
			FetchNewResults();
		}
	}
	ImGui::EndChild();
	
	ImGui::SameLine();
	
	ImGui::BeginChild(ImGui::GetID("gdxsv_replay_server_detail"), ImVec2(0, 0), true, ImGuiWindowFlags_DragScrolling);
	{
		if (selected_replay_entry_index != -1) {
			const auto& entries = fetch_replay_entry_future_.get();
			const auto& entry = entries[selected_replay_entry_index];
			
			const bool playable = "dc" + std::to_string(gdxsv.Disk()) == entry.disk;
			
//			ImGui::Text("BattleCode: %s", battle_log.battle_code().c_str());
			ImGui::Text("Game: %s", entry.disk.c_str());
			ImGui::Text("Players: %lu", entry.users.size());
//
			char buf[128] = {0};
			time_t time = entry.start_unix;
			std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
			ImGui::Text("StartAt: %s", buf);
//			time = battle_log.end_at();
//			std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
//			ImGui::Text("EndAt: %s", buf);
			OptionCheckbox("Hide name", config::GdxReplayHideName, "Replace player names with generic names");
			ImGui::NewLine();

			gdxsv_replay_draw_players(entry.users);
			
			ImGui::NewLine();

			bool pov_selected = (pov_index == -1);
			DisabledScope scope(pov_selected);

			if (ImGui::ButtonEx(pov_selected ? "Select a player" : "Replay", ScaledVec2(240, 50),
								playable ? 0 : ImGuiButtonFlags_Disabled) &&
				!scope.isDisabled()) {
				gdxsv_start_replay(entry.replay_url, pov_index);
			}
		}
	}
	ImGui::EndChild();
}

void gdxsv_replay_select_dialog() {
	centerNextWindow();
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	
	scaling = settings.display.uiScale;
	normal_padding = ImGui::GetStyle().FramePadding;
	
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::Begin("Replays##gdxsv_emu_replay_menu", nullptr,
				 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
	
	if (ImGui::Button("Close", ScaledVec2(100, 40))) {
		gui_state = GuiState::Commands;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(16, 6));
	if (ImGui::BeginTabBar("replays", ImGuiTabBarFlags_NoTooltip))
	{
		if (ImGui::BeginTabItem("Local"))
		{
			gdxsv_replay_local_tab();
			ImGui::EndTabItem();
		}
		
		if (ImGui::BeginTabItem("Server"))
		{
			gdxsv_replay_server_tab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::PopStyleVar();

	ImGui::End();
	ImGui::PopStyleVar(); // ImGuiStyleVar_WindowRounding
}
