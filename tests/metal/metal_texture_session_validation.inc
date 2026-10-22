// SPDX-License-Identifier: GPL-2.0-or-later
// Actual hostfs catalog, source loader, WorkerThread, renderer preload map,
// cache adoption and session cleanup. Files live only in the isolated CLI root.
void MetalRenderer::validateSyntheticTextureSessions()
{
	using Color=std::array<u8,4>;
	const auto oldGame=settings.content.gameId;const bool oldCustom=config::CustomTextures,oldNative=config::NativeDepthInterpolation;
	const bool oldEmulate=config::EmulateFramebuffer,oldWide=config::Widescreen;const int oldPreload=config::PreloadCustomTextures;
	const int oldFilter=config::TextureFiltering,oldAnisotropy=config::AnisotropicFiltering;
	const u32 oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;Renderer *oldRenderer=renderer;
	struct Restore {
		std::string game;bool custom,native,emulate,wide;int preload,filter,anisotropy;u32 read,spg;Renderer *previous;
		~Restore(){custom_texture.terminate();settings.content.gameId=game;config::CustomTextures=custom;config::NativeDepthInterpolation=native;config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::PreloadCustomTextures=preload;config::TextureFiltering=filter;config::AnisotropicFiltering=anisotropy;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;renderer=previous;}
	} restore{oldGame,oldCustom,oldNative,oldEmulate,oldWide,oldPreload,oldFilter,oldAnisotropy,oldRead,oldSpg,oldRenderer};
	custom_texture.terminate();renderer=this;processGpuCleanupOperations();context()->waitIdle();
	config::CustomTextures=false;config::TextureFiltering=0;config::AnisotropicFiltering=1;
	config::EmulateFramebuffer=false;config::Widescreen=false;FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;
	const auto caps=MetalTexture::customCapabilities();
	const auto require=[](bool value,const char *message){if(!value)throw RendererException(message);};
	const auto wait=[&](auto condition,const char *message,bool pump=false) {
		const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(30);
		while(!condition()) {
			if(std::chrono::steady_clock::now()>until)throw RendererException(message);
			if(pump)rend_process_custom_texture_preloads();
			std::this_thread::yield();
		}
	};
	struct Entry {TCW tcw;u32 hash;std::vector<u8> a,b;};std::vector<Entry> entries;
	TSP tsp{};tsp.FogCtrl=2;tsp.UseAlpha=1;tsp.SrcInstr=1;tsp.MipMapD=4;
	for(unsigned i=0;i<12;++i) {
		TCW tcw{};tcw.PixelFmt=Pixel4444;tcw.ScanOrder=1;tcw.TexAddr=(0x680000+i*4096)/8;
		for(unsigned pixel=0;pixel<64;++pixel){const u16 value=u16(0xf000|(i*113+pixel*7));std::memcpy(&vram[(tcw.TexAddr*8)+pixel*2],&value,2);}
		auto *texture=static_cast<MetalTexture *>(GetTexture(tsp,tcw,0));require(texture,"Original VRAM texture load failed");texture->ComputeHash();
		Entry entry{tcw,texture->texture_hash,std::vector<u8>(256),std::vector<u8>(256)};
		for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)for(unsigned c=0;c<4;++c) {
			entry.a[(y*8+x)*4+c]=c==3?u8(64+i*12):u8(20+i*7+x*11+y*13+c*31);
			entry.b[(y*8+x)*4+c]=c==3?u8(224-i*8):u8(231-i*9-x*7-y*5-c*23);
		}
		for(const auto& old:entries)require(old.hash!=entry.hash,"Session fixture texture hashes collide");entries.push_back(std::move(entry));
	}
	const std::string gameA="METAL_TEST_SESSION_A",gameB="METAL_TEST_SESSION_B";
	for(bool second:{false,true}) {
		const auto dir=std::filesystem::path(metalOffscreenCacheRoot)/"textures"/(second?gameB:gameA);std::filesystem::remove_all(dir);std::filesystem::create_directories(dir/"nested");
		for(const auto& entry:entries){char name[24];std::snprintf(name,sizeof(name),"%08x.png",entry.hash);const auto path=dir/"nested"/name;require(stbi_write_png(path.c_str(),8,8,4,(second?entry.b:entry.a).data(),32)!=0,"Cannot write isolated PNG fixture");}
		// Same-hash PNG must not override the preferred compressed source.
		std::filesystem::create_symlink(std::filesystem::path(FLYCAST_METAL_TEST_FIXTURE_ROOT)/"t4x4.ktx2",dir/"fffffffe.xuastc.ktx2");
		require(stbi_write_png((dir/"fffffffe.png").c_str(),8,8,4,entries[0].a.data(),32)!=0,"Cannot write source-priority PNG");
	}
	rend_context fixture{};rc=&fixture;fixture.clearFramebuffer=true;fixture.globClip={640,480};fixture.framebufferWidth=8;fixture.framebufferHeight=8;
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	fixture.verts.resize(4);fixture.idx={0,1,2,3};
	for(unsigned i=0;i<4;++i){auto& v=fixture.verts[i];v={};v.x=i&1?640:0;v.y=i&2?480:0;v.z=1;v.u=i&1?1:0;v.v=i&2?1:0;std::fill_n(v.col,4,255);}
	PolyParam p;p.init();p.count=4;p.pcw.Gouraud=p.pcw.Texture=1;p.isp.DepthMode=7;p.tsp=tsp;p.tsp.ClampU=p.tsp.ClampV=1;
	fixture.global_param_op={p};RenderPass pass{};pass.op_count=1;fixture.render_passes={pass};
	u8 zeros[256]{};fog.tex_type=TextureType::_8;fog.UploadToGPU(128,2,zeros,false);palette.tex_type=TextureType::_8888;palette.UploadToGPU(1,1,zeros,false);
	// The entries above existed only to compute real VRAM hashes. Start
	// package loading with the same empty cache as a renderer cold start.
	clearTextureCache();
	unsigned images=0,adoptions=0,sourceReads=0;
	const auto check=[&](MetalTexture *texture,const std::vector<u8>& source,bool second) {
		fixture.global_param_op[0].texture=texture;config::NativeDepthInterpolation=second;
		require(Render(),"Session texture render failed");std::vector<u8> pixels;context()->readTexture(screen,pixels);
		require(pixels.size()==256,"Session texture image size differs");
		for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)for(unsigned c=0;c<4;++c)
			if(pixels[(y*8+x)*4+c]!=source[((7-y)*8+x)*4+c])throw RendererException("Session PNG pixels or vertical orientation differ");
		++images;
	};
	const auto stop=[&] {custom_texture.terminate();rend_process_custom_texture_preloads();context()->waitIdle();require(gpuPreloadedTextures.empty(),"Session cleanup retained GPU preload entries");};
	const auto start=[&](const std::string& game,config::CustomTexturePreloadMode mode) {
		settings.content.gameId=game;config::CustomTextures=true;config::PreloadCustomTextures=int(mode);custom_texture.init();
		wait([&]{return custom_texture.hasReplacement(entries[0].hash) && custom_texture.hasReplacement(0xfffffffe);},"Actual package catalog did not become available");
	};
	const auto demand=[&](const Entry& entry,bool second) {
		auto *texture=static_cast<MetalTexture *>(GetTexture(tsp,entry.tcw,0));require(texture,"Demand texture lookup failed");
		wait([&]{return texture->is_custom_replaced || texture->IsCustomTextureAvailable();},"Worker did not complete the actual PNG demand request");
		require(GetTexture(tsp,entry.tcw,0)==texture && texture->is_custom_replaced,"Completed PNG request was not adopted");
		check(texture,second?entry.b:entry.a,second);++sourceReads;
		return texture;
	};
	const auto finishPreload=[&] {
		wait([&]{return !custom_texture.isPreloading();},"Real GPU preload source/bounded queue did not finish",true);
		rend_process_custom_texture_preloads();context()->flushTextureUpdates();
		int completed,total;size_t bytes;custom_texture.getPreloadProgress(completed,total,bytes);
		require(completed==13 && total==13,"Catalog/preload progress did not count unique hashes");return bytes;
	};
	start(gameA,config::CustomTexturePreloadMode::Off);
	for(const auto& entry:entries)demand(entry,false);
	require(gpuPreloadedTextures.empty(),"Demand-only session created speculative GPU entries");
	{
		struct Gate {std::atomic<bool> entered{false},canceled{false};};
		struct BlockingSource final:BaseCustomTextureSource {
			u32 hash;std::shared_ptr<Gate> gate;
			BlockingSource(u32 hash,std::shared_ptr<Gate> gate):hash(hash),gate(std::move(gate)){}
			bool shouldReplace()const override{return true;}bool loadMap(const CustomTextureCapabilities&)override{return true;}
			bool isTextureReplaced(u32 value)const override{return value==hash;}
			PreparedCustomTexture::Ptr loadCustomTexture(u32 value,const CustomTextureCapabilities&,const CancellationCheck& canceled)override {
				if(value!=hash)return {};gate->entered=true;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);
				while(!canceled()){if(std::chrono::steady_clock::now()>end)throw FlycastException("Worker cancellation fixture timed out");std::this_thread::yield();}
				gate->canceled=true;return {};
			}
		};
		std::array<std::shared_ptr<Gate>,2> gates{std::make_shared<Gate>(),std::make_shared<Gate>()};
		for(unsigned i=0;i<2;++i)custom_texture.addSource(std::make_unique<BlockingSource>(0xf0300001+i,gates[i]));
		for(bool backendChange:{false,true}) {
			const u32 hash=backendChange?0xf0300002:0xf0300001;auto gate=gates[backendChange];
			MetalTexture request;request.texture_hash=hash;request.old_texture_hash=request.old_vqtexture_hash=0;custom_texture.loadCustomTextureAsync(&request);
			const auto id=request.customRequestId;require(bool(id),"Held request was not submitted");wait([&]{return gate->entered.load();},"Held source did not run on the real worker");
			if(backendChange){custom_texture.setCapabilities(CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Unknown));require(custom_texture.needsRefresh(),"In-flight backend change did not invalidate the active session");}
			else custom_texture.cancelRequest(id);
			wait([&]{return gate->canceled.load();},"Real worker did not observe request/session cancellation");
			require(!custom_texture.isRequestComplete(id) && !request.IsCustomTextureAvailable(),"Canceled worker result remained adoptable");request.Delete();
			if(backendChange){custom_texture.setCapabilities(caps);custom_texture.refresh();processGpuCleanupOperations();wait([&]{return custom_texture.hasReplacement(entries[0].hash);},"Catalog did not recover after in-flight backend cancellation");}
		}
	}
	stop();
	start(gameA,config::CustomTexturePreloadMode::VideoMemory);const auto bytes=finishPreload();
	require(bytes>12*256 && gpuPreloadedTextures.size()==13,"Actual GPU-preload map or byte accounting incomplete");
	for(const auto& entry:entries) {
		const auto resident=findGpuPreloadedTexture(entry.hash,0,0);require(bool(resident),"GPU-preloaded PNG missing from renderer map");
		MetalTexture probe;require(probe.useGpuPreloadedTexture(resident),"Actual preloaded image is not usable");
		const auto uploads=context()->testTextureUploadCount();auto *texture=static_cast<MetalTexture *>(GetTexture(tsp,entry.tcw,0));
		require(texture && texture->usingGpuPreloadedTexture && texture->sharedPreloadImage && texture->image==probe.image,"PNG cache did not adopt the prepared GPU image");
		require(context()->testTextureUploadCount()==uploads,"Adopting an existing GPU preload uploaded it again");
		GetTexture(tsp,entry.tcw,0);require(context()->testTextureUploadCount()==uploads,"Repeated GPU preload lookup reuploaded pixels");
		check(texture,entry.a,false);++adoptions;
	}
	{
		MetalTexture probe;require(probe.useGpuPreloadedTexture(findGpuPreloadedTexture(0xfffffffe,0,0)),"Compressed catalog image missing");
		require(probe.image.width==1024 && probe.image.pixelFormat==MTLPixelFormatASTC_4x4_LDR,"Actual source priority/native ASTC selection changed");
	}
	// Execute the real renderer restart contract while a GPU-preload source
	// session is active. Init establishes Metal's format capabilities again.
	custom_texture.invalidateGpuPreloads();require(custom_texture.needsRefresh(),"Active GPU session was not invalidated before renderer restart");
	Term();require(Init(),"Metal renderer restart failed");rc=&fixture;custom_texture.refresh();
	finishPreload();require(gpuPreloadedTextures.size()==13,"Renderer restart did not repopulate GPU preloads");
	stop();
	// A backend capability change joins the old loader and restarts the real
	// catalog/transcoder. This keeps rendering offscreen on Metal while using
	// the shared RGBA-only contract, without starting another renderer API.
	start(gameA,config::CustomTexturePreloadMode::Off);
	custom_texture.setCapabilities(CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Unknown));
	require(custom_texture.needsRefresh(),"Active format/backend change did not invalidate the loader");custom_texture.refresh();
	processGpuCleanupOperations();wait([&]{return custom_texture.hasReplacement(entries[0].hash);},"RGBA-only catalog refresh failed");
	demand(entries[0],false);
	{
		MetalTexture fallback;fallback.texture_hash=0xfffffffe;fallback.old_texture_hash=fallback.old_vqtexture_hash=0;
		custom_texture.loadCustomTextureAsync(&fallback);
		wait([&]{return fallback.IsCustomTextureAvailable();},"Worker did not transcode the compressed source under RGBA-only capabilities");
		require(fallback.CheckCustomTexture() && fallback.image.width==1024 && fallback.image.pixelFormat==MTLPixelFormatRGBA8Unorm,"Actual compressed source did not take its RGBA-only fallback");fallback.Delete();
	}
	custom_texture.setCapabilities(caps);custom_texture.refresh();processGpuCleanupOperations();
	wait([&]{return custom_texture.hasReplacement(entries[0].hash);},"Metal capability restoration failed");demand(entries[1],false);
	stop();
	// Reserve the existing device admission budget without allocating VRAM.
	// This exercises denial through the actual loader and Metal controller.
	auto budget=metal_preload::deviceBudget(context()->device());require(budget->usage().bytes==0,"Preload images survived complete session cleanup/drain");
	auto pressure=budget->reserve(budget->byteLimit);require(bool(pressure),"Cannot reserve admission pressure for test");
	start(gameA,config::CustomTexturePreloadMode::VideoMemory);require(finishPreload()==0 && gpuPreloadedTextures.empty() && gpuPreloadOnDemand,"Admission denial did not defer all speculative entries");
	demand(entries[2],false);require(!static_cast<MetalTexture *>(GetTexture(tsp,entries[2].tcw,0))->sharedPreloadImage,"Denied preload bypassed the demand-upload path");
	pressure.reset();require(budget->usage().bytes==0,"Denied source kept GPU admission charges");
	custom_texture.refresh();processGpuCleanupOperations();finishPreload();require(gpuPreloadedTextures.size()==13 && !gpuPreloadOnDemand,"Session cleanup did not restore admission after budget pressure");
	// A new game must read its own same-hash files, not reuse A's images.
	settings.content.gameId=gameB;custom_texture.init();rend_process_custom_texture_preloads();finishPreload();
	for(const auto& entry:entries){const auto uploads=context()->testTextureUploadCount();auto *texture=static_cast<MetalTexture *>(GetTexture(tsp,entry.tcw,0));require(texture && texture->usingGpuPreloadedTexture,"New session preload not adopted");require(context()->testTextureUploadCount()==uploads,"New game reuploaded a prepared GPU texture");check(texture,entry.b,true);++adoptions;}
	{
		context()->waitIdle();auto event=[context()->device() newSharedEvent];
		struct OpenGate {id<MTLSharedEvent> event;~OpenGate(){event.signaledValue=1;}} openGate{event};
		auto gate=[context()->queue() commandBuffer];[gate waitForEvent:event value:1];context()->submitCommandBuffer(gate);
		fixture.global_param_op[0].texture=GetTexture(tsp,entries.back().tcw,0);
		require(Render(),"Held session render failed");context()->flushRenderCommands();
		auto snapshot=context()->makeTexture(8,8,MTLPixelFormatRGBA8Unorm);auto copy=[context()->queue() commandBuffer];auto transfer=[copy transferEncoder];
		[transfer copyFromTexture:screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(8,8,1)
			toTexture:snapshot destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[transfer endEncoding];context()->submitCommandBuffer(copy);
		custom_texture.terminate();rend_process_custom_texture_preloads();fixture.global_param_op[0].texture=nullptr;
		require(gpuPreloadedTextures.empty() && budget->usage().bytes>0 && event.signaledValue==0,"Session cleanup did not leave held GPU readers owning their preload charge");
		event.signaledValue=1;context()->waitIdle();std::vector<u8> pixels;context()->readTexture(snapshot,pixels);require(pixels.size()==256,"Held session snapshot dimensions differ");
		for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)for(unsigned c=0;c<4;++c)
			require(pixels[(y*8+x)*4+c]==entries.back().b[((7-y)*8+x)*4+c],"Clearing binding/cache/source owners corrupted a held GPU texture reader");++images;
	}
	stop();require(budget->usage().bytes==0,"Final source/cache cleanup retained a GPU preload charge");
	NOTICE_LOG(RENDERER,"Metal validation PASS: actual package catalog/WorkerThread/renderer-map sessions, %u PNG image checks, %u zero-reupload GPU adoptions, %u PNG demand completions and one compressed RGBA fallback; request/in-flight backend cancellation, source priority, renderer restart, admission denial/recovery, same-hash game switch, held GPU reader and complete budget release (%s)",images,adoptions,sourceReads,perPixel?"OIT":"standard");
	rc=nullptr;
}
