//
// Copyright (c) 2012 Artyom Beilis (Tonkikh)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <nowide/cstdlib.hpp>
#include "test.hpp"
#include <cstring>

#if defined(NOWIDE_TEST_INCLUDE_WINDOWS) && defined(NOWIDE_WINDOWS)
#include <windows.h>
#endif

// coverity[root_function]
void test_main(int, char**, char**)
{
    std::string example = "\xd7\xa9-\xd0\xbc-\xce\xbd";
    std::string envVar = "NOWIDE_TEST2=" + example + "x";

    TEST(nowide::setenv("NOWIDE_TEST1", example.c_str(), 1) == 0);
    TEST(nowide::getenv("NOWIDE_TEST1"));
    TEST(nowide::getenv("NOWIDE_TEST1") == example);
    TEST(nowide::setenv("NOWIDE_TEST1", "xx", 0) == 0);
    TEST(nowide::getenv("NOWIDE_TEST1") == example);
    char* penv = const_cast<char*>(envVar.c_str());
    TEST(nowide::putenv(penv) == 0);
    TEST(nowide::getenv("NOWIDE_TEST2"));
    TEST(nowide::getenv("NOWIDE_TEST_INVALID") == 0);
    TEST(nowide::getenv("NOWIDE_TEST2") == example + "x");
#ifdef NOWIDE_WINDOWS
    // Passing a variable without an equals sign (before \0) is an error
    // But GLIBC has an extension that unsets the env var instead
    std::string envVar2 = "NOWIDE_TEST1SOMEGARBAGE=";
    // End the string before the equals sign -> Expect fail
    envVar2[strlen("NOWIDE_TEST1")] = '\0';
    char* penv2 = const_cast<char*>(envVar2.c_str());
    TEST(nowide::putenv(penv2) == -1);
    TEST(nowide::getenv("NOWIDE_TEST1"));
    TEST(nowide::getenv("NOWIDE_TEST1") == example);
#endif
}
