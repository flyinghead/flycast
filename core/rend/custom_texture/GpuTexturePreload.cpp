// SPDX-License-Identifier: GPL-2.0-or-later
// Shared renderer/custom-texture boundary, kept separate from the emulation queue.
#include "hw/pvr/Renderer_if.h"
#include "rend/CustomTexture.h"
#include <cassert>

void rend_process_custom_texture_preloads()
{
	if (renderer != nullptr)
	{
		renderer->processGpuCleanupOperations();
		renderer->processCustomTexturePreloads();
	}
}

bool rend_supports_gpu_texture_preload()
{
	return renderer != nullptr && renderer->supportsGpuTexturePreload();
}

std::shared_ptr<GpuPreloadedTexture> Renderer::findGpuPreloadedTexture(
		u32 currentHash, u32 oldVqHash, u32 oldHash) const
{
	const auto find = [this](u32 hash) -> std::shared_ptr<GpuPreloadedTexture> {
		const auto found = gpuPreloadedTextures.find(hash);
		return found == gpuPreloadedTextures.end() ? nullptr : found->second;
	};
	if (auto texture = find(currentHash))
		return texture;
	if (oldVqHash != 0)
		if (auto texture = find(oldVqHash))
			return texture;
	if (oldHash != 0)
		return find(oldHash);
	return nullptr;
}

void Renderer::addGpuPreloadedTexture(u32 hash, std::shared_ptr<GpuPreloadedTexture> texture)
{
	assert(texture);
	gpuPreloadedTextures.emplace(hash, std::move(texture));
}

void Renderer::clearGpuPreloadedTextures()
{
	gpuPreloadedTextures.clear();
}

void Renderer::processGpuCleanupOperations()
{
	if (custom_texture.consumeGpuCleanupOperations())
	{
		clearTextureCache();
		clearGpuPreloadedTextures();
		resetTextureCache = false;
	}
}

