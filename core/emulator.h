/*
	Created on: Apr 20, 2020

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
#include <atomic>
#include <map>
#include <vector>
#include <string>

void loadGameSpecificSettings();
void SaveSettings();

extern std::atomic<bool> loading_canceled;

int flycast_init(int argc, char* argv[]);
void dc_reset(bool hard);
void dc_init();
void dc_run();
void dc_term();
void dc_stop();
void dc_term_game();
void dc_term_emulator();
void dc_request_reset();
void dc_exit();
void dc_resume();
void dc_step();
void dc_savestate(int index = 0);
void dc_loadstate(int index = 0);
bool dc_loadstate(const void **data, unsigned size);
void dc_load_game(const char *path);
void dc_start_game(const char *path);
bool dc_is_load_done();
void dc_cancel_load();
void dc_get_load_status();
bool dc_is_running();
std::string dc_get_last_error();
void dc_set_network_state(bool online);

enum class Event {
	Start,
	Pause,
	Resume,
	Terminate,
	LoadState,
};

class EventManager
{
public:
	using Callback = void (*)(Event);

	static void listen(Event event, Callback callback) {
		Instance.registerEvent(event, callback);
	}

	static void unlisten(Event event, Callback callback) {
		Instance.unregisterEvent(event, callback);
	}

	static void event(Event event) {
		Instance.broadcastEvent(event);
	}

private:
	EventManager() = default;

	void registerEvent(Event event, Callback callback);
	void unregisterEvent(Event event, Callback callback);
	void broadcastEvent(Event event);

	static EventManager Instance;
	std::map<Event, std::vector<Callback>> callbacks;
};
