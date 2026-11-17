/*
	Copyright 2026 flyinghead

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
#include "oslib/directory.h"
#include "log/LogManager.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

namespace flycast {

int rename(const char *oldpath, const char *newpath)
{
	if (::rename(oldpath, newpath) == 0)
		return 0;
	if (::unlink(newpath))
		ERROR_LOG(COMMON, "Can't unlink %s: errno %d", newpath, errno);
	return ::rename(oldpath, newpath);
}

}
