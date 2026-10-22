// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_commands.h"
#include <array>
#include <cstdint>
#include <string>

namespace metal_pipeline {
// Canonical bytes, with explicit lengths and integer widths. These identities
// are scoped to the cache's source/compiler/GPU identity, never object addresses.
class Identity {
	std::string bytes;
	bool valid = true;
public:
	void number(uint64_t value) { for(unsigned i=0;i<8;++i)bytes.push_back(char(value>>(i*8))); }
	void data(const std::string& value) {number(value.size());bytes.append(value);}
	void text(NSString *value) {
		number(value!=nil);
		if(value) {
			NSData *utf8=[value dataUsingEncoding:NSUTF8StringEncoding];
			if(!utf8) {valid=false;return;}
			number(utf8.length);if(utf8.length)bytes.append(static_cast<const char *>(utf8.bytes),utf8.length);
		}
	}
	std::string finish() {return valid ? std::move(bytes) : std::string{};}
};
template<class Build> std::string optionalIdentity(Build&& build) noexcept
{
	try { @try { return build(); } @catch(NSException *) {return {};} }
	catch(...) {return {};}
}
inline std::string functionIdentity(NSString *name,const std::array<uint32_t,4> *constants=nullptr) noexcept
{
	return optionalIdentity([&] {
	Identity key;key.text(name);key.number(constants!=nullptr);
	if(constants)for(uint32_t value:*constants)key.number(value);
	return key.finish();
	});
}
API_AVAILABLE(macos(26.0))
inline bool emptyLinking(MTL4StaticLinkingDescriptor *link)
{
	return !link.functionDescriptors.count && !link.privateFunctionDescriptors.count && !link.groups.count;
}
API_AVAILABLE(macos(26.0))
inline bool supportedFunction(MTL4FunctionDescriptor *function,bool nullable=false)
{
	if(!function)return nullable;
	if([function isKindOfClass:MTL4SpecializedFunctionDescriptor.class]) {
		auto specialized=static_cast<MTL4SpecializedFunctionDescriptor *>(function);
		if(specialized.specializedName || !specialized.constantValues)return false;
		function=specialized.functionDescriptor;
	}
	if(![function isKindOfClass:MTL4LibraryFunctionDescriptor.class])return false;
	auto library=static_cast<MTL4LibraryFunctionDescriptor *>(function);
	return library.name!=nil && library.library!=nil;
}
API_AVAILABLE(macos(26.0))
inline void commonIdentity(Identity& key,MTL4PipelineDescriptor *descriptor)
{
	key.text(descriptor.label);key.number(descriptor.options!=nil);
	if(descriptor.options) {key.number(descriptor.options.shaderValidation);key.number(descriptor.options.shaderReflection);}
}
// Function constant values are deliberately supplied by the factory that sets
// them: Metal's opaque MTLFunctionConstantValues cannot be read back reliably.
// A descriptor outside our supported family simply gets no memoization key.
API_AVAILABLE(macos(26.0))
inline std::string renderIdentity(MTL4RenderPipelineDescriptor *d,const std::string& vertex,const std::string& fragment) noexcept
{
	return optionalIdentity([&]() -> std::string {
	if(!d || vertex.empty() || fragment.empty() || !supportedFunction(d.vertexFunctionDescriptor)
		|| !supportedFunction(d.fragmentFunctionDescriptor,true)
		|| !emptyLinking(d.vertexStaticLinkingDescriptor) || !emptyLinking(d.fragmentStaticLinkingDescriptor))return {};
	Identity key;key.data("Flycast-Metal4-render-identity-v1");key.data(vertex);key.data(fragment);commonIdentity(key,d);
	key.number(d.rasterSampleCount);key.number(d.alphaToCoverageState);key.number(d.alphaToOneState);
	key.number(d.rasterizationEnabled);key.number(d.maxVertexAmplificationCount);key.number(d.inputPrimitiveTopology);
	key.number(d.supportVertexBinaryLinking);key.number(d.supportFragmentBinaryLinking);
	key.number(d.colorAttachmentMappingState);key.number(d.supportIndirectCommandBuffers);
	key.number(d.vertexDescriptor!=nil);
	if(d.vertexDescriptor)for(NSUInteger i=0;i<31;++i) {
		auto attribute=d.vertexDescriptor.attributes[i];auto buffer=d.vertexDescriptor.layouts[i];
		key.number(attribute.format);key.number(attribute.offset);key.number(attribute.bufferIndex);
		key.number(buffer.stride);key.number(buffer.stepFunction);key.number(buffer.stepRate);
	}
	for(NSUInteger i=0;i<8;++i) {
		auto color=d.colorAttachments[i];
		key.number(color.pixelFormat);key.number(color.blendingState);key.number(color.sourceRGBBlendFactor);
		key.number(color.destinationRGBBlendFactor);key.number(color.rgbBlendOperation);key.number(color.sourceAlphaBlendFactor);
		key.number(color.destinationAlphaBlendFactor);key.number(color.alphaBlendOperation);key.number(color.writeMask);
	}
	return key.finish();
	});
}
API_AVAILABLE(macos(26.0))
inline std::string computeIdentity(MTL4ComputePipelineDescriptor *d,const std::string& function) noexcept
{
	return optionalIdentity([&]() -> std::string {
	if(!d || function.empty() || !supportedFunction(d.computeFunctionDescriptor) || !emptyLinking(d.staticLinkingDescriptor))return {};
	Identity key;key.data("Flycast-Metal4-compute-identity-v1");key.data(function);commonIdentity(key,d);
	key.number(d.threadGroupSizeIsMultipleOfThreadExecutionWidth);key.number(d.maxTotalThreadsPerThreadgroup);
	key.number(d.requiredThreadsPerThreadgroup.width);key.number(d.requiredThreadsPerThreadgroup.height);key.number(d.requiredThreadsPerThreadgroup.depth);
	key.number(d.supportBinaryLinking);key.number(d.supportIndirectCommandBuffers);
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 270000
	if(@available(macOS 27.0,*)) {
		key.number(d.forwardProgressUsage);key.number(d.contentionRelief);key.number(d.optimizeForPersistentKernel);
	}
#endif
	return key.finish();
	});
}
}
