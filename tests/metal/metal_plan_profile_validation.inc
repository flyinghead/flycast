// SPDX-License-Identifier: GPL-2.0-or-later
// Production OIT planning/encoding with a linear reference retaining material
// reuse. No replacement rasterizer, window, parser input or game is involved.
void MetalRenderer::validateSyntheticPlanProfile()
{
	const auto require=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const bool timing=metalOffscreenPlanBenchmark;
	if(timing)for(const char *name:{"MTL_DEBUG_LAYER","MTL_SHADER_VALIDATION"}){
		const char *v=std::getenv(name);require(!v || std::string(v)=="0","Planning timing requires validation disabled");
	}
	metalOffscreenBeginAllocations();void *probe=::operator new(17);const auto observed=metalOffscreenEndAllocations();::operator delete(probe);
	require(observed.calls==1 && observed.bytes==17,"Planning allocation observer is not active");
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen,oldNative=config::NativeDepthInterpolation;
	const int oldLayers=config::PerPixelLayers;const int64_t oldBudget=config::PixelBufferSize;const float oldScale=config::ExtraDepthScale;
	const u32 oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	struct Restore{bool emulate,wide,native;int layers;int64_t budget;float scale;u32 read,spg;
		~Restore(){metalOffscreenClearCommandHook();config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::NativeDepthInterpolation=native;config::PerPixelLayers=layers;config::PixelBufferSize=budget;config::ExtraDepthScale=scale;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore{oldEmulate,oldWide,oldNative,oldLayers,oldBudget,oldScale,oldRead,oldSpg};
	config::EmulateFramebuffer=false;config::Widescreen=false;config::PerPixelLayers=128;config::ExtraDepthScale=1;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;
	rend_context fixture{};fixture.clearFramebuffer=true;fixture.globClip={640,480};
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	MetalRenderer reference(true);require(reference.Init(),"Planning reference initialization failed");
	struct Stop{MetalRenderer& renderer;~Stop(){renderer.Term();}} stop{reference};
	for(auto *n:{&reference,this}){
		n->rc=&fixture;n->disableBackgroundClear=true;n->disableDenseOIT=n->disablePackedOIT=true;n->measureOITPlan=true;
		u8 zero[256]{};n->fog.tex_type=TextureType::_8;n->fog.UploadToGPU(128,2,zero,false);n->palette.tex_type=TextureType::_8888;n->palette.UploadToGPU(1,1,zero,false);
	}
	reference.legacyOITPlanning=true;context()->flushTextureUpdates();context()->waitIdle();
	const char *names[]={"localized","mixed-wide","parser-tombstones","restarts","Naomi2-fallback","extreme-degenerate","multipass-reuse","short-fallback","empty","sparse-hierarchy"};
	const auto setup=[&](unsigned kind,u32 width,u32 height,unsigned tint=0){
		fixture.framebufferWidth=width;fixture.framebufferHeight=height;fixture.verts.clear();fixture.idx.clear();
		fixture.global_param_op.clear();fixture.global_param_tr.clear();fixture.render_passes.clear();fixture.matrices.clear();
		config::PixelBufferSize=int64_t(width)*height*20*((kind==4 || kind==5)?4:1)/((kind==4 || kind==5)?1:2);
		if(kind==9)config::PixelBufferSize=int64_t(width)*height*20/128;
		const auto quad=[&](float x,float y,float w,float h,float z,unsigned poly,bool opaque){
			const u32 base=fixture.verts.size();
			for(unsigned j=0;j<4;++j){Vertex v{};v.x=x+(j&1?w:0);v.y=y+(j&2?h:0);v.z=z;v.nz=1;
				v.col[0]=opaque?11:u8(31+poly*17+tint*11);v.col[1]=opaque?23:u8(193-poly*13);v.col[2]=opaque?47:u8(71+poly*7);v.col[3]=opaque?255:111;
				fixture.verts.push_back(v);fixture.idx.push_back(base+j);}
			return base;
		};
		quad(0,0,640,480,1,0,true);PolyParam bg;bg.init();bg.count=4;bg.pcw.Gouraud=1;bg.isp.DepthMode=7;bg.tsp.SrcInstr=1;bg.tsp.FogCtrl=2;fixture.global_param_op={bg};
		PolyParam p=bg;p.isp.ZWriteDis=1;p.tsp.UseAlpha=1;p.tsp.SrcInstr=4;p.tsp.DstInstr=5;
		if(kind==4){
			// Naomi2 projection produces TA screen coordinates; the shared
			// production frame matrix performs their later NDC conversion.
			fixture.matrices.resize(2);glm::mat4 identity(1),projection(1);projection[2][3]=-1;projection[3][3]=0;
			memcpy(fixture.matrices[0].mat,glm::value_ptr(identity),sizeof(N2Matrix));memcpy(fixture.matrices[1].mat,glm::value_ptr(projection),sizeof(N2Matrix));
			p.mvMatrix=p.normalMatrix=0;p.projMatrix=1;p.constantColor[0]=1;
		}
		const unsigned polys=kind==7?8:kind==8?0:kind==9?256:64;
		for(unsigned i=0;i<polys;++i){
			p.first=fixture.idx.size();p.count=4;const bool wide=kind==1 && i<8;
			const unsigned cell=kind==9?(i*37)%256:i%16;
			const float x=kind==9?float(cell%16)*40+8:float(cell%4)*160+8,y=kind==9?float(cell/16)*30+8:float(cell/4)*120+8;
			const u32 base=quad(wide?0:x,wide?0:y,wide?640:kind==9?24:136,wide?480:kind==9?14:96,kind==4?-1:2.f+(i/16)*.25f,i,false);
			if(kind==3){fixture.idx.insert(fixture.idx.end(),{~0u,base,base+1,base+2,base+3});p.count=9;}
			// Finite native-depth positions, but beyond the CPU cancellation
			// proof's safety margin: the degenerate tail must stay unbounded.
			if(kind==5){Vertex v=fixture.verts[base];v.x=v.y=1.e30f;v.z=1.e-10f;const u32 degenerate=fixture.verts.size();fixture.verts.push_back(v);fixture.idx.insert(fixture.idx.end(),{~0u,degenerate,degenerate,degenerate});p.count=8;}
			fixture.global_param_tr.push_back(p);
			if(kind==2 || kind==6)for(unsigned empty=0;empty<7;++empty){auto e=p;e.count=empty%3;e.first=~0u;fixture.global_param_tr.push_back(e);}
		}
		if(kind==8){auto empty=p;empty.count=0;empty.first=~0u;fixture.global_param_tr={empty};}
		RenderPass pass{};pass.op_count=1;pass.tr_count=fixture.global_param_tr.size();pass.autosort=true;fixture.render_passes={pass};
		if(kind==6){const auto count=fixture.global_param_tr.size();for(size_t i=0;i<count;++i)fixture.global_param_tr.push_back(fixture.global_param_tr[i]);pass.tr_count=fixture.global_param_tr.size();fixture.render_passes.push_back(pass);}
	};
	struct Sample{double cpu=0,plan=0,encode=0,gpu=0;u64 candidates=0,draws=0,preparations=0,tiles=0,indexed=0,hierarchy=0,allocations=0,bytes=0,prepared=0;};
	const auto render=[&](MetalRenderer& n,std::vector<u8>& pixels,bool count){
		n.countOITPlan=count;n.measuredOITCandidates=n.measuredOITIndexedPasses=n.measuredOITHierarchyPasses=0;
		n.measuredOITPlanAllocations=n.measuredOITPlanAllocatedBytes=n.measuredOITPreparedPeak=0;
		n.measuredOITTiles=0;n.measuredOITPlanningUs=n.measuredOITDrawEncodingUs=0;n.oitPolygonDraws=n.oitPolygonPreparations=0;
		for(auto& r:n.frameResources)if(r.counter)std::memset(r.counter.contents,0,8);
		metalOffscreenArmCommand(MetalOffscreenFault::None,nil);const auto begin=std::chrono::steady_clock::now();
		require(n.Render(),"Planning renderer failed");auto command=metalOffscreenLastCommand();require(command,"Planning command was not captured");context()->flushRenderCommands();const auto submitted=std::chrono::steady_clock::now();[command waitUntilCompleted];
		require(command.status==FCMetal4StatusCompleted && !command.error,"Planning GPU work failed");
		require(!n.counter || static_cast<const u32 *>(n.counter.contents)[1]==0,"Planning exceeded fragment capacity");
		Sample s;s.cpu=std::chrono::duration<double,std::micro>(submitted-begin).count();s.plan=n.measuredOITPlanningUs;s.encode=n.measuredOITDrawEncodingUs;s.gpu=(command.GPUEndTime-command.GPUStartTime)*1000;
		s.candidates=n.measuredOITCandidates;s.draws=n.oitPolygonDraws;s.preparations=n.oitPolygonPreparations;s.tiles=n.measuredOITTiles;s.indexed=n.measuredOITIndexedPasses;s.hierarchy=n.measuredOITHierarchyPasses;s.allocations=n.measuredOITPlanAllocations;s.bytes=n.measuredOITPlanAllocatedBytes;s.prepared=n.measuredOITPreparedPeak;
		if(timing)require(command.GPUStartTime>0 && s.gpu>0,"Planning native feedback unavailable");
		context()->readTexture(n.screen,pixels);require(pixels.size()==size_t(fixture.framebufferWidth)*fixture.framebufferHeight*4,"Planning full image size differs");
		n.countOITPlan=false;metalOffscreenClearCommandHook();return s;
	};
	const auto median=[](std::vector<double> x){std::sort(x.begin(),x.end());return x[x.size()/2];};
	unsigned cases=0,pairs=0,warmPairs=0,indexedCases=0,hierarchyCases=0;
	for(auto size:{std::array<u32,2>{320,240},{1920,1080},{3840,2160}})for(bool native:{false,true})for(unsigned kind=0;kind<10;++kind){
		if(timing && kind!=0 && kind!=1 && kind!=6 && kind!=9)continue;
		config::NativeDepthInterpolation=native;setup(kind,size[0],size[1]);
		std::array<std::vector<u8>,2> pixels;std::array<Sample,2> cold,counts,last;
		cold[0]=render(reference,pixels[0],true);cold[1]=render(*this,pixels[1],true);require(pixels[0]==pixels[1],"Cold spatial planning changed full-frame pixels");
		std::array<std::vector<double>,2> cpu,plan,encode,gpu;const int rounds=timing?31:1;
		for(int round=-4;round<rounds;++round){
			for(unsigned turn=0;turn<2;++turn){const unsigned v=turn^(unsigned(round+4)&1);last[v]=render(v==0?reference:*this,pixels[v],false);if(round>=0){cpu[v].push_back(last[v].cpu);plan[v].push_back(last[v].plan);encode[v].push_back(last[v].encode);gpu[v].push_back(last[v].gpu);}}
			require(pixels[0]==pixels[1],"Spatial planning changed full-frame pixels");require(last[0].tiles==last[1].tiles && last[0].draws==last[1].draws,"Spatial planning changed tile/draw sequence length");
			if(round<0)++warmPairs;else ++pairs;
		}
		counts[0]=render(reference,pixels[0],true);counts[1]=render(*this,pixels[1],true);require(pixels[0]==pixels[1],"Counted spatial planning changed full-frame pixels");
		require(counts[0].allocations==0 && counts[1].allocations==0,"Warm planning allocated C++ workspace");
		if(kind==7 || kind==8)require(counts[1].indexed==0,"Small/empty planning did not keep its fallback");
		else require(counts[1].indexed>0,"Spatial fixture did not exercise the real draw index");
		if(kind==9)require(counts[1].hierarchy>0,"Spatial fixture did not exercise the recursive tile hierarchy");
		if(kind==2 || kind==6)require(counts[1].prepared<=64 && counts[0].prepared>=512,"Parser tombstones still consume material records");
		require(counts[0].draws==counts[1].draws && counts[0].preparations==counts[1].preparations,"Spatial index changed existing material reuse");
		require(counts[1].candidates<=counts[0].candidates,"Spatial index increased visited candidates");
		if(kind!=8){bool colored=false;for(size_t i=0;i<pixels[1].size();i+=4)if(pixels[1][i]!=11 || pixels[1][i+1]!=23 || pixels[1][i+2]!=47){colored=true;break;}require(colored,"Spatial scene has no visible translucent geometry");}
		indexedCases+=counts[1].indexed>0;hierarchyCases+=counts[1].hierarchy>0;++cases;
		NOTICE_LOG(RENDERER,"Metal plan profile size=%ux%u native=%d case=%s pairs=%d tiles=%llu draws=%llu preparations=%llu candidates=%llu/%llu indexed=%llu hierarchy=%llu material_records=%llu/%llu first_allocations=%llu/%llu first_bytes=%llu/%llu warm_allocations=%llu/%llu cpu_us=%.3f/%.3f plan_us=%.3f/%.3f encode_us=%.3f/%.3f gpu_ms=%.6f/%.6f",size[0],size[1],native,names[kind],rounds,counts[1].tiles,counts[1].draws,counts[1].preparations,counts[0].candidates,counts[1].candidates,counts[1].indexed,counts[1].hierarchy,counts[0].prepared,counts[1].prepared,cold[0].allocations,cold[1].allocations,cold[0].bytes,cold[1].bytes,counts[0].allocations,counts[1].allocations,median(cpu[0]),median(cpu[1]),median(plan[0]),median(plan[1]),median(encode[0]),median(encode[1]),median(gpu[0]),median(gpu[1]));
	}
	unsigned gated=0;
	if(!timing)for(bool native:{false,true}){
		config::NativeDepthInterpolation=native;std::array<std::vector<u8>,3> expected;
		const auto select=[&](unsigned i){config::ExtraDepthScale=i==0?1.f:i==1?64.f:.25f;setup(i==0?0:i==1?6:3,i==0?320:i==1?1920:640,i==0?240:i==1?1080:480,i+1);};
		for(unsigned i=0;i<3;++i){select(i);render(reference,expected[i],false);}context()->waitIdle();
		auto event=[context()->device() newSharedEvent];struct Open{id<MTLSharedEvent> event;~Open(){event.signaledValue=1;}} open{event};
		auto gate=[context()->queue() commandBuffer];[gate waitForEvent:event value:1];context()->submitCommandBuffer(gate);
		std::array<id<MTLTexture>,3> snapshots;std::array<FCMetal4Commands *,3> commands,copies;std::array<size_t,3> slots;
		for(unsigned i=0;i<3;++i){select(i);metalOffscreenArmCommand(MetalOffscreenFault::None,nil);require(Render(),"Gated planning render failed");commands[i]=metalOffscreenLastCommand();slots[i]=currentFrameResource;context()->flushRenderCommands();
			snapshots[i]=context()->makeTexture(fixture.framebufferWidth,fixture.framebufferHeight,MTLPixelFormatRGBA8Unorm);copies[i]=[context()->queue() commandBuffer];auto copy=[copies[i] transferEncoder];
			[copy copyFromTexture:screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(screen.width,screen.height,1) toTexture:snapshots[i] destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[copy endEncoding];context()->submitCommandBuffer(copies[i]);metalOffscreenClearCommandHook();}
		require(slots[0]!=slots[1] && slots[0]!=slots[2] && slots[1]!=slots[2],"Planning lifetime did not occupy three distinct slots");
		for(unsigned i=0;i<3;++i){require(commands[i].status==FCMetal4StatusCommitted && copies[i].status==FCMetal4StatusCommitted,"Planning work completed before gate");const bool free=dispatch_semaphore_wait(frameResources[slots[i]].available,DISPATCH_TIME_NOW)==0;if(free)dispatch_semaphore_signal(frameResources[slots[i]].available);require(!free,"Planning slot reused before feedback");}
		event.signaledValue=1;context()->waitIdle();
		for(unsigned i=0;i<3;++i){[commands[i] waitUntilCompleted];[copies[i] waitUntilCompleted];require(!commands[i].error && !copies[i].error,"Gated planning command failed");std::vector<u8> actual;context()->readTexture(snapshots[i],actual);require(actual==expected[i],"Planning workspace reuse changed queued full image");++gated;}
		for(auto slot:slots){const bool first=dispatch_semaphore_wait(frameResources[slot].available,DISPATCH_TIME_NOW)==0,second=dispatch_semaphore_wait(frameResources[slot].available,DISPATCH_TIME_NOW)==0;if(second)dispatch_semaphore_signal(frameResources[slot].available);if(first)dispatch_semaphore_signal(frameResources[slot].available);require(first && !second,"Planning slot completion was not exactly once");}
	}
	rc=nullptr;measureOITPlan=countOITPlan=disableBackgroundClear=disableDenseOIT=disablePackedOIT=false;
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual spatial OIT planning, %u cases (%u indexed/%u hierarchy), %u measured and %u warmup exact full-image pairs, zero warm C++ planning allocations, %u GPU-gated changed workload/projection snapshots (%s)",cases,indexedCases,hierarchyCases,pairs,warmPairs,gated,timing?"timing without validation":"API/shader validation");
}
