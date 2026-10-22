// SPDX-License-Identifier: GPL-2.0-or-later
// Actual direct/Color0/punch-through consumers. Count vertex reads in separate
// untimed renders; timings use the normal, uninstrumented production evaluator.
void MetalRenderer::validateSyntheticProofProfile()
{
	const auto require=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const bool timing=metalOffscreenProofBenchmark;
	if(timing)for(const char *name:{"MTL_DEBUG_LAYER","MTL_SHADER_VALIDATION"}) {
		const char *value=std::getenv(name);require(!value || std::string(value)=="0","Proof timing requires validation disabled");
	}
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen,oldNative=config::NativeDepthInterpolation,oldSort=config::PerStripSorting;
	const int oldFilter=config::TextureFiltering,oldAniso=config::AnisotropicFiltering;
	const float oldScale=config::ExtraDepthScale;const int64_t oldBudget=config::PixelBufferSize;
	const u32 oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	struct Restore {
		bool emulate,wide,native,sort;int filter,aniso;float scale;int64_t budget;u32 read,spg;
		~Restore(){metalOffscreenClearCommandHook();config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::NativeDepthInterpolation=native;config::PerStripSorting=sort;config::TextureFiltering=filter;config::AnisotropicFiltering=aniso;config::ExtraDepthScale=scale;config::PixelBufferSize=budget;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore{oldEmulate,oldWide,oldNative,oldSort,oldFilter,oldAniso,oldScale,oldBudget,oldRead,oldSpg};
	config::EmulateFramebuffer=false;config::Widescreen=false;config::PerStripSorting=true;
	config::TextureFiltering=0;config::AnisotropicFiltering=1;config::ExtraDepthScale=1;config::PixelBufferSize=1_MB;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;
	rend_context fixture{};fixture.clearFramebuffer=true;fixture.globClip={640,480};
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	MetalTexture texture;texture.tex_type=TextureType::_8888;std::array<u8,8*8*4> texels{};
	for(unsigned i=0;i<64;++i){texels[i*4]=u8(31+i*13);texels[i*4+1]=u8(73+i*7);texels[i*4+2]=u8(151-i);texels[i*4+3]=255;}
	texture.UploadToGPU(8,8,texels.data(),true);context()->flushTextureUpdates();
	MetalRenderer reference(perPixel),generic(perPixel);
	require(reference.Init() && generic.Init(),"Proof reference initialization failed");
	struct Stop{MetalRenderer& a;MetalRenderer& b;~Stop(){a.Term();b.Term();}} stop{reference,generic};
	for(auto *n:{&reference,this,&generic}) {
		n->rc=&fixture;n->disableBackgroundClear=true;
		u8 zero[256]{};n->fog.tex_type=TextureType::_8;n->fog.UploadToGPU(128,2,zero,false);n->palette.tex_type=TextureType::_8888;n->palette.UploadToGPU(1,1,zero,false);
	}
	reference.disableProofCache=true;generic.disableDepthBounds=generic.disableColourUV=true;
	context()->flushTextureUpdates();context()->waitIdle();
	const char *names[]={"occluded-repeat-long","textured-repeat-long","unique-long","unique-short","punch-through-restarts","invalid-UV-degenerate","sorted-three-index","Naomi2-bypass","no-consumer","UV-only-boundary"};
	const auto setup=[&](unsigned kind,u32 width,u32 height,unsigned tint=0) {
		fixture.framebufferWidth=width;fixture.framebufferHeight=height;fixture.verts.clear();fixture.idx.clear();
		fixture.global_param_op.clear();fixture.global_param_pt.clear();fixture.global_param_tr.clear();fixture.sortedTriangles.clear();fixture.render_passes.clear();fixture.matrices.clear();
		config::PerStripSorting=kind!=6;
		for(auto *n:{&reference,this,&generic})n->disableDenseOIT=perPixel && kind==4;
		const auto quad=[&](float x,float y,float w,float h,float z,bool background) {
			const u32 base=fixture.verts.size();
			for(unsigned i=0;i<4;++i){Vertex v{};v.x=x+(i&1?w:0);v.y=y+(i&2?h:0);v.z=z;
				v.u=i&1?.875f:.0625f;v.v=i&2?.9375f:.125f;v.nz=1;
				v.col[0]=background?11:u8(101+tint*17);v.col[1]=background?23:u8(173-tint*13);v.col[2]=background?47:u8(61+tint*19);v.col[3]=255;
				fixture.verts.push_back(v);}
			return base;
		};
		quad(0,0,640,480,kind==0?4.f:1.f,true);fixture.idx={0,1,2,3};
		const u32 base=quad(240,180,160,120,kind==7?-1.f:2.f,false);
		PolyParam bg;bg.init();bg.count=4;bg.pcw.Gouraud=1;bg.isp.DepthMode=7;bg.tsp.SrcInstr=1;bg.tsp.FogCtrl=2;fixture.global_param_op.push_back(bg);
		PolyParam p=bg;p.isp.DepthMode=kind==8 || kind==9?7:6;
		if(kind==1 || kind==4 || kind==5 || kind==9){p.pcw.Texture=1;p.texture=&texture;p.tsp.FilterMode=1;p.tsp.IgnoreTexA=1;p.tsp.ShadInstr=3;}
		if(kind==9)for(unsigned i=0;i<4;++i){fixture.verts[base+i].u=i&1?4.f:-4.f;fixture.verts[base+i].v=i&2?3.9375f:-3.875f;}
		if(kind==7){
			fixture.matrices.resize(2);glm::mat4 identity(1),projection(1);projection[2][3]=-1;projection[3][3]=0;
			memcpy(fixture.matrices[0].mat,glm::value_ptr(identity),sizeof(N2Matrix));memcpy(fixture.matrices[1].mat,glm::value_ptr(projection),sizeof(N2Matrix));
			p.mvMatrix=p.normalMatrix=0;p.projMatrix=1;p.constantColor[0]=1;
		}
		const bool unique=kind==2 || kind==3 || (perPixel && kind==4);
		const u32 count=kind==3 || kind==6 || kind==7 || kind==8?4:1024;
		u32 range=0;
		for(unsigned poly=0;poly<64;++poly) {
			if(!poly || unique) {
				range=fixture.idx.size();for(u32 i=0;i<count;++i)fixture.idx.push_back(i<4?base+i:base+3);
				if(kind==4 || kind==5) {
					fixture.idx[range+4]=~0u;
					if(kind==5){Vertex bad=fixture.verts[base+3];bad.u=width==320?std::numeric_limits<float>::quiet_NaN():width==1920?std::numeric_limits<float>::infinity():std::nextafter(4.f,5.f);const u32 index=fixture.verts.size();fixture.verts.push_back(bad);for(u32 i=5;i<count;++i)fixture.idx[range+i]=index;}
				}
			}
			p.first=range;p.count=count;
			if(kind==4)fixture.global_param_pt.push_back(p);
			else if(kind==6){fixture.global_param_tr.push_back(p);fixture.sortedTriangles.push_back({poly,range,3});}
			else fixture.global_param_op.push_back(p);
		}
		RenderPass pass{};pass.op_count=fixture.global_param_op.size();pass.pt_count=fixture.global_param_pt.size();
		pass.tr_count=fixture.global_param_tr.size();pass.sorted_tr_count=fixture.sortedTriangles.size();pass.autosort=kind==6;fixture.render_passes={pass};
		if(perPixel && kind==4) {
			// Two real sorted logical passes retain the independent shadow-depth
			// prepass. Each unique PT range is therefore shaded in depth and
			// Color0, exercising natural proof reuse rather than duplicate polys.
			fixture.render_passes.clear();
			for(unsigned i=0;i<2;++i){const u32 v=quad(16+i*16,16,8,8,.5f,false);auto tr=bg;tr.first=fixture.idx.size();tr.count=4;tr.isp.DepthMode=6;tr.isp.ZWriteDis=1;tr.tsp.UseAlpha=1;tr.tsp.SrcInstr=4;tr.tsp.DstInstr=5;
				for(unsigned j=0;j<4;++j){fixture.idx.push_back(v+j);fixture.verts[v+j].col[3]=0;}fixture.global_param_tr.push_back(tr);pass.tr_count=i+1;pass.autosort=true;fixture.render_passes.push_back(pass);}
		}
	};
	struct Sample {double cpu=0,gpu=0;u64 evaluations=0,vertices=0,hits=0,cacheEvaluations=0;float bound=0;u32 flags=0;};
	const auto render=[&](MetalRenderer& n,std::vector<u8>& pixels,bool count) {
		n.measureProofWork=count;n.measuredProofEvaluations=n.measuredProofVertices=0;
		metalOffscreenArmCommand(MetalOffscreenFault::None,nil);const auto begin=std::chrono::steady_clock::now();
		require(n.Render(),"Proof profile actual render failed");auto command=metalOffscreenLastCommand();require(command,"Proof profile missed render command");
		context()->flushRenderCommands();const auto submitted=std::chrono::steady_clock::now();[command waitUntilCompleted];
		require(command.status==FCMetal4StatusCompleted && !command.error,"Proof profile native work failed");
		Sample s;s.cpu=std::chrono::duration<double,std::micro>(submitted-begin).count();s.gpu=(command.GPUEndTime-command.GPUStartTime)*1000;
		s.evaluations=n.measuredProofEvaluations;s.vertices=n.measuredProofVertices;s.hits=n.geometryProofs.statistics.hits;s.cacheEvaluations=n.geometryProofs.statistics.evaluations;s.bound=n.drawState.uniforms.depthBound;s.flags=n.drawState.uniforms.flags;
		if(timing)require(command.GPUStartTime>0 && s.gpu>0,"Proof GPU feedback timing unavailable");
		context()->readTexture(n.screen,pixels);require(pixels.size()==size_t(fixture.framebufferWidth)*fixture.framebufferHeight*4,"Proof profile image size differs");
		n.measureProofWork=false;metalOffscreenClearCommandHook();return s;
	};
	const auto compare=[&](const std::array<std::vector<u8>,3>& pixels) {
		require(pixels[0]==pixels[1],"Proof cache changed full-frame pixels");unsigned difference=0;
		require(pixels[0].size()==pixels[2].size(),"Proof generic image size differs");
		for(size_t i=0;i<pixels[0].size();++i){const unsigned d=std::abs(int(pixels[0][i])-int(pixels[2][i]));require(d<=(i%4==3?0:1),"Proof specialization exceeds existing one-byte RGB tolerance");difference=std::max(difference,d);}
		return difference;
	};
	const auto median=[](std::vector<double> x){std::sort(x.begin(),x.end());return x[x.size()/2];};
	unsigned cases=0,frames=0,warmFrames=0,maxGenericDifference=0;
	for(auto size:{std::array<u32,2>{320,240},{1920,1080},{3840,2160}})for(bool native:{false,true})for(unsigned kind=0;kind<10;++kind) {
		if((perPixel && kind==6) || (timing && kind>4 && kind!=9))continue;
		config::NativeDepthInterpolation=native;setup(kind,size[0],size[1]);
		std::array<Sample,3> counts;std::array<std::vector<u8>,3> pixels;
		unsigned variant=0;for(auto *n:{&reference,this,&generic})counts[variant]=render(*n,pixels[variant],true),++variant;
		maxGenericDifference=std::max(maxGenericDifference,compare(pixels));
		if(kind==2 || kind==3)require(counts[0].vertices==counts[1].vertices && counts[1].hits==0,"Unique proof ranges reused another range");
		else if(kind==6)require(counts[1].cacheEvaluations==0 && counts[1].hits==0 && counts[0].vertices==counts[1].vertices,"Short sorted triangles unexpectedly use proof cache");
		else if(kind==7 || kind==8 || (kind==9 && native))require(counts[0].vertices==0 && counts[1].vertices==0 && counts[1].hits==0,"Bypassed proof consumer scanned vertices");
		else require(counts[1].vertices<counts[0].vertices && counts[1].hits>0,"Repeated actual proof range was not reused");
		require(counts[2].vertices==0,"Disabled specialization still evaluates proofs");
		if(kind==5 || native)require(!(counts[1].flags&ColourUV),"Invalid/native-depth UV proof unexpectedly selected relaxed coordinates");
		if((kind==1 || kind==9) && !native)require(counts[1].flags&ColourUV,"Valid textured proof fixture did not execute colour-UV specialization");
		if(kind==9)require(counts[1].bound==0,"UV-only consumer introduced an unused depth bound");
		// All variants use the same completed specialized pipeline families
		// before measuring. Counting passes and compiler waits are untimed.
		for(auto *n:{&reference,this,&generic}) {
			const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
			for(;;){n->collectMaterialPipelines();if(std::all_of(n->materialPipelines.begin(),n->materialPipelines.end(),[](const auto& p){return p.second!=nil;}))break;require(std::chrono::steady_clock::now()<deadline,"Proof material compilation did not complete");[NSThread sleepForTimeInterval:.001];}
		}
		std::array<std::vector<double>,3> cpu,gpu;const int rounds=timing?31:1;
		for(int round=-4;round<rounds;++round) {
			for(unsigned turn=0;turn<3;++turn){const unsigned v=(turn+unsigned(round+4))%3;MetalRenderer *n=v==0?&reference:v==1?this:&generic;const auto s=render(*n,pixels[v],false);if(round>=0){cpu[v].push_back(s.cpu);gpu[v].push_back(s.gpu);}}
			maxGenericDifference=std::max(maxGenericDifference,compare(pixels));if(round<0)warmFrames+=3;else frames+=3;
		}
		NOTICE_LOG(RENDERER,"Metal proof profile mode=%s size=%ux%u native=%d case=%s rounds=%d scans=%llu/%llu/%llu vertex_reads=%llu/%llu/%llu hits=%llu/%llu/%llu cpu_us=%.3f/%.3f/%.3f gpu_ms=%.6f/%.6f/%.6f",
			perPixel?"OIT":"standard",size[0],size[1],native,names[kind],rounds,counts[0].evaluations,counts[1].evaluations,counts[2].evaluations,counts[0].vertices,counts[1].vertices,counts[2].vertices,counts[0].hits,counts[1].hits,counts[2].hits,median(cpu[0]),median(cpu[1]),median(cpu[2]),median(gpu[0]),median(gpu[1]),median(gpu[2]));++cases;
	}
	// Same index ranges, changed vertices/projection and three busy real slots.
	unsigned gatedImages=0;
	if(!timing)for(bool native:{false,true}) {
		config::NativeDepthInterpolation=native;std::array<std::vector<u8>,3> expected;std::array<Sample,3> direct;
		const auto select=[&](unsigned i){
			config::ExtraDepthScale=i==0?1.f:i==1?64.f:.25f;setup(1,i==0?320:i==1?640:1920,i==0?240:i==1?480:1080,i);
			for(size_t v=4;v<fixture.verts.size();++v)fixture.verts[v].z=i==0?2.f:i==1?4.f:.75f;
			if(i==1){Vertex degenerate=fixture.verts.back();degenerate.z=16;const u32 index=fixture.verts.size();fixture.verts.push_back(degenerate);for(size_t j=8;j<fixture.idx.size();++j)fixture.idx[j]=index;}
		};
		for(unsigned i=0;i<3;++i){select(i);direct[i]=render(reference,expected[i],true);}context()->waitIdle();
		auto event=[context()->device() newSharedEvent];struct Open{id<MTLSharedEvent> event;~Open(){event.signaledValue=1;}} open{event};
		auto gate=[context()->queue() commandBuffer];[gate waitForEvent:event value:1];context()->submitCommandBuffer(gate);
		std::array<id<MTLTexture>,3> snapshots;std::array<FCMetal4Commands *,3> commands,copies;std::array<size_t,3> slots;
		for(unsigned i=0;i<3;++i) {
			select(i);metalOffscreenArmCommand(MetalOffscreenFault::None,nil);require(Render(),"Gated proof snapshot render failed");commands[i]=metalOffscreenLastCommand();slots[i]=currentFrameResource;
			require(geometryProofs.statistics.evaluations==1 && geometryProofs.statistics.hits==63,"Gated proof cache did not reset at the new render");
			require(memcmp(&drawState.uniforms.depthBound,&direct[i].bound,sizeof(float))==0,"Gated proof retained old vertex/projection bits");context()->flushRenderCommands();
			snapshots[i]=context()->makeTexture(fixture.framebufferWidth,fixture.framebufferHeight,MTLPixelFormatRGBA8Unorm);copies[i]=[context()->queue() commandBuffer];auto copy=[copies[i] transferEncoder];
			[copy copyFromTexture:screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(screen.width,screen.height,1) toTexture:snapshots[i] destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[copy endEncoding];context()->submitCommandBuffer(copies[i]);metalOffscreenClearCommandHook();
		}
		require(slots[0]!=slots[1] && slots[0]!=slots[2] && slots[1]!=slots[2],"Gated proof renders did not use three distinct real slots");
		for(unsigned i=0;i<3;++i){require(commands[i].status==FCMetal4StatusCommitted && copies[i].status==FCMetal4StatusCommitted,"Proof snapshot completed before GPU gate");const bool free=dispatch_semaphore_wait(frameResources[slots[i]].available,DISPATCH_TIME_NOW)==0;if(free)dispatch_semaphore_signal(frameResources[slots[i]].available);require(!free,"Proof frame slot reused before GPU completion");}
		event.signaledValue=1;context()->waitIdle();
		for(unsigned i=0;i<3;++i){[commands[i] waitUntilCompleted];[copies[i] waitUntilCompleted];require(!commands[i].error && !copies[i].error,"Gated proof native work failed");std::vector<u8> actual;context()->readTexture(snapshots[i],actual);require(actual==expected[i],"Changed in-flight vertex/projection snapshot altered the full image");++gatedImages;}
		for(auto slot:slots){const bool first=dispatch_semaphore_wait(frameResources[slot].available,DISPATCH_TIME_NOW)==0,second=dispatch_semaphore_wait(frameResources[slot].available,DISPATCH_TIME_NOW)==0;if(second)dispatch_semaphore_signal(frameResources[slot].available);if(first)dispatch_semaphore_signal(frameResources[slot].available);require(first && !second,"Proof slot release was not exactly once");}
	}
	disableBackgroundClear=false;measureProofWork=false;rc=nullptr;
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual proof consumers %s, %u cases, %u measured and %u warmup frames, cache full images exact, uncached/cached/disabled-specialization costs, untimed actual vertex reads, generic max RGB difference %u, %u exact GPU-gated changed vertex/projection snapshots (%s)",perPixel?"OIT":"standard",cases,frames,warmFrames,maxGenericDifference,gatedImages,timing?"timing without validation":"API/shader validation");
}
