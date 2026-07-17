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
#include "util/tsqueue.h"
#include "types.h"
#include <string>
#include <memory>
#include <optional>
#include <vector>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <unordered_map>

class BaseTextureCacheData;
struct Renderer;
class WorkerThread;
namespace config
{
enum class CustomTexturePreloadMode : int;
}

class CustomTextureException final : public FlycastException
{
public:
	enum class Error
	{
		FileRead,
		ImageDecode,
		CompressedSource,
		TextureTooLarge,
		Upload,
		AllocationFailed,
		DirectX9CompressedSource,
	};

	CustomTextureException(Error error, const std::string& message)
		: FlycastException(message), error_(error) {}

	Error error() const { return error_; }

private:
	Error error_;
};

struct GpuPreloadedTexture
{
	using Ptr = std::shared_ptr<GpuPreloadedTexture>;

	explicit GpuPreloadedTexture(u8 mipLevels) : mipLevels(mipLevels) {}
	virtual ~GpuPreloadedTexture() = default;

	u8 mipLevels;
};

class BaseCustomTextureSource
{
public:
	using TextureCallback = std::function<void(u32 hash, PreparedCustomTexture::Ptr texture)>;
	using CancellationCheck = std::function<bool()>;

	virtual ~BaseCustomTextureSource() = default;
	virtual bool shouldReplace() const { return false; }
	virtual bool shouldPreload() const { return false; }
	virtual bool loadMap(const CustomTextureCapabilities& capabilities) = 0;
	virtual size_t getTextureCount() const { return 0; }
	virtual void terminate() { }
	virtual PreparedCustomTexture::Ptr loadCustomTexture(u32 hash,
			const CustomTextureCapabilities& capabilities, const CancellationCheck& cancelled) = 0;
	virtual bool isTextureReplaced(u32 hash) const = 0;
	virtual void preloadTextures(const CustomTextureCapabilities& capabilities,
			TextureCallback callback, std::atomic<bool>* stopFlag) {}
};

class CustomTexture
{
public:
	using GpuTextureUploader = std::function<bool(u32 hash, const PreparedCustomTexture& texture)>;

	~CustomTexture();
	void init();
	bool enabled() const;
	bool isPreloading() const;
	bool needsRefresh() const;
	void addSource(std::unique_ptr<BaseCustomTextureSource> source);
	void loadCustomTextureAsync(BaseTextureCacheData *textureData);
	bool isRequestComplete(CustomTextureRequestId requestId) const;
	PreparedCustomTexture::Ptr takePreparedTexture(CustomTextureRequestId requestId, bool& failed);
	void cancelRequest(CustomTextureRequestId requestId);
	void setCapabilities(const CustomTextureCapabilities& capabilities);
	void invalidateGpuPreloads();
	CustomTextureCapabilities getCapabilities() const;
	void dumpTexture(BaseTextureCacheData* texture, int w, int h, void *srcBuffer);
	void refresh();
	void terminate();
	void getPreloadProgress(int& completed, int& total, size_t& loadedSize) const;
	void processGpuPreloads(const GpuTextureUploader& uploader);
	void reportError(CustomTextureException::Error error);
	void showErrorNotification();

private:
	friend struct Renderer;
	struct GpuCleanupOperation {};
	enum class State
	{
		Stopped,
		Active,
		RefreshPending,
	};

	struct Request
	{
		PreparedCustomTexture::Ptr texture;
		bool complete = false;
		bool failed = false;
	};

	PreparedCustomTexture::Ptr loadTexture(u32 currentHash, u32 oldVqHash,
			u32 oldHash, const CustomTextureCapabilities& capabilities,
			const BaseCustomTextureSource::CancellationCheck& cancelled);
	PreparedCustomTexture::Ptr findPreloaded(u32 currentHash, u32 oldVqHash, u32 oldHash) const;
	bool isTextureReplaced(BaseTextureCacheData* texture) const;
	std::string getGameId() const;
	void startSession(const std::string& gameId);
	void clearSessionResources();
	void prepareSource(BaseCustomTextureSource* source, bool shouldPreload);
	void submitGpuPreload(u32 hash, PreparedCustomTexture::Ptr texture);
	bool consumeGpuCleanupOperations();
	void resetPreloadProgress();
	bool requestCancelled(CustomTextureRequestId requestId) const;

	State state = State::Stopped;
	std::string sessionGameId;
	bool sessionCustomTextures = false;
	config::CustomTexturePreloadMode sessionPreloadMode {};
	std::vector<std::unique_ptr<BaseCustomTextureSource>> sources;
	std::unique_ptr<WorkerThread> loaderThread;
	mutable std::mutex stateMutex;
	std::unordered_map<u32, PreparedCustomTexture::Ptr> preloadedTextures;
	std::deque<std::pair<u32, PreparedCustomTexture::Ptr>> pendingGpuPreloads;
	std::condition_variable gpuPreloadCondition;
	std::unordered_map<u64, Request> requests;
	CustomTextureCapabilities capabilities = CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Unknown);
	std::atomic<u64> nextRequestId { 1 };
	std::atomic<int> preloadTotal { 0 };
	std::atomic<int> preloadLoaded { 0 };
	std::atomic<size_t> preloadLoadedSize { 0 };
	std::atomic<int> pendingPreloads { 0 };
	std::atomic<bool> stopPreload { false };
	TsQueue<GpuCleanupOperation> gpuCleanupOperations;
	std::optional<CustomTextureException::Error> pendingError;
	bool errorNotificationShown = false;
};

extern CustomTexture custom_texture;
