#include "gdxsv_save_state.h"

#include "serialize.h"

void GdxsvSaveState::StartUsing() { enabled = true; }

void GdxsvSaveState::EndUsing() { enabled = false; }

bool GdxsvSaveState::SaveState(int frame) {
	lastSavedFrame = frame;
	// TODO this is way too much memory
	size_t allocSize = (settings.platform.isNaomi() ? 20 : 10) * 1024 * 1024;
	auto buffer = (unsigned char*)malloc(allocSize);
	if (buffer == nullptr) {
		ERROR_LOG(NETWORK, "Memory alloc failed");
		return false;
	}
	Serializer ser(buffer, allocSize, true);
	ser << frame;
	dc_serialize(ser);
	verify(ser.size() < allocSize);
	memwatch::protect();
	if (frame > 0) {
		// Save the delta to frame-1
		deltaStates[frame - 1].load();
		NOTICE_LOG(NETWORK, "Saved frame %d pages: %d ram, %d vram, %d eram, %d aica ram", frame - 1,
				   (u32)deltaStates[frame - 1].ram.size(), (u32)deltaStates[frame - 1].vram.size(),
				   (u32)deltaStates[frame - 1].elanram.size(), (u32)deltaStates[frame - 1].aram.size());
	}

	if (buffers[frame].second != nullptr) {
		free(buffers[frame].second);
	}
	buffers[frame] = std::make_pair(ser.size(), buffer);
	return true;
}

bool GdxsvSaveState::LoadState(int frame) {
	if (buffers.find(frame) == buffers.end()) {
		ERROR_LOG(COMMON, "GdxsvSaveState.LoadState: no frame info");
		return false;
	}
	auto [len, buffer] = buffers[frame];
	if (buffer == nullptr) {
		ERROR_LOG(COMMON, "GdxsvSaveState.LoadState: buffer is nullptr");
		return false;
	}

	rend_start_rollback();
	// FIXME dynarecs
	Deserializer deser(buffer, len, true);
	int frame_;
	deser >> frame_;
	verify(frame == frame_);
	memwatch::unprotect();
	for (int f = lastSavedFrame - 1; f >= frame; f--) {
		if (deltaStates.find(f) == deltaStates.end()) continue;
		const MemPages& pages = deltaStates[f];
		for (const auto& pair : pages.ram) memcpy(memwatch::ramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
		for (const auto& pair : pages.vram) memcpy(memwatch::vramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
		for (const auto& pair : pages.aram) memcpy(memwatch::aramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
		for (const auto& pair : pages.elanram) memcpy(memwatch::elanWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
		deltaStates.erase(f);
		NOTICE_LOG(NETWORK, "Restored frame %d pages: %d ram, %d vram, %d eram, %d aica ram", f, (u32)pages.ram.size(),
				   (u32)pages.vram.size(), (u32)pages.elanram.size(), (u32)pages.aram.size());

		if (auto it = buffers.find(f + 1); it != buffers.end()) {
			if (it->second.second != nullptr) {
				free(it->second.second);
			}
			buffers.erase(it);
		}
	}

	dc_deserialize(deser);
	if (deser.size() != (u32)len) {
		ERROR_LOG(NETWORK, "load_game_state len %d used %d", len, (int)deser.size());
		die("fatal");
	}
	rend_allow_rollback();	// ggpo might load another state right after this one
	memwatch::reset();
	memwatch::protect();
	return true;
}

void GdxsvSaveState::Clear() {
	const bool has_save_state = !buffers.empty();
	for (const auto& buffer : buffers) {
		free(buffer.second.second);
	}
	buffers.clear();
	deltaStates.clear();
	lastSavedFrame = -1;
	if (has_save_state) {
		memwatch::unprotect();
	}
	memwatch::reset();
}

void GdxsvSaveState::Reset() {
	Clear();
	enabled = false;
}

GdxsvSaveState gdxsv_save_state;
