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

#include <asio.hpp> // Must be included first to avoid winsock issues on Windows
#include "dreamconn.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "oslib/oslib.h"
#include "ui/gui.h"
#include <cfg/option.h>
#include "oslib/i18n.h"
#include <SDL.h>
#include <mutex>

#ifdef USE_DREAMCONN

asio::error_code sendMsg(const MapleMsg& msg, asio::ip::tcp::iostream& stream);
bool receiveMsg(MapleMsg& msg, std::istream& stream);

//! DreamConn implementation class
//! This is here mainly so asio.hpp can be included in this source file instead of the header.
class DreamConn : public DreamLink
{
	int bus = -1;
	bool maple_io_connected = false;
	std::array<MapleDeviceType, 2> expansionDevs{};
	asio::ip::tcp::iostream iostream;
	//! Base port of communication to DreamConn
	static constexpr u16 BASE_PORT = 37393;

public:
	DreamConn(int bus)
		: DreamLink(false), bus(bus)
	{}

	bool storageEnabled() override {
		// DreamConn controllers don't support physical VMU memory access
		return false;
	}

	bool send(const MapleMsg& msg) override
	{
		if (!maple_io_connected)
			return false;

		auto ec = sendMsg(msg, iostream);
		if (ec) {
			WARN_LOG(INPUT, "DreamConn[%d] send failed: %s", bus, ec.message().c_str());
			disconnect();
			return false;
		}
		return true;
	}

    bool sendReceive(const MapleMsg& txMsg, MapleMsg& rxMsg) override
    {
		if (!send(txMsg))
			return false;

		if (!receiveMsg(rxMsg, iostream)) {
			WARN_LOG(INPUT, "DreamConn[%d] receive failed", bus);
			disconnect();
			return false;
		}
		return true;
	}

	void changeBus(int newBus) override
	{
		if (newBus != bus) {
			// A different TCP port is used depending on the bus. We'll need to disconnect from the current port.
			// The caller will call connect() again if appropriate.
			disconnect();
			bus = newBus;
		}
	}

	bool isConnected() override {
		return maple_io_connected;
	}

	void connect() override
	{
		maple_io_connected = false;
		if (!DreamLink::isValidPort(bus))
			return;

		iostream = asio::ip::tcp::iostream("localhost", std::to_string(BASE_PORT + bus));
		if (!iostream) {
			WARN_LOG(INPUT, "DreamConn[%d] connection failed: %s", bus, iostream.error().message().c_str());
			disconnect();
			return;
		}
		// Optimistically assume we are connected to the maple server. If a send fails we will just set this flag back to false.
		maple_io_connected = true;

		iostream.expires_from_now(std::chrono::seconds(1));
		// Now get the controller configuration
		MapleMsg msg;
		msg.command = MDCF_GetCondition;
		msg.destAP = (bus << 6) | 0x20;
		msg.originAP = bus << 6;
		msg.pushData(MFID_0_Input);
		MapleMsg rxMsg;
		if (!sendReceive(msg, rxMsg))
			return;
		iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow

		config::MapleExpansionDevices[bus][0] = expansionDevs[0] = rxMsg.originAP & 1 ? MDT_SegaVMU : MDT_None;
		config::MapleExpansionDevices[bus][1] = expansionDevs[1] = rxMsg.originAP & 2 ? MDT_PurupuruPack : MDT_None;
		if (expansionDevs[0] == MDT_SegaVMU)
			registerLink(bus, 0);
		if (expansionDevs[1] == MDT_PurupuruPack)
			registerLink(bus, 1);

		NOTICE_LOG(INPUT, "Connected to DreamConn[%d]: Slot 1: %s, Slot 2: %s", bus,
				deviceDescription(expansionDevs[0]), deviceDescription(expansionDevs[1]));
	}

	static const char* deviceDescription(MapleDeviceType deviceType) {
		switch (deviceType) {
			case MDT_None: return "None";
			case MDT_SegaVMU: return "Sega VMU";
			case MDT_PurupuruPack: return "Vibration Pack";
			default: return "Unknown"; // note: we don't expect to reach this path, unless something has really gone wrong (e.g. somehow garbage data was written to `expansionDevs`).
		}
	}

	void disconnect() override
	{
		// Already disconnected
		if (!maple_io_connected)
			return;
		maple_io_connected = false;

		unregisterLink(bus, 0);
		unregisterLink(bus, 1);
		if (iostream)
			iostream.close();

		// Notify the user of the disconnect
		NOTICE_LOG(INPUT, "Disconnected from DreamConn[%d]", bus);
		char buf[128];
		snprintf(buf, sizeof(buf), i18n::T("WARNING: DreamConn disconnected from port %c"), 'A' + bus);
		os_notify(buf, 6000);
	}
};

bool DreamConnGamepad::identify(int deviceIndex)
{
	char guid_str[33] {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid_str, sizeof(guid_str));
	// DreamConn VID:4457 PID:4443
	const char* pid_vid_guid_str = guid_str + 8;
	if (memcmp(VID_PID_GUID, pid_vid_guid_str, 16) == 0) {
		NOTICE_LOG(INPUT, "DreamConn controller found!");
		return true;
	}
	return false;
}

DreamConnGamepad::DreamConnGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: DreamLinkGamepad(std::make_shared<DreamConn>(maple_port), maple_port, joystick_idx, sdl_joystick)
{
	_name = "DreamConn+ / DreamConn S Controller";
}

#endif // USE_DREAMCONN
