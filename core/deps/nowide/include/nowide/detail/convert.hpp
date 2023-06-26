//
// Copyright (c) 2020 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef NOWIDE_DETAIL_CONVERT_HPP_INCLUDED
#define NOWIDE_DETAIL_CONVERT_HPP_INCLUDED

#include <nowide/utf/convert.hpp>

// Legacy compatibility header only. Include <nowide/utf/convert.hpp> instead

namespace nowide {
    namespace detail {
        using nowide::utf::convert_buffer;
        using nowide::utf::convert_string;
        using nowide::utf::strlen;
    } // namespace detail
} // namespace nowide

#endif
