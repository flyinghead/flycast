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
#pragma once
#include "common.h"

class IsoFs
{
public:
	class Entry
	{
	public:
		virtual bool isDirectory() const = 0;
		virtual ~Entry() = default;
		const std::string& getName() const { return name; }

	protected:
		Entry(IsoFs *fs) : fs(fs) {}

		IsoFs *fs;
		std::string name;
		u32 startFad = 0;
		u32 len = 0;

		friend class IsoFs;
	};

	class Directory final : public Entry
	{
	public:
		bool isDirectory() const override { return true; }
		Entry *getEntry(const std::string& name);
		std::vector<Entry*> list();

	private:
		Directory(IsoFs *fs) : Entry(fs) {}
		void reset();
		Entry *nextEntry();

		std::vector<u8> data;
		u32 index = 0;

		friend class IsoFs;
	};

	class File final : public Entry
	{
	public:
		bool isDirectory() const override { return false; }
		u32 getSize() const { return len; }
		u32 read(u8 *buf, u32 size, u32 offset = 0) const;

	private:
		File(IsoFs *fs) : Entry(fs) {}

		friend class IsoFs;
	};

	IsoFs(Disc *disc);
	Directory *getRoot();

private:
	Disc *disc;
	u32 baseFad;
};
