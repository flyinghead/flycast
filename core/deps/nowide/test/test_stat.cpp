//
// Copyright (c) 2020 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <nowide/stat.hpp>

#include <nowide/cstdio.hpp>
#include "test.hpp"
#ifdef NOWIDE_WINDOWS
#include <errno.h>
#endif

// coverity[root_function]
void test_main(int, char** argv, char**)
{
    const std::string prefix = argv[0];
    const std::string filename = prefix + "\xd7\xa9-\xd0\xbc-\xce\xbd.txt";

    // Make sure file does not exist
    nowide::remove(filename.c_str());

    std::cout << " -- stat - non-existing file" << std::endl;
    {
#ifdef NOWIDE_WINDOWS
        struct _stat stdStat;
#else
        struct stat stdStat;
#endif
        TEST(nowide::stat(filename.c_str(), &stdStat) != 0);
        nowide::stat_t boostStat;
        TEST(nowide::stat(filename.c_str(), &boostStat) != 0);
    }

    std::cout << " -- stat - existing file" << std::endl;
    FILE* f = nowide::fopen(filename.c_str(), "wb");
    TEST(f);
    const char testData[] = "Hello World";
    constexpr size_t testDataSize = sizeof(testData);
    TEST_EQ(std::fwrite(testData, sizeof(char), testDataSize, f), testDataSize);
    std::fclose(f);
    {
#ifdef NOWIDE_WINDOWS
        struct _stat stdStat;
#else
        struct stat stdStat;
#endif /*  */
        TEST_EQ(nowide::stat(filename.c_str(), &stdStat), 0);
        TEST_EQ(static_cast<size_t>(stdStat.st_size), testDataSize);
        nowide::stat_t boostStat;
        TEST_EQ(nowide::stat(filename.c_str(), &boostStat), 0);
        TEST_EQ(static_cast<size_t>(boostStat.st_size), testDataSize);
    }

#ifdef NOWIDE_WINDOWS
    std::cout << " -- stat - invalid struct size" << std::endl;
    {
        struct _stat stdStat;
        // Simulate passing a struct that is 4 bytes smaller, e.g. if it uses 32 bit time field instead of 64 bit
        // Need to use the detail function directly
        TEST_EQ(nowide::detail::stat(filename.c_str(), &stdStat, sizeof(stdStat) - 4u), EINVAL);
        TEST_EQ(errno, EINVAL);
        // Same for our stat_t
        nowide::stat_t boostStat;
        TEST_EQ(nowide::detail::stat(filename.c_str(), &boostStat, sizeof(boostStat) - 4u), EINVAL);
        TEST_EQ(errno, EINVAL);
    }
#endif

    nowide::remove(filename.c_str());
}
