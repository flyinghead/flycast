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

#include <vector>

namespace hostfs
{

struct FileInfo
{
	FileInfo() = default;
	FileInfo(const std::string& name, const std::string& path, bool isDirectory, size_t size = 0, bool isWritable = false)
		: name(name), path(path), isDirectory(isDirectory), size(size), isWritable(isWritable) {}

	std::string name;
	std::string path;
	bool isDirectory = false;
	size_t size = 0;
	bool isWritable = false;
};

class StorageException : public FlycastException
{
public:
	StorageException(const std::string& reason) : FlycastException(reason) {}
};

class Storage
{
public:
	virtual bool isKnownPath(const std::string& path) = 0;
	virtual std::vector<FileInfo> listContent(const std::string& path) = 0;
	virtual FILE *openFile(const std::string& path, const std::string& mode) = 0;
	virtual std::string getParentPath(const std::string& path) = 0;
	virtual std::string getSubPath(const std::string& reference, const std::string& subpath) = 0;
	virtual FileInfo getFileInfo(const std::string& path) = 0;

	virtual ~Storage() = default;
};

class CustomStorage : public Storage
{
public:
	virtual void addStorage(bool isDirectory, bool writeAccess, void (*callback)(bool cancelled, std::string selectedPath)) = 0;
};

class AllStorage : public Storage
{
public:
	bool isKnownPath(const std::string& path) override { return true; }

	std::vector<FileInfo> listContent(const std::string& path) override;
	FILE *openFile(const std::string& path, const std::string& mode) override;
	std::string getParentPath(const std::string& path) override;
	std::string getSubPath(const std::string& reference, const std::string& subpath) override;
	FileInfo getFileInfo(const std::string& path) override;
	std::string getDefaultDirectory();
};

AllStorage& storage();
void addStorage(bool isDirectory, bool writeAccess, void (*callback)(bool cancelled, std::string selectedPath));

// iterate depth-first over the files contained in a folder hierarchy
class DirectoryTree
{
public:
	class iterator
	{
	private:
		iterator(const FileInfo dir) {
			dirs.push_back(dir);
			advance();
		}
		iterator() { }

	public:
		const FileInfo *operator->() {
			if (dirs.empty() && currentDir.empty())
				throw std::runtime_error("null iterator");
			return &currentDir[index];
		}

		const FileInfo& operator*() const {
			if (dirs.empty() && currentDir.empty())
				throw std::runtime_error("null iterator");
			return currentDir[index];
		}

		// Prefix increment
		iterator& operator++() {
			advance();
			return *this;
		}

		// Basic (in)equality implementations, just intended to work when comparing with end() or this
		friend bool operator==(const iterator& a, const iterator& b) {
			return a.dirs.size() == b.dirs.size() && a.currentDir.size() == b.currentDir.size();
		}

		friend bool operator!=(const iterator& a, const iterator& b) {
			return a.dirs.size() != b.dirs.size() || a.currentDir.size() != b.currentDir.size();
		}

	private:
		void advance()
		{
			if (!currentDir.empty())
				index++;
			while (true)
			{
				if (index >= currentDir.size())
				{
					index = 0;
					currentDir.clear();
					while (!dirs.empty() && currentDir.empty())
					{
						FileInfo& dir = dirs.back();
						currentDir = storage().listContent(dir.path);
						dirs.pop_back();
					}
				}
				if (currentDir.empty())
					return;
				while (index < currentDir.size() && currentDir[index].isDirectory)
				{
					dirs.push_back(currentDir[index]);
					index++;
				}
				if (index < currentDir.size())
					break;
			}
		}

		std::vector<FileInfo> dirs;
		std::vector<FileInfo> currentDir;
		size_t index = 0;

		friend class DirectoryTree;
	};

	DirectoryTree(const std::string& root) : root(root) {
	}

	iterator begin()
	{
		FileInfo entry(root, root, true);
		return { entry };
	}
	iterator end()
	{
		return { };
	}

private:
	const std::string& root;
};

}	// namespace hostfs
