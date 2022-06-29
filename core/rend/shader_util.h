/*
	Copyright 2021 flyinghead

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
#include <string>
#include <vector>

class ShaderSource
{
public:
	ShaderSource(const std::string& header = "") : header(header) {}

	ShaderSource& setHeader(const std::string& source) {
		header = source;
		return *this;
	}

	ShaderSource& addSource(const std::string& source) {
		segments.push_back(source);
		return *this;
	}

	ShaderSource& addConstant(const std::string& name, const std::string& value = "0") {
		constants.emplace_back(name, value);
		return *this;
	}
	ShaderSource& addConstant(const std::string& name, int value) {
		addConstant(name, std::to_string(value));
		return *this;
	}
	ShaderSource& setConstant(const std::string& name, const std::string& value) {
		for (Constant& c : constants)
			if (c.name == name)
			{
				c.value = value;
				return *this;
			}
		return addConstant(name, value);
	}
	ShaderSource& setConstant(const std::string& name, int value) {
		setConstant(name, std::to_string(value));
		return *this;
	}

	std::string generate()
	{
		std::string result(header + "\n");
		for (const Constant& c : constants)
			result += "#define " + c.name + " " + c.value + "\n";
		for (const std::string& s : segments)
			result += s + "\n";
		return result;
	}

private:
	struct Constant
	{
		Constant(const std::string& name, const std::string& defaultValue = "0") : name(name), value(defaultValue) {}

		const std::string name;
		std::string value;
	};

	std::string header;
	std::vector<std::string> segments;
	std::vector<Constant> constants;
};
