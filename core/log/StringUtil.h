#include <locale>

#ifdef _WIN32
#include <Windows.h>
constexpr u32 CODEPAGE_SHIFT_JIS = 932;
constexpr u32 CODEPAGE_WINDOWS_1252 = 1252;
#else
#include <codecvt>
#include <errno.h>
#include <iconv.h>
#include <locale.h>
#endif

#if !defined(_WIN32) && !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__)
static locale_t GetCLocale()
{
  static locale_t c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
  return c_locale;
}
#endif

bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args)
{
  int writtenCount;

#ifdef _WIN32
  // You would think *printf are simple, right? Iterate on each character,
  // if it's a format specifier handle it properly, etc.
  //
  // Nooooo. Not according to the C standard.
  //
  // According to the C99 standard (7.19.6.1 "The fprintf function")
  //     The format shall be a multibyte character sequence
  //
  // Because some character encodings might have '%' signs in the middle of
  // a multibyte sequence (SJIS for example only specifies that the first
  // byte of a 2 byte sequence is "high", the second byte can be anything),
  // printf functions have to decode the multibyte sequences and try their
  // best to not screw up.
  //
  // Unfortunately, on Windows, the locale for most languages is not UTF-8
  // as we would need. Notably, for zh_TW, Windows chooses EUC-CN as the
  // locale, and completely fails when trying to decode UTF-8 as EUC-CN.
  //
  // On the other hand, the fix is simple: because we use UTF-8, no such
  // multibyte handling is required as we can simply assume that no '%' char
  // will be present in the middle of a multibyte sequence.
  //
  // This is why we look up the default C locale here and use _vsnprintf_l.
  static _locale_t c_locale = nullptr;
  if (!c_locale)
    c_locale = _create_locale(LC_ALL, "C");
  writtenCount = _vsnprintf_l(out, outsize, format, c_locale, args);
#else
#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__)
  locale_t previousLocale = uselocale(GetCLocale());
#endif
  writtenCount = vsnprintf(out, outsize, format, args);
#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__)
  uselocale(previousLocale);
#endif
#endif

  if (writtenCount > 0 && writtenCount < outsize)
  {
    out[writtenCount] = '\0';
    return true;
  }
  else
  {
    out[outsize - 1] = '\0';
    return false;
  }
}

std::string StringFromFormatV(const char* format, va_list args)
{
  char* buf = nullptr;
#ifdef _WIN32
  int required = _vscprintf(format, args);
  buf = new char[required + 1];
  CharArrayFromFormatV(buf, required + 1, format, args);

  std::string temp = buf;
  delete[] buf;
#else
#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__)
  locale_t previousLocale = uselocale(GetCLocale());
#endif
  if (vasprintf(&buf, format, args) < 0)
  {
    ERROR_LOG(COMMON, "Unable to allocate memory for string");
    buf = nullptr;
  }

#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__)
  uselocale(previousLocale);
#endif

  std::string temp = buf;
  free(buf);
#endif
  return temp;
}

std::string StringFromFormat(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  std::string res = StringFromFormatV(format, args);
  va_end(args);
  return res;
}


#ifdef _WIN32
std::wstring CPToUTF16(u32 code_page, const std::string& input)
{
  auto const size =
      MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);

  std::wstring output;
  output.resize(size);

  if (size == 0 ||
      size != MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()),
                                  &output[0], static_cast<int>(output.size())))
  {
    output.clear();
  }

  return output;
}

std::string UTF16ToCP(u32 code_page, const std::wstring& input)
{
  int usedDefaultChar;
  auto const size = WideCharToMultiByte(code_page, 0, input.data(), static_cast<int>(input.size()),
                                        nullptr, 0, nullptr, &usedDefaultChar);

  std::string output;
  output.resize(size);

  if (size == 0 ||
      size != WideCharToMultiByte(code_page, 0, input.data(), static_cast<int>(input.size()),
                                  &output[0], static_cast<int>(output.size()), nullptr, &usedDefaultChar))
  {
    const DWORD error_code = GetLastError();
    ERROR_LOG(COMMON, "WideCharToMultiByte Error in String '%s': %lu", input.c_str(), error_code);
    output.clear();
  }
  return output;
}

std::string UTF16ToUTF8(const std::wstring& input)
{
  return UTF16ToCP(CP_UTF8, input);
}

std::wstring UTF8ToUTF16(const std::string& input)
{
  return CPToUTF16(CP_UTF8, input);
}

#ifdef _UNICODE
inline std::string TStrToUTF8(const std::wstring& str)
{
  return UTF16ToUTF8(str);
}

inline std::wstring UTF8ToTStr(const std::string& str)
{
  return UTF8ToUTF16(str);
}
#else
inline std::string TStrToUTF8(const std::string& str)
{
  return str;
}

inline std::string UTF8ToTStr(const std::string& str)
{
  return str;
}
#endif

#endif
