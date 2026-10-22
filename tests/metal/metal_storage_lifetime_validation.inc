// SPDX-License-Identifier: GPL-2.0-or-later
// Three real frame slots behind a native event. Immutable snapshots catch
// reuse of shared nodes, area-1 tails, heads or per-frame inputs too early.
void MetalRenderer::validateSyntheticStorageLifetime()
{
	const auto require=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen,oldNative=config::NativeDepthInterpolation,oldModifiers=config::ModifierVolumes;
	const int64_t oldBudget=config::PixelBufferSize;const int oldLayers=config::PerPixelLayers;
	const u32 oldShade=FPU_SHAD_SCALE.full,oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	struct Restore {
		bool emulate,wide,native,modifiers;int64_t budget;int layers;u32 shade,read,spg;
		~Restore(){metalOffscreenClearCommandHook();config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::NativeDepthInterpolation=native;config::ModifierVolumes=modifiers;config::PixelBufferSize=budget;config::PerPixelLayers=layers;FPU_SHAD_SCALE.full=shade;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore{oldEmulate,oldWide,oldNative,oldModifiers,oldBudget,oldLayers,oldShade,oldRead,oldSpg};
	config::EmulateFramebuffer=false;config::Widescreen=false;config::ModifierVolumes=true;config::PerPixelLayers=32;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;FPU_SHAD_SCALE.scale_factor=128;
	disableDenseOIT=disablePackedOIT=true;legacyOITStorage=false;
	rend_context fixture{};rc=&fixture;fixture.clearFramebuffer=true;fixture.globClip={640,480};
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	fixture.verts.resize(20);for(unsigned q=0;q<5;++q)for(unsigned i=0;i<4;++i) {
		auto& v=fixture.verts[q*4+i];v={};v.x=i&1?640:0;v.y=i&2?480:0;v.z=q==0?1.f:q==4?3.f:1.5f;
		fixture.idx.push_back(q*4+i);
	}
	PolyParam opaque;opaque.init();opaque.count=4;opaque.pcw.Gouraud=1;opaque.isp.DepthMode=7;opaque.tsp.SrcInstr=1;opaque.tsp.FogCtrl=2;
	fixture.global_param_op={opaque};for(unsigned q=1;q<5;++q){auto p=opaque;p.first=q*4;p.pcw.Shadow=1;p.isp.ZWriteDis=1;p.tsp.UseAlpha=1;p.tsp.SrcInstr=4;p.tsp.DstInstr=5;fixture.global_param_tr.push_back(p);}
	fixture.modtrig={{0,0,4,320,0,4,0,480,4},{320,0,4,320,480,4,0,480,4}};
	ModifierVolumeParam modifier;modifier.init();modifier.count=2;modifier.isp.DepthMode=1;modifier.isp.VolumeLast=1;
	fixture.global_param_mvo_tr={modifier};RenderPass pass{};pass.op_count=1;pass.tr_count=4;pass.mvo_tr_count=1;pass.autosort=true;fixture.render_passes={pass};
	u8 zero[256]{};fog.tex_type=TextureType::_8;fog.UploadToGPU(128,2,zero,false);palette.tex_type=TextureType::_8888;palette.UploadToGPU(1,1,zero,false);context()->flushTextureUpdates();
	config::PixelBufferSize=1_MB;MetalRenderer reference(true);require(reference.Init(),"Lifetime reference initialization failed");
	struct StopReference{MetalRenderer& renderer;~StopReference(){renderer.Term();}} stopReference{reference};
	reference.rc=&fixture;reference.disableDenseOIT=reference.disablePackedOIT=true;reference.legacyOITStorage=true;
	reference.fog.tex_type=TextureType::_8;reference.fog.UploadToGPU(128,2,zero,false);reference.palette.tex_type=TextureType::_8888;reference.palette.UploadToGPU(1,1,zero,false);context()->flushTextureUpdates();context()->waitIdle();
	struct Case{u32 width,height;int64_t budget;bool area1;u8 tint;};
	const std::array<Case,3> fixed{{{3840,2160,32_MB,false,0},{1280,720,32_MB,true,1},{1920,1080,32_MB,false,2}}};
	const std::array<Case,3> resized{{{1280,720,32_MB,false,3},{3840,2160,64_MB,true,4},{1920,1080,32_MB,false,5}}};
	const auto select=[&](const Case& test){
		fixture.framebufferWidth=test.width;fixture.framebufferHeight=test.height;config::PixelBufferSize=test.budget;
		for(auto& p:fixture.global_param_tr){p.tsp1.full=p.tcw1.full=~0u;if(test.area1){p.tsp1=p.tsp;p.tcw1.full=0;}}
		for(unsigned q=0;q<5;++q)for(unsigned i=0;i<4;++i){auto& v=fixture.verts[q*4+i];
			const u8 a[4]={u8(23+q*29+test.tint*7),u8(41+q*13+test.tint*5),u8(73+q*17-test.tint*3),u8(q?111:255)};
			const u8 b[4]={u8(211-q*23-test.tint*7),u8(173-q*11+test.tint*3),u8(37+q*19+test.tint*5),u8(147-q*7)};
			std::copy_n(a,4,v.col);std::copy_n(b,4,v.col1);
		}
	};
	const auto exactlyOne=[&](dispatch_semaphore_t available){
		const bool first=dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)==0,second=dispatch_semaphore_wait(available,DISPATCH_TIME_NOW)==0;
		if(second)dispatch_semaphore_signal(available);if(first)dispatch_semaphore_signal(available);
		require(first && !second,"Storage lifetime leaked or double-released a frame slot");
	};
	unsigned images=0,groups=0;uint64_t sharedPeak=0,replacedPeak=0;
	for(bool native:{false,true})for(bool replace:{false,true}) {
		@autoreleasepool {
			config::NativeDepthInterpolation=native;const auto& cases=replace?resized:fixed;
			std::array<std::vector<u8>,3> expected;
			for(unsigned i=0;i<3;++i){select(cases[i]);require(reference.Render(),"Lifetime reference render failed");context()->readTexture(reference.screen,expected[i]);}
			context()->waitIdle();for(auto& slot:frameResources)if(slot.counter)std::memset(slot.counter.contents,0,8);
			auto event=[context()->device() newSharedEvent];struct Open{id<MTLSharedEvent> event;~Open(){event.signaledValue=1;}} open{event};
			auto gate=[context()->queue() commandBuffer];[gate waitForEvent:event value:1];context()->submitCommandBuffer(gate);
			std::array<id<MTLTexture>,3> snapshots;std::array<FCMetal4Commands *,3> renders,copies;
			__weak id<MTLBuffer> weakNodes[3]{};uintptr_t nodeIds[3]{};
			for(unsigned i=0;i<3;++i) {
				@autoreleasepool {
					select(cases[i]);metalOffscreenArmCommand(MetalOffscreenFault::None,nil);
					require(Render(),"Queued storage lifetime render failed");renders[i]=metalOffscreenLastCommand();require(renders[i],"Lifetime render hook missed the real command");
					const auto layout=metal_oit::storageLayout(oitNodeBytes(),cases[i].area1);
					require(frame.capacity==layout.capacity && oitColorOffset==layout.colorOffset && bool(oitColors1)==cases[i].area1,"Queued storage interpretation differs from selected immutable frame");
					weakNodes[i]=nodes;nodeIds[i]=reinterpret_cast<uintptr_t>((__bridge void *)nodes);
					context()->flushRenderCommands();
					snapshots[i]=context()->makeTexture(cases[i].width,cases[i].height,MTLPixelFormatRGBA8Unorm);
					copies[i]=[context()->queue() commandBuffer];auto transfer=[copies[i] transferEncoder];
					[transfer copyFromTexture:screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
						sourceSize:MTLSizeMake(cases[i].width,cases[i].height,1) toTexture:snapshots[i] destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
					[transfer endEncoding];context()->submitCommandBuffer(copies[i]);metalOffscreenClearCommandHook();
				}
			}
			require(gate.status==FCMetal4StatusCommitted && event.signaledValue==0,"Lifetime gate did not keep native work pending");
			for(unsigned i=0;i<3;++i){
				require(renders[i].status==FCMetal4StatusCommitted && copies[i].status==FCMetal4StatusCommitted,"Queued frame completed before opening the native GPU event");
				const bool free=dispatch_semaphore_wait(frameResources[i].available,DISPATCH_TIME_NOW)==0;if(free)dispatch_semaphore_signal(frameResources[i].available);
				require(!free,"Lifetime matrix did not occupy all three actual frame slots");
			}
			uint64_t bytes=0;{
				@autoreleasepool {std::unordered_set<void *> seen;for(auto& weak:weakNodes){id<MTLBuffer> live=weak;require(live,"Old node allocation died while its GPU reader was gated");if(seen.insert((__bridge void *)live).second)bytes+=live.allocatedSize;}}
			}
			if(replace){require(nodeIds[0]!=nodeIds[1] && nodeIds[1]!=nodeIds[2] && nodeIds[0]!=nodeIds[2],"Budget growth/shrink did not replace the node buffer while old readers remained alive");replacedPeak=std::max(replacedPeak,bytes);}
			else{require(nodeIds[0]==nodeIds[1] && nodeIds[1]==nodeIds[2],"Area-1 transitions unnecessarily replaced the common node allocation");sharedPeak=std::max(sharedPeak,bytes);}
			event.signaledValue=1;context()->waitIdle();[gate waitUntilCompleted];require(gate.status==FCMetal4StatusCompleted && !gate.error,"Lifetime gate native feedback failed");
			for(unsigned i=0;i<3;++i){
				[renders[i] waitUntilCompleted];[copies[i] waitUntilCompleted];
				require(renders[i].status==FCMetal4StatusCompleted && !renders[i].error && copies[i].status==FCMetal4StatusCompleted && !copies[i].error,"Queued lifetime work failed on the physical GPU");
				std::vector<u8> actual;context()->readTexture(snapshots[i],actual);require(actual==expected[i],"In-flight OIT area/budget/size transition changed the complete image");++images;
			}
			for(const auto& slot:frameResources){exactlyOne(slot.available);require(static_cast<const u32 *>(slot.counter.contents)[1]==0,"In-flight storage transition overflowed or truncated a node list");}
			if(replace)require(!weakNodes[0] && !weakNodes[1] && weakNodes[2]==nodes,"Retired old node buffers remained owned by completed commands");
			++groups;
		}
	}
	disableDenseOIT=disablePackedOIT=false;rc=nullptr;
	NOTICE_LOG(RENDERER,"Metal validation PASS: %u GPU-gated three-slot OIT storage groups, %u exact full-image snapshots through 4K, both depth modes; area-1 tail reinterpretation without allocation, budget growth/shrink with completion-owned old nodes, exact slot release and zero diagnostic flags; sampled pending node allocations shared=%llu replaced=%llu bytes",groups,images,(unsigned long long)sharedPeak,(unsigned long long)replacedPeak);
}
