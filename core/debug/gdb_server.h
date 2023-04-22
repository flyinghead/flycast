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

namespace debugger {

// exception thrown in response to trap
struct Stop { };

	static const int DEFAULT_PORT = 3263;

#ifdef GDB_SERVER

	void init(int port);
	void term();
	void run();
	void debugTrap(u32 event);
	void subroutineCall();
	void subroutineReturn();

#else
	static inline void init(int port) {}
	static inline void term() {}
	static inline void run() {}
	static inline void debugTrap(u32 event) {}
	static inline void subroutineCall() {}
	static inline void subroutineReturn() {}
#endif
}
