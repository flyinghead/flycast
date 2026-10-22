// SPDX-License-Identifier: GPL-2.0-or-later
// Actual renderer passes/attachments/normalization/pipelines, not shader-only fixtures.
void MetalRenderer::validateSyntheticAccumulator()
{
	using Color=std::array<u8,4>;
	const Color primary{10,20,30,40},secondary{64,128,192,96},vertex{160,80,32,144},zero{};
	const bool oldNative=config::NativeDepthInterpolation,oldEmulate=config::EmulateFramebuffer;
	const bool oldRTT=config::RenderToTextureBuffer,oldWide=config::Widescreen;
	const bool oldModifiers=config::ModifierVolumes;
	const bool oldSorting=config::PerStripSorting;
	const u32 oldAlpha=PT_ALPHA_REF;
	struct Restore {
		bool native,emulate,rtt,wide,modifiers,sorting;u32 alpha;
		~Restore(){config::NativeDepthInterpolation=native;config::EmulateFramebuffer=emulate;config::RenderToTextureBuffer=rtt;config::Widescreen=wide;config::ModifierVolumes=modifiers;config::PerStripSorting=sorting;PT_ALPHA_REF=alpha;}
	} restore{oldNative,oldEmulate,oldRTT,oldWide,oldModifiers,oldSorting,oldAlpha};
	config::EmulateFramebuffer=false;config::RenderToTextureBuffer=false;config::Widescreen=false;
	config::ModifierVolumes=true;
	context()->flushTextureUpdates();screenDepth=screenSecondary=nil;screenRendered=framebufferRendered=false;
	rend_context fixture{};rc=&fixture;
	const auto geometry=[&](bool rtt,u32 width=64,u32 height=48) {
		fixture.isRTT=rtt;fixture.clearFramebuffer=true;
		fixture.modtrig.clear();fixture.global_param_mvo.clear();fixture.global_param_mvo_tr.clear();
		fixture.framebufferWidth=width;fixture.framebufferHeight=rtt?width:height;
		const u32 w=rtt?width:640,h=rtt?width:480;
		fixture.globClip={w,h};fixture.tileClip=fixture.fbClip=Rect({0,0},{w,h});
		fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
		fixture.fb_W_SOF1=0x400000;fixture.fb_W_LINESTRIDE=rtt?width/4:16;fixture.fb_W_CTRL.fb_packmode=1;
		fixture.verts.resize(12);fixture.idx={0,1,2,3,4,5,6,7,8,9,10,11};
		for(unsigned q=0;q<3;++q)for(unsigned i=0;i<4;++i) {
			auto& v=fixture.verts[q*4+i];v={};v.x=i&1?w:0;v.y=i&2?h:0;v.z=1;
			const auto& color=q==0?primary:q==1?secondary:vertex;
			std::copy(color.begin(),color.end(),v.col);std::copy(color.begin(),color.end(),v.col1);
		}
	};
	const auto poly=[](unsigned quad) {
		PolyParam p;p.init();p.first=quad*4;p.count=4;p.pcw.Gouraud=1;p.isp.DepthMode=7;
		p.tsp.UseAlpha=1;p.tsp.FogCtrl=2;p.tsp.SrcInstr=1;p.tsp.DstInstr=0;return p;
	};
	const auto setup=[&](PolyParam operation,bool translucent,bool split) {
		auto a=poly(0),b=poly(1);b.tsp.DstSelect=1;
		fixture.global_param_op={a,b};fixture.global_param_pt.clear();fixture.global_param_tr.clear();
		if(translucent)fixture.global_param_tr.push_back(operation);else fixture.global_param_op.push_back(operation);
		RenderPass p{};p.op_count=fixture.global_param_op.size();p.tr_count=translucent?1:0;p.autosort=false;
		fixture.render_passes={p};
		if(split){auto first=p;first.op_count=2;first.tr_count=0;fixture.render_passes.insert(fixture.render_passes.begin(),first);}
	};
	const auto coefficient=[](unsigned mode,bool source,unsigned c,const Color& s,const Color& d) {
		const double other=(source?d[c]:s[c])/255.;
		switch(mode){case 0:return 0.;case 1:return 1.;case 2:return other;case 3:return 1-other;
		case 4:return s[3]/255.;case 5:return 1-s[3]/255.;case 6:return d[3]/255.;default:return 1-d[3]/255.;}
	};
	const auto blend=[&](unsigned source,unsigned dest,const Color& s,const Color& d) {
		Color result;for(unsigned c=0;c<4;++c)result[c]=u8(std::lround(std::clamp(s[c]*coefficient(source,true,c,s,d)+d[c]*coefficient(dest,false,c,s,d),0.,255.)));return result;
	};
	unsigned frames=0,images=0;
	const auto expect=[&](id<MTLTexture> image,const Color& expected,unsigned tolerance,const char *name) {
		if(!image)throw RendererException(std::string("Missing accumulator attachment: ")+name);
		std::vector<u8> pixels;context()->readTexture(image,pixels);
		if(pixels.size()!=image.width*image.height*4)throw RendererException("Accumulator readback dimensions differ");
		for(size_t i=0;i<pixels.size();++i)if(std::abs(int(pixels[i])-int(expected[i%4]))>int(tolerance))
			throw RendererException(std::string("Production accumulator mismatch: ")+name+" byte "+std::to_string(i)+" actual "+std::to_string(pixels[i])+" expected "+std::to_string(expected[i%4]));
		++images;
	};
	const auto expectSplit=[&](id<MTLTexture> image,const Color& left,const Color& right,unsigned leftTolerance,unsigned rightTolerance) {
		if(!image)throw RendererException("Missing split accumulator attachment");
		std::vector<u8> pixels;context()->readTexture(image,pixels);
		if(pixels.size()!=image.width*image.height*4)throw RendererException("Split accumulator dimensions differ");
		for(size_t i=0;i<pixels.size();++i) {
			const bool area1=(i/4)%image.width<image.width/2;const auto& expected=area1?left:right;
			if(std::abs(int(pixels[i])-int(expected[i%4]))>int(area1?leftTolerance:rightTolerance))
				throw RendererException("Production accumulator selected area/destination differs at byte "+std::to_string(i));
		}
		++images;
	};
	const auto render=[&] {
		if(Render()!=!fixture.isRTT || !directAccumulator || !target || !secondaryColor)
			throw RendererException("Accumulator production path was not selected");
		++frames;
	};
	// Distinct seed colors make source/destination mixups observable. Each selected
	// target is checked against independent arithmetic; the untouched image is exact.
	for(bool native:{false,true})for(bool rtt:{false,true})for(bool tr:{false,true})
	for(unsigned s=0;s<8;++s)for(unsigned d=0;d<8;++d)for(bool source:{false,true})for(bool dest:{false,true}) {
		config::NativeDepthInterpolation=native;geometry(rtt);
		auto p=poly(2);p.tsp.SrcInstr=s;p.tsp.DstInstr=d;p.tsp.SrcSelect=source;p.tsp.DstSelect=dest;
		setup(p,tr,(s+d)&1);render();
		const auto result=blend(s,d,source?secondary:vertex,dest?secondary:primary);
		expect(target,dest?primary:result,dest?0:1,"blend primary");
		expect(secondaryColor,dest?result:secondary,dest?1:0,"blend secondary");
		if(rtt && textureCache.getRTTexture(fixture.fb_W_SOF1,1,64,64)->image!=target)
			throw RendererException("Accumulator RTT output was not registered in the GPU texture cache");
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: %u production accumulator blend frames, %u full RGBA images; all 64 factors, sources/destinations, opaque/unsorted, screen/RTT, logical passes and depth modes (%s)",frames,images,perPixel?"OIT":"standard");
	// Actual hardware depth rejection must preserve both images. Equal input Z
	// also exercises the EQUAL/NOT_EQUAL cases without a CPU approximation of Z.
	for(bool native:{false,true})for(unsigned mode=0;mode<8;++mode)for(unsigned z=1;z<=3;++z)
	for(bool source:{false,true})for(bool dest:{false,true}) {
		config::NativeDepthInterpolation=native;geometry(false);for(unsigned i=0;i<8;++i)fixture.verts[i].z=2;
		for(unsigned i=8;i<12;++i)fixture.verts[i].z=z;
		auto p=poly(2);p.isp.DepthMode=mode;p.tsp.SrcSelect=source;p.tsp.DstSelect=dest;setup(p,false,false);render();
		const bool passed=mode==1?z<2:mode==2?z==2:mode==3?z<=2:mode==4?z>2:mode==5?z!=2:mode==6?z>=2:mode==7;
		const auto color=source?secondary:vertex;
		expect(target,passed&&!dest?color:primary,0,"depth primary");expect(secondaryColor,passed&&dest?color:secondary,0,"depth secondary");
	}
	for(bool native:{false,true})for(u32 reference:{100u,200u})for(bool source:{false,true})for(bool dest:{false,true}) {
		config::NativeDepthInterpolation=native;PT_ALPHA_REF=reference;geometry(false);
		auto p=poly(2);p.isp.DepthMode=6;p.tsp.SrcSelect=source;p.tsp.DstSelect=dest;
		setup(p,false,false);fixture.global_param_op.resize(2);fixture.global_param_pt={p};
		fixture.render_passes[0].op_count=2;fixture.render_passes[0].pt_count=1;render();
		Color color=source?secondary:vertex;if(!source)color[3]=255;
		const bool passed=source || vertex[3]>=reference;
		expect(target,passed&&!dest?color:primary,0,"punch primary");expect(secondaryColor,passed&&dest?color:secondary,0,"punch secondary");
	}
	PT_ALPHA_REF=oldAlpha;
	if(perPixel) {
		const Color volumeVertex{37,211,59,85};unsigned volumeFrames=0;
		for(bool native:{false,true})for(unsigned s=0;s<8;++s)for(unsigned d=0;d<8;++d)
		for(bool source:{false,true})for(bool dest:{false,true}) {
			config::NativeDepthInterpolation=native;geometry(false);
			auto p=poly(2);p.pcw.Shadow=1;p.tsp.SrcSelect=!source;p.tsp.DstSelect=!dest;
			p.tsp1=p.tsp;p.tsp1.SrcInstr=s;p.tsp1.DstInstr=d;p.tsp1.SrcSelect=source;p.tsp1.DstSelect=dest;p.tcw1.full=0;
			setup(p,false,false);
			// Give both seed volumes identical colors/controls so the real
			// shadow selection cannot dim or otherwise alter their seed values.
			for(unsigned i=0;i<2;++i){auto& seed=fixture.global_param_op[i];seed.pcw.Shadow=1;seed.tsp1=seed.tsp;seed.tcw1.full=0;}
			for(unsigned i=8;i<12;++i)std::copy(volumeVertex.begin(),volumeVertex.end(),fixture.verts[i].col1);
			fixture.modtrig={{0,0,3,320,0,3,0,480,3},{320,0,3,320,480,3,0,480,3}};
			ModifierVolumeParam modifier;modifier.init();modifier.count=2;modifier.isp.DepthMode=1;modifier.isp.VolumeLast=1;
			fixture.global_param_mvo={modifier};fixture.render_passes[0].mvo_count=1;render();++volumeFrames;
			if(!colorModifiers || !shadowStencil)throw RendererException("Accumulator fixture missed production modifier stencil");
			const auto left=blend(s,d,source?secondary:volumeVertex,dest?secondary:primary);
			const auto right=source?vertex:secondary;
			expectSplit(target,dest?primary:left,dest?right:primary,dest?0:1,0);
			expectSplit(secondaryColor,dest?left:secondary,dest?secondary:right,dest?1:0,0);
		}
		NOTICE_LOG(RENDERER,"Metal validation PASS: %u production accumulator two-volume modifier frames, all blend pairs, independently selected sources/destinations per pixel, both depth modes",volumeFrames);
		// Punch-through uses the selected area's alpha in the color pass, but
		// its independent opaque-depth prepass uses area 0. Keep the seed
		// shadow-eligible in both halves so a rejected punch cannot accidentally
		// make a missing modifier mask look like a correct alpha rejection.
		unsigned punchFrames=0;
		for(bool native:{false,true})for(u32 reference:{100u,200u})for(unsigned z=1;z<=3;++z)
		for(bool source0:{false,true})for(bool source1:{false,true})
		for(bool dest0:{false,true})for(bool dest1:{false,true}) {
			config::NativeDepthInterpolation=native;PT_ALPHA_REF=reference;geometry(false);
			for(unsigned i=0;i<8;++i)fixture.verts[i].z=2;
			for(unsigned i=8;i<12;++i){fixture.verts[i].z=z;std::copy(volumeVertex.begin(),volumeVertex.end(),fixture.verts[i].col1);}
			auto p=poly(2);p.pcw.Shadow=1;p.tsp.SrcSelect=source0;p.tsp.DstSelect=dest0;
			p.tsp1=p.tsp;p.tsp1.SrcSelect=source1;p.tsp1.DstSelect=dest1;p.tcw1.full=0;
			setup(p,false,false);fixture.global_param_op.resize(2);fixture.global_param_pt={p};
			for(auto& seed:fixture.global_param_op){seed.pcw.Shadow=1;seed.tsp1=seed.tsp;seed.tcw1.full=0;}
			fixture.modtrig={{0,0,4,320,0,4,0,480,4},{320,0,4,320,480,4,0,480,4}};
			ModifierVolumeParam modifier;modifier.init();modifier.count=2;modifier.isp.DepthMode=1;modifier.isp.VolumeLast=1;
			fixture.global_param_mvo={modifier};auto& pass=fixture.render_passes[0];pass.op_count=2;pass.pt_count=1;pass.mvo_count=1;
			render();++punchFrames;
			Color left=source1?secondary:volumeVertex,right=source0?secondary:vertex;
			if(!source1)left[3]=255;if(!source0)right[3]=255;
			const bool leftPass=z>=2 && (source1 || volumeVertex[3]>=reference);
			const bool rightPass=z>=2 && (source0 || vertex[3]>=reference);
			expectSplit(target,leftPass&&!dest1?left:primary,rightPass&&!dest0?right:primary,0,0);
			expectSplit(secondaryColor,leftPass&&dest1?left:secondary,rightPass&&dest0?right:secondary,0,0);
		}
		PT_ALPHA_REF=oldAlpha;
		NOTICE_LOG(RENDERER,"Metal validation PASS: %u coupled accumulator punch-through/modifier frames; independent area sources/destinations, alpha acceptance/rejection and below/equal/above depth",punchFrames);

		// Direct raster accumulation survives the intervening compute resolve.
		// Sorted OIT has its own pass-local secondary, initially zero. Verify
		// that boundary with independent colors, not merely dense/linked parity.
		const bool savedDense=disableDenseOIT,savedPacked=disablePackedOIT;
		struct RestoreOIT {bool& dense;bool& packed;bool oldDense,oldPacked;~RestoreOIT(){dense=oldDense;packed=oldPacked;}} restoreOIT{disableDenseOIT,disablePackedOIT,savedDense,savedPacked};
		for(bool native:{false,true})for(bool linked:{false,true})for(bool separateResolve:{false,true}) {
			config::NativeDepthInterpolation=native;disableDenseOIT=disablePackedOIT=linked;geometry(false);
			for(unsigned i=8;i<12;++i)fixture.verts[i].z=2;
			auto write=poly(2),read=poly(2);write.tsp.DstSelect=1;read.tsp.SrcSelect=1;
			setup(write,true,false);fixture.global_param_tr={write,read};
			fixture.verts.resize(16);fixture.idx.insert(fixture.idx.end(),{12,13,14,15});
			for(unsigned i=0;i<4;++i){auto v=fixture.verts[8+i];v.x=i&1?640:320;v.z=3;fixture.verts[12+i]=v;}
			auto directRead=poly(3);directRead.tsp.SrcSelect=1;fixture.global_param_op.push_back(directRead);
			RenderPass seed{};seed.op_count=2;
			auto sorted=seed;sorted.tr_count=separateResolve?1:2;sorted.autosort=true;
			fixture.render_passes={seed,sorted};
			if(separateResolve){sorted.tr_count=2;fixture.render_passes.push_back(sorted);}
			auto last=sorted;last.op_count=3;last.autosort=false;fixture.render_passes.push_back(last);
			render();expectSplit(target,separateResolve?zero:vertex,secondary,1,0);expect(secondaryColor,secondary,0,"direct secondary survives OIT resolve");
		}
		NOTICE_LOG(RENDERER,"Metal validation PASS: 8 independent full-image direct/OIT accumulator boundaries, dense and linked, shared/separate sorted resolves and both depth modes");
	}
	{
		const auto oldPlatform=settings.platform;
		struct RestorePlatform {decltype(settings.platform) old;~RestorePlatform(){settings.platform=old;}} restorePlatform{oldPlatform};
		settings.platform.system=DC_PLATFORM_NAOMI2;
		const Color ambient{37,211,59,144};unsigned naomiFrames=0;
		for(bool rtt:{false,true})for(bool lit:{false,true})
		for(unsigned s=0;s<8;++s)for(unsigned d=0;d<8;++d)for(bool source:{false,true})for(bool dest:{false,true}) {
			geometry(rtt);fixture.matrices.resize(2);glm::mat4 identity(1),projection(1);projection[2][3]=-1;projection[3][3]=0;
			memcpy(fixture.matrices[0].mat,glm::value_ptr(identity),sizeof(N2Matrix));memcpy(fixture.matrices[1].mat,glm::value_ptr(projection),sizeof(N2Matrix));
			fixture.lightModels.resize(2);fixture.lightModels[1]={};
			for(unsigned c=0;c<3;++c)fixture.lightModels[1].ambientBase[0][c]=ambient[c]/255.f;
			for(auto& v:fixture.verts){v.z=-1;v.nz=1;}
			auto p=poly(2);p.tsp.SrcInstr=s;p.tsp.DstInstr=d;p.tsp.SrcSelect=source;p.tsp.DstSelect=dest;setup(p,(s+d)&1,(s+d)&1);
			for(auto *list:{&fixture.global_param_op,&fixture.global_param_tr})for(auto& n2:*list) {
				n2.mvMatrix=n2.normalMatrix=0;n2.projMatrix=1;n2.lightModel=1;n2.constantColor[0]=!lit || n2.first!=8;
			}
			render();++naomiFrames;
			const auto result=blend(s,d,source?secondary:lit?ambient:vertex,dest?secondary:primary);
			expect(target,dest?primary:result,dest?0:1,"Naomi2 accumulator primary");expect(secondaryColor,dest?result:secondary,dest?1:0,"Naomi2 accumulator secondary");
		}
		NOTICE_LOG(RENDERER,"Metal validation PASS: %u Naomi2 accumulator frames; actual matrices, constant/ambient-lit material, nonzero light offset, all blend pairs, sources/destinations and screen/RTT (%s)",naomiFrames,perPixel?"OIT":"standard");
		if(perPixel) {
			const Color areaVertex{223,41,157,85},areaAmbient{83,17,229,85};
			for(bool lit:{false,true})for(bool source0:{false,true})for(bool source1:{false,true})
			for(bool dest0:{false,true})for(bool dest1:{false,true}) {
				geometry(false);
				for(unsigned c=0;c<3;++c){fixture.lightModels[1].ambientBase[0][c]=ambient[c]/255.f;fixture.lightModels[1].ambientBase[1][c]=areaAmbient[c]/255.f;}
				for(auto& v:fixture.verts){v.z=-1;v.nz=1;}
				for(unsigned i=8;i<12;++i)std::copy(areaVertex.begin(),areaVertex.end(),fixture.verts[i].col1);
				auto p=poly(2);p.pcw.Shadow=1;p.tsp.SrcSelect=source0;p.tsp.DstSelect=dest0;
				p.tsp.SrcInstr=4;p.tsp.DstInstr=5;p.tsp1=p.tsp;p.tsp1.SrcSelect=source1;p.tsp1.DstSelect=dest1;p.tcw1.full=0;
				setup(p,false,false);
				for(auto& n2:fixture.global_param_op) {
					n2.mvMatrix=n2.normalMatrix=0;n2.projMatrix=1;n2.lightModel=1;
					n2.constantColor[0]=n2.constantColor[1]=!lit || n2.first!=8;
					n2.pcw.Shadow=1;if(n2.first!=8){n2.tsp1=n2.tsp;n2.tcw1.full=0;}
				}
				// Projection divides by .5, so this object-space rectangle
				// covers precisely the left half of the 640x480 logical image.
				fixture.modtrig={{0,0,-.5f,160,0,-.5f,0,240,-.5f},{160,0,-.5f,160,240,-.5f,0,240,-.5f}};
				ModifierVolumeParam modifier;modifier.init();modifier.count=2;modifier.isp.DepthMode=1;modifier.isp.VolumeLast=1;
				modifier.mvMatrix=0;modifier.projMatrix=1;fixture.global_param_mvo={modifier};fixture.render_passes[0].mvo_count=1;
				render();
				const auto left=blend(4,5,source1?secondary:lit?areaAmbient:areaVertex,dest1?secondary:primary);
				const auto right=blend(4,5,source0?secondary:lit?ambient:vertex,dest0?secondary:primary);
				expectSplit(target,dest1?primary:left,dest0?primary:right,dest1?0:1,dest0?0:1);
				expectSplit(secondaryColor,dest1?left:secondary,dest0?right:secondary,dest1?1:0,dest0?1:0);
			}
			NOTICE_LOG(RENDERER,"Metal validation PASS: 32 Naomi2 two-volume accumulator frames; independent lit/constant area colors, sources/destinations and actual projected modifier stencil");
		}
		fixture.matrices.clear();fixture.lightModels.clear();
	}
	if(!perPixel) {
		unsigned sortedFrames=0;
		for(bool native:{false,true})for(bool strips:{false,true})
		for(unsigned s=0;s<8;++s)for(unsigned d=0;d<8;++d)for(bool source:{false,true})for(bool dest:{false,true}) {
			config::NativeDepthInterpolation=native;config::PerStripSorting=strips;geometry(false);
			for(unsigned i=8;i<12;++i)fixture.verts[i].z=2;
			auto p=poly(2);p.tsp.SrcInstr=s;p.tsp.DstInstr=d;p.tsp.SrcSelect=source;p.tsp.DstSelect=dest;setup(p,true,false);
			fixture.idx.insert(fixture.idx.end(),{8,9,10,10,9,11});fixture.sortedTriangles={{0,12,6}};
			fixture.render_passes[0].autosort=true;fixture.render_passes[0].sorted_tr_count=1;
			render();++sortedFrames;
			const auto result=blend(s,d,source?secondary:vertex,dest?secondary:primary);
			expect(target,dest?primary:result,dest?0:1,"sorted direct primary");expect(secondaryColor,dest?result:secondary,dest?1:0,"sorted direct secondary");
		}
		fixture.sortedTriangles.clear();config::PerStripSorting=oldSorting;
		NOTICE_LOG(RENDERER,"Metal validation PASS: %u standard sorted accumulator frames; strip/triangle index paths, all blend pairs, sources/destinations and both depth modes",sortedFrames);
		const u32 savedShade=FPU_SHAD_SCALE.full;
		struct RestoreShade {u32 value;~RestoreShade(){FPU_SHAD_SCALE.full=value;}} restoreShade{savedShade};
		FPU_SHAD_SCALE.scale_factor=128;
		for(bool native:{false,true})for(bool source:{false,true})for(bool dest:{false,true}) {
			config::NativeDepthInterpolation=native;geometry(false);
			auto p=poly(2);p.pcw.Shadow=1;p.tsp.SrcSelect=source;p.tsp.DstSelect=dest;setup(p,false,false);
			fixture.modtrig={{0,0,3,320,0,3,0,480,3},{320,0,3,320,480,3,0,480,3}};
			ModifierVolumeParam modifier;modifier.init();modifier.count=2;modifier.isp.DepthMode=1;modifier.isp.VolumeLast=1;
			fixture.global_param_mvo={modifier};fixture.render_passes[0].mvo_count=1;render();
			const auto color=source?secondary:vertex;const auto before=dest?primary:color;
			Color shadow=before;for(unsigned c=0;c<3;++c)shadow[c]=u8(std::lround(before[c]*.5));shadow[3]=u8(std::lround(63.75+before[3]*.5));
			expectSplit(target,shadow,before,1,0);expect(secondaryColor,dest?color:secondary,0,"standard modifier preserves secondary");
		}
		NOTICE_LOG(RENDERER,"Metal validation PASS: 8 standard accumulator modifier frames; primary-only final shadow, secondary unchanged, both depth modes");
	}
	if(perPixel)for(bool native:{false,true}) {
		config::NativeDepthInterpolation=native;geometry(false);setup(poly(2),false,false);
		fixture.global_param_op.resize(2);fixture.render_passes[0].op_count=2;render();
		// Per-pixel color continues, but its direct secondary belongs to one
		// Render(), unlike the standard pending screen pass. A half-screen
		// read makes both properties independently observable.
		fixture.clearFramebuffer=false;
		for(unsigned i=8;i<12;++i)fixture.verts[i].x=i&1?640:320;
		auto read=poly(2);read.tsp.SrcSelect=1;fixture.global_param_op={read};fixture.render_passes[0].op_count=1;
		render();expectSplit(target,primary,zero,0,0);expect(secondaryColor,zero,0,"per-pixel new render secondary");
		if(!Present())throw RendererException("Per-pixel accumulator image was not presented");context()->present();
		render();expectSplit(target,primary,zero,0,0);expect(secondaryColor,zero,0,"per-pixel post-Present secondary");
	}
	if(perPixel)NOTICE_LOG(RENDERER,"Metal validation PASS: per-pixel color continuation, fresh secondary per Render and after Present, both depth modes");
	if(!perPixel)for(bool native:{false,true}) {
		config::NativeDepthInterpolation=native;geometry(false);auto p=poly(2);p.tsp.SrcSelect=1;
		setup(p,false,false);fixture.global_param_op.resize(2);fixture.render_passes[0].op_count=2;render();
		// Intervening ordinary rendering must retain an untouched secondary.
		fixture.clearFramebuffer=false;fixture.global_param_op={poly(2)};fixture.render_passes[0].op_count=1;
		if(!Render())throw RendererException("Ordinary pending continuation failed");
		++frames;
		fixture.global_param_op={p};render();expect(target,secondary,0,"pending secondary read");expect(secondaryColor,secondary,0,"pending secondary preserved");
		if(!Present())throw RendererException("Accumulator image was not presented");context()->present();
		render();expect(target,zero,0,"presented frame starts empty");expect(secondaryColor,zero,0,"new secondary starts empty");
	}
	if(!perPixel)NOTICE_LOG(RENDERER,"Metal validation PASS: production secondary preserved through ordinary pending render and reset at Present, both depth modes");
	// RTT starts with a separate empty accumulator and leaves pending screen data.
	for(bool native:{false,true}) {
		config::NativeDepthInterpolation=native;geometry(false);auto p=poly(2);p.tsp.SrcSelect=1;setup(p,false,false);
		fixture.global_param_op.resize(2);fixture.render_passes[0].op_count=2;render();
		id<MTLTexture> savedScreen=screen,savedSecondary=secondaryColor;
		geometry(true);p=poly(2);p.tsp.SrcSelect=1;fixture.global_param_op={p};fixture.global_param_pt.clear();fixture.global_param_tr.clear();
		RenderPass pass{};pass.op_count=1;fixture.render_passes={pass};render();
		expect(target,zero,0,"RTT empty source");expect(secondaryColor,zero,0,"RTT empty secondary");
		expect(savedScreen,primary,0,"RTT leaves pending screen");expect(savedSecondary,secondary,0,"RTT leaves pending secondary");
	}
	for(bool native:{false,true}) {
		config::NativeDepthInterpolation=native;context()->waitIdle();
		auto event=[context()->device() newSharedEvent];auto ready=dispatch_semaphore_create(0);auto done=dispatch_group_create();
		struct Release {
			dispatch_semaphore_t ready;dispatch_group_t done;bool opened=false;
			void open(){if(!opened){opened=true;dispatch_semaphore_signal(ready);}}
			~Release(){open();dispatch_group_wait(done,DISPATCH_TIME_FOREVER);}
		} release{ready,done};
		dispatch_group_enter(done);
		dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
			dispatch_semaphore_wait(ready,DISPATCH_TIME_FOREVER);event.signaledValue=1;dispatch_group_leave(done);
		});
		auto gate=[context()->queue() commandBuffer];[gate waitForEvent:event value:1];context()->submitCommandBuffer(gate);
		struct Snapshot {id<MTLTexture> primary,secondary;Color expected;};std::vector<Snapshot> snapshots;
		for(unsigned i=0;i<12;++i) {
			geometry(false,i>=4 && i<8?80:64,i>=4 && i<8?60:48);
			const Color value{u8(31+i*11),u8(211-i*7),u8(59+i*9),u8(85+i*5)};
			for(unsigned j=4;j<8;++j)std::copy(value.begin(),value.end(),fixture.verts[j].col);
			auto p=poly(2);p.tsp.SrcSelect=1;setup(p,false,false);render();context()->flushRenderCommands();
			if(i<3) {
				const bool busy=dispatch_semaphore_wait(frameResources[currentFrameResource].available,DISPATCH_TIME_NOW)!=0;
				if(!busy)dispatch_semaphore_signal(frameResources[currentFrameResource].available);
				if(!busy || event.signaledValue!=0)throw RendererException("Accumulator frame slot was not held behind the GPU event");
			}
			Snapshot snapshot{context()->makeTexture(frame.width,frame.height,MTLPixelFormatRGBA8Unorm),
				context()->makeTexture(frame.width,frame.height,MTLPixelFormatRGBA8Unorm),value};
			auto copy=[context()->queue() commandBuffer];auto encoder=[copy transferEncoder];
			for(unsigned j=0;j<2;++j)[encoder copyFromTexture:j?secondaryColor:target sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
				sourceSize:MTLSizeMake(frame.width,frame.height,1) toTexture:j?snapshot.secondary:snapshot.primary destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
			[encoder endEncoding];context()->submitCommandBuffer(copy);snapshots.push_back(snapshot);
			if(i==2)release.open();
		}
		context()->waitIdle();
		for(const auto& snapshot:snapshots){expect(snapshot.primary,snapshot.expected,0,"queued primary snapshot");expect(snapshot.secondary,snapshot.expected,0,"queued secondary snapshot");}
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: production accumulator alpha/depth rejection, all eight depth comparisons, RTT isolation and 24 queued resized frames with three gated slots; %u frames and %u full RGBA checks total (%s)",frames,images,perPixel?"OIT":"standard");
	context()->waitIdle();rc=nullptr;
}
