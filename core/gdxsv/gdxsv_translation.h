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

struct GdxsvTranslation {
    u32 offset = 0;
    const char* original = nullptr;
    const char* cantonese = nullptr;
    const char* english = nullptr;
    const char* japanese = nullptr;
    const char* Text() const;
};

template<std::size_t MAXSIZE, std::size_t C_SIZE, std::size_t E_SIZE>
static GdxsvTranslation GdxsvTranslationWithMaxLength(u32 o, const char* orig, char const (&canto)[C_SIZE], char const (&eng)[E_SIZE]) {
    static_assert(sizeof(canto) <= MAXSIZE, "canto is too big.");
    static_assert(sizeof(eng) <= MAXSIZE, "eng is too big.");
    GdxsvTranslation v;
    v.offset = o;
    v.original = orig;
    v.cantonese = canto;
    v.english = eng;
    v.japanese = nullptr;
    return v;
}

template<std::size_t MAXSIZE, std::size_t C_SIZE, std::size_t E_SIZE, std::size_t J_SIZE>
static GdxsvTranslation GdxsvTranslationWithMaxLength(u32 o, const char* orig, char const (&canto)[C_SIZE], char const (&eng)[E_SIZE], char const (&jap)[J_SIZE]) {
    static_assert(sizeof(canto) <= MAXSIZE, "canto is too big.");
    static_assert(sizeof(eng) <= MAXSIZE, "eng is too big.");
    static_assert(sizeof(jap) <= MAXSIZE, "jap is too big.");
    GdxsvTranslation v;
    v.offset = o;
    v.original = orig;
    v.cantonese = canto;
    v.english = eng;
    v.japanese = jap;
    return v;
}

class GdxsvLanguage {
public:
    enum class Lang {
        NOT_SET = -1,
        Japanese,
        Cantonese,
        English,
        Disabled
    };

    static Lang Language();
    static std::string TextureDirectoryName();
private:
    static Lang LanguageFromOS();
};
