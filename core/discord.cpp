/*
	Copyright 2024 flyinghead

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
#include "types.h"
#include "emulator.h"
#include "stdclass.h"
#include "cfg/option.h"
#include "discord_rpc.h"
#include <cstring>
#include <time.h>

#define FLYCAST_APPID "1212789289851559946"

class DiscordPresence
{
public:
	DiscordPresence()
	{
		EventManager::listen(Event::Start, handleEmuEvent, this);
		EventManager::listen(Event::Terminate, handleEmuEvent, this);
		EventManager::listen(Event::Resume, handleEmuEvent, this);
	}

	~DiscordPresence()
	{
		shutdown();
		EventManager::unlisten(Event::Start, handleEmuEvent, this);
		EventManager::unlisten(Event::Terminate, handleEmuEvent, this);
		EventManager::unlisten(Event::Resume, handleEmuEvent, this);
	}

private:
	void initialize()
	{
		if (!initialized)
			Discord_Initialize(FLYCAST_APPID, nullptr, 0, nullptr);
	    initialized = true;
	}

	void shutdown()
	{
		if (initialized)
			Discord_Shutdown();
		initialized = false;
	}

	void sendPresence()
	{
		initialize();
        DiscordRichPresence discordPresence{};
        std::string state = settings.content.title.substr(0, 128);
        discordPresence.state = state.c_str();
        discordPresence.startTimestamp = time(0);
        discordPresence.largeImageKey = "icon-512";
        Discord_UpdatePresence(&discordPresence);
	}

	static void handleEmuEvent(Event event, void *p)
	{
		if (settings.naomi.slave || settings.naomi.drivingSimSlave != 0)
			return;
		DiscordPresence *inst = (DiscordPresence *)p;
		switch (event)
		{
		case Event::Start:
			if (config::DiscordPresence)
				inst->sendPresence();
			break;
		case Event::Resume:
			if (config::DiscordPresence && !inst->initialized)
				// Discord presence enabled
				inst->sendPresence();
			else if (!config::DiscordPresence && inst->initialized)
			{
				// Discord presence disabled
				Discord_ClearPresence();
				inst->shutdown();
			}
			break;
		case Event::Terminate:
			if (inst->initialized)
				Discord_ClearPresence();
			break;
		default:
			break;
		}
	}

	bool initialized = false;
};

static DiscordPresence discordPresence;
