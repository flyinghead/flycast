// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#if !defined(_WIN32) && !defined(_ANDROID)
#include <cstdio>
#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "ConsoleListener.h"
#include "Log.h"

ConsoleListener::ConsoleListener()
{
  m_use_color = !!isatty(fileno(stderr));
}

ConsoleListener::~ConsoleListener()
{
  fflush(nullptr);
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
  char color_attr[16] = "";
  char reset_attr[16] = "";

  if (m_use_color)
  {
    strcpy(reset_attr, "\x1b[0m");
    switch (level)
    {
    case LogTypes::LOG_LEVELS::LNOTICE:
      // light green
      strcpy(color_attr, "\x1b[92m");
      break;
    case LogTypes::LOG_LEVELS::LERROR:
      // light red
      strcpy(color_attr, "\x1b[91m");
      break;
    case LogTypes::LOG_LEVELS::LWARNING:
      // light yellow
      strcpy(color_attr, "\x1b[93m");
      break;
    default:
      break;
    }
  }
#if HOST_OS != OS_DARWIN
  fprintf(stderr, "%s%s%s", color_attr, text, reset_attr);
#else
  // Skip the time
  const char *trimmed_text = strchr(text, ' ');
  if (trimmed_text != NULL)
	  trimmed_text++;
	else
	  trimmed_text = text;
  int text_size = (int)strlen(trimmed_text);
  // trim the ending newline
  if (trimmed_text[text_size - 1] == '\n')
	  text_size--;
  darw_printf("%s%.*s%s", color_attr, text_size, trimmed_text, reset_attr);
#endif
}
#endif // !_WIN32 && !_ANDROID
