//
// Copyright (c) 2012 Artyom Beilis (Tonkikh)
// Copyright (c) 2019 - 2022 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef NOWIDE_CONFIG_HPP_INCLUDED
#define NOWIDE_CONFIG_HPP_INCLUDED

/// @file

#include <nowide/replacement.hpp>

//! @cond Doxygen_Suppress
#if(defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__CYGWIN__)
#define NOWIDE_WINDOWS
#endif

#ifdef _MSC_VER
#define NOWIDE_MSVC _MSC_VER
#endif

#ifdef __GNUC__
#define NOWIDE_SYMBOL_VISIBLE __attribute__((__visibility__("default")))
#endif

#ifndef NOWIDE_SYMBOL_VISIBLE
#define NOWIDE_SYMBOL_VISIBLE
#endif

#ifdef NOWIDE_WINDOWS
#define NOWIDE_SYMBOL_EXPORT __declspec(dllexport)
#define NOWIDE_SYMBOL_IMPORT __declspec(dllimport)
#elif defined(__CYGWIN__) && defined(__GNUC__) && (__GNUC__ >= 4)
#define NOWIDE_SYMBOL_EXPORT __attribute__((__dllexport__))
#define NOWIDE_SYMBOL_IMPORT __attribute__((__dllimport__))
#else
#define NOWIDE_SYMBOL_EXPORT NOWIDE_SYMBOL_VISIBLE
#define NOWIDE_SYMBOL_IMPORT
#endif

#if defined __GNUC__
#define NOWIDE_LIKELY(x) __builtin_expect(x, 1)
#define NOWIDE_UNLIKELY(x) __builtin_expect(x, 0)
#else
#if !defined(NOWIDE_LIKELY)
#define NOWIDE_LIKELY(x) x
#endif
#if !defined(NOWIDE_UNLIKELY)
#define NOWIDE_UNLIKELY(x) x
#endif
#endif

#if defined(NOWIDE_DYN_LINK)
#ifdef NOWIDE_SOURCE
#define NOWIDE_DECL NOWIDE_SYMBOL_EXPORT
#else
#define NOWIDE_DECL NOWIDE_SYMBOL_IMPORT
#endif // NOWIDE_SOURCE
#else
#define NOWIDE_DECL
#endif // NOWIDE_DYN_LINK


//! @endcond

/// @def NOWIDE_USE_WCHAR_OVERLOADS
/// @brief Whether to use the wchar_t* overloads in fstream-classes.
///
/// Enabled by default on Windows and Cygwin as the latter may use wchar_t in filesystem::path.
#ifndef NOWIDE_USE_WCHAR_OVERLOADS
#if defined(NOWIDE_WINDOWS) || defined(__CYGWIN__) || defined(NOWIDE_DOXYGEN)
#define NOWIDE_USE_WCHAR_OVERLOADS 1
#else
#define NOWIDE_USE_WCHAR_OVERLOADS 0
#endif
#endif

/// @def NOWIDE_USE_FILEBUF_REPLACEMENT
/// @brief Define to 1 to use the class from <filebuf.hpp> that is used on Windows.
///
/// - On Windows: No effect, always overwritten to 1
/// - Others (including Cygwin): Defaults to the value of #NOWIDE_USE_WCHAR_OVERLOADS if not set.
///
/// When set to 0 nowide::basic_filebuf will be an alias for std::basic_filebuf.
///
/// Affects nowide::basic_filebuf,
/// nowide::basic_ofstream, nowide::basic_ifstream, nowide::basic_fstream
#if defined(NOWIDE_WINDOWS) || defined(NOWIDE_DOXYGEN)
#ifdef NOWIDE_USE_FILEBUF_REPLACEMENT
#undef NOWIDE_USE_FILEBUF_REPLACEMENT
#endif
#define NOWIDE_USE_FILEBUF_REPLACEMENT 1
#elif !defined(NOWIDE_USE_FILEBUF_REPLACEMENT)
#define NOWIDE_USE_FILEBUF_REPLACEMENT NOWIDE_USE_WCHAR_OVERLOADS
#endif

//! @cond Doxygen_Suppress

#if defined(__GNUC__) && __GNUC__ >= 7
#define NOWIDE_FALLTHROUGH __attribute__((fallthrough))
#else
#define NOWIDE_FALLTHROUGH
#endif

// The std::codecvt<char16/32_t, char, std::mbstate_t> are deprecated in C++20
// These macros can suppress this warning
#if defined(_MSC_VER)
#define NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4996))
#define NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_END __pragma(warning(pop))
#elif(__cplusplus >= 202002L) && defined(__clang__)
#define NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_BEGIN \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_END _Pragma("clang diagnostic pop")
#elif(__cplusplus >= 202002L) && defined(__GNUC__)
#define NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_BEGIN \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_END _Pragma("GCC diagnostic pop")
#else
#define NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_BEGIN
#define NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_END
#endif

//! @endcond

///
/// \brief This namespace includes implementations of the standard library functions and
/// classes such that they accept UTF-8 strings on Windows.
/// On other platforms (i.e. not on Windows) those functions and classes are just aliases
/// of the corresponding ones from the std namespace or behave like them.
///
namespace nowide {}

#endif
