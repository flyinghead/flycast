//
//  gdxsv_CustomTexture.cpp
//  gdxsv
//
//  Created by Edward Li on 3/6/2021.
//  Copyright 2021 flycast. All rights reserved.
//
#if defined(__APPLE__) || defined(_WIN32)

#ifdef _WIN32
#define _AMD64_	 // Fixing GitHub runner's winnt.h error
#endif

#include "gdxsv_CustomTexture.h"

#include "gdxsv_translation.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>

#include <regex>

#include "oslib/directory.h"
#elif _WIN32
#include <stb_image.h>
#include <winbase.h>
#include <winnls.h>
#include <winuser.h>
#endif

GdxsvCustomTexture gdx_custom_texture;

bool GdxsvCustomTexture::Init() {
	if (!initialized) {
		initialized = true;
		std::string game_id = GetGameId();
		if (GetGameId() == "T13306M") {
#ifdef __APPLE__
			uint32_t bufSize = PATH_MAX + 1;
			char result[bufSize];
			if (_NSGetExecutablePath(result, &bufSize) == 0) {
				textures_path = std::string(result);
				textures_path.replace(textures_path.find("MacOS/Flycast"), sizeof("MacOS/Flycast") - 1, "Resources/Textures/");
				textures_path += GdxsvLanguage::TextureDirectoryName();
			}

			DIR* dir = flycast::opendir(textures_path.c_str());
			if (dir != nullptr) {
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

static BOOL CALLBACK StaticEnumRCNamesFunc(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam) {
	// Only add target language's hash & resource index into texture_map
	auto mapping = reinterpret_cast<std::map<u32, std::string>*>(lParam);
	const auto name = std::string(lpName);
	auto lang_dir = GdxsvLanguage::TextureDirectoryName();
	std::transform(lang_dir.begin(), lang_dir.end(), lang_dir.begin(), ::toupper);

	if (name.find(lang_dir) == 0 || name.find("COMMON") == 0) {
		auto tex_hash = name.substr(name.find_last_of('_') + 1);
		u32 hash = strtoul(tex_hash.c_str(), NULL, 16);
		mapping->emplace(hash, name);
	}

	return true;
}

void GdxsvCustomTexture::LoadMap() {
	texture_map.clear();
	std::map<u32, std::string> mapping;

	auto ret = EnumResourceNames(GetModuleHandle(NULL), "GDXSV_TEXTURE", (ENUMRESNAMEPROC)&StaticEnumRCNamesFunc,
								 reinterpret_cast<LONG_PTR>(&mapping));
	if (!ret) {
		ERROR_LOG(COMMON, "EnumResourceNames error:%d", GetLastError());
	}

	texture_map = mapping;
	custom_textures_available = !texture_map.empty();
}

u8* GdxsvCustomTexture::LoadCustomTexture(u32 hash, int& width, int& height) {
	auto it = texture_map.find(hash);
	if (it == texture_map.end()) return nullptr;

	u8* imgData = nullptr;

	// Load PNG data from resource
	HRSRC source = FindResourceA(GetModuleHandle(NULL), it->second.c_str(), "GDXSV_TEXTURE");
	if (source != NULL) {
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
#endif	// __APPLE__ || _WIN32
