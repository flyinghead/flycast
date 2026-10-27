// SPDX-License-Identifier: GPL-2.0-or-later
// Actual material changes, generic fallbacks, async specializations and renderer
// restarts. Exact full-image CPU oracle; application archive cold/warm is handled
// by separate processes. Never remove or pretend to cold-reset Apple's cache.
void MetalRenderer::validateSyntheticCacheProfile()
{
	const auto require=[](bool value,const char *message){if(!value)throw RendererException(message);};
	require(context()->pipelineStatisticsForTesting().timing,"Cache profile did not enable real archive timings");
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen,oldNative=config::NativeDepthInterpolation,oldSort=config::PerStripSorting;
	const int64_t oldBudget=config::PixelBufferSize;const float oldScale=config::ExtraDepthScale;
	const u32 oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	struct Restore{bool emulate,wide,native,sort;int64_t budget;float scale;u32 read,spg;
		~Restore(){metalOffscreenClearCommandHook();config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::NativeDepthInterpolation=native;config::PerStripSorting=sort;config::PixelBufferSize=budget;config::ExtraDepthScale=scale;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore{oldEmulate,oldWide,oldNative,oldSort,oldBudget,oldScale,oldRead,oldSpg};
	config::EmulateFramebuffer=false;config::Widescreen=false;config::NativeDepthInterpolation=false;config::PerStripSorting=true;config::ExtraDepthScale=1;config::PixelBufferSize=1_MB;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;
	rend_context fixture{};fixture.framebufferWidth=320;fixture.framebufferHeight=240;fixture.clearFramebuffer=true;fixture.globClip={640,480};
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	fixture.verts.resize(8);fixture.idx={0,1,2,3,4,5,6,7};
	for(unsigned q=0;q<2;++q)for(unsigned i=0;i<4;++i){auto& v=fixture.verts[q*4+i];v={};v.x=q?(i&1?480:160):(i&1?640:0);v.y=q?(i&2?360:120):(i&2?480:0);v.z=q?2:1;v.u=i&1?1:0;v.v=i&2?1:0;v.col[0]=q?255:11;v.col[1]=q?255:23;v.col[2]=q?255:47;v.col[3]=255;}
	PolyParam bg;bg.init();bg.count=4;bg.pcw.Gouraud=1;bg.isp.DepthMode=7;bg.tsp.SrcInstr=1;bg.tsp.FogCtrl=2;fixture.global_param_op={bg};
	MetalTexture texture;texture.tex_type=TextureType::_8888;std::array<u8,256> white;white.fill(255);texture.UploadToGPU(8,8,white.data(),false);
	PolyParam p=bg;p.first=4;p.pcw.Texture=1;p.texture=&texture;p.tcw.PixelFmt=Pixel4444;p.isp.ZWriteDis=1;p.tsp.ClampU=p.tsp.ClampV=1;fixture.global_param_tr={p};
	RenderPass pass{};pass.op_count=1;pass.tr_count=1;pass.autosort=true;fixture.render_passes={pass};
	const auto setup=[&]{rc=&fixture;disableBackgroundClear=true;u8 zero[256]{};fog.tex_type=TextureType::_8;fog.UploadToGPU(128,2,zero,false);palette.tex_type=TextureType::_8888;palette.UploadToGPU(1,1,zero,false);context()->flushTextureUpdates();context()->waitIdle();};setup();
	std::vector<u8> golden(320*240*4);
	for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x){const bool center=x>=80 && x<240 && y>=60 && y<180;const u8 color[]={u8(center?255:11),u8(center?255:23),u8(center?255:47),255};std::copy_n(color,4,golden.data()+(y*320+x)*4);}
	struct Frame{unsigned index,kind;bool native,restart,specialized;double cpu,render,restartTime,gpu,interval;uint64_t hits,misses,writer,queued,duplicates;size_t pending,ready;std::array<uint64_t,4> syncCalls,syncNs;};
	// Separate processes can populate disjoint material subsets, matching
	// archives accumulated across sessions instead of one all-inclusive segment.
	const bool seedEven=metalOffscreenCachePhase=="seed-even",seedOdd=metalOffscreenCachePhase=="seed-odd";
	std::vector<Frame> samples;samples.reserve(192);auto epoch=std::chrono::steady_clock::now(),previous=epoch;
	for(unsigned frameIndex=0;frameIndex<192;++frameIndex){
		std::this_thread::sleep_until(epoch+std::chrono::nanoseconds(16666667)*frameIndex);const auto begin=std::chrono::steady_clock::now();const bool restart=frameIndex && frameIndex%64==0;
		if(restart){Term();require(Init(),"Actual cache-profile renderer restart failed");setup();}
		const auto renderBegin=std::chrono::steady_clock::now();const unsigned kind=seedEven || seedOdd ? ((frameIndex/4)%8)*2+unsigned(seedOdd) : (frameIndex/4)%16;
		config::NativeDepthInterpolation=(frameIndex/64)&1;auto& poly=fixture.global_param_tr[0];poly.pcw.Gouraud=kind&1;poly.tsp.ShadInstr=(kind>>1)&3;poly.tsp.IgnoreTexA=(kind>>3)&1;poly.tsp.UseAlpha=kind&1;
		poly.tsp.SrcInstr=kind&4?4:1;poly.tsp.DstInstr=kind&4?5:0;poly.isp.DepthMode=kind&2?6:7;
		const auto prior=context()->pipelineStatisticsForTesting();
		metalOffscreenArmCommand(MetalOffscreenFault::None,nil);require(Render(),"Cache transition Render failed");auto command=metalOffscreenLastCommand();require(command,"Cache transition missed actual command");context()->flushRenderCommands();const auto submitted=std::chrono::steady_clock::now();
		[command waitUntilCompleted];require(command.status==FCMetal4StatusCompleted && !command.error,"Cache transition native work failed");
		std::vector<u8> actual;context()->readTexture(screen,actual);require(actual==golden,"Cached/generic material changed independently expected full-frame RGBA");
		const auto stats=context()->pipelineStatisticsForTesting();const auto material=materialPipelines.find(pipelineSelection.key);const bool specialized=material!=materialPipelines.end() && material->second && material->second==pipelineSelection.state;
		const auto ready=std::count_if(materialPipelines.begin(),materialPipelines.end(),[](const auto& v){return v.second!=nil;});
		const double gpu=(command.GPUEndTime-command.GPUStartTime)*1000;require(command.GPUStartTime>0 && gpu>0,"Cache frame native GPU timing unavailable");
		// Foreground categories cannot overlap each other. Exclude material and
		// writer timings, which run asynchronously and are not frame latency.
		std::array<uint64_t,4> calls{},ns{};
		for(unsigned i=0;i<4;++i){const unsigned category=i<2?0:2;
			const auto a=i%2?prior.creation[category]:prior.lookup[category];
			const auto b=i%2?stats.creation[category]:stats.lookup[category];
			calls[i]=b.calls-a.calls;ns[i]=b.nanoseconds-a.nanoseconds;}
		samples.push_back({frameIndex,kind,bool(config::NativeDepthInterpolation),restart,specialized,std::chrono::duration<double,std::micro>(submitted-begin).count(),std::chrono::duration<double,std::micro>(submitted-renderBegin).count(),std::chrono::duration<double,std::micro>(renderBegin-begin).count(),gpu,std::chrono::duration<double,std::micro>(begin-previous).count(),stats.hits,stats.misses,stats.recordings,stats.queued,stats.duplicates,stats.pending,size_t(ready),calls,ns});previous=begin;metalOffscreenClearCommandHook();
	}
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	require(std::all_of(materialPipelines.begin(),materialPipelines.end(),[](const auto& p){return p.second!=nil;}),"Material compiler left a missing specialization");
	require(std::count_if(samples.begin(),samples.end(),[](const Frame& f){return f.specialized;})>0,"Cache profile never selected real material specialization");
	for(const auto& f:samples)NOTICE_LOG(RENDERER,"Metal cache frame mode=%s phase=%s optimized=%d frame=%u case=%u native=%d restart=%d specialized=%d cpu_us=%.3f render_us=%.3f restart_us=%.3f gpu_ms=%.6f interval_us=%.3f hits=%llu misses=%llu writer=%llu queued=%llu duplicates=%llu pending=%zu ready=%zu",perPixel?"OIT":"standard",metalOffscreenCachePhase.c_str(),metalOffscreenCacheOptimized,f.index,f.kind,f.native,f.restart,f.specialized,f.cpu,f.render,f.restartTime,f.gpu,f.interval,f.hits,f.misses,f.writer,f.queued,f.duplicates,f.pending,f.ready);
	for(const auto& f:samples)NOTICE_LOG(RENDERER,"Metal cache synchronous mode=%s frame=%u fg_lookup_calls=%llu fg_create_calls=%llu compute_lookup_calls=%llu compute_create_calls=%llu fg_lookup_us=%.3f fg_create_us=%.3f compute_lookup_us=%.3f compute_create_us=%.3f",perPixel?"OIT":"standard",f.index,f.syncCalls[0],f.syncCalls[1],f.syncCalls[2],f.syncCalls[3],f.syncNs[0]/1000.,f.syncNs[1]/1000.,f.syncNs[2]/1000.,f.syncNs[3]/1000.);
	rc=nullptr;disableBackgroundClear=false;
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual pipeline-cache material transitions %s, 192 exact full-frame CPU-oracle images, %u material cases, both depth modes, two real renderer restarts and active asynchronous specializations",perPixel?"OIT":"standard",seedEven || seedOdd ? 8:16);
}

void MetalRenderer::validateSyntheticFixedModifierPipelines()
{
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	context()->drainMaterialCompilerForTesting();
	std::vector<u64> keys={0x10000u};
	if(perPixel)keys.push_back(0x10001u);
	else for(u64 key:{0x20000ull,0x10000ull|(1ull<<49),0x20000ull|(1ull<<49)})keys.push_back(key);
	std::vector<id<MTLRenderPipelineState>> ready;
	for(u64 key:keys){
		const auto found=pipelines.find(key);
		check(found!=pipelines.end() && found->second,"Init left a fixed modifier descriptor for its first draw");
		ready.push_back(found->second);
	}
	const auto request=[&](u64 key){
		return key&0x20000u ? finalModifierPipeline((key&(1ull<<49))!=0)
			: modifierPipeline((key&1u)!=0,(key&(1ull<<49))!=0);
	};
	const auto before=context()->testRenderPipelineRequestCount();
	for(unsigned repeat=0;repeat<32;++repeat)for(size_t i=0;i<keys.size();++i)
		check(request(keys[i])==ready[i],"Fixed modifier pipeline was replaced during repeated draw-side requests");
	const auto warmed=context()->testRenderPipelineRequestCount();
	check(warmed==before,"Ready modifier descriptor reached the foreground pipeline constructor");
	// A controlled lazy-cache state uses the same actual descriptor constructors
	// as the former first draw. Count synchronous work, not a cold-driver latency.
	for(u64 key:keys)pipelines.erase(key);
	for(u64 key:keys)check(request(key)!=nil,"Lazy modifier control failed to create a real pipeline");
	const auto lazy=context()->testRenderPipelineRequestCount();
	check(lazy-warmed==keys.size(),"Lazy modifier control did not reach the actual foreground pipeline constructor once per descriptor");
	NOTICE_LOG(RENDERER,"Metal validation PASS: fixed modifier pipelines %s, %zu descriptors ready after Init, %zu repeated draw requests with zero foreground pipeline constructor requests; same-descriptor lazy control made %llu requests; remaining production image suite follows (no cold-driver latency/FPS claim)",
		perPixel?"OIT":"standard",keys.size(),keys.size()*32,(unsigned long long)(lazy-warmed));
}

// The background clear still rasterizes its original depth/stencil. A standard
// colorless depth draw already has exactly the required pipeline; OIT needs its
// distinct RGBA attachment variant. The MRT form retains both write masks off.
void MetalRenderer::validateSyntheticBackgroundPipelines()
{
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	struct Restore {bool& state;bool value;~Restore(){state=value;}} restore{directAccumulator,directAccumulator};
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	PolyParam poly;poly.init();poly.tsp.SrcInstr=1;poly.tsp.DstInstr=0;
	unsigned requests=0;
	for(bool secondary:{false,true}) {
		directAccumulator=secondary;
		const auto before=context()->testRenderPipelineRequestCount();
		auto background=pipeline(poly,perPixel?PolyPass::Color0:PolyPass::Direct,true,false,nullptr,true);
		const auto created=context()->testRenderPipelineRequestCount()-before;
		NOTICE_LOG(RENDERER,"Metal background pipeline mode=%s secondary=%d first_sync_requests=%llu",perPixel?"OIT":"standard",secondary,(unsigned long long)created);
		check(background && created==0,"First fixed background depth draw requested a synchronous render pipeline");
		if(!perPixel)check(pipeline(poly,PolyPass::DepthOnly,true)==background,"Standard background duplicated its identical colorless depth pipeline");
		else check(pipeline(poly,PolyPass::DepthOnly,true)!=background,"OIT background lost its required RGBA attachment variant");
		const auto pipelineCount=pipelines.size();
		for(bool naomi2:{false,true})for(unsigned src=0;src<8;++src)for(unsigned dst=0;dst<8;++dst) {
			poly.projMatrix=naomi2?0:-1;poly.tsp.SrcInstr=src;poly.tsp.DstInstr=dst;
			check(pipeline(poly,perPixel?PolyPass::Color0:PolyPass::Direct,true,false,nullptr,true)==background,
				"Colorless background pipeline depends on unused color/vertex inputs");++requests;
		}
		check(context()->testRenderPipelineRequestCount()==before && pipelines.size()==pipelineCount,
			"Repeated fixed background draws reached the synchronous compiler or added redundant pipelines");
		poly.projMatrix=-1;poly.tsp.SrcInstr=1;poly.tsp.DstInstr=0;
	}
	pipelineSelection={};
	NOTICE_LOG(RENDERER,"Metal validation PASS: fixed background depth pipelines %s, ordinary/MRT and %u repeated blend/vertex choices, zero foreground requests after Init; standard background shares its exact depth PSO, OIT keeps its RGBA attachment state (no application latency/FPS claim)",perPixel?"OIT":"standard",requests);
}

void MetalRenderer::validateSyntheticMaterialAdmission()
{
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	auto wanted=std::make_shared<std::atomic<bool>>(true);
	PolyParam poly;poly.init();poly.pcw.Gouraud=1;poly.tsp.SrcInstr=1;poly.tsp.FogCtrl=2;
	MetalPolyUniforms material{};material.flags=Gouraud;material.tsp0=poly.tsp.full;
	u64 key=0;
	context()->withBlockedMaterialCompilerForTesting([&]{
		for(unsigned i=0;i<32;++i)
			context()->makePipelineAsync(@"quad_vertex",@"quad_fragment",nil,MTLPixelFormatBGRA8Unorm,false,nil,
				{i,0,0,false},[](id<MTLRenderPipelineState>,NSError *){},[wanted]{return wanted->load();});
		auto fallback=pipeline(poly,PolyPass::Direct,true,false,&material);key=pipelineSelection.key;
		wanted->store(false);
		check(fallback && materialAdmissionBlocked,"Full compiler did not select a generic material fallback");
		check(materialPipelines.find(key)==materialPipelines.end(),"Deferred material was marked as permanently pending");
	});
	context()->drainMaterialCompilerForTesting();
	// Canceled jobs produced no ready results. An identical next-frame choice
	// must nevertheless retry instead of reusing the cached generic selection.
	{std::lock_guard<std::mutex> lock(pipelineResults->mutex);check(pipelineResults->ready.empty(),"Canceled setup unexpectedly published a material");}
	collectMaterialPipelines();
	check(!pipelineSelection.state && !materialAdmissionBlocked,"Deferred selection was not reset without ready results");
	check(pipeline(poly,PolyPass::Direct,true,false,&material)!=nil,"Re-admitted material lost its generic fallback");
	check(materialPipelines.find(key)!=materialPipelines.end(),"Deferred material was never re-admitted");
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(materialPipelines.at(key)!=nil && pipeline(poly,PolyPass::Direct,true,false,&material)==materialPipelines.at(key),
		"Re-admitted material did not switch to its completed specialization");
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual %s material admission, generic fallback under full compiler, canceled producer jobs, empty-result next-frame retry and completed specialization selected",perPixel?"OIT":"standard");
}

void MetalRenderer::validateSyntheticResolveAdmission()
{
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	for(auto entry=resolvePipelines.begin();entry!=resolvePipelines.end();)
		if((entry->first&511u)<32)entry=resolvePipelines.erase(entry);else ++entry;

	struct Family{bool sourceOver,dense,packed,modifiers=false;};
	const Family families[]={{false,false,false},{true,false,false},{false,true,false},
		{true,true,false},{false,true,true},{false,false,true},{true,false,true},{false,false,true,true}};
	unsigned cases=0;
	const std::pair<u32,u32> sizes[]={{8,6},{1920,1080},{3840,2160}};
	for(const auto size:sizes) {
	if(!metalOffscreenResolveBenchmark && size.first!=8)continue;
	const u32 width=size.first,height=size.second,pixels=width*height;
	for(const auto family:families)for(bool reset:{false,true}) { @autoreleasepool {
		const auto large=resolvePipeline(32,reset,family.sourceOver,family.dense,family.packed,family.modifiers);
		const u32 key=1u | (u32(reset)<<9) | (u32(family.sourceOver)<<10) | (u32(family.dense)<<11) | (u32(family.packed)<<12) | (u32(family.modifiers)<<13);
		const auto compileStart=std::chrono::steady_clock::now();
		auto small=makeResolvePipeline([NSString stringWithFormat:@"oit_resolve_1%@%@%@%@",reset?@"_reset":@"",
			family.dense?@"_dense":@"",family.sourceOver?@"_source_over":@"",family.modifiers?@"_mod_packed":family.packed?@"_packed":@""],1);
		const double compileUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-compileStart).count();
		frame={};frame.width=width;frame.height=height;frame.capacity=pixels*2;frame.maxLayers=family.dense?1:4;frame.shadeScale=1;
		frameUniforms=upload(&frame,sizeof(frame));
		std::vector<u8> background(size_t(pixels)*4);
		for(size_t i=0;i<background.size();++i)background[i]=u8(i*17+23);
		auto input=context()->makeTexture(width,height,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead,false,true);
		[input replaceRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0 withBytes:background.data() bytesPerRow:width*4];
		std::vector<MetalOITNode> list(family.dense?pixels:pixels*2);
		std::vector<u32> initialHeads(pixels);
		for(u32 pixel=0;pixel<pixels;++pixel){
			const u32 index=family.dense?pixel:pixel*2;initialHeads[pixel]=family.dense?0:index;
			list[index]={0x80000000u | ((pixel*31+19)&255) | (((pixel*13+47)&255)<<8) | (((pixel*7+67)&255)<<16),float(pixel),family.dense?~0u:index+1,pixel&0x1ffffu,0};
			if(!family.dense)list[index+1]={0xff00ff00u,float(pixel+1),~0u,(pixel+1)&0x1ffffu,0};
		}
		if(family.dense && (family.sourceOver || family.packed)){
			std::vector<MetalOITDenseColorNode> compact;for(const auto& n:list)compact.push_back({n.color0,n.depth,n.sequence});nodes=upload(compact.data(),compact.size()*sizeof(compact[0]));
		}else if(family.dense){
			std::vector<MetalOITDenseNode> compact;for(const auto& n:list)compact.push_back({n.color0,n.depth,n.sequence,0});nodes=upload(compact.data(),compact.size()*sizeof(compact[0]));
		}else if(family.packed){
			std::vector<MetalOITPackedNode> compact;for(const auto& n:list)compact.push_back({n.color0,n.depth,family.modifiers ? n.next & MetalOITModifierNextMask : n.next,n.sequence});nodes=upload(compact.data(),compact.size()*sizeof(compact[0]));
		}else nodes=upload(list.data(),list.size()*sizeof(list[0]));
		heads=upload(initialHeads.data(),initialHeads.size()*sizeof(u32));u32 zero[2]{};counter=upload(zero,sizeof(zero));
		const MetalOITPoly polygon{(4u<<29) | (5u<<26),0};oitPolygons=upload(&polygon,sizeof(polygon));oitColors1=nil;oitColorOffset=0;
		const metal_oit::Tile tile{0,0,width,height};
		const auto draw=[&](double *gpu=nullptr) {
			memcpy(heads.contents,initialHeads.data(),initialHeads.size()*sizeof(u32));memset(counter.contents,0,8);target=input;
			auto output=context()->makeTexture(width,height,MTLPixelFormatRGBA8Unorm,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite);
			auto command=[context()->queue() commandBuffer];
			resolveFramebuffer(command,true,frameResources.front(),true,reset,output,&tile,1,family.sourceOver,family.dense,family.packed,family.modifiers);
			context()->submitCommandBuffer(command);std::vector<u8> pixels;context()->readTexture(output,pixels);
			if(gpu)*gpu=(command.GPUEndTime-command.GPUStartTime)*1000;
			std::vector<u32> resultingHeads(initialHeads.size());memcpy(resultingHeads.data(),heads.contents,initialHeads.size()*sizeof(u32));
			return std::make_tuple(pixels,((u32 *)counter.contents)[1],resultingHeads);
		};
		double referenceGpu=0,fallbackGpu=0,specializedGpu=0,admitUs=0;
		resolvePipelines[key]=small;const auto reference=draw(&referenceGpu);resolvePipelines.erase(key);
		const auto prior=context()->pipelineStatisticsForTesting();
		context()->withBlockedMaterialCompilerForTesting([&]{
			const auto admitStart=std::chrono::steady_clock::now();
			const auto selected=resolvePipeline(1,reset,family.sourceOver,family.dense,family.packed,family.modifiers);
			admitUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-admitStart).count();
			check(selected.state==large.state,"Missing small resolve synchronously compiled instead of reusing an identical larger family");
			check(context()->pipelineStatisticsForTesting().misses==prior.misses,"Resolve fallback performed a foreground compiler lookup");
			check(draw(&fallbackGpu)==reference,"Larger resolve changed full-frame pixels, cutoff diagnostics or reset heads");
		});
		context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
		check(resolvePipeline(1,reset,family.sourceOver,family.dense,family.packed,family.modifiers).state!=large.state,
			"Completed small resolve was not selected");
		check(draw(&specializedGpu)==reference,"Asynchronous small resolve changed full-frame output");
		if(metalOffscreenResolveBenchmark)NOTICE_LOG(RENDERER,"Metal resolve sample width=%u height=%u family=%u reset=%d compile_us=%.3f admit_us=%.3f reference_gpu_ms=%.6f fallback_gpu_ms=%.6f specialized_gpu_ms=%.6f",width,height,(key&~511u)>>10,reset,compileUs,admitUs,referenceGpu,fallbackGpu,specializedGpu);
		++cases;
	}}
	}
	auto wanted=std::make_shared<std::atomic<bool>>(true);
	auto canceledCallbacks=std::make_shared<std::atomic<unsigned>>(0);
	unsigned accepted=0;
	const auto beforeCanceled=context()->pipelineStatisticsForTesting().misses;
	context()->withBlockedMaterialCompilerForTesting([&]{
		for(unsigned i=0;i<32;++i)accepted+=context()->makeComputePipelineAsync(@"framebuffer_finish",[canceledCallbacks](id<MTLComputePipelineState>,NSError *){++*canceledCallbacks;},[wanted]{return wanted->load();});
		check(accepted==16,"Optional compute jobs exceeded the shared compiler admission bound");
		const auto fallback=resolvePipeline(2);check(fallback.capacity>=2 && resolveAdmissionBlocked,"Full compiler did not defer optional resolve work");
		check(resolvePipelines.find(2)==resolvePipelines.end(),"Deferred resolve left a permanent pending marker");wanted->store(false);
	});
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(!canceledCallbacks->load() && context()->pipelineStatisticsForTesting().misses==beforeCanceled,"Canceled compute work compiled or published a result");
	resolvePipeline(2);context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(resolvePipeline(2).capacity==2,"Resolve admission did not recover after canceled jobs");
	const auto retired=pipelineResults;
	context()->withBlockedMaterialCompilerForTesting([&]{
		resolvePipeline(4);Term();check(Init(),"Renderer restart with optional compute jobs failed");
	});
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	{std::lock_guard<std::mutex> lock(retired->mutex);check(retired->resolves.empty(),"Canceled old renderer published compute results after restart");}
	check(pipelineResults!=retired && !retired->active && resolvePipelines.find(4)==resolvePipelines.end(),"Old resolve state crossed the renderer restart");
	context()->withBlockedMaterialCompilerForTesting([&]{
		resolvePipeline(8);check(resolvePipelines.count(8) && !resolvePipelines.at(8).state,"Init retry fixture missed its pending resolve");
		check(Init(),"Renderer Init retry failed");
		check(resolvePipelines.find(8)==resolvePipelines.end(),"Renderer Init retry retained a canceled resolve marker");
	});
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	resolvePipeline(8);context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(resolvePipeline(8).capacity==8,"Reinitialized renderer failed to retry its canceled specialization");
	// The modifier layout is optional before capture: rejection or cancellation
	// must keep the generic layout available without a foreground compile.
	constexpr u32 modifierFamily=(1u<<12)|(1u<<13);
	wanted->store(true);accepted=0;
	context()->withBlockedMaterialCompilerForTesting([&]{
		for(unsigned i=0;i<32;++i)accepted+=context()->makeComputePipelineAsync(@"framebuffer_finish",
			[canceledCallbacks](id<MTLComputePipelineState>,NSError *){++*canceledCallbacks;},[wanted]{return wanted->load();});
		check(accepted==16,"Packed modifier fixture did not fill the shared compiler queue");
		check(!packedModifierResolveReady(2,false) && resolveAdmissionBlocked
			&& !resolvePipelines.count(modifierFamily|2u),"Rejected packed modifier family changed capture layout or retained a marker");
		wanted->store(false);
	});
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(!canceledCallbacks->load(),"Canceled packed modifier setup published a result");
	check(!packedModifierResolveReady(2,false),"First optional modifier request compiled synchronously");
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(packedModifierResolveReady(2,false),"Packed modifier family did not retry after queue rejection");
	const auto retiredModifier=pipelineResults;
	context()->withBlockedMaterialCompilerForTesting([&]{
		check(!packedModifierResolveReady(8,true),"New modifier reset family unexpectedly had a ready pipeline");
		Term();check(Init(),"Renderer restart with optional modifier jobs failed");
	});
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	{std::lock_guard<std::mutex> lock(retiredModifier->mutex);check(retiredModifier->resolves.empty(),"Retired modifier request published after restart");}
	check(!retiredModifier->active && std::none_of(resolvePipelines.begin(),resolvePipelines.end(),
		[](const auto& entry){return (entry.first&(1u<<13))!=0;}),"Modifier pipeline state survived Term");
	context()->withBlockedMaterialCompilerForTesting([&]{
		check(!packedModifierResolveReady(8,true),"Modifier Init retry fixture did not queue its first request");
		check(Init() && !resolvePipelines.count(modifierFamily|512u|8u),"Init retry retained a canceled modifier marker");
	});
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(!packedModifierResolveReady(8,true),"Modifier retry unexpectedly skipped its asynchronous admission");
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(packedModifierResolveReady(8,true),"Modifier Init retry failed to recover the optional family");
	NOTICE_LOG(RENDERER,"Metal validation PASS: optional packed modifier admission, full shared queue, generic-layout fallback, rejection retry, retired owner cancellation, Term and Init retry");
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual OIT resolve admission, %u matching-layout/reset/size cases, %u exact full-frame fallback/specialization pairs with cutoff diagnostics and head resets, held background compiler, no foreground lookup, bounded queue cancellation, next-frame retry, renderer restart and Init retry",cases,cases*2);
}

void MetalRenderer::validateSyntheticStorageAdmission()
{
	const auto check=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen,oldNative=config::NativeDepthInterpolation;
	const auto oldBudget=config::PixelBufferSize.get();const int oldLayers=config::PerPixelLayers;
	const u32 oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	struct Restore {MetalRenderer *self;bool emulate,wide,native;int64_t budget;int layers;u32 read,spg;
		~Restore(){self->rc=nullptr;self->disableDenseOIT=self->disablePackedOIT=false;config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::NativeDepthInterpolation=native;config::PixelBufferSize=budget;config::PerPixelLayers=layers;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore{this,oldEmulate,oldWide,oldNative,oldBudget,oldLayers,oldRead,oldSpg};
	config::EmulateFramebuffer=false;config::Widescreen=false;config::PerPixelLayers=32;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;
	rend_context fixture{};rc=&fixture;fixture.clearFramebuffer=true;fixture.globClip={640,480};
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	fixture.verts.resize(12);
	for(unsigned q=0;q<3;++q)for(unsigned i=0;i<4;++i){
		auto& v=fixture.verts[q*4+i];v={};v.x=i&1?640:0;v.y=i&2?480:0;v.z=1.f+q*.5f;
		v.col[0]=u8(31+q*61);v.col[1]=u8(53+q*37);v.col[2]=u8(89+q*23);v.col[3]=q?u8(151-q*31):255;fixture.idx.push_back(q*4+i);
	}
	PolyParam p;p.init();p.count=4;p.pcw.Gouraud=1;p.isp.DepthMode=7;p.tsp.UseAlpha=1;p.tsp.SrcInstr=1;p.tsp.FogCtrl=2;
	fixture.global_param_op={p};p.first=4;p.isp.ZWriteDis=1;fixture.global_param_tr={p};p.first=8;fixture.global_param_tr.push_back(p);
	RenderPass pass{};pass.op_count=1;pass.tr_count=2;pass.autosort=true;fixture.render_passes={pass};
	u8 zero[256]{};fog.tex_type=TextureType::_8;fog.UploadToGPU(128,2,zero,false);palette.tex_type=TextureType::_8888;palette.UploadToGPU(1,1,zero,false);context()->flushTextureUpdates();context()->waitIdle();
	struct RestoreMeasure {bool& field;bool value;~RestoreMeasure(){field=value;}} restoreMeasure{measureOITStorage,measureOITStorage};
	measureOITStorage=true;
	unsigned cases=0,pairs=0,replanned=0;
	for(auto size:{std::array<u32,2>{16,12},{1920,1080},{3840,2160}})
	for(unsigned kind=0;kind<6;++kind)for(bool tiled:{false,true})for(bool native:{false,true}) { @autoreleasepool {
		if(size[0]>16 && (kind!=0 && kind!=3 || tiled))continue;
		fixture.framebufferWidth=size[0];fixture.framebufferHeight=size[1];config::NativeDepthInterpolation=native;
		config::PixelBufferSize=int64_t(size[0])*size[1]*(size[0]>16?48:tiled?10:96);
		const bool sourceOver=kind>=3;
		for(auto& poly:fixture.global_param_tr){poly.tsp.SrcInstr=4;poly.tsp.DstInstr=sourceOver?5:1;}
		disableDenseOIT=disablePackedOIT=true;check(Render(),"Storage admission generic image failed");std::vector<u8> reference;context()->readTexture(screen,reference);
		context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
		// Keep only the baseline linked families. Every optimized layout starts
		// absent here, even if an earlier case already compiled its descriptor.
		for(auto entry=resolvePipelines.begin();entry!=resolvePipelines.end();)
			if((entry->first&~511u)!=0 && (entry->first&~511u)!=512u
				&& (entry->first&~511u)!=1024u && (entry->first&~511u)!=1536u)entry=resolvePipelines.erase(entry);else ++entry;
		disableDenseOIT=kind==2 || kind==4 || kind==5;disablePackedOIT=kind==1 || kind==5;
		const u32 wanted=kind==0?MetalOITDense|MetalOITDensePacked:kind==1?MetalOITDense:kind==2 || kind==4?MetalOITPackedList:kind==3?MetalOITDense:0;
		const auto before=context()->testComputePipelineRequestCount();
		u64 pendingTiles=0;
		context()->withBlockedMaterialCompilerForTesting([&]{
			measuredOITTiles=0;
			check(Render(),"Storage admission pending capture failed");std::vector<u8> actual;context()->readTexture(screen,actual);
			check(actual==reference,"Pending optional storage changed a complete image");
			check(static_cast<const u32 *>(counter.contents)[1]==0,"Pending storage overflowed or truncated a list");
			pendingTiles=measuredOITTiles;
			const auto requests=context()->testComputePipelineRequestCount()-before;
			NOTICE_LOG(RENDERER,"Metal storage admission pending width=%u kind=%u native=%d sync_requests=%llu",size[0],kind,native,(unsigned long long)requests);
			check(requests==0,"First optional OIT storage family compiled synchronously during capture");
			check(!(drawState.uniforms.flags&(MetalOITDense|MetalOITPackedList)),"Unready OIT layout escaped generic capture fallback");++pairs;
		});
		context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
		measuredOITTiles=0;
		check(Render(),"Storage admission completed capture failed");std::vector<u8> actual;context()->readTexture(screen,actual);
		check(actual==reference,"Completed optional storage changed a complete image");
		check(static_cast<const u32 *>(counter.contents)[1]==0,"Completed optional storage overflowed or truncated a list");
		if(size[0]>16){check(pendingTiles>1 && measuredOITTiles==1,"Pending storage did not replan full nodes/head reset before returning to untiled dense capture");++replanned;}
		check((drawState.uniforms.flags&wanted)==wanted,"Completed optional OIT storage was never selected");++pairs;++cases;
	}}
	fixture.framebufferWidth=16;fixture.framebufferHeight=12;config::PixelBufferSize=16*12*96;
	for(auto& poly:fixture.global_param_tr){poly.tsp.SrcInstr=4;poly.tsp.DstInstr=1;}
	disableDenseOIT=disablePackedOIT=true;check(Render(),"Storage queue control failed");
	std::vector<u8> queueReference;context()->readTexture(screen,queueReference);context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	for(auto entry=resolvePipelines.begin();entry!=resolvePipelines.end();)
		if(entry->first&~2047u)entry=resolvePipelines.erase(entry);else ++entry;
	disableDenseOIT=disablePackedOIT=false;
	auto wanted=std::make_shared<std::atomic<bool>>(true);auto callbacks=std::make_shared<std::atomic<unsigned>>(0);
	const auto queueBefore=context()->testComputePipelineRequestCount();unsigned accepted=0;
	context()->withBlockedMaterialCompilerForTesting([&]{
		for(unsigned i=0;i<32;++i)accepted+=context()->makeComputePipelineAsync(@"framebuffer_finish",
			[callbacks](id<MTLComputePipelineState>,NSError *){++*callbacks;},[wanted]{return wanted->load();});
		check(accepted==16,"Storage admission fixture did not fill the shared compiler queue");
		check(Render(),"Full compiler storage fallback failed");std::vector<u8> actual;context()->readTexture(screen,actual);
		check(actual==queueReference && resolveAdmissionBlocked && !resolvePipelines.count(4u|(1u<<11)|(1u<<12)),
			"Full compiler changed capture pixels or retained a rejected storage marker");
		check(context()->testComputePipelineRequestCount()==queueBefore,"Full compiler forced a synchronous storage fallback");wanted->store(false);
	});
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();check(!callbacks->load(),"Canceled storage setup published a compute result");
	check(Render(),"Deferred storage request did not retry");std::vector<u8> retry;context()->readTexture(screen,retry);check(retry==queueReference,"Re-admitted pending storage changed pixels");
	context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
	check(Render(),"Completed deferred storage request failed");context()->readTexture(screen,retry);
	check(retry==queueReference && (drawState.uniforms.flags&MetalOITDense),"Deferred storage never selected its completed optimized layout");
	check(context()->testComputePipelineRequestCount()==queueBefore,"Storage retry performed foreground compute creation");
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual storage capture under full16-job compiler queue, rejected-marker cleanup, cancellation and next-frame retry, three complete image controls, zero synchronous compute requests");
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual OIT storage admission, %u cases and %u full-image generic/pending/ready pairs through 4K, six layout choices, tiled/untiled and both depth modes, held compiler, no draw-side synchronous compute request, optimized capture selected after completion, %u high-resolution tile/reset replans",cases,pairs,replanned);
}
