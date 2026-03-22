/*
	Copyright 2025 flyinghead

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
#include <asio.hpp>
#include <stdio.h>
#include "LogManager.h"

class NetworkListener : public LogListener
{
public:
	NetworkListener(const std::string& dest)
	{
		if (dest.empty())
			return;
		std::string host;
		std::string port("31667");
		auto colon = dest.find(':');
		if (colon != std::string::npos) {
			port = dest.substr(colon + 1);
			host = dest.substr(0, colon);
		}
		else {
			host = dest;
		}
		asio::ip::udp::resolver resolver(io_context);
		asio::error_code ec;
		auto it = resolver.resolve(host, port, ec);
		if (ec || it.empty()) {
			fprintf(stderr, "Unknown hostname %s: %s\n", host.c_str(), ec.message().c_str());
		}
		else
		{
			asio::ip::udp::endpoint endpoint = *it.begin();
			socket.connect(endpoint, ec);
			if (ec)
				fprintf(stderr, "Connect to log server failed: %s\n", ec.message().c_str());
		}
	}

	void Log(LogTypes::LOG_LEVELS level, const char* msg) override
	{
		if (!socket.is_open())
			return;
		const char *reset_attr = "";
		const char *color_attr = "";

		switch (level)
		{
		case LogTypes::LOG_LEVELS::LNOTICE:
			// light green
			color_attr = "\x1b[92m";
			reset_attr = "\x1b[0m";
			break;
		case LogTypes::LOG_LEVELS::LERROR:
			// light red
			color_attr = "\x1b[91m";
			reset_attr = "\x1b[0m";
			break;
		case LogTypes::LOG_LEVELS::LWARNING:
			// light yellow
			color_attr = "\x1b[93m";
			reset_attr = "\x1b[0m";
			break;
		default:
			break;
		}
		char temp[MAX_MSGLEN];
		snprintf(temp, sizeof(temp) - 1, "%s%s%s", color_attr, msg, reset_attr);
		temp[sizeof(temp) - 1] = '\0';
		asio::error_code ec;
		socket.send(asio::buffer(temp, strlen(temp)), 0, ec);
	}

private:
	asio::io_context io_context;
	asio::ip::udp::socket socket { io_context };
};
