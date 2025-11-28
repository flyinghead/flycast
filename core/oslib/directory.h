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
#include "nowide/config.hpp"
#include <dirent.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <io.h>
#define W_OK 2
#define R_OK 4
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
    DIR *opendir(char const *dirname);
    dirent *readdir(DIR *dirstream);
    int closedir(DIR *dirstream);
    int stat(const char *filename, struct stat *buf);
    int access(const char *filename, int how);
    int mkdir(const char *path, mode_t mode);
#endif
}
