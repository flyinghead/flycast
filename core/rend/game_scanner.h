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

#include "types.h"
#include "stdclass.h"
#include "hw/naomi/naomi_roms.h"
#include "oslib/directory.h"
#include "cfg/option.h"

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
	std::mutex threadMutex;
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
        DirectoryTree tree(path);
        std::string emptyParentPath;
        for (const DirectoryTree::item& item : tree)
        {
            if (running == false)
                break;
            
            if (game_list.empty())
            {
                if(item.parentPath.compare(emptyParentPath))
                {
                    ++empty_folders_scanned;
                    emptyParentPath = item.parentPath;
                    if (empty_folders_scanned > 1000)
                        content_path_looks_incorrect = true;
                }
            }
            else
            {
                content_path_looks_incorrect = false;
            }
            
        	if (item.name.substr(0, 2) == "._")
        		// Ignore Mac OS turds
        		continue;
        	std::string name(item.name);
			std::string child_path = item.parentPath + "/" + name;
#ifdef __APPLE__
            extern std::string os_PrecomposedString(std::string string);
            name = os_PrecomposedString(name);
#endif

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
			else if ((config::HideLegacyNaomiRoms
							|| (extension != "bin" && extension != "lst" && extension != "dat"))
					&& extension != "cdi" && extension != "cue")
				continue;
			insert_game(GameMedia{ name, child_path });
		}
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
		std::lock_guard<std::mutex> guard(threadMutex);
		running = false;
        empty_folders_scanned = 0;
        content_path_looks_incorrect = false;
		if (scan_thread && scan_thread->joinable())
			scan_thread->join();
	}

	void fetch_game_list()
	{
		std::lock_guard<std::mutex> guard(threadMutex);
		if (scan_done || running)
			return;
		if (scan_thread && scan_thread->joinable())
			scan_thread->join();
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
				for (const auto& path : config::ContentPath.get())
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
