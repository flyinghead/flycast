//
// Copyright (c) 2020-2022 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define NOWIDE_SOURCE

#if defined(__MINGW32__) && defined(__STRICT_ANSI__)
// Need the _w* functions which are extensions on MinGW but not on MinGW-w64
#include <_mingw.h>
#ifndef __MINGW64_VERSION_MAJOR
#undef __STRICT_ANSI__
#endif
#endif

#include <nowide/config.hpp>

#ifdef NOWIDE_WINDOWS

#include <nowide/stackstring.hpp>
#include <nowide/stat.hpp>
#include <errno.h>

namespace nowide {
    namespace detail {
        int stat(const char* path, posix_stat_t* buffer, size_t buffer_size)
        {
            if(sizeof(*buffer) != buffer_size)
            {
                errno = EINVAL;
                return EINVAL;
            }
            const wstackstring wpath(path);
            return _wstat(wpath.get(), buffer);
        }
        int stat(const char* path, stat_t* buffer, size_t buffer_size)
        {
            if(sizeof(*buffer) != buffer_size)
            {
                errno = EINVAL;
                return EINVAL;
            }
            const wstackstring wpath(path);
            return _wstat64(wpath.get(), buffer);
        }
    } // namespace detail
} // namespace nowide

#endif
