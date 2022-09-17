//
//  gdxsv_translation.cpp
//  gdxsv
//
//  Created by Edward Li on 15/7/2021.
//  Copyright © 2021 flycast. All rights reserved.
//
#ifdef _WIN32
#define _AMD64_ // Fixing GitHub runner's winnt.h error
#endif

#include "cfg/option.h"
#include "gdxsv_translation.h"

const char * GdxsvTranslation::Text() const {
    switch (GdxsvLanguage::Language()) {
    case GdxsvLanguage::Lang::English: return english;
    case GdxsvLanguage::Lang::Cantonese: return cantonese;
    case GdxsvLanguage::Lang::Japanese: return japanese ? japanese : original;
    }
    return original;
}

GdxsvLanguage::Lang GdxsvLanguage::Language() {
    Lang lang = static_cast<Lang>(config::GdxLanguage.get());
    switch (lang) {
    case Lang::Japanese:
    case Lang::Cantonese:
    case Lang::English:
    case Lang::Disabled:
        return lang;
    case Lang::NOT_SET:
    default:
        lang = LanguageFromOS();
        config::GdxLanguage.set((int)lang);
        return lang;
    }
}

std::string GdxsvLanguage::TextureDirectoryName() {
    switch (Language()) {
    case Lang::English: return "English";
    case Lang::Cantonese: return "Cantonese";
    default: return "Japanese";
    }
}
#ifdef _WIN32
#include <winnls.h>
#include <locale>
#include <codecvt>
USHORT GdxsvLanguage::TextureLanguageID() {
    switch (Language()) {
    case Lang::English: return LANG_ENGLISH;
    case Lang::Cantonese: return LANG_CHINESE;
    default: return LANG_JAPANESE;
    }
}
#endif

GdxsvLanguage::Lang GdxsvLanguage::LanguageFromOS() {
#ifdef __APPLE__
    extern std::string os_Locale();
    std::string locale = os_Locale();
    
    time_t ts = 0;
    struct tm t;
    char buf[16];
    localtime_r(&ts, &t);
    strftime(buf, sizeof(buf), "%z%Z", &t);
#elif _WIN32
    std::string locale;
    DWORD bufferLength = 0;
    ULONG numberOfLanguages = 0;
    std::wstring languagesWString;
    GetUserPreferredUILanguages(MUI_LANGUAGE_NAME,
                                &numberOfLanguages,
                                NULL,
                                &bufferLength);
    languagesWString.resize(bufferLength);
    BOOL result = GetUserPreferredUILanguages(MUI_LANGUAGE_NAME,
                                              &numberOfLanguages,
                                              const_cast<PZZWSTR>(languagesWString.data()),
                                              &bufferLength);
    if ( result ) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
        locale = convert.to_bytes(languagesWString);
    } else {
        locale = "en";
    }
#else
    std::string locale = "en";
#endif
    if (locale.find("ja") == 0
#ifdef __APPLE__
        || strcmp(buf, "+0900JST") == 0
#elif _WIN32
        || strcmp(_tzname[0], "Tokyo Standard Time") == 0
#endif
       )
        return Lang::Japanese;
    else if (locale.find("yue") == 0 || locale.find("zh") == 0 //Chinese fallback
#ifdef __APPLE__
             || strcmp(buf, "+0800HKT") == 0 //Cantonese users love using English OS
             || strcmp(buf, "+0800CST") == 0
#elif _WIN32
             || strcmp(_tzname[0], "China Standard Time") == 0
             || strcmp(_tzname[0], "Taipei Standard Time") == 0
#endif
            )
        return Lang::Cantonese;
    else
        return Lang::English;
}
