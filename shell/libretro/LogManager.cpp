// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "LogManager.h"

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <string>

#include "log/Log.h"
#include "log/BitSet.h"
#include "log/StringUtil.h"

constexpr size_t MAX_MSGLEN = 1024;
// Singleton. Ugh.
static LogManager* s_log_manager;

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file, int line,
		const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
   if (s_log_manager)
      if (s_log_manager->IsEnabled(type, level))
         s_log_manager->LogWithFullPath(level, type, file, line, fmt, args);
	va_end(args);
}

static size_t DeterminePathCutOffPoint()
{
	constexpr const char* pattern = "shell/";
#ifdef _WIN32
	constexpr const char* pattern2 = "shell\\";
#endif
	std::string path = __FILE__;
	std::transform(path.begin(), path.end(), path.begin(),
			[](char c) { return std::tolower(c, std::locale::classic()); });
	size_t pos = path.find(pattern);
#ifdef _WIN32
	if (pos == std::string::npos)
		pos = path.find(pattern2);
#endif
	if (pos != std::string::npos)
		return pos;
	return 0;
}

LogManager::LogManager(void *log_cb)
{
	// create log containers
	m_log[LogTypes::AICA] = {"AICA", "AICA Audio Emulation"};
	m_log[LogTypes::AICA_ARM] = {"AICA_ARM", "AICA ARM Emulation"};
	m_log[LogTypes::AUDIO] = {"AUDIO", "Audio Ouput Interface"};
	m_log[LogTypes::BOOT] = {"BOOT", "Boot"};
	m_log[LogTypes::COMMON] = {"COMMON", "Common"};
	m_log[LogTypes::DYNAREC] = {"DYNAREC", "Dynamic Recompiler"};
	m_log[LogTypes::FLASHROM] = {"FLASHROM", "FlashROM / EEPROM"};
	m_log[LogTypes::GDROM] = {"GDROM", "GD-Rom Drive"};
	m_log[LogTypes::HOLLY] = {"HOLLY", "Holly Chipset"};
	m_log[LogTypes::INPUT] = {"INPUT", "Input Peripherals"};
	m_log[LogTypes::JVS] = {"JVS", "Naomi JVS Protocol"};
	m_log[LogTypes::MAPLE] = {"MAPLE", "Maple Bus and Peripherals"};
	m_log[LogTypes::INTERPRETER] = {"INTERPRETER", "SH4 Interpreter"};
	m_log[LogTypes::MEMORY] = {"MEMORY", "Memory Management"};
	m_log[LogTypes::VMEM] = {"VMEM", "Virtual Memory Management"};
	m_log[LogTypes::MODEM] = {"MODEM", "Modem and Network"};
	m_log[LogTypes::NAOMI] = {"NAOMI", "Naomi"};
	m_log[LogTypes::PVR] = {"PVR", "PowerVR GPU"};
	m_log[LogTypes::REIOS] = {"REIOS", "HLE BIOS"};
	m_log[LogTypes::RENDERER] = {"RENDERER", "OpenGL Renderer"};
	m_log[LogTypes::SAVESTATE] = {"SAVESTATE", "Save States"};
	m_log[LogTypes::SH4] = {"SH4", "SH4 Modules"};

   retro_printf  = (retro_log_printf_t)log_cb;
	// Set up log listeners
	int verbosity = (int)LogTypes::LDEBUG;

	// Ensure the verbosity level is valid
	if (verbosity < 1)
		verbosity = 1;
	if (verbosity > MAX_LOGLEVEL)
		verbosity = MAX_LOGLEVEL;

	SetLogLevel(static_cast<LogTypes::LOG_LEVELS>(verbosity));

	for (LogContainer& container : m_log)
	{
		container.m_enable = true;
	}

	m_path_cutoff_point = DeterminePathCutOffPoint();
}

LogManager::~LogManager()
{
}

void LogManager::LogWithFullPath(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type,
		const char* file_, int line, const char* format, va_list args)
{
   const char *file =  file_ + m_path_cutoff_point;
	char temp[MAX_MSGLEN];
	CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);
	std::string msg =
			StringFromFormat("%s:%u %c[%s]: %s\n", file,
					line, LogTypes::LOG_LEVEL_TO_CHAR[(int)level], GetShortName(type), temp);
   retro_log_level retro_level;
   const char *text = msg.c_str();
   switch (level)
   {
      case LogTypes::LNOTICE:
      case LogTypes::LINFO:
         retro_level = RETRO_LOG_INFO;
         break;
      case LogTypes::LERROR:
         retro_level = RETRO_LOG_ERROR;
         break;
      case LogTypes::LWARNING:
         retro_level = RETRO_LOG_WARN;
         break;
      case LogTypes::LDEBUG:
         retro_level = RETRO_LOG_DEBUG;
         break;

   }
   if (retro_printf != nullptr)
      retro_printf(retro_level, "%s", text);
}

void LogManager::SetLogLevel(LogTypes::LOG_LEVELS level)
{
	m_level = level;
}

void LogManager::SetEnable(LogTypes::LOG_TYPE type, bool enable)
{
	m_log[type].m_enable = enable;
}

bool LogManager::IsEnabled(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level) const
{
	return m_log[type].m_enable && m_level >= level;
}

const char* LogManager::GetShortName(LogTypes::LOG_TYPE type) const
{
	return m_log[type].m_short_name;
}

void LogManager::Init(void *log_cb)
{
	s_log_manager = new LogManager(log_cb);
}

void LogManager::Shutdown()
{
	delete s_log_manager;
	s_log_manager = nullptr;
}
