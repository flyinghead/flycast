// SPDX-License-Identifier: GPL-2.0-or-later
// Actual Context/Process/Render recovery; the raw parser alone supplies parsed
// synthetic geometry. Native submissions are valid; failures are wrapper hooks.
void MetalRenderer::validateSyntheticRecovery()
{
	using Fault=MetalOffscreenFault;using Color=std::array<u8,4>;
	const Color replacement{191,47,131,96},demandColor{37,83,149,211},oldColor{11,23,41,255};
	const auto oldGame=settings.content.gameId;Renderer *oldRenderer=renderer;
	const bool oldCustom=config::CustomTextures,oldFog=config::Fog,oldNative=config::NativeDepthInterpolation;
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen;
	const int oldPreload=config::PreloadCustomTextures,oldFilter=config::TextureFiltering;
	const u32 oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	struct Restore {
		std::string game;Renderer *previous;bool custom,fog,native,emulate,wide;int preload,filter;u32 read,spg;
		~Restore(){metalOffscreenClearCommandHook();metalOffscreenParsedTa={};custom_texture.terminate();renderer=previous;settings.content.gameId=game;config::CustomTextures=custom;config::Fog=fog;config::NativeDepthInterpolation=native;config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::PreloadCustomTextures=preload;config::TextureFiltering=filter;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore{oldGame,oldRenderer,oldCustom,oldFog,oldNative,oldEmulate,oldWide,oldPreload,oldFilter,oldRead,oldSpg};
	const auto require=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const auto wait=[&](auto predicate,const char *message){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(!predicate()){if(std::chrono::steady_clock::now()>end)throw RendererException(message);std::this_thread::yield();}};
	const auto exactlyOne=[&](dispatch_semaphore_t available) {
		const bool first=dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)==0;
		const bool second=dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)==0;
		if(second)dispatch_semaphore_signal(available);if(first)dispatch_semaphore_signal(available);
		require(first && !second,"Context recovery leaked or double-released a staging/render slot");
	};
	const auto pixels=[&](id<MTLTexture> image,Color color) {
		std::vector<u8> data;context()->readTexture(image,data);require(data.size()==image.width*image.height*4,"Recovery image dimensions differ");
		for(size_t i=0;i<data.size();++i)require(data[i]==color[i%4],"Recovery image content differs");
	};
	const auto tables=[&] {
		context()->flushTextureUpdates();auto fogBytes=[context()->device() newBufferWithLength:256 options:MTLResourceStorageModeShared];
		auto paletteBytes=[context()->device() newBufferWithLength:4096 options:MTLResourceStorageModeShared];
		auto command=[context()->queue() commandBuffer];auto copy=[command transferEncoder];
		[copy copyFromTexture:fog.image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(128,2,1)
			toBuffer:fogBytes destinationOffset:0 destinationBytesPerRow:128 destinationBytesPerImage:0];
		[copy copyFromTexture:palette.image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(1024,1,1)
			toBuffer:paletteBytes destinationOffset:0 destinationBytesPerRow:4096 destinationBytesPerImage:0];
		[copy endEncoding];context()->submitCommandBuffer(command);[command waitUntilCompleted];u8 referenceFog[256];MakeFogTexture(referenceFog);
		require(command.status==FCMetal4StatusCompleted && std::memcmp(fogBytes.contents,referenceFog,256)==0 && std::memcmp(paletteBytes.contents,palette32_ram,4096)==0,"Process recovery fog/palette GPU payload differs from CPU tables");
	};
	custom_texture.terminate();renderer=this;clearTextureCache();context()->waitIdle();
	config::CustomTextures=false;config::Fog=true;config::EmulateFramebuffer=false;config::Widescreen=false;config::TextureFiltering=1;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;
	TSP tsp{};tsp.FogCtrl=2;tsp.UseAlpha=1;tsp.SrcInstr=1;
	TCW tcw{};tcw.ScanOrder=1;tcw.PixelFmt=Pixel4444;tcw.TexAddr=0x710000/8;
	for(unsigned i=0;i<64;++i){const u16 value=0xf123;std::memcpy(&vram[tcw.TexAddr*8+i*2],&value,2);}
	auto *original=static_cast<MetalTexture *>(GetTexture(tsp,tcw,0));require(original,"Recovery fixture original texture missing");original->ComputeHash();const u32 hash=original->texture_hash;
	const std::string game="METAL_TEST_RECOVERY";const auto directory=std::filesystem::path(metalOffscreenCacheRoot)/"textures"/game;
	std::filesystem::remove_all(directory);std::filesystem::create_directories(directory);char filename[24];std::snprintf(filename,sizeof(filename),"%08x.png",hash);
	std::vector<u8> png(256);for(size_t i=0;i<png.size();++i)png[i]=demandColor[i%4];
	require(stbi_write_png((directory/filename).c_str(),8,8,4,png.data(),32)!=0,"Cannot write recovery PNG");
	TextureData prepared;prepared.w=prepared.h=8;prepared.data.resize(256);
	for(size_t i=0;i<256;++i)prepared.data[i]=replacement[i%4];
	TA_context input{};input.Alloc();auto& fixture=input.rend;fixture.clearFramebuffer=true;fixture.globClip={640,480};fixture.framebufferWidth=fixture.framebufferHeight=8;
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	fixture.idx={0,1,2,3};for(unsigned i=0;i<4;++i){auto& v=fixture.verts[i];v={};v.x=i&1?640:0;v.y=i&2?480:0;v.z=1;v.u=i&1?1:0;v.v=i&2?1:0;std::fill_n(v.col,4,255);}
	PolyParam poly;poly.init();poly.count=4;poly.pcw.Gouraud=poly.pcw.Texture=1;poly.isp.DepthMode=7;poly.tsp=tsp;poly.tsp.ClampU=poly.tsp.ClampV=1;
	fixture.global_param_op={poly};RenderPass pass{};pass.op_count=1;fixture.render_passes={pass};
	MetalTexture previous;previous.tex_type=TextureType::_8888;previous.UploadToGPU(1,1,oldColor.data(),false);context()->flushTextureUpdates();context()->setLastFrame(previous.image,4.f/3.f);
	clearTextureCache(); // The initial VRAM entry was used only to derive its hash.
	unsigned cases=0,recoveries=0,validSubmissions=0;
	for(bool merged:{false,true})for(Fault fault:{Fault::Cancel,Fault::BeforeCommit,Fault::BeforeEnqueue,Fault::AfterEnqueue,Fault::Feedback,Fault::None}) {
		if(merged && fault==Fault::Cancel)continue; // Scope-unwind cancellation precedes Render.
		@autoreleasepool {
			custom_texture.terminate();clearTextureCache();context()->waitIdle();
			settings.content.gameId=game;config::CustomTextures=true;config::PreloadCustomTextures=false;custom_texture.init();
			wait([&]{return custom_texture.hasReplacement(hash);},"Recovery PNG catalog did not become available");
			rc=&fixture;fixture.global_param_op[0].texture=nullptr;config::NativeDepthInterpolation=cases&1;
			// Initial fog/palette arguments are valid even in a merged Render
			// which has not gone through Process yet.
			if(!fog.image){u8 zeros[256]{};fog.tex_type=TextureType::_8;fog.UploadToGPU(128,2,zeros,false);}
			if(!palette.image){u8 zeros[4]{};palette.tex_type=TextureType::_8888;palette.UploadToGPU(1,1,zeros,false);}
			context()->flushTextureUpdates();context()->waitIdle();
			auto event=[context()->device() newSharedEvent];struct Open {id<MTLSharedEvent> value;~Open(){value.signaledValue=1;}} open{event};
			metalOffscreenArmCommand(fault,event);MetalPreloadedTexture::Ptr preloaded;std::shared_ptr<metal_upload::State> state;FCMetal4Commands *command=nil;
			bool threw=false;
			try {
				{metal_upload::Scope updates(context(),merged);bool deferred=false;preloaded=MetalTexture::preload(prepared,deferred);require(bool(preloaded) && !deferred,"Recovery preload was not admitted");
				 state=context()->textureUploadState();command=metalOffscreenLastCommand();require(command,"Recovery hook missed the actual Context upload command");gpuPreloadedTextures[hash]=preloaded->image;
				 fixture.global_param_op[0].texture=GetTexture(tsp,tcw,0);require(static_cast<MetalTexture *>(fixture.global_param_op[0].texture)->sharedPreloadImage,"Pending same-queue preload was not adopted");
				 if(fault==Fault::Cancel)throw RendererException("Injected outer upload scope cancellation");}
				if(merged){require(Render(),"Merged recovery render failed before submission");context()->flushRenderCommands();}
			} catch(const std::exception& error) {require(std::string(error.what()).find("Injected")!=std::string::npos,"Unexpected exception at recovery fault boundary");threw=true;}
			require(threw==(fault==Fault::Cancel || fault==Fault::BeforeCommit || fault==Fault::BeforeEnqueue || fault==Fault::AfterEnqueue),"Wrong Context fault propagation boundary");
			const bool enqueued=fault==Fault::AfterEnqueue || fault==Fault::Feedback || fault==Fault::None;
			if(enqueued){require(command.status==FCMetal4StatusCommitted && state->phase()==metal_upload::Phase::Pending,"Post-enqueue error lost committed/Pending ownership");require(MetalTexture::preloadUsable(preloaded),"Pending enqueued upload was prematurely invalidated");}
			else{require(state->phase()==metal_upload::Phase::Canceled && !MetalTexture::preloadUsable(preloaded),"Canceled/pre-enqueue Context upload stayed adoptable");}
			event.signaledValue=1;context()->waitIdle();if(enqueued)[command waitUntilCompleted];
			if(fault==Fault::Feedback)require([command.error.domain isEqualToString:@"org.flycast.test.context-feedback"],"Real GPU error was substituted for a synthetic test failure");
			else if(enqueued)require(command.status==FCMetal4StatusCompleted && !command.error,"Real GPU failure in a successful submission control");
			const bool failed=!enqueued || fault==Fault::Feedback;
			require(state->phase()==(fault==Fault::Feedback?metal_upload::Phase::Failed:enqueued?metal_upload::Phase::Ready:metal_upload::Phase::Canceled),"Context upload final phase differs");
			if(failed) {
				require(!MetalTexture::preloadUsable(preloaded),"Failed upload remained adoptable after feedback");
				require(!Render() && rc==nullptr && gpuPreloadedTextures.empty() && gpuPreloadOnDemand && resetTextureCache && updatePalette && updateFogTable,"Actual renderer did not discard prepared work and invalidate the failed GPU map");
				require(!Present() && context()->lastFrame()==previous.image,"Recovery published a failed frame or lost the previous displayed image");
				pixels(previous.image,oldColor);
				command=nil;require(!context()->consumeTextureUploadFailure(),"Delayed command retirement reported the same failure twice");
				const auto before=context()->testTextureUploadCount();
				metalOffscreenParsedTa=[&](TA_context *parsed){require(parsed==&input,"Recovery parser adapter received another context");parsed->rend.global_param_op[0].texture=GetTexture(tsp,tcw,0);};
				Process(&input);auto *fresh=static_cast<MetalTexture *>(fixture.global_param_op[0].texture);require(fresh && !resetTextureCache && !updatePalette && !updateFogTable && fog.image && palette.image,"Process did not rebuild cache/fog/palette after failure");
				require(context()->testTextureUploadCount()==before+3,"Process recovery did not upload original texture, fog and palette exactly once");tables();
				wait([&]{return fresh->is_custom_replaced || fresh->IsCustomTextureAvailable();},"Recovery demand loader did not complete");
				Process(&input);fresh=static_cast<MetalTexture *>(fixture.global_param_op[0].texture);
				require(fresh->is_custom_replaced && !fresh->sharedPreloadImage && gpuPreloadedTextures.empty() && gpuPreloadOnDemand,"Recovery did not rebuild through ordinary custom demand upload");
				require(context()->testTextureUploadCount()==before+4,"Recovery did not upload the PNG replacement exactly once after table rebuild");
				require(Render(),"Fresh render failed after Context upload recovery");pixels(screen,demandColor);
				require(Render(),"Late retirement invalidated a recovered render again");pixels(screen,demandColor);++recoveries;
			} else {
				require(MetalTexture::preloadUsable(preloaded),"Successful/post-enqueue submission lost usable data");
				require(Render(),"Valid submission could not render after completion");pixels(screen,replacement);++validSubmissions;
			}
			metalOffscreenParsedTa={};command=nil;preloaded.reset();fixture.global_param_op[0].texture=nullptr;
			custom_texture.terminate();clearTextureCache();context()->waitIdle();
			for(unsigned i=0;i<3;++i){exactlyOne(context()->testTextureUploadSlot(i));exactlyOne(context()->testPresentationSlot(i));exactlyOne(frameResources[i].available);}
			const auto budget=metal_preload::deviceBudget(context()->device());wait([&]{return budget->usage().bytes==0;},"Recovery left a GPU preload charge after all image owners and work retired");
			metalOffscreenClearCommandHook();++cases;
		}
	}
	for(Fault fault:{Fault::BeforeCommit,Fault::BeforeEnqueue}) {
		@autoreleasepool {
			context()->waitIdle();rc=&fixture;fixture.global_param_op[0].pcw.Texture=0;fixture.global_param_op[0].texture=nullptr;
			auto event=[context()->device() newSharedEvent];struct Open {id<MTLSharedEvent> event;~Open(){event.signaledValue=1;}} open{event};
			auto frame=context()->beginFrame();[frame waitForEvent:event value:1];
			auto source=[context()->device() newBufferWithLength:256 options:MTLResourceStorageModeShared];auto destination=[context()->device() newBufferWithLength:256 options:MTLResourceStorageModeShared];
			for(unsigned i=0;i<256;++i)static_cast<u8 *>(source.contents)[i]=u8(i*29+7);
			auto image=context()->makeTexture(8,8,MTLPixelFormatRGBA8Unorm);auto copy=[frame transferEncoder];
			[copy copyFromBuffer:source sourceOffset:0 sourceBytesPerRow:32 sourceBytesPerImage:0 sourceSize:MTLSizeMake(8,8,1)
				toTexture:image destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
			[copy copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(8,8,1)
				toBuffer:destination destinationOffset:0 destinationBytesPerRow:32 destinationBytesPerImage:0];[copy endEncoding];
			metalOffscreenArmCommand(fault,nil);require(Render(),"Deferred render for actual waitIdle test failed");auto bad=metalOffscreenLastCommand();require(bad,"waitIdle test did not instrument the actual render command");
			auto finished=dispatch_group_create();auto sawSubmission=std::make_shared<std::atomic<bool>>(false);dispatch_group_enter(finished);
			dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
				const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);
				while(frame.status==FCMetal4StatusEncoding && std::chrono::steady_clock::now()<end)std::this_thread::yield();
				sawSubmission->store(frame.status==FCMetal4StatusCommitted);event.signaledValue=1;dispatch_group_leave(finished);
			});
			bool threw=false;try{context()->waitIdle();}catch(const std::exception& error){require(std::string(error.what()).find("Injected")!=std::string::npos,"Unexpected actual waitIdle failure");threw=true;}
			dispatch_group_wait(finished,DISPATCH_TIME_FOREVER);
			require(threw && sawSubmission->load() && frame.status==FCMetal4StatusCompleted,"waitIdle did not submit/drain independent presentation work before rethrowing");[frame waitUntilCompleted];
			require(std::memcmp(source.contents,destination.contents,256)==0,"Independent frame transfer differs after waitIdle error");
			require(!Render() && resetTextureCache && !screenRendered,"Canceled render batch was not invalidated after waitIdle failure");bad=nil;
			for(unsigned i=0;i<3;++i){exactlyOne(context()->testTextureUploadSlot(i));exactlyOne(context()->testPresentationSlot(i));exactlyOne(frameResources[i].available);}
			metalOffscreenClearCommandHook();
		}
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual waitIdle drains GPU-gated presentation work after two deferred render-submit failures, preserves exact transfers, cancels stale output and releases all staging/render/presentation slots once (%s)",perPixel?"OIT":"standard");
	context()->setLastFrame(nil,4.f/3.f);previous.Delete();rc=nullptr;
	NOTICE_LOG(RENDERER,"Metal validation PASS: %u actual Context upload/merged-render fault cases, %u full Process/cache/fog/palette/demand recoveries, %u successful/post-enqueue controls; retained commands, old display image, final upload phases, exact full images, once-only failure/slot release and zero preload budget (%s)",cases,recoveries,validSubmissions,perPixel?"OIT":"standard");
}
