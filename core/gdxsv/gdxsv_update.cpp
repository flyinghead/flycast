#include "gdxsv_update.h"

#include <stdclass.h>
#include <types.h>
#include <zip.h>

#include <future>
#include <regex>
#include <string>

#include "json.hpp"
#include "rend/boxart/http_client.h"
#include "version.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
#include <libproc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
#include <libgen.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <unistd.h>
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

template <typename T>
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

	const auto future_fn = [this]() -> LatestVersionInfo {
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
	};

	fetch_latest_version_future_ = std::async(std::launch::async, future_fn).share();
}

bool GdxsvUpdate::IsSupportSelfUpdate() {
#if defined(_WIN32)
	return true;
#elif defined(__APPLE__) && !defined(TARGET_IPHONE)
	return true;
#else
	return false;
#endif
}

void GdxsvUpdate::HandleReleaseJSON(const std::string& json_string, LatestVersionInfo& out) {
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

std::string GdxsvUpdate::DownloadPageURL() { return "https://github.com/inada-s/flycast/releases/latest/"; }

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
#elif defined(__APPLE__) && defined(__MACH__)
	std::string path;
	char buffer[PROC_PIDPATHINFO_MAXSIZE];
	if (proc_pidpath(getpid(), buffer, sizeof(buffer)) > 0) {
		return std::string(buffer);
	}
	return "";
#else
	char result[PATH_MAX] = {};
	ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
	const char* path;
	if (count != -1) {
		return std::string(dirname(result));
	}
	return "";
#endif
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
#else
	return "/tmp";
#endif
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
		auto executable_path = GetExecutablePath();
		if (!file_exists(executable_path)) {
			ERROR_LOG(COMMON, "GetExecutablePath failure: %s", executable_path.c_str());
			return false;
		}

#if defined(__APPLE__)
		{
			// Fix Flycast.app/Contents/MacOS/Flycast to Flycast.app
			const auto pos = executable_path.find_last_of('.');
			if (pos == std::string::npos) {
				return false;
			}
			if (executable_path.substr(pos, 4) != ".app") {
				return false;
			}
			executable_path = executable_path.substr(0, pos + 4);
		}
#endif

		const auto& latest = fetch_latest_version_future_.get();
		if (get_file_name(latest.download_url) != ReleaseFileName) {
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
			ERROR_LOG(COMMON, "invalid size e:%ld a:%ld", latest.download_size, download_buf_.size());
			return false;
		}

		const auto tmp_dir = GetTempDir() + "/" + latest.version_tag;
		if (tmp_dir.empty()) {
			ERROR_LOG(COMMON, "GetTempDir failure");
			return false;
		}

		if (!file_exists(tmp_dir) && !make_directory(tmp_dir)) {
			ERROR_LOG(COMMON, "cannot access tmp_dir");
			return false;
		}

		const auto download_file_path = tmp_dir + "/" + ReleaseFileName;
		auto fp = nowide::fopen(download_file_path.c_str(), "wb");
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

		if (!ExtractZipFile(download_file_path, tmp_dir)) {
			ERROR_LOG(COMMON, "Extract zip failure");
			return false;
		}

		const auto executable_dir = executable_path.substr(0, get_last_slash_pos(executable_path));
		const auto new_version_path = executable_dir + "/" + GetFlycastFileNameWithVersion(latest.version);

		if (nowide::rename((tmp_dir + "/" + DefaultFlycastName).c_str(), new_version_path.c_str()) != 0) {
			ERROR_LOG(COMMON, "failed to move latest version");
			return false;
		}

#if defined(__APPLE__)
		// Move macOS current executable to Trash, rename new version to original
		auto current_version = std::string(GIT_VERSION).erase(0, 6);
		auto trash_path = std::string(getenv("HOME")) + "/.Trash/" + GetFlycastFileNameWithVersion(current_version);
		
		if (nowide::rename(executable_path.c_str(), trash_path.c_str()) == 0) {
			nowide::rename(new_version_path.c_str(), executable_path.c_str());
		}
#else
		if (get_file_name(executable_path) == DefaultFlycastName) {
			// overwrite the binary
			const auto old_version_path = executable_dir + "/" + GetFlycastFileNameWithVersion("old");
			if (file_exists(old_version_path)) {
				nowide::remove(old_version_path.c_str());
			}

			nowide::rename(executable_path.c_str(), old_version_path.c_str());
			nowide::rename(new_version_path.c_str(), executable_path.c_str());
		}
#endif

		return true;
	};

	return std::async(std::launch::async, update_fn).share();
}

bool GdxsvUpdate::ExtractZipFile(const std::string& zip_path, const std::string& dst_path) {
	auto zip_file = nowide::fopen(zip_path.c_str(), "rb");
	if (zip_file == nullptr) {
		ERROR_LOG(COMMON, "ExtractZipFile: fopen zip_path failure");
		return false;
	}

	zip_error_t error;
	zip_source_t* source = zip_source_filep_create(zip_file, 0, -1, &error);
	if (source == nullptr) {
		ERROR_LOG(COMMON, "ExtractZipFile: zip_source_filep_create failure: %s", error.str);
		std::fclose(zip_file);
		return false;
	}

	auto zip = zip_open_from_source(source, ZIP_RDONLY, &error);
	if (zip == nullptr) {
		ERROR_LOG(COMMON, "ExtractZipFile: zip_open_from_source failure: %s", error.str);
		std::fclose(zip_file);
		zip_source_free(source);
		return false;
	}

	bool result = true;
	auto num_entries = zip_get_num_entries(zip, 0);
	if (num_entries < 0) {
		ERROR_LOG(COMMON, "ExtractZipFile: zip_get_num_entries failure");
		result = false;
	}

	for (int i = 0; i < num_entries && result; i++) {
		std::string name(zip_get_name(zip, i, 0));	// NOTE: need nowide?
		if (name.empty()) {
			ERROR_LOG(COMMON, "ExtractZipFile: zip_get_name");
			result = false;
			break;
		}

		zip_uint8_t opsys = 0;
		zip_uint32_t attributes = 0;
		zip_file_get_external_attributes(zip, i, 0, &opsys, &attributes);

		if (name.back() == '/') {
			name.pop_back();
			const auto dir = dst_path + "/" + name;
			if (!file_exists(dir) && !make_directory(dir.c_str())) {
				ERROR_LOG(COMMON, "ExtractZipFile: make_directory failure: %s", name.c_str());
				result = false;
				break;
			}
			continue;
		}

		constexpr uint32_t FA_IFREG = 0100000;
		constexpr uint32_t FA_IFLNK = 0120000;
		FILE* wfp = nullptr;
		zip_file_t* zfp = nullptr;

		do {
			zfp = zip_fopen_index(zip, i, 0);
			if (zfp == nullptr) {
				ERROR_LOG(COMMON, "ExtractZipFile: zip_fopen_index failure");
				result = false;
				break;
			}

			char buf[4096];
			zip_int64_t read_bytes = 0;

			if (opsys == ZIP_OPSYS_UNIX && ((attributes >> 16) & FA_IFLNK) == FA_IFLNK) {
#if defined(__APPLE__) && !defined(TARGET_IPHONE) || defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
				read_bytes = zip_fread(zfp, buf, sizeof(buf));
				buf[read_bytes] = '\0';
				auto dst = dst_path + "/" + name;
				if (file_exists(dst)) {
					nowide::remove(dst.c_str());
				}
				if (symlink(buf, dst.c_str()) != 0) {
					ERROR_LOG(COMMON, "ExtractZipFile: symlink failure: %d", errno);
					result = false;
					break;
				}
#else
				ERROR_LOG(COMMON, "ExtractZipFile: symlink is not supported");
				result = false;
				break;
#endif
				break;
			}

			wfp = nowide::fopen((dst_path + "/" + name).c_str(), "wb");
			if (wfp == nullptr) {
				ERROR_LOG(COMMON, "ExtractZipFile: nowide::fopen failure");
				result = false;
				break;
			}

			while (0 < (read_bytes = zip_fread(zfp, buf, sizeof(buf)))) {
				std::fwrite(buf, 1, read_bytes, wfp);
			}

			if (opsys == ZIP_OPSYS_UNIX && ((attributes >> 16) & FA_IFREG) == FA_IFREG) {
#if defined(__APPLE__) && !defined(TARGET_IPHONE) || defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
				fchmod(fileno(wfp), attributes >> 16);
#endif
			}
		} while (0);
		if (wfp != nullptr) fclose(wfp);
		if (zfp != nullptr) zip_fclose(zfp);
	}

	zip_close(zip);
	std::fclose(zip_file);
	return result;
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
