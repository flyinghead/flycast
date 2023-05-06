#pragma once

#include <future>
#include <string>
#include <vector>

class GdxsvUpdate {
   public:
	void Reset();
	bool IsUpdateAvailable();
	void FetchLatestVersionInfo();
	std::string GetLatestVersionTag() const;
	static bool IsSupportSelfUpdate();
	static std::string DownloadPageURL();
	std::shared_future<bool> StartSelfUpdate();
	float SelfUpdateProgress() const;

   private:
	struct LatestVersionInfo {
		bool is_new_version;
		std::string version;
		std::string version_tag;
		std::string download_url;
		size_t download_size = 0;
	};

	void HandleReleaseJSON(const std::string& json_string, LatestVersionInfo& out) const;
	static std::string GetFlycastFileNameWithVersion(const std::string& version);
	static std::string GetExecutablePath();
	static std::string GetTempDir();

	std::shared_future<LatestVersionInfo> fetch_latest_version_future_;
	std::vector<uint8_t> download_buf_;
};

extern GdxsvUpdate gdxsv_update;
