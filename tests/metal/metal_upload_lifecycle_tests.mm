// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_preload.h"
#include "rend/metal4/metal4_preload_lookup.h"
#include "rend/metal4/metal4_upload_state.h"
#include "rend/metal4/metal4_upload_scope.h"
#include "rend/metal4/metal4_submission.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void availableOnce(dispatch_semaphore_t available) {
	require(dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)==0,"Upload staging slot not returned");
	const bool extra=dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)==0;
	dispatch_semaphore_signal(available);require(!extra,"Upload staging slot returned twice");
}
static PreparedCustomTexture payload() {
	PreparedCustomTexture source;source.width=source.height=16;source.levels.push_back(computeMipLayout(NativeTextureFormat::Rgba8Unorm,16,16,0));
	source.bytes.resize(1024);for(size_t i=0;i<source.bytes.size();++i)source.bytes[i]=uint8_t(i*17+31);return source;
}
struct SignalEvent {id<MTLSharedEvent> event;~SignalEvent(){event.signaledValue=1;}};
static void transfer(FCMetal4Commands *cb,id<MTLTexture> image,id<MTLBuffer> input,id<MTLBuffer> output) {
	auto copy=[cb transferEncoder];
	[copy copyFromBuffer:input sourceOffset:0 sourceBytesPerRow:64 sourceBytesPerImage:0 sourceSize:MTLSizeMake(16,16,1) toTexture:image destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
	[copy copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(16,16,1) toBuffer:output destinationOffset:0 destinationBytesPerRow:64 destinationBytesPerImage:0];[copy endEncoding];
}
static void cancellation(id<MTLDevice> device,bool failedCommit) {
	auto queue=[[FCMetal4Queue alloc] initWithDevice:device];auto available=dispatch_semaphore_create(1);
	auto failed=std::make_shared<std::atomic<bool>>(false);std::shared_ptr<metal_upload::State> state;
	auto source=payload();auto budget=std::make_shared<metal_preload::Budget>(1ull<<20,1);id<MTLTexture> image;
	@autoreleasepool {
		auto allocated=metal_preload::allocate(device,source,budget);image=allocated.image;require(image!=nil,"Cannot allocate canceled preload");
		require(!metal_preload::usable(image,queue),"Untracked allocation admitted");
		// Narrow no-window adapter for the actual production Scope template;
		// watch, command submission/retirement and GPU resources remain real.
		struct ScopeContext {
			FCMetal4Commands *command;dispatch_group_t submissions;unsigned canceled=0;
			void beginTextureUpdates(){}
			void endTextureUpdates(bool){auto cb=command;command=nil;commitCommands(cb,submissions);}
			void cancelTextureUpdates()noexcept{++canceled;command=nil;}
		} context{};
		context.command=[queue commandBuffer];context.submissions=dispatch_group_create();bool caught=false;
		try {
			metal_upload::Scope scope(&context);auto cb=context.command;
			state=metal_upload::watch(cb,available,failed);metal_preload::trackUpload(image,state);
			require(metal_preload::usable(image,queue),"Pending preload unavailable on its own queue");
			auto input=[device newBufferWithBytes:source.bytes.data() length:1024 options:MTLResourceStorageModeShared];auto output=[device newBufferWithLength:1024 options:MTLResourceStorageModeShared];
			transfer(cb,image,input,output);
			if(failedCommit)[cb setCommitFailureForTesting:YES];else throw std::runtime_error("Injected preparation failure");
		} catch(const std::exception&){caught=true;}
		require(caught && !context.command && context.canceled==unsigned(!failedCommit) && dispatch_group_wait(context.submissions,DISPATCH_TIME_NOW)==0,"Scope failure leaked command/group or submitted while unwinding");
	}
	require(state->phase()==metal_upload::Phase::Canceled && failed->load() && !metal_preload::usable(image,queue),"Canceled/pre-commit-failed preload remained adoptable");
	availableOnce(available);image=nil;require(!budget->usage().bytes,"Canceled preload image retained its budget after owner release");
}
static void submitted(id<MTLDevice> device,bool feedbackError,unsigned iteration) {
	auto queue=[[FCMetal4Queue alloc] initWithDevice:device],other=[[FCMetal4Queue alloc] initWithDevice:device];
	auto available=dispatch_semaphore_create(1);auto failed=std::make_shared<std::atomic<bool>>(false);auto gate=[device newSharedEvent];SignalEvent signal{gate};
	auto source=payload();auto budget=std::make_shared<metal_preload::Budget>(1ull<<20,1);id<MTLTexture> image;
	auto input=[device newBufferWithBytes:source.bytes.data() length:1024 options:MTLResourceStorageModeShared];auto output=[device newBufferWithLength:1024 options:MTLResourceStorageModeShared];
	std::shared_ptr<metal_upload::State> state;FCMetal4Commands *cb;auto submissions=dispatch_group_create();
	@autoreleasepool {
		auto allocated=metal_preload::allocate(device,source,budget);image=allocated.image;cb=[queue commandBuffer];
		state=metal_upload::watch(cb,available,failed);metal_preload::trackUpload(image,state);
		require(metal_preload::usable(image,queue) && !metal_preload::usable(image,other),"Pending image crossed queues without completion/event");
		[cb waitForEvent:gate value:1];transfer(cb,image,input,output);
		if(iteration&1) {
			auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:4 height:4 mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageRenderTarget;
			auto target=[device newTextureWithDescriptor:d];auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(.2,.4,.6,1);
			auto render=[cb renderCommandEncoderWithDescriptor:pass];[render endEncoding];
		}
		if(feedbackError)[cb setFeedbackErrorForTesting:[NSError errorWithDomain:@"org.flycast.test.synthetic-feedback" code:17 userInfo:nil]];
		commitCommands(cb,submissions);
	}
	require(state->phase()==metal_upload::Phase::Pending && !failed->load(),"GPU event did not hold pending state");
	require(dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)!=0,"Staging slot released before GPU feedback");
	// Consumer work on the same queue is ordered after the upload without a
	// producer CPU wait. This mirrors uploads merged with/submitted before Render.
	FCMetal4Commands *consumer;id<MTLBuffer> consumed=[device newBufferWithLength:1024 options:MTLResourceStorageModeShared];
	if(!feedbackError){consumer=[queue commandBuffer];auto e=[consumer transferEncoder];[e copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(16,16,1) toBuffer:consumed destinationOffset:0 destinationBytesPerRow:64 destinationBytesPerImage:0];[e endEncoding];[consumer commit];}
	gate.signaledValue=1;[cb waitUntilCompleted];require(dispatch_group_wait(submissions,DISPATCH_TIME_NOW)==0,"Completed upload leaked submission group");
	require(std::memcmp(output.contents,source.bytes.data(),1024)==0,"Actual GPU transfer payload changed");
	if(feedbackError){require(cb.status==FCMetal4StatusError && state->phase()==metal_upload::Phase::Failed && failed->load() && !metal_preload::usable(image,queue) && !metal_preload::usable(image,other),"Synthetic feedback failure remained adoptable");require([queue submissionPoolStatistics][@"pooledAllocatorCount"].unsignedIntegerValue==0,"Failed GPU submission retained allocator storage in the reuse pool");}
	else{[consumer waitUntilCompleted];require(cb.status==FCMetal4StatusCompleted && state->phase()==metal_upload::Phase::Ready && !failed->load() && metal_preload::usable(image,other),"Successful upload state/queue handoff failed");require(std::memcmp(consumed.contents,source.bytes.data(),1024)==0,"Same-queue dependent read raced the upload");
		auto handoff=[other commandBuffer];auto e=[handoff transferEncoder];[e copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(16,16,1) toBuffer:consumed destinationOffset:0 destinationBytesPerRow:64 destinationBytesPerImage:0];[e endEncoding];[handoff commit];[handoff waitUntilCompleted];require(handoff.status==FCMetal4StatusCompleted && std::memcmp(consumed.contents,source.bytes.data(),1024)==0,"Ready image handoff to another queue changed contents");}
	availableOnce(available);cb=nil;consumer=nil;availableOnce(available);
	// A failed current-hash entry must not silently use a resident old alias.
	std::unordered_map<uint32_t,id<MTLTexture>> images{{1,image},{2,image}};
	auto found=metal_preload::findResident(images,1,0,2,[](uint32_t){return false;},[&](auto texture){return metal_preload::usable(texture,queue);});
	require(bool(found)==!feedbackError,"Lookup exposed a failed current-hash upload");found=nil;images.clear();image=nil;
	@autoreleasepool {auto again=metal_preload::allocate(device,source,budget);require(again.image!=nil,"Retired upload kept budget charged");auto fresh=[queue commandBuffer];auto freshState=metal_upload::watch(fresh,available,failed);metal_preload::trackUpload(again.image,freshState);transfer(fresh,again.image,input,output);[fresh commit];[fresh waitUntilCompleted];require(fresh.status==FCMetal4StatusCompleted && freshState->phase()==metal_upload::Phase::Ready,"Synthetic error leaked into the next pooled command");}
	require(!budget->usage().bytes,"Retired test images kept their budget");availableOnce(available);
}
int main(){@autoreleasepool{try{
	auto device=MTLCreateSystemDefaultDevice();require(device && [device supportsFamily:MTLGPUFamilyMetal4],"Metal 4 device required");
	cancellation(device,false);cancellation(device,true);
	for(unsigned i=0;i<8;++i){@autoreleasepool{submitted(device,false,i);submitted(device,true,i);}}
	std::puts("PASS native upload lifecycle: unsubmitted cancellation, injected pre-commit failure/group rollback, 16 gated real-GPU upload/readbacks (8 merged with render passes), 8 synthetic feedback errors with allocator discard, exactly-once staging release, same-queue pending/other-queue ready reads, invalid-entry lookup, budget release and fresh pooled-command reuse; no physical GPU fault injected");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}}
