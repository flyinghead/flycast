//
// Copyright (c) 2015 Artyom Beilis (Tonkikh)
// Copyright (c) 2021 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(__GNUC__) && __GNUC__ >= 7
#pragma GCC diagnostic ignored "-Wattributes"
#endif
#include <nowide/filesystem.hpp>

#include <nowide/convert.hpp>
#include <nowide/cstdio.hpp>
#include <nowide/fstream.hpp>
#include <nowide/quoted.hpp>
#include <nowide/utf/convert.hpp>
#include "test.hpp"
#include <iomanip>
#include <sstream>
#include <type_traits>
#if defined(_MSC_VER)
#pragma warning(disable : 4714) // function marked as __forceinline not inlined
#endif
#include <nowide/filesystem.hpp>

// Exclude apple as support there is target level specific -.-
#if defined(__cpp_lib_filesystem) && !defined(__APPLE__)
#include <filesystem>
#define NOWIDE_TEST_STD_PATH
#endif
#if defined(__cpp_lib_experimental_filesystem)
#ifndef _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif
#include <experimental/filesystem>
#define NOWIDE_TEST_STD_EXPERIMENTAL_PATH
#endif

template<typename T, typename = void>
struct is_istreamable : std::false_type
{};
using nowide::detail::void_t;
template<typename T>
struct is_istreamable<T, void_t<decltype(std::declval<std::istream&>() >> std::declval<T>())>> : std::true_type
{};

template<typename T_Char>
std::string maybe_narrow(const std::basic_string<T_Char>& s)
{
    return nowide::narrow(s);
}

const std::string& maybe_narrow(const std::string& s)
{
    return s;
}

template<class Path>
void test_fs_path_io(std::string utf8_name)
{
#if defined(__cpp_lib_quoted_string_io) && __cpp_lib_quoted_string_io >= 201304
    Path path(nowide::utf::convert_string<typename Path::value_type>(utf8_name));
    // Get native and UTF-8/narrow name here as the Path ctor may change the string (e.g. slash substitution)
    const auto nativeName = path.native();
    utf8_name = maybe_narrow(nativeName);
    // Output
    std::ostringstream s, sRef;
    sRef << std::quoted(utf8_name);
    s << nowide::quoted(path);
    TEST_EQ(s.str(), sRef.str());
    // const
    const Path constPath(path);
    s.str("");
    s << nowide::quoted(constPath);
    TEST_EQ(s.str(), sRef.str());
    // Rvalue
    s.str("");
    s << nowide::quoted(Path(path));
    TEST_EQ(s.str(), sRef.str());

    // Input
    std::istringstream sIn(sRef.str());
    Path pathOut;
    static_assert(is_istreamable<decltype(nowide::quoted(pathOut))>::value, "!");
    sIn >> nowide::quoted(pathOut);
    TEST_EQ(pathOut.native(), nativeName);
    // Can't read into a const path
    static_assert(!is_istreamable<decltype(nowide::quoted(constPath))>::value, "!");
    // or an Rvalue
    static_assert(!is_istreamable<decltype(nowide::quoted(Path(path)))>::value, "!");

    // Wide stream
    std::wostringstream ws, wsRef;
    wsRef << std::quoted(nowide::widen(utf8_name));
    ws << nowide::quoted(path);
    TEST_EQ(ws.str(), wsRef.str());
    std::wistringstream wsIn(wsRef.str());
    pathOut.clear();
    wsIn >> nowide::quoted(pathOut);
    TEST_EQ(maybe_narrow(pathOut.native()), utf8_name);
#else
    (void)utf8_name; // Suppress unused warning
    std::cout << "Skipping tests for nowide::quoted" << std::endl;
#endif
}

// coverity[root_function]
void test_main(int, char** argv, char**)
{
    nowide::nowide_filesystem();
    const std::string prefix = argv[0];
    const std::string utf8_name =
      prefix + "\xf0\x9d\x92\x9e-\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82.txt";

    {
        nowide::ofstream f(utf8_name.c_str());
        TEST(f);
        f << "Test" << std::endl;
    }

    TEST(nowide::filesystem::is_regular_file(nowide::widen(utf8_name)));
    TEST(nowide::filesystem::is_regular_file(utf8_name));

    TEST(nowide::remove(utf8_name.c_str()) == 0);

    TEST(!nowide::filesystem::is_regular_file(nowide::widen(utf8_name)));
    TEST(!nowide::filesystem::is_regular_file(utf8_name));

    const nowide::filesystem::path path = utf8_name;
    {
        nowide::ofstream f(path);
        TEST(f);
        f << "Test" << std::endl;
        TEST(is_regular_file(path));
    }
    {
        nowide::ifstream f(path);
        TEST(f);
        std::string test;
        f >> test;
        TEST(test == "Test");
    }
    {
        nowide::fstream f(path);
        TEST(f);
        std::string test;
        f >> test;
        TEST(test == "Test");
    }
    nowide::filesystem::remove(path);

    std::cout << "Testing nowide::filesystem::path" << std::endl;
    test_fs_path_io<nowide::filesystem::path>(utf8_name);
#ifdef NOWIDE_TEST_STD_EXPERIMENTAL_PATH
    std::cout << "Testing std::experimental::filesystem::path" << std::endl;
    test_fs_path_io<std::experimental::filesystem::path>(utf8_name);
#endif
#ifdef NOWIDE_TEST_STD_PATH
    std::cout << "Testing std::filesystem::path" << std::endl;
    test_fs_path_io<std::filesystem::path>(utf8_name);
#endif
}
