// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdarg>
#include <memory>

#include "BitSet.h"
#include "Log.h"

// pure virtual interface
class LogListener
{
public:
  virtual ~LogListener() = default;
  virtual void Log(LogTypes::LOG_LEVELS, const char* msg) = 0;

  enum LISTENER
  {
    FILE_LISTENER = 0,
    CONSOLE_LISTENER,
    LOG_WINDOW_LISTENER,
	IN_MEMORY_LISTENER,
	NETWORK_LISTENER,

    NUMBER_OF_LISTENERS  // Must be last
  };
};

class LogManager
{
public:
  static LogManager* GetInstance();
  static void Init();
  static void Shutdown();

  void Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file, int line,
           const char* format, va_list args);
  void LogWithFullPath(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file,
                       int line, const char* format, va_list args);

  LogTypes::LOG_LEVELS GetLogLevel() const;
  void SetLogLevel(LogTypes::LOG_LEVELS level);

  void SetEnable(LogTypes::LOG_TYPE type, bool enable);
  bool IsEnabled(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level = LogTypes::LNOTICE) const;

  const char* GetShortName(LogTypes::LOG_TYPE type) const;
  const char* GetFullName(LogTypes::LOG_TYPE type) const;

  void RegisterListener(LogListener::LISTENER id, LogListener* listener);
  void EnableListener(LogListener::LISTENER id, bool enable);
  bool IsListenerEnabled(LogListener::LISTENER id) const;
  void UpdateConfig();

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

  LogManager();

  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;
  LogManager(LogManager&&) = delete;
  LogManager& operator=(LogManager&&) = delete;

  LogTypes::LOG_LEVELS m_level;
  std::array<LogContainer, LogTypes::NUMBER_OF_LOGS> m_log{};
  std::array<std::unique_ptr<LogListener>, LogListener::NUMBER_OF_LISTENERS> m_listeners{};
  BitSet32 m_listener_ids;
  size_t m_path_cutoff_point = 0;
  std::string logServer;
};
