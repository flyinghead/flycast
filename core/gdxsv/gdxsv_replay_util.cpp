#include "gdxsv_replay_util.h"

#include <sys/stat.h>
#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

#include "dirent.h"
#include "gdxsv.h"
#include "json.hpp"
#include "oslib/directory.h"
#include "oslib/oslib.h"
#include "rend/boxart/http_client.h"
#include "rend/gui_util.h"
#include "stdclass.h"

// For macOS
std::string os_PrecomposedString(std::string string);

namespace {
struct UserEntry {
	std::string user_id;
	std::string name;
	int battle_count;
	int win_count;
	int lose_count;
	int kill_count;
	int renpo_battle_count;
	int renpo_win_count;
	int renpo_lose_count;
	int renpo_kill_count;
	int zeon_battle_count;
	int zeon_win_count;
	int zeon_lose_count;
	int zeon_kill_count;
	int daily_battle_count;
	int daily_win_count;
	int daily_lose_count;
};

struct ReplayEntry {
	std::string disk;
	std::vector<proto::BattleLogUser> users;
	int round = 0;
	int renpo_win = 0;
	int zeon_win = 0;
	time_t start_unix;
	std::string replay_url;
};

bool read_dir = false;
std::vector<std::pair<std::string, uint64_t>> files;
std::string selected_replay_file;
std::string battle_log_file_name;
std::string broken_replay_path;
proto::BattleLogFile battle_log;

std::string search_user_id;
std::string search_user_name;
std::string search_pilot_name;
std::string search_lobby_id;
unsigned int search_no_of_players;
std::string search_battle_code;
int search_ranking = -1;
std::string search_disk;
bool search_reverse;

std::shared_future<std::vector<UserEntry>> fetch_user_entry_future_;
int fetch_user_entry_http_status;

std::shared_future<std::vector<ReplayEntry>> fetch_replay_entry_future_;
int fetch_replay_entry_http_status;
int selected_replay_entry_index = -1;

int entry_paging = 0;
char page_buf[4] = "1";
int pov_index = -1;
ImVec2 normal_padding;
float scaling;

bool download_replay_savestate(int disk, const std::string& save_path) {
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

void textCentered(const std::string& text) {
	auto windowWidth = ImGui::GetWindowSize().x;
	auto textWidth = ImGui::CalcTextSize(text.c_str()).x;
	ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
	ImGui::Text(text.c_str());
};

void gdxsv_replay_draw_forces(const bool is_renpo, const std::vector<int>& force_index, int& user_index,
							  const std::vector<proto::BattleLogUser>& users) {
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
		ImGui::BeginChild(ImGui::GetID(("gdxsv_replay_file_detail_renpo_" + std::to_string(i)).c_str()), ScaledVec2(180, 90), true,
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

void gdxsv_replay_draw_info(const std::string& battle_code, const std::string& game_disk, const int& users_size,
							const std::string& close_reason, const time_t& start_time, const time_t& end_time,
							const std::vector<proto::BattleLogUser>& users, const std::string& replay_dst) {
	const bool playable = "dc" + std::to_string(gdxsv.Disk()) == game_disk;

	ImGui::Text("BattleCode: %s", battle_code.c_str());
	ImGui::Text("Game: %s", game_disk.c_str());
	ImGui::Text("Players: %d", users_size);

	char buf[128] = {0};
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&start_time));
	ImGui::Text("StartAt: %s", buf);
	if (end_time != 0) {
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&end_time));
		ImGui::Text("EndAt: %s", buf);
	}
	if (!close_reason.empty()) {
		ImGui::Text("CloseReason: %s", close_reason.c_str());
	}
	OptionCheckbox("Hide name", config::GdxReplayHideName, "Replace player names with generic names");
	OptionCheckbox("Show Ally HP", config::GdxReplayShowAllyHP, "Hack the total HP field to display Ally HP");
	ImGui::NewLine();

	gdxsv_replay_draw_players(users);

	ImGui::NewLine();

	bool pov_selected = (pov_index == -1);
	DisabledScope scope(pov_selected);

	if (ImGui::ButtonEx(pov_selected ? "Select a player" : "Replay", ScaledVec2(240, 50), playable ? 0 : ImGuiItemFlags_Disabled) &&
		!scope.isDisabled()) {
		gdxsv_start_replay(replay_dst, pov_index);
	}

	if (!broken_replay_path.empty() && broken_replay_path == replay_dst) {
		ImGui::Text("Failed to start replay. The replay file is corrupted or outdated.");
	}
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
				name = os_PrecomposedString(name);
#endif
				if (name == ".") continue;
				std::string extension = get_file_extension(name);
				if (extension == "pb") {
					struct stat result {};
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

	struct stat st {};
	if (flycast::stat(replay_dir.c_str(), &st) == 0) {
		ImGui::SameLine();
#if defined(TARGET_MAC)
		if (ImGui::Button("Reveal in Finder")) {
			char temp[512];
			snprintf(temp, sizeof(temp), "open \"%s\"", replay_dir.c_str());
			system(temp);
		}
#elif defined(_WIN32) && !defined(TARGET_UWP)
		if (ImGui::Button("Open folder")) {
			SHELLEXECUTEINFOA sei{};
			sei.cbSize = sizeof(sei);
			sei.fMask = SEE_MASK_NOCLOSEPROCESS;
			sei.lpFile = "Explorer.exe";
			sei.lpParameters = ("/root, " + replay_dir).c_str();
			sei.nShow = SW_SHOWDEFAULT;
			ShellExecuteExA(&sei);
		}
#endif
	}

	ImGui::SameLine();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	ImVec2 size;
	size.x = ImGui::GetContentRegionAvail().x;
	size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f);
	if (ImGui::BeginListBox("##Replay File Directory", size)) {
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", get_writable_data_path("replays").c_str());

		ImGui::EndListBox();
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
	scrollWhenDraggingOnVoid();
	windowDragScroll();
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

			gdxsv_replay_draw_info(battle_log.battle_code(), battle_log.game_disk(), battle_log.users_size(), battle_log.close_reason(),
								   battle_log.start_at(), battle_log.end_at(),
								   std::vector<proto::BattleLogUser>(battle_log.users().begin(), battle_log.users().end()),
								   replay_dir + "/" + selected_replay_file);
		}
	}
	scrollWhenDraggingOnVoid();
	windowDragScroll();
	ImGui::EndChild();
}

void parse_replay_json(const std::vector<u8>& json_string, std::vector<ReplayEntry>& out) {
	try {
		nlohmann::json j = nlohmann::json::parse(json_string);

		for (auto item : j) {
			ReplayEntry entry;
			entry.disk = item.at("disk");

			for (auto u : item.at("users")) {
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

void parse_user_json(const std::vector<u8>& json_string, std::vector<UserEntry>& out) {
	try {
		nlohmann::json j = nlohmann::json::parse(json_string);

		for (auto item : j) {
			UserEntry entry;
			entry.user_id = item.value("user_id", "");
			entry.name = item.value("name", "");
			entry.battle_count = item.value("battle_count", -1);
			entry.win_count = item.value("win_count", -1);
			entry.lose_count = item.value("lose_count", -1);
			entry.kill_count = item.value("kill_count", -1);
			entry.renpo_battle_count = item.value("renpo_battle_count", -1);
			entry.renpo_win_count = item.value("renpo_win_count", -1);
			entry.renpo_lose_count = item.value("renpo_lose_count", -1);
			entry.renpo_kill_count = item.value("renpo_kill_count", -1);
			entry.zeon_battle_count = item.value("zeon_battle_count", -1);
			entry.zeon_win_count = item.value("zeon_win_count", -1);
			entry.zeon_lose_count = item.value("zeon_lose_count", -1);
			entry.zeon_kill_count = item.value("zeon_kill_count", -1);
			entry.daily_battle_count = item.value("daily_battle_count", -1);
			entry.daily_win_count = item.value("daily_win_count", -1);
			entry.daily_lose_count = item.value("daily_lose_count", -1);
			out.push_back(entry);
		}
	} catch (const nlohmann::json::exception& e) {
		WARN_LOG(COMMON, "json parse failure: %s", e.what());
	}
}

template <typename T>
bool future_is_ready(const T& future) {
	return future.valid() && future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

void fetch_replay_json() {
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
		if (!search_user_id.empty()) {
			url += "&user_id=" + http::urlEncode(search_user_id);
		}
		if (!search_user_name.empty()) {
			url += "&user_name=" + http::urlEncode("%" + search_user_name + "%");
		}
		if (!search_pilot_name.empty()) {
			url += "&pilot_name=" + http::urlEncode("%" + search_pilot_name + "%");
		}
		if (!search_lobby_id.empty()) {
			url += "&lobby_id=" + http::urlEncode(search_lobby_id);
		}
		if (search_no_of_players != 0) {
			url += "&players=" + http::urlEncode(std::to_string(search_no_of_players));
		}
		if (!search_battle_code.empty()) {
			url += "&battle_code=" + http::urlEncode(search_battle_code);
		}
		if (search_ranking != -1) {
			url += "&aggregate=" + http::urlEncode(std::to_string(search_ranking));
		}
		if (!search_disk.empty()) {
			url += "&disk=" + http::urlEncode(search_disk);
		}
		if (search_reverse) {
			url += "&reverse=" + http::urlEncode(std::to_string(1));
		}

		fetch_replay_entry_http_status = http::get(url, dl, content_type);
		if (fetch_replay_entry_http_status != 200) {
			ERROR_LOG(COMMON, "version check failure: %s", url.c_str());
			return entries;
		}

		parse_replay_json(dl, entries);
		return entries;
	};

	fetch_replay_entry_future_ = std::async(std::launch::async, future_fn).share();
}

void fetch_user_json() {
	if (fetch_user_entry_future_.valid()) {
		return;
	}

	const auto future_fn = []() -> std::vector<UserEntry> {
		std::vector<UserEntry> entries{};
		std::vector<u8> dl;
		std::string content_type;
		http::init();
		std::string url = "https://asia-northeast1-gdxsv-274515.cloudfunctions.net/lbsapi/user?";

		std::string loginkey = cfgLoadStr("gdxsv", "loginkey", "");
		std::vector<u8> e_loginkey(loginkey.size());
		static constexpr int magic[] = {0x46, 0xcf, 0x2d, 0x55};
		for (int i = 0; i < e_loginkey.size(); ++i) e_loginkey[i] ^= loginkey[i] ^ magic[i & 3];
		unsigned hash = 2166136261U;
		for (const unsigned char e : e_loginkey) hash = hash * 16777619U ^ static_cast<unsigned>(e);
		std::ostringstream hashed_loginkey_s;
		hashed_loginkey_s << std::setfill('0') << std::setw(8) << std::hex << hash;

		url += "login_key=" + http::urlEncode(hashed_loginkey_s.str());

		fetch_user_entry_http_status = http::get(url, dl, content_type);
		if (fetch_user_entry_http_status != 200) {
			ERROR_LOG(COMMON, "version check failure: %s", url.c_str());
			return entries;
		}

		parse_user_json(dl, entries);
		return entries;
	};

	fetch_user_entry_future_ = std::async(std::launch::async, future_fn).share();
}

void fetch_new_results(bool reset_page = true) {
	if (reset_page) {
		entry_paging = 0;
		snprintf(page_buf, sizeof(page_buf), "%d", 1);
	}
	selected_replay_entry_index = -1;
	fetch_replay_entry_future_ = std::shared_future<std::vector<ReplayEntry>>();
}

void fetch_target_page() { fetch_new_results(false); }

template <typename Callable>
void draw_filter_label(const std::string& label, const std::string& value, Callable on_click) {
	static char filter_user_id_buf[100] = {0};
	snprintf(filter_user_id_buf, sizeof(filter_user_id_buf), u8"%s = %s  ×", label.c_str(), value.c_str());

	ImGui::SameLine();
	ImGui::GetStyle().FrameRounding = 5.0f * scaling;
	if (ImGui::Button(
			filter_user_id_buf,
			ImVec2(ImGui::CalcTextSize(filter_user_id_buf, NULL, true).x + ImGui::GetStyle().FramePadding.x * 2 - 5.0f * scaling, 0))) {
		on_click();
		fetch_new_results();
	}
	ImGui::GetStyle().FrameRounding = 0.0f;
};

void draw_filter_label_string(const std::string& label, std::string& value) {
	if (!value.empty()) {
		draw_filter_label(label, value, [&value]() { value = ""; });
	}
};

void draw_filter_label_int(const std::string& label, unsigned int& value) {
	if (value != 0) {
		draw_filter_label(label, std::to_string(value), [&value]() { value = 0; });
	}
};
void draw_filter_label_yesno(const std::string& label, int& value) {
	if (value != -1) {
		draw_filter_label(label, value ? "Yes" : "No", [&value]() { value = -1; });
	}
};
void draw_filter_label_bool(const std::string& label, bool& value) {
	if (value != false) {
		draw_filter_label(label, "Yes", [&value]() { value = false; });
	}
};

void gdxsv_replay_server_tab() {
	struct TextFilters {
		// Return 0 (pass) if the character is number
		static int FilterNumber(ImGuiInputTextCallbackData* data) {
			if (data->EventChar < 256 && strchr("0123456789", (char)data->EventChar)) return 0;
			return 1;
		}

		static int FullWidthAlphaNum(ImGuiInputTextCallbackData* data) {
			if (data->EventChar >= 0x21 && data->EventChar <= 0x7E) data->EventChar = (data->EventChar + 0xFEE0);

			return 0;
		}

		static int UppercaseAlpha(ImGuiInputTextCallbackData* data) {
			if (data->EventChar >= 0x21 && data->EventChar <= 0x7E) data->EventChar = toupper(data->EventChar);

			return 0;
		}
	};

	ImGui::AlignTextToFramePadding();

	ImGui::Text("Filter by");

	const std::array<std::string, 9> filter_labels{"User ID",	  "User Name", "Pilot Name", "Lobby ID",	 "No. of Players",
												   "Battle Code", "Ranking",   "Disk",		 "Reverse Order"};
	static unsigned int filter_selected = 0;

	ImGui::SameLine();
	ImGui::PushItemWidth(150.0f * scaling);
	if (ImGui::BeginCombo("##FilterItems", filter_labels[filter_selected].c_str(), ImGuiComboFlags_HeightLargest)) {
		for (u32 i = 0; i < filter_labels.size(); i++) {
			bool is_selected = i == filter_selected;
			if (ImGui::Selectable(filter_labels[i].c_str(), is_selected)) filter_selected = i;
			if (is_selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	{
		DisabledScope loading_scope(!future_is_ready(fetch_replay_entry_future_));

		switch (filter_selected) {
			case 0:	 // User ID
			{
				if (!fetch_user_entry_future_.valid()) {
					fetch_user_json();
				}
				static char user_id_buf[7] = {0};
				{
					DisabledScope loading_scope(!future_is_ready(fetch_user_entry_future_));
					ImGui::SameLine();
					if (ImGui::Button("My IDs")) ImGui::OpenPopup("my_id_popup");
					ImGui::SameLine();
					if (ImGui::BeginPopup("my_id_popup")) {
						ImGui::Text("Handle Name");
						ImGui::Separator();
						auto entries = fetch_user_entry_future_.get();
						for (int i = 0; i < entries.size(); i++)
							if (ImGui::Selectable(entries[i].name.c_str())) {
								snprintf(user_id_buf, sizeof(user_id_buf), "%s", entries[i].user_id.c_str());
							}
						ImGui::EndPopup();
					}
				}

				ImGui::SameLine();
				if (ImGui::InputText("##user_id_input", user_id_buf, IM_ARRAYSIZE(user_id_buf),
									 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter,
									 TextFilters::UppercaseAlpha)) {
					search_user_id = std::string(user_id_buf);
					fetch_new_results();
				}

				ImGui::SameLine();
				if (ImGui::Button("Add Filter")) {
					search_user_id = std::string(user_id_buf);
					fetch_new_results();
				}

				break;
			}
			case 1:	 // User Name
			{
				static char user_name_buf[100] = {0};
				ImGui::SameLine();
				if (ImGui::InputText("##user_name_input", user_name_buf, IM_ARRAYSIZE(user_name_buf),
									 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter,
									 TextFilters::FullWidthAlphaNum)) {
					search_user_name = std::string(user_name_buf);
					fetch_new_results();
				}

				ImGui::SameLine();
				if (ImGui::Button("Add Filter")) {
					search_user_name = std::string(user_name_buf);
					fetch_new_results();
				}

				break;
			}
			case 2:	 // Pilot Name
			{
				static char pilot_name_buf[100] = {0};
				ImGui::SameLine();
				if (ImGui::InputText("##user_name_input", pilot_name_buf, IM_ARRAYSIZE(pilot_name_buf),
									 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter,
									 TextFilters::FullWidthAlphaNum)) {
					search_pilot_name = std::string(pilot_name_buf);
					fetch_new_results();
				}

				ImGui::SameLine();
				if (ImGui::Button("Add Filter")) {
					search_pilot_name = std::string(pilot_name_buf);
					fetch_new_results();
				}

				break;
			}
			case 3:	 // Lobby ID
			{
				const static std::array<std::array<std::string, 4>, 17> lobby_data{
					{{u8"タクラマカン砂漠", u8"塔克拉瑪干沙漠", "Taklamakan Desert", "2"},
					 {u8"黒海南岸森林地帯", u8"黒海南岸森林地帶", "Black Sea Forest", "4"},
					 {u8"オデッサ", u8"奧迪沙", "Odessa", "5"},
					 {u8"ベルファスト", u8"貝爾法斯特", "Belfast", "6"},
					 {u8"ニューヤーク", u8"紐約", "New York", "9"},
					 {u8"グレートキャニオン", u8"大峽谷", "Grand Canyon", "10"},
					 {u8"ジャブロー", u8"査布羅", "Jaburo", "11"},
					 {u8"地下基地", u8"地下基地", "UG Complex", "12"},
					 {u8"ソロモン", u8"所羅門", "Solomon", "13"},
					 {u8"ソロモン宙域", u8"所羅門宙域", "Solomon (Space)", "14"},
					 {u8"ア・バオア・クー宙域", u8"阿・巴瓦・庫 宙域", "A Baoa Qu (Space)", "15"},
					 {u8"ア・バオア・クー外部", u8"阿・巴瓦・庫 外部", "A Baoa Qu (Outter)", "16"},
					 {u8"ア・バオア・クー内部", u8"阿・巴瓦・庫 内部", "A Baoa Qu (Inner)", "17"},
					 {u8"衛星軌道１", u8"衛星軌道１", "Sat.Orbit 1", "19"},
					 {u8"衛星軌道2", u8"衛星軌道２", "Sat.Orbit 2", "20"},
					 {u8"サイド６宙域", u8"SIDE 6 宙域", "SIDE 6 (Space)", "21"},
					 {u8"サイド７内部", u8"SIDE 7 内部", "SIDE 7 (Inner)", "22"}}};

				static unsigned int lobby_selected = 0;

				static int language = config::GdxLanguage.get();
				if (language < 0 || 3 <= language) {
					language = 0;
				}

				ImGui::SameLine();
				ImGui::PushItemWidth(300.0f * scaling);
				if (ImGui::BeginCombo("##LobbyItems", lobby_data[lobby_selected][language].c_str(), ImGuiComboFlags_HeightLargest)) {
					for (u32 i = 0; i < lobby_data.size(); i++) {
						bool is_selected = i == lobby_selected;
						if (ImGui::Selectable(lobby_data[i][language].c_str(), is_selected)) {
							lobby_selected = i;
							search_lobby_id = lobby_data[i][3];
							fetch_new_results();
						}
						if (is_selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();

				break;
			}
			case 4:	 // No of Players
			{
				ImGui::SameLine();
				static int no_of_players_input = 2;
				ImGui::SliderInt("##no_of_players_input", &no_of_players_input, 2, 4);

				ImGui::SameLine();
				if (ImGui::Button("Add Filter")) {
					search_no_of_players = no_of_players_input;
					fetch_new_results();
				}

				break;
			}
			case 5:	 // Battle Code
			{
				static char battle_code_buf[100] = {0};
				ImGui::SameLine();
				if (ImGui::InputText("##battle_code_input", battle_code_buf, IM_ARRAYSIZE(battle_code_buf),
									 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter,
									 TextFilters::FilterNumber)) {
					search_battle_code = std::string(battle_code_buf);
					fetch_new_results();
				}

				ImGui::SameLine();
				if (ImGui::Button("Add Filter")) {
					search_battle_code = std::string(battle_code_buf);
					fetch_new_results();
				}

				break;
			}
			case 6:	 // Ranking
			{
				ImGui::SameLine();
				if (ImGui::Button("Yes")) {
					search_ranking = 1;
					fetch_new_results();
				}
				ImGui::SameLine();
				if (ImGui::Button("No")) {
					search_ranking = 0;
					fetch_new_results();
				}
				break;
			}
			case 7:	 // Disk
			{
				ImGui::SameLine();
				if (ImGui::Button("MUJI")) {
					search_disk = "dc1";
					fetch_new_results();
				}
				ImGui::SameLine();
				if (ImGui::Button("DX")) {
					search_disk = "dc2";
					fetch_new_results();
				}
				break;
			}
			case 8:	 // Reverse
			{
				ImGui::SameLine();
				if (ImGui::Button("Add Filter")) {
					search_reverse = true;
					fetch_new_results();
				}
				break;
			}
			default:
				break;
		}

		ImGui::Dummy(ImVec2(0, 0));	 // Newline

		draw_filter_label_string("User ID", search_user_id);
		draw_filter_label_string("User Name", search_user_name);
		draw_filter_label_string("Pilot Name", search_pilot_name);
		draw_filter_label_string("Lobby ID", search_lobby_id);
		draw_filter_label_int("Players", search_no_of_players);
		draw_filter_label_string("Battle Code", search_battle_code);
		draw_filter_label_yesno("Ranking", search_ranking);
		draw_filter_label_string("Disk", search_disk);
		draw_filter_label_bool("Reverse", search_reverse);
	}

	ImGui::BeginChild(ImGui::GetID("gdxsv_replay_server_list_paging"), ScaledVec2(450, 0), false, ImGuiWindowFlags_NoDecoration);
	{
		ImGui::BeginChild(ImGui::GetID("gdxsv_replay_server_list"), ImVec2(0, ImGui::GetContentRegionAvail().y - 40.f * scaling), true,
						  ImGuiWindowFlags_DragScrolling);
		{
			if (!fetch_replay_entry_future_.valid()) {
				fetch_replay_json();
			} else if (!future_is_ready(fetch_replay_entry_future_)) {
				ImGui::Text("Loading...");
			} else {
				const auto& entries = fetch_replay_entry_future_.get();
				if (entries.size() == 0 || fetch_replay_entry_http_status == 204) {
					ImGui::Text("No result");
				} else if (fetch_replay_entry_http_status != 200) {
					ImGui::Text("Error: HTTP %d", fetch_replay_entry_http_status);
				} else {
					const static auto item_spacing = 30.0f;
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ScaledVec2(0, item_spacing));
					for (int i = 0; i < entries.size(); ++i) {
						ImGui::PushID(i);
						const auto& entry = entries[i];

						time_t t = entry.start_unix;
						char buf[128] = {0};
						std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

						snprintf(buf, sizeof(buf), u8"%s ― Result: %d：%d\n\n", buf, entry.renpo_win, entry.zeon_win);

						for (int i = 0; i < entry.users.size(); i++) {
							const auto& user = entry.users[i];

							if (i + 1 == entry.users.size()) {
								snprintf(buf, sizeof(buf), "%s%s", buf, user.user_name().c_str());
							} else {
								snprintf(buf, sizeof(buf), "%s%s%s ", buf, user.user_name().c_str(),
										 (entry.users[i + 1].team() != user.team() ? " vs" : ","));
							}
						}

						auto drawlist = ImGui::GetWindowDrawList();

						static auto SelectableColor = [drawlist](ImU32 color) {
							ImVec2 p_min = ImGui::GetItemRectMin();
							ImVec2 p_max = ImGui::GetItemRectMax();
							drawlist->AddRectFilled(p_min, p_max, color);
						};

						drawlist->ChannelsSplit(2);
						drawlist->ChannelsSetCurrent(1);

						if (ImGui::Selectable(buf, i == selected_replay_entry_index, 0, ImVec2(0, 0))) {
							selected_replay_entry_index = i;
							pov_index = -1;
						}
						if (i % 2 == 1) {
							drawlist->ChannelsSetCurrent(0);
							SelectableColor(IM_COL32(50, 50, 50, 100));
						}
						drawlist->ChannelsMerge();

						ImGui::PopID();
					}
					ImGui::PopStyleVar();
				}
			}
		}
		scrollWhenDraggingOnVoid();
		windowDragScroll();
		ImGui::EndChild();

		{
			DisabledScope loading_scope(!future_is_ready(fetch_replay_entry_future_));
			{
				{
					DisabledScope scope(entry_paging == 0);
					if (ImGui::Button("Prev Page") && !scope.isDisabled()) {
						entry_paging--;
						snprintf(page_buf, sizeof(page_buf), "%d", entry_paging + 1);
						fetch_target_page();
					}
				}
				{
					ImGui::SameLine();
					DisabledScope scope(future_is_ready(fetch_replay_entry_future_) && fetch_replay_entry_future_.get().size() < 100);
					if (ImGui::Button("Next Page")) {
						entry_paging++;
						snprintf(page_buf, sizeof(page_buf), "%d", entry_paging + 1);
						fetch_target_page();
					}

					ImGui::SameLine();
					ImGui::SetNextItemWidth(60.f * scaling);
					if (ImGui::InputText("##page_input", page_buf, IM_ARRAYSIZE(page_buf),
										 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCharFilter,
										 TextFilters::FilterNumber)) {
						entry_paging = atoi(page_buf) - 1;
						if (entry_paging < 0) entry_paging = 0;
						fetch_target_page();
					}
				}
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild(ImGui::GetID("gdxsv_replay_server_detail"), ImVec2(0, 0), true, ImGuiWindowFlags_DragScrolling);
	{
		if (selected_replay_entry_index != -1) {
			const auto& entries = fetch_replay_entry_future_.get();
			const auto& entry = entries[selected_replay_entry_index];
			std::string battle_code = entry.replay_url.substr(entry.replay_url.find_last_of("/") + 1);
			battle_code = battle_code.substr(0, battle_code.find(".pb"));

			gdxsv_replay_draw_info(battle_code, entry.disk, (int)entry.users.size(), "", entry.start_unix, 0, entry.users,
								   entry.replay_url);
		}
	}
	scrollWhenDraggingOnVoid();
	windowDragScroll();
	ImGui::EndChild();
}

}  // namespace

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
		if (gdxsv.StartReplayFile(replay_file.c_str(), pov)) {
			gui_state = GuiState::Closed;
		} else {
			dc_loadstate(90);
			broken_replay_path = replay_file;
		}
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
	if (ImGui::BeginTabBar("replays", ImGuiTabBarFlags_NoTooltip)) {
		if (ImGui::BeginTabItem("Local")) {
			gdxsv_replay_local_tab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Server")) {
			gdxsv_replay_server_tab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::PopStyleVar();

	ImGui::End();
	ImGui::PopStyleVar();  // ImGuiStyleVar_WindowRounding
}
