/*
	Copyright 2023 flyinghead

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
#include "types.h"
#include <memory>

#ifdef NAOMI_MULTIBOARD
struct SharedMemory;

class Multiboard
{
public:
	Multiboard();
	~Multiboard();

	u32 readG1(u32 addr, u32 size);
	void writeG1(u32 addr, u32 size, u32 data);

	u32 readG2Ext(u32 addr, u32 size);
	void writeG2Ext(u32 addr, u32 size, u32 data);

	bool dmaStart();
	void reset();
	void syncWait();

private:
	bool isMaster() const { return boardId == 0; }
	bool isSlave() const { return boardId != 0; }
	void startSlave();

	int boardId = 0;
	u32 offset = 0;
	u32 reg9c = 0;
	SharedMemory *sharedMem;
#ifdef _WIN32
	HANDLE mapFile;
#else
	std::string sharedMemFileName;
#endif
	int boardCount = 0;
	bool slaveStarted = false;
	int schedId;
	u16 g2status = 0;
	u16 dmaOffset = 0;
};

#else // !NAOMI_MULTIBOARD

class Multiboard
{
public:
	u32 readG1(u32 addr, u32 size) {
		return 0;
	}
	void writeG1(u32 addr, u32 size, u32 data) { }

	u32 readG2Ext(u32 addr, u32 size) {
		return 0;
	}
	void writeG2Ext(u32 addr, u32 size, u32 data) { }

	bool dmaStart() {
		return false;
	}
	void reset() { }

	void syncWait() { }
};

#endif // !NAOMI_MULTIBOARD
