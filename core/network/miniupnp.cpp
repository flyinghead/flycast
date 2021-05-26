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
#include "build.h"
#ifdef USE_MINIUPNPC
#include <miniupnpc.h>
#include <upnpcommands.h>
#include "types.h"
#include "miniupnp.h"

#ifndef UPNP_LOCAL_PORT_ANY
#define UPNP_LOCAL_PORT_ANY 0
#endif

bool MiniUPnP::Init()
{
	int error = 0;
#if MINIUPNPC_API_VERSION >= 14
	UPNPDev *devlist = upnpDiscover(2000, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &error);
#else
	UPNPDev *devlist = upnpDiscover(2000, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, &error);
#endif
	if (devlist == nullptr)
	{
		INFO_LOG(MODEM, "UPnP discover failed: error %d", error);
		return false;
	}
	error = UPNP_GetValidIGD(devlist, &urls, &data, lanAddress, sizeof(lanAddress));
	freeUPNPDevlist(devlist);
	if (error != 1)
	{
		INFO_LOG(MODEM, "Internet Gateway not found: error %d", error);
		return false;
	}
	wanAddress[0] = 0;
	if (UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, wanAddress) != 0)
		INFO_LOG(MODEM, "Cannot determine external IP address");
	return true;
}

void MiniUPnP::Term()
{
	for (const auto& port : mappedPorts)
		UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.first.c_str(),
							   port.second ? "TCP" : "UDP", nullptr);
	mappedPorts.clear();
	FreeUPNPUrls(&urls);
}

bool MiniUPnP::AddPortMapping(int port, bool tcp)
{
	std::string portStr(std::to_string(port));
	int error = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
								portStr.c_str(), // WAN port
								portStr.c_str(), // LAN port
								lanAddress,
								"Flycast",
								tcp ? "TCP" : "UDP",
								nullptr,   // remote (peer) host address or nullptr for no restriction
								"86400");  // port map lease duration (in seconds) or zero for "as long as possible"
	if (error != 0)
	{
		INFO_LOG(MODEM, "Port %d redirection failed: error %d", port, error);
		return false;
	}
	mappedPorts.emplace_back(portStr, tcp);
	return true;
}
#endif
