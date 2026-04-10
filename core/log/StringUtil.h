#pragma once
#ifndef _WIN32
#include <locale>
#include <locale.h>
#endif

#if !defined(_WIN32) && !defined(__ANDROID__) && !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__SWITCH__) && !defined(__vita__)
#define DO_SWITCH_LOCALE
#endif

#ifdef DO_SWITCH_LOCALE
inline static locale_t GetCLocale()
{
  static locale_t c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
  return c_locale;
}
#endif

class UseCLocale
{
#ifdef DO_SWITCH_LOCALE
public:
	UseCLocale() {
		previousLocale = uselocale(GetCLocale());
	}

	~UseCLocale() {
		uselocale(previousLocale);
	}

private:
	locale_t previousLocale;
#endif
};

#undef DO_SWITCH_LOCALE
