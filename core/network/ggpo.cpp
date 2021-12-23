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
#include "hw/maple/maple_devs.h"
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "input/mouse.h"
#include "cfg/option.h"
#include <algorithm>

void UpdateInputState();

namespace ggpo
{

static void getLocalInput(MapleInputState inputState[4])
{
	if (!config::ThreadedRendering)
		UpdateInputState();
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
		state.mouseButtons = mo_buttons[player];
		state.absPos.x = mo_x_abs[player];
		state.absPos.y = mo_y_abs[player];
		state.keyboard.shift = kb_shift[player];
		memcpy(state.keyboard.key, kb_key[player], sizeof(kb_key[player]));
		state.relPos.x = std::round(mo_x_delta[player]);
		state.relPos.y = std::round(mo_y_delta[player]);
		state.relPos.wheel = std::round(mo_wheel_delta[player]);
		mo_x_delta[player] -= state.relPos.x;
		mo_y_delta[player] -= state.relPos.y;
		mo_wheel_delta[player] -= state.relPos.wheel;
	}
}

}

#ifdef USE_GGPO
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
#include "hw/naomi/naomi_cart.h"

//#define SYNC_TEST 1

namespace ggpo
{
using namespace std::chrono;

constexpr int MAX_PLAYERS = 2;
constexpr int SERVER_PORT = 19713;

constexpr u32 BTN_TRIGGER_LEFT	= DC_BTN_RELOAD << 1;
constexpr u32 BTN_TRIGGER_RIGHT	= DC_BTN_RELOAD << 2;

#pragma pack(push, 1)
struct VerificationData
{
	const int protocol = 2;
	u8 gameMD5[16] { };
	u8 stateMD5[16] { };
} ;
#pragma pack(pop)

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
static int analogAxes;
static bool absPointerPos;
static bool keyboardGame;
static bool mouseGame;
static int inputSize;
static bool inRollback;
static void (*chatCallback)(int playerNum, const std::string& msg);

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

#pragma pack(push, 1)
struct Inputs
{
	u32 kcode:20;
	u32 mouseButtons:4;
	u32 kbModifiers:8;

	union {
		struct {
			u8 x;
			u8 y;
		} analog;
		struct {
			s16 x;
			s16 y;
		} absPos;
		struct {
			s16 x;
			s16 y;
			s16 wheel;
		} relPos;
		u8 keys[6];
	} u;
};
static_assert(sizeof(Inputs) == 10, "wrong Inputs size");

struct GameEvent
{
	enum : char {
		Chat
	} type;
	union {
		struct {
			u8 playerNum;
			char message[512 - sizeof(playerNum) - sizeof(type)];
		} chat;
	} u;

	constexpr static int chatMessageLen(int len) { return len - sizeof(u.chat.playerNum) - sizeof(type); }
};
#pragma pack(pop)

/*
 * begin_game callback - This callback has been deprecated.  You must
 * implement it, but should ignore the 'game' parameter.
 */
static bool begin_game(const char *)
{
	DEBUG_LOG(NETWORK, "Game begin");
	rend_allow_rollback();
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
	inRollback = true;

	emu.run();
	ggpo_advance_frame(ggpoSession);

	settings.aica.muteAudio = false;
	settings.disableRenderer = false;
	inRollback = false;
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
	// FIXME dynarecs
	Deserializer deser(buffer, len, true);
	int frame;
	deser >> frame;
	dc_deserialize(deser);
	if (deser.size() != (u32)len)
	{
		ERROR_LOG(NETWORK, "load_game_state len %d used %d", len, (int)deser.size());
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
	Serializer ser(*buffer, allocSize, true);
	ser << frame;
	dc_serialize(ser);
	verify(ser.size() < allocSize);
	*len = ser.size();
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

static void on_message(u8 *msg, int len)
{
	if (len == 0)
		return;
	GameEvent *event = (GameEvent *)msg;
	switch (event->type)
	{
	case GameEvent::Chat:
		if (chatCallback != nullptr && GameEvent::chatMessageLen(len) > 0)
			chatCallback(event->u.chat.playerNum, std::string(event->u.chat.message, GameEvent::chatMessageLen(len)));
		break;

	default:
		WARN_LOG(NETWORK, "Unknown app message type %d", event->type);
		break;
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
	cb.on_message      = on_message;

#ifdef SYNC_TEST
	GGPOErrorCode result = ggpo_start_synctest(&ggpoSession, &cb, settings.content.gameId.c_str(), MAX_PLAYERS, sizeof(kcode[0]), 1);
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO start sync session failed: %d", result);
		ggpoSession = nullptr;
		throw FlycastException("GGPO start sync session failed");
	}
	ggpo_idle(ggpoSession, 0);
	ggpo::localPlayerNum = localPlayerNum;
	GGPOPlayer player{ sizeof(GGPOPlayer), GGPO_PLAYERTYPE_LOCAL, localPlayerNum + 1 };
	result = ggpo_add_player(ggpoSession, &player, &localPlayer);
	player.player_num = (1 - localPlayerNum) + 1;
	result = ggpo_add_player(ggpoSession, &player, &remotePlayer);
	synchronized = true;
	analogAxes = 0;
	NOTICE_LOG(NETWORK, "GGPO synctest session started");
#else
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
		analogAxes = config::GGPOAnalogAxes;
	else
	{
		analogAxes = 0;
		absPointerPos = false;
		keyboardGame = false;
		mouseGame = false;
		if (settings.input.JammaSetup == JVS::LightGun || settings.input.JammaSetup == JVS::LightGunAsAnalog)
			absPointerPos = true;
		else if (settings.input.JammaSetup == JVS::Keyboard)
			keyboardGame = true;
		else if (settings.input.JammaSetup == JVS::RotaryEncoders)
			mouseGame = true;
		else if (NaomiGameInputs != nullptr)
		{
			for (const auto& axis : NaomiGameInputs->axes)
			{
				if (axis.name == nullptr)
					break;
				if (axis.type == Full)
					analogAxes = std::max(analogAxes, (int)axis.axis + 1);
			}
		}
		NOTICE_LOG(NETWORK, "GGPO: Using %d full analog axes", analogAxes);
	}
	inputSize = sizeof(kcode[0]) + analogAxes + (int)absPointerPos * sizeof(Inputs::u.absPos)
		+ (int)keyboardGame * sizeof(Inputs::u.keys) + (int)mouseGame * sizeof(Inputs::u.relPos);

	VerificationData verif;
	MD5Sum().add(settings.network.md5.bios)
			.add(settings.network.md5.game)
			.getDigest(verif.gameMD5);
	auto& digest = settings.network.md5.savestate;
	if (std::find_if(std::begin(digest), std::end(digest), [](u8 b) { return b != 0; }) != std::end(digest))
		memcpy(verif.stateMD5, digest, sizeof(digest));
	else
	{
		MD5Sum().add(settings.network.md5.nvmem)
				.add(settings.network.md5.nvmem2)
				.add(settings.network.md5.eeprom)
				.add(settings.network.md5.vmu)
				.getDigest(verif.stateMD5);
	}

	GGPOErrorCode result = ggpo_start_session(&ggpoSession, &cb, settings.content.gameId.c_str(), MAX_PLAYERS, inputSize, localPort,
			&verif, sizeof(verif));
	if (result != GGPO_OK)
	{
		WARN_LOG(NETWORK, "GGPO start session failed: %d", result);
		ggpoSession = nullptr;
		throw FlycastException("GGPO network initialization failed");
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
		throw FlycastException("GGPO cannot add local player");
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
		throw FlycastException("GGPO cannot add remote player");
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
	emu.setNetworkState(false);
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

	std::vector<u8> inputData(inputSize * MAX_PLAYERS);
	// should not call any callback
	GGPOErrorCode error = ggpo_synchronize_input(ggpoSession, (void *)&inputData[0], inputData.size(), nullptr);
	if (error != GGPO_OK)
	{
		stopSession();
		throw FlycastException("GGPO error");
	}

	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		MapleInputState& state = inputState[player];
		const Inputs *inputs = (Inputs *)&inputData[player * inputSize];
		state.kcode = ~inputs->kcode;
		if (analogAxes > 0)
		{
			state.fullAxes[PJAI_X1] = inputs->u.analog.x;
			if (analogAxes >= 2)
				state.fullAxes[PJAI_Y1] = inputs->u.analog.y;
		}
		else if (absPointerPos)
		{
			state.absPos.x = inputs->u.absPos.x;
			state.absPos.y = inputs->u.absPos.y;
		}
		else if (keyboardGame)
		{
			memcpy(state.keyboard.key, inputs->u.keys, sizeof(state.keyboard.key));
			state.keyboard.shift = inputs->kbModifiers;
		}
		else if (mouseGame)
		{
			state.relPos.x = inputs->u.relPos.x;
			state.relPos.y = inputs->u.relPos.y;
			state.relPos.wheel = inputs->u.relPos.wheel;
			state.mouseButtons = ~inputs->mouseButtons;
		}
		state.halfAxes[PJTI_R] = (state.kcode & BTN_TRIGGER_RIGHT) == 0 ? 255 : 0;
		state.halfAxes[PJTI_L] = (state.kcode & BTN_TRIGGER_LEFT) == 0 ? 255 : 0;
	}
}

bool nextFrame()
{
	if (!_endOfFrame)
		return false;
	_endOfFrame = false;
	if (inRollback)
		return true;
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
	GGPOErrorCode error = ggpo_advance_frame(ggpoSession);

	// may rollback
	if (error == GGPO_OK)
		error = ggpo_idle(ggpoSession, 0);
	if (error != GGPO_OK)
	{
		stopSession();
		if (error == GGPO_ERRORCODE_INPUT_SIZE_DIFF)
			throw FlycastException("GGPO analog settings are different from peer");
		else if (error != GGPO_OK)
			throw FlycastException("GGPO error");
	}

	// may call save_game_state
	do {
		if (!config::ThreadedRendering)
			UpdateInputState();
		Inputs inputs;
		inputs.kcode = ~kcode[0];
		if (rt[0] >= 64)
			inputs.kcode |= BTN_TRIGGER_RIGHT;
		else
			inputs.kcode &= ~BTN_TRIGGER_RIGHT;
		if (lt[0] >= 64)
			inputs.kcode |= BTN_TRIGGER_LEFT;
		else
			inputs.kcode &= ~BTN_TRIGGER_LEFT;
		if (analogAxes > 0)
		{
			inputs.u.analog.x = joyx[0];
			if (analogAxes >= 2)
				inputs.u.analog.y = joyy[0];
		}
		else if (absPointerPos)
		{
			inputs.u.absPos.x = mo_x_abs[0];
			inputs.u.absPos.y = mo_y_abs[0];
		}
		else if (keyboardGame)
		{
			inputs.kbModifiers = kb_shift[0];
			memcpy(inputs.u.keys, kb_key[0], sizeof(kb_key[0]));
		}
		else if (mouseGame)
		{
			inputs.mouseButtons = ~mo_buttons[0];
			inputs.u.relPos.x = std::round(mo_x_delta[0]);
			inputs.u.relPos.y = std::round(mo_y_delta[0]);
			inputs.u.relPos.wheel = std::round(mo_wheel_delta[0]);
			mo_x_delta[0] -= inputs.u.relPos.x;
			mo_y_delta[0] -= inputs.u.relPos.y;
			mo_wheel_delta[0] -= inputs.u.relPos.wheel;
		}
		GGPOErrorCode result = ggpo_add_local_input(ggpoSession, localPlayer, &inputs, inputSize);
		if (result == GGPO_OK)
			break;
		if (result != GGPO_ERRORCODE_PREDICTION_THRESHOLD)
		{
			WARN_LOG(NETWORK, "ggpo_add_local_input failed %d", result);
			stopSession();
			throw FlycastException("GGPO error");
		}
		DEBUG_LOG(NETWORK, "ggpo_add_local_input prediction barrier reached");
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

			try {
				if (config::ActAsServer)
					startSession(SERVER_PORT, 0);
				else
					// Use SERVER_PORT-1 as local port if connecting to ourselves
					startSession(config::NetworkServer.get().empty() || config::NetworkServer.get() == "127.0.0.1" ? SERVER_PORT - 1 : SERVER_PORT, 1);
			} catch (...) {
				miniupnp.Term();
				throw;
			}
#endif
		}
		while (!synchronized && active())
		{
			{
				std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
				if (ggpoSession == nullptr)
					break;
				GGPOErrorCode result = ggpo_idle(ggpoSession, 0);
				if (result == GGPO_ERRORCODE_VERIFICATION_ERROR)
					throw FlycastException("Peer verification failed");
				else if (result != GGPO_OK)
				{
					WARN_LOG(NETWORK, "ggpo_idle failed %d", result);
					throw FlycastException("GGPO error");
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
#ifdef SYNC_TEST
		// save initial state (frame 0)
		if (active())
		{
			MapleInputState state[4];
			getInput(state);
		}
#endif
		emu.setNetworkState(active());
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
	ImGui::Begin("##ggpostats", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
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

void sendChatMessage(int playerNum, const std::string& msg) {
	if (!active())
		return;
	GameEvent event;
	event.type = GameEvent::Chat;
	event.u.chat.playerNum = playerNum;
	size_t msgLen = std::min(msg.length(), sizeof(event.u.chat.message));
	memcpy(event.u.chat.message, msg.c_str(), msgLen);
	ggpo_send_message(ggpoSession, &event, sizeof(GameEvent) - sizeof(event.u.chat.message) + msgLen, true);
}

void receiveChatMessages(void (*callback)(int playerNum, const std::string& msg))
{
	chatCallback = callback;
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

void sendChatMessage(int playerNum, const std::string& msg) {
}

void receiveChatMessages(void (*callback)(int playerNum, const std::string& msg)) {
}

}
#endif
