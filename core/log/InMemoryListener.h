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
		lines.emplace_back(msg);
		if (lines.size() > MaxLines)
			lines.pop_front();
	}

	std::vector<std::string> getLog()
	{
		std::lock_guard<std::mutex> lock(mutex);
		std::vector<std::string> v(lines.begin(), lines.end());

		return v;
	}

	static InMemoryListener *getInstance() {
		return instance;
	}

private:
	std::mutex mutex;
	std::deque<std::string> lines;

	static constexpr int MaxLines = 20;
	static InMemoryListener *instance;
};
