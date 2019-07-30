// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#ifdef _WIN32

#include <windows.h>

#include "ConsoleListener.h"

ConsoleListener::ConsoleListener()
{
}

ConsoleListener::~ConsoleListener()
{
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
  ::OutputDebugStringA(text);
}

#endif // _WIN32
