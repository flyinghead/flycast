// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "LogManager.h"

class ConsoleListener : public LogListener
{
public:
  ConsoleListener();
  ~ConsoleListener() override;

  void Log(LogTypes::LOG_LEVELS, const char* text) override;

private:
  bool m_use_color;
};
