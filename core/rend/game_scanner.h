/*
	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <dirent.h>
#include <sys/stat.h>

#include "types.h"
#include "stdclass.h"
#include "hw/naomi/naomi_roms.h"

struct GameMedia {
	std::string name;
	std::string path;
};

static bool operator<(const GameMedia &left, const GameMedia &right)
{
	return left.name < right.name;
}

class GameScanner
{
	std::vector<GameMedia> game_list;
	std::mutex mutex;
	std::unique_ptr<std::thread> scan_thread;
	bool scan_done = false;
	bool running = false;
	std::unordered_map<std::string, const Game*> arcade_games;
	std::unordered_set<std::string> arcade_gdroms;

	void insert_game(const GameMedia& game)
	{
		std::lock_guard<std::mutex> guard(mutex);
		game_list.insert(std::upper_bound(game_list.begin(), game_list.end(), game), game);
	}

	void add_game_directory(const std::string& path)
	{
		//printf("Exploring %s\n", path.c_str());
        if (game_list.size() == 0)
        {
            ++empty_folders_scanned;
            if (empty_folders_scanned > 1000)
                content_path_looks_incorrect = true;
        }
        else
        {
            content_path_looks_incorrect = false;
        }
        
		DIR *dir = opendir(path.c_str());
		if (dir == NULL)
			return;
		while (running)
		{
			struct dirent *entry = readdir(dir);
			if (entry == NULL)
				break;
			std::string name(entry->d_name);
			if (name == "." || name == "..")
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
			if (is_dir)
			{
				add_game_directory(child_path);
			}
			else
			{
				std::string extension = get_file_extension(name);
				if (extension == "zip" || extension == "7z")
				{
					std::string basename = get_file_basename(name);
					string_tolower(basename);
					auto it = arcade_games.find(basename);
					if (it == arcade_games.end())
						continue;
					name = name + " (" + std::string(it->second->description) + ")";
				}
				else if (extension == "chd" || extension == "gdi")
				{
					// Hide arcade gdroms
					std::string basename = get_file_basename(name);
					string_tolower(basename);
					if (arcade_gdroms.count(basename) != 0)
						continue;
				}
				else if ((settings.dreamcast.HideLegacyNaomiRoms
								|| (extension != "bin" && extension != "lst" && extension != "dat"))
						&& extension != "cdi" && extension != "cue")
					continue;
				insert_game(GameMedia{ name, child_path });
			}
		}
		closedir(dir);
	}

public:
	~GameScanner()
	{
		stop();
	}
	void refresh()
	{
		stop();
		scan_done = false;
	}

	void stop()
	{
		running = false;
        empty_folders_scanned = 0;
        content_path_looks_incorrect = false;
		if (scan_thread && scan_thread->joinable())
			scan_thread->join();
	}

	void fetch_game_list()
	{
		if (scan_done || running)
			return;
		running = true;
		scan_thread = std::unique_ptr<std::thread>(
			new std::thread([this]()
			{
				if (arcade_games.empty())
					for (int gameid = 0; Games[gameid].name != nullptr; gameid++)
					{
						const Game *game = &Games[gameid];
						arcade_games[game->name] = game;
						if (game->gdrom_name != nullptr)
							arcade_gdroms.insert(game->gdrom_name);
					}
				{
					std::lock_guard<std::mutex> guard(mutex);
					game_list.clear();
				}
				for (const auto& path : settings.dreamcast.ContentPath)
				{
					add_game_directory(path);
					if (!running)
						break;
				}
				if (running)
					scan_done = true;
				running = false;
			}));
	}

	std::mutex& get_mutex() { return mutex; }
	const std::vector<GameMedia>& get_game_list() { return game_list; }
    unsigned int empty_folders_scanned = 0;
    bool content_path_looks_incorrect = false;
};
