// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <QuartzCore/CADisplayLink.h>
#pragma push_macro("Rect")
#pragma push_macro("RGBColor")
#define Rect MacOSRect
#define RGBColor MacOSRGBColor
#import <AppKit/NSView.h>
#pragma pop_macro("RGBColor")
#pragma pop_macro("Rect")
#include <atomic>
#include <deque>
#include <thread>
#include "metal4_upload_state.h"

// Immutable commands and leased source/UI snapshots cross from the producer
// to this display queue; drawables remain exclusively on the display thread.
struct API_AVAILABLE(macos(26.0)) MetalDisplayState {
	std::mutex mutex;
	std::deque<std::shared_ptr<MetalDisplayImage>> pending;
	std::shared_ptr<MetalDisplayImage> visible;
	std::atomic<bool> active{false};
	std::atomic<bool> failed{false};
	std::atomic<double> schedulingLead{0};
	CAMetalLayer *layer;
	FCMetal4Queue *queue;
	id<MTLRenderPipelineState> pipeline;
	id<MTLSamplerState> sampler;
	dispatch_group_t submissions;
	dispatch_semaphore_t flight = dispatch_semaphore_create(2);
	bool timing = false, repeats = false, needsPresent = false;
	u64 presentations = 0;
	u64 busy = 0, evicted = 0;
	std::weak_ptr<MetalDisplayImage> resizedImage;
	id<MTLTexture> resizedComposition;
};

API_AVAILABLE(macos(26.0))
@interface FCMetalDisplayDelegate : NSObject
- (instancetype)initWithState:(std::shared_ptr<MetalDisplayState>)state;
@end
@implementation FCMetalDisplayDelegate {
	std::shared_ptr<MetalDisplayState> _state;
}
- (instancetype)initWithState:(std::shared_ptr<MetalDisplayState>)state
{
	if ((self = [super init])) _state = std::move(state);
	return self;
}
- (void)displayUpdate:(CADisplayLink *)link
{
	@autoreleasepool {
		auto state = _state;
		if (!state->active) return;
		if(state->failed.exchange(false,std::memory_order_acq_rel)) {
			// A failed GPU composition is not a reusable resize snapshot.
			// Only this display thread changes the cache; feedback sets a flag.
			state->resizedImage.reset();state->resizedComposition=nil;
			std::lock_guard<std::mutex> lock(state->mutex);
			state->needsPresent=bool(state->visible);
		}
		// The immutable image is ready to replay. Follow this view's next display
		// tick rather than acquiring a drawable several refreshes in advance.
		const double lead = link.targetTimestamp - link.timestamp;
		if (std::isfinite(lead) && lead > 0 && lead < 1)
			state->schedulingLead = lead;
		{
			std::lock_guard<std::mutex> lock(state->mutex);
			const bool due = !state->pending.empty() && state->pending.front()->target <= link.targetTimestamp;
			// With repetition disabled, keep an idle callback from acquiring a
			// drawable it cannot present. Do not advance the pending queue here:
			// GPU capacity and a drawable must be reserved first.
			if (!due && (!state->visible || (!state->repeats && !state->needsPresent))) return;
		}
		// Reserve GPU capacity before advancing the visible image. Otherwise a
		// busy output queue consumes game images without ever presenting them.
		if (dispatch_semaphore_wait(state->flight, DISPATCH_TIME_NOW) != 0) {
			++state->busy; return;
		}
		id<CAMetalDrawable> drawable;
		@try { drawable = [state->layer nextDrawable]; }
		@catch (NSException *failure) {
			dispatch_semaphore_signal(state->flight);
			ERROR_LOG(RENDERER, "Metal display drawable: %s", failure.reason.UTF8String);
			return;
		}
		if (!drawable) { dispatch_semaphore_signal(state->flight); return; }
		std::shared_ptr<MetalDisplayImage> image;
		bool changed = false, repeats, needsPresent;
		{
			std::lock_guard<std::mutex> lock(state->mutex);
			// Keep source order when rendering arrives in bursts. Each display
			// update can show one new image; consuming several here drops frames.
			// The native GPU event below orders the producer and copy without
			// waiting for GPU completion on the display or emulation thread.
			if (!state->pending.empty() && state->pending.front()->target <= link.targetTimestamp) {
				state->visible = std::move(state->pending.front());
				state->pending.pop_front(); changed = true;state->needsPresent=true;
			}
			image = state->visible;
			repeats = state->repeats;
			needsPresent=state->needsPresent;
		}
		if (!image || (!changed && !repeats && !needsPresent)) {
			dispatch_semaphore_signal(state->flight); return;
		}
		bool ownsFlight=true;
		try {
			FCMetal4Commands *command = [state->queue commandBuffer];
			if(!command)throw RendererException("Cannot allocate Metal display command buffer");
			metal_upload::adoptSlot(command,state->flight);ownsFlight=false;
			[command waitForEvent:image->readyEvent value:image->readyValue];
			[command useDrawable:drawable];
			id<MTLTexture> texture = image->texture;
			id<MTLTexture> newResizeComposition;
			if (image->recording && (image->width != drawable.texture.width || image->height != drawable.texture.height)) {
				// Match the old nearest resampling of an already composed image
				// during resize. Reuse it on this queue until the visible image changes.
				if (state->resizedImage.lock() != image || !state->resizedComposition) {
					MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
						width:image->width height:image->height mipmapped:NO];
					d.storageMode = MTLStorageModePrivate; d.hazardTrackingMode = MTLHazardTrackingModeUntracked;
					d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
					id<MTLTexture> composed = [state->queue.device newTextureWithDescriptor:d];
					if (!composed) throw RendererException("Cannot allocate Metal resized presentation");
					MTL4RenderPassDescriptor *p = [MTL4RenderPassDescriptor new];
					p.colorAttachments[0].texture = composed; p.colorAttachments[0].loadAction = MTLLoadActionClear;
					p.colorAttachments[0].storeAction = MTLStoreActionStore; p.colorAttachments[0].clearColor = image->clearColor;
					FCMetal4RenderEncoder *e = [command renderCommandEncoderWithDescriptor:p];
					[image->recording encodeTo:e opaque:NO]; [e endEncoding];
					newResizeComposition = composed;
				}
				texture = newResizeComposition ?: state->resizedComposition;
			} else { state->resizedImage.reset(); state->resizedComposition = nil; }
			const bool direct = image->recording && image->width == drawable.texture.width && image->height == drawable.texture.height;
			MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
			pass.colorAttachments[0].texture = drawable.texture;
			pass.colorAttachments[0].loadAction = direct ? MTLLoadActionClear : MTLLoadActionDontCare;
			pass.colorAttachments[0].storeAction = MTLStoreActionStore;
			pass.colorAttachments[0].clearColor = image->clearColor;
			FCMetal4RenderEncoder *encoder = [command renderCommandEncoderWithDescriptor:pass];
			if (direct) [image->recording encodeTo:encoder opaque:YES];
			else {
				struct alignas(16) Uniform { float color[4]; u32 rotate, ignoreAlpha; } u{{1,1,1,1},0,1};
				[encoder setRenderPipelineState:state->pipeline];
				[encoder setCullMode:MTLCullModeNone];
				[encoder setViewport:MTLViewport{0,0,double(drawable.texture.width),double(drawable.texture.height),0,1}];
				[encoder setSharedBytes:&u length:sizeof(u) atIndex:0];
				[encoder setFragmentTexture:texture atIndex:0];
				[encoder setFragmentSamplerState:state->sampler atIndex:0];
				[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			}
			[encoder endEncoding];
			[command addCompletedHandler:^(FCMetal4Commands *finished) {
				// Keep the image's pool lease until this GPU read completes.
				(void)image;
				if (finished.status == FCMetal4StatusError) {
					state->failed.store(true,std::memory_order_release);
					ERROR_LOG(RENDERER, "Metal display: %s", finished.error.localizedDescription.UTF8String);
				}
			}];
			if (state->timing && state->presentations < 18000) {
				const u64 number = ++state->presentations, content = image->content;
				if (changed)
					NOTICE_LOG(RENDERER, "Metal display submit %llu: now %.6f deadline %.6f target %.6f image %.6f ready %d content %llu",
						(unsigned long long)number, CACurrentMediaTime(), link.targetTimestamp,
						link.targetTimestamp, image->target,
						image->readyEvent.signaledValue >= image->readyValue, (unsigned long long)content);
				[drawable addPresentedHandler:^(id<MTLDrawable> drawable) {
					NOTICE_LOG(RENDERER, "Metal display presented %llu: %.6f s content %llu", (unsigned long long)number, drawable.presentedTime, (unsigned long long)content);
				}];
			}
			// Submit promptly for the next display tick; never delay the producer.
			[command presentDrawable:drawable];
			commitCommands(command, state->submissions);
			// A failed encoding/submission must not publish an unrendered cache.
			if (newResizeComposition) {
				state->resizedComposition = newResizeComposition; state->resizedImage = image;
			}
			{std::lock_guard<std::mutex> lock(state->mutex);state->needsPresent=false;}
		} catch (const std::exception& failure) {
			if(ownsFlight)dispatch_semaphore_signal(state->flight);
			ERROR_LOG(RENDERER, "Metal display failed: %s", failure.what());
		}
	}
}
@end

class API_AVAILABLE(macos(26.0)) MetalDisplayLink {
	std::shared_ptr<MetalDisplayState> state = std::make_shared<MetalDisplayState>();
	std::thread thread;
	CFRunLoopRef runLoop = nullptr;
	CADisplayLink *link;
public:
	MetalDisplayLink(NSView *view, CAMetalLayer *layer, FCMetal4Queue *queue, id<MTLRenderPipelineState> pipeline,
		id<MTLSamplerState> sampler, dispatch_group_t submissions, bool timing, double refresh) {
		// Drawable waits must not stall the game's native Metal 4 queue.
		state->queue = [[FCMetal4Queue alloc] initWithDevice:queue.device]; state->pipeline = pipeline; state->sampler = sampler;
		state->submissions = submissions; state->timing = timing; state->layer = layer;
		dispatch_semaphore_t ready = dispatch_semaphore_create(0);
		// Create the view-bound clock on the SDL/AppKit thread. Only its
		// callbacks and drawable acquisition run on the display thread.
		FCMetalDisplayDelegate *delegate = [[FCMetalDisplayDelegate alloc] initWithState:state];
		link = [view displayLinkWithTarget:delegate selector:@selector(displayUpdate:)];
		link.paused = YES;
		if (std::isfinite(refresh) && refresh > 0)
			link.preferredFrameRateRange = CAFrameRateRangeMake(refresh, refresh, refresh);
		thread = std::thread([this, ready] {
			@autoreleasepool {
				pthread_setname_np("Flycast Metal display");
				runLoop = CFRunLoopGetCurrent();
				// A paused display link need not keep its run loop alive.
				CFRunLoopSourceContext context{};
				CFRunLoopSourceRef keepAlive = CFRunLoopSourceCreate(nullptr, 0, &context);
				CFRunLoopAddSource(runLoop, keepAlive, kCFRunLoopCommonModes);
				[link addToRunLoop:NSRunLoop.currentRunLoop forMode:NSRunLoopCommonModes];
				dispatch_semaphore_signal(ready);
				CFRunLoopRun();
				[link invalidate]; link = nil;
				CFRunLoopRemoveSource(runLoop, keepAlive, kCFRunLoopCommonModes);
				CFRelease(keepAlive);
			}
		});
		dispatch_semaphore_wait(ready, DISPATCH_TIME_FOREVER);
	}
	~MetalDisplayLink() {
		setActive(false, false);
		CFRunLoopPerformBlock(runLoop, kCFRunLoopCommonModes, ^{ CFRunLoopStop(runLoop); });
		CFRunLoopWakeUp(runLoop); thread.join();
		if (state->timing)
			NOTICE_LOG(RENDERER, "Metal display queue: busy %llu evicted %llu",
				(unsigned long long)state->busy, (unsigned long long)state->evicted);
	}
	void setActive(bool active, bool repeats) {
		if (state->active == active) {
			std::lock_guard<std::mutex> lock(state->mutex); state->repeats = repeats; return;
		}
		state->active = false;
		dispatch_semaphore_t done = dispatch_semaphore_create(0);
		CFRunLoopPerformBlock(runLoop, kCFRunLoopCommonModes, ^{
			link.paused = !active;
			dispatch_semaphore_signal(done);
		});
		CFRunLoopWakeUp(runLoop); dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
		{
			std::lock_guard<std::mutex> lock(state->mutex);
			state->pending.clear(); state->visible.reset(); state->repeats = repeats;state->needsPresent=false;
		}
		state->active = active;
	}
	bool active() const { return state->active; }
	double schedulingLead() const { return state->schedulingLead.load(); }
	void prepare(std::shared_ptr<MetalDisplayImage> image, FCMetal4Commands *command) {
		[command signalEvent:image->readyEvent value:image->readyValue];
		[command addCompletedHandler:^(FCMetal4Commands *) { (void)image; }];
	}
	void enqueue(std::shared_ptr<MetalDisplayImage> image) {
		// Publish after CPU submission. DisplayLink reads GPU readiness directly;
		// delayed CPU feedback must not become an extra frame of latency.
		std::lock_guard<std::mutex> lock(state->mutex);
		if (!state->active) return;
		if (state->pending.size() == 3) { state->pending.pop_front(); ++state->evicted; }
		state->pending.push_back(image);
	}
};
