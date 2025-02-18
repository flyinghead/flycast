/*
	Copyright 2020 flyinghead

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
#ifndef FEAT_NO_MINIUPNPC
#include <miniupnpc.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <mutex>

class MiniUPnP
{
public:
	MiniUPnP() {
		lanAddress[0] = 0;
		memset(&urls, 0, sizeof(urls));
		memset(&data, 0, sizeof(data));
	}
	bool Init();
	void Term();
	bool AddPortMapping(int port, bool tcp);
	bool isInitialized() const { return initialized; }

private:
	UPNPUrls urls;
	IGDdatas data;
	char lanAddress[32];
	std::vector<std::pair<std::string, bool>> mappedPorts;
	bool initialized = false;
	std::mutex mutex;
};

#else

class MiniUPnP
{
public:
	bool Init() { return true; }
	void Term() {}
	bool AddPortMapping(int port, bool tcp) { return true; }
	bool isInitialized() const { return false; }
};

#endif
