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
#include "types.h"
#include "hw/naomi/naomi_roms.h"
#include <vector>
#include <mutex>
#include <memory>
#include <thread>
#include <unordered_map>
#include <unordered_set>

struct GameMedia {
	std::string name;		// Display name
	std::string path;		// Full path to rom. May be an encoded uri
	std::string fileName;	// Last component of the path, decoded
	std::string gameName;	// for arcade games only, description from the rom list
};

class GameScanner
{
	std::vector<GameMedia> game_list;
	std::vector<GameMedia> arcade_game_list;
	std::mutex mutex;
	std::mutex threadMutex;
	std::unique_ptr<std::thread> scan_thread;
	bool scan_done = false;
	bool running = false;
	std::unordered_map<std::string, const Game*> arcade_games;
	std::unordered_set<std::string> arcade_gdroms;
	using LockGuard = std::lock_guard<std::mutex>;

	void insert_game(const GameMedia& game);
	void insert_arcade_game(const GameMedia& game);
	void add_game_directory(const std::string& path);

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

	void stop();
	void fetch_game_list();

	std::mutex& get_mutex() { return mutex; }
	const std::vector<GameMedia>& get_game_list() { return game_list; }
    unsigned int empty_folders_scanned = 0;
    bool content_path_looks_incorrect = false;
};
