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
#include "types.h"
#include "nowide/config.hpp"
#include "nowide/stackstring.hpp"
#include <dirent.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <io.h>
#define R_OK   4
typedef unsigned short mode_t;
#else
#include <unistd.h>
#endif

namespace flycast {
#if !defined(NOWIDE_WINDOWS)
    using ::opendir;
    using ::readdir;
    using ::closedir;
    using ::stat;
    using ::access;
    using ::mkdir;
#else

inline DIR *opendir(char const *dirname)
{
    nowide::wstackstring wname;
    if (!wname.convert(dirname)) {
        errno = EINVAL;
        return nullptr;
    }
    return (DIR *)::_wopendir(wname.c_str());
}

inline dirent *readdir(DIR *dirstream)
{
	_WDIR *wdir = (_WDIR *)dirstream;
	_wdirent *wdirent = ::_wreaddir(wdir);

	if (wdirent == nullptr)
		return nullptr;

    nowide::stackstring name;
    if (!name.convert(wdirent->d_name)) {
        errno = EINVAL;
        return nullptr;
    }
    static dirent d;
	d.d_ino = wdirent->d_ino;
	d.d_off = wdirent->d_off;
	d.d_type = wdirent->d_type;
	d.d_reclen = sizeof(dirent);
	d.d_namlen = wdirent->d_namlen;
	strcpy(d.d_name, name.c_str());

	return &d;
}

inline int closedir(DIR *dirstream)
{
	return ::_wclosedir((_WDIR *)dirstream);
}

inline int stat(const char *filename, struct stat *buf)
{
	nowide::wstackstring wname;
    if (!wname.convert(filename)) {
    	errno = EINVAL;
    	return -1;
    }
    struct _stat _st;
    int rc = _wstat(wname.c_str(), &_st);
    buf->st_ctime = _st.st_ctime;
    buf->st_mtime = _st.st_mtime;
    buf->st_atime = _st.st_atime;
    buf->st_dev = _st.st_dev;
    buf->st_ino = _st.st_ino;
    buf->st_uid = _st.st_uid;
    buf->st_gid = _st.st_gid;
    buf->st_mode = _st.st_mode;
    buf->st_nlink = _st.st_nlink;
    buf->st_rdev = _st.st_rdev;
    buf->st_size = _st.st_size;

    return rc;
}

inline int access(const char *filename, int how)
{
	nowide::wstackstring wname;
    if (!wname.convert(filename)) {
    	errno = EINVAL;
    	return -1;
    }
    return ::_waccess(wname.c_str(), how);
}

inline int mkdir(const char *path, mode_t mode) {
	nowide::wstackstring wpath;
    if (!wpath.convert(path)) {
    	errno = EINVAL;
    	return -1;
    }
    return ::_wmkdir(wpath.c_str());
}
#endif
}

// iterate depth-first over the files contained in a folder hierarchy
class DirectoryTree
{
public:
	struct item {
		std::string name;
		std::string parentPath;
	};

	class iterator
	{
	private:
		iterator(DIR *dir, std::string pathname) {
			if (dir != nullptr)
			{
				dirs.push_back(dir);
				pathnames.push_back(pathname);
				advance();
			}
		}

	public:
		~iterator() {
			for (DIR *dir : dirs)
				flycast::closedir(dir);
		}

		const item *operator->() {
			if (direntry == nullptr)
				throw std::runtime_error("null iterator");
			return &currentItem;
		}

		const item& operator*() const {
			if (direntry == nullptr)
				throw std::runtime_error("null iterator");
			return currentItem;
		}

		// Prefix increment
		iterator& operator++() {
			advance();
			return *this;
		}

		// Basic (in)equality implementations, just intended to work when comparing with end() or this
		friend bool operator==(const iterator& a, const iterator& b) {
			return a.direntry == b.direntry;
		}

		friend bool operator!=(const iterator& a, const iterator& b) {
			return a.direntry != b.direntry;
		}

	private:
		void advance()
		{
			while (!dirs.empty())
			{
				direntry = flycast::readdir(dirs.back());
				if (direntry == nullptr)
				{
					flycast::closedir(dirs.back());
					dirs.pop_back();
					pathnames.pop_back();
					continue;
				}
				currentItem.name = direntry->d_name;
				if (currentItem.name == "." || currentItem.name == "..")
					continue;
				std::string childPath = pathnames.back() + "/" + currentItem.name;
				bool isDir = false;
#ifndef _WIN32
				if (direntry->d_type == DT_DIR)
					isDir = true;
				else if (direntry->d_type == DT_UNKNOWN || direntry->d_type == DT_LNK)
#endif
				{
					struct stat st;
					if (flycast::stat(childPath.c_str(), &st) != 0)
						continue;
					if (S_ISDIR(st.st_mode))
						isDir = true;
				}
				if (!isDir)
				{
					currentItem.parentPath = pathnames.back();
					break;
				}

				DIR *childDir = flycast::opendir(childPath.c_str());
				if (childDir == nullptr)
				{
					INFO_LOG(COMMON, "Cannot read directory '%s'", childPath.c_str());
				}
				else
				{
					dirs.push_back(childDir);
					pathnames.push_back(childPath);
				}
			}
		}

		std::vector<DIR *> dirs;
		std::vector<std::string> pathnames;
		dirent *direntry = nullptr;
		item currentItem;

		friend class DirectoryTree;
	};

	DirectoryTree(const std::string& root) : root(root) {
	}

	iterator begin()
	{
		DIR *dir = flycast::opendir(root.c_str());
		if (dir == nullptr)
			INFO_LOG(COMMON, "Cannot read directory '%s'", root.c_str());

		return iterator(dir, root);
	}
	iterator end()
	{
		return iterator(nullptr, root);
	}

private:
	const std::string& root;
};
