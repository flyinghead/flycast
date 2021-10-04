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
#include "build.h"
#include <string>

namespace lua
{
#ifdef USE_LUA

void init();
void term();
void exec(const std::string& path);
void vblank();
void overlay();

#else

inline static void init() {}
inline static void term() {}
inline static void exec(const std::string& path) {}
inline static void vblank() {}
inline static void overlay() {}

#endif
}
