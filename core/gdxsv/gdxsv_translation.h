//
//  gdxsv_translation.h
//  gdxsv
//
//  Created by Edward Li on 3/6/2021.
//  Copyright 2021 flycast. All rights reserved.
//
#pragma once
#include "types.h"
#ifdef _WIN32
#include <windef.h>
#endif

class Translation
{
public:
    virtual ~Translation() {}
    u32 offset;
    const char * original;
    const char * cantonese;
    const char * english;
    const char * japanese;
    
    const char * Text();
};

template<std::size_t MAXSIZE>
class TranslationWithMaxLength : public Translation
{
public:
    template<std::size_t C_SIZE, std::size_t E_SIZE>
    explicit TranslationWithMaxLength(u32 o, const char * orig, char const (&canto) [C_SIZE], char const (&eng) [E_SIZE])
    {
        static_assert(sizeof(canto) <= MAXSIZE, "input is too big.");
        static_assert(sizeof(eng) <= MAXSIZE, "input is too big.");
        offset = o;
        original = orig;
        cantonese = canto;
        english = eng;
        japanese = NULL;
    }
    
    template<std::size_t C_SIZE, std::size_t E_SIZE, std::size_t J_SIZE>
    explicit TranslationWithMaxLength(u32 o, const char * orig, char const (&canto) [C_SIZE], char const (&eng) [E_SIZE], char const (&jap) [J_SIZE])
    {
        static_assert(sizeof(canto) <= MAXSIZE, "input is too big.");
        static_assert(sizeof(eng) <= MAXSIZE, "input is too big.");
        static_assert(sizeof(jap) <= MAXSIZE, "input is too big.");
        offset = o;
        original = orig;
        cantonese = canto;
        english = eng;
        japanese = jap;
    }
};

class GDXLanguage{
public:
    enum class Lang {
        NOT_SET = -1,
        Japanese,
        Cantonese,
        English,
        Disabled
    };
    
    static Lang Language();
    static std::string LanguageString();
#ifdef _WIN32
    static USHORT TextureLanguageID();
#endif
private:
    static Lang language_;
    static Lang LanguageFromOS_();
};
