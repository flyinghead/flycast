/*
	 Copyright 2018 flyinghead
 
	 This file is part of reicast.
 
	 reicast is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 2 of the License, or
	 (at your option) any later version.
 
	 reicast is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.
 
	 You should have received a copy of the GNU General Public License
	 along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CORE_REND_GLES_CUSTOMTEXTURE_H_
#define CORE_REND_GLES_CUSTOMTEXTURE_H_

#include <string>
#include <set>
#include "gles.h"

class CustomTexture {
public:
	CustomTexture() : loader_thread(loader_thread_func, this), wakeup_thread(false, true) {}
	~CustomTexture() { Terminate(); }
	u8* LoadCustomTexture(u32 hash, int& width, int& height);
	void LoadCustomTextureAsync(TextureCacheData *texture_data);

private:
	bool Init();
	void Terminate();
	void LoaderThread();
	
	static void *loader_thread_func(void *param) { ((CustomTexture *)param)->LoaderThread(); return NULL; }
	
	bool initialized;
	bool custom_textures_available;
	std::string textures_path;
	std::set<u32> unknown_hashes;
	cThread loader_thread;
	cResetEvent wakeup_thread;
	std::vector<struct TextureCacheData *> work_queue;
	cMutex work_queue_mutex;
};

#endif /* CORE_REND_GLES_CUSTOMTEXTURE_H_ */
