#pragma once

#include <future>
#include <string>

extern bool gdxsv_update_available;
extern std::string gdxsv_latest_version_tag;
extern std::string gdxsv_latest_version_download_url;

void gdxsv_latest_version_check();

bool gdxsv_self_update_support();

void gdxsv_open_download_page();

std::string gdxsv_release_file_name();

std::shared_future<bool> gdxsv_self_update(const std::string& download_url);

float gdxsv_self_update_progress();
