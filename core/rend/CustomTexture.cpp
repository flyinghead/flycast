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
#include "CustomTexture.h"
#include "TexCache.h"
#include "custom_texture/TextureTranscoder.h"
#include "hw/pvr/Renderer_if.h"
#include "oslib/directory.h"
#include "oslib/storage.h"
#include "cfg/option.h"
#include "oslib/i18n.h"
#include "oslib/oslib.h"
#include "stdclass.h"
#include "util/worker_thread.h"

#include <sstream>
#include <locale>
#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <map>
#include <new>
#include <unordered_set>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stbi.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace
{
constexpr size_t MaxPendingGpuPreloads = 8;

std::vector<u32> replacementHashes(u32 currentHash, u32 oldVqHash, u32 oldHash)
{
	std::vector<u32> hashes;
	hashes.reserve(3);
	hashes.push_back(currentHash);
	const auto appendDistinct = [&hashes](u32 hash) {
		if (std::find(hashes.begin(), hashes.end(), hash) == hashes.end())
			hashes.push_back(hash);
	};
	if (oldVqHash != 0)
		appendDistinct(oldVqHash);
	appendDistinct(oldHash);
	return hashes;
}

bool readFile(const CustomTextureCandidate& candidate, std::vector<u8>& bytes, std::string& error)
{
	std::unique_ptr<hostfs::File> file(hostfs::storage().openFile(candidate.path, "rb"));
	if (!file)
	{
		error = "file open failed";
		return false;
	}
	const s64 actualSize = file->size();
	if (actualSize <= 0 || static_cast<u64>(actualSize) > static_cast<u64>(SIZE_MAX))
	{
		error = "file is empty or cannot be addressed on this platform";
		return false;
	}
	try
	{
		bytes.resize(static_cast<size_t>(actualSize));
	}
	catch (const std::bad_alloc&)
	{
		error = "file buffer allocation failed";
		return false;
	}
	if (file->read(bytes.data(), 1, bytes.size()) != bytes.size())
	{
		error = "short file read";
		bytes.clear();
		return false;
	}
	return true;
}

std::array<CustomTextureSourceKind, 7> preferredSourceKinds(
		const CustomTextureCapabilities& capabilities)
{
	std::array<CustomTextureSourceKind, 7> kinds {{
		CustomTextureSourceKind::Ktx2Xubc7,
		CustomTextureSourceKind::Ktx2Xuastc,
		CustomTextureSourceKind::Ktx2Etc1s,
		CustomTextureSourceKind::Ktx2Generic,
		CustomTextureSourceKind::DdsBc7,
		CustomTextureSourceKind::Png,
		CustomTextureSourceKind::Jpeg,
	}};
	bool xuastcFirst = false;
	if (capabilities.supports(NativeTextureFormat::Astc4x4Unorm))
	{
#if defined(__APPLE__) || defined(__ANDROID__)
		xuastcFirst = true;
#else
		xuastcFirst = !capabilities.supports(NativeTextureFormat::Bc7Unorm);
#endif
	}
	if (xuastcFirst)
		std::swap(kinds[0], kinds[1]);
	return kinds;
}

PreparedCustomTexturePtr decodeImageToRGBA(const CustomTextureCandidate& candidate,
		u32 hash, const std::vector<u8>& fileBytes,
		const CustomTextureCapabilities& capabilities, std::string& error)
{
	if (fileBytes.size() > INT_MAX)
	{
		error = "legacy image is too large for stb_image";
		custom_texture.reportError(CustomTexture::Error::ImageDecode);
		return nullptr;
	}
	int width = 0;
	int height = 0;
	int channels = 0;
	if (!stbi_info_from_memory(fileBytes.data(), static_cast<int>(fileBytes.size()),
			&width, &height, &channels) || width <= 0 || height <= 0)
	{
		error = "legacy image header is invalid";
		custom_texture.reportError(CustomTexture::Error::ImageDecode);
		return nullptr;
	}
	if (static_cast<u32>(width) > capabilities.max2DWidth
			|| static_cast<u32>(height) > capabilities.max2DHeight)
	{
		error = "legacy image dimensions exceed active renderer limits";
		custom_texture.reportError(CustomTexture::Error::TextureTooLarge);
		return nullptr;
	}
	PreparedMipLevel level;
	if (!computeMipLayout(NativeTextureFormat::Rgba8Unorm,
			static_cast<u32>(width), static_cast<u32>(height), 0, level, error))
	{
		custom_texture.reportError(CustomTexture::Error::ImageDecode);
		return nullptr;
	}
	stbi_set_flip_vertically_on_load_thread(1);
	stbi_uc *decoded = stbi_load_from_memory(fileBytes.data(), static_cast<int>(fileBytes.size()),
			&width, &height, &channels, STBI_rgb_alpha);
	if (decoded == nullptr)
	{
		error = stbi_failure_reason() ? stbi_failure_reason() : "stb_image decode failed";
		custom_texture.reportError(CustomTexture::Error::ImageDecode);
		return nullptr;
	}
	std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decodedOwner(decoded, stbi_image_free);
	std::shared_ptr<PreparedCustomTexture> output;
	try
	{
		output = std::make_shared<PreparedCustomTexture>();
	}
	catch (const std::bad_alloc&)
	{
		error = "legacy image descriptor allocation failed";
		custom_texture.reportError(CustomTexture::Error::ImageDecode);
		return nullptr;
	}
	output->replacementHash = hash;
	output->sourceCodec = CustomTextureCodec::LegacyRgba;
	output->nativeFormat = NativeTextureFormat::Rgba8Unorm;
	output->pathClass = TranscodePathClass::DecodeToRgba;
	output->width = static_cast<u32>(width);
	output->height = static_cast<u32>(height);
	output->sourceSrgb = true;
	output->hasAlpha = true;
	output->generateMipmaps = true;
	try
	{
		output->levels.push_back(level);
		output->bytes.assign(decoded, decoded + level.byteSize);
	}
	catch (const std::bad_alloc&)
	{
		error = "legacy image payload allocation failed";
		custom_texture.reportError(CustomTexture::Error::ImageDecode);
		return nullptr;
	}
	return output;
}
}

CustomTexture custom_texture;

class CustomTextureSource : public BaseCustomTextureSource
{
public:
	CustomTextureSource(const std::string gameId)
	{
		texturesPath = hostfs::getTextureLoadPath(gameId);
		if (!texturesPath.empty())
		{
			customTexturesAvailable = hostfs::storage().exists(texturesPath);
			if (customTexturesAvailable)
				NOTICE_LOG(RENDERER, "Found custom textures directory: %s", texturesPath.c_str());
		}
	}
	bool shouldReplace() const override { return config::CustomTextures && customTexturesAvailable; }
	bool shouldPreload() const override
	{
		const config::CustomTexturePreloadMode mode = config::customTexturePreloadMode();
		return shouldReplace() && (mode == config::CustomTexturePreloadMode::SystemMemory
				|| (mode == config::CustomTexturePreloadMode::VideoMemory
						&& rend_supports_gpu_texture_preload()));
	}
	bool loadMap(const CustomTextureCapabilities& capabilities) override;
	size_t getTextureCount() const override;
	void preloadTextures(const CustomTextureCapabilities& capabilities,
			TextureCallback callback, std::atomic<bool>* stopFlag) override;
	PreparedCustomTexturePtr loadCustomTexture(u32 hash,
			const CustomTextureCapabilities& capabilities,
			const CancellationCheck& cancelled) override;
	bool isTextureReplaced(u32 hash) const override final;

private:
	bool isFailed(const std::string& path) const;
	void markFailed(const std::string& path);

	bool customTexturesAvailable = false;
	std::string texturesPath;
	mutable std::mutex catalogMutex;
	std::map<u32, CustomTextureCandidate> textureCatalog;
	std::unordered_set<std::string> sessionFailures;
};

bool CustomTextureSource::loadMap(const CustomTextureCapabilities& capabilities)
{
	std::map<u32, CustomTextureCandidate> newCatalog;
	const auto preferredKinds = preferredSourceKinds(capabilities);
	hostfs::DirectoryTree tree(texturesPath);
	for (const hostfs::FileInfo& item : tree)
	{
		if (item.isDirectory)
			continue;
		const auto parsed = parseCustomTextureFilename(item.name);
		if (!parsed)
			continue;
		CustomTextureCandidate candidate { parsed->kind, item.path };
		auto [catalogEntry, inserted] = newCatalog.emplace(parsed->hash, candidate);
		if (inserted)
			continue;

		const auto candidatePriority = std::find(
				preferredKinds.begin(), preferredKinds.end(), candidate.kind);
		const auto selectedPriority = std::find(
				preferredKinds.begin(), preferredKinds.end(), catalogEntry->second.kind);
		if (candidatePriority < selectedPriority || (candidatePriority == selectedPriority
				&& candidate.path < catalogEntry->second.path))
		{
			WARN_LOG(RENDERER, "Ignoring custom texture candidate for %08x: %s (using %s)",
					parsed->hash, catalogEntry->second.path.c_str(), candidate.path.c_str());
			catalogEntry->second = std::move(candidate);
		}
		else
		{
			WARN_LOG(RENDERER, "Ignoring custom texture candidate for %08x: %s (using %s)",
					parsed->hash, candidate.path.c_str(), catalogEntry->second.path.c_str());
		}
	}
	{
		std::lock_guard<std::mutex> lock(catalogMutex);
		textureCatalog = std::move(newCatalog);
		sessionFailures.clear();
	}
	std::lock_guard<std::mutex> lock(catalogMutex);
	return !textureCatalog.empty();
}

size_t CustomTextureSource::getTextureCount() const
{
	std::lock_guard<std::mutex> lock(catalogMutex);
	return textureCatalog.size();
}

void CustomTextureSource::preloadTextures(const CustomTextureCapabilities& capabilities,
		TextureCallback callback, std::atomic<bool>* stopFlag)
{
	std::vector<u32> hashes;
	{
		std::lock_guard<std::mutex> lock(catalogMutex);
		hashes.reserve(textureCatalog.size());
		for (const auto& [hash, unused] : textureCatalog)
			hashes.push_back(hash);
	}
	for (u32 hash : hashes)
	{
		if (stopFlag != nullptr && *stopFlag)
			return;
		auto cancelled = [stopFlag]() { return stopFlag != nullptr && *stopFlag; };
		PreparedCustomTexturePtr texture = loadCustomTexture(hash, capabilities, cancelled);
		callback(hash, std::move(texture));
	}
}

PreparedCustomTexturePtr CustomTextureSource::loadCustomTexture(u32 hash,
		const CustomTextureCapabilities& capabilities,
		const CancellationCheck& cancelled)
{
	CustomTextureCandidate candidate;
	{
		std::lock_guard<std::mutex> lock(catalogMutex);
		const auto found = textureCatalog.find(hash);
		if (found == textureCatalog.end())
			return nullptr;
		candidate = found->second;
	}
	if (isFailed(candidate.path))
		return nullptr;
	if (cancelled && cancelled())
		return nullptr;
	const bool legacyImage = candidate.kind == CustomTextureSourceKind::Png
			|| candidate.kind == CustomTextureSourceKind::Jpeg;
	if (!legacyImage && capabilities.backend == CustomTextureBackend::Direct3D9)
	{
		WARN_LOG(RENDERER, "DDS/KTX2 custom texture %08x is not supported by DirectX 9: %s",
				hash, candidate.path.c_str());
		custom_texture.reportError(CustomTexture::Error::DirectX9CompressedSource);
		markFailed(candidate.path);
		return nullptr;
	}

	std::vector<u8> fileBytes;
	std::string readError;
	if (!readFile(candidate, fileBytes, readError))
	{
		WARN_LOG(RENDERER, "Custom texture %08x read failed (%s): %s", hash,
			candidate.path.c_str(), readError.c_str());
		custom_texture.reportError(CustomTexture::Error::FileRead);
		markFailed(candidate.path);
		return nullptr;
	}

	if (legacyImage)
	{
		std::string imageError;
		PreparedCustomTexturePtr texture = decodeImageToRGBA(candidate, hash, fileBytes,
				capabilities, imageError);
		if (texture)
			return texture;
		WARN_LOG(RENDERER, "Custom texture %08x image preparation failed (%s): %s", hash,
			candidate.path.c_str(), imageError.c_str());
		markFailed(candidate.path);
		return nullptr;
	}

	TextureTranscoder transcoder;
	TextureInspection inspection;
	TextureTranscodeStatus status = transcoder.inspect(candidate.kind, fileBytes, inspection);
	if (!status)
	{
		WARN_LOG(RENDERER, "Custom texture %08x inspection failed (%s): %s", hash,
			candidate.path.c_str(), status.message.c_str());
		custom_texture.reportError(CustomTexture::Error::CompressedSource);
		markFailed(candidate.path);
		return nullptr;
	}
	if (inspection.width > capabilities.max2DWidth
			|| inspection.height > capabilities.max2DHeight)
	{
		WARN_LOG(RENDERER, "Custom texture %08x dimensions %ux%u exceed renderer limit %ux%u: %s",
				hash, inspection.width, inspection.height, capabilities.max2DWidth,
				capabilities.max2DHeight, candidate.path.c_str());
		custom_texture.reportError(CustomTexture::Error::TextureTooLarge);
		markFailed(candidate.path);
		return nullptr;
	}
	auto targets = selectNativeTextureTargets(capabilities, inspection.codec,
			inspection.blockWidth, inspection.blockHeight, inspection.hasAlpha);
	for (NativeTextureFormat target : targets)
	{
		if (!capabilities.canUpload(target, inspection.width, inspection.height, inspection.levels))
			continue;
		PreparedCustomTexturePtr texture;
		status = transcoder.prepare(inspection, fileBytes, target, hash, cancelled, texture);
		if (status)
		{
			DEBUG_LOG(RENDERER, "Prepared custom texture %08x %s -> %s (%zu bytes, %zu mips)",
				hash, customTextureCodecName(inspection.codec), nativeTextureFormatName(target),
				texture->bytes.size(), texture->levels.size());
			return texture;
		}
		if (status.category == TextureTranscodeError::Cancelled)
			return nullptr;
	}
	WARN_LOG(RENDERER, "Custom texture %08x preparation failed (%s, %s): %s", hash,
			customTextureCodecName(inspection.codec), candidate.path.c_str(), status.message.c_str());
	custom_texture.reportError(CustomTexture::Error::CompressedSource);
	markFailed(candidate.path);
	return nullptr;
}

bool CustomTextureSource::isTextureReplaced(u32 hash) const
{
	std::lock_guard<std::mutex> lock(catalogMutex);
	return textureCatalog.count(hash);
}

bool CustomTextureSource::isFailed(const std::string& path) const
{
	std::lock_guard<std::mutex> lock(catalogMutex);
	return sessionFailures.count(path) != 0;
}

void CustomTextureSource::markFailed(const std::string& path)
{
	std::lock_guard<std::mutex> lock(catalogMutex);
	sessionFailures.insert(path);
}

std::string CustomTexture::getGameId() const
{
   std::string gameId(settings.content.gameId);
   const size_t strEnd = gameId.find_last_not_of(' ');
   if (strEnd == std::string::npos)
	  return "";
   gameId = gameId.substr(0, strEnd + 1);
   std::replace(gameId.begin(), gameId.end(), ' ', '_');

   return gameId;
}

bool CustomTexture::init()
{
	if (!initialized)
	{
		stopPreload = false;
		resetPreloadProgress();
		pendingPreloads = 0;
		initialized = true;
		std::string gameId = getGameId();
		if (gameId.length() > 0)
		{
			// The first source added has highest priority.
			// Add your source after the default `CustomTextureSource`(data/textures/<game id> folder), so end-users can override your textures.
			addSource(std::make_unique<CustomTextureSource>(gameId));
		}
	}

	return loaderThread != nullptr;
}

bool CustomTexture::enabled() const {
	return loaderThread != nullptr;
}

bool CustomTexture::preloaded() const {
	return preloadTotal > 0;
}

bool CustomTexture::isPreloading() const {
	if (pendingPreloads > 0)
		return true;

	int texLoaded = 0;
	int texTotal = 0;
	size_t loadedSizeB = 0;
	getPreloadProgress(texLoaded, texTotal, loadedSizeB);
	
	return (texTotal > 0 && texLoaded < texTotal);
}

void CustomTexture::addSource(std::unique_ptr<BaseCustomTextureSource> source)
{
	BaseCustomTextureSource* ptr = source.get();
	sources.emplace_back(std::move(source));
	
	if (initialized)
	{
		if (!loaderThread && ptr->shouldReplace())
		{
			loaderThread = std::make_unique<WorkerThread>("CustomTexLoader");
		}
		if (loaderThread)
		{
			if (ptr->shouldPreload())
				pendingPreloads++;
			loaderThread->run([this, ptr]() {
				prepareSource(ptr);
			});
		}
	}
}

CustomTexture::~CustomTexture() {
	terminate();
}

void CustomTexture::terminate()
{
	stopPreload = true;
	rend_request_gpu_preloaded_texture_cleanup();
	gpuPreloadCondition.notify_all();
	if (loaderThread)
		loaderThread->stop();
	loaderThread.reset();
	for (auto& source : sources)
		source->terminate();
	sources.clear();
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		preloadedTextures.clear();
		pendingGpuPreloads.clear();
		completions.clear();
		activeRequests.clear();
		pendingError = Error::None;
		errorNotificationShown = false;
	}
	resetPreloadProgress();
	initialized = false;
}

PreparedCustomTexturePtr CustomTexture::findPreloaded(u32 currentHash, u32 oldVqHash,
		u32 oldHash) const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	for (u32 hash : replacementHashes(currentHash, oldVqHash, oldHash))
	{
		const auto found = preloadedTextures.find(hash);
		if (found != preloadedTextures.end())
			return found->second;
	}
	return nullptr;
}

PreparedCustomTexturePtr CustomTexture::loadTexture(u32 currentHash, u32 oldVqHash,
		u32 oldHash, const CustomTextureCapabilities& activeCapabilities,
		const BaseCustomTextureSource::CancellationCheck& cancelled)
{
	for (u32 hash : replacementHashes(currentHash, oldVqHash, oldHash))
	{
		for (const auto& source : sources)
		{
			if (!source->shouldReplace())
				continue;
			if (PreparedCustomTexturePtr texture = source->loadCustomTexture(hash,
					activeCapabilities, cancelled))
				return texture;
		}
	}
	return nullptr;
}

bool CustomTexture::requestCancelled(CustomTextureRequestId requestId) const
{
	if (stopPreload)
		return true;
	std::lock_guard<std::mutex> lock(stateMutex);
	return activeRequests.count(requestId.value) == 0;
}

void CustomTexture::loadCustomTextureAsync(BaseTextureCacheData *textureData)
{
	if (!init())
		return;
	showErrorNotification();
	if (textureData->customRequestId)
		cancelRequest(textureData->customRequestId);
	textureData->customPayload.reset();
	const CustomTextureCapabilities activeCapabilities = getCapabilities();
	if (PreparedCustomTexturePtr preloaded = findPreloaded(textureData->texture_hash,
			textureData->old_vqtexture_hash, textureData->old_texture_hash))
	{
		textureData->customPayload = std::move(preloaded);
		textureData->customRequestId = {};
		return;
	}

	const CustomTextureRequestId requestId { nextRequestId.fetch_add(1) };
	const u32 currentHash = textureData->texture_hash;
	const u32 oldVqHash = textureData->old_vqtexture_hash;
	const u32 oldHash = textureData->old_texture_hash;
	textureData->customRequestId = requestId;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		activeRequests.insert(requestId.value);
	}
	loaderThread->run([this, requestId, currentHash, oldVqHash, oldHash,
			activeCapabilities]() {
		auto cancelled = [this, requestId]() {
			return requestCancelled(requestId);
		};
		PreparedCustomTexturePtr texture = loadTexture(currentHash, oldVqHash, oldHash,
				activeCapabilities, cancelled);
		std::lock_guard<std::mutex> lock(stateMutex);
		if (activeRequests.erase(requestId.value) != 0)
		{
			const bool failed = texture == nullptr;
			completions[requestId.value] = { std::move(texture), failed };
		}
	});
}

bool CustomTexture::isRequestComplete(CustomTextureRequestId requestId) const
{
	if (!requestId)
		return false;
	std::lock_guard<std::mutex> lock(stateMutex);
	return completions.count(requestId.value) != 0;
}

PreparedCustomTexturePtr CustomTexture::takePreparedTexture(CustomTextureRequestId requestId, bool& failed)
{
	showErrorNotification();
	failed = false;
	if (!requestId)
		return nullptr;
	std::lock_guard<std::mutex> lock(stateMutex);
	const auto found = completions.find(requestId.value);
	if (found == completions.end())
		return nullptr;
	failed = found->second.failed;
	PreparedCustomTexturePtr texture = failed ? nullptr : std::move(found->second.texture);
	completions.erase(found);
	return texture;
}

void CustomTexture::cancelRequest(CustomTextureRequestId requestId)
{
	if (!requestId)
		return;
	std::lock_guard<std::mutex> lock(stateMutex);
	activeRequests.erase(requestId.value);
	completions.erase(requestId.value);
}

void CustomTexture::setCapabilities(const CustomTextureCapabilities& newCapabilities)
{
	CustomTextureCapabilities normalized = newCapabilities;
	if (!normalized.supports(NativeTextureFormat::Rgba8Unorm))
		normalized.setSupported(NativeTextureFormat::Rgba8Unorm);
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		if (capabilities.backend == normalized.backend
				&& capabilities.max2DWidth == normalized.max2DWidth
				&& capabilities.max2DHeight == normalized.max2DHeight
				&& capabilities.sampledFormats == normalized.sampledFormats)
			return;
	}

	const bool restart = initialized;
	if (restart)
		terminate();
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		capabilities = normalized;
	}
	if (restart)
		init();
}

CustomTextureCapabilities CustomTexture::getCapabilities() const
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return capabilities;
}

bool CustomTexture::isTextureReplaced(BaseTextureCacheData* texture) const
{
	if (findPreloaded(texture->texture_hash, texture->old_vqtexture_hash,
			texture->old_texture_hash))
		return true;
	for (const auto& source : sources)
	{
		if (!source->shouldReplace())
			continue;
		for (u32 hash : replacementHashes(texture->texture_hash,
				texture->old_vqtexture_hash, texture->old_texture_hash))
		{
			if (source->isTextureReplaced(hash))
				return true;
		}
	}
	return false;
}

void CustomTexture::dumpTexture(BaseTextureCacheData* texture, int w, int h, void *srcBuffer)
{
	if (!config::DumpTextures)
		return;

	if (config::DumpUniqueTextures && (texture->Updates > 1 || texture->tcw.PixelFmt == PixelYUV))
		return;

	if (!config::DumpReplacedTextures.get() && isTextureReplaced(texture))
		return;

	std::string baseDumpDir = hostfs::getTextureDumpPath();
	if (!file_exists(baseDumpDir))
		make_directory(baseDumpDir);
	std::string gameId = getGameId();
	if (gameId.length() == 0)
		return;

	baseDumpDir += gameId + "/";
	if (!file_exists(baseDumpDir))
		make_directory(baseDumpDir);

	std::ostringstream path;
	path.imbue(std::locale::classic());
	path << baseDumpDir << std::hex << texture->texture_hash << ".png";

	u16 *src = (u16 *)srcBuffer;
	u8 *dstBuffer = (u8 *)malloc(w * h * 4);	// 32-bit per pixel
	if (dstBuffer == nullptr)
	{
		ERROR_LOG(RENDERER, "Dump texture: out of memory");
		return;
	}
	u8 *dst = dstBuffer;

	for (int y = 0; y < h; y++)
	{
		if (!isDirectX(config::RendererType))
		{
			switch (texture->tex_type)
			{
			case TextureType::_4444:
				for (int x = 0; x < w; x++)
				{
					*dst++ = (((*src >> 12) & 0xF) << 4) | ((*src >> 12) & 0xF);
					*dst++ = (((*src >> 8) & 0xF) << 4) | ((*src >> 8) & 0xF);
					*dst++ = (((*src >> 4) & 0xF) << 4) | ((*src >> 4) & 0xF);
					*dst++ = ((*src & 0xF) << 4) | (*src & 0xF);
					src++;
				}
				break;
			case TextureType::_565:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker565_32<RGBAPacker>::unpack(*src);
					dst += 4;
					src++;
				}
				break;
			case TextureType::_5551:
				for (int x = 0; x < w; x++)
				{
					*dst++ = (((*src >> 11) & 0x1F) << 3) | ((*src >> 13) & 7);
					*dst++ = (((*src >> 6) & 0x1F) << 3) | ((*src >> 8) & 7);
					*dst++ = (((*src >> 1) & 0x1F) << 3) | ((*src >> 3) & 7);
					*dst++ = (*src & 1) ? 255 : 0;
					src++;
				}
				break;
			case TextureType::_8888:
				memcpy(dst, src, w * 4);
				dst += w * 4;
				src += w * 2;
				break;
			default:
				WARN_LOG(RENDERER, "dumpTexture: unsupported picture format %x", (u32)texture->tex_type);
				free(dstBuffer);
				return;
			}
		}
		else
		{
			switch (texture->tex_type)
			{
			case TextureType::_4444:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker4444_32<RGBAPacker>::unpack(*src);
					dst += 4;
					src++;
				}
				break;
			case TextureType::_565:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker565_32<RGBAPacker>::unpack(*src);
					dst += 4;
					src++;
				}
				break;
			case TextureType::_5551:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker1555_32<RGBAPacker>::unpack(*src);
					dst += 4;
					src++;
				}
				break;
			case TextureType::_8888:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker8888<RGBAPacker>::unpack(*(u32 *)src);
					dst += 4;
					src += 2;
				}
				break;
			default:
				WARN_LOG(RENDERER, "dumpTexture: unsupported picture format %x", (u32)texture->tex_type);
				free(dstBuffer);
				return;
			}
		}
	}

	stbi_flip_vertically_on_write(1);
	const auto& savefunc = [](void *context, void *data, int size) {
		FILE *f = nowide::fopen((const char *)context, "wb");
		if (f == nullptr)
		{
			WARN_LOG(RENDERER, "Dump texture: can't save to file %s: error %d", (const char *)context, errno);
		}
		else
		{
			fwrite(data, 1, size, f);
			fclose(f);
		}
	};
	stbi_write_png_to_func(savefunc, (void *)path.str().c_str(), w, h, STBI_rgb_alpha, dstBuffer, 0);

	free(dstBuffer);
}

void CustomTexture::prepareSource(BaseCustomTextureSource* source)
{
	bool shouldPreload = source->shouldPreload();
	const bool preloadToGpu = shouldPreload
			&& config::customTexturePreloadMode() == config::CustomTexturePreloadMode::VideoMemory;
	const CustomTextureCapabilities activeCapabilities = getCapabilities();

	if (!stopPreload && source->loadMap(activeCapabilities))
	{
		if (shouldPreload)
		{
			int count = static_cast<int>(source->getTextureCount());
			if (count > 0)
			{
				preloadTotal += count;
				auto callback = [this, preloadToGpu](u32 hash, PreparedCustomTexturePtr texture) {
					size_t size = texture ? texture->bytes.size() : 0;
					if (preloadToGpu)
					{
						if (texture)
							submitGpuPreload(hash, std::move(texture));
						else
							preloadLoaded++;
						return;
					}
					if (texture)
					{
						std::lock_guard<std::mutex> lock(stateMutex);
						preloadedTextures.emplace(hash, std::move(texture));
					}
					preloadLoaded++;
					preloadLoadedSize += size;
				};
				source->preloadTextures(activeCapabilities, callback, &stopPreload);
			}
		}
	}

	if (shouldPreload)
		pendingPreloads--;
}

void CustomTexture::submitGpuPreload(u32 hash, PreparedCustomTexturePtr texture)
{
	std::unique_lock<std::mutex> lock(stateMutex);
	gpuPreloadCondition.wait(lock, [this] {
		return pendingGpuPreloads.size() < MaxPendingGpuPreloads || stopPreload;
	});
	if (stopPreload)
		return;
	pendingGpuPreloads.emplace_back(hash, std::move(texture));
}

void CustomTexture::processGpuPreloads(const GpuTextureUploader& uploader)
{
	constexpr int MaxUploadsPerFrame = 8;
	for (int i = 0; i < MaxUploadsPerFrame; ++i)
	{
		std::pair<u32, PreparedCustomTexturePtr> pending;
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (pendingGpuPreloads.empty())
				break;
			pending = std::move(pendingGpuPreloads.front());
			pendingGpuPreloads.pop_front();
		}
		gpuPreloadCondition.notify_one();
		const bool uploaded = pending.second && uploader
				&& uploader(pending.first, *pending.second);
		if (uploaded)
			preloadLoadedSize += pending.second->bytes.size();
		else
			reportError(Error::Upload);
		preloadLoaded++;
	}
}

void CustomTexture::getPreloadProgress(int& completed, int& total, size_t& loadedSize) const
{
	total = preloadTotal;
	if (total == 0 && pendingPreloads > 0)
		total = -1; // Prints Preparing... in UI
	completed = preloadLoaded;
	loadedSize = preloadLoadedSize;
}

void CustomTexture::reportError(Error error)
{
	if (error == Error::None)
		return;
	// Loader errors may occur during preloading, before the in-game toast is visible.
	std::lock_guard<std::mutex> lock(stateMutex);
	if (!errorNotificationShown && pendingError == Error::None)
		pendingError = error;
}

void CustomTexture::showErrorNotification()
{
	Error error;
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		if (errorNotificationShown || pendingError == Error::None)
			return;
		error = pendingError;
		pendingError = Error::None;
		errorNotificationShown = true;
	}

	const char *message = nullptr;
	switch (error)
	{
	case Error::FileRead:
		message = i18n::T("Custom texture file could not be read");
		break;
	case Error::ImageDecode:
		message = i18n::T("PNG/JPEG custom texture could not be decoded");
		break;
	case Error::CompressedSource:
		message = i18n::T("Compressed custom texture could not be loaded.\nSupported formats: KTX2/XUBC7, KTX2/XUASTC, KTX2/ETC1S, and DDS/BC7");
		break;
	case Error::TextureTooLarge:
		message = i18n::T("Custom texture exceeds the renderer's maximum texture dimensions");
		break;
	case Error::Upload:
		message = i18n::T("Custom texture could not be uploaded to the GPU");
		break;
	case Error::DirectX9CompressedSource:
		message = i18n::T("DirectX 9 supports PNG/JPEG custom textures only");
		break;
	case Error::None:
		return;
	}
	if (message != nullptr)
		os_notify(message, 10000);
}

void CustomTexture::resetPreloadProgress()
{
	preloadTotal = 0;
	preloadLoaded = 0;
	preloadLoadedSize = 0;
}
