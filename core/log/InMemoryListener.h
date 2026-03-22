/*
	Copyright 2022 flyinghead

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

#include "LogManager.h"
#include <deque>
#include <mutex>
#include <vector>

class InMemoryListener : public LogListener
{
public:
	InMemoryListener() {
		instance = this;
	}
	~InMemoryListener() override {
		instance = nullptr;
	}

	void Log(LogTypes::LOG_LEVELS, const char* msg) override
	{
		std::lock_guard<std::mutex> lock(mutex);
		strncpy(lines[linesIndex], msg, MAX_MSGLEN - 1);
		linesIndex = (linesIndex + 1) % MaxLines;
	}

	std::vector<std::string> getLog()
	{
		std::vector<std::string> v;
		v.reserve(MaxLines);
		std::lock_guard<std::mutex> lock(mutex);
		int i = linesIndex;
		do {
			if (lines[i][0] != '\0')
				v.push_back(lines[i]);
			i = (i + 1) % MaxLines;
		} while (i != linesIndex);

		return v;
	}

	static InMemoryListener *getInstance() {
		return instance;
	}

private:
	static constexpr int MaxLines = 20;

	std::mutex mutex;
	std::array<char[MAX_MSGLEN], MaxLines> lines {};
	int linesIndex = 0;

	static InMemoryListener *instance;
};
