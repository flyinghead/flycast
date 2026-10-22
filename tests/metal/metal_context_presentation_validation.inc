// SPDX-License-Identifier: GPL-2.0-or-later
// Actual Context selection/submission/cancellation, with no NSView or display ticks.
void MetalContext::validateDeferredPresentationForTesting()
{
	if (impl->view) throw RendererException("Deferred presentation fixture requires offscreen context");
	const bool oldVsync=config::VSync, oldPacing=config::FramePacing, oldFX=config::MetalFXSpatial;
	const auto oldFrame=impl->lastFrame; const float oldAspect=impl->aspectRatio;
	const auto oldSize=impl->layer.drawableSize;
	struct Restore {
		MetalContext *self; bool vsync,pacing,fx; id<MTLTexture> frame; float aspect; CGSize size;
		~Restore(){try{self->waitIdle();}catch(...){} config::VSync=vsync;config::FramePacing=pacing;config::MetalFXSpatial=fx;
			self->setLastFrame(frame,aspect);self->impl->layer.drawableSize=size;}
	} restore{this,oldVsync,oldPacing,oldFX,oldFrame,oldAspect,oldSize};
	auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	config::VSync=true;config::MetalFXSpatial=false;
	unsigned pairs=0,cancellations=0;
	for (unsigned iteration=0;iteration<20;++iteration) {
		dispatch_semaphore_t available;
		@autoreleasepool {
		const unsigned mode=iteration%4;
		setOffscreenSizeForTesting(iteration&4?80:64,iteration&4?60:48);
		auto input=makeTexture(16,12,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead,false,true);
		std::vector<u8> pixels(16*12*4);
		for(size_t i=0;i<pixels.size();i+=4){pixels[i]=u8(i+iteration);pixels[i+1]=u8(i/4);pixels[i+2]=u8(iteration*17);pixels[i+3]=255;}
		[input replaceRegion:MTLRegionMake2D(0,0,16,12) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:16*4];
		setLastFrame(input,4.f/3.f);
		config::FramePacing=false;beginPresentation();auto reference=impl->drawable.texture;present();waitIdle();
		std::vector<u8> expected;readTexture(reference,expected);
		config::FramePacing=true;beginPresentation();
		auto packet=impl->composition;const size_t slot=impl->compositionSlot;
		FCMetal4Commands *producer=impl->frame;
		available=impl->compositions[slot].available;
		check(packet && recordingPresentation() && packet->texture==input,"Context did not defer the source snapshot");
		if(mode&1)requirePresentationComposition();
		if(mode>=2) {
			waitIdle();check(!impl->composition && !impl->frame && !impl->encoder,"Canceled recording retained active context state");++cancellations;
		} else {
			present();waitIdle();
			check((packet->recording!=nil)==(mode==0),"Context selected wrong recorded/composed path");
			check(packet->texture!=input,"Mutable source escaped to display packet");
			if(mode==1)check(!impl->compositions[slot].texture,"Late fallback allocated/retained a source snapshot");
			// Producer source reuse must not change either queued packet type.
			std::fill(pixels.begin(),pixels.end(),u8(229));
			[input replaceRegion:MTLRegionMake2D(0,0,16,12) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:16*4];
			auto output=packet->texture;
			if(packet->recording) {
				output=makeTexture(packet->width,packet->height,MTLPixelFormatBGRA8Unorm,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead);
				auto command=[queue() commandBuffer];auto pass=[MTL4RenderPassDescriptor new];
				pass.colorAttachments[0].texture=output;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
				pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=packet->clearColor;
				auto encoder=[command renderCommandEncoderWithDescriptor:pass];[packet->recording encodeTo:encoder opaque:YES];[encoder endEncoding];submitCommandBuffer(command);
			}
			std::vector<u8> actual;readTexture(output,actual);check(actual==expected,"Context deferred presentation changed complete image");++pairs;
		}
		[producer waitUntilCompleted];waitIdle();packet.reset();
		} // Drain producer autoreleases before asserting final lease ownership.
		check(dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)==0,"Context leaked composition lease");
		const bool duplicate=dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)==0;
		dispatch_semaphore_signal(available);check(!duplicate,"Context returned composition lease twice");
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual Context deferred presentation: %u complete image pairs, %u unpublished/direct-redirect cancellations, source overwrite, resize, seven-slot reuse and exact lease release; no view or display tick",pairs,cancellations);
}
