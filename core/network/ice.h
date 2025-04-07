/*
	Copyright 2025 Flyinghead <flyinghead.github@gmail.com>

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
#pragma once
#include <string>
#include <vector>

namespace ice
{
typedef enum {
	Offline,
	Online,
	ChalSent,
	ChalReceived,
	ChalRefused,
	ChalAccepted,
	Playing
} State;

#ifdef USE_ICE

void init(const std::string& username, bool matchCode = false);
State getState();
std::string getStatusText();
std::vector<std::string> getUserList();
void sendChallenge(const std::string& user);
std::string getChallenger();
void respondChallenge(bool accept);
void say(const std::string& msg);
std::vector<std::string> getChat();
void term();
void displayStats();

#else

static inline State getState() {
	return Offline;
}
static inline void displayStats() {
}

#endif

}


