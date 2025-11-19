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
#include <vector>
#include <atomic>
#include <functional>

class BaseTextureCacheData;
class WorkerThread;

struct TextureData {
	std::vector<u8> data;
	int w = 0;
	int h = 0;
};

class BaseCustomTextureSource
{
public:
	using TextureCallback = std::function<void(u32 hash, TextureData&& data)>;

	virtual ~BaseCustomTextureSource() { }
	virtual bool customTexturesAvailable() { return true; }
	virtual bool shouldReplace() const { return false; }
	virtual bool shouldPreload() const { return false; }
	virtual bool LoadMap() = 0;
	virtual size_t GetTextureCount() const { return 0; }
	virtual void Terminate() { }
	virtual u8* LoadCustomTexture(u32 hash, int& width, int& height) = 0;
	virtual bool isTextureReplaced(u32 hash) = 0;
	virtual void PreloadTextures(TextureCallback callback, std::atomic<bool>* stop_flag) { }
};

class CustomTexture
{
public:
	~CustomTexture();
	bool Init();
	bool enabled();
	bool preloaded();
	bool isPreloading();
	void AddSource(std::unique_ptr<BaseCustomTextureSource> source);
	void LoadCustomTextureAsync(BaseTextureCacheData *texture_data);
	void DumpTexture(BaseTextureCacheData* texture, int w, int h, void *src_buffer);
	void Terminate();
	void GetPreloadProgress(int& completed, int& total, size_t& loaded_size) const;

private:
	u8* loadTexture(u32 hash, int& width, int& height);
	bool isTextureReplaced(BaseTextureCacheData* texture);
	void loadTexture(BaseTextureCacheData *texture);
	std::string getGameId();
	void prepareSource(BaseCustomTextureSource* source);
	void resetPreloadProgress();
	
	bool initialized = false;
	std::vector<std::unique_ptr<BaseCustomTextureSource>> sources;
	std::unique_ptr<WorkerThread> loaderThread;
	std::map<u32, TextureData> preloaded_textures;
	std::atomic<int> preload_total { 0 };
	std::atomic<int> preload_loaded { 0 };
	std::atomic<size_t> preload_loaded_size { 0 };
	std::atomic<int> pending_preloads { 0 };
	std::atomic<bool> stop_preload { false };
};

extern CustomTexture custom_texture;
