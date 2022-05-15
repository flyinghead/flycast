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

const char * Translation::Text() {
    switch (GDXLanguage::Language()) {
        case GDXLanguage::Lang::English:
            return english;
        case GDXLanguage::Lang::Cantonese:
            return cantonese;
        default:
            return japanese;
    }
}

GDXLanguage::Lang GDXLanguage::language_ = Lang::NOT_SET;

GDXLanguage::Lang GDXLanguage::Language() {
    if (language_ != Lang::NOT_SET)
        return language_;
    
    int lang = config::GdxLanguage;
    switch (lang) {
        case (int)Lang::NOT_SET:
            language_ = LanguageFromOS_();
            config::GdxLanguage = (int)language_;
            break;
            
        case (int)Lang::Cantonese:
            language_ = Lang::Cantonese;
            break;
            
        case (int)Lang::English:
            language_ = Lang::English;
            break;
            
        default:
            language_ = Lang::Japanese;
            break;
    }
    return language_;
}

std::string GDXLanguage::LanguageString() {
    switch (Language()) {
        case Lang::English:
            return "English";
        case Lang::Cantonese:
            return "Cantonese";
        default:
            return "Japanese";
    }
}
#ifdef _WIN32
#include <winnls.h>
#include <locale>
#include <codecvt>
USHORT GDXLanguage::TextureLanguageID() {
    switch (Language()) {
        case Lang::English:
            return LANG_ENGLISH;
        case Lang::Cantonese:
            return LANG_CHINESE;
        default:
            return LANG_JAPANESE;
    }
}
#endif

GDXLanguage::Lang GDXLanguage::LanguageFromOS_(){
#ifdef __APPLE__
    extern std::string os_Locale();
    std::string locale = os_Locale();
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
    if (locale.find("ja") == 0)
        return Lang::Japanese;
    else if (locale.find("yue") == 0 || locale.find("zh") == 0) //Chinese fallback
        return Lang::Cantonese;
    else
        return Lang::English;
}
