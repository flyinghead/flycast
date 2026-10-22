// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_preload.h"
#include "rend/metal4/metal4_commands.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static TextureData payload() {
	TextureData source;source.w=source.h=16;source.data.resize(1024);
	for(size_t i=0;i<source.data.size();++i)source.data[i]=uint8_t(i*17+31);return source;
}
struct ReleaseEvent {id<MTLSharedEvent> event;~ReleaseEvent(){event.signaledValue=1;}};
int main(){@autoreleasepool{try{
	auto device=MTLCreateSystemDefaultDevice();require(device && [device supportsFamily:MTLGPUFamilyMetal4],"Metal 4 device required");
	auto defaults=metal_preload::deviceBudget(device);require(defaults && defaults==metal_preload::deviceBudget(device),"Per-device budget was recreated");
	require(defaults->byteLimit<=512ull*1024*1024 && defaults->imageLimit==4096,"Default speculative budget is not bounded");
	require(!metal_preload::deviceBudget(nil),"Nil device created a budget");
	auto budget=std::make_shared<metal_preload::Budget>(64ull<<20,1);unsigned formats=0;
	@autoreleasepool {
		auto source=payload();auto first=metal_preload::allocate(device,source,budget);
		require(first.image && !first.deferred && budget->usage().bytes==first.image.allocatedSize,"RGBA allocation not charged at actual size");
		require(first.image.mipmapLevelCount==5,"RGBA preload lost generated mip levels");
		auto denied=metal_preload::allocate(device,source,budget);require(!denied.image && denied.deferred,"Image cap did not defer preload");
		id<MTLTexture> adopted=first.image;first.image=nil;require(budget->usage().images==1 && adopted!=nil,"Adopted image lost charge");++formats;
	}
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
	auto bytes=[device newBufferWithBytes:source.data.data() length:source.data.size() options:MTLResourceStorageModeShared];
	auto readback=[device newBufferWithLength:source.data.size() options:MTLResourceStorageModeShared];FCMetal4Commands *pending;
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
	require(std::memcmp(readback.contents,source.data.data(),source.data.size())==0,"Budgeted GPU payload changed");pending=nil;
	require(!budget->usage().bytes && !budget->usage().images,"GPU completion failed to release final image charge");
	@autoreleasepool {require(metal_preload::allocate(device,source,budget).image!=nil,"Completed budget could not be reused");}
	require(!budget->usage().bytes && !budget->usage().images,"Readmission leaked charge");
	std::printf("PASS native preload admission: %u RGBA allocations and generated mip chain charged at allocatedSize, per-device reuse, byte/image caps, adopted ownership, cancellation, gated GPU upload/readback and readmission after completion\n",formats);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}}
