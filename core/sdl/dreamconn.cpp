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
#include "dreamconn.h"

#ifdef USE_DREAMCASTCONTROLLER
#include "hw/maple/maple_devs.h"
#include "ui/gui.h"
#include <cfg/option.h>
#include <SDL.h>
#include <asio.hpp>
#include <iomanip>
#include <sstream>
#include <optional>
#include <thread>
#include <list>

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
#include <dirent.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <setupapi.h>
#endif

void createDreamConnDevices(std::shared_ptr<DreamConn> dreamconn, bool gameStart);

class DreamcastControllerConnection
{
protected:
	//! The maple bus index [0,3]
	const int bus;

public:
	DreamcastControllerConnection(const DreamcastControllerConnection&) = delete;
	DreamcastControllerConnection() = delete;

	explicit DreamcastControllerConnection(int bus) : bus(bus)
	{}

	std::optional<MapleMsg> connect(){
		if (!establishConnection()) {
			return std::nullopt;
		}

		// Now get the controller configuration
		MapleMsg msg;
		msg.command = MDCF_GetCondition;
		msg.destAP = (bus << 6) | 0x20;
		msg.originAP = bus << 6;
		msg.setData(MFID_0_Input);

		asio::error_code ec = sendMsg(msg);
		if (ec)
		{
			WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, ec.message().c_str());
			disconnect();
			return std::nullopt;
		}
		if (!receiveMsg(msg)) {
			WARN_LOG(INPUT, "DreamcastController[%d] read timeout", bus);
			disconnect();
			return std::nullopt;
		}

		onConnectComplete();

		return msg;
	}

	virtual void disconnect() = 0;
	virtual asio::error_code sendMsg(const MapleMsg& msg) = 0;
	virtual bool receiveMsg(MapleMsg& msg) = 0;

protected:
	virtual bool establishConnection() = 0;
	virtual void onConnectComplete() = 0;

	std::string msgToString(const MapleMsg& msg, const std::string& delim = " ")
	{
		std::ostringstream s;
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

		return s.str();
	}
};

class DreamConnConnection : public DreamcastControllerConnection
{
	//! Base port of communication to DreamConn
	static constexpr u16 BASE_PORT = 37393;
	//! Stream to a DreamConn device
	asio::ip::tcp::iostream iostream;

public:
	//! DreamConn VID:4457 PID:4443
	static constexpr const char* VID_PID_GUID = "5744000043440000";

public:
	DreamConnConnection(const DreamConnConnection&) = delete;
	DreamConnConnection() = delete;

	explicit DreamConnConnection(int bus) : DreamcastControllerConnection(bus)
	{}

	~DreamConnConnection() {
		disconnect();
	}

	bool establishConnection() override {
#if !defined(_WIN32)
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: DreamConn+ / DreamConn S Controller supported on Windows only", bus);
		return false;
#else
		iostream = asio::ip::tcp::iostream("localhost", std::to_string(BASE_PORT + bus));
		if (!iostream) {
			WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, iostream.error().message().c_str());
			disconnect();
			return false;
		}
		iostream.expires_from_now(std::chrono::seconds(1));
		return true;
#endif
	}

	void onConnectComplete() override {
		iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow
	}

	void disconnect() override
	{
		if (iostream) {
			iostream.close();
		}
	}

	asio::error_code sendMsg(const MapleMsg& msg) override
	{
		const std::string msgStr = msgToString(msg);
		asio::error_code ec;

		if (!iostream) {
			return asio::error::not_connected;
		}
		asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(iostream.socket());
		asio::write(sock, asio::buffer(msgStr), ec);

		return ec;
	}

	bool receiveMsg(MapleMsg& msg) override
	{
		std::string response;

		if (!std::getline(iostream, response))
			return false;
		sscanf(response.c_str(), "%hhx %hhx %hhx %hhx", &msg.command, &msg.destAP, &msg.originAP, &msg.size);
		if ((msg.getDataSize() - 1) * 3 + 13 >= response.length())
			return false;
		for (unsigned i = 0; i < msg.getDataSize(); i++)
			sscanf(&response[i * 3 + 12], "%hhx", &msg.data[i]);
		return !iostream.fail();

		return false;
	}
};

//! See: https://github.com/OrangeFox86/DreamcastControllerUsbPico
class DreamcastControllerUsbPicoConnection : public DreamcastControllerConnection
{
	//! Asynchronous context for serial_handler
	asio::io_context io_context;
	//! Output buffer data for serial_handler
	std::string serial_out_data;
	//! Handles communication to DreamcastControllerUsbPico
	asio::serial_port serial_handler{io_context};
	//! Set to true while an async write is in progress with serial_handler
	bool serial_write_in_progress = false;
	//! Signaled when serial_write_in_progress transitions to false
	std::condition_variable write_cv;
	//! Mutex for write_cv and serializes access to serial_write_in_progress
	std::mutex write_cv_mutex;
	//! Input stream buffer from serial_handler
	asio::streambuf serial_read_buffer;
	//! Thread which runs the io_context
	std::unique_ptr<std::thread> io_context_thread;
	//! Contains queue of incoming lines from serial
	std::list<std::string> read_queue;
	//! Signaled when data is in read_queue
	std::condition_variable read_cv;
	//! Mutex for read_cv and serializes access to read_queue
	std::mutex read_cv_mutex;
	//! Current timeout in milliseconds
	std::chrono::milliseconds timeout_ms;

public:
	//! Dreamcast Controller USB VID:1209 PID:2f07
	static constexpr const char* VID_PID_GUID = "09120000072f0000";

public:
	DreamcastControllerUsbPicoConnection(const DreamcastControllerUsbPicoConnection&) = delete;
	DreamcastControllerUsbPicoConnection() = delete;

	explicit DreamcastControllerUsbPicoConnection(int bus) : DreamcastControllerConnection(bus)
	{}

	~DreamcastControllerUsbPicoConnection(){
		disconnect();
	}

	bool establishConnection() override {
		asio::error_code ec;

		// Timeout is 1 second while establishing connection
		timeout_ms = std::chrono::seconds(1);

		// the serial port isn't ready at this point, so we need to sleep briefly
		// we probably should have a better way to handle this
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		serial_handler = asio::serial_port(io_context);
		io_context.reset();

		std::string serial_device = "";

		// use user-configured serial device if available, fallback to first available
		if (cfgLoadStr("input", "DreamcastControllerUsbSerialDevice", "default") != "default") {
			serial_device = cfgLoadStr("input", "DreamcastControllerUsbSerialDevice", "default");
			INFO_LOG(INPUT, "DreamcastController[%d] connecting to user-configured serial device: %s", bus, serial_device.c_str());
		}
		else
		{
			serial_device = getFirstSerialDevice();
			INFO_LOG(INPUT, "DreamcastController[%d] connecting to autoselected serial device: %s", bus, serial_device.c_str());
		}

		serial_handler.open(serial_device, ec);

		if (ec || !serial_handler.is_open()) {
			WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, ec.message().c_str());
			disconnect();
			return false;
		}

		// This must be done before the io_context is run because it will keep io_context from returning immediately
		startSerialRead();

		io_context_thread = std::make_unique<std::thread>([this](){contextThreadEnty();});

		return true;
	}

	void onConnectComplete() override {
		// Timeout is extended to 5 seconds for all other communication after connection
		timeout_ms = std::chrono::seconds(5);
	}

	void disconnect() override
	{
		io_context.stop();

		if (serial_handler.is_open()) {
			try
			{
				serial_handler.cancel();
			}
			catch(const asio::system_error&)
			{
				// Ignore cancel errors
			}
		}

		try
		{
			serial_handler.close();
		}
		catch(const asio::system_error&)
		{
			// Ignore closing errors
		}
	}

	void contextThreadEnty()
	{
		// This context should never exit until disconnect due to read handler automatically rearming
		io_context.run();
	}

	asio::error_code sendMsg(const MapleMsg& msg) override
	{
		asio::error_code ec;

		if (!serial_handler.is_open()) {
			return asio::error::not_connected;
		}

		// Wait for last write to complete
		std::unique_lock<std::mutex> lock(write_cv_mutex);
		const std::chrono::steady_clock::time_point expiration = std::chrono::steady_clock::now() + timeout_ms;
		if (!write_cv.wait_until(lock, expiration, [this](){return (!serial_write_in_progress || !serial_handler.is_open());}))
		{
			return asio::error::timed_out;
		}

		// Check again before continuing
		if (!serial_handler.is_open()) {
			return asio::error::not_connected;
		}

		// Clear out the read buffer before writing next command
		read_queue.clear();

		serial_write_in_progress = true;
		// Messages to Dreamcast Controller USB need to be prefixed to trigger the correct parser
		serial_out_data = std::string("X ") + msgToString(msg);
		asio::async_write(serial_handler, asio::buffer(serial_out_data), asio::transfer_exactly(serial_out_data.size()), [this](const asio::error_code& error, size_t bytes_transferred)
		{
			std::unique_lock<std::mutex> lock(write_cv_mutex);
			if (error) {
				try
				{
					serial_handler.cancel();
				}
				catch(const asio::system_error&)
				{
					// Ignore cancel errors
				}
			}
			serial_write_in_progress = false;
			write_cv.notify_all();
		});

		return ec;
	}

	bool receiveMsg(MapleMsg& msg) override
	{
		std::string response;

		// Wait for at least 2 lines to be received (first line is echo back)
		std::unique_lock<std::mutex> lock(read_cv_mutex);
		const std::chrono::steady_clock::time_point expiration = std::chrono::steady_clock::now() + timeout_ms;
		if (!read_cv.wait_until(lock, expiration, [this](){return ((read_queue.size() >= 2) || !serial_handler.is_open());}))
		{
			// Timeout
			return false;
		}

		if (read_queue.size() < 2) {
			// Connection was closed before data could be received
			return false;
		}

		// discard the first message as we are interested in the second only which returns the controller configuration
		response = read_queue.back();

		sscanf(response.c_str(), "%hhx %hhx %hhx %hhx", &msg.command, &msg.destAP, &msg.originAP, &msg.size);

		if (serial_handler.is_open()) {
			return true;
		}
		else {
			return false;
		}

		return false;
	}

private:
	static std::string getFirstSerialDevice() {

		// On Windows, we get the first serial device matching our VID/PID
#if defined(_WIN32)
		HDEVINFO deviceInfoSet = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
		if (deviceInfoSet == INVALID_HANDLE_VALUE) {
			return "";
		}

		SP_DEVINFO_DATA deviceInfoData;
		deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

		for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
			DWORD dataType, bufferSize = 0;
			SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, &dataType, NULL, 0, &bufferSize);

			if (bufferSize > 0) {
				std::vector<char> buffer(bufferSize);
				if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, &dataType, (PBYTE)buffer.data(), bufferSize, NULL)) {
					std::string hardwareId(buffer.begin(), buffer.end());
					if (hardwareId.find("VID_1209") != std::string::npos && hardwareId.find("PID_2F07") != std::string::npos) {
						HKEY deviceKey = SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
						if (deviceKey != INVALID_HANDLE_VALUE) {
							char portName[256];
							DWORD portNameSize = sizeof(portName);
							if (RegQueryValueEx(deviceKey, "PortName", NULL, NULL, (LPBYTE)portName, &portNameSize) == ERROR_SUCCESS) {
								RegCloseKey(deviceKey);
								SetupDiDestroyDeviceInfoList(deviceInfoSet);
								return std::string(portName);
							}
							RegCloseKey(deviceKey);
						}
					}
				}
			}
		}

		SetupDiDestroyDeviceInfoList(deviceInfoSet);
		return "";
#endif

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
	// On MacOS/Linux, we get the first serial device matching the device prefix
	std::string device_prefix = "";

#if defined(__linux__)
		device_prefix = "ttyACM";
#elif (defined(__APPLE__) && defined(TARGET_OS_MAC))
		device_prefix = "tty.usbmodem";
#endif

		std::string path = "/dev/";
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir(path.c_str())) != NULL) {
			while ((ent = readdir(dir)) != NULL) {
				std::string device = ent->d_name;
				if (device.find(device_prefix) != std::string::npos) {
					closedir(dir);
					return path + device;
				}
			}
			closedir(dir);
		}
		return "";
#endif
	}

	void startSerialRead()
	{
		serialReadHandler(asio::error_code(), 0);
		// Just to make sure initial data is cleared off of incoming buffer
		io_context.poll_one();
		read_queue.clear();
	}

	void serialReadHandler(const asio::error_code& error, std::size_t size)
	{
		if (error) {
			std::lock_guard<std::mutex> lock(read_cv_mutex);
			try
			{
				serial_handler.cancel();
			}
			catch(const asio::system_error&)
			{
				// Ignore cancel errors
			}
			read_cv.notify_all();
		}
		else {
			// Rearm the read
			asio::async_read_until(
				serial_handler,
				serial_read_buffer,
				'\n',
				[this](const asio::error_code& error, std::size_t size) -> void {
					if (size > 0)
					{
						// Lock access to read_queue
						std::lock_guard<std::mutex> lock(read_cv_mutex);
						// Consume the received data
						if (consumeReadBuffer() > 0)
						{
							// New lines available
							read_cv.notify_all();
						}
					}
					// Auto reload read - io_context will always have work to do
					serialReadHandler(error, size);
				}
			);
		}
	}

	int consumeReadBuffer() {
		if (serial_read_buffer.size() <= 0) {
			return 0;
		}

		int numberOfLines = 0;
		while (true)
		{
			char c = '\0';
			std::string line;

			// Consume characters until buffers are empty or \n found
			asio::const_buffers_1 data = serial_read_buffer.data();
			std::size_t consumed = 0;
			for (const asio::const_buffer& buff : data)
			{
				const char* buffDat = static_cast<const char*>(buff.data());
				for (std::size_t i = 0; i < buff.size(); ++i)
				{
					c = *buffDat++;
					++consumed;

					if (c == '\n') {
						// Stop reading now
						break;
					}

					line += c;
				}

				if (c == '\n') {
					// Stop reading now
					break;
				}
			}

			if (c == '\n') {
				serial_read_buffer.consume(consumed);

				// Remove carriage return if found and add this line to queue
				if (line.size() > 0 && line[line.size() - 1] == '\r') {
					line.pop_back();
				}
				read_queue.push_back(std::move(line));

				++numberOfLines;
			}
			else {
				// Ran out of data to consume
				return numberOfLines;
			}
		}
	}
};

DreamConn::DreamConn(int bus, int dreamcastControllerType) : bus(bus), dreamcastControllerType(dreamcastControllerType) {
	switch (dreamcastControllerType)
	{
		case TYPE_DREAMCONN:
			dcConnection = std::make_unique<DreamConnConnection>(bus);
			break;

		case TYPE_DREAMCASTCONTROLLERUSB:
			dcConnection = std::make_unique<DreamcastControllerUsbPicoConnection>(bus);
			break;
	}

	connect();
}

DreamConn::~DreamConn() {
	disconnect();
}

void DreamConn::connect()
{
	maple_io_connected = false;
	expansionDevs = 0;

	if (!dcConnection) {
		return;
	}

	std::optional<MapleMsg> msg = dcConnection->connect();
	if (!msg)
	{
		return;
	}

	expansionDevs = msg->originAP & 0x1f;

	config::MapleExpansionDevices[bus][0] = hasVmu() ? MDT_SegaVMU : MDT_None;
	config::MapleExpansionDevices[bus][1] = hasRumble() ? MDT_PurupuruPack : MDT_None;

	if (hasVmu() || hasRumble())
	{
		NOTICE_LOG(INPUT, "Connected to DreamcastController[%d]: Type:%s, VMU:%d, Rumble Pack:%d", bus, dreamcastControllerType == 1 ? "DreamConn+ / DreamcConn S Controller" : "Dreamcast Controller USB", hasVmu(), hasRumble());
		maple_io_connected = true;
	}
	else
	{
		WARN_LOG(INPUT, "DreamcastController[%d] connection: no VMU or Rumble Pack connected", bus);
		disconnect();
		return;
	}
}

void DreamConn::disconnect()
{
	if (!dcConnection) {
		return;
	}

	dcConnection->disconnect();

	maple_io_connected = false;

	NOTICE_LOG(INPUT, "Disconnected from DreamcastController[%d]", bus);
}

bool DreamConn::send(const MapleMsg& msg)
{
	if (!dcConnection) {
		return false;
	}

	asio::error_code ec;

	if (maple_io_connected)
		ec = dcConnection->sendMsg(msg);
	else
		return false;
	if (ec) {
		maple_io_connected = false;
		WARN_LOG(INPUT, "DreamcastController[%d] send failed: %s", bus, ec.message().c_str());
		disconnect();
		return false;
	}
	return true;
}

bool DreamConnGamepad::isDreamcastController(int deviceIndex)
{
	char guid_str[33] {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid_str, sizeof(guid_str));
	NOTICE_LOG(INPUT, "GUID: %s VID:%c%c%c%c PID:%c%c%c%c", guid_str,
			guid_str[10], guid_str[11], guid_str[8], guid_str[9],
			guid_str[18], guid_str[19], guid_str[16], guid_str[17]);

	// DreamConn VID:4457 PID:4443
	// Dreamcast Controller USB VID:1209 PID:2f07
	if (memcmp(DreamConnConnection::VID_PID_GUID, guid_str + 8, 16) == 0 ||
		memcmp(DreamcastControllerUsbPicoConnection::VID_PID_GUID, guid_str + 8, 16) == 0)
	{
		NOTICE_LOG(INPUT, "Dreamcast controller found!");
		return true;
	}
	return false;
}

DreamConnGamepad::DreamConnGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick)
{
	char guid_str[33] {};

	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(joystick_idx), guid_str, sizeof(guid_str));

	// DreamConn VID:4457 PID:4443
	// Dreamcast Controller USB VID:1209 PID:2f07
	if (memcmp(DreamConnConnection::VID_PID_GUID, guid_str + 8, 16) == 0)
	{
		dreamcastControllerType = TYPE_DREAMCONN;
		_name = "DreamConn+ / DreamConn S Controller";
	}
	else if (memcmp(DreamcastControllerUsbPicoConnection::VID_PID_GUID, guid_str + 8, 16) == 0)
	{
		dreamcastControllerType = TYPE_DREAMCASTCONTROLLERUSB;
		_name = "Dreamcast Controller USB";
	}

	EventManager::listen(Event::Start, handleEvent, this);
	EventManager::listen(Event::LoadState, handleEvent, this);
}

DreamConnGamepad::~DreamConnGamepad() {
	EventManager::unlisten(Event::Start, handleEvent, this);
	EventManager::unlisten(Event::LoadState, handleEvent, this);
}

void DreamConnGamepad::set_maple_port(int port)
{
	if (port < 0 || port >= 4) {
		dreamconn.reset();
	}
	else if (dreamconn == nullptr || dreamconn->getBus() != port) {
		dreamconn.reset();
		dreamconn = std::make_shared<DreamConn>(port, dreamcastControllerType);
	}
	SDLGamepad::set_maple_port(port);
}

void DreamConnGamepad::handleEvent(Event event, void *arg)
{
	DreamConnGamepad *gamepad = static_cast<DreamConnGamepad*>(arg);
	if (gamepad->dreamconn != nullptr)
		createDreamConnDevices(gamepad->dreamconn, event == Event::Start);
}

bool DreamConnGamepad::gamepad_btn_input(u32 code, bool pressed)
{
	if (!is_detecting_input() && input_mapper)
	{
		DreamcastKey key = input_mapper->get_button_id(0, code);
		if (key == DC_BTN_START) {
			startPressed = pressed;
			checkKeyCombo();
		}
	}
	else {
		startPressed = false;
	}
	return SDLGamepad::gamepad_btn_input(code, pressed);
}

bool DreamConnGamepad::gamepad_axis_input(u32 code, int value)
{
	if (!is_detecting_input())
	{
		if (code == leftTrigger) {
			ltrigPressed = value > 0;
			checkKeyCombo();
		}
		else if (code == rightTrigger) {
			rtrigPressed = value > 0;
			checkKeyCombo();
		}
	}
	else {
		ltrigPressed = false;
		rtrigPressed = false;
	}
	return SDLGamepad::gamepad_axis_input(code, value);
}

void DreamConnGamepad::checkKeyCombo() {
	if (ltrigPressed && rtrigPressed && startPressed)
		gui_open_settings();
}

#else // USE_DREAMCASTCONTROLLER

void DreamConn::connect() {
}
void DreamConn::disconnect() {
}

bool DreamConnGamepad::isDreamcastController(int deviceIndex) {
	return false;
}
DreamConnGamepad::DreamConnGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick)
	: SDLGamepad(maple_port, joystick_idx, sdl_joystick) {
}
DreamConnGamepad::~DreamConnGamepad() {
}
void DreamConnGamepad::set_maple_port(int port) {
	SDLGamepad::set_maple_port(port);
}
bool DreamConnGamepad::gamepad_btn_input(u32 code, bool pressed) {
	return SDLGamepad::gamepad_btn_input(code, pressed);
}
bool DreamConnGamepad::gamepad_axis_input(u32 code, int value) {
	return SDLGamepad::gamepad_axis_input(code, value);
}
#endif
