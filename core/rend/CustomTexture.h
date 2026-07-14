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
#include "custom_texture/CustomTextureTypes.h"
#include <string>
#include <memory>
#include <vector>
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class BaseTextureCacheData;
class WorkerThread;

class BaseCustomTextureSource
{
public:
	using TextureCallback = std::function<void(u32 hash, PreparedCustomTexturePtr texture)>;
	using CancellationCheck = std::function<bool()>;

	virtual ~BaseCustomTextureSource() = default;
	virtual bool shouldReplace() const { return false; }
	virtual bool shouldPreload() const { return false; }
	virtual bool loadMap(const CustomTextureCapabilities& capabilities) = 0;
	virtual size_t getTextureCount() const { return 0; }
	virtual void terminate() { }
	virtual PreparedCustomTexturePtr loadCustomTexture(u32 hash,
			const CustomTextureCapabilities& capabilities, const CancellationCheck& cancelled) = 0;
	virtual bool isTextureReplaced(u32 hash) const = 0;
	virtual void preloadTextures(const CustomTextureCapabilities& capabilities,
			TextureCallback callback, std::atomic<bool>* stopFlag) {}
};

class CustomTexture
{
public:
	~CustomTexture();
	bool init();
	bool enabled() const;
	bool preloaded() const;
	bool isPreloading() const;
	void addSource(std::unique_ptr<BaseCustomTextureSource> source);
	void loadCustomTextureAsync(BaseTextureCacheData *textureData);
	bool isRequestComplete(CustomTextureRequestId requestId) const;
	PreparedCustomTexturePtr takePreparedTexture(CustomTextureRequestId requestId, bool& failed);
	void cancelRequest(CustomTextureRequestId requestId);
	void setCapabilities(const CustomTextureCapabilities& capabilities);
	CustomTextureCapabilities getCapabilities() const;
	void dumpTexture(BaseTextureCacheData* texture, int w, int h, void *srcBuffer);
	void terminate();
	void getPreloadProgress(int& completed, int& total, size_t& loadedSize) const;

private:
	struct Completion
	{
		PreparedCustomTexturePtr texture;
		bool failed = false;
	};

	PreparedCustomTexturePtr loadTexture(u32 currentHash, u32 oldVqHash,
			u32 oldHash, const CustomTextureCapabilities& capabilities,
			const BaseCustomTextureSource::CancellationCheck& cancelled);
	PreparedCustomTexturePtr findPreloaded(u32 currentHash, u32 oldVqHash, u32 oldHash) const;
	bool isTextureReplaced(BaseTextureCacheData* texture) const;
	std::string getGameId() const;
	void prepareSource(BaseCustomTextureSource* source);
	void resetPreloadProgress();
	bool requestCancelled(CustomTextureRequestId requestId) const;

	bool initialized = false;
	std::vector<std::unique_ptr<BaseCustomTextureSource>> sources;
	std::unique_ptr<WorkerThread> loaderThread;
	mutable std::mutex stateMutex;
	std::unordered_map<u32, PreparedCustomTexturePtr> preloadedTextures;
	std::unordered_map<u64, Completion> completions;
	std::unordered_set<u64> activeRequests;
	CustomTextureCapabilities capabilities = CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Unknown);
	std::atomic<u64> nextRequestId { 1 };
	std::atomic<int> preloadTotal { 0 };
	std::atomic<int> preloadLoaded { 0 };
	std::atomic<size_t> preloadLoadedSize { 0 };
	std::atomic<int> pendingPreloads { 0 };
	std::atomic<bool> stopPreload { false };
};

extern CustomTexture custom_texture;
