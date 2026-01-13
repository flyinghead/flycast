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
#pragma once

#ifdef USE_DREAMLINK_DEVICES

// This file contains abstraction layer for access to different kinds of remote peripherals.
// This includes both real Dreamcast controllers, VMUs, jump packs etc. but also emulated VMUs.

#include "types.h"
#include "emulator.h"
#include "sdl_gamepad.h"

#include <functional>
#include <memory>
#include <array>

struct MapleMsg
{
	u8 command = 0;
	u8 destAP = 0;
	u8 originAP = 0;
	u8 size = 0;
	u8 data[1024];

	u32 getDataSize() const {
		return size * 4;
	}

	template<typename T>
	void setData(const T& p) {
		memcpy(data, &p, sizeof(T));
		this->size = (sizeof(T) + 3) / 4;
	}

	void setWord(const u32& p, int index) {
		if (index < 0 || index >= 256) {
			return;
		}
		memcpy(&data[index * 4], &p, sizeof(u32));
		if (this->size <= index) {
			this->size = index + 1;
		}
	}
};
static_assert(sizeof(MapleMsg) == 1028);

// Abstract base class for communication with physical controllers and remote expansion devices
class DreamLink : public std::enable_shared_from_this<DreamLink>
{
public:
	//! Number of physical dreamcast ports
	static constexpr const int NUM_PORTS = 4;
	// The active DreamLink, if any, for each port.
	// Note that multiple gamepad DreamLinks may exist for a given port, in which case only one is active.
	static std::array<std::shared_ptr<DreamLink>, NUM_PORTS> activeDreamLinks;

	DreamLink() = default;

	virtual ~DreamLink() = default;

	//! Check if a given port is valid
	//! @param[in] port The dreamcast port index to test
	//! @return true iff port is a valid physical port
	static bool isValidPort(int port) {
		return (port >= 0 && port < NUM_PORTS);
	}

	virtual bool isForPhysicalController() = 0;

	virtual bool isPhysicalVMUMemorySupported() = 0;

	//! Sends a message to the controller, ignoring the response
	//! @note The implementation shall be thread safe
	virtual bool send(const MapleMsg& msg) = 0;

	//! Sends a message to the controller and waits for a response
	//! @note The implementation shall be thread safe
    virtual bool send(const MapleMsg& txMsg, MapleMsg& rxMsg) = 0;

	//! When called, do teardown stuff like reset screen
	virtual inline void gameTermination() {}

    //! @param[in] forPort The port number to get the function code of
    //! @return the device type for the given port
    virtual u32 getFunctionCode(int forPort) const = 0;

    //! @param[in] forPort The port number to get the function definitions of
	//! @return the 3 function definitions for the supported function codes
    virtual std::array<u32, 3> getFunctionDefinitions(int forPort) const = 0;

	//! @return the default bus number to select for this controller or -1 to not select a default
	virtual int getDefaultBus() const {
		return -1;
	}

	//! Allows a DreamLink device to dictate the default mapping
	virtual void setDefaultMapping(const std::shared_ptr<InputMapping>& mapping) const {
	}

	//! Allows button names to be defined by a DreamLink device
	//! @param[in] code The button code to retrieve name of
	//! @return the button name for the given code to override what is defined by the gamepad
	//! @return nullptr to fall back to gamepad definitions
	virtual const char *getButtonName(u32 code) const {
		return nullptr;
	}

	//! Allows axis names to be defined by a DreamLink device
	//! @param[in] code The axis code to retrieve name of
	//! @return the axis name for the given code to override what is defined by the gamepad
	//! @return nullptr to fall back to gamepad definitions
	virtual const char *getAxisName(u32 code) const {
		return nullptr;
	}

	//! @return a unique ID for this DreamLink device or empty string to use default
	virtual std::string getUniqueId() const {
		return std::string();
	}

	//! @return the selected bus number of the controller
	virtual int getBus() const = 0;

	//! Changes the selected maple port is changed by the user
	virtual void changeBus(int newBus) = 0;

	//! @return the display name of the controller
	virtual std::string getName() const = 0;

	//! Fetch the latest remote device configuration and return 'true' if it changed.
	//! Caller is responsible for recreating the actual devices.
	virtual bool needsRefresh() = 0;

	//! Returns true if connected to the hardware controller (TODO: "hardware controller or remote device" throughout?)
	virtual bool isConnected() = 0;

	//! Attempt connection to the hardware controller
	virtual void connect() = 0;

	//! Disconnect from the hardware controller
	virtual void disconnect() = 0;

	//! Sends the current game id to a DreamLink backed expansion device if supported
	virtual void sendGameId(int expansion, const std::string& gameId) {}
};

class DreamLinkGamepad : public SDLGamepad
{
public:
    DreamLinkGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick);
	~DreamLinkGamepad();

	const char* dreamLinkStatus();
	void set_maple_port(int port) override;
	void registered() override;
	static bool isDreamcastController(int deviceIndex);
	void resetMappingToDefault(bool arcade, bool gamepad) override;
	const char *get_button_name(u32 code) override;
	const char *get_axis_name(u32 code) override;

protected:
	std::shared_ptr<InputMapping> getDefaultMapping() override;
	void setBaseDefaultMapping(const std::shared_ptr<InputMapping>& mapping) const;

	std::shared_ptr<DreamLink> dreamlink;
	bool ltrigPressed = false;
	bool rtrigPressed = false;
	bool startPressed = false;
	std::string device_guid;
};

// Creates and destroys DreamLinks according to config settings.
// Attempts to connect/reconnect DreamLinks which are not connected.
// Returns true if any new connection was established.
// Note that this doesn't createDreamLinkDevices for DreamLinks created by this function. Caller is responsible for that.
bool reconnectDreamLinks();

void refreshDreamLinksIfNeeded();
void createAllDreamLinkDevices();
void createDreamLinkDevices(std::shared_ptr<DreamLink> dreamlink, bool gameStart, bool stateLoaded);
void tearDownDreamLinkDevices(DreamLink* dreamlink);

void registerDreamLinkEvents();
void unregisterDreamLinkEvents();

#endif // USE_DREAMLINK_DEVICES
