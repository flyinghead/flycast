#include <stdclass.h>
#include <types.h>

#include <future>
#include <regex>
#include <string>

#include "archive/ZipArchive.h"
#include "json.hpp"
#include "rend/boxart/http_client.h"
#include "version.h"
#include "oslib/oslib.h"

#if defined(_WIN32)
// #include <libloaderapi.h>
#include <Windows.h>
#endif

constexpr size_t MAX_DOWNLOAD_SIZE = 30 * 1024 * 1024;

bool gdxsv_update_available = false;
std::string gdxsv_current_version;
std::string gdxsv_latest_version;
std::string gdxsv_latest_version_tag;
std::string gdxsv_latest_version_download_url;
size_t gdxsv_latest_version_download_size;

static std::vector<u8> downloaded;
static void gdxsv_handle_release_json(const std::string& json_string);

#if defined(WIN32)
const std::string default_flycast_name = "flycast-gdxsv.exe";
#elif defined(__APPLE__)
const std::string default_flycast_name = "Flycast-gdxsv.app";
#else
const std::string default_flycast_name = "flycast-gdxsv";
#endif

void gdxsv_latest_version_check() {
	static std::once_flag once;
	std::call_once(once, [] {
		std::thread([]() {
			downloaded.clear();
			std::string content_type;
			http::init();
			std::string url = "https://api.github.com/repos/inada-s/flycast/releases/latest";
			int rc = http::get(url, downloaded, content_type);
			if (rc != 200) {
				ERROR_LOG(COMMON, "version check failure: %s", url.c_str());
				return;
			}
			gdxsv_handle_release_json(std::string(downloaded.begin(), downloaded.end()));
		}).detach();
	});
}

static void gdxsv_handle_release_json(const std::string& json_string) {
	const std::regex tag_name_regex(R"|#|("tag_name":"(.*?)")|#|");
	const std::string version_prefix = "gdxsv-";
	const std::regex semver_regex(R"|#|(^([0-9]+)\.([0-9]+)\.([0-9]+)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+)?$)|#|");

	std::string latest_tag_name;
	std::string latest_download_url;
	size_t latest_download_size;
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
		nlohmann::json v = nlohmann::json::parse(json_string);
		latest_tag_name = v.at("tag_name");
		for (auto e : v.at("assets")) {
			std::string name = e.at("name");
			if (name == expected_name) {
				latest_download_url = e.at("browser_download_url");
				latest_download_size = e.at("size");
			}
		}
	} catch (const nlohmann::json::exception& e) {
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

	gdxsv_current_version = current_version_str;
	gdxsv_latest_version_tag = latest_tag_name;
	gdxsv_latest_version = latest_version_str;
	gdxsv_latest_version_download_url = latest_download_url;
	gdxsv_latest_version_download_size = latest_download_size;

	if (current_version < latest_version) {
		gdxsv_update_available = true;
	}
}

bool gdxsv_self_update_support() {
#if defined(_WIN32)
	return true;
#else
	return false;
#endif
}

void gdxsv_open_download_page() {
	os_LaunchFromURL("https://github.com/inada-s/flycast/releases/latest/");
}

std::string gdxsv_release_file_name() {
#if HOST_CPU == CPU_X64
#if defined(_WIN32)
	return "flycast-gdxsv-windows-msvc.zip";
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
	return "flycast-gdxsv-macos-x86_64.zip";
#elif defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
	return "flycast-gdxsv-linux-x86_64.zip";
#endif
#endif
	return "";
}

static std::string get_flycast_filename_with_version(const std::string& version) {
	auto dot_pos = default_flycast_name.find_last_of('.');
	if (dot_pos == std::string::npos) {
		return default_flycast_name + "-" + version;
	}

	const auto base_name = default_flycast_name.substr(0, dot_pos);
	const auto ext = default_flycast_name.substr(dot_pos);
	return base_name + "-" + version + ext;
}

static std::string get_executable_path() {
#if defined(_WIN32)
	wchar_t module_file_name[_MAX_PATH + 1];
	GetModuleFileNameW(NULL, module_file_name, _MAX_PATH);
	nowide::stackstring nws;
	if (nws.convert(module_file_name)) {
		return nws.c_str();
	}
	return "";
#endif
	// TODO: Other platform
	return "";
}

static std::string get_file_name(const std::string& file_path) {
	const auto pos = get_last_slash_pos(file_path);
	if (pos == std::string::npos) {
		return "";
	}

	return file_path.substr(pos + 1);
}

static std::string get_temp_path() {
#if defined(_WIN32)
	wchar_t temp_path[_MAX_PATH + 1];
	GetTempPathW(_MAX_PATH, temp_path);
	nowide::stackstring nws;
	if (!nws.convert(temp_path)) return "";
	auto result = std::string(nws.c_str());
	if (result.back() == '/' || result.back() == '\\') result.pop_back();
	return result;
#endif
	// TODO: Other platform
	return "";
}

std::shared_future<bool> gdxsv_self_update(const std::string& download_url) {
	auto update_fn = [download_url]() -> bool {
		const auto executable_path = get_executable_path();
		if (!file_exists(executable_path)) {
			ERROR_LOG(COMMON, "get_executable_path failure: %s", executable_path.c_str());
			return false;
		}

		const auto download_file_name = get_file_name(download_url);
		if (download_file_name != gdxsv_release_file_name()) {
			ERROR_LOG(COMMON, "unexcpected download file");
			return false;
		}

		if (MAX_DOWNLOAD_SIZE < gdxsv_latest_version_download_size) {
			ERROR_LOG(COMMON, "latest version is too big");
			return false;
		}

		std::string content_type;
		downloaded.clear();
		downloaded.reserve(gdxsv_latest_version_download_size);
		http::init();
		int rc = http::get(download_url, downloaded, content_type);
		if (rc != 200) {
			ERROR_LOG(COMMON, "download failure: %s", download_url.c_str());
			return false;
		}

		if (downloaded.size() != gdxsv_latest_version_download_size) {
			ERROR_LOG(COMMON, "invalid size e:%d a:%d", gdxsv_latest_version_download_size, downloaded.size());
			return false;
		}

		const auto download_file_path = get_temp_path() + "/" + download_file_name;
		FILE* fp = nowide::fopen(download_file_path.c_str(), "wb");
		if (fp == nullptr) {
			ERROR_LOG(COMMON, "fopen failure: %s", download_file_path.c_str());
			return false;
		}

		const auto written = std::fwrite(downloaded.data(), 1, downloaded.size(), fp);
		std::fclose(fp);
		if (written != downloaded.size()) {
			ERROR_LOG(COMMON, "fwrite failure");
			return false;
		}

		downloaded.clear();
		downloaded.shrink_to_fit();

		const auto executable_file_name = get_file_name(executable_path);
		const auto executable_dir = executable_path.substr(0, get_last_slash_pos(executable_path));
		const auto new_version_path = executable_dir + "/" + get_flycast_filename_with_version(gdxsv_latest_version);

		fp = nowide::fopen(new_version_path.c_str(), "wb");
		if (fp == nullptr) {
			ERROR_LOG(COMMON, "fopen failure: %s", new_version_path.c_str());
			return false;
		}

		std::unique_ptr<Archive> archive(OpenArchive(download_file_path.c_str()));
		if (archive == nullptr) {
			ERROR_LOG(COMMON, "OpenArchive failure: %s", download_file_path.c_str());
		}

		auto zip = archive->OpenFile(default_flycast_name.c_str());	 // TODO: will now work on APPLE
		if (zip == nullptr) {
			ERROR_LOG(COMMON, "zip OpenFile failure: %s", download_file_path.c_str());
			std::fclose(fp);
			return false;
		}

		bool unzip_ok = false;
		u8 buffer[4096];
		while (true) {
			const auto read = zip->Read(buffer, sizeof(buffer));
			if (read == 0) {
				unzip_ok = true;
				break;
			}

			if (sizeof(buffer) < read) {
				ERROR_LOG(COMMON, "zip Read failure");
				break;
			}

			if (fwrite(buffer, 1, read, fp) != read) {
				ERROR_LOG(COMMON, "fwrite failure");
				break;
			}
		}
		std::fclose(fp);

		if (!unzip_ok) {
			nowide::remove(new_version_path.c_str());
			return false;
		}

		if (executable_file_name == default_flycast_name) {
			// overwrite the binary
			const auto old_version_path = executable_dir + "/" + get_flycast_filename_with_version("old");
			if (file_exists(old_version_path)) {
				nowide::remove(old_version_path.c_str());
			}

			nowide::rename(executable_path.c_str(), old_version_path.c_str());
			nowide::rename(new_version_path.c_str(), executable_path.c_str());
		}

		return true;
	};

	return std::async(std::launch::async, update_fn).share();
}

float gdxsv_self_update_progress() {
	if (gdxsv_latest_version_download_size == 0) return 0;
	return float(downloaded.size()) / gdxsv_latest_version_download_size;
}
