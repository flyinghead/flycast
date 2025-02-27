/*
	 Copyright 2018 flyinghead
 
	 This file is part of reicast.
 
	 reicast is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 2 of the License, or
	 (at your option) any later version.
 
	 reicast is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.
 
	 You should have received a copy of the GNU General Public License
	 along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "texconv.h"
#include <string>
#include <map>
#include <memory>

class BaseTextureCacheData;
class WorkerThread;

class CustomTexture
{
public:
	~CustomTexture();
	void LoadCustomTextureAsync(BaseTextureCacheData *texture_data);
	void DumpTexture(u32 hash, int w, int h, TextureType textype, void *src_buffer);
	void Terminate();

private:
	bool init();
	u8* loadTexture(u32 hash, int& width, int& height);
	void loadTexture(BaseTextureCacheData *texture);
	std::string getGameId();
	void loadMap();
	
	bool initialized = false;
	bool custom_textures_available = false;
	std::string textures_path;
	std::map<u32, std::string> texture_map;
	std::unique_ptr<WorkerThread> loaderThread;
};

extern CustomTexture custom_texture;
