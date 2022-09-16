//
//  gdxsv_CustomTexture.cpp
//  gdxsv
//
//  Created by Edward Li on 3/6/2021.
//  Copyright 2021 flycast. All rights reserved.
//
#if defined(__APPLE__) || defined(_WIN32)

#ifdef _WIN32
#define _AMD64_ // Fixing GitHub runner's winnt.h error
#endif

#include "gdxsv_CustomTexture.h"
#include "gdxsv_translation.h"

#ifdef __APPLE__
#include "oslib/directory.h"
#include <mach-o/dyld.h>
#include <regex>
#elif _WIN32
#include <winbase.h>
#include <winuser.h>
#include <winnls.h>
#include <stb_image.h>
#endif

GDXCustomTexture gdx_custom_texture;

bool GDXCustomTexture::Init()
{
    if (!initialized)
    {
        initialized = true;
        std::string game_id = GetGameId();
        if (GetGameId() == "T13306M")
        {
#ifdef __APPLE__
            uint32_t bufSize = PATH_MAX+1;
            char result [bufSize];
            if (_NSGetExecutablePath(result, &bufSize) == 0)
            {
                textures_path = std::string (result);
                textures_path.replace(textures_path.find("MacOS/Flycast"), sizeof("MacOS/Flycast") - 1, "Resources/Textures/");
                textures_path += GDXLanguage::LanguageString();
            }

            DIR *dir = flycast::opendir(textures_path.c_str());
            if (dir != nullptr)
            {
                INFO_LOG(RENDERER, "Found custom textures directory: %s", textures_path.c_str());
                custom_textures_available = true;
                closedir(dir);
                loader_thread.Start();
            }
#elif _WIN32
            custom_textures_available = true;
            loader_thread.Start();
#endif
        }
    }
    return custom_textures_available;
}

#ifdef _WIN32

static BOOL CALLBACK StaticEnumRCLangsFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, WORD wLang, LONG_PTR lParam)
{
    //Only add target language's hash & resource index into texture_map
    if (wLang == MAKELANGID(GDXLanguage::TextureLanguageID(), SUBLANG_NEUTRAL)){
        std::map<u32, std::string> * mapping = reinterpret_cast<std::map<u32, std::string>*>(lParam);
        HRSRC source = FindResourceExA(GetModuleHandle(NULL), MAKEINTRESOURCE(777),  lpName, wLang);
        if (source != NULL)
        {
            unsigned int size = SizeofResource(NULL, source);
            HGLOBAL memory = LoadResource(NULL, source);

            if (memory != NULL) {
                void* data = LockResource(memory);

                char* hex = new char[size+1];
                snprintf(hex, size+1, "%.*s", size, (char *)data);
                uint32_t hash = strtoul(hex, NULL, 16);

                char* name = new char[5];
                snprintf(name, 5, "%u", PtrToUint(lpName));

                (*mapping)[hash] = name;

                delete [] hex;
                delete [] name;
                FreeResource(memory);
            }
        }
    }
    return true;
}

static BOOL CALLBACK StaticEnumRCNamesFunc(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam)
{
    EnumResourceLanguages(hModule, lpType, lpName, (ENUMRESLANGPROC)&StaticEnumRCLangsFunc, lParam);
    return true;
}

void GDXCustomTexture::LoadMap()
{
    texture_map.clear();
    
    std::map<u32, std::string> * mapping = new std::map<u32, std::string>();
    
    //Load texture hash value (hardcoded type as 777) & PNG resources index into texture_map
    EnumResourceNames(GetModuleHandle(NULL), MAKEINTRESOURCE(777), (ENUMRESNAMEPROC)&StaticEnumRCNamesFunc, reinterpret_cast<LONG_PTR>(mapping));
    
    texture_map = *mapping;
    delete mapping;
    
    custom_textures_available = !texture_map.empty();
}

u8* GDXCustomTexture::LoadCustomTexture(u32 hash, int& width, int& height)
{
    auto it = texture_map.find(hash);
    if (it == texture_map.end())
        return nullptr;
    
    u8 *imgData = nullptr;
    
    //Load PNG data from resource
    HRSRC source = FindResourceA(GetModuleHandle(NULL), MAKEINTRESOURCE(std::stoi(it->second)), "PNG");
    if (source != NULL)
    {
        unsigned int size = SizeofResource(NULL, source);
        HGLOBAL memory = LoadResource(NULL, source);

        if (memory != NULL) {
            void* data = LockResource(memory);

            int n;
            stbi_set_flip_vertically_on_load(1);
            imgData = stbi_load_from_memory(static_cast<unsigned char*>(data), size, &width, &height, &n, STBI_rgb_alpha);

            FreeResource(memory);
        }
    }

    return imgData;
}

#endif
#endif // __APPLE__ || _WIN32
