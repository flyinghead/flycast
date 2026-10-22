// SPDX-License-Identifier: GPL-2.0-or-later
// Production custom upload/cache lookup and polygon sampling, with independent
// decoded colors for deliberately simple valid compressed blocks.
void MetalRenderer::validateSyntheticCustomTextures()
{
	using Color=std::array<u8,4>;
	const auto caps=MetalTexture::customCapabilities(),oldCaps=custom_texture.getCapabilities();
	const bool oldNative=config::NativeDepthInterpolation,oldEmulate=config::EmulateFramebuffer;
	const bool oldWide=config::Widescreen,oldMips=config::UseMipmaps;
	const int oldFilter=config::TextureFiltering,oldAnisotropy=config::AnisotropicFiltering;
	const u32 oldRead=FB_R_CTRL.full,oldSpg=SPG_CONTROL.full;
	struct Restore {
		CustomTextureCapabilities caps;bool native,emulate,wide,mips;int filter,anisotropy;u32 read,spg;
		~Restore(){custom_texture.setCapabilities(caps);config::NativeDepthInterpolation=native;config::EmulateFramebuffer=emulate;config::Widescreen=wide;config::UseMipmaps=mips;config::TextureFiltering=filter;config::AnisotropicFiltering=anisotropy;FB_R_CTRL.full=read;SPG_CONTROL.full=spg;}
	} restore{oldCaps,oldNative,oldEmulate,oldWide,oldMips,oldFilter,oldAnisotropy,oldRead,oldSpg};
	custom_texture.setCapabilities(caps);config::EmulateFramebuffer=false;config::Widescreen=false;
	config::UseMipmaps=true;config::TextureFiltering=0;config::AnisotropicFiltering=1;
	FB_R_CTRL.vclk_div=1;FB_R_CTRL.fb_line_double=0;SPG_CONTROL.interlace=0;
	context()->flushTextureUpdates();screenRendered=false;screenDepth=screenSecondary=nil;
	rend_context fixture{};rc=&fixture;fixture.clearFramebuffer=true;fixture.globClip={640,480};
	fixture.tileClip=fixture.fbClip=Rect({0,0},{640,480});fixture.scaler_ctl.vscalefactor=1024;fixture.fog_clamp_max.full=~0u;
	fixture.verts.resize(4);fixture.idx={0,1,2,3};
	for(unsigned i=0;i<4;++i){auto& v=fixture.verts[i];v={};v.x=i&1?640:0;v.y=i&2?480:0;v.z=1;std::fill_n(v.col,4,255);}
	PolyParam poly;poly.init();poly.count=4;poly.pcw.Gouraud=1;poly.pcw.Texture=1;poly.isp.DepthMode=7;
	poly.tsp.FogCtrl=2;poly.tsp.UseAlpha=1;poly.tsp.SrcInstr=1;poly.tsp.ShadInstr=0;poly.tsp.MipMapD=4;
	fixture.global_param_op={poly};RenderPass pass{};pass.op_count=1;fixture.render_passes={pass};
	MetalTexture reference;reference.tex_type=TextureType::_8888;
	unsigned formats=0,mipImages=0,filterPairs=0,lookups=0;
	const auto draw=[&](MetalTexture *texture,unsigned width,unsigned height,float first=0,float last=1) {
		fixture.framebufferWidth=width;fixture.framebufferHeight=height;fixture.global_param_op[0].texture=texture;
		for(unsigned i=0;i<4;++i){fixture.verts[i].u=i&1?last:first;fixture.verts[i].v=i&2?last:first;}
		if(!Render())throw RendererException("Custom texture production render failed");
		std::vector<u8> pixels;context()->readTexture(screen,pixels);
		if(pixels.size()!=size_t(width)*height*4)throw RendererException("Custom texture rendered image size differs");return pixels;
	};
	const auto same=[&](const std::vector<u8>& actual,const std::vector<u8>& expected,unsigned tolerance,const char *name) {
		if(actual.size()!=expected.size())throw RendererException("Custom texture comparison sizes differ");
		for(size_t i=0;i<actual.size();++i)if(std::abs(int(actual[i])-int(expected[i]))>int(tolerance))
			throw RendererException(std::string("Custom texture ")+name+" byte "+std::to_string(i)+" actual "+std::to_string(actual[i])+" expected "+std::to_string(expected[i]));
	};
	const auto word=[](u8 *p,u16 value){p[0]=u8(value);p[1]=u8(value>>8);};
	for(unsigned f=0;f<unsigned(NativeTextureFormat::Count);++f) {
		const auto format=NativeTextureFormat(f);if(!caps.supports(format))continue;
		auto payload=std::make_shared<PreparedCustomTexture>();payload->nativeFormat=format;payload->width=19;payload->height=13;
		std::vector<std::vector<u8>> decoded;u64 offset=0;
		for(unsigned level=0;level<5;++level){auto layout=computeMipLayout(format,std::max(1u,19u>>level),std::max(1u,13u>>level),offset);payload->levels.push_back(layout);offset+=layout.byteSize;decoded.emplace_back(size_t(layout.width)*layout.height*4);}
		payload->bytes.resize(offset);const auto block=getBlockGeometry(format);
		for(unsigned level=0;level<5;++level) {
			const auto& mip=payload->levels[level];auto& expected=decoded[level];
			for(unsigned by=0;by<mip.blocksY;++by)for(unsigned bx=0;bx<mip.blocksX;++bx) {
				u8 *data=payload->bytes.data()+mip.byteOffset+by*mip.rowPitchBytes+bx*block.bytesPerBlockOrPixel;
				const unsigned seed=17+bx*31+by*47+level*19;
				Color color{u8((seed*2)&254),u8((seed*6+14)&254),u8((seed*10+38)&254),u8((seed*14+62)&254)};
				if(format==NativeTextureFormat::Rgba8Unorm)std::copy(color.begin(),color.end(),data);
				else if(format==NativeTextureFormat::Bc7Unorm) {
					// BC7 mode 6: two identical even endpoints, zero p-bits and
					// indices. This exercises all four independently chosen channels.
					unsigned bit=0;const auto bits=[&](unsigned value,unsigned count){for(unsigned i=0;i<count;++i,++bit)data[bit/8]|=((value>>i)&1)<<(bit%8);};
					bits(64,7);for(auto c:color){bits(c/2,7);bits(c/2,7);}bits(0,2);bits(0,3);for(unsigned i=1;i<16;++i)bits(0,4);
					if(bit!=128)throw RendererException("BC7 fixture bit count differs");
				} else if(format==NativeTextureFormat::Bc1Unorm || format==NativeTextureFormat::Bc3Unorm) {
					u8 *rgb=data;if(format==NativeTextureFormat::Bc3Unorm){data[0]=color[3];data[1]=255-color[3];rgb+=8;}
					// Include BC1's three-color selector, whose alpha must remain
					// opaque under the native RGB replacement contract.
					word(rgb,0x001f);word(rgb+2,0xf800);for(unsigned y=0;y<4;++y)rgb[4+y]=u8(0xe4u^(by&1?0x55:0));
				} else if(format==NativeTextureFormat::Etc2Rgb8Unorm || format==NativeTextureFormat::Etc2Rgba8Unorm) {
					u8 *rgb=data;if(format==NativeTextureFormat::Etc2Rgba8Unorm){data[0]=color[3];rgb+=8;}
					// ETC individual mode, equal subblocks, table 0 / selector 0:
					// its +2 modifier and EAC multiplier zero have exact references.
					for(unsigned c=0;c<3;++c){const u8 nibble=(color[c]>>4)&15;rgb[c]=nibble*17;color[c]=std::min(255,int(nibble)*17+2);}
					if(format==NativeTextureFormat::Etc2Rgb8Unorm)color[3]=255;
				} else {
					// ASTC LDR void-extent block with full-extent coordinates;
					// 16-bit UNORM endpoint channels replicate the expected byte.
					std::fill_n(data,16,255);data[0]=0xfc;data[1]=0xfd;for(unsigned c=0;c<4;++c)word(data+8+c*2,u16(color[c])*257);
				}
				for(unsigned y=0;y<block.blockHeight;++y)for(unsigned x=0;x<block.blockWidth;++x) {
					const unsigned px=bx*block.blockWidth+x,py=by*block.blockHeight+y;if(px>=mip.width || py>=mip.height)continue;
					Color value=color;
					if(format==NativeTextureFormat::Bc1Unorm || format==NativeTextureFormat::Bc3Unorm) {
						const unsigned selector=((0xe4u^(by&1?0x55:0))>>(x*2))&3;
						const Color rgb3[]={{0,0,255,255},{255,0,0,255},{127,0,127,255},{0,0,0,255}};
						const Color rgb4[]={{0,0,255,255},{255,0,0,255},{85,0,170,255},{170,0,85,255}};
						value=format==NativeTextureFormat::Bc1Unorm?rgb3[selector]:rgb4[selector];if(format==NativeTextureFormat::Bc3Unorm)value[3]=color[3];
					}
					std::copy(value.begin(),value.end(),expected.data()+(py*mip.width+px)*4);
				}
			}
		}
		// Exercise the production cache entry, availability check and virtual
		// prepared-upload call rather than assigning an independently built image.
		TSP tsp=poly.tsp;TCW tcw{};tcw.PixelFmt=Pixel4444;tcw.TexAddr=0x600000/8;tcw.MipMapped=1;
		auto *texture=textureCache.getTextureCacheData(tsp,tcw,0);texture->dirty=0;texture->customPayload=payload;
		if(GetTexture(tsp,tcw,0)!=texture || texture->customPayload || !texture->is_custom_replaced || texture->gpuPalette || texture->customMipLevels!=5)
			throw RendererException("Native custom texture cache lookup/adoption failed");++lookups;
		const id<MTLTexture> chain=texture->image;
		if(!chain || chain.mipmapLevelCount!=5)throw RendererException("Native custom mip chain missing");
		for(unsigned level=0;level<5;++level) {
			const auto& mip=payload->levels[level];
			texture->image=[chain newTextureViewWithPixelFormat:chain.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(level,1) slices:NSMakeRange(0,1)];
			if(!texture->image)throw RendererException("Custom mip view failed");
			auto& controls=fixture.global_param_op[0].tsp;controls.FilterMode=0;controls.ClampU=controls.ClampV=1;controls.FlipU=controls.FlipV=0;
			config::NativeDepthInterpolation=level&1;
			same(draw(texture,mip.width,mip.height),decoded[level],1,nativeTextureFormatName(format));++mipImages;
			reference.UploadToGPU(mip.width,mip.height,decoded[level].data(),false);
			for(bool linear:{false,true})for(unsigned addressing=0;addressing<3;++addressing) {
				controls.FilterMode=linear;controls.ClampU=controls.ClampV=addressing==0;controls.FlipU=controls.FlipV=addressing==2;
				const auto native=draw(texture,63,47,-.2f,1.3f),rgba=draw(&reference,63,47,-.2f,1.3f);
				same(native,rgba,1,nativeTextureFormatName(format));++filterPairs;
			}
		}
		texture->image=chain;++formats;
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: custom sampling %u formats, %u decoded mip images, %u full RGBA filter/address pairs, %u actual cache lookups; odd dimensions, sub-block mips, alpha and BC1 RGB swizzle (%s)",formats,mipImages,filterPairs,lookups,perPixel?"OIT":"standard");
#ifdef FLYCAST_METAL_TEST_FIXTURE_ROOT
	// These are existing Basis distribution fixtures, read in place. The
	// production container parser/transcoder supplies both native blocks and
	// its CPU-decoded RGBA fallback from the same unchanged source container.
	struct Pack {const char *name;NativeTextureFormat format;};
	const Pack packs[]={
		{"t4x4.ktx2",NativeTextureFormat::Astc4x4Unorm},{"t5x4.ktx2",NativeTextureFormat::Astc5x4Unorm},
		{"t5x5.ktx2",NativeTextureFormat::Astc5x5Unorm},{"t6x5.ktx2",NativeTextureFormat::Astc6x5Unorm},
		{"t6x6.ktx2",NativeTextureFormat::Astc6x6Unorm},{"t8x5.ktx2",NativeTextureFormat::Astc8x5Unorm},
		{"t8x6.ktx2",NativeTextureFormat::Astc8x6Unorm},{"t8x8.ktx2",NativeTextureFormat::Astc8x8Unorm},
		{"t10x5.ktx2",NativeTextureFormat::Astc10x5Unorm},{"t10x6.ktx2",NativeTextureFormat::Astc10x6Unorm},
		{"t10x8.ktx2",NativeTextureFormat::Astc10x8Unorm},{"kodim14_12x12.ktx2",NativeTextureFormat::Astc12x12Unorm},
		{"kodim26_xubc7.ktx2",NativeTextureFormat::Bc7Unorm}};
	unsigned packCount=0,packPairs=0,fallbacks=0,sourceDifference=0,ddsContainers=0;
	for(const auto& pack:packs) {
		if(!caps.supports(pack.format))continue;
		const std::string path=std::string(FLYCAST_METAL_TEST_FIXTURE_ROOT)+"/"+pack.name;
		std::ifstream input(path,std::ios::binary);if(!input)throw RendererException("Missing existing Basis test fixture: "+path);
		std::vector<u8> bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
		TextureTranscoder decoder;const auto inspection=decoder.inspect(CustomTextureSourceKind::Ktx2Generic,std::move(bytes));
		auto prepared=decoder.prepare(pack.format,0xf02,{});
		const auto fallbackCaps=CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Metal4);
		const auto targets=selectNativeTextureTargets(fallbackCaps,inspection.codec,inspection.blockWidth,inspection.blockHeight,inspection.hasAlpha);
		if(targets!=std::vector<NativeTextureFormat>{NativeTextureFormat::Rgba8Unorm})throw RendererException("RGBA-only capabilities did not select the fallback");
		auto rgba=decoder.prepare(targets.front(),0xf02,{});
		if(pack.format==NativeTextureFormat::Bc7Unorm) {
			// Also exercise the actual DX10 DDS reader with the very same
			// odd-sized, complete BC7 chain. The container exists only in RAM.
			std::vector<u8> dds(148+prepared->bytes.size());
			const auto put=[&](unsigned offset,u32 value){for(unsigned i=0;i<4;++i)dds[offset+i]=u8(value>>(8*i));};
			put(0,0x20534444);put(4,124);put(8,0xa1007);put(12,prepared->height);put(16,prepared->width);
			put(20,u32(prepared->levels[0].byteSize));put(28,u32(prepared->levels.size()));put(76,32);put(80,4);put(84,0x30315844);
			put(108,0x401008);put(128,98);put(132,3);put(140,1);std::copy(prepared->bytes.begin(),prepared->bytes.end(),dds.begin()+148);
			TextureTranscoder ddsDecoder;const auto info=ddsDecoder.inspect(CustomTextureSourceKind::DdsBc7,dds);
			if(info.width!=inspection.width || info.height!=inspection.height || info.levels!=inspection.levels)throw RendererException("DDS inspection geometry differs");
			const auto ddsNative=ddsDecoder.prepare(pack.format,0xf02,{});
			if(ddsNative->bytes!=prepared->bytes)throw RendererException("DDS BC7 native pass-through changed its blocks");
			prepared=ddsNative;rgba=ddsDecoder.prepare(NativeTextureFormat::Rgba8Unorm,0xf02,{});++ddsContainers;
			dds.pop_back();bool rejected=false;try{TextureTranscoder invalid;invalid.inspect(CustomTextureSourceKind::DdsBc7,std::move(dds));}catch(const FlycastException&){rejected=true;}
			if(!rejected)throw RendererException("Truncated DDS chain accepted");
		}
		TSP tsp=poly.tsp;TCW tcw{};tcw.PixelFmt=Pixel4444;tcw.TexAddr=0x600000/8;tcw.MipMapped=1;
		auto *texture=textureCache.getTextureCacheData(tsp,tcw,0);texture->dirty=0;texture->customPayload=prepared;
		if(GetTexture(tsp,tcw,0)!=texture || texture->customPayload || texture->customMipLevels!=inspection.levels)throw RendererException("Container cache upload failed");
		const id<MTLTexture> chain=texture->image;
		for(unsigned level=0;level<prepared->levels.size();++level) {
			const auto& mip=prepared->levels[level];const auto& rgbaMip=rgba->levels[level];
			// Compare the GPU against CPU decoding of the *same native blocks*.
			// Decoding XUASTC directly to RGBA can use a different decode profile
			// or reconstruction from materializing physical ASTC blocks.
			const auto geometry=getBlockGeometry(pack.format);std::vector<u8> nativeDecoded(size_t(mip.width)*mip.height*4);
			for(unsigned by=0;by<mip.blocksY;++by)for(unsigned bx=0;bx<mip.blocksX;++bx) {
				const u8 *block=prepared->bytes.data()+mip.byteOffset+by*mip.rowPitchBytes+bx*geometry.bytesPerBlockOrPixel;
				basist::color_rgba pixels[astc_helpers::MAX_BLOCK_PIXELS];
				if(pack.format==NativeTextureFormat::Bc7Unorm) {
					if(!basist::bc7u::unpack_bc7(block,pixels))throw RendererException("CPU BC7 fixture decode failed");
				} else {
					astc_helpers::log_astc_block logical;
					if(!astc_helpers::unpack_block(block,logical,geometry.blockWidth,geometry.blockHeight)
						|| !astc_helpers::decode_block(logical,pixels,geometry.blockWidth,geometry.blockHeight,astc_helpers::cDecodeModeLDR8))
						throw RendererException("CPU ASTC fixture decode failed");
				}
				for(unsigned y=0;y<geometry.blockHeight;++y)for(unsigned x=0;x<geometry.blockWidth;++x) {
					const unsigned px=bx*geometry.blockWidth+x,py=by*geometry.blockHeight+y;if(px>=mip.width || py>=mip.height)continue;
					std::copy_n(reinterpret_cast<const u8 *>(&pixels[y*geometry.blockWidth+x]),4,nativeDecoded.data()+(py*mip.width+px)*4);
				}
			}
			for(size_t i=0;i<nativeDecoded.size();++i)sourceDifference=std::max(sourceDifference,unsigned(std::abs(int(nativeDecoded[i])-int(rgba->bytes[rgbaMip.byteOffset+i]))));
			texture->image=[chain newTextureViewWithPixelFormat:chain.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(level,1) slices:NSMakeRange(0,1)];
			reference.UploadToGPU(mip.width,mip.height,nativeDecoded.data(),false);
			auto& controls=fixture.global_param_op[0].tsp;controls.ClampU=controls.ClampV=1;controls.FlipU=controls.FlipV=0;
			config::NativeDepthInterpolation=level&1;
			for(bool linear:{false,true}) {
				controls.FilterMode=linear;
				const auto native=draw(texture,std::min(127u,mip.width),std::min(95u,mip.height)),decoded=draw(&reference,std::min(127u,mip.width),std::min(95u,mip.height));
				same(native,decoded,1,pack.name);++packPairs;
			}
		}
		// Change the active format contract, then load the actual RGBA fallback
		// into this same cache entry. Its pixels must match a direct RGBA upload.
		custom_texture.setCapabilities(fallbackCaps);texture->customPayload=rgba;
		if(GetTexture(tsp,tcw,0)!=texture || texture->customPayload || texture->image.pixelFormat!=MTLPixelFormatRGBA8Unorm)
			throw RendererException("Container RGBA fallback replacement failed");
		if(!reference.uploadCustomTexture(*rgba,true))throw RendererException("Direct RGBA fallback chain upload failed");
		fixture.global_param_op[0].tsp.FilterMode=0;
		same(draw(texture,127,95),draw(&reference,127,95),0,"container RGBA fallback");++fallbacks;
		custom_texture.setCapabilities(caps);++packCount;
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: %u existing KTX2 containers and %u in-memory DDS BC7 container through production parser/transcoder/cache, %u full RGBA mip/filter pairs against CPU native-block decode, %u exact RGBA-only fallback replacements; source-vs-native CPU decode maximum channel difference %u (%s)",packCount,ddsContainers,packPairs,fallbacks,sourceDifference,perPixel?"OIT":"standard");
#endif
	// Replace and erase the real cache entry while three frame slots and all
	// their sampled images are still held behind a native GPU event. The
	// command wrappers must retain each old sampled image until its last read.
	context()->waitIdle();auto event=[context()->device() newSharedEvent];auto ready=dispatch_semaphore_create(0);auto done=dispatch_group_create();
	struct Release {dispatch_semaphore_t ready;dispatch_group_t done;bool opened=false;void open(){if(!opened){opened=true;dispatch_semaphore_signal(ready);}}~Release(){open();dispatch_group_wait(done,DISPATCH_TIME_FOREVER);}} release{ready,done};
	dispatch_group_enter(done);dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{dispatch_semaphore_wait(ready,DISPATCH_TIME_FOREVER);event.signaledValue=1;dispatch_group_leave(done);});
	auto gate=[context()->queue() commandBuffer];[gate waitForEvent:event value:1];context()->submitCommandBuffer(gate);
	struct Snapshot {id<MTLTexture> image;Color color;};std::vector<Snapshot> snapshots;
	for(unsigned i=0;i<24;++i) {
		const auto format=i&1?NativeTextureFormat::Astc4x4Unorm:NativeTextureFormat::Rgba8Unorm;
		const Color color{u8(20+i*8),u8(222-i*6),u8(34+i*8),u8(48+i*6)};
		auto payload=std::make_shared<PreparedCustomTexture>();payload->nativeFormat=format;payload->width=i%3?19:8;payload->height=i%3?13:8;
		const auto mip=computeMipLayout(format,payload->width,payload->height,0);payload->levels={mip};payload->bytes.resize(mip.byteSize);
		for(size_t b=0;b<payload->bytes.size();b+=getBlockGeometry(format).bytesPerBlockOrPixel) {
			u8 *data=payload->bytes.data()+b;if(format==NativeTextureFormat::Rgba8Unorm)std::copy(color.begin(),color.end(),data);
			else{std::fill_n(data,16,255);data[0]=0xfc;data[1]=0xfd;for(unsigned c=0;c<4;++c)word(data+8+c*2,u16(color[c])*257);}
		}
		custom_texture.setCapabilities(i&1?caps:CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Metal4));
		TSP tsp=poly.tsp;TCW tcw{};tcw.PixelFmt=Pixel4444;tcw.TexAddr=0x600000/8;
		auto *texture=textureCache.getTextureCacheData(tsp,tcw,0);texture->dirty=0;texture->customPayload=payload;
		if(GetTexture(tsp,tcw,0)!=texture || texture->customPayload || texture->customMipLevels!=1)throw RendererException("Queued custom resource replacement failed");
		fixture.framebufferWidth=i%3?80:64;fixture.framebufferHeight=i%3?60:48;fixture.global_param_op[0].texture=texture;
		fixture.global_param_op[0].tsp=poly.tsp;config::NativeDepthInterpolation=i&1;
		for(unsigned j=0;j<4;++j){fixture.verts[j].u=j&1?1:0;fixture.verts[j].v=j&2?1:0;}
		if(!Render())throw RendererException("Queued custom resource render failed");context()->flushRenderCommands();
		if(i<3) {
			const bool busy=dispatch_semaphore_wait(frameResources[currentFrameResource].available,DISPATCH_TIME_NOW)!=0;
			if(!busy)dispatch_semaphore_signal(frameResources[currentFrameResource].available);
			if(!busy || event.signaledValue!=0)throw RendererException("Custom texture fixture did not hold three GPU frame slots");
		}
		auto snapshot=context()->makeTexture(fixture.framebufferWidth,fixture.framebufferHeight,MTLPixelFormatRGBA8Unorm);
		auto copy=[context()->queue() commandBuffer];auto transfer=[copy transferEncoder];
		[transfer copyFromTexture:screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(snapshot.width,snapshot.height,1)
			toTexture:snapshot destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		[transfer endEncoding];context()->submitCommandBuffer(copy);snapshots.push_back({snapshot,color});
		if(i==2){textureCache.Clear();release.open();}
		else if(i%5==4)textureCache.Clear();
	}
	context()->waitIdle();
	for(const auto& snapshot:snapshots) {
		std::vector<u8> pixels;context()->readTexture(snapshot.image,pixels);std::vector<u8> referencePixels(snapshot.image.width*snapshot.image.height*4);
		for(size_t i=0;i<referencePixels.size();++i)referencePixels[i]=snapshot.color[i%4];same(pixels,referencePixels,0,"in-flight custom image");
	}
	NOTICE_LOG(RENDERER,"Metal validation PASS: 24 queued custom texture replacements, alternating RGBA/ASTC and capabilities, odd sizes/resize/cache erasure, three GPU-gated slots and exact full-image snapshots (%s)",perPixel?"OIT":"standard");
#ifdef FLYCAST_METAL_TEST_FIXTURE_ROOT
	{
		// A completed request from the previous format contract must never
		// replace a valid image after native-format support has disappeared.
		TSP tsp=poly.tsp;TCW tcw{};tcw.PixelFmt=Pixel4444;tcw.TexAddr=0x600000/8;
		const auto rgbaPayload=[&](Color color){auto p=std::make_shared<PreparedCustomTexture>();p->width=p->height=8;p->levels={computeMipLayout(p->nativeFormat,8,8,0)};p->bytes.resize(256);for(size_t i=0;i<p->bytes.size();++i)p->bytes[i]=color[i%4];return p;};
		const Color baseline{12,34,56,78},fallback{210,32,154,96};
		auto *texture=textureCache.getTextureCacheData(tsp,tcw,0);texture->dirty=0;texture->customPayload=rgbaPayload(baseline);
		custom_texture.setCapabilities(caps);GetTexture(tsp,tcw,0);const id<MTLTexture> before=texture->image;
		auto stale=std::make_shared<PreparedCustomTexture>();stale->nativeFormat=NativeTextureFormat::Astc4x4Unorm;stale->width=stale->height=8;
		stale->levels={computeMipLayout(stale->nativeFormat,8,8,0)};stale->bytes.resize(stale->levels[0].byteSize);
		for(size_t b=0;b<stale->bytes.size();b+=16){auto p=stale->bytes.data()+b;std::fill_n(p,16,255);p[0]=0xfc;p[1]=0xfd;}
		custom_texture.setCapabilities(CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Metal4));texture->customPayload=stale;
		const unsigned notifications=metalOffscreenExpectedNotificationCount;
		struct NotificationScope {NotificationScope(){metalOffscreenAllowExpectedNotification=true;}~NotificationScope(){metalOffscreenAllowExpectedNotification=false;}} notificationScope;
		if(GetTexture(tsp,tcw,0)!=texture || texture->customPayload || texture->customRequestId || texture->image!=before)
			throw RendererException("Stale native payload was not rejected without replacing the valid image");
		if(metalOffscreenExpectedNotificationCount-notifications!=(perPixel?0u:1u))throw RendererException("Custom upload error notification was not limited to once per session");
		fixture.global_param_op[0].tsp=poly.tsp;
		std::vector<u8> baselinePixels(256),fallbackPixels(256);for(size_t i=0;i<256;++i){baselinePixels[i]=baseline[i%4];fallbackPixels[i]=fallback[i%4];}
		same(draw(texture,8,8),baselinePixels,0,"rejected stale payload preserves valid pixels");
		texture->customPayload=rgbaPayload(fallback);
		if(GetTexture(tsp,tcw,0)!=texture || texture->customPayload || texture->image.pixelFormat!=MTLPixelFormatRGBA8Unorm)throw RendererException("RGBA fallback after stale request failed");
		same(draw(texture,8,8),fallbackPixels,0,"RGBA fallback after rejected stale request");
		custom_texture.setCapabilities(caps);
		NOTICE_LOG(RENDERER,"Metal validation PASS: stale unsupported native request rejected, valid image retained exactly, subsequent RGBA fallback exact and notification limited to once per session (%s)",perPixel?"OIT":"standard");
	}
#endif
	context()->waitIdle();reference.Delete();rc=nullptr;
}
