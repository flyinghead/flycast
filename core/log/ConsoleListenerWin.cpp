// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#if defined(_WIN32) && !defined(LOG_TO_PTY)

#include <windows.h>

#include "ConsoleListener.h"

ConsoleListener::ConsoleListener() = default;

ConsoleListener::~ConsoleListener() = default;

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
  ::OutputDebugStringA(text);
}

#endif // _WIN32
