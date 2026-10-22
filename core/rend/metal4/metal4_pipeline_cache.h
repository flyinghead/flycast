// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_commands.h"
#include <cstdint>
#include <array>
#include <memory>
#include <string>

// Independent read archives keep foreground PSO creation separate from the
// material compiler and the serial, best-effort archive writer.
class API_AVAILABLE(macos(26.0)) MetalPipelineCache : public std::enable_shared_from_this<MetalPipelineCache>
{
public:
	static std::shared_ptr<MetalPipelineCache> create(id<MTL4Compiler> foreground, id<MTL4Compiler> material,
		const char *source, size_t sourceSize, MTLCompileOptions *options);
	~MetalPipelineCache();
	id<MTLRenderPipelineState> makeRenderPipeline(MTL4RenderPipelineDescriptor *descriptor,
		NSError **error, bool material = false, const std::string& identity = {});
	id<MTLComputePipelineState> makeComputePipeline(MTL4ComputePipelineDescriptor *descriptor,
		NSError **error, const std::string& identity = {});
	struct Statistics {
		struct Timing { uint64_t calls=0,nanoseconds=0,maximum=0; };
		uint64_t hits,misses,probes,hintedHits,queued,recordings,duplicates,savedSegments;
		size_t pending;
		bool writable,accepting;
		std::array<Timing,3> lookup{},creation{};
		std::array<Timing,2> writer{};
		Timing serialization{},publication{};
		bool timing=false;
		uint64_t descriptorCopies=0;
	};
#ifdef FLYCAST_METAL_STANDALONE
	static void setIdentityOptimizationsForTesting(bool enabled,bool timing=false);
	void recordForTesting(id descriptor,bool compute,const std::string& identity) { record(descriptor,compute,identity); }
#endif
	Statistics statistics() const;
	// Call after draining the material queue, before releasing the context.
	void finish() noexcept;
private:
	MetalPipelineCache();
	void record(id descriptor, bool compute, const std::string& identity) noexcept;
	void recordOnWriter(id descriptor, bool compute, const std::string& identity) noexcept;
	void save() noexcept;
	void prune() noexcept;
	struct Impl;
	std::unique_ptr<Impl> impl;
};
