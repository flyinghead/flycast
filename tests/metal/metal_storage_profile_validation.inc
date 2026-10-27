// SPDX-License-Identifier: GPL-2.0-or-later
// Actual general-list OIT: historical split storage vs shared-budget layout.
// Timing excludes readback. Allocation sizes are not physical DRAM traffic.
void MetalRenderer::validateSyntheticStorageProfile()
{
	const auto require=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const bool timing=metalOffscreenStorageBenchmark;
	const bool boundaryProfile=metalOffscreenBoundaryProfile;
	const bool passProfile=metalOffscreenPassProfile;
	const bool packedProfile=metalOffscreenPackedModifiers;
	if(timing)for(const char *name:{"MTL_DEBUG_LAYER","MTL_SHADER_VALIDATION"}) {
		const char *value=std::getenv(name);
		require(!value || std::string(value)=="0","Storage timing requires API/shader validation disabled");
	}
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen,oldNative=config::NativeDepthInterpolation,oldModifiers=config::ModifierVolumes;
	const auto oldBudget=config::PixelBufferSize.get();const int oldLayers=config::PerPixelLayers;
	const u32 oldShade=FPU_SHAD_SCALE.full,oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	struct Restore {
		bool emulate,wide,native,modifiers;int64_t budget;int layers;u32 shade,read,spg;
		~Restore(){config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::NativeDepthInterpolation=native;config::ModifierVolumes=modifiers;config::PixelBufferSize=budget;config::PerPixelLayers=layers;FPU_SHAD_SCALE.full=shade;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore{oldEmulate,oldWide,oldNative,oldModifiers,oldBudget,oldLayers,oldShade,oldRead,oldSpg};
	config::EmulateFramebuffer=false;config::Widescreen=false;config::ModifierVolumes=true;config::PerPixelLayers=32;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;FPU_SHAD_SCALE.scale_factor=128;
	disableDenseOIT=true;disablePackedOIT=!packedProfile;measureOITStorage=true;
	rend_context fixture{};rc=&fixture;fixture.clearFramebuffer=true;fixture.globClip={640,480};
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	fixture.verts.resize(20);for(unsigned q=0;q<5;++q)for(unsigned i=0;i<4;++i) {
		auto& v=fixture.verts[q*4+i];v={};v.x=i&1?640:0;v.y=i&2?480:0;
		v.z=q==0?1.f:q==4?3.f:1.5f;
		const u8 color[4]={u8(23+q*31),u8(41+q*17),u8(73+q*13),u8(q?111:255)};
		const u8 area[4]={u8(211-q*23),u8(173-q*11),u8(37+q*19),u8(147-q*7)};
		std::copy_n(color,4,v.col);std::copy_n(area,4,v.col1);fixture.idx.push_back(q*4+i);
	}
	PolyParam opaque;opaque.init();opaque.count=4;opaque.pcw.Gouraud=1;opaque.isp.DepthMode=7;opaque.tsp.SrcInstr=1;opaque.tsp.FogCtrl=2;
	fixture.global_param_op={opaque};
	for(unsigned q=1;q<5;++q){auto p=opaque;p.first=q*4;p.pcw.Shadow=1;p.isp.ZWriteDis=1;p.tsp.UseAlpha=1;p.tsp.SrcInstr=4;p.tsp.DstInstr=5;fixture.global_param_tr.push_back(p);}
	fixture.modtrig={{0,0,4,320,0,4,0,480,4},{320,0,4,320,480,4,0,480,4}};
	ModifierVolumeParam modifier;modifier.init();modifier.count=2;modifier.isp.DepthMode=1;modifier.isp.VolumeLast=1;
	fixture.global_param_mvo_tr={modifier};RenderPass pass{};pass.op_count=1;pass.tr_count=4;pass.mvo_tr_count=1;pass.autosort=true;fixture.render_passes={pass};
	u8 zero[256]{};fog.tex_type=TextureType::_8;fog.UploadToGPU(128,2,zero,false);palette.tex_type=TextureType::_8888;palette.UploadToGPU(1,1,zero,false);
	context()->flushTextureUpdates();context()->waitIdle();
	// Separate renderer instances keep both storage interpretations warm.
	// Alternating one instance would reallocate nodes on every sample and
	// measure layout switching instead of normal steady-frame encoding.
	config::PixelBufferSize=1_MB;MetalRenderer reference(true);
	require(reference.Init(),"Storage reference renderer initialization failed");
	struct StopReference {MetalRenderer& renderer;~StopReference(){renderer.Term();}} stopReference{reference};
	reference.rc=&fixture;reference.disableDenseOIT=reference.disablePackedOIT=true;
	reference.legacyOITStorage=!boundaryProfile && !packedProfile;reference.measureOITStorage=true;
	reference.fog.tex_type=TextureType::_8;reference.fog.UploadToGPU(128,2,zero,false);
	reference.palette.tex_type=TextureType::_8888;reference.palette.UploadToGPU(1,1,zero,false);context()->flushTextureUpdates();
	const auto ownedBytes=[&](const MetalRenderer& n) {
		std::unordered_set<void *> seen;uint64_t bytes=0;
		const auto add=[&](id<MTLResource> resource){
			if([resource conformsToProtocol:@protocol(MTLTexture)]){
				id<MTLTexture> image=(id<MTLTexture>)resource;while(image.parentTexture)image=image.parentTexture;
				resource=image.buffer?(id<MTLResource>)image.buffer:image;
			}
			require(!resource.heap,"Storage profile cannot attribute an imported heap allocation");
			if(resource && seen.insert((__bridge void *)resource).second)bytes+=resource.allocatedSize;
		};
		for(id<MTLBuffer> resource:{n.vertices,n.indices,n.modifiers,n.lights,n.nodes,n.oitColors1,n.heads,n.counter,n.oitPolygons,n.frameUniforms,n.naomi2,n.emptyNaomi2,n.emptyLights})add(resource);
		for(id<MTLTexture> resource:{n.screen,n.screenDepth,n.screenSecondary,n.secondaryColor,n.depth,n.shadowDepth,n.shadowStencil,n.emptyStencil,n.target,n.white,n.fog.image,n.palette.image,n.framebuffer.image})add(resource);
		const auto scratch=[&](const GPUIntermediates& images){
			for(auto image:{images.shadowDepth,images.shadowStencil,images.rasterColor})add(image);
			for(auto image:images.resolveScratch)add(image);
		};
		scratch(n.gpuIntermediates);
		for(const auto& r:n.frameResources){
			for(id<MTLBuffer> resource:{r.counter,r.vertices,r.indices,r.modifiers,r.lights,r.oitPolygons,r.naomi2,r.frameUniforms})add(resource);
			for(id<MTLTexture> resource:{r.secondaryColor,r.screenOutput,r.rttReadbackOutput,r.framebufferScale})add(resource);
			scratch(r.legacyIntermediates);for(auto page:r.polyUniformPages)add(page);
		}
		return bytes;
	};
	struct Sample {double cpu=0,plan=0,gpu=0;u64 tiles=0,draws=0,preparations=0,capacity=0,nodeBytes=0,storageAllocated=0,owned=0,device=0,consumers=0,producers=0,narrow=0,compact=0;};
	const auto render=[&](MetalRenderer& n,bool area1,std::vector<u8>& pixels) {
		n.measuredOITTiles=0;n.measuredOITPlanningUs=0;n.oitPolygonDraws=n.oitPolygonPreparations=0;
		// Prior iteration completed before these shared diagnostic words change.
		for(auto& r:n.frameResources)if(r.counter)std::memset(r.counter.contents,0,8);
		const auto compactBefore=n.packedModifierPreparations;
		metalOffscreenArmCommand(MetalOffscreenFault::None,nil,boundaryProfile && !passProfile && &n==&reference,!boundaryProfile || (passProfile && &n!=&reference));const auto begin=std::chrono::steady_clock::now();
		require(n.Render(),"Storage profile render failed");const auto encoded=std::chrono::steady_clock::now();auto command=metalOffscreenLastCommand();
		require(command,"Storage profile missed the actual render command");context()->flushRenderCommands();const auto submitted=std::chrono::steady_clock::now();[command waitUntilCompleted];
		require(command.status==FCMetal4StatusCompleted && !command.error,"Storage profile native render failed");
		require(n.passStates.size()==1 && n.oitModifiers && !n.denseOIT && !n.packedListOIT,"Storage fixture did not execute the general OIT modifier path");
		require(bool(n.oitColors1)==area1,"Storage fixture did not select the intended area-1 allocation");
		require(static_cast<const u32 *>(n.counter.contents)[1]==0,"Storage profile overflowed or truncated a fragment list");
		Sample sample;sample.cpu=std::chrono::duration<double,std::micro>((boundaryProfile?submitted:encoded)-begin).count();sample.plan=n.measuredOITPlanningUs;
		const auto synchronization=[command synchronizationStatistics];sample.consumers=[synchronization[@"queueConsumers"] unsignedLongLongValue];sample.producers=[synchronization[@"queueProducers"] unsignedLongLongValue];
		sample.narrow=[synchronization[@"queueNarrowConsumers"] unsignedLongLongValue];
		sample.compact=n.packedModifierPreparations-compactBefore;
		sample.gpu=(command.GPUEndTime-command.GPUStartTime)*1000;
		if(timing)require(command.GPUStartTime>0 && sample.gpu>0,"Storage timing feedback is unavailable");
		sample.tiles=n.measuredOITTiles;sample.draws=n.oitPolygonDraws;sample.preparations=n.oitPolygonPreparations;sample.capacity=n.frame.capacity;
		sample.nodeBytes=n.nodes.length+(n.oitColors1 && n.oitColors1!=n.nodes?n.oitColors1.length:0);
		sample.storageAllocated=n.nodes.allocatedSize+(n.oitColors1 && n.oitColors1!=n.nodes?n.oitColors1.allocatedSize:0);
		sample.owned=ownedBytes(n);sample.device=context()->device().currentAllocatedSize;
		context()->readTexture(n.screen,pixels);require(pixels.size()==size_t(n.frame.width)*n.frame.height*4,"Storage profile readback dimensions differ");
		const auto left=(size_t(n.frame.height/2)*n.frame.width+n.frame.width/4)*4,right=(size_t(n.frame.height/2)*n.frame.width+n.frame.width*3/4)*4;
		require(std::memcmp(pixels.data()+left,pixels.data()+right,4)!=0,"Storage modifier fixture did not change its two image halves");
		if(packedProfile){context()->drainMaterialCompilerForTesting();n.collectMaterialPipelines();}
		metalOffscreenClearCommandHook();return sample;
	};
	const auto median=[](std::vector<double> values){std::sort(values.begin(),values.end());return values[values.size()/2];};
	unsigned untiledPairs=0,proofFallbackPairs=0;
	if(packedProfile) {
		fixture.framebufferWidth=8;fixture.framebufferHeight=6;config::PixelBufferSize=1_MB;
		for(bool area1:{false,true})for(bool native:{false,true}) {
			config::NativeDepthInterpolation=native;
			for(auto& poly:fixture.global_param_tr){poly.tsp1.full=~0u;poly.tcw1.full=~0u;if(area1){poly.tsp1=poly.tsp;poly.tcw1.full=0;}}
			std::vector<u8> warm,actual,baseline;
			render(*this,area1,warm);
			const auto compact=render(*this,area1,actual),general=render(reference,area1,baseline);
			require(actual==baseline && compact.compact>0 && !general.compact && compact.tiles==1 && general.tiles==1,
				"Untiled packed modifier family failed its exact general-layout control");
			++untiledPairs;
		}
		// Equal geometry with vertex IDs which cannot carry polygon identity:
		// unsigned subtraction below the base and carry out of the low 17 bits.
		const auto originalVertices=fixture.verts;
		const auto originalIndices=fixture.idx;
		for(bool carry:{false,true}) {
			fixture.verts=originalVertices;fixture.idx=originalIndices;
			const u32 first=carry ? (1u<<17)+4u : 128u;
			fixture.verts.resize(first+4);
			for(u32 i=0;i<(carry?4u:1u);++i){
				const u32 position=carry?16u+i:4u;
				fixture.verts[first+i]=originalVertices[originalIndices[position]];
				fixture.idx[position]=first+i;
			}
			require(!sequenceCarriesPolygon(0,4),"Packed modifier fixture failed to invalidate its sequence proof");
			for(bool area1:{false,true})for(bool native:{false,true}) {
				config::NativeDepthInterpolation=native;
				for(auto& poly:fixture.global_param_tr){poly.tsp1.full=~0u;poly.tcw1.full=~0u;if(area1){poly.tsp1=poly.tsp;poly.tcw1.full=0;}}
				std::vector<u8> actual,baseline;
				const auto fallback=render(*this,area1,actual),general=render(reference,area1,baseline);
				require(!fallback.compact && !general.compact && actual==baseline,
					"Invalid sequence proof selected packed modifier nodes or changed the general image");
				++proofFallbackPairs;
			}
		}
		fixture.verts=originalVertices;fixture.idx=originalIndices;
	}
	unsigned pairs=0,warmPairs=0,cases=0;u64 reduced=0;
	for(auto size:{std::array<u32,2>{320,240},{1280,720},{1920,1080},{3840,2160}})for(u32 factor:{10,22,30})for(bool area1:{false,true})for(bool native:{false,true}) {
		fixture.framebufferWidth=size[0];fixture.framebufferHeight=size[1];config::NativeDepthInterpolation=native;
		config::PixelBufferSize=int64_t(size[0])*size[1]*factor;
		for(auto& poly:fixture.global_param_tr){poly.tsp1.full=~0u;poly.tcw1.full=~0u;if(area1){poly.tsp1=poly.tsp;poly.tcw1.full=0;}}
		if(packedProfile && cases==0) {
			const auto before=context()->testComputePipelineRequestCount();const auto compactBefore=packedModifierPreparations;
			context()->withBlockedMaterialCompilerForTesting([&]{
				require(Render(),"Packed modifier admission fixture failed to render its generic fallback");
				context()->flushRenderCommands();std::vector<u8> fallback;context()->readTexture(screen,fallback);
				require(packedModifierPreparations==compactBefore,"Pending packed modifier compiler changed the captured node layout");
				require(context()->testComputePipelineRequestCount()==before,
					"Optional packed modifier family compiled synchronously on the foreground");
			});
			context()->drainMaterialCompilerForTesting();collectMaterialPipelines();
		}
		std::vector<double> cpu[2],plan[2],gpu[2];Sample peak[2],last[2];
		const int rounds=timing?31:1,warmup=4;
		for(int round=-warmup;round<rounds;++round) {
			std::vector<u8> images[2];
			for(unsigned turn=0;turn<2;++turn){const unsigned variant=turn^(unsigned(round+warmup)&1);last[variant]=render(variant==0?reference:*this,area1,images[variant]);
				if(round>=0){cpu[variant].push_back(last[variant].cpu);plan[variant].push_back(last[variant].plan);gpu[variant].push_back(last[variant].gpu);
					peak[variant].storageAllocated=std::max(peak[variant].storageAllocated,last[variant].storageAllocated);
					peak[variant].owned=std::max(peak[variant].owned,last[variant].owned);peak[variant].device=std::max(peak[variant].device,last[variant].device);}}
			require(images[0]==images[1],"Shared OIT storage changed full-image modifier/sort-tie pixels");
			const auto budget=oitNodeBytes();require(last[1].nodeBytes==budget && last[0].nodeBytes<=budget,"Storage profile exceeded the common configured budget");
			require(last[1].capacity==metal_oit::storageLayout(budget,area1).capacity && last[0].capacity==((boundaryProfile||packedProfile)?last[1].capacity:budget/24),"Storage profile capacity differs from its physical layout");
			if(packedProfile && round>=0)require(last[1].compact>0 && last[0].compact==0
				&& last[0].tiles==last[1].tiles && last[0].draws==last[1].draws && last[0].nodeBytes==last[1].nodeBytes,
				"Packed modifier comparison changed capacity/geometry or failed to execute both real layouts");
			if(boundaryProfile) {
				require(last[0].tiles==last[1].tiles && last[0].draws==last[1].draws && last[0].nodeBytes==last[1].nodeBytes,"Synchronization profile changed geometry/storage");
				if(passProfile)require(last[0].narrow==0 && last[1].narrow>0,"Pass profile did not select resource-specific consumer stages");
				else require(last[1].producers==1 && last[0].producers>last[1].producers,"Boundary profile failed to fuse producer barriers");
			}
			require(last[1].tiles<=last[0].tiles,"Shared storage increased conservative tile work");
			if(area1)require(last[1].capacity==last[0].capacity && last[1].tiles==last[0].tiles,"Area-1 control changed its effective capacity");
			if(round>=0)++pairs;else ++warmPairs;
		}
		if(last[1].tiles<last[0].tiles)++reduced;++cases;
		NOTICE_LOG(RENDERER,"Metal %s profile size=%ux%u factor=%u area1=%d native=%d pairs=%d budget=%llu capacity=%llu/%llu tiles=%llu/%llu draws=%llu/%llu preparations=%llu/%llu node_bytes=%llu/%llu storage_allocated_peak=%llu/%llu renderer_owned_peak=%llu/%llu device_observed_peak=%llu/%llu consumers=%llu/%llu producers=%llu/%llu narrow=%llu/%llu cpu_us=%.3f/%.3f plan_us=%.3f/%.3f gpu_ms=%.6f/%.6f compact_preparations=%llu/%llu",
			packedProfile?"packed-modifier":passProfile?"pass":boundaryProfile?"boundary":"storage",size[0],size[1],factor,area1,native,rounds,(unsigned long long)oitNodeBytes(),last[0].capacity,last[1].capacity,last[0].tiles,last[1].tiles,last[0].draws,last[1].draws,last[0].preparations,last[1].preparations,last[0].nodeBytes,last[1].nodeBytes,peak[0].storageAllocated,peak[1].storageAllocated,peak[0].owned,peak[1].owned,peak[0].device,peak[1].device,last[0].consumers,last[1].consumers,last[0].producers,last[1].producers,last[0].narrow,last[1].narrow,median(cpu[0]),median(cpu[1]),median(plan[0]),median(plan[1]),median(gpu[0]),median(gpu[1]),last[0].compact,last[1].compact);
	}
	legacyOITStorage=measureOITStorage=false;legacyOITColors=nil;disableDenseOIT=disablePackedOIT=false;rc=nullptr;
	if(packedProfile)NOTICE_LOG(RENDERER,"Metal validation PASS: actual general/packed modifier layouts, %u cases, %u measured and %u warmup full-image pairs through 4K, %u extra untiled pairs, %u invalid-sequence generic fallback pairs, both depth/area modes, identical node allocation/capacity/tiles/draws, held compiler preserves generic foreground fallback, exact full images (%s)",cases,pairs,warmPairs,untiledPairs,proofFallbackPairs,timing?"31 alternating timing pairs without validation":"correctness with validation");
	else if(passProfile)NOTICE_LOG(RENDERER,"Metal validation PASS: actual OIT resource dependency graph, %u cases, %u measured and %u warmup full-image pairs through 4K, both depth/area modes, identical nodes/tiles/draws, scoped consumers and exact full images (%s)",cases,pairs,warmPairs,timing?"31-pair timing without validation; CPU includes commit":"correctness with validation");
	else if(boundaryProfile)NOTICE_LOG(RENDERER,"Metal validation PASS: actual OIT encoder boundary policies, %u cases, %u measured and %u warmup full-image pairs through 4K, both depth/area modes, identical nodes/tiles/draws, fewer producer barriers and exact full images (%s)",cases,pairs,warmPairs,timing?"31-pair timing without validation; CPU includes commit":"correctness with validation");
	else NOTICE_LOG(RENDERER,"Metal validation PASS: actual general OIT shared/split storage, %u cases, %u measured and %u warmup full-image pairs from 320x240 through 4K; both depth/area modes, modifiers, equal-depth sort keys, bounded diagnostics and allocation/capacity/tile checks; %llu cases reduce tile work (%s)",cases,pairs,warmPairs,reduced,timing?"31-pair timing without validation":"correctness with validation");
}
