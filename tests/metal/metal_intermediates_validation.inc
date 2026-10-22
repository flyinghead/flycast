// SPDX-License-Identifier: GPL-2.0-or-later
// Actual renderer: one same-queue intermediate set versus the original three
// slot-owned sets. Final outputs and all CPU-written inputs remain per-slot.
void MetalRenderer::validateSyntheticIntermediates()
{
	const auto require=[](bool ok,const char *message){if(!ok)throw RendererException(message);};
	const bool timing=metalOffscreenIntermediatesBenchmark;
	if(timing)for(const char *name:{"MTL_DEBUG_LAYER","MTL_SHADER_VALIDATION"}){
		const char *value=std::getenv(name);require(!value || std::string(value)=="0","Intermediate timing requires validation disabled");
	}
	struct Restore {
		bool emulate=config::EmulateFramebuffer,wide=config::Widescreen,native=config::NativeDepthInterpolation,modifiers=config::ModifierVolumes;
		int64_t budget=config::PixelBufferSize;int layers=config::PerPixelLayers;
		u32 shade=FPU_SHAD_SCALE.full,read=FB_R_CTRL.full,spg=SPG_CONTROL.full;
		~Restore(){metalOffscreenClearCommandHook();config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::NativeDepthInterpolation=native;config::ModifierVolumes=modifiers;config::PixelBufferSize=budget;config::PerPixelLayers=layers;FPU_SHAD_SCALE.full=shade;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore;
	config::EmulateFramebuffer=false;config::Widescreen=false;config::ModifierVolumes=true;config::PerPixelLayers=32;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;FPU_SHAD_SCALE.scale_factor=128;
	rend_context fixture{};rc=&fixture;fixture.clearFramebuffer=true;fixture.globClip={640,480};
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	fixture.verts.resize(52);for(unsigned q=0;q<13;++q)for(unsigned i=0;i<4;++i){auto& v=fixture.verts[q*4+i];v={};
		v.x=i&1?640:0;v.y=i&2?480:0;v.z=q?1.5f:1.f;fixture.idx.push_back(q*4+i);}
	PolyParam opaque;opaque.init();opaque.count=4;opaque.pcw.Gouraud=1;opaque.isp.DepthMode=7;opaque.tsp.SrcInstr=1;opaque.tsp.FogCtrl=2;
	fixture.global_param_op={opaque};
	for(unsigned q=1;q<13;++q){auto p=opaque;p.first=q*4;p.pcw.Shadow=1;p.isp.ZWriteDis=1;p.tsp.UseAlpha=1;p.tsp.SrcInstr=4;p.tsp.DstInstr=5;p.tsp1=p.tsp;p.tcw1.full=0;fixture.global_param_tr.push_back(p);}
	fixture.modtrig={{0,0,4,320,0,4,0,480,4},{320,0,4,320,480,4,0,480,4}};
	ModifierVolumeParam modifier;modifier.init();modifier.count=2;modifier.isp.DepthMode=1;modifier.isp.VolumeLast=1;
	for(unsigned pass=0;pass<3;++pass){fixture.global_param_mvo_tr.push_back(modifier);RenderPass p{};p.op_count=1;p.tr_count=(pass+1)*4;p.mvo_tr_count=pass+1;p.autosort=true;fixture.render_passes.push_back(p);}
	config::PixelBufferSize=1_MB;MetalRenderer reference(true);require(reference.Init(),"Intermediate reference initialization failed");
	struct Stop{MetalRenderer& r;~Stop(){r.Term();}} stop{reference};
	reference.rc=&fixture;reference.perSlotIntermediates=true;
	for(auto *n:{this,&reference}){n->measureOITStorage=true;
		u8 zero[256]{};n->fog.tex_type=TextureType::_8;n->fog.UploadToGPU(128,2,zero,false);n->palette.tex_type=TextureType::_8888;n->palette.UploadToGPU(1,1,zero,false);}
	context()->flushTextureUpdates();context()->waitIdle();
	const auto select=[&](u32 width,u32 height,unsigned tint){
		fixture.framebufferWidth=width;fixture.framebufferHeight=height;config::PixelBufferSize=int64_t(width)*height*24;
		for(unsigned q=0;q<13;++q)for(unsigned i=0;i<4;++i){auto& v=fixture.verts[q*4+i];
			const u8 a[4]={u8(23+q*17+tint*13),u8(41+q*13+tint*7),u8(73+q*11),u8(q?111:255)};
			const u8 b[4]={u8(211-q*13),u8(173-q*7-tint*17),u8(37+q*17+tint*11),u8(147-q*3)};
			std::copy_n(a,4,v.col);std::copy_n(b,4,v.col1);}
	};
	const auto resources=[](const GPUIntermediates& s){return std::array<id<MTLTexture>,4>{s.rasterColor,s.shadowDepth,s.resolveScratch[0],s.resolveScratch[1]};};
	const auto retainedBytes=[&](const MetalRenderer& n){
		u64 total=0;std::unordered_set<void *> seen;
		const auto add=[&](const GPUIntermediates& s){for(auto image:resources(s))if(image && seen.insert((__bridge void *)image).second)total+=image.allocatedSize;};
		add(n.gpuIntermediates);for(const auto& slot:n.frameResources)add(slot.legacyIntermediates);return total;
	};
	// Weak observations do not extend GPU resource lifetimes. Count only base
	// allocations: shadowStencil is a view of shadowDepth, not another image.
	struct ObservedImage { __weak id<MTLTexture> image; u64 bytes; };
	const auto observe=[&](const MetalRenderer& n,std::vector<ObservedImage>& observed){
		const auto add=[&](const GPUIntermediates& s){for(auto image:resources(s))if(image){
			const bool known=std::any_of(observed.begin(),observed.end(),[&](const auto& entry){return entry.image==image;});
			if(!known)observed.push_back({image,image.allocatedSize});
		}};
		add(n.gpuIntermediates);for(const auto& slot:n.frameResources)add(slot.legacyIntermediates);
	};
	const auto liveBytes=[](const std::vector<ObservedImage>& observed){
		u64 bytes=0;for(const auto& entry:observed)if(entry.image)bytes+=entry.bytes;return bytes;
	};
	struct Sample{double cpu,gpu;u64 tiles,draws;};
	const auto render=[&](MetalRenderer& n,std::vector<u8>& pixels){
		n.measuredOITTiles=n.oitPolygonDraws=0;metalOffscreenArmCommand(MetalOffscreenFault::None,nil);
		const auto begin=std::chrono::steady_clock::now();require(n.Render(),"Intermediate render failed");auto command=metalOffscreenLastCommand();
		require(command,"Intermediate render missed actual command");context()->flushRenderCommands();const auto encoded=std::chrono::steady_clock::now();[command waitUntilCompleted];
		require(command.status==FCMetal4StatusCompleted && !command.error,"Intermediate GPU work failed");
		require(static_cast<const u32 *>(n.counter.contents)[1]==0,"Intermediate render exceeded an OIT limit");
		Sample s{std::chrono::duration<double,std::micro>(encoded-begin).count(),(command.GPUEndTime-command.GPUStartTime)*1000,n.measuredOITTiles,n.oitPolygonDraws};
		if(timing)require(command.GPUStartTime>0 && s.gpu>0,"Intermediate GPU timing unavailable");
		context()->readTexture(n.screen,pixels);require(pixels.size()==size_t(n.frame.width)*n.frame.height*4,"Intermediate image dimensions differ");
		const size_t left=(size_t(n.frame.height/2)*n.frame.width+n.frame.width/4)*4,right=(size_t(n.frame.height/2)*n.frame.width+n.frame.width*3/4)*4;
		require(std::memcmp(pixels.data()+left,pixels.data()+right,4)!=0,"Intermediate fixture lost visible modifier effects");
		metalOffscreenClearCommandHook();return s;
	};
	const auto median=[](std::vector<double> a){std::sort(a.begin(),a.end());return a[a.size()/2];};
	unsigned pairs=0,groups=0,pendingImages=0;
	for(auto size:{std::array<u32,2>{640,480},{1920,1080},{3840,2160}})for(bool native:{false,true}){
		config::NativeDepthInterpolation=native;select(size[0],size[1],0);
		std::vector<double> cpu[2],gpu[2];Sample last[2]{};const int rounds=timing?21:1,warmup=6;
		for(int round=-warmup;round<rounds;++round){std::vector<u8> pixels[2];
			for(unsigned turn=0;turn<2;++turn){unsigned v=turn^(unsigned(round+warmup)&1u);last[v]=render(v?*this:reference,pixels[v]);
				if(round>=0){cpu[v].push_back(last[v].cpu);gpu[v].push_back(last[v].gpu);}}
			require(pixels[0]==pixels[1],"Shared intermediate images changed full GPU pixels");
			require(last[0].tiles==last[1].tiles && last[0].draws==last[1].draws,"Intermediate sharing changed draw work");++pairs;
		}
		const u64 before=retainedBytes(reference),after=retainedBytes(*this);
		require(after && before==after*3,"Shared intermediates did not reduce three steady-size sets to one");
		for(auto image:resources(gpuIntermediates))require(image && image!=screen,"Intermediate image escaped as final output");
		NOTICE_LOG(RENDERER,"Metal intermediates size=%ux%u native=%d rounds=%d retained_bytes=%llu/%llu tiles=%llu/%llu draws=%llu/%llu cpu_us=%.3f/%.3f gpu_ms=%.6f/%.6f",size[0],size[1],native,rounds,before,after,last[0].tiles,last[1].tiles,last[0].draws,last[1].draws,median(cpu[0]),median(cpu[1]),median(gpu[0]),median(gpu[1]));
		if(timing)continue;
		for(bool resize:{false,true})for(bool legacy:{true,false}){@autoreleasepool{
			auto& n=legacy?reference:*this;
			std::array<std::vector<u8>,3> expected;
			const auto pick=[&](unsigned i){select(resize && i==1?640:size[0],resize && i==1?480:size[1],i+1);};
			for(unsigned i=0;i<3;++i){pick(i);render(reference,expected[i]);}context()->waitIdle();
			// Reference image generation must not pre-populate the legacy slots
			// with the upcoming mixed sizes. Start both paths at the same warm
			// resolution before submitting the A/B/A sequence behind the gate.
			for(unsigned i=0;i<3;++i){@autoreleasepool{
				select(size[0],size[1],0);std::vector<u8> ignored;render(n,ignored);
			}}
			std::vector<ObservedImage> observed;observe(n,observed);
			const u64 startingBytes=liveBytes(observed);u64 peakBytes=startingBytes;
			require(startingBytes==(legacy?before:after),"Intermediate overlap did not start at the common warm size");
			auto event=[context()->device() newSharedEvent];struct Open{id<MTLSharedEvent> event;~Open(){event.signaledValue=1;}} open{event};
			auto gate=[context()->queue() commandBuffer];[gate waitForEvent:event value:1];context()->submitCommandBuffer(gate);
			std::array<FCMetal4Commands *,3> renders,copies;std::array<id<MTLTexture>,3> snapshots;
			uintptr_t identities[3][4]{};__weak id<MTLTexture> oldImages[4]{};
			for(unsigned i=0;i<3;++i){@autoreleasepool{
				pick(i);metalOffscreenArmCommand(MetalOffscreenFault::None,nil);require(n.Render(),"Queued intermediate render failed");
				renders[i]=metalOffscreenLastCommand();require(renders[i],"Queued intermediate render missed command");context()->flushRenderCommands();
				const auto images=resources(n.intermediateImages(n.frameResources[n.currentFrameResource]));for(unsigned j=0;j<4;++j){identities[i][j]=reinterpret_cast<uintptr_t>((__bridge void *)images[j]);if(i==0)oldImages[j]=images[j];}
				observe(n,observed);
				snapshots[i]=context()->makeTexture(n.frame.width,n.frame.height,MTLPixelFormatRGBA8Unorm);
				copies[i]=[context()->queue() commandBuffer];auto transfer=[copies[i] transferEncoder];
				[transfer copyFromTexture:n.screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(n.frame.width,n.frame.height,1)
					toTexture:snapshots[i] destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
				[transfer endEncoding];context()->submitCommandBuffer(copies[i]);metalOffscreenClearCommandHook();
			}peakBytes=std::max(peakBytes,liveBytes(observed));}
			require(event.signaledValue==0 && gate.status==FCMetal4StatusCommitted,"Intermediate GPU gate released early");
			const bool replaced=resize && size[0]!=640;
			for(unsigned j=0;j<4;++j){require(oldImages[j],"Intermediate image retired before GPU readers completed");
				require((identities[0][j]!=identities[1][j])==(legacy || replaced) && (identities[1][j]!=identities[2][j])==(legacy || replaced),"Intermediate sharing/replacement identity differs");}
			for(unsigned i=0;i<3;++i){require(renders[i].status==FCMetal4StatusCommitted && copies[i].status==FCMetal4StatusCommitted,"Intermediate work completed before opening GPU gate");
				const bool free=dispatch_semaphore_wait(n.frameResources[i].available,DISPATCH_TIME_NOW)==0;if(free)dispatch_semaphore_signal(n.frameResources[i].available);
				require(!free,"Intermediate test failed to occupy all actual frame slots");}
			event.signaledValue=1;context()->waitIdle();
			for(unsigned i=0;i<3;++i){[renders[i] waitUntilCompleted];[copies[i] waitUntilCompleted];
				require(renders[i].status==FCMetal4StatusCompleted && !renders[i].error && copies[i].status==FCMetal4StatusCompleted && !copies[i].error,"Queued intermediate GPU feedback failed");
				std::vector<u8> pixels;context()->readTexture(snapshots[i],pixels);require(pixels==expected[i],"Queued intermediate reuse changed full snapshot");++pendingImages;}
			if(replaced && !legacy)for(auto& weak:oldImages)require(!weak,"Old intermediate remained retained after completed replacement");
			const u64 finalBytes=liveBytes(observed);
			require(finalBytes==retainedBytes(n),"Completed intermediate overlap retained an old allocation");
			require(peakBytes>=finalBytes && peakBytes>=startingBytes,"Intermediate peak omitted live allocations");
			if(!legacy && replaced)require(peakBytes>finalBytes,"Resize fixture missed concurrent old/new GPU allocations");
			NOTICE_LOG(RENDERER,"Metal intermediate overlap size=%ux%u native=%d resize=%d legacy=%d start_bytes=%llu peak_bytes=%llu final_bytes=%llu",size[0],size[1],native,resize,legacy,startingBytes,peakBytes,finalBytes);
			for(const auto& slot:n.frameResources){const bool first=dispatch_semaphore_wait(slot.available,DISPATCH_TIME_NOW)==0,second=dispatch_semaphore_wait(slot.available,DISPATCH_TIME_NOW)==0;
				if(second)dispatch_semaphore_signal(slot.available);if(first)dispatch_semaphore_signal(slot.available);
				require(first && !second && static_cast<const u32 *>(slot.counter.contents)[1]==0,"Intermediate test leaked slot or exceeded OIT limits");}
			++groups;
		}}
	}
	measureOITStorage=false;rc=nullptr;
	NOTICE_LOG(RENDERER,"Metal validation PASS: shared GPU intermediates; %u exact full-image pairs, %u gated three-slot groups / %u snapshots; three OIT passes, both depth modes through 4K, fixed/changed dimensions, old-reader retirement, identical draw work, one third retained scratch bytes",pairs,groups,pendingImages);
}
