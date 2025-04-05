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

#include "dreampicoport.h"

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
#include <vector>
#include <array>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <locale>
#include <codecvt>

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
#include <dirent.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <setupapi.h>
#endif

class DreamPicoPortSerialHandler
{
	//! Asynchronous context for serial_handler
	asio::io_context io_context;
	//! Output buffer data for serial_handler
	std::string serial_out_data;
	//! Handles communication to DreamPicoPort
	asio::serial_port serial_handler{io_context};
	//! Set to true while an async write is in progress with serial_handler
	bool serial_write_in_progress = false;
	//! Set to true while an async read is in progress with serial_handler
	std::atomic<bool> serial_read_in_progress = false;
	//! Signaled when serial_write_in_progress transitions to false
	std::condition_variable write_cv;
	//! Mutex for write_cv and serializes access to serial_write_in_progress
	std::mutex write_cv_mutex;
	//! Input stream buffer from serial_handler
	char serial_read_buffer[1024];
	//! Holds on to partially parsed line
	std::string read_line_buffer;
	//! Thread which runs the io_context
	std::unique_ptr<std::thread> io_context_thread;
	//! Contains queue of incoming lines from serial
	std::list<std::string> read_queue;
	//! Signaled when data is in read_queue
	std::condition_variable read_cv;
	//! Mutex for read_cv and serializes access to read_queue
	std::mutex read_cv_mutex;

	//! When >= 0, parsing binary input and signifies total number parsed in this set
	//! When < 0, not parsing binary input
	int32_t num_binary_parsed = -1;
	//! Number of binary bytes left to parse
	uint16_t stored_binary_size = 0;
	//! Number of binary bytes left to parse in current set
	uint16_t num_binary_left = 0;

	//! Serializes send calls, making them thread-safe
	std::mutex send_mutex;

public:
	DreamPicoPortSerialHandler() {

		// the serial port isn't ready at this point, so we need to sleep briefly
		// we probably should have a better way to handle this
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		serial_handler = asio::serial_port(io_context);
		io_context.reset();

		std::string serial_device = "";

		// use user-configured serial device if available, fallback to first available
		serial_device = cfgLoadStr("input", "DreamPicoPortSerialDevice", "");
		if (!serial_device.empty())
		{
			NOTICE_LOG(INPUT, "DreamPicoPort connecting to user-configured serial device: %s", serial_device.c_str());
		} else {
			serial_device = getFirstSerialDevice();
			NOTICE_LOG(INPUT, "DreamPicoPort connecting to autoselected serial device: %s", serial_device.c_str());
		}

		asio::error_code ec;
		serial_handler.open(serial_device, ec);

		if (ec || !serial_handler.is_open()) {
			WARN_LOG(INPUT, "DreamPicoPort serial connection failed: %s", ec.message().c_str());
			disconnect();
		} else {
			NOTICE_LOG(INPUT, "DreamPicoPort serial connection successful!");
		}

		// This must be done before the io_context is run because it will keep io_context from returning immediately
		startSerialRead();

		io_context_thread = std::make_unique<std::thread>([this](){contextThreadEnty();});
	}

	~DreamPicoPortSerialHandler() {
		disconnect();
		io_context_thread->join();
	}

	bool is_open() const {
		return serial_handler.is_open();
	}

	asio::error_code sendCmd(
		const std::string& cmd,
		std::string& response,
		std::chrono::milliseconds timeout_ms
	) {
		const std::chrono::steady_clock::time_point expiration = std::chrono::steady_clock::now() + timeout_ms;

		std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations

		asio::error_code ec = transmit(cmd, true, expiration);

		if (!ec) {
			ec = receive(response, expiration);
		}

		return ec;
	}

	asio::error_code sendCmd(
		const std::string& cmd,
		std::chrono::milliseconds timeout_ms
	) {
		const std::chrono::steady_clock::time_point expiration = std::chrono::steady_clock::now() + timeout_ms;

		std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations

		return transmit(cmd, false, expiration);
	}

	asio::error_code sendMsg(
		const MapleMsg& msg,
		int hardware_bus,
		MapleMsg& response,
		std::chrono::milliseconds timeout_ms)
	{
		const std::chrono::steady_clock::time_point expiration = std::chrono::steady_clock::now() + timeout_ms;

		std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations

		std::string cmd = msgToStr(msg, hardware_bus);
		asio::error_code ec = transmit(cmd, true, expiration);

		if (!ec) {
			ec = receive(response, expiration);
		}

		return ec;
	}

	asio::error_code sendMsg(
		const MapleMsg& msg,
		int hardware_bus,
		std::chrono::milliseconds timeout_ms)
	{
		const std::chrono::steady_clock::time_point expiration = std::chrono::steady_clock::now() + timeout_ms;

		std::lock_guard<std::mutex> lock(send_mutex); // Ensure thread safety for send operations

		std::string cmd = msgToStr(msg, hardware_bus);
		return transmit(cmd, false, expiration);
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

	asio::error_code transmit(
		const std::string& cmd,
		bool receive_expected,
		const std::chrono::steady_clock::time_point& expiration
	) {
		asio::error_code ec;

		if (!serial_handler.is_open()) {
			return asio::error::not_connected;
		}

		if (receive_expected && serial_read_in_progress) {
			// Wait up to 30 ms for read to complete before writing to help ensure expected command order.
			// Continue regardless of result.
			std::string rx;

			std::chrono::steady_clock::time_point rxExpiration =
				std::chrono::steady_clock::now() + std::chrono::milliseconds(30);

			if (rxExpiration > expiration) {
				rxExpiration = expiration;
			}

			(void)receive(rx, rxExpiration);
		} else {
			// Just clear out the read queue before continuing
			std::unique_lock<std::mutex> lock(read_cv_mutex);
			read_queue.clear();
		}

		// Wait for last write to complete
		std::unique_lock<std::mutex> lock(write_cv_mutex);
		if (!write_cv.wait_until(lock, expiration, [this](){return (!serial_write_in_progress || !serial_handler.is_open());}))
		{
			return asio::error::timed_out;
		}

		// Check again before continuing
		if (!serial_handler.is_open()) {
			return asio::error::not_connected;
		}

		serial_out_data = cmd;

		// Clear out the read buffer before writing next command
		serial_write_in_progress = true;
		serial_read_in_progress = true;
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

	asio::error_code receive(std::string& cmd, const std::chrono::steady_clock::time_point& expiration)
	{
		asio::error_code ec;

		// Wait for at least 2 lines to be received (first line is echo back)
		std::unique_lock<std::mutex> lock(read_cv_mutex);
		if (!read_cv.wait_until(lock, expiration, [this](){return ((read_queue.size() >= 2) || !serial_handler.is_open());}))
		{
			// Timeout
			return asio::error::timed_out;
		}

		if (read_queue.size() < 2) {
			// Connection was closed before data could be received
			return asio::error::connection_aborted;
		}

		// discard the first message as we are interested in the second only which returns the controller configuration
		cmd = std::move(read_queue.back());
		read_queue.clear();
		serial_read_in_progress = false;
		return ec;
	}

	asio::error_code receive(MapleMsg& msg, const std::chrono::steady_clock::time_point& expiration)
	{
		asio::error_code ec;
		std::string response;

		ec = receive(response, expiration);
		if (ec) {
			return ec;
		}

		std::vector<uint32_t> words;
		bool valid = false;
		const char* iter = response.c_str();
		const char* eol = iter + response.size();

		if (*iter == '*')
		{
			// Asterisk indicates the write or read operation failed
			return asio::error::no_data;
		}
		else if (*iter == '\5') // binary parsing
		{
			// binary
			++iter;
			while (iter < eol)
			{
				uint32_t word = 0;
				uint32_t i = 0;
				while (i < 4 && iter < eol)
				{
					const u8* pu8 = reinterpret_cast<const u8*>(iter++);
					// Apply value into current word
					word |= (*pu8 << ((4 - i) * 8 - 8));
					++i;
				}

				// Invalid if a partial word was given
				valid = ((i == 4) || (i == 0));

				if (i == 4)
				{
					words.push_back(word);
				}
			}
		}
		else
		{
			while (iter < eol)
			{
				uint32_t word = 0;
				uint32_t i = 0;
				while (i < 8 && iter < eol)
				{
					char v = *iter++;
					uint_fast8_t value = 0;

					if (v >= '0' && v <= '9')
					{
						value = v - '0';
					}
					else if (v >= 'a' && v <= 'f')
					{
						value = v - 'a' + 0xa;
					}
					else if (v >= 'A' && v <= 'F')
					{
						value = v - 'A' + 0xA;
					}
					else
					{
						// Ignore this character
						continue;
					}

					// Apply value into current word
					word |= (value << ((8 - i) * 4 - 4));
					++i;
				}

				// Invalid if a partial word was given
				valid = ((i == 8) || (i == 0));

				if (i == 8)
				{
					words.push_back(word);
				}
			}
		}

		if (words.size() > 0)
		{
			msg.command = (words[0] >> 24) & 0xFF;
			msg.destAP = (words[0] >> 16) & 0xFF;
			msg.originAP = (words[0] >> 8) & 0xFF;
			msg.size = words[0] & 0xFF;

			for (uint32_t i = 1; i < words.size(); ++i)
			{
				uint32_t dat = ntohl(words[i]);
				memcpy(&msg.data[(i-1)*4], &dat, sizeof(dat));
			}
		}
		else
		{
			return asio::error::message_size;
		}

		if (!serial_handler.is_open()) {
			return asio::error::not_connected;
		}

		return ec;
	}

	std::string msgToStr(const MapleMsg& msg, int hardware_bus) {
		// Build serial_out_data string
		// Need to message the hardware bus instead of the software bus
		u8 hwDestAP = (hardware_bus << 6) | (msg.destAP & 0x3F);
		u8 hwOriginAP = (hardware_bus << 6) | (msg.originAP & 0x3F);

		std::ostringstream s;
		s << "X "; // 'X' prefix triggers flycast command parser
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

		return s.str();
	}

	void startSerialRead()
	{
		serialReadHandler();
		// Just to make sure initial data is cleared off of incoming buffer
		io_context.poll_one();
		read_queue.clear();
	}

	void serialReadHandler()
	{
		// Arm or rearm the read
		serial_handler.async_read_some(
			asio::buffer(serial_read_buffer, sizeof(serial_read_buffer)),
			[this](const asio::error_code& error, std::size_t size) -> void {
				std::lock_guard<std::mutex> lock(read_cv_mutex);
				if (error) {
					try
					{
						serial_handler.cancel();
					}
					catch(const asio::system_error&)
					{
						// Ignore cancel errors
					}
					read_cv.notify_all();
				} else {
					if (size > 0) {
						// Consume the received data
						if (consumeReadBuffer(size) > 0)
						{
							// New lines available
							read_cv.notify_all();
						}
					}
					// Auto reload read - io_context will always have work to do
					serialReadHandler();
				}
			}
		);
	}

	int consumeReadBuffer(std::size_t size) {
		if (size <= 0) {
			return 0;
		}

		int numberOfLines = 0;
		const char* iter = serial_read_buffer;
		while (size-- > 0)
		{
			char c = *iter++;

			if (num_binary_parsed >= 0)
			{
				++num_binary_parsed;
				--num_binary_left;

				if (num_binary_parsed == 1)
				{
					stored_binary_size = (c << 8);
				}
				else if (num_binary_parsed == 2)
				{
					stored_binary_size |= c;
					num_binary_left = stored_binary_size;
					read_line_buffer.reserve(1 + stored_binary_size);
				}
				else
				{
					read_line_buffer += c;
				}

				if (num_binary_left == 0)
				{
					num_binary_parsed = -1;
				}
			}
			else if (c == '\5') // binary start character
			{
				read_line_buffer += c;
				num_binary_parsed = 0;
				stored_binary_size = 0;
				num_binary_left = 2; // Parse size
			}
			else if (c == '\n')
			{
				// Remove carriage return if found and add this line to queue
				if (read_line_buffer.size() > 0 && read_line_buffer[read_line_buffer.size() - 1] == '\r') {
					read_line_buffer.pop_back();
				}
				read_queue.push_back(read_line_buffer);
				read_line_buffer.clear();

				++numberOfLines;
			}
			else
			{
				read_line_buffer += c;
			}
		}

		return numberOfLines;
	}
};

// Define the static instances here
std::unique_ptr<DreamPicoPortSerialHandler> DreamPicoPort::serial;
std::atomic<std::uint32_t> DreamPicoPort::connected_dev_count = 0;

DreamPicoPort::DreamPicoPort(int bus, int joystick_idx, SDL_Joystick* sdl_joystick) :
	software_bus(bus)
{
#if defined(_WIN32)
	// Workaround: Getting the instance ID here fixes some sort of L/R trigger bug in Windows dinput for some reason
	(void)SDL_JoystickGetDeviceInstanceID(joystick_idx);
#endif
	determineHardwareBus(joystick_idx, sdl_joystick);

	unique_id.clear();
	if (!is_hardware_bus_implied && !serial_number.empty()) {
		// Locking to name, which includes A-D, plus serial number will ensure correct enumeration every time
		unique_id = std::string("sdl_") + getName("") + std::string("_") + serial_number;
	}
}

DreamPicoPort::~DreamPicoPort() {
	disconnect();
}

bool DreamPicoPort::send(const MapleMsg& msg) {
	if (serial) {
		asio::error_code ec = serial->sendMsg(msg, hardware_bus, timeout_ms);
		return !ec;
	}

	return false;
}

bool DreamPicoPort::send(const MapleMsg& txMsg, MapleMsg& rxMsg) {
	if (serial) {
		asio::error_code ec = serial->sendMsg(txMsg, hardware_bus, rxMsg, timeout_ms);
		return !ec;
	}

	return false;
}

inline void DreamPicoPort::gameTermination() {
	// Need a short delay to wait for last screen draw to complete
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	// Reset screen to selected port
	sendPort();
}

int DreamPicoPort::getBus() const {
	return software_bus;
}

u32 DreamPicoPort::getFunctionCode(int forPort) const {
	u32 mask = 0;
	if (peripherals.size() > forPort) {
		for (const auto& peripheral : peripherals[forPort]) {
			mask |= peripheral[0];
		}
	}
	// swap bytes to get the correct function code
	return SWAP32(mask);
}

std::array<u32, 3> DreamPicoPort::getFunctionDefinitions(int forPort) const {
	std::array<u32, 3> arr{0, 0, 0};
	if (peripherals.size() > forPort) {
		std::size_t idx = 0;
		for (const auto& peripheral : peripherals[forPort]) {
			arr[idx++] = SWAP32(peripheral[1]);
			if (idx >= 3) break;
		}
	}
	return arr;
}

int DreamPicoPort::getDefaultBus() const {
	if (!is_hardware_bus_implied && !is_single_device) {
		return hardware_bus;
	} else {
		// Value of -1 means to use enumeration order
		return -1;
	}
}

void DreamPicoPort::setDefaultMapping(const std::shared_ptr<InputMapping>& mapping) const {
	// Since this is a real DC controller, no deadzone adjustment is needed
	mapping->dead_zone = 0.0f;
	// Map the things not set by SDL
	mapping->set_button(DC_BTN_C, 2);
	mapping->set_button(DC_BTN_Z, 5);
	mapping->set_button(DC_BTN_D, 10);
}

std::string DreamPicoPort::getUniqueId() const {
	return unique_id;
}

void DreamPicoPort::changeBus(int newBus) {
	software_bus = newBus;
}

std::string DreamPicoPort::getName() const {
	return getName(" ");
}

std::string DreamPicoPort::getName(std::string separator) const {
	std::string name = "DreamPicoPort";
	if (!is_hardware_bus_implied && !is_single_device) {
		const char portChar = ('A' + hardware_bus);
		name += separator + std::string(1, portChar);
	}
	return name;
}

void DreamPicoPort::connect() {
	// Timeout is 1 second while establishing connection
	timeout_ms = std::chrono::seconds(1);

	if (connection_established && serial) {
		if (serial->is_open()) {
			sendPort();
		} else {
			disconnect();
			return;
		}
	}

	++connected_dev_count;
	connection_established = true;
	if (!serial) {
		serial = std::make_unique<DreamPicoPortSerialHandler>();
	}

	if (serial && serial->is_open()) {
		sendPort();
	} else {
		disconnect();
		return;
	}

	if (!queryInterfaceVersion()) {
		disconnect();
		return;
	}

	if (!queryPeripherals()) {
		disconnect();
		return;
	}

	// Timeout is extended to 5 seconds for all other communication after connection
	timeout_ms = std::chrono::seconds(5);

	int vmuCount = 0;
	int vibrationCount = 0;

	u32 portOneFn = getFunctionCode(1);
	if (portOneFn & MFID_1_Storage) {
		config::MapleExpansionDevices[software_bus][0] = MDT_SegaVMU;
		++vmuCount;
	}
	else {
		config::MapleExpansionDevices[software_bus][0] = MDT_None;
	}

	u32 portTwoFn = getFunctionCode(2);
	if (portTwoFn & MFID_8_Vibration) {
		config::MapleExpansionDevices[software_bus][1] = MDT_PurupuruPack;
		++vibrationCount;
	}
	else if (portTwoFn & MFID_1_Storage) {
		config::MapleExpansionDevices[software_bus][1] = MDT_SegaVMU;
		++vmuCount;
	}
	else {
		config::MapleExpansionDevices[software_bus][1] = MDT_None;
	}

	NOTICE_LOG(INPUT, "Connected to DreamcastController[%d]: Type:%s, VMU:%d, Rumble Pack:%d", software_bus, getName().c_str(), vmuCount, vibrationCount);
}

void DreamPicoPort::disconnect() {
	if (connection_established) {
		connection_established = false;
		if (--connected_dev_count == 0) {
			// serial is no longer needed
			serial.reset();
		}
	}
}

void DreamPicoPort::sendPort() {
	if (connection_established && serial && software_bus >= 0 && software_bus <= 3 && hardware_bus >=0 && hardware_bus <= 3) {
		// This will update the displayed port letter on the screen
		std::ostringstream s;
		s << "XP "; // XP is flycast "set port" command
		s << hardware_bus << " " << software_bus << "\n";
		serial->sendCmd(s.str(), timeout_ms);
	}
}

int DreamPicoPort::hardwareBus() const {
	return hardware_bus;
}

bool DreamPicoPort::isHardwareBusImplied() const {
	return is_hardware_bus_implied;
}

bool DreamPicoPort::isSingleDevice() const {
	return is_single_device;
}

void DreamPicoPort::determineHardwareBus(int joystick_idx, SDL_Joystick* sdl_joystick) {
	// This function determines what bus index to use when communicating with the hardware.

	// Set the serial number if found by SDL Joystick
	const char* joystick_serial = SDL_JoystickGetSerial(sdl_joystick);
	if (joystick_serial) {
		serial_number = joystick_serial;
	}

#if defined(_WIN32)
	// This only works in Windows because the joystick_path is not given in other OSes
	const char* joystick_name = SDL_JoystickName(sdl_joystick);
	const char* joystick_path = SDL_JoystickPath(sdl_joystick);

	struct SDL_hid_device_info* devs = SDL_hid_enumerate(VID, PID);
	if (devs) {
		struct SDL_hid_device_info* my_dev = nullptr;

		if (!devs->next) {
			// Only single device found, so this is simple (host-1p firmware used)
			hardware_bus = 0;
			is_hardware_bus_implied = false;
			is_single_device = true;
			my_dev = devs;
		} else {
			struct SDL_hid_device_info* it = devs;

			if (joystick_path)
			{
				while (it)
				{
					// Note: hex characters will be differing case, so case-insensitive cmp is needed
					if (it->path && 0 == SDL_strcasecmp(it->path, joystick_path)) {
						my_dev = it;
						break;
					}
					it = it->next;
				}
			}

			if (my_dev) {
				it = devs;
				int count = 0;
				if (my_dev->serial_number) {
					while (it) {
						if (it->serial_number &&
							0 == wcscmp(it->serial_number, my_dev->serial_number))
						{
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
						if (my_dev->release_number < 0x0102) {
							// Interfaces go in decending order
							hardware_bus = (count - (my_dev->interface_number % 4) - 1);
							is_hardware_bus_implied = false;
						} else {
							// Version 1.02 of interface will make interfaces in ascending order
							hardware_bus = (my_dev->interface_number % 4);
							is_hardware_bus_implied = false;
						}
					}
				}
			}
		}

		// Set serial number if found in SDL_hid
		if (my_dev) {
			if (serial_number.empty() && my_dev->serial_number) {
				std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
				serial_number = converter.to_bytes(my_dev->serial_number);
			}
		}

		SDL_hid_free_enumeration(devs);
	}

#endif

	if (hardware_bus < 0) {
		// The number of buttons gives a clue as to what index the controller is
		int nbuttons = SDL_JoystickNumButtons(sdl_joystick);

		if (nbuttons >= 32 || nbuttons <= 27) {
			// Older version of firmware or single player
			hardware_bus = 0;
			is_hardware_bus_implied = true;
			is_single_device = true;
		}
		else {
			hardware_bus = 31 - nbuttons;
			is_hardware_bus_implied = false;
			is_single_device = false;
		}
	}
}

bool DreamPicoPort::queryInterfaceVersion() {
	std::string buffer;
	asio::error_code error = serial->sendCmd("XV\n", buffer, timeout_ms);
	if (error) {
		WARN_LOG(INPUT, "DreamPicoPort[%d] send(XV) failed: %s", software_bus, error.message().c_str());
		return false;
	}

	if (0 == strncmp("*failed", buffer.c_str(), 7) || 0 == strncmp("0: failed", buffer.c_str(), 9)) {
		// Using a version of firmware before "XV" was available
		interface_version = 0.0;
	}
	else {
		try {
			interface_version = std::stod(buffer);
		}
		catch(const std::exception&) {
			WARN_LOG(INPUT, "DreamPicoPort[%d] command XV received invalid response: %s", software_bus, buffer.c_str());
			return false;
		}
	}

	return true;
}

bool DreamPicoPort::queryPeripherals() {
	peripherals.clear();
	expansionDevs = 0;

	MapleMsg msg;
	msg.command = MDCF_GetCondition;
	msg.destAP = (hardware_bus << 6) | 0x20;
	msg.originAP = hardware_bus << 6;
	msg.setData(MFID_0_Input);

	asio::error_code error = serial->sendMsg(msg, hardware_bus, msg, timeout_ms);
	if (error)
	{
		WARN_LOG(INPUT, "DreamPicoPort[%d] send(condition) failed: %s", software_bus, error.message().c_str());
		return true; // assume simply controller not connected yet
	}

	expansionDevs = msg.originAP & 0x1f;

	if (interface_version >= 1.0) {
		// Can just use X?
		std::string buffer;
		error = serial->sendCmd("X?" + std::to_string(hardware_bus) + "\n", buffer, timeout_ms);
		if (error) {
			WARN_LOG(INPUT, "DreamPicoPort[%d] send(X?) failed: %s", software_bus, error.message().c_str());
			return false;
		}

		{
			std::istringstream stream(buffer);
			std::string outerGroup;
			while (std::getline(stream, outerGroup, ';')) {
				if (outerGroup.empty() || outerGroup == ",") continue;
				std::vector<std::array<uint32_t, 2>> outerList;
				std::istringstream outerStream(outerGroup.substr(1)); // Skip the leading '{'
				std::string innerGroup;

				while (std::getline(outerStream, innerGroup, '}')) {
					if (innerGroup.empty() || innerGroup == ",") continue;
					std::array<uint32_t, 2> innerList = {{0, 0}};
					std::istringstream innerStream(innerGroup.substr(1)); // Skip the leading '{'
					std::string number;
					std::size_t idx = 0;

					while (std::getline(innerStream, number, ',')) {
						if (!number.empty() && number[0] == '{') {
							number = number.substr(1);
						}
						uint32_t value;
						std::stringstream ss;
						ss << std::hex << number;
						ss >> value;
						if (idx < 2) {
							innerList[idx] = value;
						}
						++idx;
					}

					outerList.push_back(innerList);
				}

				peripherals.push_back(outerList);
			}
		}
	}
	else {
		// TODO: probably should just pop up a toast asking user to update firmware
		// Manually query each sub-peripheral
		peripherals.push_back({}); // skip controller since it's not used
		for (u32 i = 0; i < 2; ++i) {
			std::vector<std::array<uint32_t, 2>> portPeripherals;
			u8 port = (1 << i);
			if (expansionDevs & port) {
				msg.command = MDC_DeviceRequest;
				msg.destAP = (hardware_bus << 6) | port;
				msg.originAP = hardware_bus << 6;
				msg.size = 0;

				error = serial->sendMsg(msg, hardware_bus, msg, timeout_ms);
				if (error) {
					WARN_LOG(INPUT, "DreamPicoPort[%d] send(query) failed: %s", software_bus, error.message().c_str());
					return false;
				}

				if (msg.size < 4) {
					WARN_LOG(INPUT, "DreamPicoPort[%d] read(query) failed: invalid size %d", software_bus, msg.size);
					return false;
				}

				const u32 fnCode = (msg.data[0] << 24) | (msg.data[1] << 16) | (msg.data[2] << 8) | msg.data[3];
				u8 fnIdx = 1;
				u32 mask = 0x80000000;
				while (mask > 0) {
					if (fnCode & mask) {
						u32 i = fnIdx++ * 4;
						u32 code = (msg.data[i] << 24) | (msg.data[i+1] << 16) | (msg.data[i+2] << 8) | msg.data[i+3];
						std::array<uint32_t, 2> peripheral = {{mask, code}};
						portPeripherals.push_back(std::move(peripheral));
					}
					mask >>= 1;
				}

			}
			peripherals.push_back(portPeripherals);
		}
	}

	return true;
}

#endif // USE_DREAMCASTCONTROLLER
