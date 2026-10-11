// Runs the production renderer with synthetic parsed TA geometry, without ROMs.
// Included only by builds explicitly enabling METAL_RENDERER_TESTS.
// SPDX-License-Identifier: GPL-2.0-or-later
void MetalRenderer::validateSynthetic()
{
	const auto oldEmulate = config::EmulateFramebuffer.get();
	const auto oldWidescreen = config::Widescreen.get();
	const auto oldSorting = config::PerStripSorting.get();
	const auto oldBudget = config::PixelBufferSize.get();
	config::EmulateFramebuffer = false;
	config::Widescreen = false;
	config::PerStripSorting = true;
	config::PixelBufferSize = 1_MB;
	rend_context fixture{};
	fixture.globClip = {640, 480};
	fixture.framebufferWidth = 64; fixture.framebufferHeight = 48;
	fixture.tileClip = fixture.fbClip = Rect({0, 0}, {640, 480});
	fixture.scaler_ctl.vscalefactor = 1024;
	fixture.clearFramebuffer = true;
	fixture.fog_clamp_max.full = ~0u;
	fixture.verts.resize(8);
	for (int i = 0; i < 8; ++i) {
		auto& v = fixture.verts[i];
		v.x = (i & 1) ? 640 : 0; v.y = (i & 2) ? 480 : 0;
		v.z = i < 4 ? 1 : 2;
		v.col[i < 4 ? 0 : 2] = 255; v.col[3] = i < 4 ? 255 : 128;
		v.col1[1] = v.col1[3] = 255;
	}
	fixture.idx = {0, 1, 2, 3, 4, 5, 6, 7};
	PolyParam opaque; opaque.init(); opaque.count = 4;
	opaque.isp.DepthMode = 6; opaque.pcw.Gouraud = 1;
	opaque.tsp.UseAlpha = 1; opaque.tsp.FogCtrl = 2; opaque.tsp.SrcInstr = 1;
	fixture.global_param_op.push_back(opaque);
	RenderPass pass{}; pass.op_count = 1;
	fixture.render_passes.push_back(pass);
	rc = &fixture;
	u8 zeros[256]{};
	fog.tex_type = TextureType::_8; fog.UploadToGPU(128, 2, zeros, false);
	palette.tex_type = TextureType::_8888; palette.UploadToGPU(1, 1, zeros, false);
	auto check = [&](const char *name, int r, int g, int b) {
		if (!Render()) throw RendererException("Synthetic Metal render failed");
		std::vector<u8> pixels; context()->readTexture(screen, pixels);
		const u8 *pixel = pixels.data() + (24 * 64 + 32) * 4;
		if (std::abs(int(pixel[0]) - r) > 1 || std::abs(int(pixel[1]) - g) > 1 || std::abs(int(pixel[2]) - b) > 1)
			throw RendererException(std::string("Metal renderer pixel mismatch: ") + name
				+ " (" + std::to_string(pixel[0]) + "," + std::to_string(pixel[1]) + "," + std::to_string(pixel[2]) + ")");
		NOTICE_LOG(RENDERER, "Metal validation PASS: %s (%s)", name, perPixel ? "OIT" : "standard");
	};
	check("opaque production geometry", 255, 0, 0);
	{
		// Submit ordered snapshots without waiting for each render on the CPU.
		// Decreasing Z catches stale depth; resizing catches in-flight lifetime errors.
		const auto savedVertices = fixture.verts;
		const bool oldNative = config::NativeDepthInterpolation;
		PolyParam behind = opaque; behind.first = 4;
		fixture.global_param_op.push_back(behind); fixture.render_passes[0].op_count = 2;
		for (bool native : {false, true}) {
			config::NativeDepthInterpolation = native;
			std::vector<id<MTLTexture>> pending;
			for (int i = 0; i < 13; ++i) {
				fixture.framebufferWidth = i >= 4 && i < 8 ? 80 : 64;
				fixture.framebufferHeight = i >= 4 && i < 8 ? 60 : 48;
				for (int j = 0; j < 8; ++j) {
					auto& v = fixture.verts[j]; v.z = (j < 4 ? 4.f : 2.f) / (i + 1);
					v.col[0] = v.col[1] = v.col[2] = 0; v.col[3] = 255;
					v.col[j < 4 ? i % 3 : (i + 1) % 3] = 255;
				}
				if (!Render()) throw RendererException("Metal queued depth render failed");
				context()->flushRenderCommands();
				id<MTLTexture> snapshot = context()->makeTexture(fixture.framebufferWidth,
					fixture.framebufferHeight, MTLPixelFormatRGBA8Unorm);
				FCMetal4Commands * copy = [context()->queue() commandBuffer];
				FCMetal4ComputeEncoder * blit = [copy transferEncoder];
				if (!copy || !blit) throw RendererException("Metal queued depth snapshot allocation failed");
				[blit copyFromTexture:screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
					sourceSize:MTLSizeMake(snapshot.width, snapshot.height, 1) toTexture:snapshot
					destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
				[blit endEncoding]; context()->submitCommandBuffer(copy); pending.push_back(snapshot);
			}
			for (size_t i = 0; i < pending.size(); ++i) {
				std::vector<u8> pixels; context()->readTexture(pending[i], pixels);
				for (size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
					for (size_t channel = 0; channel < 3; ++channel)
						if (std::abs(int(pixels[pixel + channel]) - (channel == i % 3 ? 255 : 0)) > 1)
							throw RendererException("Metal queued depth or resized attachment changed a frame");
					if (pixels[pixel + 3] != 255) throw RendererException("Metal queued depth snapshot alpha changed");
				}
			}
			NOTICE_LOG(RENDERER, "Metal validation PASS: queued depth reset and resized attachment lifetime (%s, native-depth=%d)", perPixel ? "OIT" : "standard", native);
		}
		fixture.verts = savedVertices; fixture.global_param_op.resize(1); fixture.render_passes[0].op_count = 1;
		fixture.framebufferWidth = 64; fixture.framebufferHeight = 48;
		config::NativeDepthInterpolation = oldNative;
	}
	{
		const auto savedVertices = fixture.verts;
		const auto savedOpaque = fixture.global_param_op;
		const bool oldNative = config::NativeDepthInterpolation, oldFog = config::Fog;
		const int oldFilter = config::TextureFiltering;
		config::NativeDepthInterpolation = false; config::Fog = true; config::TextureFiltering = 1;
		MetalTexture texture; texture.tex_type = TextureType::_8888;
		std::array<u8, 8 * 8 * 4> data{};
		for (size_t i = 0; i < data.size(); ++i) data[i] = u8(19 + i * 73);
		texture.UploadToGPU(8, 8, data.data(), false);
		auto& poly = fixture.global_param_op[0];
		poly.pcw.Texture = poly.pcw.Offset = poly.pcw.Gouraud = 1;
		poly.texture = &texture; poly.tsp.FogCtrl = 0; poly.tsp.ShadInstr = 3; poly.tsp.IgnoreTexA = 1;
		for (int i = 0; i < 4; ++i) {
			auto& v = fixture.verts[i]; v.z = .125f * (i + 1);
			v.u = (i & 1) ? .875f : .0625f; v.v = (i & 2) ? .9375f : .125f;
			for (int c = 0; c < 4; ++c) { v.col[c] = u8(40 + 37 * i + 13 * c); v.spc[c] = u8(3 + 17 * i + c); }
		}
		if (!Render()) throw RendererException("Metal linear reference render failed");
		const u64 key = pipelineSelection.key;
		if (materialPipelines.at(key) || !(metalVertexFunctionFlags(drawState.uniforms.flags,
				drawState.uniforms.tsp0, drawState.uniforms.tsp1) & (1u << 9)))
			throw RendererException("Metal linear fixture did not start with eligible generic shader");
		std::vector<u8> reference; context()->readTexture(screen, reference);
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		for (;;) {
			collectMaterialPipelines();
			auto found = materialPipelines.find(key);
			if (found != materialPipelines.end() && found->second) break;
			if (std::chrono::steady_clock::now() >= deadline) throw RendererException("Metal linear varying pipeline did not compile");
			[NSThread sleepForTimeInterval:.001];
		}
		if (!Render() || !pipelineSelection.material || pipelineSelection.state != materialPipelines.at(key))
			throw RendererException("Metal linear varying specialization was not used");
		std::vector<u8> candidate; context()->readTexture(screen, candidate);
		if (candidate != reference) throw RendererException("Metal linear varying production pixels differ");
		fixture.verts = savedVertices; fixture.global_param_op = savedOpaque;
		config::NativeDepthInterpolation = oldNative; config::Fog = oldFog; config::TextureFiltering = oldFilter;
		NOTICE_LOG(RENDERER, "Metal validation PASS: linear varying cache contract and generic pixels (%s)", perPixel ? "OIT" : "standard");
	}
	{
		const auto savedVertices = fixture.verts;
		const auto savedOpaque = fixture.global_param_op;
		const auto savedPunch = fixture.global_param_pt;
		const auto savedPasses = fixture.render_passes;
		const bool oldNativeDepth = config::NativeDepthInterpolation;
		const u32 savedAlphaRef = PT_ALPHA_REF;
		PT_ALPHA_REF = 128;
		for (bool native : {false, true}) for (bool smooth : {false, true}) for (bool punch : {false, true})
			for (float z : {.125f, 1.75f, 10000.f}) for (float ratio : {1.f, 256.f, 65536.f}) {
			config::NativeDepthInterpolation = native;
			fixture.verts = savedVertices;
			fixture.global_param_op = savedOpaque;
			fixture.global_param_pt = savedPunch; fixture.render_passes = savedPasses;
			fixture.global_param_op[0].pcw.Gouraud = smooth;
			PolyParam front = fixture.global_param_op[0]; front.first = 4;
			if (punch) { fixture.global_param_pt.push_back(front); fixture.render_passes[0].pt_count = 1; }
			else { fixture.global_param_op.push_back(front); fixture.render_passes[0].op_count = 2; }
			for (int i = 4; i < 8; ++i) {
				fixture.verts[i].z = ratio == 1 ? z * (1 + .25f * (i & 3))
					: z * std::pow(ratio, float(i & 3) / 3) / ratio;
				fixture.verts[i].col[3] = punch && (i & 1) ? 0 : 255;
			}
			disableDepthBounds = true;
			if (!Render()) throw RendererException("Metal depth-bound reference failed");
			std::vector<u8> reference; context()->readTexture(screen, reference);
			if (punch && z > 1 && ratio == 1) {
				bool red = false, blue = false;
				for (size_t i = 0; i < reference.size(); i += 4) {
					red |= reference[i] == 255 && reference[i + 2] == 0;
					blue |= reference[i] == 0 && reference[i + 2] == 255;
				}
				if (!red || !blue) throw RendererException("Metal bound fixture did not cover alpha rejection and acceptance");
			}
			disableDepthBounds = false;
			if (!Render()) throw RendererException("Metal depth-bound candidate failed");
			// Wait for the actual bounded specialization, so this comparison
			// cannot accidentally pass by exercising its generic fallback.
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			for (;;) {
				collectMaterialPipelines();
				bool pending = false;
				for (const auto& entry : materialPipelines)
					if ((entry.first & (1ull << 47)) && !entry.second) pending = true;
				if (!pending) break;
				if (std::chrono::steady_clock::now() >= deadline)
					throw RendererException("Metal bounded specialization did not complete");
				[NSThread sleepForTimeInterval:.001];
			}
			if (!Render() || !(pipelineSelection.key & (1ull << 47)))
				throw RendererException("Metal bounded specialization was not selected");
			std::vector<u8> candidate; context()->readTexture(screen, candidate);
			if (reference != candidate) throw RendererException("Metal bounded production pixels differ");
		}
		fixture.verts = savedVertices; fixture.global_param_op = savedOpaque; fixture.render_passes = savedPasses;
		fixture.global_param_pt = savedPunch;
		config::NativeDepthInterpolation = oldNativeDepth;
		PT_ALPHA_REF = savedAlphaRef;
		if (!(conservativeDepthBound(opaque, 0, 4) > 0))
			throw RendererException("Metal valid depth bound rejected");
		for (float invalid : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
			fixture.verts[0].z = invalid;
			if (conservativeDepthBound(opaque, 0, 4) != 0)
				throw RendererException("Metal invalid depth bound accepted");
		}
		fixture.verts = savedVertices;
		const auto savedDepthMatrix = frame.ndc;
		frame.ndc = glm::mat4(1);
		for (int i = 0; i < 4; ++i) {
			fixture.verts[i].x = (i & 1) ? 1.f : -1.f;
			fixture.verts[i].y = (i & 2) ? 1.f : -1.f;
		}
		for (bool subnormalMatrix : {false, true}) {
			const float z = subnormalMatrix ? 1.e30f : 1.e-40f;
			const float scale = subnormalMatrix ? 1.e-40f : 1.e30f;
			// The double-precision product alone is inside the old accepted
			// range, while Metal may flush a subnormal operand to zero.
			const double transformed = double(z) * scale;
			if (!(transformed >= 1.e-12 && transformed <= 1.e5))
				throw RendererException("Metal subnormal depth fixture outside CPU range");
			frame.ndc[2][2] = scale;
			for (int i = 0; i < 4; ++i) fixture.verts[i].z = z;
			bool coordinates = true;
			if (conservativeDepthBound(opaque, 0, 4, &coordinates) != 0 || coordinates)
				throw RendererException("Metal subnormal depth proof accepted");
		}
		for (bool smallestMatrix : {false, true}) {
			const float smallest = std::numeric_limits<float>::min();
			frame.ndc[2][2] = smallestMatrix ? smallest : 1.e30f;
			for (int i = 0; i < 4; ++i) fixture.verts[i].z = smallestMatrix ? 1.e30f : smallest;
			bool coordinates = false;
			if (!(conservativeDepthBound(opaque, 0, 4, &coordinates) > 0) || !coordinates)
				throw RendererException("Metal smallest normal depth operand rejected");
		}
		frame.ndc = savedDepthMatrix; fixture.verts = savedVertices;
		if (conservativeDepthBound(opaque, u32(fixture.idx.size()), 4) != 0)
			throw RendererException("Metal out-of-range depth bound accepted");
		fixture.verts[0].z = .000001f;
		if (conservativeDepthBound(opaque, 0, 4) != 0)
			throw RendererException("Metal excessive depth-bound Z range accepted");
		fixture.verts = savedVertices;
		NOTICE_LOG(RENDERER, "Metal validation PASS: bounded production pipelines and depth fallbacks (%s)", perPixel ? "OIT" : "standard");
	}
	{
		const auto savedVertices = fixture.verts;
		const auto savedOpaque = fixture.global_param_op;
		const bool oldNative = config::NativeDepthInterpolation;
		const int oldFilter = config::TextureFiltering, oldAnisotropy = config::AnisotropicFiltering;
		config::NativeDepthInterpolation = false; config::TextureFiltering = 0; config::AnisotropicFiltering = 1;
		MetalTexture texture; texture.tex_type = TextureType::_8888;
		std::array<u8, 8 * 8 * 4> data{};
		for (size_t i = 0; i < data.size(); ++i) data[i] = u8(i * 73 + i / 7);
		texture.UploadToGPU(8, 8, data.data(), true);
		auto& poly = fixture.global_param_op[0];
		poly.pcw.Texture = 1; poly.texture = &texture; poly.tsp.FilterMode = 1;
		poly.tsp.IgnoreTexA = 1; poly.tsp.ShadInstr = 3;
		for (int i = 0; i < 4; ++i) {
			auto& v = fixture.verts[i]; v.y = (i & 2) ? 240 : 0;
			v.u = (i & 1) ? .875f : .0625f; v.v = (i & 2) ? .9375f : .125f;
			v.z = .125f * (i + 1);
			for (int c = 0; c < 4; ++c) v.col[c] = u8(40 + 37 * i + 13 * c);
		}
		auto material = [&] { auto p = uniforms(poly, ListType_Opaque); normalizePoly(p, PolyPass::Direct); return p; };
		const bool supported = allowsColourUV(poly, material());
		if (supported) {
			auto image = [&](bool relaxed) {
				disableColourUV = !relaxed;
				if (!Render()) throw RendererException("Metal colour UV production render failed");
				const u64 key = pipelineSelection.key;
				if (bool(key & (1ull << 48)) != relaxed) {
					bool coordinates = false;
					const float bound = conservativeDepthBound(poly, 0, 4, &coordinates);
					NOTICE_LOG(RENDERER, "Metal colour UV fixture expected=%d flags=%u allowed=%d bound=%g coordinates=%d matrixX=%g/%g matrixY=%g/%g Z=%g", relaxed, drawState.uniforms.flags, allowsColourUV(poly, material()), bound, coordinates, frame.ndc[0][0], frame.ndc[3][0], frame.ndc[1][1], frame.ndc[3][1], frame.ndc[2][2]);
					throw RendererException("Metal colour UV production family was not selected");
				}
				const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
				for (;;) {
					collectMaterialPipelines();
					auto found = materialPipelines.find(key);
					if (found != materialPipelines.end() && found->second) break;
					if (std::chrono::steady_clock::now() >= deadline)
						throw RendererException("Metal colour UV specialization did not finish");
					[NSThread sleepForTimeInterval:.001];
				}
				if (!Render() || pipelineSelection.state != materialPipelines.at(key)
						|| bool(drawState.uniforms.flags & ColourUV) != relaxed)
					throw RendererException("Metal colour UV specialized pipeline was not used");
				std::vector<u8> pixels; context()->readTexture(screen, pixels); return pixels;
			};
			for (auto type : {TextureType::_8888, TextureType::_565, TextureType::_5551, TextureType::_4444}) {
				if (type != TextureType::_8888 && !context()->supportsPackedTextures()) continue;
				texture.tex_type = type; texture.UploadToGPU(8, 8, data.data(), true);
				if (!allowsColourUV(poly, material())) throw RendererException("Metal normalized colour UV format rejected");
				for (bool repeated : {false, true}) {
					for (int i = 0; i < 4; ++i) {
						fixture.verts[i].u = repeated ? ((i & 1) ? 4.f : -4.f) : ((i & 1) ? .875f : .0625f);
						fixture.verts[i].v = repeated ? ((i & 2) ? 3.9375f : -3.875f) : ((i & 2) ? .9375f : .125f);
					}
					for (bool smooth : {false, true}) for (bool fogEnabled : {false, true})
						for (bool alpha : {false, true}) for (float span : {1.f, 256.f, 65536.f}) {
						poly.pcw.Gouraud = smooth; poly.tsp.UseAlpha = alpha; poly.tsp.FogCtrl = fogEnabled ? 0 : 2;
						for (int i = 0; i < 4; ++i) fixture.verts[i].z = std::pow(span, float(i) / 3) / span;
						auto reference = image(false), candidate = image(true);
						if (reference.size() != candidate.size()) throw RendererException("Metal colour UV image size mismatch");
						for (size_t i = 0; i < reference.size(); ++i)
							if (std::abs(int(reference[i]) - int(candidate[i])) > (i % 4 == 3 ? 0 : 1))
								throw RendererException("Metal colour UV production pixel mismatch");
					}
				}
			}
			disableColourUV = false;
			poly.pcw.Gouraud = 1; poly.tsp.UseAlpha = 1; poly.tsp.FogCtrl = 2;
			auto p = material();
			for (u32 flag : {NativeDepth, Naomi2, TwoVolumes, AlphaTest, Bump, 1u << 16, 2u << 16}) {
				auto invalid = p; invalid.flags |= flag;
				if (allowsColourUV(poly, invalid)) throw RendererException("Metal unsafe colour UV material accepted");
			}
			for (int variant : {0, 1}) {
				auto invalid = p;
				if (variant) invalid.flags &= ~Texture; else invalid.tsp0 &= ~(1u << 19);
				if (allowsColourUV(poly, invalid)) throw RendererException("Metal alpha-dependent colour UV sample accepted");
			}
			config::TextureFiltering = 1;
			if (allowsColourUV(poly, p)) throw RendererException("Metal nearest colour UV accepted");
			config::TextureFiltering = 0; poly.tsp.FilterMode = 0;
			if (allowsColourUV(poly, p)) throw RendererException("Metal material nearest colour UV accepted");
			config::TextureFiltering = 2;
			if (!allowsColourUV(poly, p)) throw RendererException("Metal forced linear colour UV rejected");
			config::AnisotropicFiltering = 2;
			if (allowsColourUV(poly, p)) throw RendererException("Metal anisotropic colour UV accepted");
			config::AnisotropicFiltering = 1; poly.tsp.FilterMode = 1; config::TextureFiltering = 0;
			for (unsigned src = 0; src < 8; ++src) for (unsigned dst = 0; dst < 8; ++dst) {
				poly.tsp.SrcInstr = src; poly.tsp.DstInstr = dst;
				const bool expected = (src == 1 && dst == 0) || (src == 4 && dst == 5) || (src == 6 && dst == 7);
				if (allowsColourUV(poly, p) != expected) throw RendererException("Metal colour UV blend guard mismatch");
			}
			poly.tsp.SrcInstr = 1; poly.tsp.DstInstr = 0;
			const auto imageSaved = texture.image;
			texture.image = context()->makeTexture(1024, 1, MTLPixelFormatRGBA8Unorm);
			if (allowsColourUV(poly, p)) throw RendererException("Metal large colour UV texture accepted");
			texture.image = imageSaved; texture.gpuPalette = true;
			if (allowsColourUV(poly, p)) throw RendererException("Metal indexed colour UV texture accepted");
			texture.gpuPalette = false;
			texture.image = context()->makeTexture(8, 8, MTLPixelFormatR8Unorm);
			if (allowsColourUV(poly, p)) throw RendererException("Metal unsupported colour UV format accepted");
			texture.image = imageSaved;
			bool coordinates = false;
			if (!(conservativeDepthBound(poly, 0, 4, &coordinates) > 0) || !coordinates)
				throw RendererException("Metal bounded colour UV coordinates rejected");
			const auto valid = fixture.verts;
			for (float boundary : {-ColourUVCoordinateLimit, ColourUVCoordinateLimit}) {
				fixture.verts[0].u = boundary; fixture.verts[0].v = -boundary;
				if (!(conservativeDepthBound(poly, 0, 4, &coordinates) > 0) || !coordinates)
					throw RendererException("Metal colour UV inclusive boundary rejected");
			}
			fixture.verts = valid;
			for (float invalid : {std::nextafter(ColourUVCoordinateLimit, std::numeric_limits<float>::infinity()),
					-std::nextafter(ColourUVCoordinateLimit, std::numeric_limits<float>::infinity()),
					std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
				fixture.verts[0].u = invalid; coordinates = true;
				if (!(conservativeDepthBound(poly, 0, 4, &coordinates) > 0) || coordinates)
					throw RendererException("Metal invalid colour UV coordinates accepted");
			}
			fixture.verts = valid; fixture.verts[0].x = 2048; coordinates = true;
			if (!(conservativeDepthBound(poly, 0, 4, &coordinates) > 0) || coordinates)
				throw RendererException("Metal excessive colour UV clipping accepted");
			fixture.verts = valid; fixture.verts[0].z = 0; coordinates = true;
			if (conservativeDepthBound(poly, 0, 4, &coordinates) != 0 || coordinates)
				throw RendererException("Metal invalid colour UV Z did not clear the proof");
			NOTICE_LOG(RENDERER, "Metal validation PASS: colour UV production images, keys and consumer guards (%s)", perPixel ? "OIT" : "standard");
		}
		disableColourUV = false; fixture.verts = savedVertices; fixture.global_param_op = savedOpaque;
		config::NativeDepthInterpolation = oldNative; config::TextureFiltering = oldFilter; config::AnisotropicFiltering = oldAnisotropy;
	}
	if (config::loadBool("log", "MetalBackgroundBenchmarks", false)) {
		const bool oldNativeDepth = config::NativeDepthInterpolation;
		fixture.framebufferWidth = 4096; fixture.framebufferHeight = 3072;
		fixture.global_param_op[0].isp.DepthMode = 7;
		for (bool native : {false, true}) for (bool write : {false, true}) {
			config::NativeDepthInterpolation = native;
			fixture.global_param_op[0].isp.ZWriteDis = !write;
			for (int round = 0; round < 8; ++round) for (bool candidate : {false, true, true, false}) {
				disableBackgroundClear = !candidate;
				NOTICE_LOG(RENDERER, "Metal background benchmark begin %s native-depth=%d zwrite=%d round=%d candidate=%d", perPixel ? "OIT" : "standard", native, write, round, candidate);
				if (!Render()) throw RendererException("Metal background benchmark render failed");
				context()->waitIdle();
				NOTICE_LOG(RENDERER, "Metal background benchmark end");
			}
		}
		disableBackgroundClear = false; config::NativeDepthInterpolation = oldNativeDepth;
		fixture.framebufferWidth = 64; fixture.framebufferHeight = 48;
		fixture.global_param_op[0] = opaque;
	}
	// A presented image must survive independently of the next render. Test
	// all output pixels, including the edges, before permitting copy elision.
	{
		const auto savedVertices = fixture.verts;
		const auto savedIndices = fixture.idx;
		const bool oldNativeDepth = config::NativeDepthInterpolation;
		for (bool native : {false, true}) {
			config::NativeDepthInterpolation = native;
			auto seed = [&] {
				fixture.verts = savedVertices; fixture.idx = savedIndices;
				fixture.global_param_op[0] = opaque;
				fixture.global_param_op[0].isp.DepthMode = 7;
				fixture.fbClip = Rect({0, 0}, {640, 480});
				fixture.clearFramebuffer = true;
				for (int i = 0; i < 4; ++i) { fixture.verts[i].col[0] = 0; fixture.verts[i].col[2] = 255; }
				check("framebuffer history seed", 0, 0, 255);
				if (!Present()) throw RendererException("Metal color-history fixture was not presented");
				context()->present();
				fixture.clearFramebuffer = false;
				fixture.verts = savedVertices;
			};
			auto verifyHistory = [&](const char *name, bool overwrite, int mode) {
				if (!Render() || replacesFramebufferColor() != overwrite)
					throw RendererException(std::string("Metal color-history selection mismatch: ") + name
						+ " scissor=" + std::to_string(baseScissor.origin.x) + "," + std::to_string(baseScissor.origin.y)
						+ ":" + std::to_string(baseScissor.size.x) + "," + std::to_string(baseScissor.size.y)
						+ " ndc=" + std::to_string(frame.ndc[0][0]) + "," + std::to_string(frame.ndc[1][1])
						+ "," + std::to_string(frame.ndc[2][2]) + "," + std::to_string(frame.ndc[3][0])
						+ "," + std::to_string(frame.ndc[3][1]));
				std::vector<u8> pixels; context()->readTexture(screen, pixels);
				for (int y = 0; y < 48; ++y) for (int x = 0; x < 64; ++x) {
					const u8 *p = pixels.data() + (y * 64 + x) * 4;
					const int red = mode == 0 ? 255 : mode == 1 ? (x < 32 ? 255 : 0) : mode == 2 ? 128 : 0;
					const int blue = mode == 0 ? 0 : mode == 1 ? (x < 32 ? 0 : 255) : mode == 2 ? 127 : 255;
					if (std::abs(int(p[0]) - red) > 1 || p[1] != 0 || std::abs(int(p[2]) - blue) > 1)
						throw RendererException(std::string("Metal color-history pixel mismatch: ") + name);
				}
				std::vector<u8> previous; context()->readTexture(context()->lastFrame(), previous);
				for (size_t i = 0; i < previous.size(); i += 4)
					if (previous[i] != 0 || previous[i + 1] != 0 || previous[i + 2] != 255)
						throw RendererException("Metal overwrote the independently presented image");
				NOTICE_LOG(RENDERER, "Metal validation PASS: %s (%s, native-depth=%d)", name, perPixel ? "OIT" : "standard", native);
			};
			seed(); verifyHistory("full opaque overwrite without framebuffer copy", true, 0);
			seed(); for (int i : {1, 3}) fixture.verts[i].x = 320;
			verifyHistory("partial geometry retains preceding color", false, 1);
			seed(); fixture.fbClip.size.x = 320;
			verifyHistory("partial scissor retains preceding color", false, 1);
			seed(); fixture.global_param_op[0].tsp.SrcInstr = 4; fixture.global_param_op[0].tsp.DstInstr = 5;
			for (int i = 0; i < 4; ++i) fixture.verts[i].col[3] = 128;
			verifyHistory("blended background retains preceding color", false, 2);
			seed(); fixture.global_param_op[0].isp.DepthMode = 0;
			verifyHistory("rejected background retains preceding color", false, 3);
			// A malformed fourth corner cannot justify discarding color. Its
			// surviving triangle leaves the opposite corner of the seed intact.
			seed(); fixture.idx[3] = 2;
			if (!Render() || replacesFramebufferColor()) throw RendererException("Metal accepted an incomplete background rectangle");
			std::vector<u8> pixels; context()->readTexture(screen, pixels);
			const u8 *corner = pixels.data() + (40 * 64 + 56) * 4;
			if (corner[0] != 0 || corner[1] != 0 || corner[2] != 255)
				throw RendererException("Metal incomplete rectangle lost preceding color");
			NOTICE_LOG(RENDERER, "Metal validation PASS: incomplete rectangle retains preceding color (%s, native-depth=%d)", perPixel ? "OIT" : "standard", native);
		}
		config::NativeDepthInterpolation = oldNativeDepth;
		fixture.verts = savedVertices; fixture.idx = savedIndices;
		fixture.global_param_op[0] = opaque;
		fixture.fbClip = Rect({0, 0}, {640, 480}); fixture.clearFramebuffer = true;
	}
	// Compare the constant-color clear with the original GPU fragment path,
	// including alpha consumed by a following blend and preserved depth tests.
	{
		const auto savedVertices = fixture.verts;
		const bool oldNativeDepth = config::NativeDepthInterpolation;
		const u8 values[] = {0, 1, 63, 127, 128, 254, 255};
		const float depths[] = {std::nextafter(1e-20f, 1.f), 1e-11f, 1e-3f, 1.f, 32.f, 4096.f, 100000.f};
		fixture.global_param_op.reserve(2);
		for (bool native : {false, true}) for (bool smooth : {false, true}) {
			config::NativeDepthInterpolation = native;
			for (size_t sample = 0; sample < sizeof(values); ++sample) {
				fixture.global_param_op[0] = opaque;
				auto& bg = fixture.global_param_op[0];
				bg.isp.DepthMode = 7; bg.pcw.Gouraud = smooth;
				bg.tsp.UseAlpha = sample != 0;
				for (int i = 0; i < 4; ++i) {
					auto& v = fixture.verts[i];
					v.z = depths[sample];
					for (int c = 0; c < 4; ++c) v.col[c] = values[(sample + c) % sizeof(values)];
				}
				for (int mode = 0; mode < 4; ++mode) {
					bg.isp.ZWriteDis = mode == 2;
					if (mode != 0) {
						PolyParam next = opaque; next.first = 4;
						if (mode == 1) { next.tsp.SrcInstr = 6; next.tsp.DstInstr = 7; }
						fixture.global_param_op.push_back(next); fixture.render_passes[0].op_count = 2;
						for (int i = 4; i < 8; ++i) {
							fixture.verts[i].z = mode == 1 ? 2 : fixture.verts[0].z / 2;
							fixture.verts[i].col[0] = 0; fixture.verts[i].col[1] = 0;
							fixture.verts[i].col[2] = 255; fixture.verts[i].col[3] = 128;
						}
					}
					disableBackgroundClear = true;
					if (!Render()) throw RendererException("Metal background reference render failed");
					std::vector<u8> reference; context()->readTexture(screen, reference);
					disableBackgroundClear = false;
					if (!Render() || !backgroundColorCleared || skipBackgroundDepth != (mode == 2))
						throw RendererException("Metal constant background color/depth path was not selected: sample="
							+ std::to_string(sample) + " mode=" + std::to_string(mode) + " native=" + std::to_string(native));
					std::vector<u8> candidate; context()->readTexture(screen, candidate);
					if (reference != candidate) throw RendererException("Metal background clear changed color, alpha or depth behavior");
					fixture.global_param_op.resize(1); fixture.render_passes[0].op_count = 1;
				}
			}
			NOTICE_LOG(RENDERER, "Metal validation PASS: constant background exact color, alpha blend and disabled depth writes (%s, native-depth=%d, smooth=%d)", perPixel ? "OIT" : "standard", native, smooth);
		}
		config::NativeDepthInterpolation = oldNativeDepth;
		fixture.verts = savedVertices; fixture.global_param_op[0] = opaque;
	}
	{
		const auto savedVertices = fixture.verts;
		const bool oldNativeDepth = config::NativeDepthInterpolation, oldFog = config::Fog;
		MetalTexture texture; texture.tex_type = TextureType::_8888;
		const u8 green[] = {0, 255, 0, 255}; texture.UploadToGPU(1, 1, green, false);
		config::Fog = true;
		for (bool native : {false, true}) {
			config::NativeDepthInterpolation = native;
			for (int variant = 0; variant < 7; ++variant) {
				fixture.verts = savedVertices; fixture.global_param_op[0] = opaque;
				auto& bg = fixture.global_param_op[0]; bg.isp.DepthMode = 7;
				if (variant == 0) fixture.verts[1].col[1] = 255; // Genuine color gradient.
				if (variant == 1) {
					bg.tsp.ColorClamp = 1;
					fixture.fog_clamp_min.full = 0x40404040; fixture.fog_clamp_max.full = 0x80808080;
				}
				if (variant == 2) bg.tsp.FogCtrl = 3;
				if (variant == 3) { bg.tsp.FogCtrl = 1; bg.pcw.Offset = 1; }
				if (variant == 4) bg.pcw.Shadow = 1;
				if (variant == 5) { bg.pcw.Texture = 1; bg.texture = &texture; bg.tsp.ShadInstr = 0; }
				if (variant == 6) { bg.tsp1 = bg.tsp; bg.tcw1.full = 0; }
				disableBackgroundClear = true;
				if (!Render()) throw RendererException("Metal background fallback reference failed");
				std::vector<u8> reference; context()->readTexture(screen, reference);
				disableBackgroundClear = false;
				if (!Render() || backgroundColorCleared) throw RendererException("Metal background incorrectly replaced a varying fragment color");
				std::vector<u8> candidate; context()->readTexture(screen, candidate);
				if (reference != candidate) throw RendererException("Metal background fallback changed fragment colors");
				fixture.fog_clamp_min.full = 0; fixture.fog_clamp_max.full = ~0u;
			}
			NOTICE_LOG(RENDERER, "Metal validation PASS: background gradient, clamp, fog, shadow, texture and two-volume fallbacks (%s, native-depth=%d)", perPixel ? "OIT" : "standard", native);
		}
		config::NativeDepthInterpolation = oldNativeDepth; config::Fog = oldFog;
		fixture.verts = savedVertices; fixture.global_param_op[0] = opaque;
	}
	{
		const auto savedVertices = fixture.verts;
		fixture.global_param_op[0] = opaque; fixture.global_param_op[0].isp.DepthMode = 7;
		auto renderPair = [&]() {
			fixture.verts = savedVertices; fixture.clearFramebuffer = true;
			if (!Render()) throw RendererException("Metal background continuation seed failed");
			fixture.clearFramebuffer = false;
			for (int i = 0; i < 4; ++i) { fixture.verts[i].col[0] = 0; fixture.verts[i].col[2] = 255; }
			if (!Render()) throw RendererException("Metal background continuation failed");
		};
		disableBackgroundClear = true; renderPair();
		std::vector<u8> reference; context()->readTexture(screen, reference);
		disableBackgroundClear = false; renderPair();
		if (!perPixel && backgroundColorCleared) throw RendererException("Metal reopened a retained encoder for a background clear");
		std::vector<u8> candidate; context()->readTexture(screen, candidate);
		if (reference != candidate) throw RendererException("Metal background continuation changed the pending image");
		NOTICE_LOG(RENDERER, "Metal validation PASS: background preserves pending render continuation (%s)", perPixel ? "OIT" : "standard");
		fixture.verts = savedVertices; fixture.global_param_op[0] = opaque; fixture.clearFramebuffer = true;
	}
	{
		fixture.global_param_op[0] = opaque; fixture.global_param_op[0].isp.DepthMode = 7;
		fixture.global_param_op[0].isp.ZWriteDis = 1;
		PolyParam shadowed = opaque; shadowed.first = 4; shadowed.pcw.Shadow = 1;
		fixture.global_param_op.push_back(shadowed); fixture.render_passes[0].op_count = 2;
		fixture.modtrig = {{0,0,3, 640,0,3, 0,480,3}, {640,0,3, 640,480,3, 0,480,3}};
		ModifierVolumeParam modifier; modifier.init(); modifier.count = 2;
		modifier.isp.DepthMode = 1; modifier.isp.VolumeLast = 1;
		fixture.global_param_mvo.push_back(modifier); fixture.render_passes[0].mvo_count = 1;
		disableBackgroundClear = true;
		if (!Render()) throw RendererException("Metal background stencil reference failed");
		std::vector<u8> reference; context()->readTexture(screen, reference);
		disableBackgroundClear = false;
		if (!Render() || backgroundColorCleared == perPixel || skipBackgroundDepth == perPixel)
			throw RendererException("Metal background stencil path selection failed");
		std::vector<u8> candidate; context()->readTexture(screen, candidate);
		if (reference != candidate) throw RendererException("Metal background changed following modifier stencil");
		NOTICE_LOG(RENDERER, "Metal validation PASS: background preserves following modifier stencil and OIT shadow fallback (%s)", perPixel ? "OIT" : "standard");
		fixture.global_param_op.resize(1); fixture.render_passes[0].op_count = 1;
		fixture.global_param_op[0] = opaque;
		fixture.global_param_mvo.clear(); fixture.modtrig.clear(); fixture.render_passes[0].mvo_count = 0;
	}
	// Exercise the two-bit palette flag with every PVR filter, including mode 3.
	{
		const int oldFilter = config::TextureFiltering;
		MetalTexture indexed; indexed.gpuPalette = true; indexed.tex_type = TextureType::_8;
		const u8 indices[] = {0, 1, 0, 1}; indexed.UploadToGPU(2, 2, indices, false);
		std::vector<u32> colors(1024); colors[16] = 0xff0000ff; colors[17] = 0xff00ff00;
		palette.UploadToGPU(1024, 1, reinterpret_cast<const u8 *>(colors.data()), false);
		auto& poly = fixture.global_param_op[0];
		poly.pcw.Texture = 1; poly.texture = &indexed; poly.tcw.PixelFmt = PixelPal4; poly.tcw.PalSelect = 1;
		poly.tsp.ClampU = poly.tsp.ClampV = 1;
		for (auto& v : fixture.verts) v.u = v.v = .5f;
		config::TextureFiltering = 0;
		for (int mode = 0; mode < 4; ++mode) {
			poly.tsp.FilterMode = mode;
			check(("GPU palette filter mode " + std::to_string(mode)).c_str(), mode == 0 ? 0 : 128, mode == 0 ? 255 : 128, 0);
		}
		config::TextureFiltering = 1; check("GPU palette forced nearest", 0, 255, 0);
		poly.tsp.FilterMode = 0; config::TextureFiltering = 2; check("GPU palette forced linear", 128, 128, 0);
		config::TextureFiltering = oldFilter; poly = opaque;
		for (auto& v : fixture.verts) v.u = v.v = 0;
	}
	if (!perPixel) {
		// Separate Render calls may append to one screen image before Present.
		for (int i = 0; i < 4; ++i) { fixture.verts[i].z = 2; fixture.verts[i].col[0] = 0; fixture.verts[i].col[2] = 255; }
		check("pending near image", 0, 0, 255);
		fixture.clearFramebuffer = false;
		for (int i = 0; i < 4; ++i) { fixture.verts[i].z = 1; fixture.verts[i].col[0] = 255; fixture.verts[i].col[2] = 0; }
		check("separate render retains pending depth", 0, 0, 255);
		if (!Present()) throw RendererException("Metal depth fixture was not presented"); context()->present();
		check("present starts fresh depth", 255, 0, 0);
		fixture.clearFramebuffer = true;
	}
	// The second pass contains new geometry behind the first pass.
	{
		const auto saved = fixture.verts;
		for (int i = 0; i < 4; ++i) { fixture.verts[i].z = 2; fixture.verts[i].col[0] = 0; fixture.verts[i].col[2] = 255; }
		for (int i = 4; i < 8; ++i) { fixture.verts[i].z = 1; fixture.verts[i].col[0] = 255; fixture.verts[i].col[2] = 0; fixture.verts[i].col[3] = 255; }
		PolyParam behind = opaque; behind.first = 4;
		fixture.global_param_op.push_back(behind); fixture.render_passes.push_back(pass); fixture.render_passes[1].op_count = 2;
		check("multipass rejects new geometry behind prior depth", 0, 0, 255);
		fixture.global_param_op.resize(1); fixture.render_passes.resize(1); fixture.verts = saved;
	}
	// Two restarted strips with distinct last-vertex flat colors. The
	// non-flat input colors must not leak across triangles or restarts.
	{
		const auto saved = fixture.verts; const auto savedIndices = fixture.idx;
		fixture.global_param_op[0].pcw.Gouraud = 0;
		fixture.global_param_op[0].count = 9;
		fixture.idx = {0, 1, 2, 3, ~0u, 4, 5, 6, 7};
		for (int i = 0; i < 8; ++i) {
			auto& v = fixture.verts[i]; v.z = 1; v.x = (i < 4 ? 0 : 320) + ((i & 1) ? 320 : 0);
			v.col[0] = i < 4 ? 255 : 0; v.col[1] = i >= 4 ? 255 : 0; v.col[2] = 0; v.col[3] = 255;
		}
		fixture.verts[0].col[0] = fixture.verts[1].col[0] = 0;
		fixture.verts[4].col[1] = fixture.verts[5].col[1] = 0;
		if (!Render()) throw RendererException("Metal restarted flat render failed");
		std::vector<u8> pixels; context()->readTexture(screen, pixels);
		for (int y : {8, 40}) for (int x : {8, 24, 40, 56}) {
			const u8 *p = pixels.data() + (y * 64 + x) * 4;
			if (p[0] != (x < 32 ? 255 : 0) || p[1] != (x < 32 ? 0 : 255) || p[2] != 0)
				throw RendererException("Metal last provoking vertex or restart mismatch");
		}
		NOTICE_LOG(RENDERER, "Metal validation PASS: last provoking vertex and restarted flat strips (%s)", perPixel ? "OIT" : "standard");
		fixture.verts = saved; fixture.idx = savedIndices; fixture.global_param_op[0] = opaque;
	}
	PolyParam tr = opaque; tr.first = 4; tr.tsp.SrcInstr = 4; tr.tsp.DstInstr = 5;
	const u32 oldAlpha = PT_ALPHA_REF;
	fixture.global_param_pt.push_back(tr); fixture.render_passes[0].pt_count = 1;
	PT_ALPHA_REF = 192; check("punch-through rejection", 255, 0, 0);
	PT_ALPHA_REF = 128; check("punch-through acceptance", 0, 0, 255);
	PT_ALPHA_REF = oldAlpha;
	fixture.global_param_pt.clear(); fixture.render_passes[0].pt_count = 0;
	fixture.global_param_tr.push_back(tr);
	fixture.render_passes[0].tr_count = 1;
	check("unsorted alpha", 127, 0, 128);
	fixture.render_passes[0].autosort = true;
	check("sorted alpha", 127, 0, 128);
	if (perPixel) {
		const int oldLayers = config::PerPixelLayers;
		for (int layers : {1, 17, 32, 33, 129, 256}) {
			config::PerPixelLayers = layers;
			check("OIT layer setting change", 127, 0, 128);
		}
		config::PerPixelLayers = oldLayers;
		// Snapshot outputs on the same GPU queue without a CPU wait, submit
		// more frames than resource slots, then inspect them. Holding a texture
		// reference alone does not freeze a pooled output's later contents.
		// The ordered copies catch counter/node reuse before GPU completion.
		std::vector<id<MTLTexture>> pending;
		for (int i = 0; i < 13; ++i) {
			config::PixelBufferSize = 1_MB + (i >= 6 ? 48 : 0);
			for (int j = 4; j < 8; ++j) {
				fixture.verts[j].col[1] = i & 1 ? 255 : 0;
				fixture.verts[j].col[2] = i & 1 ? 0 : 255;
			}
			if (!Render()) throw RendererException("Metal OIT queued render failed");
			context()->flushRenderCommands();
			id<MTLTexture> snapshot = context()->makeTexture(64, 48, MTLPixelFormatRGBA8Unorm);
			FCMetal4Commands * copy = [context()->queue() commandBuffer];
			FCMetal4ComputeEncoder * blit = [copy transferEncoder];
			if (!copy || !blit) throw RendererException("Metal queued validation snapshot allocation failed");
			[blit copyFromTexture:screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
				sourceSize:MTLSizeMake(64, 48, 1) toTexture:snapshot destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
			[blit endEncoding]; context()->submitCommandBuffer(copy);
			pending.push_back(snapshot);
		}
		for (size_t i = 0; i < pending.size(); ++i) {
			std::vector<u8> pixels; context()->readTexture(pending[i], pixels);
			const u8 *pixel = pixels.data() + (24 * 64 + 32) * 4;
			if (std::abs(int(pixel[0]) - 127) > 1
					|| std::abs(int(pixel[1]) - (i & 1 ? 128 : 0)) > 1
					|| std::abs(int(pixel[2]) - (i & 1 ? 0 : 128)) > 1)
				throw RendererException("Metal OIT resource recycling corrupted a queued frame");
		}
		config::PixelBufferSize = 1_MB;
		for (int j = 4; j < 8; ++j) { fixture.verts[j].col[1] = 0; fixture.verts[j].col[2] = 255; }
		NOTICE_LOG(RENDERER, "Metal validation PASS: queued OIT resource recycling and budget change");
	}
	// Force a second pass to exercise depth continuation and retained color.
	fixture.render_passes.push_back(fixture.render_passes[0]);
	check("multipass depth continuation", 127, 0, 128);
	fixture.render_passes.resize(1);
	fixture.global_param_tr.clear(); fixture.render_passes[0].tr_count = 0;
	fixture.global_param_op[0].pcw.Shadow = 1;
	fixture.modtrig = {{0,0,3, 640,0,3, 0,480,3}, {640,0,3, 640,480,3, 0,480,3}};
	ModifierVolumeParam mv; mv.init(); mv.count = 2; mv.isp.DepthMode = 1; mv.isp.VolumeLast = 1;
	fixture.global_param_mvo.push_back(mv); fixture.render_passes[0].mvo_count = 1;
	// Validate stencil geometry and compare the production shadow result.
	check("opaque modifier stencil", int(std::round(FPU_SHAD_SCALE.scale_factor / 256.f * 255)), 0, 0);
	if (perPixel) {
		fixture.global_param_op[0].tsp1 = opaque.tsp;
		fixture.global_param_op[0].tcw1.full = 0;
		check("opaque two-volume stencil selection", 0, 255, 0);
		fixture.global_param_op[0].tsp1.full = fixture.global_param_op[0].tcw1.full = ~0u;
		tr.pcw.Shadow = 1;
		fixture.global_param_tr.push_back(tr); fixture.render_passes[0].tr_count = 1;
		fixture.render_passes[0].autosort = false;
		const float shade = FPU_SHAD_SCALE.scale_factor / 256.f;
		check("unsorted translucent opaque stencil shadow", int(std::round(std::round(shade * 255) * 127 / 255)), 0, int(std::round(128 * shade)));
		fixture.global_param_tr[0].tsp1 = tr.tsp; fixture.global_param_tr[0].tcw1.full = 0;
		check("unsorted translucent two-volume stencil", 0, 255, 0);
		fixture.global_param_tr.clear(); fixture.render_passes[0].tr_count = 0;
		fixture.render_passes[0].autosort = true;
		fixture.global_param_mvo.clear(); fixture.render_passes[0].mvo_count = 0;
		fixture.global_param_op[0].pcw.Shadow = 0;
		tr.pcw.Shadow = 1; fixture.global_param_tr.push_back(tr); fixture.render_passes[0].tr_count = 1;
		fixture.global_param_mvo_tr.push_back(mv); fixture.render_passes[0].mvo_tr_count = 1;
		check("translucent production modifiers", 127, 0, int(std::round(128 * FPU_SHAD_SCALE.scale_factor / 256.f)));
		if (context()->device().rasterOrderGroupsSupported) {
			const auto savedTriangles = fixture.modtrig;
			const auto savedOpaqueVolumes = fixture.global_param_mvo;
			const auto savedTranslucentVolumes = fixture.global_param_mvo_tr;
			const auto savedPasses = fixture.render_passes;
			const bool savedNative = config::NativeDepthInterpolation;
			const u32 savedShade = FPU_SHAD_SCALE.full;
			FPU_SHAD_SCALE.scale_factor = 128;
			bool shadowSeen = false, unshadowedSeen = false;
			for (bool native : {false, true}) for (bool shared : {false, true}) for (bool volumeLast : {false, true}) {
				config::NativeDepthInterpolation = native;
				for (u32 sequence = 0; sequence < 64; ++sequence) {
					fixture.modtrig.clear(); fixture.global_param_mvo.clear(); fixture.global_param_mvo_tr.clear();
					fixture.render_passes = savedPasses;
					fixture.render_passes[0].mv_op_tr_shared = shared;
					fixture.render_passes[0].mvo_count = shared ? 3 : 0;
					fixture.render_passes[0].mvo_tr_count = shared ? 0 : 3;
					for (u32 i = 0; i < 3; ++i) {
						// Full, left and right coverage create dependent overlapping
						// modifier groups, with independently selected volume modes.
						const float left = i == 2 ? 320.f : 0.f, right = i == 1 ? 320.f : 640.f;
						const float z = i == 1 ? .5f : 3.f;
						ModifierVolumeParam volume; volume.init();
						volume.first = u32(fixture.modtrig.size()); volume.count = 2;
						volume.isp.DepthMode = (sequence >> (2 * i)) & 3;
						volume.isp.VolumeLast = volumeLast;
						fixture.modtrig.push_back({left,0,z, right,0,z, left,480,z});
						fixture.modtrig.push_back({right,0,z, right,480,z, left,480,z});
						(shared ? fixture.global_param_mvo : fixture.global_param_mvo_tr).push_back(volume);
					}
					disableOrderedModifiers = true;
					if (!Render()) throw RendererException("Metal modifier ordering reference failed");
					std::vector<u8> reference; context()->readTexture(screen, reference);
					disableOrderedModifiers = false;
					if (!Render()) throw RendererException("Metal modifier ordering candidate failed");
					std::vector<u8> candidate; context()->readTexture(screen, candidate);
					if (reference != candidate) throw RendererException("Metal ordered modifier production pixels differ");
					for (size_t pixel = 0; pixel < reference.size(); pixel += 4) {
						shadowSeen |= reference[pixel + 2] >= 60 && reference[pixel + 2] <= 70;
						unshadowedSeen |= reference[pixel + 2] >= 120;
					}
				}
				NOTICE_LOG(RENDERER, "Metal validation PASS: 64 ordered modifier production image pairs native=%d shared=%d last=%d", native, shared, volumeLast);
			}
			if (!shadowSeen || !unshadowedSeen) throw RendererException("Metal modifier fixtures did not exercise both shadow states");
			config::NativeDepthInterpolation = savedNative;
			FPU_SHAD_SCALE.full = savedShade;
			fixture.modtrig = savedTriangles; fixture.global_param_mvo = savedOpaqueVolumes;
			fixture.global_param_mvo_tr = savedTranslucentVolumes; fixture.render_passes = savedPasses;
		}
		fixture.global_param_mvo_tr.clear(); fixture.render_passes[0].mvo_tr_count = 0;
		fixture.global_param_tr.clear(); fixture.render_passes[0].tr_count = 0;
	}
	if (!perPixel) {
		// No opaque geometry in pass two: its modifier must still see pass one's shadow eligibility.
		fixture.render_passes[0].mvo_count = 0;
		fixture.render_passes.push_back(fixture.render_passes[0]); fixture.render_passes[1].mvo_count = 1;
		check("multipass retained shadow eligibility", int(std::round(FPU_SHAD_SCALE.scale_factor / 256.f * 255)), 0, 0);
		fixture.render_passes.resize(1); fixture.render_passes[0].mvo_count = 1;
		check("pending shadow eligible image", int(std::round(FPU_SHAD_SCALE.scale_factor / 256.f * 255)), 0, 0);
		fixture.clearFramebuffer = false; fixture.global_param_op.clear(); fixture.render_passes[0].op_count = 0;
		const int once = int(std::round(FPU_SHAD_SCALE.scale_factor / 256.f * 255));
		check("separate render retains pending stencil", int(std::round(once * FPU_SHAD_SCALE.scale_factor / 256.f)), 0, 0);
		fixture.global_param_op.push_back(opaque); fixture.clearFramebuffer = true; fixture.render_passes[0].op_count = 1;
	}
	fixture.global_param_mvo.clear(); fixture.modtrig.clear(); fixture.render_passes[0].mvo_count = 0;
	fixture.global_param_op[0].pcw.Shadow = 0;
	// Feed actual Naomi-2 matrices/light-model arrays to both production paths.
	const auto savedVertices = fixture.verts;
	const auto savedOpaque = fixture.global_param_op[0];
	const auto savedPlatform = settings.platform;
	settings.platform.system = DC_PLATFORM_NAOMI2;
	fixture.matrices.resize(2);
	glm::mat4 identity(1), projection(1);
	projection[2][3] = -1; projection[3][3] = 0;
	memcpy(fixture.matrices[0].mat, glm::value_ptr(identity), sizeof(N2Matrix));
	memcpy(fixture.matrices[1].mat, glm::value_ptr(projection), sizeof(N2Matrix));
	for (auto& v : fixture.verts) { v.z = -1; v.nz = 1; }
	auto& n2 = fixture.global_param_op[0];
	n2.mvMatrix = n2.normalMatrix = 0; n2.projMatrix = 1; n2.lightModel = 0;
	fixture.lightModels.resize(2);
	fixture.lightModels[0].ambientBase[0][1] = .5f;
	fixture.lightModels[1].ambientBase[0][0] = .25f;
	check("Naomi2 production ambient model", 0, 128, 0);
	n2.lightModel = 1;
	check("Naomi2 second light-model buffer offset", 64, 0, 0);
	n2.lightModel = -1;
	check("Naomi2 absent light model", 0, 0, 0);
	n2.constantColor[0] = true;
	check("Naomi2 production constant material", 255, 0, 0);
	glm::mat4 shifted(1); shifted[3][0] = 1000;
	memcpy(fixture.matrices[0].mat, glm::value_ptr(shifted), sizeof(N2Matrix));
	check("Naomi2 production model-view transform", 0, 0, 0);
	memcpy(fixture.matrices[0].mat, glm::value_ptr(identity), sizeof(N2Matrix));
	PolyParam n2tr = tr; n2tr.mvMatrix = n2tr.normalMatrix = 0; n2tr.projMatrix = 1; n2tr.lightModel = 1;
	fixture.lightModels[1] = {}; fixture.lightModels[1].ambientBase[0][2] = .5f;
	fixture.global_param_tr.push_back(n2tr); fixture.render_passes[0].tr_count = 1;
	check("Naomi2 production sorted translucency", 127, 0, 64);
	fixture.global_param_tr.clear(); fixture.render_passes[0].tr_count = 0;
	n2.pcw.Shadow = 1;
	fixture.modtrig = {{0,0,-.5f, 640,0,-.5f, 0,480,-.5f}, {640,0,-.5f, 640,480,-.5f, 0,480,-.5f}};
	ModifierVolumeParam n2mv = mv; n2mv.mvMatrix = 0; n2mv.projMatrix = 1;
	fixture.global_param_mvo.push_back(n2mv); fixture.render_passes[0].mvo_count = 1;
	check("Naomi2 production modifier transform", int(std::round(FPU_SHAD_SCALE.scale_factor / 256.f * 255)), 0, 0);
	if (perPixel) {
		n2.constantColor[1] = false; n2.lightModel = 0;
		fixture.lightModels[0].ambientBase[1][1] = .25f;
		n2.tsp1 = opaque.tsp; n2.tcw1.full = 0;
		check("Naomi2 production second-volume material", 0, 64, 0);
	}
	fixture.verts = savedVertices; fixture.global_param_op[0] = savedOpaque;
	fixture.matrices.clear(); fixture.lightModels.clear(); settings.platform = savedPlatform;
	fixture.global_param_mvo.clear(); fixture.modtrig.clear(); fixture.render_passes[0].mvo_count = 0;
	check("Dreamcast render after Naomi2 fixtures", 255, 0, 0);
	context()->setLastFrame(screen, 4.f / 3.f); clearLastFrame = false;
	std::vector<u8> screenshot; int width = 32, height = 24;
	if (!GetLastFrame(screenshot, width, height) || screenshot.size() != size_t(width * height * 3))
		throw RendererException("Metal screenshot validation failed");
	NOTICE_LOG(RENDERER, "Metal validation PASS: screenshot (%s)", perPixel ? "OIT" : "standard");
	if (!Present() || Present()) throw RendererException("Metal queued an already consumed screen image");
	context()->present();
	if (!RenderLastFrame()) throw RendererException("Metal consumed image was lost to the GUI");
	NOTICE_LOG(RENDERER, "Metal validation PASS: screen image presented once and retained for GUI (%s)", perPixel ? "OIT" : "standard");
	// Both supplied and generated mip chains must use real texture-cache uploads.
	MetalTexture mip;
	mip.tex_type = TextureType::_8888;
	std::vector<u8> levels((1 + 4 + 16 + 64) * 4, 255);
	mip.UploadToGPU(8, 8, levels.data(), true, true);
	if (mip.image.mipmapLevelCount != 4) throw RendererException("Metal supplied mip chain failed");
	mip.UploadToGPU(8, 8, levels.data(), true, false);
	context()->waitIdle();
	NOTICE_LOG(RENDERER, "Metal validation PASS: texture mip chains (%s)", perPixel ? "OIT" : "standard");
	fixture.globClip = {64, 48}; fixture.tileClip = fixture.fbClip = Rect({0, 0}, {64, 48});
	for (auto& v : fixture.verts) { v.x /= 10; v.y /= 10; }
	fixture.fb_W_SOF1 = 0x1000; fixture.fb_W_LINESTRIDE = 16; fixture.fb_W_CTRL.fb_packmode = 1;
	const auto oldRTTBuffer = config::RenderToTextureBuffer.get();
	std::vector<u8> savedVRAM(64 * 64 * 2);
	memcpy(savedVRAM.data(), &vram[fixture.fb_W_SOF1], savedVRAM.size());
	fixture.isRTT = true; config::RenderToTextureBuffer = true;
	Render();
	if (Present()) throw RendererException("Metal presented RTT-only work");
	u16 packed = 0; memcpy(&packed, &vram[fixture.fb_W_SOF1 + (24 * 64 + 32) * 2], 2);
	if (packed != 0xf800) throw RendererException("Metal RTT VRAM packing failed");
	NOTICE_LOG(RENDERER, "Metal validation PASS: RTT VRAM write (%s)", perPixel ? "OIT" : "standard");
	config::RenderToTextureBuffer = false;
	fixture.framebufferHeight = 64;
	Render();
	if (textureCache.getRTTexture(fixture.fb_W_SOF1, 1, 64, 64)->image != target)
		throw RendererException("Metal RTT cache reuse failed");
	NOTICE_LOG(RENDERER, "Metal validation PASS: RTT GPU cache (%s)", perPixel ? "OIT" : "standard");
	fixture.isRTT = false; fixture.framebufferHeight = 48; Render();
	auto pendingScreen = screen;
	fixture.isRTT = true; fixture.framebufferHeight = 64; Render();
	if (!perPixel) {
		// An RTT closes Vulkan's pending screen depth pass. New screen work
		// must clear depth even when it loads the retained screen color.
		fixture.isRTT = false; fixture.framebufferHeight = 48; fixture.clearFramebuffer = false;
		for (int i = 0; i < 4; ++i) fixture.verts[i].z = .5f;
		check("RTT boundary starts fresh screen depth", 255, 0, 0);
		pendingScreen = screen; fixture.clearFramebuffer = true;
		for (int i = 0; i < 4; ++i) fixture.verts[i].z = 1;
	}
	if (!Present() || context()->lastFrame() != pendingScreen || Present())
		throw RendererException("Metal RTT changed or duplicated the pending screen image");
	context()->present();
	NOTICE_LOG(RENDERER, "Metal validation PASS: RTT preserves pending display image without duplicate swaps (%s)", perPixel ? "OIT" : "standard");
	fixture.isRTT = false; fixture.framebufferHeight = 48; config::EmulateFramebuffer = true;
	Render();
	packed = pvr_read32p<u16>(fixture.fb_W_SOF1 + (24 * 64 + 32) * 2);
	if (packed != 0xf800) throw RendererException("Metal framebuffer VRAM packing failed");
	FramebufferInfo info{}; info.fb_r_ctrl.fb_enable = 1; info.fb_r_ctrl.fb_depth = 1;
	info.fb_r_sof1 = fixture.fb_W_SOF1; info.fb_r_size.fb_x_size = 31; info.fb_r_size.fb_y_size = 47; info.fb_r_size.fb_modulus = 1;
	RenderFramebuffer(info);
	std::vector<u8> readback; context()->readTexture(framebuffer.image, readback);
	if (readback[(24 * 64 + 32) * 4] != 248)
		throw RendererException("Metal CPU framebuffer read failed");
	NOTICE_LOG(RENDERER, "Metal validation PASS: full framebuffer round trip (%s)", perPixel ? "OIT" : "standard");
	if (!Present() || Present()) throw RendererException("Metal framebuffer image was lost or presented twice");
	context()->present();
	NOTICE_LOG(RENDERER, "Metal validation PASS: framebuffer image presented once (%s)", perPixel ? "OIT" : "standard");
	info.vo_control.blank_video = 1; info.vo_border_col.full = 0x123456;
	RenderFramebuffer(info); context()->readTexture(framebuffer.image, readback);
	if (readback[0] != 0x12 || readback[1] != 0x34 || readback[2] != 0x56)
		throw RendererException("Metal blank framebuffer border failed");
	NOTICE_LOG(RENDERER, "Metal validation PASS: blank framebuffer border (%s)", perPixel ? "OIT" : "standard");
	memcpy(&vram[fixture.fb_W_SOF1], savedVRAM.data(), savedVRAM.size());
	config::RenderToTextureBuffer = oldRTTBuffer;
	config::EmulateFramebuffer = oldEmulate; config::Widescreen = oldWidescreen;
	config::PerStripSorting = oldSorting; config::PixelBufferSize = oldBudget;
	rc = nullptr;
}
