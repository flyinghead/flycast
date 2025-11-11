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
#include "directory.h"
#ifdef NOWIDE_WINDOWS
#include "types.h"
#include "nowide/stackstring.hpp"
#include <string>
#include <cstring>

#ifdef TARGET_UWP
#include <windows.h>
// Define dirent type constants if not already defined
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif
#endif // UWP

namespace flycast
{

#ifdef TARGET_UWP
struct DIR_UWP {
    HANDLE hFind;
    WIN32_FIND_DATAW findData;
    bool firstRead;
    bool hasNext;
};

DIR *opendir(char const *dirname)
{
    std::string searchPath(dirname);
    if (!searchPath.empty() && searchPath.back() != '\\' && searchPath.back() != '/')
        searchPath += "\\";
    searchPath += "*";

    nowide::wstackstring wsearchPath;
    if (!wsearchPath.convert(searchPath.c_str())) {
        errno = EINVAL;
        return nullptr;
    }

    DIR_UWP* dir = new DIR_UWP;
    dir->hFind = FindFirstFileExFromAppW(wsearchPath.get(), FindExInfoBasic, &dir->findData,
                                          FindExSearchNameMatch, nullptr, 0);

    if (dir->hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        delete dir;
        if (error == ERROR_ACCESS_DENIED)
            errno = EACCES;
        else if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            errno = ENOENT;
        else
            errno = error;
        return nullptr;
    }

    dir->firstRead = true;
    dir->hasNext = true;
    return (DIR*)dir;
}
#else
DIR *opendir(char const *dirname)
{
    nowide::wstackstring wname;
    if (!wname.convert(dirname)) {
        errno = EINVAL;
        return nullptr;
    }
    return (DIR *)::_wopendir(wname.get());
}
#endif

#ifdef TARGET_UWP
dirent *readdir(DIR *dirstream)
{
    DIR_UWP* dir = (DIR_UWP*)dirstream;

    if (!dir->hasNext)
        return nullptr;

    static dirent d;

    if (dir->firstRead) {
        dir->firstRead = false;
    } else {
        if (!FindNextFileW(dir->hFind, &dir->findData)) {
            dir->hasNext = false;
            return nullptr;
        }
    }

    // Skip hidden files/directories on UWP
    // if (dir->findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
    //     return readdir(dirstream); // Skip to next entry

    nowide::stackstring name;
    if (!name.convert(dir->findData.cFileName)) {
        errno = EINVAL;
        return nullptr;
    }

    d.d_ino = 0;
    d.d_off = 0;
    // Set d_type based on file attributes
    if (dir->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        d.d_type = DT_DIR;
    else
        d.d_type = DT_REG;
    d.d_reclen = sizeof(dirent);
    strncpy(d.d_name, name.get(), sizeof(d.d_name) - 1);
    d.d_name[sizeof(d.d_name) - 1] = '\0';
    d.d_namlen = strlen(d.d_name);

    return &d;
}

int closedir(DIR *dirstream)
{
    DIR_UWP* dir = (DIR_UWP*)dirstream;
    if (dir->hFind != INVALID_HANDLE_VALUE)
        FindClose(dir->hFind);
    delete dir;
    return 0;
}
#else
dirent *readdir(DIR *dirstream)
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
	strncpy(d.d_name, name.get(), sizeof(d.d_name) - 1);
	d.d_name[sizeof(d.d_name) - 1] = '\0';
	d.d_namlen = strlen(d.d_name);

	return &d;
}

int closedir(DIR *dirstream)
{
	return ::_wclosedir((_WDIR *)dirstream);
}
#endif

inline static void _set_errno(int error)
{
#ifdef _MSC_VER
	::_set_errno (error);
#else
	errno = error;
#endif
}

int stat(const char *filename, struct stat *buf)
{
	nowide::wstackstring wname;
    if (!wname.convert(filename)) {
    	_set_errno(EINVAL);
    	return -1;
    }
#ifdef TARGET_UWP
    WIN32_FILE_ATTRIBUTE_DATA attrs;
	bool rc = GetFileAttributesExFromAppW(wname.get(),  GetFileExInfoStandard, &attrs);
	if (!rc)
	{
		_set_errno(GetLastError());
		return -1;
	}
	memset(buf, 0, sizeof(struct stat));
	if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		buf->st_mode = S_IFDIR;
	else
		buf->st_mode = S_IFREG;
	buf->st_size = attrs.nFileSizeLow;

	constexpr UINT64 WINDOWS_TICK = 10000000u;
	constexpr UINT64 SEC_TO_UNIX_EPOCH = 11644473600llu;
    buf->st_ctime = (unsigned)(((UINT64)attrs.ftCreationTime.dwLowDateTime | ((UINT64)attrs.ftCreationTime.dwHighDateTime << 32)) / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
    buf->st_mtime = (unsigned)(((UINT64)attrs.ftLastWriteTime.dwLowDateTime | ((UINT64)attrs.ftLastWriteTime.dwHighDateTime << 32)) / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
    buf->st_atime =(unsigned)(((UINT64)attrs.ftLastAccessTime.dwLowDateTime | ((UINT64)attrs.ftLastAccessTime.dwHighDateTime << 32)) / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);

    return 0;
#else
    struct _stat _st;
    int rc = _wstat(wname.get(), &_st);
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
#endif
}

int access(const char *filename, int how)
{
	nowide::wstackstring wname;
    if (!wname.convert(filename)) {
    	_set_errno(EINVAL);
    	return -1;
    }
#ifdef TARGET_UWP
    // For UWP just check if the file exists
    WIN32_FILE_ATTRIBUTE_DATA attrs;
	bool rc = GetFileAttributesExFromAppW(wname.get(), GetFileExInfoStandard, &attrs);
	if (!rc)
	{
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			_set_errno(ENOENT);
		else if (error == ERROR_ACCESS_DENIED)
			_set_errno(EACCES);
		else
			_set_errno(error);
		return -1;
	}
	// Only fail on write access check for readonly files
	// With broadFileSystemAccess, we generally have read access
	if (how == W_OK && (attrs.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
	{
		_set_errno(EACCES);
		return -1;
	}
	return 0;
#else
    return ::_waccess(wname.get(), how);
#endif
}

int mkdir(const char *path, mode_t mode) {
	nowide::wstackstring wpath;
    if (!wpath.convert(path)) {
    	errno = EINVAL;
    	return -1;
    }
    return ::_wmkdir(wpath.get());
}

}	// namespace flycast
#endif	// NOWIDE_WINDOWS

