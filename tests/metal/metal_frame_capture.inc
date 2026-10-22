// Diagnostic builds only: capture complete Metal output after loading a copied
// state, with a geometry fingerprint to align before/after frame sequences.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stb_image_write.h>
#include <atomic>
#include <cstdlib>
#include <cstdio>

namespace metal_capture {
static std::atomic<bool> loaded{false};
static u64 geometry = 0;
static unsigned vertices = 0, modifiers = 0;
static unsigned renderIndex = 0;
static void stateLoaded(Event, void *) { loaded = true; }
struct Listener {
	Listener() { EventManager::listen(Event::LoadState, stateLoaded); }
	~Listener() { EventManager::unlisten(Event::LoadState, stateLoaded); }
};
static Listener listener;
static bool enabled() { return loaded && std::getenv("FLYCAST_METAL_CAPTURE_DIR") != nullptr; }
static bool analyzeOIT()
{
	if (!enabled() || !std::getenv("FLYCAST_METAL_ANALYZE_OIT")) return false;
	switch (renderIndex) {
		case 0: case 100: case 700: case 1000: case 1200: case 1400:
		case 1600: case 1800: case 2200: case 2600: case 3000: return true;
		default: return false;
	}
}
API_AVAILABLE(macos(26.0))
static void analyzeOIT(MetalContext *context, FCMetal4Commands *commands, MetalFrameUniforms frame,
	id<MTLBuffer> nodes, id<MTLBuffer> heads, metal_oit::Tile tile,
	unsigned pass, unsigned layout, unsigned capacity)
{
	if (!analyzeOIT()) return;
	frame.oitTile = {tile.x,tile.y,tile.width,tile.height}; frame.capacity = capacity;
	id<MTLBuffer> lengths = [context->device() newBufferWithLength:NSUInteger(tile.width)*tile.height*sizeof(u32)
		options:MTLResourceStorageModeShared];
	if (!lengths) throw RendererException("Metal OIT analysis buffer allocation failed");
	FCMetal4ComputeEncoder *encoder = [commands computeCommandEncoder];
	[encoder setComputePipelineState:context->makeComputePipeline(@"oit_analysis")];
	[encoder setBytes:&frame length:sizeof(frame) atIndex:1];
	[encoder setBuffer:nodes offset:0 atIndex:4];
	[encoder setBuffer:heads offset:0 atIndex:5];
	[encoder setBuffer:lengths offset:0 atIndex:6];
	[encoder setBytes:&layout length:sizeof(layout) atIndex:7];
	[encoder dispatchThreads:MTLSizeMake(tile.width,tile.height,1) threadsPerThreadgroup:MTLSizeMake(32,4,1)];
	[encoder endEncoding];
	const unsigned index = renderIndex;
	const std::string directory = std::getenv("FLYCAST_METAL_CAPTURE_DIR");
	[commands addCompletedHandler:^(FCMetal4Commands *done) {
		if (done.status == FCMetal4StatusError) return;
		const u32 *data = static_cast<const u32 *>(lengths.contents);
		std::array<u64,4097> histogram{};
		const unsigned width = (tile.width+7)/8, height = (tile.height+7)/8;
		std::vector<u8> image(size_t(width)*height);
		u64 total = 0;
		unsigned maximum = 0;
		for (unsigned y=0;y<tile.height;++y) for (unsigned x=0;x<tile.width;++x) {
			const unsigned n = data[size_t(y)*tile.width+x];
			++histogram[std::min(4096u,n)]; total += n; maximum = std::max(maximum,n);
			u8& pixel = image[size_t(y/8)*width+x/8]; pixel = std::max(pixel,u8(std::min(255u,n)));
		}
		char suffix[160]; std::snprintf(suffix,sizeof(suffix),"/oit-%04u-pass%u-tile%u-%u",index,pass,tile.x,tile.y);
		const std::string base = directory+suffix;
		FILE *file = nowide::fopen((base+".json").c_str(),"w");
		if (file) {
			std::fprintf(file,"{\"index\":%u,\"pass\":%u,\"layout\":%u,\"tile\":[%u,%u,%u,%u],\"fragments\":%llu,\"max_layers\":%u,\"histogram\":{",
				index,pass,layout,tile.x,tile.y,tile.width,tile.height,(unsigned long long)total,maximum);
			bool first=true;
			for (unsigned i=0;i<histogram.size();++i) if (histogram[i]) {
				std::fprintf(file,"%s\"%u\":%llu",first?"":",",i,(unsigned long long)histogram[i]);first=false;
			}
			std::fprintf(file,"}}\n");std::fclose(file);
		}
		stbi_write_png((base+".png").c_str(),width,height,1,image.data(),width);
	}];
}
static void dumpGeometry(const rend_context& rc)
{
	const char *directory = std::getenv("FLYCAST_METAL_GEOMETRY_DIR");
	if (!directory) return;
	static unsigned index = 0;
	const unsigned current = index++;
	const char *strideText = std::getenv("FLYCAST_METAL_GEOMETRY_STRIDE");
	if (strideText ? current % unsigned(std::max(1, std::atoi(strideText))) != 0
		: current != 0 && current != 60 && current != 120) return;
	char name[64]; std::snprintf(name, sizeof(name), "/geometry-%04u.json", current);
	FILE *file = nowide::fopen((std::string(directory) + name).c_str(), "w");
	if (!file) throw RendererException("Metal geometry capture write failed");
	std::fprintf(file, "{\"passes\":[");
	for (size_t i = 0; i < rc.render_passes.size(); ++i) {
		const auto& p = rc.render_passes[i];
		std::fprintf(file, "%s{\"autosort\":%s,\"z_clear\":%s,\"op\":%u,\"pt\":%u,\"tr\":%u,\"mvo\":%u,\"mvt\":%u}",
			i ? "," : "", p.autosort ? "true" : "false", p.z_clear ? "true" : "false",
			p.op_count, p.pt_count, p.tr_count, p.mvo_count, p.mvo_tr_count);
	}
	std::fprintf(file, "],\"vertices\":[");
	for (size_t i = 0; i < rc.verts.size(); ++i) {
		const auto& v = rc.verts[i];
		std::fprintf(file, "%s{\"xyz\":[%.9g,%.9g,%.9g],\"uv\":[%.9g,%.9g],\"rgba\":[%u,%u,%u,%u]}",
			i ? "," : "", v.x, v.y, v.z, v.u, v.v, v.col[0], v.col[1], v.col[2], v.col[3]);
	}
	std::fprintf(file, "],\"indices\":[");
	for (size_t i = 0; i < rc.idx.size(); ++i) std::fprintf(file, "%s%u", i ? "," : "", rc.idx[i]);
	std::fprintf(file, "],\"lists\":[");
	unsigned list = 0;
	for (const auto *polygons : {&rc.global_param_op, &rc.global_param_pt, &rc.global_param_tr}) {
		std::fprintf(file, "%s[", list++ ? "," : "");
		for (size_t i = 0; i < polygons->size(); ++i) {
			const auto& p = (*polygons)[i];
			std::fprintf(file, "%s{\"first\":%u,\"count\":%u,\"isp\":%u,\"pcw\":%u,\"tsp\":%u,\"tcw\":%u,\"tileclip\":%u,\"texture_size\":[%u,%u]}",
				i ? "," : "", p.first, p.count, p.isp.full, p.pcw.full, p.tsp.full, p.tcw.full,
				p.tileclip, p.texture ? p.texture->width : 0, p.texture ? p.texture->height : 0);
		}
		std::fprintf(file, "]");
	}
	std::fprintf(file, "]}\n");
	std::fclose(file);
}
static void fingerprint(const rend_context& rc)
{
	if (!enabled() || rc.isRTT) return;
	static unsigned next = 0;
	renderIndex = next++;
	dumpGeometry(rc);
	u64 hash = 14695981039346656037ull;
	auto bytes = [&](const void *data, size_t length) {
		const u8 *p = static_cast<const u8 *>(data);
		for (size_t i = 0; i < length; ++i) { hash ^= p[i]; hash *= 1099511628211ull; }
	};
	for (const auto& v : rc.verts) bytes(&v.x, sizeof(float) * 3);
	bytes(rc.idx.data(), rc.idx.size() * sizeof(u32));
	bytes(rc.modtrig.data(), rc.modtrig.size() * sizeof(ModTriangle));
	for (const auto *list : {&rc.global_param_op, &rc.global_param_pt, &rc.global_param_tr})
		for (const auto& p : *list) {
			bytes(&p.first, sizeof(p.first)); bytes(&p.count, sizeof(p.count));
			bytes(&p.isp.full, sizeof(p.isp.full)); bytes(&p.pcw.full, sizeof(p.pcw.full));
			bytes(&p.tsp.full, sizeof(p.tsp.full)); bytes(&p.tcw.full, sizeof(p.tcw.full));
		}
	geometry = hash; vertices = rc.verts.size(); modifiers = rc.modtrig.size();
}
static void frame(Renderer *renderer, double videoTime)
{
	if (!enabled() || geometry == 0) return;
	static unsigned index = 0, captured = 0;
	const unsigned current = index++;
	const char *startText = std::getenv("FLYCAST_METAL_CAPTURE_START");
	const char *strideText = std::getenv("FLYCAST_METAL_CAPTURE_STRIDE");
	const unsigned start = startText ? std::max(0, std::atoi(startText)) : 0;
	const unsigned stride = strideText ? std::max(1, std::atoi(strideText)) : 1;
	if (current < start || (current - start) % stride != 0) return;
	const char *directory = std::getenv("FLYCAST_METAL_CAPTURE_DIR");
	std::vector<u8> rgb;
	int width = 0, height = 0;
	if (const char *requestedWidth = std::getenv("FLYCAST_METAL_CAPTURE_WIDTH"))
		width = std::max(0, std::atoi(requestedWidth));
	if (!renderer->GetLastFrame(rgb, width, height) || width <= 0 || height <= 0
			|| rgb.size() != size_t(width) * height * 3) throw RendererException("Metal capture readback failed");
	const bool jpeg = std::getenv("FLYCAST_METAL_CAPTURE_JPEG") != nullptr;
	char name[64]; std::snprintf(name, sizeof(name), "/frame-%04u.%s", current, jpeg ? "jpg" : "png");
	const std::string path = std::string(directory) + name;
	stbi_flip_vertically_on_write(0);
	const bool written = jpeg ? stbi_write_jpg(path.c_str(), width, height, 3, rgb.data(), 90)
		: stbi_write_png(path.c_str(), width, height, 3, rgb.data(), width * 3);
	if (!written) throw RendererException("Metal capture image write failed");
	const std::string manifest = std::string(directory) + "/frames.jsonl";
	FILE *file = nowide::fopen(manifest.c_str(), "a");
	if (!file) throw RendererException("Metal capture manifest write failed");
	std::fprintf(file, "{\"index\":%u,\"geometry\":\"%016llx\",\"video_time\":%.9f,\"width\":%d,\"height\":%d,\"vertices\":%u,\"modifiers\":%u}\n",
		current, (unsigned long long)geometry, videoTime, width, height, vertices, modifiers);
	std::fclose(file);
	const char *limit = std::getenv("FLYCAST_METAL_CAPTURE_FRAMES");
	if (++captured >= unsigned(limit ? std::max(1, std::atoi(limit)) : 400)) {
		std::fprintf(stderr, "Metal complete-frame capture finished: %u frames\n", captured);
		std::fflush(nullptr);
		// Isolated diagnostic profile only; never save or alter user state.
		std::_Exit(0);
	}
}
} // namespace metal_capture
