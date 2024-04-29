/*
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
// Derived from duckstation: https://github.com/stenzek/duckstation/blob/master/src/core/achievements.cpp
// SPDX-FileCopyrightText: 2019-2023 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: (GPL-3.0 OR CC-BY-NC-ND-4.0)
#include "achievements.h"
#include "serialize.h"
#ifdef USE_RACHIEVEMENTS
#include "oslib/http_client.h"
#include "hw/sh4/sh4_mem.h"
#include "rend/gui.h"
#include "imgread/common.h"
#include "cfg/option.h"
#include "oslib/oslib.h"
#include "emulator.h"
#include "stdclass.h"
#include <cassert>
#include <rc_client.h>
#include <rc_hash.h>
#include <thread>

namespace achievements
{

class Achievements
{
public:
	Achievements();
	~Achievements();
	bool init();
	void term();
	void login(const char *username, const char *password);
	bool isLoggedOn() const { return loggedOn; }
	void serialize(Serializer& ser);
	void deserialize(Deserializer& deser);

	static Achievements& Instance();

private:
	bool createClient();
	std::string getGameHash();
	void loadGame();
	void unloadGame();
	void pauseGame();
	void resumeGame();

	static void clientLoginWithTokenCallback(int result, const char *error_message, rc_client_t *client, void *userdata);
	static void clientLoginWithPasswordCallback(int result, const char *error_message, rc_client_t *client, void *userdata);
	static void clientMessageCallback(const char *message, const rc_client_t *client);
	static u32 clientReadMemory(u32 address, u8 *buffer, u32 num_bytes, rc_client_t *client);
	static void clientServerCall(const rc_api_request_t *request, rc_client_server_callback_t callback,
			void *callback_data, rc_client_t *client);
	static void clientEventHandler(const rc_client_event_t *event, rc_client_t *client);
	static void clientLoadGameCallback(int result, const char *error_message, rc_client_t *client, void *userdata);
	void handleResetEvent(const rc_client_event_t *event);
	void handleUnlockEvent(const rc_client_event_t *event);
	void handleAchievementChallengeIndicatorShowEvent(const rc_client_event_t *event);
	void handleAchievementChallengeIndicatorHideEvent(const rc_client_event_t *event);
	static void emuEventCallback(Event event, void *arg);

	rc_client_t *rc_client = nullptr;
	bool loggedOn = false;
	bool active = false;
	bool paused = false;
	std::string lastError;
	cResetEvent resetEvent;
	std::thread thread;
};

bool init()
{
	return Achievements::Instance().init();
}

void term()
{
	Achievements::Instance().term();
}

void login(const char *username, const char *password)
{
	Achievements::Instance().login(username, password);
}

bool isLoggedOn()
{
	return Achievements::Instance().isLoggedOn();
}

void serialize(Serializer& ser)
{
	Achievements::Instance().serialize(ser);
}
void deserialize(Deserializer& deser)
{
	Achievements::Instance().deserialize(deser);
}

Achievements& Achievements::Instance()
{
	static Achievements instance;
	return instance;
}
// create the instance at start up
OnLoad _([]() { Achievements::Instance(); });

Achievements::Achievements()
{
	EventManager::listen(Event::Start, emuEventCallback, this);
	EventManager::listen(Event::Terminate, emuEventCallback, this);
	EventManager::listen(Event::Pause, emuEventCallback, this);
	EventManager::listen(Event::Resume, emuEventCallback, this);
}

Achievements::~Achievements()
{
	EventManager::unlisten(Event::Start, emuEventCallback, this);
	EventManager::unlisten(Event::Terminate, emuEventCallback, this);
	EventManager::unlisten(Event::Pause, emuEventCallback, this);
	EventManager::unlisten(Event::Resume, emuEventCallback, this);
	term();
}

bool Achievements::init()
{
	if (rc_client != nullptr)
		return true;

	if (!createClient())
		return false;

	rc_client_set_event_handler(rc_client, clientEventHandler);

	//TODO
	rc_client_set_hardcore_enabled(rc_client, 0);
	//rc_client_set_encore_mode_enabled(rc_client, 0);
	//rc_client_set_unofficial_enabled(rc_client, 0);
	//rc_client_set_spectator_mode_enabled(rc_client, 0);

	if (!config::AchievementsUserName.get().empty() && !config::AchievementsToken.get().empty())
	{
		INFO_LOG(COMMON, "RA: Attempting login with user '%s'...", config::AchievementsUserName.get().c_str());
		rc_client_begin_login_with_token(rc_client, config::AchievementsUserName.get().c_str(),
				config::AchievementsToken.get().c_str(), clientLoginWithTokenCallback, nullptr);
	}

	return true;
}

bool Achievements::createClient()
{
	http::init();
	rc_client = rc_client_create(clientReadMemory, clientServerCall);
	if (rc_client == nullptr)
	{
		WARN_LOG(COMMON, "Can't create RetroAchievements client");
		return false;
	}

#if !defined(NDEBUG) || defined(DEBUGFAST)
	rc_client_enable_logging(rc_client, RC_CLIENT_LOG_LEVEL_VERBOSE, clientMessageCallback);
#else
	rc_client_enable_logging(rc_client, RC_CLIENT_LOG_LEVEL_WARN, clientMessageCallback);
#endif

	rc_client_set_userdata(rc_client, this);

	return true;
}

void Achievements::term()
{
	if (rc_client == nullptr)
		return;
	unloadGame();
	rc_client_destroy(rc_client);
	rc_client = nullptr;
}

static inline void authenticationSuccessMsg()
{
	NOTICE_LOG(COMMON, "RA Login successful: token %s", config::AchievementsToken.get().c_str());
	std::string msg = "User " + config::AchievementsUserName.get() + " authenticated to RetroAchievements";
	gui_display_notification(msg.c_str(), 5000);
}

void Achievements::clientLoginWithTokenCallback(int result, const char* error_message, rc_client_t* client,
                                                void* userdata)
{
	if (result != RC_OK)
	{
		WARN_LOG(COMMON, "RA Login failed: %s", error_message);
		gui_display_notification(error_message, 10000);
		return;
	}

	authenticationSuccessMsg();
	Achievements *achievements = (Achievements *)rc_client_get_userdata(client);
	achievements->loggedOn = true;
}

void Achievements::login(const char* username, const char* password)
{
	init();
	rc_client_begin_login_with_password(rc_client, username, password, clientLoginWithPasswordCallback, nullptr);

	if (!loggedOn)
		throw FlycastException(lastError);
}

void Achievements::clientLoginWithPasswordCallback(int result, const char* error_message, rc_client_t* client,
                                                   void* userdata)
{
	Achievements *achievements = (Achievements *)rc_client_get_userdata(client);

	if (result != RC_OK)
	{
		achievements->lastError = rc_error_str(result);
		if (error_message != nullptr)
			achievements->lastError += ": " + std::string(error_message);
		WARN_LOG(COMMON, "RA Login failed: %s", achievements->lastError.c_str());
		return;
	}

	const rc_client_user_t* user = rc_client_get_user_info(client);
	if (!user || !user->token)
	{
		WARN_LOG(COMMON, "RA: rc_client_get_user_info() returned NULL");
		return;
	}

	// Store token in config
	config::AchievementsToken = user->token;
	SaveSettings();
	achievements->loggedOn = true;

	authenticationSuccessMsg();
}

void Achievements::clientMessageCallback(const char* message, const rc_client_t* client)
{
#if !defined(NDEBUG) || defined(DEBUGFAST)
	DEBUG_LOG(COMMON, "RA: %s", message);
#else
	WARN_LOG(COMMON, "RA error: %s", message);
#endif
}

u32 Achievements::clientReadMemory(u32 address, u8* buffer, u32 num_bytes, rc_client_t* client)
{
	if (address + num_bytes > RAM_SIZE)
		return 0;
	address += 0x0C000000;
	switch (num_bytes)
	{
	case 1:
    	*buffer = ReadMem8_nommu(address);
    	break;
	case 2:
		*(u16 *)buffer = ReadMem16_nommu(address);
		break;
	case 4:
		*(u32 *)buffer = ReadMem32_nommu(address);
		break;
	default:
		return 0;
	}
	return num_bytes;
}

void Achievements::clientServerCall(const rc_api_request_t* request, rc_client_server_callback_t callback,
                                    void* callback_data, rc_client_t* client)
{
	int rc;
	std::vector<u8> reply;
	if (request->post_data != nullptr)
		rc = http::post(request->url, request->post_data, reply);
	else
		rc = http::get(request->url, reply);
	rc_api_server_response_t rr;
	rr.http_status_code = rc;	// TODO RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR if connection fails?
	rr.body_length = reply.size();
	rr.body = (const char *)reply.data();
	callback(&rr, callback_data);
}

static void handleServerError(const rc_client_server_error_t* error)
{
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s: %s", error->api, error->error_message);
  gui_display_notification(buffer, 5000);
}

void Achievements::clientEventHandler(const rc_client_event_t* event, rc_client_t* client)
{
	Achievements *achievements = (Achievements *)rc_client_get_userdata(client);
	switch (event->type)
	{
	case RC_CLIENT_EVENT_RESET:
		achievements->handleResetEvent(event);
		break;

	case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
		achievements->handleUnlockEvent(event);
		break;

	case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
		achievements->handleAchievementChallengeIndicatorShowEvent(event);
		break;

	case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
		achievements->handleAchievementChallengeIndicatorHideEvent(event);
		break;

	case RC_CLIENT_EVENT_SERVER_ERROR:
		handleServerError(event->server_error);
		break;

/*
 TODO
	case RC_CLIENT_EVENT_GAME_COMPLETED:
	case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
	case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
	case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
	case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
	case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
	case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
	case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
	case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
	case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
	case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
	case RC_CLIENT_EVENT_SERVER_ERROR:
	case RC_CLIENT_EVENT_DISCONNECTED:
	case RC_CLIENT_EVENT_RECONNECTED:
*/
	default:
		WARN_LOG(COMMON, "RA: Unhandled event: %u", event->type);
		break;
	}
}

void Achievements::handleResetEvent(const rc_client_event_t *event)
{
	INFO_LOG(COMMON, "RA: Resetting runtime due to reset event");
	rc_client_reset(rc_client);
}

void Achievements::handleUnlockEvent(const rc_client_event_t *event)
{
	const rc_client_achievement_t* cheevo = event->achievement;
	assert(cheevo != nullptr);

	INFO_LOG(COMMON, "RA: Achievement %s (%u) for game %s unlocked", cheevo->title, cheevo->id, settings.content.title.c_str());

	std::string msg = "Achievement " + std::string(cheevo->title) + " unlocked!";
	gui_display_notification(msg.c_str(), 10000);
}

void Achievements::handleAchievementChallengeIndicatorShowEvent(const rc_client_event_t *event)
{
	// TODO there might be more than one. Need to display an icon.
	std::string msg = "Challenge: " + std::string(event->achievement->title);
	gui_display_notification(msg.c_str(), 10000);
}

void Achievements::handleAchievementChallengeIndicatorHideEvent(const rc_client_event_t *event)
{
	gui_display_notification("", 1);
}

static bool add150;

static void *cdreader_open_track(const char* path, u32 track)
{
	DEBUG_LOG(COMMON, "RA: cdreader_open_track %s track %d", path, track);
	if (track == RC_HASH_CDTRACK_FIRST_DATA)
	{
		u32 toc[102];
		libGDR_GetToc(toc, SingleDensity);
		for (int i = 0; i < 99; i++)
			if (toc[i] != 0xffffffff)
			{
				if (((toc[i] >> 4) & 0xf) & 4)
					return reinterpret_cast<void *>(i + 1);
			}
		if (libGDR_GetDiscType() == GdRom)
		{
			libGDR_GetToc(toc, DoubleDensity);
			for (int i = 0; i < 99; i++)
				if (toc[i] != 0xffffffff)
				{
					if (((toc[i] >> 4) & 0xf) & 4)
						return reinterpret_cast<void *>(i + 1);
				}
		}
	}
	u32 start, end;
	if (!libGDR_GetTrack(track, start, end))
		return nullptr;
	else
		return reinterpret_cast<void *>(track);
}

static size_t cdreader_read_sector(void* track_handle, u32 sector, void* buffer, size_t requested_bytes)
{
	//DEBUG_LOG(COMMON, "RA: cdreader_read_sector track %zd sec %d num %zd", reinterpret_cast<uintptr_t>(track_handle), sector, requested_bytes);
	if (requested_bytes == 2048)
		add150 = true;
	if (add150)	// FIXME how to get rid of this?
		sector += 150;
	u32 count = requested_bytes;
	u32 secNum = count / 2048;
	if (secNum > 0)
	{
		libGDR_ReadSector((u8 *)buffer, sector, secNum, 2048);
		buffer = (u8 *)buffer + secNum * 2048;
		count -= secNum * 2048;
	}
	if (count > 0)
	{
		u8 locbuf[2048];
		libGDR_ReadSector(locbuf, sector + secNum, 1, 2048);
		memcpy(buffer, locbuf, count);
	}
	return requested_bytes;
}

static void cdreader_close_track(void* track_handle)
{
}

static u32 cdreader_first_track_sector(void* track_handle)
{
	u32 trackNum = reinterpret_cast<uintptr_t>(track_handle);
	u32 start, end;
	if (!libGDR_GetTrack(trackNum, start, end))
		return 0;
	DEBUG_LOG(COMMON, "RA: cdreader_first_track_sector track %d -> %d", trackNum, start);
	return start;
}

std::string Achievements::getGameHash()
{
	if (settings.platform.isConsole())
	{
		add150 = false;
		rc_hash_cdreader hooks = {
				cdreader_open_track,
				cdreader_read_sector,
				cdreader_close_track,
				cdreader_first_track_sector
		};
		rc_hash_init_custom_cdreader(&hooks);
		rc_hash_init_error_message_callback([](const char *msg) {
			WARN_LOG(COMMON, "cdreader: %s", msg);
		});
#ifndef NDEBUG
		rc_hash_init_verbose_message_callback([](const char *msg) {
			DEBUG_LOG(COMMON, "cdreader: %s", msg);
		});
#endif
	}
	char hash[33] {};
	rc_hash_generate_from_file(hash, settings.platform.isConsole() ? RC_CONSOLE_DREAMCAST : RC_CONSOLE_ARCADE,
			settings.content.fileName.c_str());	// fileName is only used for arcade

	return hash;
}

void Achievements::pauseGame()
{
	paused = true;
	if (active)
		resetEvent.Reset();
}

void Achievements::resumeGame()
{
	paused = false;
	if (config::EnableAchievements)
		loadGame();
	else
		term();
}

void Achievements::emuEventCallback(Event event, void *arg)
{
	Achievements *instance = ((Achievements *)arg);
	switch (event)
	{
	case Event::Start:
	case Event::Resume:
		instance->resumeGame();
		break;
	case Event::Terminate:
		instance->unloadGame();
		break;
	case Event::VBlank:
		instance->resetEvent.Set();
		break;
	case Event::Pause:
		instance->pauseGame();
		break;
	default:
		break;
	}
}

void Achievements::loadGame()
{
	if (active)
		// already loaded
		return;
	if (!init() || !isLoggedOn())
		return;
	std::string gameHash = getGameHash();
	if (!gameHash.empty())
		rc_client_begin_load_game(rc_client, gameHash.c_str(), clientLoadGameCallback, nullptr);
	if (!active)
		return;
	thread = std::thread([this] {
		ThreadName _("Flycast-RA");
		while (active)
		{
			if (!resetEvent.Wait(1000)) {
				if (paused) {
					DEBUG_LOG(COMMON, "RA: rc_client_idle");
					rc_client_idle(rc_client);
				}
				else {
					INFO_LOG(COMMON, "RA: timeout on event and not paused!");
				}
			}
			else if (active)
				rc_client_do_frame(rc_client);
		}
	});
	EventManager::listen(Event::VBlank, emuEventCallback, this);
}

void Achievements::unloadGame()
{
	if (!active)
		return;
	active = false;
	resetEvent.Set();
	thread.join();
	EventManager::unlisten(Event::VBlank, emuEventCallback, this);
	rc_client_unload_game(rc_client);
}

void Achievements::clientLoadGameCallback(int result, const char* error_message, rc_client_t* client, void* userdata)
{
	Achievements *achiev = (Achievements *)rc_client_get_userdata(client);
	if (result == RC_NO_GAME_LOADED)
	{
		// Unknown game.
		INFO_LOG(COMMON, "RA: Unknown game, disabling achievements.");
		return;
	}
	if (result == RC_LOGIN_REQUIRED)
	{
		// We would've asked to re-authenticate, so leave HC on for now.
		// Once we've done so, we'll reload the game.
		return;
	}
	if (result != RC_OK)
	{
		WARN_LOG(COMMON, "RA Loading game failed: %s", error_message);
		return;
	}
	const rc_client_game_t* info = rc_client_get_game_info(client);
	if (info == nullptr)
	{
		WARN_LOG(COMMON, "RA: rc_client_get_game_info() returned NULL");
		return;
	}
	// TODO? rc_client_game_get_image_url(info, buf, std::size(buf));
	achiev->active = true;
	NOTICE_LOG(COMMON, "RA: game %d loaded: %s, achievements %d leaderboards %d rich presence %d", info->id, info->title,
			rc_client_has_achievements(client), rc_client_has_leaderboards(client), rc_client_has_rich_presence(client));
}

void Achievements::serialize(Serializer& ser)
{
	u32 size = (u32)rc_client_progress_size(rc_client);
	if (size > 0)
	{
		u8 *buffer = new u8[size];
		if (rc_client_serialize_progress(rc_client, buffer) != RC_OK)
			size = 0;
		ser << size;
		ser.serialize(buffer, size);
		delete[] buffer;
	}
	else {
		ser << size;
	}
}
void Achievements::deserialize(Deserializer& deser)
{
	if (deser.version() < Deserializer::V50) {
		 rc_client_deserialize_progress(rc_client, nullptr);
	}
	else {
		u32 size;
		deser >> size;
		if (size > 0)
		{
			u8 *buffer = new u8[size];
			deser.deserialize(buffer, size);
			rc_client_deserialize_progress(rc_client, buffer);
			delete[] buffer;
		}
		else {
			rc_client_deserialize_progress(rc_client, nullptr);
		}
	}
}

}	// namespace achievements

#else	// !USE_RACHIEVEMENTS

namespace achievements
{

// Maintain savestate compatiblity with RA-enabled builds
void serialize(Serializer& ser)
{
	u32 size = 0;
	ser << size;
}

void deserialize(Deserializer& deser)
{
	if (deser.version() >= Deserializer::V50)
	{
		u32 size;
		deser >> size;
		deser.skip(size);
	}
}

}
#endif
