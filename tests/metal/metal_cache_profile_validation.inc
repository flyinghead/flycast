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
