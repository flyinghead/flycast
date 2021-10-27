/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "oslib.h"
#include "stdclass.h"
#include "cfg/cfg.h"
#include "cfg/option.h"

namespace hostfs
{

std::string getVmuPath(const std::string& port)
{
	char tempy[512];
	sprintf(tempy, "vmu_save_%s.bin", port.c_str());
	// VMU saves used to be stored in .reicast, not in .reicast/data
	std::string apath = get_writable_config_path(tempy);
	if (!file_exists(apath))
		apath = get_writable_data_path(tempy);
	return apath;
}

std::string getArcadeFlashPath()
{
	std::string nvmemSuffix = cfgLoadStr("net", "nvmem", "");
	return get_game_save_prefix() + nvmemSuffix;
}

std::string findFlash(const std::string& prefix, const std::string& names)
{
	const size_t npos = std::string::npos;
	size_t start = 0;
	while (start < names.size())
	{
		size_t semicolon = names.find(';', start);
		std::string name = names.substr(start, semicolon == npos ? semicolon : semicolon - start);

		size_t percent = name.find('%');
		if (percent != npos)
			name = name.replace(percent, 1, prefix);

		std::string fullpath = get_readonly_data_path(name);
		if (file_exists(fullpath))
			return fullpath;
		for (const auto& path : config::ContentPath.get())
		{
			fullpath = path + "/" + name;
			if (file_exists(fullpath))
				return fullpath;
		}

		start = semicolon;
		if (start != npos)
			start++;
	}
	return "";

}

std::string getFlashSavePath(const std::string& prefix, const std::string& name)
{
	return get_writable_data_path(prefix + name);
}

std::string findNaomiBios(const std::string& name)
{
	std::string fullpath = get_readonly_data_path(name);
	if (file_exists(fullpath))
		return fullpath;
	for (const auto& path : config::ContentPath.get())
	{
		fullpath = path + "/" + name;
		if (file_exists(fullpath))
			return fullpath;
	}
	return "";
}

std::string getSavestatePath(int index, bool writable)
{
	std::string state_file = settings.content.path;
	size_t lastindex = state_file.find_last_of('/');
#ifdef _WIN32
	size_t lastindex2 = state_file.find_last_of('\\');
	if (lastindex == std::string::npos)
		lastindex = lastindex2;
	else if (lastindex2 != std::string::npos)
		lastindex = std::max(lastindex, lastindex2);
#endif
	if (lastindex != std::string::npos)
		state_file = state_file.substr(lastindex + 1);
	lastindex = state_file.find_last_of('.');
	if (lastindex != std::string::npos)
		state_file = state_file.substr(0, lastindex);

	char index_str[4] = "";
	if (index > 0) // When index is 0, use same name before multiple states is added
		sprintf(index_str, "_%d", std::min(99, index));

	state_file = state_file + index_str + ".state";
	if (index == -1)
		state_file += ".net";
	if (writable)
		return get_writable_data_path(state_file);
	else
		return get_readonly_data_path(state_file);
}

std::string getVulkanCachePath()
{
	return get_writable_data_path("vulkan_pipeline.cache");
}

std::string getTextureLoadPath(const std::string& gameId)
{
	if (gameId.length() > 0)
		return get_readonly_data_path("textures/" + gameId) + "/";
	else
		return "";
}

std::string getTextureDumpPath()
{
	return get_writable_data_path("texdump/");
}

std::string getBiosFontPath()
{
	return get_readonly_data_path("font.bin");
}

}
