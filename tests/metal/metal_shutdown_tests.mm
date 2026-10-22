// SPDX-License-Identifier: GPL-2.0-or-later
// Real Metal queues and transfers; no app, display surface or game is opened.
#include "rend/metal4/metal4_shutdown.h"
#include "rend/metal4/metal4_submission.h"
#include "rend/metal4/metal4_upload_state.h"
#include "rend/metal4/metal4_context.h"
#include "rend/metal4/metal4_preload.h"
#include "log/Log.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <cstdarg>
#include <optional>

void GenericLog(LogTypes::LOG_LEVELS,LogTypes::LOG_TYPE,const char*,int,const char *format,...)
{va_list args;va_start(args,format);std::vfprintf(stderr,format,args);std::fputc('\n',stderr);va_end(args);}
struct MetalRenderCommandsTest {
	static MetalContext::RenderCommands make(FCMetal4Commands *command,dispatch_group_t submissions,std::function<void()> canceled,
		std::shared_ptr<metal_upload::State> state,metal_upload::FailureFlag failed) {
		return {command,submissions,std::move(canceled),std::move(state),std::move(failed)};
	}
};

static void require(bool ok,const char *message) {if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);throw std::runtime_error(message);}}
static void exactlyOne(dispatch_semaphore_t slot) {
	require(dispatch_semaphore_wait(slot,DISPATCH_TIME_NOW)==0,"Slot was not returned");
	const bool extra=dispatch_semaphore_wait(slot,DISPATCH_TIME_NOW)==0;
	dispatch_semaphore_signal(slot);require(!extra,"Slot returned twice");
}
static bool unavailable(dispatch_semaphore_t slot) {
	if(dispatch_semaphore_wait(slot,DISPATCH_TIME_NOW)!=0)return true;
	dispatch_semaphore_signal(slot);return false;
}
struct EventRelease {
	id<MTLSharedEvent> event;
	~EventRelease(){event.signaledValue=1;}
};
static void copy(FCMetal4Commands *command,id<MTLBuffer> source,id<MTLBuffer> output,id<MTLTexture> texture=nil) {
	auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:16 height:16 mipmapped:NO];
	descriptor.storageMode=MTLStorageModePrivate;
	if(!texture)texture=[command.commandQueue.device newTextureWithDescriptor:descriptor];require(texture!=nil,"Cannot allocate transfer texture");
	auto encoder=[command transferEncoder];
	[encoder copyFromBuffer:source sourceOffset:0 sourceBytesPerRow:64 sourceBytesPerImage:0 sourceSize:MTLSizeMake(16,16,1) toTexture:texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
	[encoder copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(16,16,1) toBuffer:output destinationOffset:0 destinationBytesPerRow:64 destinationBytesPerImage:0];
	[encoder endEncoding];
}
enum class Fault { Cancel, BeforeCommit, BeforeEnqueue, AfterEnqueue, Feedback, None };
static void inject(FCMetal4Commands *command,Fault fault) {
	if(fault==Fault::BeforeCommit)[command setCommitFailureForTesting:YES];
	if(fault==Fault::BeforeEnqueue)[command setQueueCommitFailureForTesting:YES];
	if(fault==Fault::AfterEnqueue)[command setPostCommitFailureForTesting:YES];
	if(fault==Fault::Feedback)[command setFeedbackErrorForTesting:
		[NSError errorWithDomain:@"org.flycast.test.synthetic-feedback" code:23 userInfo:nil]];
}
static void slotLifetime(id<MTLDevice> device,Fault fault,bool adopted) {
	auto queue=[[FCMetal4Queue alloc] initWithDevice:device];
	auto slot=dispatch_semaphore_create(1);auto submissions=dispatch_group_create();
	auto event=[device newSharedEvent];EventRelease release{event};
	std::array<unsigned char,1024> expected{};
	for(unsigned i=0;i<expected.size();++i)expected[i]=i*29+unsigned(fault);
	auto source=[device newBufferWithBytes:expected.data() length:expected.size() options:MTLResourceStorageModeShared];
	auto output=[device newBufferWithLength:expected.size() options:MTLResourceStorageModeShared];
	const bool enqueued=fault==Fault::AfterEnqueue || fault==Fault::Feedback || fault==Fault::None;
	@autoreleasepool {
		auto command=[queue commandBuffer];
		if(adopted){require(dispatch_semaphore_wait(slot,DISPATCH_TIME_NOW)==0,"Cannot reserve caller slot");metal_upload::adoptSlot(command,slot);}
		else metal_upload::reserveSlot(command,slot);
		require(unavailable(slot),"Slot not acquired");
		[command waitForEvent:event value:1];copy(command,source,output);inject(command,fault);
		bool threw=false;
		if(fault!=Fault::Cancel){try {commitCommands(command,submissions);}catch(const std::exception&){threw=true;}}
		require(threw==(fault==Fault::BeforeCommit || fault==Fault::BeforeEnqueue || fault==Fault::AfterEnqueue),"Wrong submission fault boundary");
		require((command.status>=FCMetal4StatusCommitted)==enqueued,"Submission status lost enqueue boundary");
		require((dispatch_group_wait(submissions,DISPATCH_TIME_NOW)!=0)==enqueued,"Submission group not balanced at failure boundary");
		require(unavailable(slot),"Fault returned slot while owner or GPU still holds it");
		if(enqueued){
			event.signaledValue=1;[command waitUntilCompleted];
			require(dispatch_group_wait(submissions,DISPATCH_TIME_NOW)==0,"GPU feedback did not drain group");
			require(command.status==(fault==Fault::Feedback?FCMetal4StatusError:FCMetal4StatusCompleted),"Unexpected feedback status");
			require(std::memcmp(output.contents,expected.data(),expected.size())==0,"GPU payload mismatch after failure boundary");
			exactlyOne(slot);
		}
	}
	exactlyOne(slot);
	// Cancellation and all failure types must leave the same queue usable.
	@autoreleasepool {
		auto next=[queue commandBuffer];metal_upload::reserveSlot(next,slot);copy(next,source,output);
		commitCommands(next,submissions);[next waitUntilCompleted];
		require(next.status==FCMetal4StatusCompleted && dispatch_group_wait(submissions,DISPATCH_TIME_NOW)==0,"Failure poisoned the next submission");
	}
	exactlyOne(slot);
}
static void adoptionFailure(id<MTLDevice> device) {
	auto slot=dispatch_semaphore_create(1);
	require(dispatch_semaphore_wait(slot,DISPATCH_TIME_NOW)==0,"Cannot acquire adoption test slot");
	bool caught=false;try {metal_upload::adoptSlot(nil,slot);}catch(const std::invalid_argument&){caught=true;}
	require(caught && dispatch_semaphore_wait(slot,DISPATCH_TIME_NOW)!=0,"Failed adoption stole caller ownership");
	dispatch_semaphore_signal(slot);exactlyOne(slot);
	// A completed command is not an encoding owner. Rejection must also keep
	// an already-reserved slot with its caller.
	auto queue=[[FCMetal4Queue alloc] initWithDevice:device];auto command=[queue commandBuffer];[command commit];[command waitUntilCompleted];
	dispatch_semaphore_wait(slot,DISPATCH_TIME_FOREVER);caught=false;
	try {metal_upload::adoptSlot(command,slot);}catch(const std::invalid_argument&){caught=true;}
	require(caught && dispatch_semaphore_wait(slot,DISPATCH_TIME_NOW)!=0,"Completed-command adoption changed caller ownership");
	dispatch_semaphore_signal(slot);exactlyOne(slot);
}
static void drainQueues(id<MTLDevice> device) {
	std::array<FCMetal4Queue *,2> queues{[[FCMetal4Queue alloc] initWithDevice:device],[[FCMetal4Queue alloc] initWithDevice:device]};
	std::array<FCMetal4Commands *,4> commands{};
	std::array<id<MTLBuffer>,4> outputs{};
	std::array<dispatch_semaphore_t,4> slots{};
	auto submissions=dispatch_group_create();auto event=[device newSharedEvent];EventRelease release{event};
	std::array<unsigned char,1024> expected{};for(unsigned i=0;i<expected.size();++i)expected[i]=i*7+13;
	auto source=[device newBufferWithBytes:expected.data() length:expected.size() options:MTLResourceStorageModeShared];
	@autoreleasepool {
		for(unsigned i=0;i<commands.size();++i){
			commands[i]=[queues[i%2] commandBuffer];slots[i]=dispatch_semaphore_create(1);
			metal_upload::reserveSlot(commands[i],slots[i]);[commands[i] waitForEvent:event value:1];
			outputs[i]=[device newBufferWithLength:expected.size() options:MTLResourceStorageModeShared];copy(commands[i],source,outputs[i]);
		}
		// Already submitted render work must survive failure of a later pending
		// upload; another queue and later pending presentation still need draining.
		commitCommands(commands[0],submissions);
		inject(commands[1],Fault::BeforeEnqueue);inject(commands[2],Fault::AfterEnqueue);
		metal_shutdown::Drain errors;unsigned attempts=0;bool cleanup=false;
		for(unsigned i=1;i<commands.size();++i)errors.attempt([&]{++attempts;auto command=commands[i];commands[i]=nil;commitCommands(command,submissions);});
		require(attempts==3 && errors.failed(),"An independent shutdown operation was skipped");
		require(dispatch_group_wait(submissions,DISPATCH_TIME_NOW)!=0,"Shutdown lost previously enqueued GPU work");
		for(unsigned i:{0u,2u,3u})require(unavailable(slots[i]),"Shutdown freed a live GPU command slot");
		std::thread signal([event]{std::this_thread::sleep_for(std::chrono::milliseconds(30));event.signaledValue=1;});
		dispatch_group_wait(submissions,DISPATCH_TIME_FOREVER);signal.join();
		require(event.signaledValue==1,"Drain did not wait for gated GPU work");
		errors.attempt([&]{cleanup=true;});
		bool first=false;try {errors.rethrow();}catch(const std::exception& e){first=std::string(e.what())=="Injected Metal queue pre-submit failure";}
		require(cleanup && first,"Shutdown did not preserve first failure through drain/cleanup");
		for(unsigned i:{0u,2u,3u})require(std::memcmp(outputs[i].contents,expected.data(),expected.size())==0,"Shutdown cleared a live GPU resource");
		commands.fill(nil);
		// Repeated drain must be harmless and must not enqueue extra work.
		metal_shutdown::Drain repeated;repeated.attempt([&]{dispatch_group_wait(submissions,DISPATCH_TIME_FOREVER);});
		repeated.rethrow();require(!repeated.failed(),"Repeated drain failed");
	}
	for(auto slot:slots)exactlyOne(slot);
}
static void renderOwner(id<MTLDevice> device,Fault fault,bool moved) {
	auto queue=[[FCMetal4Queue alloc] initWithDevice:device];auto submissions=dispatch_group_create();
	auto staging=dispatch_semaphore_create(1),render=dispatch_semaphore_create(1);
	auto failed=std::make_shared<std::atomic<bool>>(false);auto gate=[device newSharedEvent];EventRelease release{gate};
	std::array<unsigned char,1024> expected{};for(unsigned i=0;i<expected.size();++i)expected[i]=i*19+unsigned(fault);
	auto source=[device newBufferWithBytes:expected.data() length:expected.size() options:MTLResourceStorageModeShared];
	auto output=[device newBufferWithLength:expected.size() options:MTLResourceStorageModeShared];
	auto budget=std::make_shared<metal_preload::Budget>(1ull<<20,1);
	const bool enqueued=fault==Fault::AfterEnqueue || fault==Fault::Feedback || fault==Fault::None;
	@autoreleasepool {
		__attribute__((objc_precise_lifetime)) FCMetal4Commands *command=[queue commandBuffer];
		auto state=metal_upload::watch(command,staging,failed);bool canceled=false,consumed=false;
		PreparedCustomTexture payload;payload.width=payload.height=16;
		payload.levels.push_back(computeMipLayout(NativeTextureFormat::Rgba8Unorm,16,16,0));payload.bytes.assign(expected.begin(),expected.end());
		auto allocated=metal_preload::allocate(device,payload,budget);require(allocated.image!=nil,"Cannot allocate retained-owner preload");
		metal_preload::trackUpload(allocated.image,state);require(metal_preload::usable(allocated.image,queue),"Pending preload unavailable on producer queue");
		[command waitForEvent:gate value:1];copy(command,source,output,allocated.image);inject(command,fault);
		auto colorDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:8 height:8 mipmapped:NO];
		colorDescriptor.storageMode=MTLStorageModePrivate;colorDescriptor.usage=MTLTextureUsageRenderTarget;
		auto color=[device newTextureWithDescriptor:colorDescriptor];
		auto depthDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8 width:8 height:8 mipmapped:NO];
		depthDescriptor.storageMode=MTLStorageModePrivate;depthDescriptor.usage=MTLTextureUsageRenderTarget;
		auto depth=[device newTextureWithDescriptor:depthDescriptor];
		require(color && depth,"Cannot allocate actual render-owner attachments");
		auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=color;
		pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
		pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionUnknown;
		pass.stencilAttachment.texture=depth;pass.stencilAttachment.loadAction=MTLLoadActionClear;pass.stencilAttachment.storeAction=MTLStoreActionUnknown;
		{
			auto owner=MetalRenderCommandsTest::make(command,submissions,[&]{canceled=true;},state,failed);
			dispatch_semaphore_wait(render,DISPATCH_TIME_FOREVER);owner.retainRenderSlot(render);
			[command addCompletedHandler:^(FCMetal4Commands *){dispatch_semaphore_signal(render);}];
			auto encoder=[command renderCommandEncoderWithDescriptor:pass];owner.retainRasterEncoder(encoder,color,depth,true);
			auto submit=[&](MetalContext::RenderCommands& current) {
				require(current.continuingRasterEncoder(color,depth)==encoder,"Moving RenderCommands lost its continued raster encoder");
				bool threw=false;
				if(fault!=Fault::Cancel){try {current.commit(moved);}catch(const std::exception&){threw=true;}}
				require(threw==(fault==Fault::BeforeCommit || fault==Fault::BeforeEnqueue || fault==Fault::AfterEnqueue),"Actual render-owner commit failed at wrong boundary");
				if(fault==Fault::BeforeCommit || fault==Fault::BeforeEnqueue) {
					require(state->phase()==metal_upload::Phase::Canceled && failed->exchange(false),"Pre-submit failure left upload adoptable until owner destruction");
					require(!metal_preload::usable(allocated.image,queue),"Failed owner left actual preload image adoptable");
					consumed=true;
				}
			};
			if(moved){auto next=std::move(owner);submit(next);}else submit(owner);
		}
		require(canceled==!enqueued,"Actual RenderCommands canceled already enqueued work or skipped cancellation");
		if(!enqueued) {
			exactlyOne(render);
			require(state->phase()==metal_upload::Phase::Canceled && !state->usableOn(state->producerQueue()),"Canceled render upload remains usable while native command is retained");
			require(!metal_preload::usable(allocated.image,queue),"Abandoned owner left actual preload image adoptable");
			require(failed->load()==!consumed,"Owner destruction lost or repeated its upload failure notification");
			failed->store(false);
			require(dispatch_group_wait(submissions,DISPATCH_TIME_NOW)==0,"Canceled render-owner leaked completion group");
		} else {
			require(state->phase()==metal_upload::Phase::Pending && !failed->load() && unavailable(render),"Actual render-owner retired enqueued work before feedback");
			require(dispatch_group_wait(submissions,DISPATCH_TIME_NOW)!=0,"Render-owner lost GPU completion tracking");
			gate.signaledValue=1;[command waitUntilCompleted];
			require(state->phase()==(fault==Fault::Feedback?metal_upload::Phase::Failed:metal_upload::Phase::Ready) && failed->load()==(fault==Fault::Feedback),"Actual render-owner corrupted terminal upload state");
			require(metal_preload::usable(allocated.image,queue)==(fault!=Fault::Feedback),"Actual preload image metadata differs from render-owner outcome");
			require(std::memcmp(output.contents,expected.data(),expected.size())==0,"Render-owner transfer payload changed");
			require(dispatch_group_wait(submissions,DISPATCH_TIME_NOW)==0,"Render-owner did not balance GPU feedback group");
			exactlyOne(render);
		}
		require(command.native!=nil || enqueued,"Retained-command lifetime test lost its command");
		if(!enqueued)require(unavailable(staging),"Immediate invalidation released a still-retained command's staging lease");
	}
	if(!enqueued)require(!failed->load(),"Retiring an already canceled command reissued the upload failure");
	require(!budget->usage().bytes,"Retired render-owner preload retained its budget");
	exactlyOne(staging);exactlyOne(render);
}
static void mergedOwners(id<MTLDevice> device,Fault fault) {
	auto queue=[[FCMetal4Queue alloc] initWithDevice:device];auto submissions=dispatch_group_create();
	auto staging=dispatch_semaphore_create(1);std::array<dispatch_semaphore_t,MetalContext::RenderBatchSlots> slots;
	for(auto& slot:slots)slot=dispatch_semaphore_create(1);
	auto failed=std::make_shared<std::atomic<bool>>(false);auto gate=[device newSharedEvent];EventRelease release{gate};
	std::array<unsigned char,1024> expected{};for(unsigned i=0;i<expected.size();++i)expected[i]=i*13+unsigned(fault);
	auto source=[device newBufferWithBytes:expected.data() length:expected.size() options:MTLResourceStorageModeShared];
	auto output=[device newBufferWithLength:expected.size() options:MTLResourceStorageModeShared];
	const bool enqueued=fault==Fault::AfterEnqueue || fault==Fault::Feedback || fault==Fault::None;
	@autoreleasepool {
		__attribute__((objc_precise_lifetime)) FCMetal4Commands *command=[queue commandBuffer];
		auto state=metal_upload::watch(command,staging,failed);unsigned canceled=0;
		[command waitForEvent:gate value:1];copy(command,source,output);inject(command,fault);
		{
			std::optional<MetalContext::RenderCommands> pending;
			pending.emplace(MetalRenderCommandsTest::make(command,submissions,[&]{++canceled;},state,failed));
			for(auto slot:slots) {
				MetalContext::RenderCommands current(std::move(*pending));pending.reset();
				dispatch_semaphore_wait(slot,DISPATCH_TIME_FOREVER);current.retainRenderSlot(slot);
				[command addCompletedHandler:^(FCMetal4Commands *){dispatch_semaphore_signal(slot);}];
				pending.emplace(std::move(current));
				require(!canceled && state->phase()==metal_upload::Phase::Pending,"Deferred owner move canceled an active merged batch");
			}
			bool bounded=false;
			try {pending->retainRenderSlot(dispatch_semaphore_create(1));}catch(const std::exception&){bounded=true;}
			require(bounded,"Merged render batch exceeded its slot bound");
			if(fault!=Fault::Cancel){try {pending->commit();}catch(const std::exception&){}
				if(!enqueued)require(state->phase()==metal_upload::Phase::Canceled && failed->load(),"Failed merged submit remained adoptable");}
		}
		require(canceled==unsigned(!enqueued),"Merged batch cancellation ran more than once or after enqueue");
		if(enqueued) {
			for(auto slot:slots)require(unavailable(slot),"Merged batch returned an in-flight render slot");
			require(!failed->load() && state->phase()==metal_upload::Phase::Pending,"Merged batch invalidated valid GPU work");
			gate.signaledValue=1;[command waitUntilCompleted];
			require(std::memcmp(output.contents,expected.data(),expected.size())==0,"Merged batch GPU transfer changed");
			require(state->phase()==(fault==Fault::Feedback?metal_upload::Phase::Failed:metal_upload::Phase::Ready),"Merged batch feedback state differs");
		} else {
			require(state->phase()==metal_upload::Phase::Canceled && failed->load(),"Abandoned merged batch remained usable");
			failed->store(false);
		}
		for(auto slot:slots)exactlyOne(slot);
		require(dispatch_group_wait(submissions,DISPATCH_TIME_NOW)==0,"Merged batch left completion group entered");
	}
	if(!enqueued)require(!failed->load(),"Retiring merged command reissued an already consumed cancellation");
	exactlyOne(staging);for(auto slot:slots)exactlyOne(slot);
}
int main(){@autoreleasepool{try {
	auto device=MTLCreateSystemDefaultDevice();require(device && [device supportsFamily:MTLGPUFamilyMetal4],"Metal 4 device required");
	for(bool moved:{false,true})for(auto fault:{Fault::Cancel,Fault::BeforeCommit,Fault::BeforeEnqueue,Fault::AfterEnqueue,Fault::Feedback,Fault::None})
		@autoreleasepool {renderOwner(device,fault,moved);}
	std::puts("PASS actual RenderCommands: 12 retained-command/owner-move fault paths; actual preload image immediately unadoptable on cancel/pre-submit failure, one failure notification through delayed retirement, raster continuation and deferred depth close, no cancellation/double slot release after enqueue, gated exact transfers, completion groups and budget release");
	for(auto fault:{Fault::Cancel,Fault::BeforeCommit,Fault::BeforeEnqueue,Fault::AfterEnqueue,Fault::Feedback,Fault::None})
		@autoreleasepool {mergedOwners(device,fault);}
	std::puts("PASS actual merged RenderCommands: six fault paths, three render-slot leases across repeated deferred-owner moves, slot bound enforced, exactly one cancellation before enqueue, no in-flight reuse and exact GPU transfers");
	adoptionFailure(device);
	for(bool adopted:{false,true})for(auto fault:{Fault::Cancel,Fault::BeforeCommit,Fault::BeforeEnqueue,Fault::AfterEnqueue,Fault::Feedback,Fault::None})
		@autoreleasepool {slotLifetime(device,fault,adopted);}
	drainQueues(device);
	std::puts("PASS native shutdown/slot lifetime: 12 cancellation/submission/feedback paths, failed-adoption caller ownership, exactly-once release and queue reuse; independent cleanup after pre-/post-enqueue failures, gated two-queue GPU drain and exact transfers; synthetic faults only, no full context/window/game test");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}}
