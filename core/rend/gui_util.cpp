/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "gui_util.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#ifdef _MSC_VER
#include <io.h>
#define access _access
#define R_OK   4
#else
#include <unistd.h>
#endif
#include <dirent.h>
#include <sys/stat.h>

#include "types.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

extern int screen_width, screen_height;

static std::string select_current_directory;
static std::vector<std::string> select_subfolders;
static std::vector<std::string> display_files;
bool subfolders_read;
#ifdef _WIN32
static const std::string separators = "/\\";
static const std::string native_separator = "\\";
#else
static const std::string separators = "/";
static const std::string native_separator = "/";
#endif
#define PSEUDO_ROOT ":"

void select_directory_popup(const char *prompt, float scaling, StringCallback callback)
{
	if (select_current_directory.empty())
	{
#if defined(__ANDROID__)
		const char *home = getenv("REICAST_HOME");
		if (home != NULL)
		{
			const char *pcolon = strchr(home, ':');
			if (pcolon != NULL)
				select_current_directory = std::string(home, pcolon - home);
			else
				select_current_directory = home;
		}
#elif HOST_OS == OS_LINUX || defined(__APPLE__)
		const char *home = getenv("HOME");
		if (home != NULL)
			select_current_directory = home;
#elif defined(_WIN32)
		const char *home = getenv("HOMEPATH");
		const char *home_drive = getenv("HOMEDRIVE");
		if (home != NULL)
		{
			if (home_drive != NULL)
				select_current_directory = home_drive;
			else
				select_current_directory.clear();
			select_current_directory += home;
		}
#endif
		if (select_current_directory.empty())
		{
			select_current_directory = get_writable_config_path("");
			if (select_current_directory.empty())
				select_current_directory = ".";
		}
	}

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	if (ImGui::BeginPopupModal(prompt, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize ))
	{
		std::string path = select_current_directory;
		std::string::size_type last_sep = path.find_last_of(separators);
		if (last_sep == path.size() - 1)
			path.pop_back();

		static std::string error_message;

		if (!subfolders_read)
		{
			select_subfolders.clear();
            display_files.clear();
			error_message.clear();
#ifdef _WIN32
			if (select_current_directory == PSEUDO_ROOT)
			{
				error_message = "Drives";
				// List all the drives
				u32 drives = GetLogicalDrives();
				for (int i = 0; i < 32; i++)
					if ((drives & (1 << i)) != 0)
						select_subfolders.push_back(std::string(1, (char)('A' + i)) + ":\\");
			}
			else
#elif __ANDROID__
			if (select_current_directory == PSEUDO_ROOT)
			{
				error_message = "Storage Locations";
				const char *home = getenv("REICAST_HOME");
				while (home != NULL)
				{
					const char *pcolon = strchr(home, ':');
					if (pcolon != NULL)
					{
						select_subfolders.push_back(std::string(home, pcolon - home));
						home = pcolon + 1;
					}
					else
					{
						select_subfolders.push_back(home);
						home = NULL;
					}
				}
			}
			else
#endif
			{
				DIR *dir = opendir(select_current_directory.c_str());
				if (dir == NULL)
				{
					error_message = "Cannot read " + select_current_directory;
					select_subfolders.push_back("..");
				}
				else
				{
					bool dotdot_seen = false;
					while (true)
					{
						struct dirent *entry = readdir(dir);
						if (entry == NULL)
							break;
						std::string name(entry->d_name);
						if (name == ".")
							continue;
						std::string child_path = path + "/" + name;
						bool is_dir = false;
#ifndef _WIN32
						if (entry->d_type == DT_DIR)
							is_dir = true;
						if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
#endif
						{
							struct stat st;
							if (stat(child_path.c_str(), &st) != 0)
								continue;
							if (S_ISDIR(st.st_mode))
								is_dir = true;
						}
						if (is_dir && access(child_path.c_str(), R_OK) == 0)
						{
							if (name == "..")
								dotdot_seen = true;
							select_subfolders.push_back(name);
						}
                        else
                        {
                            std::string extension = get_file_extension(name);
                            if ( extension == "zip" || extension == "7z" || extension == "chd" || extension == "gdi" || ((settings.dreamcast.HideLegacyNaomiRoms
                                    || (extension != "bin" && extension != "lst" && extension != "dat"))
                            && extension != "cdi" && extension != "cue") == false )
                                display_files.push_back(name);
                        }
					}
					closedir(dir);
#if defined(_WIN32) || defined(__ANDROID__)
					if (!dotdot_seen)
						select_subfolders.push_back("..");
#endif
				}
			}

			std::stable_sort(select_subfolders.begin(), select_subfolders.end());
			subfolders_read = true;
		}

		ImGui::Text("%s", error_message.empty() ? select_current_directory.c_str() : error_message.c_str());
		ImGui::BeginChild(ImGui::GetID("dir_list"), ImVec2(0, - 30 * scaling - ImGui::GetStyle().ItemSpacing.y), true);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scaling, 20 * scaling));		// from 8, 4


		for (auto& name : select_subfolders)
		{
			std::string child_path;
			if (name == "..")
			{
				std::string::size_type last_sep = path.find_last_of(separators);
				if (last_sep == std::string::npos)
				{
					if (path.empty())
						// Root folder
						continue;
#ifdef _WIN32
					if (path.size() == 2 && path[1] == ':')
						child_path = PSEUDO_ROOT;
					else
#endif
					if (path == ".")
						child_path = "..";
					else if (path == "..")
						child_path = ".." + native_separator + "..";
					else
						child_path = ".";
				}
				else if (last_sep == 0)
					child_path = native_separator;
				else if (path.size() >= 2 && path.substr(path.size() - 2) == "..")
					child_path = path + native_separator + "..";
				else
				{
					child_path = path.substr(0, last_sep);
#ifdef _WIN32
					if (child_path.size() == 2 && child_path[1] == ':')		// C: -> C:/
						child_path += native_separator;
#endif
				}
#ifdef __ANDROID__
				if (access(child_path.c_str(), R_OK) != 0)
					child_path = PSEUDO_ROOT;
#endif
			}
			else
			{
#if defined(_WIN32) || defined(__ANDROID__)
				if (path == PSEUDO_ROOT)
					child_path = name;
				else
#endif
					child_path = path + native_separator + name;
			}
			if (ImGui::Selectable(name.c_str()))
			{
				subfolders_read = false;
				select_current_directory = child_path;
			}
		}
        ImGui::PushStyleColor(ImGuiCol_Text, { 1, 1, 1, 0.3f });
        for (auto& name : display_files)
        {
            ImGui::Text("%s", name.c_str());
        }
        ImGui::PopStyleColor();
        
		ImGui::PopStyleVar();
		ImGui::EndChild();
		if (ImGui::Button("Select Current Directory", ImVec2(0, 30 * scaling)))
		{
			subfolders_read = false;
			callback(false, select_current_directory);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(0, 30 * scaling)))
		{
			subfolders_read = false;
			callback(true, "");
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

// See https://github.com/ocornut/imgui/issues/3379
void ScrollWhenDraggingOnVoid(const ImVec2& delta, ImGuiMouseButton mouse_button)
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = g.CurrentWindow;
    bool hovered = false;
    bool held = false;
    ImGuiButtonFlags button_flags = (mouse_button == 0) ? ImGuiButtonFlags_MouseButtonLeft : (mouse_button == 1) ? ImGuiButtonFlags_MouseButtonRight : ImGuiButtonFlags_MouseButtonMiddle;
    if (g.HoveredId == 0) // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
        ImGui::ButtonBehavior(window->Rect(), window->GetID("##scrolldraggingoverlay"), &hovered, &held, button_flags);
    if (held && delta.x != 0.0f)
        ImGui::SetScrollX(window, window->Scroll.x + delta.x);
    if (held && delta.y != 0.0f)
        ImGui::SetScrollY(window, window->Scroll.y + delta.y);
}
