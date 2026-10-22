// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_preload.h"
#include "rend/metal4/metal4_commands.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static PreparedCustomTexture payload(NativeTextureFormat format=NativeTextureFormat::Rgba8Unorm,unsigned levels=1) {
	PreparedCustomTexture source;source.nativeFormat=format;source.width=source.height=16;
	uint32_t size=16;uint64_t offset=0;
	for(unsigned i=0;i<levels;++i){auto mip=computeMipLayout(format,size,size,offset);source.levels.push_back(mip);offset+=mip.byteSize;size=std::max(1u,size/2);}
	source.bytes.resize(offset);
	for(size_t i=0;i<source.bytes.size();++i)source.bytes[i]=uint8_t(i*17+31);
	return source;
}
struct ReleaseEvent {id<MTLSharedEvent> event;~ReleaseEvent(){event.signaledValue=1;}};
int main(){@autoreleasepool{try{
	auto device=MTLCreateSystemDefaultDevice();require(device && [device supportsFamily:MTLGPUFamilyMetal4],"Metal 4 device required");
	auto defaults=metal_preload::deviceBudget(device);require(defaults && defaults==metal_preload::deviceBudget(device),"Per-device budget was recreated");
	require(defaults->byteLimit<=512ull*1024*1024 && defaults->imageLimit==4096,"Default speculative budget is not bounded");
	require(!metal_preload::deviceBudget(nil),"Nil device created a budget");
	auto budget=std::make_shared<metal_preload::Budget>(64ull<<20,1);unsigned formats=0;
	const auto caps=metal_custom::capabilities(device);
	for(unsigned f=0;f<unsigned(NativeTextureFormat::Count);++f){@autoreleasepool{
		if(!caps.supports(NativeTextureFormat(f)))continue;
		auto source=payload(NativeTextureFormat(f),5);auto first=metal_preload::allocate(device,source,budget);
		require(first.image && !first.deferred && budget->usage().bytes==first.image.allocatedSize && budget->usage().images==1,"Native preload allocation not charged at actual size");
		require(first.image.mipmapLevelCount==5,"Budgeted preload lost mip levels");
		auto denied=metal_preload::allocate(device,source,budget);require(!denied.image && denied.deferred,"Image cap did not defer preload");
		if(f==unsigned(NativeTextureFormat::Bc1Unorm))require(first.image.swizzle.alpha==MTLTextureSwizzleOne,"BC1 preload lost RGB alpha semantics");
		id<MTLTexture> adopted=first.image;first.image=nil;require(budget->usage().images==1 && adopted!=nil,"Adopted image lost its charge");++formats;
	}require(!budget->usage().bytes && !budget->usage().images,"Released format allocation retained a charge");}
	@autoreleasepool {auto generated=payload();generated.generateMipmaps=true;auto allocated=metal_preload::allocate(device,generated,budget);require(allocated.image.mipmapLevelCount==5 && budget->usage().bytes==allocated.image.allocatedSize,"Generated mip chain was not fully charged");}
	require(!budget->usage().bytes,"Generated mip allocation retained charge");
	auto source=payload();auto descriptor=metal_custom::descriptor(source,true);const uint64_t estimate=[device heapTextureSizeAndAlignWithDescriptor:descriptor].size;
	require(estimate>0,"Driver allocation estimate unavailable");
	auto small=std::make_shared<metal_preload::Budget>(estimate-1,4);
	{auto denied=metal_preload::allocate(device,source,small);require(!denied.image && denied.deferred && !small->usage().bytes,"Byte admission did not reject before allocating");}
	// Cancellation before submission must return the charge after releasing
	// the unsubmitted command's resource references and caller ownership.
	auto queue=[[FCMetal4Queue alloc] initWithDevice:device];
	@autoreleasepool {
		auto allocated=metal_preload::allocate(device,source,budget);require(allocated.image!=nil,"Cancellation fixture allocation failed");
		auto canceled=[queue commandBuffer];[canceled useResource:allocated.image];allocated.image=nil;
		require(budget->usage().images==1,"Unsubmitted commands lost resource charge");canceled=nil;
	}
	require(!budget->usage().bytes && !budget->usage().images,"Canceled submission leaked admission");
	auto gate=[device newSharedEvent];ReleaseEvent release{gate};
	auto bytes=[device newBufferWithBytes:source.bytes.data() length:source.bytes.size() options:MTLResourceStorageModeShared];
	auto readback=[device newBufferWithLength:source.bytes.size() options:MTLResourceStorageModeShared];FCMetal4Commands *pending;
	@autoreleasepool {
		auto allocated=metal_preload::allocate(device,source,budget);pending=[queue commandBuffer];
		[pending waitForEvent:gate value:1];auto copy=[pending transferEncoder];
		[copy copyFromBuffer:bytes sourceOffset:0 sourceBytesPerRow:64 sourceBytesPerImage:0 sourceSize:MTLSizeMake(16,16,1) toTexture:allocated.image destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		[copy copyFromTexture:allocated.image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(16,16,1) toBuffer:readback destinationOffset:0 destinationBytesPerRow:64 destinationBytesPerImage:0];
		[copy endEncoding];[pending commit];allocated.image=nil;
	}
	require(budget->usage().images==1 && budget->usage().bytes>0,"In-flight GPU image lost reservation after all CPU owners released");
	{auto denied=metal_preload::allocate(device,source,budget);require(denied.deferred && !denied.image,"In-flight resource was admitted twice past cap");}
	gate.signaledValue=1;[pending waitUntilCompleted];require(pending.status==FCMetal4StatusCompleted,"GPU upload/readback failed");
	require(std::memcmp(readback.contents,source.bytes.data(),source.bytes.size())==0,"Budgeted GPU payload changed");pending=nil;
	require(!budget->usage().bytes && !budget->usage().images,"GPU completion failed to release final image charge");
	@autoreleasepool {require(metal_preload::allocate(device,source,budget).image!=nil,"Completed budget could not be reused");}
	require(!budget->usage().bytes && !budget->usage().images,"Readmission leaked charge");
	std::printf("PASS native preload admission: %u five-mip format allocations and generated mip chain charged at allocatedSize, per-device reuse, byte/image caps, BC1 swizzle, adopted ownership, cancellation, gated GPU upload/readback and readmission after completion\n",formats);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}}
