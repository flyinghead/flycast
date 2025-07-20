#pragma once

#if defined(_WIN32)

#include <thread>
#include <chrono>

// To ensure usleep() in tests works for Windows
inline void usleep(unsigned int usec)
{
    std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

#endif
