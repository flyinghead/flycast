// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal_context.h"

// Independent read archives keep foreground PSO creation separate from the
// material compiler and the serial, best-effort archive writer.
class MetalPipelineCache : public std::enable_shared_from_this<MetalPipelineCache>
{
public:
	static std::shared_ptr<MetalPipelineCache> create(id<MTL4Compiler> foreground, id<MTL4Compiler> material,
		const char *source, size_t sourceSize, MTLCompileOptions *options);
	~MetalPipelineCache();
	id<MTLRenderPipelineState> makeRenderPipeline(MTL4RenderPipelineDescriptor *descriptor,
		NSError **error, bool material = false);
	id<MTLComputePipelineState> makeComputePipeline(MTL4ComputePipelineDescriptor *descriptor,
		NSError **error);
	// Call after draining the material queue, before releasing the context.
	void finish() noexcept;
private:
	MetalPipelineCache();
	void record(id descriptor, bool compute) noexcept;
	void recordOnWriter(id descriptor, bool compute) noexcept;
	void save() noexcept;
	void prune() noexcept;
	struct Impl;
	std::unique_ptr<Impl> impl;
};
