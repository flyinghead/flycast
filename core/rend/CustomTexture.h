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
	virtual bool shouldReplace() const { return false; }
	virtual bool shouldPreload() const { return false; }
	virtual bool loadMap() = 0;
	virtual size_t getTextureCount() const { return 0; }
	virtual void terminate() { }
	virtual u8* loadCustomTexture(u32 hash, int& width, int& height) = 0;
	virtual bool isTextureReplaced(u32 hash) = 0;
	virtual void preloadTextures(TextureCallback callback, std::atomic<bool>* stopFlag) { }
};

class CustomTexture
{
public:
	~CustomTexture();
	bool init();
	bool enabled();
	bool preloaded();
	bool isPreloading();
	void addSource(std::unique_ptr<BaseCustomTextureSource> source);
	void loadCustomTextureAsync(BaseTextureCacheData *textureData);
	void dumpTexture(BaseTextureCacheData* texture, int w, int h, void *srcBuffer);
	void terminate();
	void getPreloadProgress(int& completed, int& total, size_t& loadedSize) const;

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
	std::map<u32, TextureData> preloadedTextures;
	std::atomic<int> preloadTotal { 0 };
	std::atomic<int> preloadLoaded { 0 };
	std::atomic<size_t> preloadLoadedSize { 0 };
	std::atomic<int> pendingPreloads { 0 };
	std::atomic<bool> stopPreload { false };
};

extern CustomTexture custom_texture;
