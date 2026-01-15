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
#include "maplelink.h"
#include "cfg/option.h"
#include "hw/maple/maple_if.h"

std::array<std::array<MapleLink::Ptr, 2>, 4> MapleLink::Links;
std::mutex MapleLink::Mutex;

namespace
{
struct GameState
{
	GameState()
	{
		EventManager::listen(Event::Start, [](Event, void*) {
			started = true;
		});
		EventManager::listen(Event::Terminate, [](Event, void*) {
			started = false;
		});
	}
	static bool started;
	static GameState instance;
};
bool GameState::started;
GameState GameState::instance;
}

bool MapleLink::isGameStarted() const {
	return GameState::started;
}

void MapleLink::registerLink(int bus, int port)
{
	if (bus >= 0 && bus < (int)Links.size()
			&& port >= 0 && port < (int)Links[0].size())
	{
		std::lock_guard<std::mutex> _(Mutex);
		Links[bus][port] = shared_from_this();
	}
}
void MapleLink::unregisterLink(int bus, int port)
{
	if (bus >= 0 && bus < (int)Links.size()
			&& port >= 0 && port < (int)Links[0].size())
	{
		std::lock_guard<std::mutex> _(Mutex);
		Ptr& existing = Links[bus][port];
		if (existing == shared_from_this())
			existing = nullptr;
	}
}

BaseMapleLink::BaseMapleLink(bool storageSupported)
	: storageSupported(storageSupported)
{
	EventManager::listen(Event::LoadState, eventLoadState, this);
	EventManager::listen(Event::Start, eventStart, this);
	if (isGameStarted())
		vmuStorage = storageSupported && config::UsePhysicalVmuMemory;
}

BaseMapleLink::~BaseMapleLink()
{
	EventManager::unlisten(Event::LoadState, eventLoadState, this);
	EventManager::unlisten(Event::Start, eventStart, this);
}

void BaseMapleLink::eventStart(Event event, void *p)
{
	BaseMapleLink *self = (BaseMapleLink *)p;
	self->vmuStorage = self->storageSupported && config::UsePhysicalVmuMemory && self->isConnected();
}
void BaseMapleLink::eventLoadState(Event event, void *p)
{
	BaseMapleLink *self = (BaseMapleLink *)p;
	self->disableStorage();
	// FIXME at this point, if storage is enabled, the save state VMU content has been loaded into the MapleLinkVmu.
	// when maple_ReconnectDevices is called, it will be replaced by a regular maple vmu AND WILL LOOSE ITS CONTENT...
	// Add Event::PreLoadState? -> can disable storage at this point and switch to regular maple_vmu (which will be loaded by the savestate)
	// Or: ignore savestate vmu data and reconnect self after load state? -> no need for PreLoadState event and clean behavior but no need to disable storage then...
}

void BaseMapleLink::disableStorage()
{
	if (!vmuStorage)
		return;
	vmuStorage = false;
	if (isGameStarted())
		emu.run(maple_ReconnectDevices);
}

bool BaseMapleLink::storageEnabled()
{
	if (!isConnected())
		return false;
	if (!isGameStarted())
		return storageSupported && config::UsePhysicalVmuMemory;
	else
		return vmuStorage;
}
