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
#include <iomanip>
#include <sstream>
#include <locale>
#include <mutex>

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
#include <dirent.h>
#endif

static asio::error_code sendMsg(const MapleMsg& msg, asio::ip::tcp::iostream& stream)
{
	std::ostringstream s;
	s.imbue(std::locale::classic());
	s.fill('0');
	s << std::hex << std::uppercase
		<< std::setw(2) << (u32)msg.command << " "
		<< std::setw(2) << (u32)msg.destAP << " "
		<< std::setw(2) << (u32)msg.originAP << " "
		<< std::setw(2) << (u32)msg.size;
	const u32 sz = msg.getDataSize();
	for (u32 i = 0; i < sz; i++)
		s << " " << std::setw(2) << (u32)msg.data[i];
	s << "\r\n";

	asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(stream.socket());
	asio::error_code ec;
	asio::write(sock, asio::buffer(s.str()), ec);
	return ec;
}

static bool receiveMsg(MapleMsg& msg, std::istream& stream)
{
	std::string response;
	if (!std::getline(stream, response))
		return false;
	sscanf(response.c_str(), "%hhx %hhx %hhx %hhx", &msg.command, &msg.destAP, &msg.originAP, &msg.size);
	if ((msg.getDataSize() - 1) * 3 + 13 >= response.length())
		return false;
	for (unsigned i = 0; i < msg.getDataSize(); i++)
		sscanf(&response[i * 3 + 12], "%hhx", &msg.data[i]);
	return !stream.fail();
}

//! DreamConn implementation class
//! This is here mainly so asio.hpp can be included in this source file instead of the header.
class DreamConnImp : public DreamConn
{
	int bus = -1;
	const bool _isForPhysicalController;
	bool maple_io_connected = false;
	std::array<MapleDeviceType, 2> expansionDevs{};
	asio::ip::tcp::iostream iostream;
	std::mutex send_mutex;

public:
	DreamConnImp(int bus, bool isForPhysicalController) :
		DreamConn(),
		bus(bus),
		_isForPhysicalController(isForPhysicalController)
	{}

	~DreamConnImp() {
		disconnect();
	}

	bool isForPhysicalController() override {
		return _isForPhysicalController;
	}

	bool send(const MapleMsg& msg) override {
		std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations
		return send_no_lock(msg);
	}

    bool send(const MapleMsg& txMsg, MapleMsg& rxMsg) override {
		std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations
		return send_no_lock(txMsg, rxMsg);
	}

private:
	bool send_no_lock(const MapleMsg& msg) {
		if (!maple_io_connected)
			return false;

		auto ec = sendMsg(msg, iostream);
		if (ec) {
			WARN_LOG(INPUT, "DreamcastController[%d] send failed: %s", bus, ec.message().c_str());
			disconnect();
			return false;
		}
		return true;
	}

	bool send_no_lock(const MapleMsg& txMsg, MapleMsg& rxMsg) {
		if (!send_no_lock(txMsg)) {
			return false;
		}

		return receiveMsg(rxMsg, iostream);
	}

public:
	int getBus() const override {
		return bus;
	}

    u32 getFunctionCode(int forPort) const override {
		MapleDeviceType deviceType = expansionDevs.at(forPort);
		if (deviceType == MDT_SegaVMU) {
			return 0x0E000000;
		}
		else if (deviceType == MDT_PurupuruPack) {
			return 0x00010000;
		}
		return 0;
	}

	std::array<u32, 3> getFunctionDefinitions(int forPort) const override {
		MapleDeviceType deviceType = expansionDevs.at(forPort);
		if (deviceType == MDT_SegaVMU)
			// For clock, LCD, storage
			return std::array<u32, 3>{0x403f7e7e, 0x00100500, 0x00410f00};
		else if (deviceType == MDT_PurupuruPack)
			return std::array<u32, 3>{0x00000101, 0, 0};

		return std::array<u32, 3>{0, 0, 0};
	}

	void changeBus(int newBus) override {
		if (newBus != bus) {
			// A different TCP port is used depending on the bus. We'll need to disconnect from the current port.
			// The caller will call connect() again if appropriate.
			disconnect();
			bus = newBus;
		}
	}

	std::string getName() const override {
		return "DreamConn+ / DreamConn S Controller";
	}

	bool needsRefresh() override {
		if (!isConnected())
			return false;

		std::lock_guard<std::mutex> lock(send_mutex);

		// Check if there is a refresh message waiting in the socket buffer.
		// Avoid reading (consuming) any other kind of message in this context.
		const int REFRESH_MESSAGE_SIZE = 13;
		asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(iostream.socket());
		if (sock.available() < REFRESH_MESSAGE_SIZE)
			return false;

		char buffer[REFRESH_MESSAGE_SIZE];
		int bytesPeeked = recv(sock.native_handle(), buffer, REFRESH_MESSAGE_SIZE, MSG_PEEK);
		if (bytesPeeked != REFRESH_MESSAGE_SIZE)
			return false;

		MapleMsg message;
		sscanf(buffer, "%hhx %hhx %hhx %hhx", &message.command, &message.destAP, &message.originAP, &message.size);
		if (message.command == 0xff && message.destAP == 0xff && message.originAP == 0xff && message.size == 0xff) {
			// It is a refresh message, so consume it.
			receiveMsg(message, iostream);

			if (!updateExpansionDevs_no_lock())
				return false;

			return true;
		}
		return false;
	}

	bool isConnected() override {
		if (!maple_io_connected)
			return false;

		if (isSocketDisconnected()) {
			NOTICE_LOG(INPUT, "DreamLink server disconnected bus[%d]", bus);
			disconnect();
			return false;
		}

		return true;
	}

	void connect() override {
		maple_io_connected = false;

#if !defined(_WIN32)
		if (isForPhysicalController()) {
			WARN_LOG(INPUT, "DreamcastController[%d] connection failed: DreamConn+ / DreamConn S Controller supported on Windows only", bus);
			return;
		}
#endif

		iostream = asio::ip::tcp::iostream("localhost", std::to_string(DreamConn::BASE_PORT + bus));
		if (!iostream) {
			WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, iostream.error().message().c_str());
			disconnect();
			return;
		}
		// Optimistically assume we are connected to the maple server. If a send fails we will just set this flag back to false.
		maple_io_connected = true;
		iostream.expires_from_now(std::chrono::seconds(1));

		if (!updateExpansionDevs_no_lock())
			return;

		iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow

		// Remain connected even if no devices were found, so that connecting a device later will be detected
		NOTICE_LOG(INPUT, "Connected to DreamcastController[%d]: Type:%s, Slot 1: %s, Slot 2: %s", bus, getName().c_str(), deviceDescription(expansionDevs[0]), deviceDescription(expansionDevs[1]));
	}

	static const char* deviceDescription(MapleDeviceType deviceType) {
		switch (deviceType) {
			case MDT_None: return "None";
			case MDT_SegaVMU: return "Sega VMU";
			case MDT_PurupuruPack: return "Vibration Pack";
			default: return "Unknown"; // note: we don't expect to reach this path, unless something has really gone wrong (e.g. somehow garbage data was written to `expansionDevs`).
		}
	}

	void disconnect() override {
		// Already disconnected
		if (!maple_io_connected)
			return;

		maple_io_connected = false;
		if (iostream)
			iostream.close();

		// Notify the user of the disconnect
		NOTICE_LOG(INPUT, "Disconnected from DreamcastController[%d]", bus);
		char buf[128];
		snprintf(buf, sizeof(buf), i18n::T("WARNING: DreamLink disconnected from port %c"), 'A' + bus);
		os_notify(buf, 6000);

		tearDownDreamLinkDevices(shared_from_this());
		maple_ReconnectDevices();
	}

	void gameTermination() override {
		clearScreen(0);
		clearScreen(1);
	}

private:
	void clearScreen(int deviceIndex) {
		if (expansionDevs[deviceIndex] != MDT_SegaVMU)
			return;

		// Clear the remote VMU screen
		MapleMsg msg;
		msg.command = MDCF_BlockWrite;
		msg.destAP = (bus << 6) | 0x20 | (1 << deviceIndex);
		msg.originAP = bus << 6;

		u32 localData[0x32];
		memset(localData, 0, sizeof(localData));
		localData[0] = MFID_2_LCD;
		msg.setData(localData);

		send(msg);
	}

	bool updateExpansionDevs_no_lock() {
		// Now get the controller configuration
		MapleMsg msg;
		msg.command = MDCF_GetCondition;
		msg.destAP = (bus << 6) | 0x20;
		msg.originAP = bus << 6;
		msg.setData(MFID_0_Input);

		auto ec = sendMsg(msg, iostream);
		if (ec)
		{
			WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, ec.message().c_str());
			disconnect();
			return false;
		}
		if (!receiveMsg(msg, iostream)) {
			WARN_LOG(INPUT, "DreamcastController[%d] read timeout", bus);
			disconnect();
			return false;
		}

		u8 portFlags = msg.originAP & 0x1f;
		config::MapleExpansionDevices[bus][0] = expansionDevs[0] = getDevice_no_lock(portFlags, 1 << 0);
		config::MapleExpansionDevices[bus][1] = expansionDevs[1] = getDevice_no_lock(portFlags, 1 << 1);
		return true;
	}

	MapleDeviceType getDevice_no_lock(u8 portFlags, u8 portFlag) {
		if (!(portFlags & portFlag)) {
			// This is the case where nothing is connected to the expansion slot.
			// We should not send a DeviceRequest message in that case.
			return MDT_None;
		}

		MapleMsg txMsg;
		txMsg.command = MDC_DeviceRequest;
		txMsg.destAP = (bus << 6) | portFlag;
		txMsg.originAP = bus << 6;
		txMsg.size = 0;

		MapleMsg rxMsg;
		if (!send_no_lock(txMsg, rxMsg)) {
			return MDT_None;
		}

		// 32-bit words are in little-endian format on the wire
		const u32 fnCode = (rxMsg.data[0] << 0) | (rxMsg.data[1] << 8) | (rxMsg.data[2] << 16) | (rxMsg.data[3] << 24);
		if (fnCode & MFID_1_Storage) {
			return MDT_SegaVMU;
		}
		else if (fnCode & MFID_8_Vibration) {
			return MDT_PurupuruPack;
		}
		else {
			WARN_LOG(INPUT, "DreamcastController[%d] MDC_DeviceRequest unsupported function code: 0x%x", bus, fnCode);
			return MDT_None;
		}
	}

	bool isSocketDisconnected() {
		std::lock_guard<std::mutex> lock(send_mutex);

		// A socket was disconnected if 'select()' says the socket is ready to read, and a subsequent 'recv()' fails or says 0 bytes available to read.
		auto& sock = static_cast<asio::ip::tcp::socket&>(iostream.socket());
		auto nativeHandle = sock.native_handle();
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(nativeHandle, &readfds);
		timeval timeout = { 0, 0 };
		// nfds should be set to the highest-numbered file descriptor plus 1.
		// See https://www.man7.org/linux/man-pages/man2/select.2.html
		int nfds = nativeHandle + 1;
		int nReady = select(nfds, &readfds, nullptr, nullptr, &timeout);
		bool socketIsReady = nReady > 0 && FD_ISSET(nativeHandle, &readfds);
		if (!socketIsReady)
			return false;

		char dest;
		int len = recv(nativeHandle, &dest, sizeof(dest), MSG_PEEK);
		return len <= 0;
	}
};

std::shared_ptr<DreamConn> DreamConn::create_shared(int bus, bool isForPhysicalController) {
	return std::make_shared<DreamConnImp>(bus, isForPhysicalController);
}
