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
#include "ggpo.h"
#include "hw/maple/maple_cfg.h"
#include "input/gamepad_device.h"

namespace ggpo
{

static void getLocalInput(MapleInputState inputState[4])
{
	for (int player = 0; player < 4; player++)
	{
		MapleInputState& state = inputState[player];
		state.kcode = kcode[player];
		state.halfAxes[PJTI_L] = lt[player];
		state.halfAxes[PJTI_R] = rt[player];
		state.fullAxes[PJAI_X1] = joyx[player];
		state.fullAxes[PJAI_Y1] = joyy[player];
		state.fullAxes[PJAI_X2] = joyrx[player];
		state.fullAxes[PJAI_Y2] = joyry[player];
	}
}

}

#ifndef LIBRETRO
#include "ggponet.h"
#include "emulator.h"
#include "rend/gui.h"
#include "hw/mem/mem_watch.h"
#include "hw/sh4/sh4_sched.h"
#include <string.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <numeric>
#include <xxhash.h>
#include "imgui/imgui.h"
#include "miniupnp.h"

//#define SYNC_TEST 1

namespace ggpo
{
using namespace std::chrono;

constexpr int MAX_PLAYERS = 2;
constexpr int SERVER_PORT = 19713;

static GGPOSession *ggpoSession;
static int localPlayerNum;
static GGPOPlayerHandle localPlayer;
static GGPOPlayerHandle remotePlayer;
static bool synchronized;
static std::recursive_mutex ggpoMutex;
static std::array<int, 5> msPerFrame;
static int msPerFrameIndex;
static time_point<steady_clock> lastFrameTime;
static int msPerFrameAvg;
static bool _endOfFrame;
static MiniUPnP miniupnp;
static int analogInputs = 0;

struct MemPages
{
	void load()
	{
		ram = memwatch::ramWatcher.getPages();
		vram = memwatch::vramWatcher.getPages();
		aram = memwatch::aramWatcher.getPages();
	}
	memwatch::PageMap ram;
	memwatch::PageMap vram;
	memwatch::PageMap aram;
};
static std::unordered_map<int, MemPages> deltaStates;
static int lastSavedFrame = -1;

static int timesyncOccurred;

/*
 * begin_game callback - This callback has been deprecated.  You must
 * implement it, but should ignore the 'game' parameter.
 */
static bool begin_game(const char *)
{
	DEBUG_LOG(NETWORK, "Game begin");
	return true;
}

/*
 * on_event - Notification that something has happened.  See the GGPOEventCode
 * structure for more information.
 */
static bool on_event(GGPOEvent *info)
{
	switch (info->code) {
	case GGPO_EVENTCODE_CONNECTED_TO_PEER:
		INFO_LOG(NETWORK, "Connected to peer %d", info->u.connected.player);
		gui_display_notification("Connected to peer", 2000);
		break;
	case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
		INFO_LOG(NETWORK, "Synchronizing with peer %d", info->u.synchronizing.player);
		gui_display_notification("Synchronizing with peer", 2000);
		break;
	case GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER:
		INFO_LOG(NETWORK, "Synchronized with peer %d", info->u.synchronized.player);
		gui_display_notification("Synchronized with peer", 2000);
		break;
	case GGPO_EVENTCODE_RUNNING:
		INFO_LOG(NETWORK, "Running");
		gui_display_notification("Running", 2000);
		synchronized = true;
		break;
	case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
		INFO_LOG(NETWORK, "Disconnected from peer %d", info->u.disconnected.player);
		throw FlycastException("Disconnected from peer");
		break;
	case GGPO_EVENTCODE_TIMESYNC:
		INFO_LOG(NETWORK, "Timesync: %d frames ahead", info->u.timesync.frames_ahead);
		timesyncOccurred += 5;
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 * info->u.timesync.frames_ahead / (msPerFrameAvg >= 25 ? 30 : 60)));
		break;
	case GGPO_EVENTCODE_CONNECTION_INTERRUPTED:
		INFO_LOG(NETWORK, "Connection interrupted with player %d", info->u.connection_interrupted.player);
		gui_display_notification("Connection interrupted", 2000);
		break;
	case GGPO_EVENTCODE_CONNECTION_RESUMED:
		INFO_LOG(NETWORK, "Connection resumed with player %d", info->u.connection_resumed.player);
		gui_display_notification("Connection resumed", 2000);
		break;
	}
	return true;
}

/*
 * advance_frame - Called during a rollback.  You should advance your game
 * state by exactly one frame.  Before each frame, call ggpo_synchronize_input
 * to retrieve the inputs you should use for that frame.  After each frame,
 * you should call ggpo_advance_frame to notify GGPO.net that you're
 * finished.
 *
 * The flags parameter is reserved.  It can safely be ignored at this time.
 */
static bool advance_frame(int)
{
	INFO_LOG(NETWORK, "advance_frame");
	settings.aica.muteAudio = true;
	settings.disableRenderer = true;

	dc_run();
	ggpo_advance_frame(ggpoSession);

	settings.aica.muteAudio = false;
	settings.disableRenderer = false;
	_endOfFrame = false;

	return true;
}

/*
 * load_game_state - GGPO.net will call this function at the beginning
 * of a rollback.  The buffer and len parameters contain a previously
 * saved state returned from the save_game_state function.  The client
 * should make the current game state match the state contained in the
 * buffer.
 */
static bool load_game_state(unsigned char *buffer, int len)
{
	INFO_LOG(NETWORK, "load_game_state");

	rend_start_rollback();
	// FIXME will invalidate too much stuff: palette/fog textures, maple stuff
	// FIXME dynarecs
	int frame = *(u32 *)buffer;
	unsigned usedLen = sizeof(frame);
	buffer += usedLen;
	dc_unserialize((void **)&buffer, &usedLen, true);
	if (len != (int)usedLen)
	{
		ERROR_LOG(NETWORK, "load_game_state len %d used %d", len, usedLen);
		die("fatal");
	}
	for (int f = lastSavedFrame - 1; f >= frame; f--)
	{
		const MemPages& pages = deltaStates[f];
		for (const auto& pair : pages.ram)
			memcpy(memwatch::ramWatcher.getMemPage(pair.first), &pair.second[0], PAGE_SIZE);
		for (const auto& pair : pages.vram)
			memcpy(memwatch::vramWatcher.getMemPage(pair.first), &pair.second[0], PAGE_SIZE);
		for (const auto& pair : pages.aram)
			memcpy(memwatch::aramWatcher.getMemPage(pair.first), &pair.second[0], PAGE_SIZE);
		DEBUG_LOG(NETWORK, "Restored frame %d pages: %d ram, %d vram, %d aica ram", f, (u32)pages.ram.size(),
					(u32)pages.vram.size(), (u32)pages.aram.size());
	}
	rend_allow_rollback();	// ggpo might load another state right after this one
	memwatch::reset();
	memwatch::protect();
	return true;
}

/*
 * save_game_state - The client should allocate a buffer, copy the
 * entire contents of the current game state into it, and copy the
 * length into the *len parameter.  Optionally, the client can compute
 * a checksum of the data and store it in the *checksum argument.
 */
static bool save_game_state(unsigned char **buffer, int *len, int *checksum, int frame)
{
	verify(!sh4_cpu.IsCpuRunning());
	lastSavedFrame = frame;
	size_t allocSize = (settings.platform.system == DC_PLATFORM_NAOMI ? 20 : 10) * 1024 * 1024;
	*buffer = (unsigned char *)malloc(allocSize);
	if (*buffer == nullptr)
	{
		WARN_LOG(NETWORK, "Memory alloc failed");
		*len = 0;
		return false;
	}
	u8 *data = *buffer;
	*(u32 *)data = frame;
	unsigned usedSize = sizeof(frame);
	data += usedSize;
	dc_serialize((void **)&data, &usedSize, true);
	verify(usedSize < allocSize);
	*len = usedSize;
#ifdef SYNC_TEST
	*checksum = XXH32(*buffer, usedSize, 7);
#endif
	if (frame > 0)
	{
#ifdef SYNC_TEST
		if (deltaStates.count(frame - 1) != 0)
		{
			MemPages memPages;
			memPages.load();
			const MemPages& savedPages = deltaStates[frame - 1];
			//verify(memPages.ram.size() == savedPages.ram.size());
			if (memPages.ram.size() != savedPages.ram.size())
			{
				ERROR_LOG(NETWORK, "old ram size %d new %d", (u32)savedPages.ram.size(), (u32)memPages.ram.size());
				if (memPages.ram.size() > savedPages.ram.size())
					for (const auto& pair : memPages.ram)
					{
						if (savedPages.ram.count(pair.first) == 0)
							ERROR_LOG(NETWORK, "new page @ %x", pair.first);
						else
							DEBUG_LOG(NETWORK, "page ok @ %x", pair.first);
					}
				die("fatal");
			}
			for (const auto& pair : memPages.ram)
			{
				verify(savedPages.ram.count(pair.first) == 1);
				verify(memcmp(&pair.second[0], &savedPages.ram.find(pair.first)->second[0], PAGE_SIZE) == 0);
			}
			verify(memPages.vram.size() == savedPages.vram.size());
			for (const auto& pair : memPages.vram)
			{
				verify(savedPages.vram.count(pair.first) == 1);
				verify(memcmp(&pair.second[0], &savedPages.vram.find(pair.first)->second[0], PAGE_SIZE) == 0);
			}
			//verify(memPages.aram.size() == savedPages.aram.size());
			if (memPages.aram.size() != savedPages.aram.size())
			{
				ERROR_LOG(NETWORK, "old aram size %d new %d", (u32)savedPages.aram.size(), (u32)memPages.aram.size());
				die("fatal");
			}
			for (const auto& pair : memPages.aram)
			{
				verify(savedPages.aram.count(pair.first) == 1);
				verify(memcmp(&pair.second[0], &savedPages.aram.find(pair.first)->second[0], PAGE_SIZE) == 0);
			}
		}
#endif
		// Save the delta to frame-1
		deltaStates[frame - 1].load();
		DEBUG_LOG(NETWORK, "Saved frame %d pages: %d ram, %d vram, %d aica ram", frame - 1, (u32)deltaStates[frame - 1].ram.size(),
				(u32)deltaStates[frame - 1].vram.size(), (u32)deltaStates[frame - 1].aram.size());
	}
	memwatch::protect();

	return true;
}

/*
 * log_game_state - Used in diagnostic testing.  The client should use
 * the ggpo_log function to write the contents of the specified save
 * state in a human readible form.
 */
static bool log_game_state(char *filename, unsigned char *buffer, int len)
{
#ifdef SYNC_TEST
	static int lastLoggedFrame = -1;
	static u8 *lastState;
	int frame = *(u32 *)buffer;
	DEBUG_LOG(NETWORK, "log_game_state frame %d len %d", frame, len);
	if (lastLoggedFrame == frame) {
		for (int i = 0; i < len; i++)
			if (buffer[i] != lastState[i])
			{
				WARN_LOG(NETWORK, "States for frame %d differ at offset %d: now %x prev %x", frame, i, *(u32 *)&buffer[i & ~3], *(u32 *)&lastState[i & ~3]);
				break;
			}
	}
	lastState = buffer;
	lastLoggedFrame = frame;
#endif

	return true;
}

/*
 * free_buffer - Frees a game state allocated in save_game_state.  You
 * should deallocate the memory contained in the buffer.
 */
static void free_buffer(void *buffer)
{
	if (buffer != nullptr)
	{
		int frame = *(u32 *)buffer;
		deltaStates.erase(frame);
		free(buffer);
	}
}

void startSession(int localPort, int localPlayerNum)
{
	GGPOSessionCallbacks cb{};
	cb.begin_game      = begin_game;
	cb.advance_frame   = advance_frame;
	cb.load_game_state = load_game_state;
	cb.save_game_state = save_game_state;
	cb.free_buffer     = free_buffer;
	cb.on_event        = on_event;
	cb.log_game_state  = log_game_state;

#ifdef SYNC_TEST
	GGPOErrorCode result = ggpo_start_synctest(&ggpoSession, &cb, config::Settings::instance().getGameId().c_str(), MAX_PLAYERS, sizeof(kcode[0]), 1);
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO start sync session failed: %d", result);
		ggpoSession = nullptr;
		return;
	}
	ggpo_idle(ggpoSession, 0);
	ggpo::localPlayerNum = localPlayerNum;
	GGPOPlayer player{ sizeof(GGPOPlayer), GGPO_PLAYERTYPE_LOCAL, localPlayerNum + 1 };
	result = ggpo_add_player(ggpoSession, &player, &localPlayer);
	player.player_num = (1 - localPlayerNum) + 1;
	result = ggpo_add_player(ggpoSession, &player, &remotePlayer);
	synchronized = true;
	NOTICE_LOG(NETWORK, "GGPO synctest session started");
#else
	u32 inputSize = sizeof(kcode[0]) + analogInputs;
	GGPOErrorCode result = ggpo_start_session(&ggpoSession, &cb, config::Settings::instance().getGameId().c_str(), MAX_PLAYERS, inputSize, localPort);
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO start session failed: %d", result);
		ggpoSession = nullptr;
		return;
	}

	// automatically disconnect clients after 3000 ms and start our count-down timer
	// for disconnects after 1000 ms.   To completely disable disconnects, simply use
	// a value of 0 for ggpo_set_disconnect_timeout.
	ggpo_set_disconnect_timeout(ggpoSession, 3000);
	ggpo_set_disconnect_notify_start(ggpoSession, 1000);

	ggpo::localPlayerNum = localPlayerNum;
	GGPOPlayer player{ sizeof(GGPOPlayer), GGPO_PLAYERTYPE_LOCAL, localPlayerNum + 1 };
	result = ggpo_add_player(ggpoSession, &player, &localPlayer);
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO cannot add local player: %d", result);
		stopSession();
		return;
	}
	ggpo_set_frame_delay(ggpoSession, localPlayer, config::GGPODelay.get());

	size_t colon = config::NetworkServer.get().find(':');
	std::string peerIp = config::NetworkServer.get().substr(0, colon);
	if (peerIp.empty())
		peerIp = "127.0.0.1";
	u32 peerPort;
	if (colon == std::string::npos)
	{
		if (peerIp == "127.0.0.1")
			peerPort = localPort ^ 1;
		else
			peerPort = SERVER_PORT;
	}
	else
	{
		peerPort = atoi(config::NetworkServer.get().substr(colon + 1).c_str());
	}
	player.type = GGPO_PLAYERTYPE_REMOTE;
	strcpy(player.u.remote.ip_address, peerIp.c_str());
	player.u.remote.port = peerPort;
	player.player_num = (1 - localPlayerNum) + 1;
	result = ggpo_add_player(ggpoSession, &player, &remotePlayer);
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO cannot add remote player: %d", result);
		stopSession();
	}
	DEBUG_LOG(NETWORK, "GGPO session started");
#endif
}

void stopSession()
{
	std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
	if (ggpoSession == nullptr)
		return;
	ggpo_close_session(ggpoSession);
	ggpoSession = nullptr;
	miniupnp.Term();
	dc_set_network_state(false);
}

void getInput(MapleInputState inputState[4])
{
	std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
	if (ggpoSession == nullptr)
	{
		getLocalInput(inputState);
		return;
	}
	for (int player = 0; player < 4; player++)
		inputState[player] = {};

	u32 inputSize = sizeof(u32) + analogInputs;
	std::vector<u8> inputs(inputSize * MAX_PLAYERS);
	// should not call any callback
	ggpo_synchronize_input(ggpoSession, (void *)&inputs[0], inputs.size(), nullptr);

	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		MapleInputState& state = inputState[player];
		state.kcode = ~(*(u32 *)&inputs[player * inputSize]);
		if (analogInputs > 0)
		{
			state.fullAxes[PJAI_X1] = inputs[player * inputSize + 4];
			if (analogInputs == 2)
				state.fullAxes[PJAI_Y1] = inputs[player * inputSize + 5];
		}
		state.halfAxes[PJTI_R] = (state.kcode & EMU_BTN_TRIGGER_RIGHT) == 0 ? 255 : 0;
		state.halfAxes[PJTI_L] = (state.kcode & EMU_BTN_TRIGGER_LEFT) == 0 ? 255 : 0;
	}
}

bool nextFrame()
{
	if (!_endOfFrame)
		return false;
	_endOfFrame = false;
	auto now = std::chrono::steady_clock::now();
	if (lastFrameTime != time_point<steady_clock>())
	{
		msPerFrame[msPerFrameIndex++] = duration_cast<milliseconds>(now - lastFrameTime).count();
		if (msPerFrameIndex >= (int)msPerFrame.size())
			msPerFrameIndex = 0;
		msPerFrameAvg = std::accumulate(msPerFrame.begin(), msPerFrame.end(), 0) / msPerFrame.size();
	}
	lastFrameTime = now;

	std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
	if (ggpoSession == nullptr)
		return false;
	// will call save_game_state
	ggpo_advance_frame(ggpoSession);

	// may rollback
	ggpo_idle(ggpoSession, 0);
	// may call save_game_state
	do {
		u32 input = ~kcode[localPlayerNum];
		if (settings.platform.system != DC_PLATFORM_NAOMI)
		{
			if (rt[localPlayerNum] >= 64)
				input |= EMU_BTN_TRIGGER_RIGHT;
			else
				input &= ~EMU_BTN_TRIGGER_RIGHT;
			if (lt[localPlayerNum] >= 64)
				input |= EMU_BTN_TRIGGER_LEFT;
			else
				input &= ~EMU_BTN_TRIGGER_LEFT;
		}
		u32 inputSize = sizeof(input) + analogInputs;
		std::vector<u8> allInput(inputSize);
		*(u32 *)&allInput[0] = input;
		if (analogInputs > 0)
		{
			allInput[4] = joyx[localPlayerNum];
			if (analogInputs == 2)
				allInput[5] = joyy[localPlayerNum];
		}
		GGPOErrorCode result = ggpo_add_local_input(ggpoSession, localPlayer, &allInput[0], inputSize);
		if (result == GGPO_OK)
			break;
		WARN_LOG(NETWORK, "ggpo_add_local_input failed %d", result);
		if (result != GGPO_ERRORCODE_PREDICTION_THRESHOLD)
		{
			ggpo_close_session(ggpoSession);
			ggpoSession = nullptr;
			throw FlycastException("GGPO error");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		ggpo_idle(ggpoSession, 0);
	} while (active());
#ifdef SYNC_TEST
	u32 input = ~kcode[1 - localPlayerNum];
	GGPOErrorCode result = ggpo_add_local_input(ggpoSession, remotePlayer, &input, sizeof(input));
	if (result != GGPO_OK)
		WARN_LOG(NETWORK, "ggpo_add_local_input(2) failed %d", result);
#endif
	return active();
}

bool active()
{
	return ggpoSession != nullptr;
}

std::future<bool> startNetwork()
{
	synchronized = false;
	return std::async(std::launch::async, []{
		{
			std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
#ifdef SYNC_TEST
			startSession(0, 0);
#else
			miniupnp.Init();
			miniupnp.AddPortMapping(SERVER_PORT, false);

			if (config::ActAsServer)
				startSession(SERVER_PORT, 0);
			else
				// Use SERVER_PORT-1 as local port if connecting to ourselves
				startSession(config::NetworkServer.get().empty() || config::NetworkServer.get() == "127.0.0.1" ? SERVER_PORT - 1 : SERVER_PORT, 1);
#endif
		}
		while (!synchronized && active())
		{
			{
				std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
				if (ggpoSession == nullptr)
					break;
				ggpo_idle(ggpoSession, 0);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
#ifdef SYNC_TEST
		// save initial state (frame 0)
		if (active())
		{
			u32 k[4];
			getInput(k);
		}
#endif
		dc_set_network_state(active());
		return active();
	});
}

void displayStats()
{
	if (!active())
		return;
	GGPONetworkStats stats;
	ggpo_get_network_stats(ggpoSession, remotePlayer, &stats);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ImVec2(95 * scaling, 0));
	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin("", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	// Send Queue
	ImGui::Text("Send Q");
	ImGui::ProgressBar(stats.network.send_queue_len / 10.f, ImVec2(-1, 10.f * scaling), "");

	// Frame Delay
	ImGui::Text("Delay");
	std::string delay = std::to_string(config::GGPODelay.get());
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(delay.c_str()).x);
	ImGui::Text("%s", delay.c_str());

	// Ping
	ImGui::Text("Ping");
	std::string ping = std::to_string(stats.network.ping);
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ping.c_str()).x);
	ImGui::Text("%s", ping.c_str());

	// Predicted Frames
	if (stats.sync.predicted_frames >= 7)
		// red
	    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1, 0, 0, 1));
	else if (stats.sync.predicted_frames >= 5)
		// yellow
	    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(.9f, .9f, .1f, 1));
	ImGui::Text("Predicted");
	ImGui::ProgressBar(stats.sync.predicted_frames / 7.f, ImVec2(-1, 10.f * scaling), "");
	if (stats.sync.predicted_frames >= 5)
		ImGui::PopStyleColor();

	// Frames behind
	int timesync = timesyncOccurred;
	if (timesync > 0)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
	ImGui::Text("Behind");
	ImGui::ProgressBar(0.5f + stats.timesync.local_frames_behind / 16.f, ImVec2(-1, 10.f * scaling), "");
	if (timesync > 0)
	{
		ImGui::PopStyleColor();
		timesyncOccurred--;
	}

	ImGui::PopStyleColor();
	ImGui::End();
	ImGui::PopStyleVar(2);
}

void endOfFrame()
{
	if (active())
	{
		_endOfFrame = true;
		sh4_cpu.Stop();
	}
}

}

#else // LIBRETRO

namespace ggpo
{

void stopSession() {
}

void getInput(MapleInputState inputState[4])
{
	getLocalInput(inputState);
}

bool nextFrame() {
	return true;
}

bool active() {
	return false;
}

std::future<bool> startNetwork() {
	return std::async(std::launch::deferred, []{ return false; });;
}

void displayStats() {
}

void endOfFrame() {
}

}
#endif
