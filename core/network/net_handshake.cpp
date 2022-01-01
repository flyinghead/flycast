/*
	Copyright 2021 flyinghead

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
#include "net_handshake.h"
#include "cfg/option.h"
#include "ggpo.h"
#include "naomi_network.h"

NetworkHandshake *NetworkHandshake::instance;

class GGPONetworkHandshake : public NetworkHandshake
{
public:
	std::future<bool> start() override {
		return ggpo::startNetwork();
	}

	void stop() override {
		ggpo::stopSession();
	}

	bool canStartNow() override { return false; }
	void startNow() override {}
};

class NaomiNetworkHandshake : public NetworkHandshake
{
public:
	std::future<bool> start() override {
		return naomiNetwork.startNetworkAsync();
	}

	void stop() override {
		naomiNetwork.terminate();
	}

	bool canStartNow() override {
		return config::ActAsServer;
	}

	void startNow() override {
		naomiNetwork.startNow();
	}
};

void NetworkHandshake::init()
{
	if (settings.platform.system == DC_PLATFORM_NAOMI)
		SetNaomiNetworkConfig(-1);

	if (config::GGPOEnable)
		instance = new GGPONetworkHandshake();
	else if (NaomiNetworkSupported())
		instance = new NaomiNetworkHandshake();
	else
		instance = nullptr;
}

void NetworkHandshake::term()
{
	if (instance != nullptr)
	{
		instance->stop();
		delete instance;
		instance = nullptr;
	}
}
