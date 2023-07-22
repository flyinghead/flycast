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

class MiniUPnP
{
public:
	MiniUPnP() {
		lanAddress[0] = 0;
		wanAddress[0] = 0;
		memset(&urls, 0, sizeof(urls));
		memset(&data, 0, sizeof(data));
		memset(&lastError, 0, sizeof(lastError));
	}
	bool Init();
	void Term();
	bool AddPortMapping(int port, bool tcp);
	bool isMapped(int port, bool tcp) { return std::find(mappedPorts.begin(), mappedPorts.end(),
		std::make_pair(std::to_string(port), tcp)) != mappedPorts.end(); }
	const char *localAddress() const { return lanAddress; }
	const char *externalAddress() const { return wanAddress; }
	const char *getLastError() const { return lastError; }

private:
	UPNPUrls urls;
	IGDdatas data;
	char lanAddress[32];
	char wanAddress[32];
	char lastError[256];
	std::vector<std::pair<std::string, bool>> mappedPorts;
	bool initialized = false;
};

#else

class MiniUPnP
{
public:
	bool Init() { return true; }
	void Term() {}
	bool AddPortMapping(int port, bool tcp) { return true; }
	const char *localAddress() const { return ""; }
	const char *externalAddress() const { return ""; }
	const char *getLastError() const { return ""; }
};

#endif
