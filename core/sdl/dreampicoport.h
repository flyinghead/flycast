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

#include "dreamlink.h"

#ifdef USE_DREAMCASTCONTROLLER

#include <asio.hpp>

#include <atomic>
#include <chrono>
#include <vector>
#include <array>

// Forward declaration of underlying serial connection
class DreamPicoPortSerialHandler;

//! See: https://github.com/OrangeFox86/DreamPicoPort
class DreamPicoPort : public DreamLink
{
	u8 expansionDevs = 0;

	//! The one and only serial port
	static std::unique_ptr<DreamPicoPortSerialHandler> serial;
	//! Number of devices using the above serial
	static std::atomic<std::uint32_t> connected_dev_count;
	//! Current timeout in milliseconds
	std::chrono::milliseconds timeout_ms;
	//! The bus ID dictated by flycast
	int software_bus = -1;
	//! The bus index of the hardware connection which will differ from the software bus
	int hardware_bus = -1;
	//! true iff only a single devices was found when enumerating devices
	bool is_single_device = true;
	//! True when initial enumeration failed
	bool is_hardware_bus_implied = true;
	//! True once connection is established
	bool connection_established = false;
    //! The queried interface version
    double interface_version = 0.0;
    //! The queried peripherals; for each function, index 0 is function code and index 1 is the function definition
    std::vector<std::vector<std::array<uint32_t, 2>>> peripherals;

public:
    //! Dreamcast Controller USB VID:1209 PID:2f07
    static constexpr const std::uint16_t VID = 0x1209;
    static constexpr const std::uint16_t PID = 0x2f07;
    static constexpr const char* VID_PID_GUID = "09120000072f0000";

public:
    DreamPicoPort(int bus, int joystick_idx, SDL_Joystick* sdl_joystick);

	virtual ~DreamPicoPort();

	bool send(const MapleMsg& msg) override;

    bool send(const MapleMsg& txMsg, MapleMsg& rxMsg) override;

	void gameTermination() override;

	int getBus() const override;

    u32 getFunctionCode(int forPort) const override;

	std::array<u32, 3> getFunctionDefinitions(int forPort) const override;

	int getDefaultBus() const override;

	void setDefaultMapping(const std::shared_ptr<InputMapping>& mapping) const override;

	void changeBus(int newBus);

	std::string getName() const override;

	void connect() override;

	void disconnect() override;

    void sendPort();

	int hardwareBus() const;

	bool isHardwareBusImplied() const;

	bool isSingleDevice() const;

private:
    asio::error_code sendCmd(const std::string& cmd);
    asio::error_code sendMsg(const MapleMsg& msg);
    asio::error_code receiveCmd(std::string& cmd);
    asio::error_code receiveMsg(MapleMsg& msg);
	void determineHardwareBus(int joystick_idx, SDL_Joystick* sdl_joystick);
    bool queryInterfaceVersion();
    bool queryPeripherals();
};

#endif // USE_DREAMCASTCONTROLLER
