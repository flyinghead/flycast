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
#include <mutex>
#include <condition_variable>
#include <atomic>

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
#include <dirent.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <setupapi.h>
#endif

void createDreamConnDevices(std::shared_ptr<DreamConn> dreamconn, bool gameStart);
void tearDownDreamConnDevices(std::shared_ptr<DreamConn> dreamconn);

class DreamcastControllerConnection
{
private:
	MapleMsg connection_msg;

protected:
	enum class EstablishConnectionResult
	{
		//! Connection establishment has failed
		ConnectionFailed,
		//! A new connection was established
		NewConnectionEstablished,
		//! The current connection is still valid
		ConnectionPersists
	};

public:
	DreamcastControllerConnection(const DreamcastControllerConnection&) = delete;

	DreamcastControllerConnection() = default;
	~DreamcastControllerConnection() = default;

	std::optional<MapleMsg> connect(int bus){
		EstablishConnectionResult result = establishConnection(bus);

		switch (result)
		{
			default:
			case EstablishConnectionResult::ConnectionFailed:
				return std::nullopt;

			case EstablishConnectionResult::ConnectionPersists:
				return connection_msg;

			case EstablishConnectionResult::NewConnectionEstablished:
			{
				// Now get the controller configuration
				connection_msg.command = MDCF_GetCondition;
				connection_msg.destAP = (bus << 6) | 0x20;
				connection_msg.originAP = bus << 6;
				connection_msg.setData(MFID_0_Input);

				asio::error_code ec = sendMsg(connection_msg);
				if (ec)
				{
					WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, ec.message().c_str());
					disconnect();
					return std::nullopt;
				}
				if (!receiveMsg(connection_msg)) {
					WARN_LOG(INPUT, "DreamcastController[%d] read timeout", bus);
					disconnect();
					return std::nullopt;
				}

				onConnectComplete();

				return connection_msg;
			}
		}
	}

	virtual void disconnect() = 0;
	virtual asio::error_code sendMsg(const MapleMsg& msg) = 0;
	virtual bool receiveMsg(MapleMsg& msg) = 0;
	virtual std::string getName() = 0;
	virtual int getDefaultBus() {
		// Value of -1 means to use enumeration order
		return -1;
	}

protected:
	virtual EstablishConnectionResult establishConnection(int bus) = 0;
	virtual void onConnectComplete() = 0;
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

	DreamConnConnection() = default;

	~DreamConnConnection() {
		disconnect();
	}

	EstablishConnectionResult establishConnection(int bus) override {
#if !defined(_WIN32)
		WARN_LOG(INPUT, "DreamcastController[%d] connection failed: DreamConn+ / DreamConn S Controller supported on Windows only", bus);
		return EstablishConnectionResult::ConnectionFailed;
#else
		iostream = asio::ip::tcp::iostream("localhost", std::to_string(BASE_PORT + bus));
		if (!iostream) {
			WARN_LOG(INPUT, "DreamcastController[%d] connection failed: %s", bus, iostream.error().message().c_str());
			disconnect();
			return EstablishConnectionResult::ConnectionFailed;
		}
		iostream.expires_from_now(std::chrono::seconds(1));
		return EstablishConnectionResult::NewConnectionEstablished;
#endif
	}

	void onConnectComplete() override {
		iostream.expires_from_now(std::chrono::duration<u32>::max());	// don't use a 64-bit based duration to avoid overflow
	}

	void disconnect() override {
		if (iostream) {
			iostream.close();
		}
	}

	asio::error_code sendMsg(const MapleMsg& msg) override {
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

		asio::ip::tcp::socket& sock = static_cast<asio::ip::tcp::socket&>(iostream.socket());
		asio::error_code ec;
		asio::write(sock, asio::buffer(s.str()), ec);
		return ec;
	}

	bool receiveMsg(MapleMsg& msg) override {
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

	std::string getName() override {
		return "DreamConn+ / DreamConn S Controller";
	}
};

class DreamPortSerialHandler
{
	//! Asynchronous context for serial_handler
	asio::io_context io_context;
	//! Output buffer data for serial_handler
	std::string serial_out_data;
	//! Handles communication to DreamPort
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

public:
	DreamPortSerialHandler() {

		// the serial port isn't ready at this point, so we need to sleep briefly
		// we probably should have a better way to handle this
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		serial_handler = asio::serial_port(io_context);
		io_context.reset();

		std::string serial_device = "";

		// use user-configured serial device if available, fallback to first available
		serial_device = cfgLoadStr("input", "DreamPortSerialDevice", "");
		if (serial_device.empty()) {
			serial_device = cfgLoadStr("input", "DreamcastControllerUsbSerialDevice", "");
			if (!serial_device.empty()) {
				WARN_LOG(INPUT, "DreamcastControllerUsbSerialDevice config is deprecated; use DreamPortSerialDevice instead");
			}
		}

		if (!serial_device.empty())
		{
			NOTICE_LOG(INPUT, "DreamPort connecting to user-configured serial device: %s", serial_device.c_str());
		} else {
			serial_device = getFirstSerialDevice();
			NOTICE_LOG(INPUT, "DreamPort connecting to autoselected serial device: %s", serial_device.c_str());
		}

		asio::error_code ec;
		serial_handler.open(serial_device, ec);

		if (ec || !serial_handler.is_open()) {
			WARN_LOG(INPUT, "DreamPort serial connection failed: %s", ec.message().c_str());
			disconnect();
		} else {
			NOTICE_LOG(INPUT, "DreamPort serial connection successful!");
		}

		// This must be done before the io_context is run because it will keep io_context from returning immediately
		startSerialRead();

		io_context_thread = std::make_unique<std::thread>([this](){contextThreadEnty();});
	}

	~DreamPortSerialHandler() {
		disconnect();
		io_context_thread->join();
	}

	bool is_open() const {
		return serial_handler.is_open();
	}

	asio::error_code sendMsg(const MapleMsg& msg, int hardware_bus, std::chrono::milliseconds timeout_ms)
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

		// Build serial_out_data string
		{
			// Need to message the hardware bus instead of the software bus
			u8 hwDestAP = (hardware_bus << 6) | (msg.destAP & 0x3F);
			u8 hwOriginAP = (hardware_bus << 6) | (msg.originAP & 0x3F);

			std::ostringstream s;
			s << "X"; // 'X' prefix triggers flycast command parser
			s.fill('0');
			s << std::hex << std::uppercase
				<< std::setw(2) << (u32)msg.command
				<< std::setw(2) << (u32)hwDestAP // override dest
				<< std::setw(2) << (u32)hwOriginAP // override origin
				<< std::setw(2) << (u32)msg.size;
			const u32 sz = msg.getDataSize();
			for (u32 i = 0; i < sz; i++) {
				s << std::setw(2) << (u32)msg.data[i];
			}
			s << "\n";

			serial_out_data = std::move(s.str());
		}

		// Clear out the read buffer before writing next command
		read_queue.clear();
		serial_write_in_progress = true;
		asio::async_write(
			serial_handler,
			asio::buffer(serial_out_data),
			asio::transfer_exactly(serial_out_data.size()),
			[this](const asio::error_code& error, size_t bytes_transferred)
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
			}
		);

		return ec;
	}

	bool receiveMsg(MapleMsg& msg, std::chrono::milliseconds timeout_ms)
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
		response = std::move(read_queue.back());
		read_queue.clear();

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
	void disconnect()
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

//! See: https://github.com/OrangeFox86/DreamPort
class DreamPortConnection : public DreamcastControllerConnection
{
	//! The one and only serial port
	static std::unique_ptr<DreamPortSerialHandler> serial;
	//! Number of devices using the above serial
	static std::atomic<std::uint32_t> connected_dev_count;
	//! Current timeout in milliseconds
	std::chrono::milliseconds timeout_ms;
	//! The bus index of the hardware connection which will differ from the software bus
	int hardware_bus = 0;
	//! true iff only a single devices was found when enumerating devices
	bool is_single_device = true;
	//! True when initial enumeration failed
	bool is_hardware_bus_implied = true;
	//! True once connection is established
	bool connection_established = false;

public:
	//! Dreamcast Controller USB VID:1209 PID:2f07
	static constexpr const std::uint16_t VID = 0x1209;
	static constexpr const std::uint16_t PID = 0x2f07;
	static constexpr const char* VID_PID_GUID = "09120000072f0000";

public:
	DreamPortConnection(const DreamPortConnection&) = delete;
	DreamPortConnection() = delete;

	DreamPortConnection(SDL_Joystick* sdl_joystick) :
		DreamcastControllerConnection()
	{
		const char* joystick_name = SDL_JoystickName(sdl_joystick);
		const char* joystick_path = SDL_JoystickPath(sdl_joystick);

		// Set hardware_bus
		struct SDL_hid_device_info* devs = SDL_hid_enumerate(VID, PID);
		if (devs) {
			hardware_bus = -1;
			if (!devs->next) {
				// Only single device found, so this is simple (host-1p firmware used)
				hardware_bus = 0;
				is_hardware_bus_implied = false;
				is_single_device = true;
			} else {
				struct SDL_hid_device_info* it = devs;
				struct SDL_hid_device_info* my_dev = nullptr;

				while (it)
				{
					// Note: hex characters will be differing case, so case-insensitive cmp is needed
					if (0 == SDL_strcasecmp(it->path, joystick_path)) {
						my_dev = it;
						break;
					}
					it = it->next;
				}

				if (my_dev) {
					it = devs;
					int count = 0;
					while (it) {
						if (0 == wcscmp(it->serial_number, my_dev->serial_number)) {
							++count;
						}
						it = it->next;
					}
					if (count == 1) {
						// Single device of this serial found
						is_single_device = true;
						hardware_bus = 0;
						is_hardware_bus_implied = false;
					} else {
						is_single_device = false;
						// Interfaces go in decending order
						hardware_bus = (count - my_dev->interface_number - 1);
						is_hardware_bus_implied = false;
					}
				}

				if (hardware_bus < 0) {
#if defined(_WIN32)
						// Windows doesn't set joystick name properly, so there is nothing else that can be done
						WARN_LOG(INPUT, "DreamPort connection: failed to locate enumerated device; assuming HW ID of 0");
						hardware_bus = 0;
						is_hardware_bus_implied = true;
						is_single_device = true;
#else
						if (0 == strcmp("P4", joystick_name)) {
							hardware_bus = 3;
							is_hardware_bus_implied = false;
						} else if (0 == strcmp("P3", joystick_name)) {
							hardware_bus = 2;
							is_hardware_bus_implied = false;
						} else if (0 == strcmp("P2", joystick_name)) {
							hardware_bus = 1;
							is_hardware_bus_implied = false;
						} else {
							hardware_bus = 0;
							is_hardware_bus_implied = false;
						}
#endif
				}
			}

			SDL_hid_free_enumeration(devs);
		} else {
			WARN_LOG(INPUT, "DreamPort connection: failed to enumerate devices; assuming HW ID of 0");
			hardware_bus = 0;
			is_hardware_bus_implied = true;
		}
	}

	~DreamPortConnection(){
		disconnect();
	}

	int hardwareBus() const {
		return hardware_bus;
	}

	bool isHardwareBusImplied() const {
		return is_hardware_bus_implied;
	}

	bool isSingleDevice() const {
		return is_single_device;
	}

	EstablishConnectionResult establishConnection(int bus) override {
		// Timeout is 1 second while establishing connection
		timeout_ms = std::chrono::seconds(1);

		if (connection_established && serial) {
			if (serial->is_open())
			{
				// This equipment is fixed to the hardware bus - the software bus isn't relevant
				return EstablishConnectionResult::ConnectionPersists;
			}
			else
			{
				disconnect();
				return EstablishConnectionResult::ConnectionFailed;
			}
		}

		++connected_dev_count;
		connection_established = true;
		if (!serial)
		{
			serial = std::make_unique<DreamPortSerialHandler>();
		}

		if (serial && serial->is_open())
		{
			return EstablishConnectionResult::NewConnectionEstablished;
		}
		else
		{
			disconnect();
			return EstablishConnectionResult::ConnectionFailed;
		}
	}

	void onConnectComplete() override {
		// Timeout is extended to 5 seconds for all other communication after connection
		timeout_ms = std::chrono::seconds(5);
	}

	void disconnect() override {
		if (connection_established) {
			connection_established = false;
			if (--connected_dev_count == 0) {
				// serial is no longer needed
				serial.reset();
			}
		}
	}

	asio::error_code sendMsg(const MapleMsg& msg) override {
		if (serial) {
			return serial->sendMsg(msg, hardware_bus, timeout_ms);
		}

		return asio::error::not_connected;
	}

	bool receiveMsg(MapleMsg& msg) override {
		if (serial) {
			return serial->receiveMsg(msg, timeout_ms);
		}

		return false;
	}

	std::string getName() override {
		std::string name = "DreamPort";
		if (!is_hardware_bus_implied && !is_single_device) {
			const char portChar = ('A' + hardware_bus);
			name += " " + std::string(1, portChar);
		}
		return name;
	}

	virtual int getDefaultBus() {
		if (!is_hardware_bus_implied && !is_single_device) {
			return hardware_bus;
		} else {
			// Value of -1 means to use enumeration order
			return -1;
		}
	}
};

// Define the static instances here
std::unique_ptr<DreamPortSerialHandler> DreamPortConnection::serial;
std::atomic<std::uint32_t> DreamPortConnection::connected_dev_count = 0;

DreamConn::DreamConn(int bus, int dreamcastControllerType, SDL_Joystick* sdl_joystick) :
	bus(bus), dreamcastControllerType(dreamcastControllerType)
{
	switch (dreamcastControllerType)
	{
		case TYPE_DREAMCONN:
			dcConnection = std::make_unique<DreamConnConnection>();
			break;

		case TYPE_DREAMCASTCONTROLLERUSB:
			dcConnection = std::make_unique<DreamPortConnection>(sdl_joystick);
			break;
	}
}

DreamConn::~DreamConn() {
	disconnect();
}

int DreamConn::getDefaultBus() {
	if (dcConnection) {
		return dcConnection->getDefaultBus();
	}
	return -1;
}

void DreamConn::changeBus(int newBus) {
	bus = newBus;
}

std::string DreamConn::getName() {
	if (dcConnection) {
		return dcConnection->getName();
	}
	return "Unknown DreamConn";
}

void DreamConn::connect()
{
	if (maple_io_connected) {
		disconnect();
	}

	maple_io_connected = false;
	expansionDevs = 0;

	if (!dcConnection) {
		return;
	}

	std::optional<MapleMsg> msg = dcConnection->connect(bus);
	if (!msg)
	{
		return;
	}

	expansionDevs = msg->originAP & 0x1f;

	config::MapleExpansionDevices[bus][0] = hasVmu() ? MDT_SegaVMU : MDT_None;
	config::MapleExpansionDevices[bus][1] = hasRumble() ? MDT_PurupuruPack : MDT_None;

	if (hasVmu() || hasRumble())
	{
		NOTICE_LOG(INPUT, "Connected to DreamcastController[%d]: Type:%s, VMU:%d, Rumble Pack:%d", bus, getName().c_str(), hasVmu(), hasRumble());
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
		memcmp(DreamPortConnection::VID_PID_GUID, guid_str + 8, 16) == 0)
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
		dreamconn = std::make_shared<DreamConn>(maple_port, TYPE_DREAMCONN, sdl_joystick);
	}
	else if (memcmp(DreamPortConnection::VID_PID_GUID, guid_str + 8, 16) == 0)
	{
		dreamconn = std::make_shared<DreamConn>(maple_port, TYPE_DREAMCASTCONTROLLERUSB, sdl_joystick);
	}

	if (dreamconn) {
		_name = dreamconn->getName();
		int defaultBus = dreamconn->getDefaultBus();
		if (defaultBus >= 0 && defaultBus < 4) {
			set_maple_port(defaultBus);
		}
	}

	EventManager::listen(Event::Start, handleEvent, this);
	EventManager::listen(Event::LoadState, handleEvent, this);
}

DreamConnGamepad::~DreamConnGamepad() {
	EventManager::unlisten(Event::Start, handleEvent, this);
	EventManager::unlisten(Event::LoadState, handleEvent, this);
	if (dreamconn) {
		tearDownDreamConnDevices(dreamconn);
		dreamconn.reset();
	}
}

void DreamConnGamepad::set_maple_port(int port)
{
	if (dreamconn) {
		if (port < 0 || port >= 4) {
			dreamconn->disconnect();
		}
		else if (dreamconn->getBus() != port) {
			dreamconn->changeBus(port);
			if (is_registered()) {
				dreamconn->connect();
			}
		}
	}
	SDLGamepad::set_maple_port(port);
}

void DreamConnGamepad::registered()
{
	if (dreamconn)
	{
		dreamconn->connect();
	}
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
void DreamConnGamepad::registered() {
}
bool DreamConnGamepad::gamepad_btn_input(u32 code, bool pressed) {
	return SDLGamepad::gamepad_btn_input(code, pressed);
}
bool DreamConnGamepad::gamepad_axis_input(u32 code, int value) {
	return SDLGamepad::gamepad_axis_input(code, value);
}
#endif
