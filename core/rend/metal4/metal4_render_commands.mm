// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(USE_METAL) || defined(FLYCAST_METAL_STANDALONE)
#include "metal4_context.h"
#include "metal4_submission.h"
#include "hw/pvr/Renderer_if.h"
#include "log/Log.h"

MetalContext::RenderCommands::RenderCommands(FCMetal4Commands * command, dispatch_group_t submissions,
	std::function<void()> canceledBatch,std::shared_ptr<metal_upload::State> uploadState,
	metal_upload::FailureFlag uploadFailure) noexcept
	: command(command), submissions(submissions), canceledBatch(std::move(canceledBatch)),
	  uploadState(std::move(uploadState)),uploadFailure(std::move(uploadFailure)) {}
MetalContext::RenderCommands::RenderCommands(RenderCommands&& other) noexcept
	: command(other.command), submissions(other.submissions), rasterEncoder(other.rasterEncoder),
	  rasterColor(other.rasterColor), rasterDepth(other.rasterDepth), deferredDepthStore(other.deferredDepthStore),
	  canceledBatch(std::move(other.canceledBatch)),uploadState(std::move(other.uploadState)),
	  uploadFailure(std::move(other.uploadFailure)), renderAvailable(std::move(other.renderAvailable)),
	  renderCount(other.renderCount), timingSubmission(other.timingSubmission), submitted(other.submitted)
{
	other.command = nil;
	other.submissions = nil;
	other.rasterEncoder = nil;
	other.rasterColor = other.rasterDepth = nil;
	other.deferredDepthStore = false;
}
MetalContext::RenderCommands::~RenderCommands()
{
	endRasterEncoder();
	if (!submitted && command) {
		// No GPU work was submitted, so completion cannot release staging or
		// render slots. Both uploaded and pending screen data need rebuilding.
		metal_upload::cancelPending(uploadState,uploadFailure);
		command = nil;
		for (size_t i = 0; i < renderCount; ++i) dispatch_semaphore_signal(renderAvailable[i]);
		if (canceledBatch) canceledBatch();
	}
}
void MetalContext::RenderCommands::retainRenderSlot(dispatch_semaphore_t available)
{
	if (renderCount == renderAvailable.size()) throw RendererException("Metal render batch slot limit exceeded");
	renderAvailable[renderCount++] = available;
}
FCMetal4RenderEncoder * MetalContext::RenderCommands::continuingRasterEncoder(id<MTLTexture> color, id<MTLTexture> depth)
{
	if (rasterColor != color || rasterDepth != depth) endRasterEncoder();
	return rasterEncoder;
}
void MetalContext::RenderCommands::retainRasterEncoder(FCMetal4RenderEncoder * encoder,
	id<MTLTexture> color, id<MTLTexture> depth, bool deferredDepthStore)
{
	if (rasterEncoder) throw RendererException("Metal raster encoder already pending");
	rasterEncoder = encoder; rasterColor = color; rasterDepth = depth;
	this->deferredDepthStore = deferredDepthStore;
}
void MetalContext::RenderCommands::endRasterEncoder(bool preserveDepth)
{
	if (rasterEncoder) {
		if (deferredDepthStore) {
			const MTLStoreAction store = preserveDepth ? MTLStoreActionStore : MTLStoreActionDontCare;
			[rasterEncoder setDepthStoreAction:store];
			[rasterEncoder setStencilStoreAction:store];
		}
		[rasterEncoder endEncoding];
	}
	rasterEncoder = nil; rasterColor = rasterDepth = nil;
	deferredDepthStore = false;
}
void MetalContext::RenderCommands::commit(bool discardRasterDepth)
{
	try {
		endRasterEncoder(!discardRasterDepth);
		if (timingSubmission != 0) {
			// GPU timestamps describe this whole submission, including any Process
			// uploads and continued screen renders, rather than an individual render.
			const u64 number = timingSubmission;
			const size_t renders = renderCount;
			[command addCompletedHandler:^(FCMetal4Commands * done) {
				if (done.status == FCMetal4StatusCompleted && done.GPUStartTime > 0
						&& done.GPUEndTime >= done.GPUStartTime)
					NOTICE_LOG(RENDERER, "Metal timing submission %llu renders %zu GPU: %.3f ms",
						(unsigned long long)number, renders, (done.GPUEndTime - done.GPUStartTime) * 1000);
			}];
		}
		commitCommands(command, submissions); submitted=true;
	} catch(...) {
		if(command.status>=FCMetal4StatusCommitted)submitted=true;
		else metal_upload::cancelPending(uploadState,uploadFailure);
		throw;
	}
}
#endif
