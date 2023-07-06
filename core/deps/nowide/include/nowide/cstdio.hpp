//
// Copyright (c) 2012 Artyom Beilis (Tonkikh)
// Copyright (c) 2020 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef NOWIDE_CSTDIO_HPP_INCLUDED
#define NOWIDE_CSTDIO_HPP_INCLUDED

#include <nowide/config.hpp>
#include <cstdio>

namespace nowide {
#if !defined(NOWIDE_WINDOWS) && !defined(NOWIDE_DOXYGEN)
    using std::fopen;
    using std::freopen;
    using std::remove;
    using std::rename;
#else

    ///
    /// \brief Same as freopen but file_name and mode are UTF-8 strings
    ///
    NOWIDE_DECL FILE* freopen(const char* file_name, const char* mode, FILE* stream);
    ///
    /// \brief Same as fopen but file_name and mode are UTF-8 strings
    ///
    NOWIDE_DECL FILE* fopen(const char* file_name, const char* mode);
    ///
    /// \brief Same as rename but old_name and new_name are UTF-8 strings
    ///
    NOWIDE_DECL int rename(const char* old_name, const char* new_name);
    ///
    /// \brief Same as rename but name is UTF-8 string
    ///
    NOWIDE_DECL int remove(const char* name);
#endif
    namespace detail {
        NOWIDE_DECL FILE* wfopen(const wchar_t* filename, const wchar_t* mode);
    }
} // namespace nowide

#endif
