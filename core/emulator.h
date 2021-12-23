/*
	Created on: Apr 20, 2020

	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "types.h"
#include <atomic>
#include <map>
#include <vector>
#include <future>
#include <string>
#include <memory>
#include <utility>

void loadGameSpecificSettings();
void SaveSettings();

int flycast_init(int argc, char* argv[]);
void dc_reset(bool hard); // for tests only
void flycast_term();
void dc_exit();
void dc_savestate(int index = 0);
void dc_loadstate(int index = 0);
void dc_loadstate(Deserializer& deser);

enum class Event {
	Start,
	Pause,
	Resume,
	Terminate,
	LoadState,
};

class EventManager
{
public:
	using Callback = void (*)(Event, void *);

	static void listen(Event event, Callback callback, void *param = nullptr) {
		Instance.registerEvent(event, callback, param);
	}

	static void unlisten(Event event, Callback callback, void *param = nullptr) {
		Instance.unregisterEvent(event, callback, param);
	}

	static void event(Event event) {
		Instance.broadcastEvent(event);
	}

private:
	EventManager() = default;

	void registerEvent(Event event, Callback callback, void *param);
	void unregisterEvent(Event event, Callback callback, void *param);
	void broadcastEvent(Event event);

	static EventManager Instance;
	std::map<Event, std::vector<std::pair<Callback, void *>>> callbacks;
};

struct LoadProgress
{
	void reset()
	{
		cancelled = false;
		label = nullptr;
		progress = 0.f;
	}
	std::atomic<bool> cancelled;
	std::atomic<const char *> label;
	std::atomic<float> progress;
};

class Emulator
{
public:
	/**
	 * Initialize the emulator. Does nothing if already initialized.
	 */
	void init();
	/**
	 * Terminate the emulator. After calling this method, the application must be restarted.
	 */
	void term();
	/**
	 * Load the specified game/media. Will call init() first.
	 * May throw if the game cannot be loaded.
	 * If a game is already loaded, or the emulator is in the error state, unloadGame() must be called first.
	 */
	void loadGame(const char *path, LoadProgress *progress = nullptr);
	/**
	 * Reset the emulator in order to load another game. After calling this method, only loadGame() and term() can be called.
	 * Does nothing if no game is loaded.
	 */
	void unloadGame();
	/**
	 * Run the emulator in the calling thread until a frame is rendered. A game must be loaded and start() must be called
	 * prior to calling this method.
	 */
	void run();
	/**
	 * Prepare the emulator to start executing. Starts the execution thread if in multi-thread mode.
	 * A game must be loaded successfully before calling this method.
	 */
	void start();
	/**
	 * Stops the execution thread if in multi-thread mode. The game can be restarted by calling start().
	 * Does nothing if the game isn't running.
	 */
	void stop();
	/**
	 * Execute a single instruction.
	 */
	void step();
	/**
	 * Return whether the emulator is currently running.
	 */
	bool running() const {
		return state == Running;
	}
	/**
	 * Wait for the next frame and render it. If in single-thread mode, it will run the emulator until a frame is rendered.
	 */
	bool render();
	/**
	 * Set the network state.
	 */
	void setNetworkState(bool online);
	/**
	 * Called internally to reset the emulator.
	 */
	void requestReset();
	/**
	 * Called internally on vblank.
	 */
	void vblank();

private:
	bool checkStatus();
	void runInternal();

	enum State {
		Uninitialized = 0,
		Init,
		Loaded,
		Running,
		Error,
		Terminated,
	};
	State state = Uninitialized;
	std::shared_future<void> threadResult;
	bool resetRequested = false;
	bool singleStep = false;
	u64 startTime = 0;
	bool renderTimeout = false;
};
extern Emulator emu;
