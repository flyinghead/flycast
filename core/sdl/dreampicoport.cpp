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

#include "hw/maple/maple_devs.h"
#include "ui/gui.h"
#include "cfg/option.h"
#include "oslib/i18n.h"

#include "DreamPicoPortApi.hpp"

// C++ standard library
#include <iomanip>
#include <sstream>
#include <thread>
#include <list>
#include <vector>
#include <array>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <chrono>
#include <unordered_map>

#ifndef TARGET_UWP
#include <asio.hpp>
#endif

#if defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))
#include <dirent.h>
#endif

#if defined(_WIN32) && !defined(TARGET_UWP)
#include <windows.h>
#include <setupapi.h>
#endif

//! Interface class for different DreamPicoPort communications interface
class DreamPicoPortComms
{
public:
	DreamPicoPortComms() = default;
	virtual ~DreamPicoPortComms() = default;
	virtual void changeSoftwareBus(int software_bus) = 0;
	virtual bool isConnected() = 0;
	virtual bool initialize(std::chrono::milliseconds timeout_ms) = 0;
	virtual std::optional<std::vector<std::vector<std::array<uint32_t, 2>>>> getPeripherals(
		std::chrono::milliseconds timeout_ms
	) = 0;
	virtual bool send(const MapleMsg& msg, std::chrono::milliseconds timeout_ms) = 0;
    virtual bool send(const MapleMsg& txMsg, MapleMsg& rxMsg, std::chrono::milliseconds timeout_ms) = 0;
	virtual void sendPort(std::chrono::milliseconds timeout_ms) = 0;
};

// asio::serial_port is not accessible for UWP. DreamPicoPort-API may be used in UWP.
#ifndef TARGET_UWP

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
		serial_device = config::loadStr("input", "DreamPicoPortSerialDevice");
		if (!serial_device.empty())
		{
			NOTICE_LOG(INPUT, "DreamPicoPort connecting to user-configured serial device: %s", serial_device.c_str());
		} else {
			serial_device = getFirstSerialDevice();
			NOTICE_LOG(INPUT, "DreamPicoPort connecting to autoselected serial device: %s", serial_device.c_str());
		}

		asio::error_code ec;
		if (!serial_device.empty()) {
			serial_handler.open(serial_device, ec);
		}

		if (ec || !serial_handler.is_open()) {
			WARN_LOG(INPUT, "DreamPicoPort serial connection failed: %s", ec.message().c_str());
			disconnect();
			return;
		}

		NOTICE_LOG(INPUT, "DreamPicoPort serial connection successful!");

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
		// (getFirstSerialDevice() not compatible with UWP)
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
#endif
		return "";
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
				if (i == 4)
					words.push_back(word);
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
				if (i == 8)
					words.push_back(word);
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
		s.imbue(std::locale::classic());
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

class SerialDreamPicoPortComms : public DreamPicoPortComms
{
	//! The one and only serial port
	static std::unique_ptr<DreamPicoPortSerialHandler> serial;
	//! Number of devices using the above serial
	static std::atomic<std::uint32_t> connected_dev_count;

	//! The bus ID dictated by flycast
	int software_bus = -1;
	//! The bus index of the hardware connection which will differ from the software bus
	int hardware_bus = -1;
    //! The queried interface version
	double interface_version = 0.0;
	//! Flags which specifies which subperipherals are connected
	u8 expansion_devs = 0;

public:
	SerialDreamPicoPortComms() = delete;

	SerialDreamPicoPortComms(int software_bus, int hardware_bus) :
		software_bus(software_bus),
		hardware_bus(hardware_bus)
	{
		++connected_dev_count;
		if (!serial) {
			serial = std::make_unique<DreamPicoPortSerialHandler>();
		}
	}

	virtual ~SerialDreamPicoPortComms() {
		if (--connected_dev_count == 0) {
			// serial is no longer needed
			serial.reset();
		}
	}

	void changeSoftwareBus(int software_bus) override {
		this->software_bus = software_bus;
	}

	bool isConnected() override {
		return (serial && serial->is_open());
	}

	bool initialize(std::chrono::milliseconds timeout_ms) override {
		interface_version = 0.0;

		if (!isConnected()) {
			return false;
		}

		std::string buffer;
		asio::error_code error = serial->sendCmd("XV\n", buffer, timeout_ms);
		if (error) {
			WARN_LOG(INPUT, "DreamPicoPort[%d] send(XV) failed: %s", software_bus, error.message().c_str());
			return false;
		}

		if (0 == strncmp("*failed", buffer.c_str(), 7) || 0 == strncmp("0: failed", buffer.c_str(), 9)) {
			// Using a version of firmware before "XV" was available
			interface_version = 0.0;
		} else {
			try {
				interface_version = std::stod(buffer);
			}
			catch(const std::exception&) {
				WARN_LOG(
					INPUT,
					"DreamPicoPort[%d] command XV received invalid response: %s",
					software_bus,
					buffer.c_str()
				);
				return false;
			}
		}

		return true;
	}

	std::optional<std::vector<std::vector<std::array<uint32_t, 2>>>> getPeripherals(
		std::chrono::milliseconds timeout_ms
	) override {
		expansion_devs = 0;

		if (!isConnected()) {
			return std::nullopt;
		}

		std::vector<std::vector<std::array<uint32_t, 2>>> peripherals;

		MapleMsg msg;
		msg.command = MDCF_GetCondition;
		msg.destAP = (hardware_bus << 6) | 0x20;
		msg.originAP = hardware_bus << 6;
		msg.setData(MFID_0_Input);

		asio::error_code error = serial->sendMsg(msg, hardware_bus, msg, timeout_ms);
		if (error)
		{
			WARN_LOG(INPUT, "DreamPicoPort[%d] send(condition) failed: %s", software_bus, error.message().c_str());
			return peripherals; // assume simply controller not connected yet
		}

		expansion_devs = msg.originAP & 0x1f;

		if (interface_version >= 1.0) {
			// Can just use X?
			std::string buffer;
			error = serial->sendCmd("X?" + std::to_string(hardware_bus) + "\n", buffer, timeout_ms);
			if (error) {
				WARN_LOG(INPUT, "DreamPicoPort[%d] send(X?) failed: %s", software_bus, error.message().c_str());
				return std::nullopt;
			}

			{
				std::istringstream stream(buffer);
				stream.imbue(std::locale::classic());

				std::string outerGroup;
				while (std::getline(stream, outerGroup, ';')) {
					if (outerGroup.empty() || outerGroup == ",") continue;
					std::vector<std::array<uint32_t, 2>> outerList;
					std::istringstream outerStream(outerGroup.substr(1)); // Skip the leading '{'
					outerStream.imbue(std::locale::classic());
					std::string innerGroup;

					while (std::getline(outerStream, innerGroup, '}')) {
						if (innerGroup.empty() || innerGroup == ",") continue;
						std::array<uint32_t, 2> innerList = {{0, 0}};
						std::istringstream innerStream(innerGroup.substr(1)); // Skip the leading '{'
						innerStream.imbue(std::locale::classic());
						std::string number;
						std::size_t idx = 0;

						while (std::getline(innerStream, number, ',')) {
							if (!number.empty() && number[0] == '{') {
								number = number.substr(1);
							}
							uint32_t value;
							std::stringstream ss;
							ss.imbue(std::locale::classic());
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
			// Manually query each sub-peripheral
			peripherals.push_back({}); // skip controller since it's not used
			for (u32 i = 0; i < 2; ++i) {
				std::vector<std::array<uint32_t, 2>> portPeripherals;
				u8 port = (1 << i);
				if (expansion_devs & port) {
					msg.command = MDC_DeviceRequest;
					msg.destAP = (hardware_bus << 6) | port;
					msg.originAP = hardware_bus << 6;
					msg.size = 0;

					error = serial->sendMsg(msg, hardware_bus, msg, timeout_ms);
					if (error) {
						WARN_LOG(INPUT, "DreamPicoPort[%d] send(query) failed: %s", software_bus, error.message().c_str());
						return std::nullopt;
					}

					if (msg.size < 4) {
						WARN_LOG(INPUT, "DreamPicoPort[%d] read(query) failed: invalid size %d", software_bus, msg.size);
						return std::nullopt;
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

		return peripherals;
	}

	bool send(const MapleMsg& msg, std::chrono::milliseconds timeout_ms) override {
		if (!isConnected()) {
			return false;
		}

		asio::error_code ec = serial->sendMsg(msg, hardware_bus, timeout_ms);
		return !ec;
	}

    bool send(const MapleMsg& txMsg, MapleMsg& rxMsg, std::chrono::milliseconds timeout_ms) override {
		if (!isConnected()) {
			return false;
		}

		asio::error_code ec = serial->sendMsg(txMsg, hardware_bus, rxMsg, timeout_ms);
		return !ec;
	}

	void sendPort(std::chrono::milliseconds timeout_ms) override {
		// This will update the displayed port letter on the screen
		std::ostringstream s;
		s.imbue(std::locale::classic());
		s << "XP "; // XP is flycast "set port" command
		s << hardware_bus << " " << software_bus << "\n";
		serial->sendCmd(s.str(), timeout_ms);
	}
};

std::unique_ptr<DreamPicoPortSerialHandler> SerialDreamPicoPortComms::serial;
std::atomic<std::uint32_t> SerialDreamPicoPortComms::connected_dev_count = 0;

#endif // TARGET_UWP

class ApiDreamPicoPortComms : public DreamPicoPortComms
{
	//! All known dpp_api devices by serial number; already connected if set
	static std::unordered_map<std::string, std::weak_ptr<dpp_api::DppDevice>> all_dpp_api_devices;
	//! The mutex serializing access to all_dpp_api_devices
	static std::mutex all_dpp_api_devices_mutex;

	//! This is set when the device supports this new API
	std::shared_ptr<dpp_api::DppDevice> dpp_api_device;
	//! The bus ID dictated by flycast
	int software_bus = -1;
	//! The bus index of the hardware connection which will differ from the software bus
	int hardware_bus = -1;

public:
	ApiDreamPicoPortComms() = delete;

	ApiDreamPicoPortComms(const std::string& serial_number, int software_bus, int hardware_bus) :
		software_bus(software_bus),
		hardware_bus(hardware_bus)
	{
		std::lock_guard<std::mutex> lock(all_dpp_api_devices_mutex);

		auto iter = all_dpp_api_devices.find(serial_number);
		if (iter != all_dpp_api_devices.end()) {
			dpp_api_device = iter->second.lock();
			if (!dpp_api_device) {
				// The weak pointer was no longer valid; remove item from map
				all_dpp_api_devices.erase(iter);
			}
		}

		if (!dpp_api_device) {
			dpp_api::DppDevice::Filter dppFilter;
			dppFilter.serial = serial_number;
			dpp_api_device = dpp_api::DppDevice::find(dppFilter);
			if (!dpp_api_device) {
				WARN_LOG(
					INPUT,
					"DreamPicoPort[%d] new API connect failed: find failed for serial %s\n"
					"Update DreamPicoPort firmware to version 1.2.1 or later to use new, faster API",
					software_bus,
					serial_number.c_str()
				);
			}
			else if (!dpp_api_device->connect()) {
				WARN_LOG(
					INPUT,
					"DreamPicoPort[%d] new API connect failed: %s",
					software_bus,
					dpp_api_device->getLastErrorStr().c_str()
				);
				dpp_api_device.reset();
			} else {
				// Save this instance to the map
				all_dpp_api_devices.insert(std::make_pair(serial_number, dpp_api_device));
			}
		}

		if (dpp_api_device) {
			NOTICE_LOG(INPUT, "DreamPicoPort[%d] new API connected", software_bus);
		}
	}

	virtual ~ApiDreamPicoPortComms() = default;

	void changeSoftwareBus(int software_bus) override {
		this->software_bus = software_bus;
	}

	bool isConnected() override {
		return (dpp_api_device && dpp_api_device->isConnected());
	}

	bool initialize(std::chrono::milliseconds timeout_ms) override {
		if(!isConnected()) {
			return false;
		}

		// SDL workaround to ensure axis values are up-to-date
		dpp_api_device->send(dpp_api::msg::tx::RefreshGamepad{static_cast<std::uint8_t>(hardware_bus)});

		return true;
	}

	std::optional<std::vector<std::vector<std::array<uint32_t, 2>>>> getPeripherals(
		std::chrono::milliseconds timeout_ms
	) override {
		if (!isConnected()) {
			return std::nullopt;
		}

		std::vector<std::vector<std::array<uint32_t, 2>>> peripherals;

		dpp_api::msg::rx::GetDcSummary summary =
			dpp_api_device->sendSync(dpp_api::msg::tx::GetDcSummary{static_cast<uint8_t>(hardware_bus)});
		peripherals = summary.summary;

		return peripherals;
	}

	bool send(const MapleMsg& msg, std::chrono::milliseconds timeout_ms) override {
		if (!isConnected()) {
			return false;
		}

		dpp_api::msg::tx::Maple tx;
		tx.emu = true;
		const u32 data_size = msg.getDataSize();
		tx.packet.reserve(data_size + 4);
		tx.packet.push_back(msg.command);
		// Need to message the hardware bus instead of the software bus
		u8 hwDestAP = (hardware_bus << 6) | (msg.destAP & 0x3F);
		u8 hwOriginAP = (hardware_bus << 6) | (msg.originAP & 0x3F);
		tx.packet.push_back(hwDestAP);
		tx.packet.push_back(hwOriginAP);
		tx.packet.push_back(msg.size);
		tx.packet.insert(tx.packet.end(), msg.data, msg.data + data_size);
		const uint64_t id = dpp_api_device->send(tx);
		return (id != 0);
	}

    bool send(const MapleMsg& txMsg, MapleMsg& rxMsg, std::chrono::milliseconds timeout_ms) override {
		if (!isConnected()) {
			return false;
		}

		dpp_api::msg::tx::Maple tx;
		tx.emu = true;
		const u32 data_size = txMsg.getDataSize();
		tx.packet.reserve(data_size + 4);
		tx.packet.push_back(txMsg.command);
		// Need to message the hardware bus instead of the software bus
		u8 hwDestAP = (hardware_bus << 6) | (txMsg.destAP & 0x3F);
		u8 hwOriginAP = (hardware_bus << 6) | (txMsg.originAP & 0x3F);
		tx.packet.push_back(hwDestAP);
		tx.packet.push_back(hwOriginAP);
		tx.packet.push_back(txMsg.size);
		tx.packet.insert(tx.packet.end(), txMsg.data, txMsg.data + data_size);
		dpp_api::msg::rx::Maple rx = dpp_api_device->sendSync(tx, 100);
		if (rx.cmd != dpp_api::msg::rx::Msg::kCmdSuccess || rx.packet.size() < 4) {
			return false;
		}
		rxMsg.command = rx.packet[0];
		rxMsg.destAP = rx.packet[1];
		rxMsg.originAP = rx.packet[2];
		rxMsg.size = rx.packet[3];
		if (rx.packet.size() > 4) {
			memcpy(rxMsg.data, &rx.packet[4], (std::min)(rx.packet.size() - 4, sizeof(rxMsg.data)));
		}
		return (rxMsg.getDataSize() <= (rx.packet.size() - 4));
	}

	void sendPort(std::chrono::milliseconds timeout_ms) override {
		dpp_api::msg::tx::ChangePlayerDisplay changePlayerDisplay;
		changePlayerDisplay.idx = hardware_bus;
		changePlayerDisplay.toIdx = software_bus;
		dpp_api_device->send(changePlayerDisplay, nullptr);
	}
};

std::unordered_map<std::string, std::weak_ptr<dpp_api::DppDevice>> ApiDreamPicoPortComms::all_dpp_api_devices;
std::mutex ApiDreamPicoPortComms::all_dpp_api_devices_mutex;

class DreamPicoPortImp : public DreamPicoPort
{
	//! Implements communication interface to DreamPicoPort
	std::unique_ptr<class DreamPicoPortComms> dpp_comms;
	//! Current timeout in milliseconds
	std::chrono::milliseconds timeout_ms;
	//! The bus ID dictated by flycast
	int software_bus = -1;
    //! The queried interface version
    double interface_version = 0.0;
    //! The queried peripherals; for each function, index 0 is function code and index 1 is the function definition
    std::vector<std::vector<std::array<uint32_t, 2>>> peripherals;

	//! Static hardware information
	struct HardwareInfo
	{
		//! The bus index of the hardware connection which will differ from the software bus
		int hardware_bus = -1;
		//! true iff only a single devices was found when enumerating devices
		bool is_single_device = true;
		//! True when initial enumeration failed
		bool is_hardware_bus_implied = true;
		//! The located serial number of this device or empty string if could not be found
		std::string serial_number;
		//! If set, the determined unique ID of this device. If not set, the serial could not be parsed.
		std::string unique_id;

		//! @param[in] separator Separator string to use between name and port char
		//! @return unique name of this device using the given separator
		std::string getName(const std::string& separator = " ") const {
			std::string name = "DreamPicoPort";
			if (!is_hardware_bus_implied && !is_single_device) {
				const char portChar = ('A' + hardware_bus);
				name += separator + std::string(1, portChar);
			}
			return name;
		}
	};

	//! Hardware information determined on instantiation
	const HardwareInfo hw_info;

public:
    DreamPicoPortImp(int bus, int joystick_idx, SDL_Joystick* sdl_joystick) :
		DreamPicoPort(),
		software_bus(bus),
		hw_info(parseHardwareInfo(joystick_idx, sdl_joystick))
	{}

	~DreamPicoPortImp() {
		disconnect();
	}

	bool isForPhysicalController() override {
		return true;
	}

	bool send(const MapleMsg& msg) override {
		if (!dpp_comms) {
			return false;
		}

		return dpp_comms->send(msg, timeout_ms);
	}

    bool send(const MapleMsg& txMsg, MapleMsg& rxMsg) override {
		if (!dpp_comms) {
			return false;
		}

		return dpp_comms->send(txMsg, rxMsg, timeout_ms);
	}

	void sendGameId(int expansion, const std::string& gameId) override {
		if (!dpp_comms || hw_info.hardware_bus < 0)
				return;

		if (gameId.empty() || expansion < 0 || expansion > 1)
				return;

		MapleMsg msg{};
		msg.command = 33;
		msg.destAP = (hw_info.hardware_bus << 6) | (1u << expansion);
		msg.originAP = hw_info.hardware_bus << 6;
		msg.setWord(MFID_1_Storage, 0);

		const size_t idSize = 12;
		const size_t copyLength = std::min(gameId.size(), idSize);
		memcpy(&msg.data[4], gameId.data(), copyLength);
		msg.size = 4;

		dpp_comms->send(msg, timeout_ms);
	}

	void gameTermination() override {
		// Need a short delay to wait for last screen draw to complete
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		// Reset screen to selected port
		sendPort();
	}

	int getBus() const override {
		return software_bus;
	}

	//! Transform flycast port index into DreamPicoPort port index
	static int fcPortToDppPort(int forPort) {
		// Flycast uses port index 5 for main peripheral and 0 is the first sub-peripheral slot
		// DreamPicoPort uses port index 0 for main peripheral and 1 is the first sub-peripheral slot
		if (forPort >= 5) {
			return 0;
		} else {
			return forPort + 1;
		}
	}

    u32 getFunctionCode(int forPort) const override {
		forPort = fcPortToDppPort(forPort);
		u32 mask = 0;
		if ((int)peripherals.size() > forPort) {
			for (const auto& peripheral : peripherals[forPort]) {
				mask |= peripheral[0];
			}
		}
		// swap bytes to get the correct function code
		return SWAP32(mask);
	}

	std::array<u32, 3> getFunctionDefinitions(int forPort) const override {
		forPort = fcPortToDppPort(forPort);
		std::array<u32, 3> arr{0, 0, 0};
		if ((int)peripherals.size() > forPort) {
			std::size_t idx = 0;
			for (const auto& peripheral : peripherals[forPort]) {
				arr[idx++] = SWAP32(peripheral[1]);
				if (idx >= 3) break;
			}
		}
		return arr;
	}

	int getDefaultBus() const override {
		if (!hw_info.is_hardware_bus_implied && !hw_info.is_single_device) {
			return hw_info.hardware_bus;
		} else {
			// Value of -1 means to use enumeration order
			return -1;
		}
	}

	void setDefaultMapping(const std::shared_ptr<InputMapping>& mapping) const override {
		// Since this is a real DC controller, no deadzone adjustment is needed
		mapping->dead_zone = 0.0f;
		// Map the things not set by SDL
		mapping->set_button(DC_BTN_C, 2);
		mapping->set_button(DC_BTN_Z, 5);
		mapping->set_button(DC_BTN_D, 10);
		mapping->set_button(DC_DPAD2_UP, 9);
		mapping->set_button(DC_DPAD2_DOWN, 8);
		mapping->set_button(DC_DPAD2_LEFT, 7);
		mapping->set_button(DC_DPAD2_RIGHT, 6);
	}

	const char *getButtonName(u32 code) const override {
		using namespace i18n;
		switch (code) {
			// Coincides with buttons setup in setDefaultMapping
			case 2: return "C";
			case 5: return "Z";
			case 10: return "D";
			case 9: return T("DPad2 Up");
			case 8: return T("DPad2 Down");
			case 7: return T("DPad2 Left");
			case 6: return T("DPad2 Right");

			// These buttons are normally not physically accessible but are mapped on DreamPicoPort
			case 12: return T("VMU1 A");
			case 15: return T("VMU1 B");
			case 16: return T("VMU1 Up");
			case 17: return T("VMU1 Down");
			case 18: return T("VMU1 Left");
			case 19: return T("VMU1 Right");

			default: return nullptr; // no override
		}
	}

	std::string getUniqueId() const override {
		return hw_info.unique_id;
	}

	void changeBus(int newBus) override {
		software_bus = newBus;
		if (dpp_comms) {
			dpp_comms->changeSoftwareBus(software_bus);
		}
	}

	std::string getName() const override {
		return hw_info.getName();
	}

	bool needsRefresh() override {
		// TODO: implementing this method may also help to support hot plugging of VMUs/jump packs here.
		return false;
	}

	bool isConnected() override {
		return (dpp_comms && dpp_comms->isConnected());
	}

	void connect() override {
		// Timeout is 1 second while establishing connection
		timeout_ms = std::chrono::seconds(1);

		if (isConnected()) {
			sendPort();
			return;
		}

		// Attempt to connect to new API
		if (!hw_info.serial_number.empty()) {
			dpp_comms = std::make_unique<ApiDreamPicoPortComms>(
				hw_info.serial_number,
				software_bus,
				hw_info.hardware_bus
			);

			if (!dpp_comms->isConnected() || !dpp_comms->initialize(timeout_ms)) {
				dpp_comms.reset();
			}
		} else {
			NOTICE_LOG(INPUT, "Serial number for DreamPicoPort[%d] not found", software_bus);
		}

	#ifndef TARGET_UWP
		if (!dpp_comms) {
			NOTICE_LOG(
				INPUT,
				"Could not find DppDevice for DreamPicoPort[%d]; falling back to serial interface",
				software_bus
			);
			dpp_comms = std::make_unique<SerialDreamPicoPortComms>(software_bus, hw_info.hardware_bus);
			if (!dpp_comms->isConnected() || !dpp_comms->initialize(timeout_ms)) {
				dpp_comms.reset();
			}
		}
	#endif

		if (isConnected()) {
			sendPort();
		} else {
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

		if (software_bus >= 0 && static_cast<std::size_t>(software_bus) < config::MapleExpansionDevices.size()) {
			u32 portOneFn = getFunctionCode(0);
			if (portOneFn & MFID_1_Storage) {
				config::MapleExpansionDevices[software_bus][0] = MDT_SegaVMU;
				++vmuCount;
			}
			else {
				config::MapleExpansionDevices[software_bus][0] = MDT_None;
			}

			u32 portTwoFn = getFunctionCode(1);
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
		}

		NOTICE_LOG(
			INPUT,
			"Connected to DreamPicoPort[%d]: Type:%s, VMU:%d, Jump Pack:%d",
			software_bus,
			getName().c_str(),
			vmuCount,
			vibrationCount
		);
	}

	void disconnect() override {
		dpp_comms.reset();
	}

    void sendPort() {
		if (dpp_comms) {
			dpp_comms->sendPort(timeout_ms);
		}
	}

	int hardwareBus() const {
		return hw_info.hardware_bus;
	}

	bool isHardwareBusImplied() const {
		return hw_info.is_hardware_bus_implied;
	}

	bool isSingleDevice() const {
		return hw_info.is_single_device;
	}

private:
	//! Only to be called during instantiation to determine hardware information
	//! @param[in] joystick_idx SDL joystick index
	//! @param[in] sdl_joystick SDL joystick object
	static HardwareInfo parseHardwareInfo(int joystick_idx, SDL_Joystick* sdl_joystick) {
#if defined(_WIN32)
		// Workaround: Getting the instance ID here fixes some sort of L/R trigger bug in Windows dinput for some reason
		(void)SDL_JoystickGetDeviceInstanceID(joystick_idx);
#endif

		HardwareInfo hw_info;

		// Set the serial number if found by SDL Joystick
		const char* joystick_serial = SDL_JoystickGetSerial(sdl_joystick);
		if (joystick_serial) {
			hw_info.serial_number = joystick_serial;
		} else {
			// Version 1.2.0 and later embeds serial in name as a workaround for MacOS and Linux
			// Serial is expected between a dash (-) and space ( ) character or until end of string
			const char* joystick_name = SDL_JoystickName(sdl_joystick);
			if (joystick_name) {
				std::string name_str(joystick_name);
				size_t dash_pos = name_str.find('-');
				if (dash_pos != std::string::npos) {
					size_t start_pos = dash_pos + 1;
					size_t end_pos = name_str.find(' ', start_pos);
					if (end_pos == std::string::npos) {
						end_pos = name_str.length();
					}
					// Serials are normally 16 characters, but check for at least 10 to account for any future changes
					if ((start_pos + 10) <= end_pos) {
						hw_info.serial_number = name_str.substr(start_pos, end_pos - start_pos);
					}
				}
			}
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
				hw_info.hardware_bus = 0;
				hw_info.is_hardware_bus_implied = false;
				hw_info.is_single_device = true;
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
							hw_info.is_single_device = true;
							hw_info.hardware_bus = 0;
							hw_info.is_hardware_bus_implied = false;
						} else {
							hw_info.is_single_device = false;
							if (my_dev->release_number < 0x0102) {
								// Interfaces go in decending order
								hw_info.hardware_bus = (count - (my_dev->interface_number % 4) - 1);
								hw_info.is_hardware_bus_implied = false;
							} else {
								// Version 1.02 of interface will make interfaces in ascending order
								hw_info.hardware_bus = (my_dev->interface_number % 4);
								hw_info.is_hardware_bus_implied = false;
							}
						}
					}
				}
			}

			// Set serial number if found in SDL_hid
			if (my_dev) {
				if (hw_info.serial_number.empty() && my_dev->serial_number) {
					int len = WideCharToMultiByte(CP_UTF8, 0, my_dev->serial_number, -1, nullptr, 0, nullptr, nullptr);
					if (len > 0) {
						std::vector<char> buffer(len);
						WideCharToMultiByte(CP_UTF8, 0, my_dev->serial_number, -1, buffer.data(), len, nullptr, nullptr);
						hw_info.serial_number = std::string(buffer.data());
					}
				}
			}

			SDL_hid_free_enumeration(devs);
		}

#endif // #if defined(_WIN32)

		if (hw_info.hardware_bus < 0) {
			// The number of buttons gives a clue as to what index the controller is
			int nbuttons = SDL_JoystickNumButtons(sdl_joystick);

			if (nbuttons >= 32 || nbuttons <= 27) {
				// Older version of firmware or single player
				hw_info.hardware_bus = 0;
				hw_info.is_hardware_bus_implied = true;
				hw_info.is_single_device = true;
			}
			else {
				hw_info.hardware_bus = 31 - nbuttons;
				hw_info.is_hardware_bus_implied = false;
				hw_info.is_single_device = false;
			}
		}

		hw_info.unique_id.clear();
		if (!hw_info.is_hardware_bus_implied && !hw_info.serial_number.empty()) {
			// Locking to name, which includes A-D, plus serial number will ensure correct enumeration every time
			hw_info.unique_id = std::string("sdl_") + hw_info.getName("") + std::string("_") + hw_info.serial_number;
		}

		return hw_info;
	}

    bool queryPeripherals() {
		peripherals.clear();

		if (!isConnected()) {
			return false;
		}

		std::optional<std::vector<std::vector<std::array<uint32_t, 2>>>> optPeriph = dpp_comms->getPeripherals(timeout_ms);

		if (!optPeriph) {
			return false;
		}

		peripherals = std::move(optPeriph.value());

		return true;
	}
};

std::shared_ptr<DreamPicoPort> DreamPicoPort::create_shared(int bus, int joystick_idx, SDL_Joystick* sdl_joystick) {
	return std::make_shared<DreamPicoPortImp>(bus, joystick_idx, sdl_joystick);
}
