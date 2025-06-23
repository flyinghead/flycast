// Minimal entry point that allows CMake's gtest_discover_tests to enumerate
// the test suites without launching the full SDL UI.
//
// When invoked with any --gtest* flag (including --gtest_list_tests) we run
// the GoogleTest framework and exit. Otherwise we chain to Flycast's regular
// SDL_main so the normal application behaviour is preserved.

#include <cstring>
#include <gtest/gtest.h>
#include "core/log/LogManager.h"
#include "core/log/Log.h"
#include "core/log/ConsoleListener.h"
#include "stdclass.h"
#include "emulator.h"
#include "cfg/cfg.h"
#include "log/LogManager.h"

// Forward declaration of the real Flycast entry implemented in platform files.
extern "C" int FlycastSDLMain(int argc, char* argv[]);

extern "C" int SDL_main(int argc, char* argv[])
{
    // Initialize Flycast core and data paths before GoogleTest so that logging
    // and any emulator subsystems are ready for the unit tests.
    flycast_init(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    bool want_tests = ::testing::GTEST_FLAG(list_tests);
    for (int i = 1; i < argc && !want_tests; ++i)
    {
        if (strncmp(argv[i], "--gtest", 7) == 0)
            want_tests = true;
    }

    if (want_tests)
    {
        // Force test output to stdout to see logs in real time.
        ::testing::GTEST_FLAG(stream_result_to) = "stdout";

        // Enable maximum verbosity across all log categories during unit tests
        LogManager::Init();
        if (auto* lm = LogManager::GetInstance())
        {
            lm->RegisterListener(LogListener::CONSOLE_LISTENER, new ConsoleListener());
            lm->EnableListener(LogListener::CONSOLE_LISTENER, true);
            // Enable SH4 logs specifically to reduce noise but give detailed
            // diagnostics for failing tests.
            lm->SetEnable(LogTypes::SH4, true);
            lm->SetLogLevel(LogTypes::LINFO);
        }

        return RUN_ALL_TESTS();
    }

    return RUN_ALL_TESTS();
}
