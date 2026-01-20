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
#pragma once
#include "types.h"
#include "hw/maple/maple_devs.h"
#include "emulator.h"
#include <array>
#include <list>
#include <memory>
#include <mutex>

// parent interface of DreamLink and DreamPotato for use by hw/maple
class MapleLink : public std::enable_shared_from_this<MapleLink>
{
public:
	using Ptr = std::shared_ptr<MapleLink>;

	virtual ~MapleLink() = default;

	//! Sends a message to the controller, not expecting a response
	virtual bool send(const MapleMsg& msg) = 0;
	//! Sends a message to the controller and waits for a response
	virtual bool sendReceive(const MapleMsg& txMsg, MapleMsg& rxMsg) = 0;
	//! Called on GetLastError command over previous writes
	//! @param[in] msg The message which contains GetLastError command
	//! @return true iff successful
	virtual bool handleGetLastError(const MapleMsg& msg) = 0;
	//! True if VMU reads and writes should be sent to the device
	virtual bool storageEnabled() = 0;
	//! True if the link is operational
	virtual bool isConnected() = 0;
	std::size_t activeLinkCount(int bus) const;

	//! Returns the maple link at the given location if any
	static Ptr GetMapleLink(int bus, int port) {
		std::lock_guard<std::mutex> _(Mutex);
		std::list<MapleLink::Ptr>& list = Links[bus][port];
		if (list.empty())
			return nullptr;
		return list.front();
	}
	//! True if any maple link currently has storage enabled
	static bool StorageEnabled();

protected:
	//! True if a game has been started
	bool isGameStarted() const;
	void registerLink(int bus, int port);
	void unregisterLink(int bus, int port);
	int bus = -1;
	u32 ports = 0;

private:
	static std::mutex Mutex;
	//! Indexed by [bus][extension port][priority idx]
	//! Multiple links may be registered for a bus/port, and the last added to front takes precedence
	static std::array<std::array<std::list<Ptr>, 2>, 4> Links;
};

class BaseMapleLink : public MapleLink
{
public:
	~BaseMapleLink();
	bool storageEnabled() override;
	bool handleGetLastError(const MapleMsg& msg) override;

protected:
	BaseMapleLink(bool storageSupported);
	void disableStorage(); // Disable VMU storage for this link

	static void eventStart(Event event, void *p);
	static void eventLoadState(Event event, void *p);

	const bool storageSupported;
	bool vmuStorage = false;
};
