#include "gdxsv_update.h"

#include <stdclass.h>
#include <types.h>
#include <future>
#include <regex>
#include <string>

#include "archive/ZipArchive.h"
#include "json.hpp"
#include "rend/boxart/http_client.h"
#include "version.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

static constexpr size_t MaxDownloadSize = 30 * 1024 * 1024;

static const std::string ReleaseFileName =
#if HOST_CPU == CPU_X64
#if defined(_WIN32)
"flycast-gdxsv-windows-msvc.zip";
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
"flycast-gdxsv-macos-x86_64.zip";
#elif defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
"flycast-gdxsv-linux-x86_64.zip";
#else
"";
#endif
#else
"";
#endif

static const std::string DefaultFlycastName =
#if defined(_WIN32)
"flycast-gdxsv.exe";
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
"Flycast-gdxsv.app";
#elif defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
"flycast-gdxsv";
#else
"";
#endif

template<typename T>
static bool future_is_ready(const T& future) {
	return future.valid() && future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

void GdxsvUpdate::Reset() {
	fetch_latest_version_future_ = {};
	download_buf_.clear();
	download_buf_.shrink_to_fit();
}

bool GdxsvUpdate::IsUpdateAvailable() {
	if (!fetch_latest_version_future_.valid()) {
		FetchLatestVersionInfo();
		return false;
	}

	if (!future_is_ready(fetch_latest_version_future_)) {
		return false;
	}

	const auto& v = fetch_latest_version_future_.get();
	return v.is_new_version && !v.download_url.empty();
}

std::string GdxsvUpdate::GetLatestVersionTag() const {
	verify(future_is_ready(fetch_latest_version_future_));
	const auto& v = fetch_latest_version_future_.get();
	return v.version_tag;
}

void GdxsvUpdate::FetchLatestVersionInfo() {
	if (fetch_latest_version_future_.valid()) {
		return;
	}

	fetch_latest_version_future_ = std::async(std::launch::async, [this]() -> LatestVersionInfo {
		LatestVersionInfo latest{};
		std::vector<u8> dl;
		std::string content_type;
		http::init();
		const std::string url = "https://api.github.com/repos/inada-s/flycast/releases/latest";
		const int rc = http::get(url, dl, content_type);
		if (rc != 200) {
			ERROR_LOG(COMMON, "version check failure: %s", url.c_str());
			return latest;
		}

		HandleReleaseJSON(std::string(dl.begin(), dl.end()), latest);
		return latest;
		}).share();
}

bool GdxsvUpdate::IsSupportSelfUpdate() {
#if defined(_WIN32)
	return true;
#else
	return false;
#endif
}


void GdxsvUpdate::HandleReleaseJSON(const std::string& json_string,  LatestVersionInfo& out) const {
	const std::regex tag_name_regex(R"|#|("tag_name":"(.*?)")|#|");
	const std::string version_prefix = "gdxsv-";
	const std::regex semver_regex(R"|#|(^([0-9]+)\.([0-9]+)\.([0-9]+)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+)?$)|#|");

	std::string latest_tag_name;
	std::string latest_download_url;
	size_t latest_download_size = 0;

	try {
		nlohmann::json v = nlohmann::json::parse(json_string);
		latest_tag_name = v.at("tag_name");
		for (auto e : v.at("assets")) {
			std::string name = e.at("name");
			if (name == ReleaseFileName) {
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

	out.version = latest_version_str;
	out.version_tag = latest_tag_name;
	out.download_url = latest_download_url;
	out.download_size = latest_download_size;
	out.is_new_version = current_version < latest_version;
}

std::string GdxsvUpdate::DownloadPageURL() {
	return "https://github.com/inada-s/flycast/releases/latest/";
}

std::string GdxsvUpdate::GetFlycastFileNameWithVersion(const std::string& version) {
	const auto dot_pos = DefaultFlycastName.find_last_of('.');
	if (dot_pos == std::string::npos) {
		// No extension
		return DefaultFlycastName + "-" + version;
	}

	const auto base_name = DefaultFlycastName.substr(0, dot_pos);
	const auto ext = DefaultFlycastName.substr(dot_pos);
	return base_name + "-" + version + ext;
}

std::string GdxsvUpdate::GetExecutablePath() {
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

std::string GdxsvUpdate::GetTempDir() {
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

static std::string get_file_name(const std::string& file_path) {
	const auto pos = get_last_slash_pos(file_path);
	if (pos == std::string::npos) {
		return "";
	}

	return file_path.substr(pos + 1);
}

std::shared_future<bool> GdxsvUpdate::StartSelfUpdate() {
	verify(future_is_ready(fetch_latest_version_future_));

	auto update_fn = [this]() -> bool {
		const auto& latest = fetch_latest_version_future_.get();
		const auto executable_path = GetExecutablePath();
		if (!file_exists(executable_path)) {
			ERROR_LOG(COMMON, "get_executable_path failure: %s", executable_path.c_str());
			return false;
		}

		const auto download_file_name = get_file_name(latest.download_url);
		if (download_file_name != ReleaseFileName) {
			ERROR_LOG(COMMON, "unexcpected download file");
			return false;
		}

		if (MaxDownloadSize < latest.download_size) {
			ERROR_LOG(COMMON, "latest version is too big");
			return false;
		}

		std::string content_type;
		download_buf_.clear();
		download_buf_.reserve(latest.download_size);
		http::init();
		const int rc = http::get(latest.download_url, download_buf_, content_type);

		if (rc != 200) {
			ERROR_LOG(COMMON, "download failure: %s", latest.download_url.c_str());
			return false;
		}

		if (download_buf_.size() != latest.download_size) {
			ERROR_LOG(COMMON, "invalid size e:%d a:%d", latest.download_size, download_buf_.size());
			return false;
		}

		const auto download_file_path = GetTempDir() + "/" + download_file_name;
		FILE* fp = nowide::fopen(download_file_path.c_str(), "wb");
		if (fp == nullptr) {
			ERROR_LOG(COMMON, "fopen failure: %s", download_file_path.c_str());
			return false;
		}

		const auto written = std::fwrite(download_buf_.data(), 1, download_buf_.size(), fp);
		std::fclose(fp);
		if (written != download_buf_.size()) {
			ERROR_LOG(COMMON, "fwrite failure");
			return false;
		}

		download_buf_.clear();
		download_buf_.shrink_to_fit();

		const auto executable_file_name = get_file_name(executable_path);
		const auto executable_dir = executable_path.substr(0, get_last_slash_pos(executable_path));
		const auto new_version_path = executable_dir + "/" + GetFlycastFileNameWithVersion(latest.version);

		fp = nowide::fopen(new_version_path.c_str(), "wb");
		if (fp == nullptr) {
			ERROR_LOG(COMMON, "fopen failure: %s", new_version_path.c_str());
			return false;
		}

		std::unique_ptr<Archive> archive(OpenArchive(download_file_path.c_str()));
		if (archive == nullptr) {
			ERROR_LOG(COMMON, "OpenArchive failure: %s", download_file_path.c_str());
		}

		const auto zip = archive->OpenFile(DefaultFlycastName.c_str());	 // TODO: will now work on APPLE
		if (zip == nullptr) {
			ERROR_LOG(COMMON, "zip OpenFile failure: %s", download_file_path.c_str());
			std::fclose(fp);
			return false;
		}

		bool unzip_ok = false;
		while (true) {
			u8 buffer[4096];
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

		if (executable_file_name == DefaultFlycastName) {
			// overwrite the binary
			const auto old_version_path = executable_dir + "/" + GetFlycastFileNameWithVersion("old");
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

float GdxsvUpdate::SelfUpdateProgress() const {
	verify(future_is_ready(fetch_latest_version_future_));

	const auto& latest = fetch_latest_version_future_.get();

	if (latest.download_size == 0) {
		return 0;
	}

	return static_cast<float>(download_buf_.size()) / latest.download_size;
}

GdxsvUpdate gdxsv_update;
