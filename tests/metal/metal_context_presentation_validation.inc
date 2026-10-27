// SPDX-License-Identifier: GPL-2.0-or-later
// Actual Context selection/submission/cancellation, with no NSView or display ticks.
#include "metal_offscreen_test_hooks.h"
#include "input/mouse.h"
void MetalContext::validateDeferredPresentationForTesting()
{
	validateFixedUIOverlaysForTesting();
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
	// The RGB-only variant is needed by recorded alpha overlays, not the
	// initial opaque frame. It must also be ready before a first presentation.
	for(u64 key:{0u,1u,2u})check(impl->quads.count(key) && impl->quads.at(key),
		"Context initialization left a fixed quad variant for first presentation");
	const auto before=validationRenderPipelineRequests;
	for(unsigned i=0;i<32;++i)for(u64 key:{0u,1u,2u})quadPipelineState((key&1)!=0,(key&2)!=0);
	check(validationRenderPipelineRequests==before,"Ready quad requested foreground archive/compiler work");
	impl->quads.erase(2);
	check(quadPipelineState(false,true) && validationRenderPipelineRequests==before+1,
		"Same-descriptor lazy quad control missed its single foreground request");
	{
		auto input=makeTexture(8,6,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead,false,true);
		std::vector<u8> source(8*6*4);
		const u8 alpha[]={0,17,73,128,191,255};
		for(unsigned y=0;y<6;++y)for(unsigned x=0;x<8;++x){
			const size_t offset=(y*8+x)*4;
			source[offset]=u8(19+x*23);source[offset+1]=u8(31+y*29);source[offset+2]=u8(47+x*11+y*7);source[offset+3]=alpha[y];
		}
		[input replaceRegion:MTLRegionMake2D(0,0,8,6) mipmapLevel:0 withBytes:source.data() bytesPerRow:32];
		for(u64 key:{0u,1u,2u}) {
			auto output=makeTexture(8,6,MTLPixelFormatBGRA8Unorm);
			auto command=[queue() commandBuffer];auto pass=[MTL4RenderPassDescriptor new];
			pass.colorAttachments[0].texture=output;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
			pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
			auto encoder=[command renderCommandEncoderWithDescriptor:pass];
			struct alignas(16) Uniform {float color[4];u32 rotate,ignoreAlpha;} uniform{{1,1,1,1},0,u32(key&1)};
			[encoder setRenderPipelineState:quadPipelineState((key&1)!=0,(key&2)!=0)];
			[encoder setCullMode:MTLCullModeNone];[encoder setViewport:MTLViewport{0,0,8,6,0,1}];
			[encoder setSharedBytes:&uniform length:sizeof(uniform) atIndex:0];[encoder setFragmentTexture:input atIndex:0];
			[encoder setFragmentSamplerState:impl->nearest atIndex:0];[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			[encoder endEncoding];submitCommandBuffer(command);std::vector<u8> pixels;readTexture(output,pixels);
			check(pixels.size()==source.size(),"Quad image dimensions differ from CPU reference");
			for(size_t i=0;i<source.size();i+=4)for(unsigned c=0;c<4;++c){
				const unsigned a=source[i+3],channel=c==0?2:c==2?0:c;
				const unsigned expected=c<3 ? (key==1?source[i+channel]:(unsigned(source[i+channel])*a+127)/255)
					: key==0 ? (a*a+255*(255-a)+127)/255 : 255;
				check(std::abs(int(pixels[i+c])-int(expected))<=1,"Fixed quad blend/opaque-alpha/RGBA output differs from CPU reference");
			}
		}
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: three quad variants ready at Context init, 96 cached requests with no foreground creation, same-descriptor lazy RGB-only control makes one request, three complete CPU alpha/blend/channel image references; initialization work moved earlier, no total latency/FPS claim");
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

void MetalContext::validateDuplicateCadenceForTesting()
{
	if(impl->view)throw RendererException("Duplicate cadence fixture requires an offscreen context");
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const bool oldSync=config::VSync,oldPace=config::FramePacing,oldDupe=config::DupeFrames,oldFX=config::MetalFXSpatial;
	const auto oldFrame=impl->lastFrame;const auto oldSize=impl->layer.drawableSize;
	const float oldAspect=impl->aspectRatio;const double oldRefresh=settings.display.refreshRate,oldField=metalOffscreenVideoFieldDuration;
	const int oldInterval=impl->swapInterval;
	struct Restore {MetalContext *self;bool sync,pace,dupe,fx;id<MTLTexture> image;CGSize size;float aspect;double refresh,field;int interval;metal::DuplicateCadence cadence;
		~Restore(){self->waitIdle();self->impl->testPresentationObserver={};config::VSync=sync;config::FramePacing=pace;config::DupeFrames=dupe;config::MetalFXSpatial=fx;
			self->setLastFrame(image,aspect);self->impl->layer.drawableSize=size;settings.display.refreshRate=refresh;metalOffscreenVideoFieldDuration=field;self->setSwapInterval(interval);self->impl->duplicateCadence=cadence;}
	} restore{this,oldSync,oldPace,oldDupe,oldFX,oldFrame,oldSize,oldAspect,oldRefresh,oldField,oldInterval,impl->duplicateCadence};
	waitIdle();config::FramePacing=false;config::VSync=true;config::MetalFXSpatial=false;
	setOffscreenSizeForTesting(64,48);
	struct Case{u32 fieldNumerator,fieldDenominator,display;};
	const Case cases[]={{50,1,100},{50,1,120},{50,1,60},{60000,1001,120},{60000,1001,60},{60,1,90},{60,1,144},{60,1,120}};
	unsigned frames=0,outputs=0;
	for(const auto test:cases)for(int mode:{1,2,3}) {
		settings.display.refreshRate=test.display;metalOffscreenVideoFieldDuration=double(test.fieldDenominator)/test.fieldNumerator;
		config::DupeFrames=false;config::VSync=false;setSwapInterval(1);setLastFrame(nil,4.f/3.f);present();waitIdle();config::DupeFrames=true;config::VSync=true;
		uint64_t numerator=0,observed=0;
		for(unsigned n=0;n<30;++n){ @autoreleasepool {
			const int interval=mode==3?1+n%2:mode;setSwapInterval(interval);
			const u8 rgba[]{u8(19+n),u8(53+n*3),u8(87+n*5),255};
			auto image=makeTexture(1,1,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead,false,true);
			[image replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:rgba bytesPerRow:4];
			std::vector<id<MTLTexture>> drawn;
			impl->testPresentationObserver=[&drawn](id<MTLDrawable> drawable){drawn.push_back(((id<CAMetalDrawable>)drawable).texture);};
			setLastFrame(image,4.f/3.f);if(n%2)drawOverlays();present();impl->testPresentationObserver={};waitIdle();
			numerator+=uint64_t(test.display)*interval*test.fieldDenominator;
			observed+=drawn.size();check(observed==numerator/test.fieldNumerator,"Direct presentation lost PAL/NTSC fractional duplicate cadence");
			std::vector<u8> golden(64*48*4);for(size_t i=0;i<golden.size();i+=4){golden[i]=rgba[2];golden[i+1]=rgba[1];golden[i+2]=rgba[0];golden[i+3]=255;}
			for(auto texture:drawn){std::vector<u8> actual;readTexture(texture,actual);check(actual==golden,"Duplicate presentation changed a full source image");}
			outputs+=drawn.size();++frames;
		}}
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: direct duplicate cadence, %u source frames and %u complete images; PAL/NTSC, 60/90/100/120/144-Hz targets, interval 1/2 and alternating intervals, explicit/lazy presentation after VSync changes; no view or physical display timestamps",frames,outputs);
}

void MetalContext::validateLazyPresentationForTesting()
{
	if(impl->view)throw RendererException("Lazy presentation fixture requires an offscreen context");
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const bool oldSync=config::VSync,oldPace=config::FramePacing,oldDupe=config::DupeFrames,oldFX=config::MetalFXSpatial;
	const auto oldFrame=impl->lastFrame;const float oldAspect=impl->aspectRatio;
	struct Restore{MetalContext *self;bool sync,pace,dupe,fx;id<MTLTexture> frame;float aspect;double refresh;
		~Restore(){metalOffscreenClearCommandHook();self->waitIdle();config::VSync=sync;config::FramePacing=pace;config::DupeFrames=dupe;config::MetalFXSpatial=fx;settings.display.refreshRate=refresh;self->setLastFrame(frame,aspect);}
	} restore{this,oldSync,oldPace,oldDupe,oldFX,oldFrame,oldAspect,settings.display.refreshRate};
	waitIdle();config::VSync=true;config::FramePacing=true;config::DupeFrames=false;config::MetalFXSpatial=false;
	auto source=makeTexture(1,1,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead,false,true);
	std::vector<std::shared_ptr<MetalDisplayImage>> packets;
	for(unsigned n=0;n<2;++n){ @autoreleasepool {
		const u8 rgba[]{u8(19+n*31),u8(53+n*17),u8(87+n*23),255};
		[source replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:rgba bytesPerRow:4];
		auto gate=[device() newSharedEvent];
		struct Release{id<MTLSharedEvent> gate;~Release(){gate.signaledValue=1;}} release{gate};
		metalOffscreenArmCommand(MetalOffscreenFault::None,gate);
		setLastFrame(source,4.f/3.f);present(); // Deliberately no beginPresentation/drawOverlays.
		auto producer=metalOffscreenLastCommand();metalOffscreenClearCommandHook();
		check(!impl->composition && !impl->frame && !impl->encoder,"Lazy paced present retained an unpublished composition");
		check(impl->displayLink && impl->displayLink->active(),"Lazy present failed to activate the display queue");
		auto packet=impl->displayLink->pendingForTesting();
		check(packet && producer && packet->recording.sealed && packet->texture!=source,"Lazy present did not seal and enqueue its snapshot");
		check(packet->readyEvent.signaledValue<packet->readyValue,"Gated source became ready before its producer");
		if(!packets.empty())check(packet!=packets.back() && packet->content!=packets.back()->content,"New source was lost behind the active display clock");
		present();
		check(impl->displayLink->pendingForTesting()==packet && !impl->frame && !impl->composition,"Idle present enqueued an unsolicited duplicate");
		config::DupeFrames=true;present();
		check(impl->displayLink->repeatsForTesting() && impl->displayLink->pendingForTesting()==packet,"Idle repeat toggle did not reach the display clock");
		config::DupeFrames=false;present();
		check(!impl->displayLink->repeatsForTesting() && impl->displayLink->pendingForTesting()==packet,"Disabling repeats changed the pending image");
		gate.signaledValue=1;[producer waitUntilCompleted];
		// Queue signals follow the producer command. A later readback on the
		// same queue proves that signal has executed, not merely been enqueued.
		std::vector<u8> snapshot;readTexture(packet->texture,snapshot);
		check(snapshot==std::vector<u8>(rgba,rgba+4),"Lazy source snapshot differs from producer pixels");
		check(producer.status==FCMetal4StatusCompleted && packet->readyEvent.signaledValue>=packet->readyValue,"Lazy present did not publish its producer-ready event");
		packets.push_back(packet);
	}}
	// Both mutable-source writes are complete. Replay the two frozen packets
	// afterwards, so the older one must still contain its own source pixels.
	for(unsigned n=0;n<packets.size();++n){ @autoreleasepool {
		const auto packet=packets[n];auto output=makeTexture(64,48,MTLPixelFormatBGRA8Unorm);
		auto command=[queue() commandBuffer];[command waitForEvent:packet->readyEvent value:packet->readyValue];
		auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=output;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=packet->clearColor;
		auto encoder=[command renderCommandEncoderWithDescriptor:pass];[packet->recording encodeTo:encoder opaque:YES];[encoder endEncoding];submitCommandBuffer(command);
		std::vector<u8> actual;readTexture(output,actual);std::vector<u8> golden(64*48*4);
		for(size_t i=0;i<golden.size();i+=4){golden[i]=u8(87+n*23);golden[i+1]=u8(53+n*17);golden[i+2]=u8(19+n*31);golden[i+3]=255;}
		check(actual==golden,"Lazy presentation lost its immutable full source image");
	}}
	config::FramePacing=false;present();
	check(!impl->displayLink && !impl->frame && !impl->composition,"Lazy pacing disable ignored an unchanged source");
	config::FramePacing=true;present();
	check(impl->displayLink && impl->displayLink->pendingForTesting(),"Lazy pacing enable lost an unchanged source");
	const auto previous=impl->displayLink->pendingForTesting();
	settings.display.refreshRate=settings.display.refreshRate==120?60:120;present();
	check(impl->displayRefresh==settings.display.refreshRate && impl->displayLink->pendingForTesting()!=previous,"Lazy target-refresh change retained the old display clock");
	std::vector<u8> refreshed;readTexture(impl->displayLink->pendingForTesting()->texture,refreshed);
	check(refreshed==std::vector<u8>({50,70,110,255}),"Lazy mode/refresh changes lost the retained source pixels");
	setLastFrame(nil,4.f/3.f);present();
	check(!impl->displayLink && !impl->frame && !impl->composition,"Lazy source clear retained the old paced image");
	NOTICE_LOG(RENDERER,"Metal validation PASS: lazy paced Context present, first/subsequent sources, GPU-gated ready events, no idle packets, repeat and pacing toggles with unchanged source, target-refresh changes, two exact frozen full images after overwrite, retained pixels and explicit clear; no view or display ticks");
}


void MetalContext::validateFixedUIOverlaysForTesting()
{
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	check(!impl->view,"Fixed UI/overlay fixture requires an offscreen context");
	auto driver=dynamic_cast<MetalDriver *>(imguiDriver.get());
	check(driver && driver->fixedPipelinesReadyForTesting() && impl->crosshair
		&& impl->crosshairPipeline && impl->opaqueCrosshairPipeline,"Fixed UI/crosshair resources were not ready at initialization");
	const auto oldDisplay=settings.display;
	const auto oldPlatform=settings.platform;const auto oldInput=settings.input;
	const auto oldFrame=impl->lastFrame;const auto oldAspect=impl->aspectRatio;const auto oldSize=impl->layer.drawableSize;
	const auto oldGui=gui_state;
	const bool oldSync=config::VSync,oldPacing=config::FramePacing,oldFX=config::MetalFXSpatial,oldDupe=config::DupeFrames;
	const bool oldFloat=config::FloatVMUs,oldRotate=config::Rotate90;
	const int oldCrosshairSize=config::CrosshairSize,oldStretch=config::ScreenStretching;
	std::array<u32,4> colors;std::array<MapleDeviceType,4> devices;std::array<s32,4> mx,my;
	for(unsigned i=0;i<4;++i){colors[i]=config::CrosshairColor[i];devices[i]=config::MapleMainDevices[i];mx[i]=mo_x_abs[i];my[i]=mo_y_abs[i];}
	struct Restore {std::function<void()> restore;~Restore(){try{restore();}catch(...){}}} restore{[&]{
		waitIdle();imguiDriver->deleteTexture("__metal_fixed_ui");
		for(unsigned i=0;i<4;++i){config::CrosshairColor[i]=colors[i];config::MapleMainDevices[i]=devices[i];mo_x_abs[i]=mx[i];mo_y_abs[i]=my[i];}
		settings.display=oldDisplay;settings.platform=oldPlatform;settings.input=oldInput;gui_state=oldGui;
		config::VSync=oldSync;config::FramePacing=oldPacing;config::MetalFXSpatial=oldFX;config::DupeFrames=oldDupe;
		config::FloatVMUs=oldFloat;config::Rotate90=oldRotate;config::CrosshairSize=oldCrosshairSize;config::ScreenStretching=oldStretch;
		setLastFrame(oldFrame,oldAspect);impl->layer.drawableSize=oldSize;
	}};
	setOffscreenSizeForTesting(64,48);settings.display.width=64;settings.display.height=48;settings.display.uiScale=1;
	settings.platform.system=DC_PLATFORM_DREAMCAST;settings.input.fastForwardMode=false;gui_state=GuiState::Closed;
	config::VSync=true;config::FramePacing=false;config::MetalFXSpatial=false;config::DupeFrames=false;
	config::FloatVMUs=false;config::Rotate90=false;config::CrosshairSize=16;config::ScreenStretching=100;
	for(unsigned i=0;i<4;++i)config::CrosshairColor[i]=0;
	config::MapleMainDevices[0]=MDT_LightGun;mo_x_abs[0]=320;mo_y_abs[0]=240;
	std::vector<u8> mask;readTexture(impl->crosshair,mask);
	check(mask.size()==16*16*4 && std::memcmp(mask.data(),getCrosshairTextureData(),mask.size())==0,
		"Prepared immutable crosshair differs from its independent source bitmap");
	const auto beforeControl=validationRenderPipelineRequests;
	impl->crosshair=nil;impl->crosshairPipeline=nil;impl->opaqueCrosshairPipeline=nil;prepareCrosshair();
	check(validationRenderPipelineRequests==beforeControl+2,"Same-constructor lazy crosshair control missed its two requests");
	u32 white=~0u;auto texture=imguiDriver->updateTexture("__metal_fixed_ui",reinterpret_cast<u8 *>(&white),1,1,true);
	ImDrawList list(ImGui::GetDrawListSharedData());list.VtxBuffer.resize(4);list.IdxBuffer.resize(6);
	for(unsigned i=0;i<4;++i)list.VtxBuffer[i]={ImVec2(i&1?56.f:8.f,i&2?40.f:8.f),ImVec2(.5f,.5f),IM_COL32(255,0,0,128)};
	const ImDrawIdx ids[]={0,1,2,2,1,3};std::copy_n(ids,6,list.IdxBuffer.Data);
	list._VtxWritePtr=list.VtxBuffer.Data+4;list._IdxWritePtr=list.IdxBuffer.Data+6;list._VtxCurrentIdx=4;
	ImDrawCmd draw;draw.ElemCount=6;draw.ClipRect=ImVec4(8,8,56,40);draw.TexRef=ImTextureRef(texture);list.CmdBuffer.push_back(draw);
	ImDrawData data;data.Valid=true;data.DisplaySize=ImVec2(64,48);data.FramebufferScale=ImVec2(1,1);data.AddDrawList(&list);
	auto background=makeTexture(1,1,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead,false,true);const u32 black=0xff000000u;
	[background replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:&black bytesPerRow:4];
	const auto readyRequests=validationRenderPipelineRequests;unsigned images=0;
	for(bool ui:{false,true})for(bool paced:{false,true}) { @autoreleasepool {
		config::CrosshairColor[0]=ui?0:0x800000ffu;config::FramePacing=paced;setLastFrame(background,4.f/3.f);
		if(ui){imguiDriver->setFrameRendered();imguiDriver->renderDrawData(&data,false);}
		else drawOverlays();
		auto packet=impl->composition;auto output=impl->drawable.texture;
		check(!paced || (packet && recordingPresentation()),"UI/overlay fixture missed actual recorded presentation");
		if(ui)imguiDriver->present();else present();waitIdle();
		if(packet){
			check(packet->recording,"Owned UI/overlay unexpectedly forced composition");
			output=makeTexture(packet->width,packet->height,MTLPixelFormatBGRA8Unorm);
			auto command=[queue() commandBuffer];auto pass=[MTL4RenderPassDescriptor new];
			pass.colorAttachments[0].texture=output;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
			pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=packet->clearColor;
			auto encoder=[command renderCommandEncoderWithDescriptor:pass];[packet->recording encodeTo:encoder opaque:YES];[encoder endEncoding];submitCommandBuffer(command);
		}
		std::vector<u8> pixels;readTexture(output,pixels);check(pixels.size()==64*48*4,"UI/overlay image dimensions differ");
		for(unsigned y=0;y<48;++y)for(unsigned x=0;x<64;++x){
			const bool covered=ui ? x>=8 && x<56 && y>=8 && y<40
				: x>=24 && x<40 && y>=16 && y<32 && getCrosshairTextureData()[(y-16)*16+x-24]!=0;
			const unsigned expectedAlpha=!ui && !paced && covered?191:255;
			const unsigned expected[]={0,0,covered?128u:0u,expectedAlpha};
			for(unsigned c=0;c<4;++c)check(std::abs(int(pixels[(y*64+x)*4+c])-int(expected[c]))<=1,
				"Actual UI/overlay direct or recorded pixels differ from independent CPU blend/shape reference");
		}
		++images;
	}}
	check(validationRenderPipelineRequests==readyRequests,"First UI/overlay draws reached a foreground pipeline constructor");
	NOTICE_LOG(RENDERER,"Metal validation PASS: fixed UI RGB and two crosshair PSOs ready at initialization; actual UI indexed draws and instanced lightgun draws in direct/recorded presentation, %u full CPU alpha/shape/channel images, no draw-side foreground pipeline requests; lazy crosshair control two requests; immutable bitmap exact (no physical display/FPS claim)",images);
}
