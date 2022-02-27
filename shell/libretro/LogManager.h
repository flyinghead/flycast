// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdarg>

#include <libretro.h>

#include "log/Log.h"

class LogManager
{
public:
  static void Init(void *log_cb);
  static void Shutdown();

  void LogWithFullPath(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file,
        int line, const char* fmt, va_list args);

  void SetLogLevel(LogTypes::LOG_LEVELS level);

  void SetEnable(LogTypes::LOG_TYPE type, bool enable);
  bool IsEnabled(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level = LogTypes::LNOTICE) const;

  const char* GetShortName(LogTypes::LOG_TYPE type) const;

private:
  struct LogContainer
  {
	  LogContainer() : m_short_name(NULL), m_full_name(NULL) {}
	  LogContainer(const char* shortName, const char* fullName, bool enable = false)
	  	  : m_short_name(shortName), m_full_name(fullName), m_enable(enable)
	  {}
	  const char* m_short_name;
	  const char* m_full_name;
	  bool m_enable = false;
  };

  LogManager(void *log_cb);
  ~LogManager();

  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;
  LogManager(LogManager&&) = delete;
  LogManager& operator=(LogManager&&) = delete;

  LogTypes::LOG_LEVELS m_level;
  std::array<LogContainer, LogTypes::NUMBER_OF_LOGS> m_log{};
  size_t m_path_cutoff_point = 0;
  retro_log_printf_t retro_printf = nullptr;
};
