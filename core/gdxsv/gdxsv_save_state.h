#pragma once

#include <unordered_map>

#include "hw/mem/mem_watch.h"

class GdxsvSaveState {
   public:
	bool Enabled() { return enabled; }
	void StartUsing();
	void EndUsing();
	bool SaveState(int frame);
	bool LoadState(int frame);
	void Clear();
	void Reset();

   private:
	struct MemPages {
		void load() {
			memwatch::ramWatcher.getPages(ram);
			memwatch::vramWatcher.getPages(vram);
			memwatch::aramWatcher.getPages(aram);
			memwatch::elanWatcher.getPages(elanram);
		}
		memwatch::PageMap ram;
		memwatch::PageMap vram;
		memwatch::PageMap aram;
		memwatch::PageMap elanram;
	};

	bool enabled = false;
	std::unordered_map<int, MemPages> deltaStates;
	std::unordered_map<int, std::pair<int, unsigned char *>> buffers;
	int lastSavedFrame = -1;
};

extern GdxsvSaveState gdxsv_save_state;
