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
#include "CustomTexture.h"

#include <algorithm>
#include <sstream>
#include <sys/types.h>
#include <dirent.h>

#include "deps/libpng/png.h"
#include "reios.h"

void CustomTexture::LoaderThread()
{
	while (initialized)
	{
		TextureCacheData *texture = NULL;
		
		work_queue_mutex.Lock();
		if (!work_queue.empty())
		{
			texture = work_queue.back();
			work_queue.pop_back();
		}
		work_queue_mutex.Unlock();
		
		if (texture != NULL)
		{
			// FIXME texture may have been deleted. Need to detect this.
			texture->ComputeHash();
			int width, height;
			u8 *image_data = LoadCustomTexture(texture->texture_hash, width, height);
			if (image_data != NULL)
			{
				if (texture->custom_image_data != NULL)
					delete [] texture->custom_image_data;
				texture->custom_width = width;
				texture->custom_height = height;
				texture->custom_image_data = image_data;
			}
		}
		
		wakeup_thread.Wait();
	}
}

bool CustomTexture::Init()
{
	if (!initialized)
	{
		initialized = true;
		std::string game_id_str = reios_product_number;
		if (game_id_str.length() > 0)
		{
			std::replace(game_id_str.begin(), game_id_str.end(), ' ', '_');
			textures_path = get_readonly_data_path("/data/") + "textures/" + game_id_str + "/";

			DIR *dir = opendir(textures_path.c_str());
			if (dir != NULL)
			{
				printf("Found custom textures directory: %s\n", textures_path.c_str());
				custom_textures_available = true;
				closedir(dir);
				loader_thread.Start();
			}
		}
	}
	return custom_textures_available;
}

void CustomTexture::Terminate()
{
	if (initialized)
	{
		initialized = false;
		work_queue_mutex.Lock();
		work_queue.clear();
		work_queue_mutex.Unlock();
		wakeup_thread.Set();
		loader_thread.WaitToEnd();
	}
}

u8* CustomTexture::LoadCustomTexture(u32 hash, int& width, int& height)
{
	if (unknown_hashes.find(hash) != unknown_hashes.end())
		return NULL;
	std::stringstream path;
	path << textures_path << std::hex << hash << ".png";

	u8 *image_data = loadPNGData(path.str(), width, height, false);
	if (image_data == NULL)
		unknown_hashes.insert(hash);

	return image_data;
}

void CustomTexture::LoadCustomTextureAsync(TextureCacheData *texture_data)
{
	if (!Init())
		return;
	work_queue_mutex.Lock();
	work_queue.insert(work_queue.begin(), texture_data);
	work_queue_mutex.Unlock();
	wakeup_thread.Set();
}

