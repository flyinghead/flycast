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
#include "cfg/option.h"
#include "gui.h"
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
		EventManager::listen(Event::Network, handleEmuEvent, this);
	}

	~DiscordPresence()
	{
		shutdown();
		EventManager::unlisten(Event::Start, handleEmuEvent, this);
		EventManager::unlisten(Event::Terminate, handleEmuEvent, this);
		EventManager::unlisten(Event::Resume, handleEmuEvent, this);
		EventManager::unlisten(Event::Network, handleEmuEvent, this);
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
        discordPresence.state = settings.content.title.c_str();
       	discordPresence.startTimestamp = startTimestamp;
        std::string imageUrl = gui_getCurGameBoxartUrl();
        if (!imageUrl.empty())
        {
        	discordPresence.largeImageKey = imageUrl.c_str();
        	discordPresence.largeImageText = settings.content.title.c_str();
        	discordPresence.smallImageKey = "icon-512";
        	discordPresence.smallImageText = "Flycast is a Dreamcast, Naomi and Atomiswave emulator";
        }
        else
        {
        	discordPresence.largeImageKey = "icon-512";
        	discordPresence.largeImageText = "Flycast is a Dreamcast, Naomi and Atomiswave emulator";
        }
        if (settings.network.online)
        	discordPresence.details = "Online";
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
			inst->startTimestamp = time(nullptr);
			[[fallthrough]];
		case Event::Network:
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
			inst->startTimestamp = 0;
			break;
		default:
			break;
		}
	}

	bool initialized = false;
	int64_t startTimestamp = 0;
};

static DiscordPresence discordPresence;
