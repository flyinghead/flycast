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
static void stateLoaded(Event, void *) { loaded = true; }
struct Listener {
	Listener() { EventManager::listen(Event::LoadState, stateLoaded); }
	~Listener() { EventManager::unlisten(Event::LoadState, stateLoaded); }
};
static Listener listener;
static bool enabled() { return loaded && std::getenv("FLYCAST_METAL_CAPTURE_DIR") != nullptr; }
static void fingerprint(const rend_context& rc)
{
	if (!enabled() || rc.isRTT) return;
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
	static unsigned index = 0;
	const char *directory = std::getenv("FLYCAST_METAL_CAPTURE_DIR");
	std::vector<u8> rgb;
	int width = 0, height = 0;
	if (!renderer->GetLastFrame(rgb, width, height) || width <= 0 || height <= 0
			|| rgb.size() != size_t(width) * height * 3) throw RendererException("Metal capture readback failed");
	char name[64]; std::snprintf(name, sizeof(name), "/frame-%04u.png", index);
	const std::string path = std::string(directory) + name;
	stbi_flip_vertically_on_write(0);
	if (!stbi_write_png(path.c_str(), width, height, 3, rgb.data(), width * 3))
		throw RendererException("Metal capture PNG write failed");
	const std::string manifest = std::string(directory) + "/frames.jsonl";
	FILE *file = nowide::fopen(manifest.c_str(), "a");
	if (!file) throw RendererException("Metal capture manifest write failed");
	std::fprintf(file, "{\"index\":%u,\"geometry\":\"%016llx\",\"video_time\":%.9f,\"width\":%d,\"height\":%d,\"vertices\":%u,\"modifiers\":%u}\n",
		index, (unsigned long long)geometry, videoTime, width, height, vertices, modifiers);
	std::fclose(file);
	const char *limit = std::getenv("FLYCAST_METAL_CAPTURE_FRAMES");
	if (++index >= unsigned(limit ? std::max(1, std::atoi(limit)) : 400)) {
		std::fprintf(stderr, "Metal complete-frame capture finished: %u frames\n", index);
		std::fflush(nullptr);
		// Isolated diagnostic profile only; never save or alter user state.
		std::_Exit(0);
	}
}
} // namespace metal_capture
