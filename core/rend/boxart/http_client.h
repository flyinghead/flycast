/*
	Copyright 2022 flyinghead

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
#include <string>
#include <vector>
#include <cctype>
#include <iomanip>
#include <sstream>
#include "types.h"

namespace http {

void init();
void term();

int get(const std::string& url, std::vector<u8>& content, std::string& content_type);

static inline int get(const std::string& url, std::vector<u8>& content) {
	 std::string contentType;
	 return get(url, content, contentType);
}

static inline bool success(int status) {
	return status >= 200 && status < 300;
}

static inline std::string urlEncode(const std::string& value)
{
	std::ostringstream escaped;
	escaped.fill('0');
	escaped << std::hex;

	for (char c : value)
	{
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			// Keep alphanumeric and other accepted characters intact
			escaped << c;
		}
		else
		{
			// Any other characters are percent-encoded
			escaped << std::uppercase;
			escaped << '%' << std::setw(2) << int((u8)c);
			escaped << std::nouppercase;
		}
	}

	return escaped.str();
}

}
