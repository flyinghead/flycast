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
#include <sys/stat.h>
#ifdef _MSC_VER
#include "dirent/dirent.h"
#else
#include <dirent.h>
#endif

#include "deps/libpng/png.h"
#include "reios/reios.h"

void CustomTexture::LoaderThread()
{
	while (initialized)
	{
		TextureCacheData *texture;
		
		do {
			texture = NULL;

			work_queue_mutex.Lock();
			if (!work_queue.empty())
			{
				texture = work_queue.back();
				work_queue.pop_back();
			}
			work_queue_mutex.Unlock();
			
			if (texture != NULL)
			{
				texture->ComputeHash();
				if (texture->custom_image_data != NULL)
				{
					delete [] texture->custom_image_data;
					texture->custom_image_data = NULL;
				}
				if (!texture->dirty)
				{
					int width, height;
					u8 *image_data = LoadCustomTexture(texture->texture_hash, width, height);
					if (image_data == NULL)
					{
						image_data = LoadCustomTexture(texture->old_texture_hash, width, height);
					}
					if (image_data != NULL)
					{
						texture->custom_width = width;
						texture->custom_height = height;
						texture->custom_image_data = image_data;
					}
				}
				texture->custom_load_in_progress--;
			}

		} while (texture != NULL);
		
		wakeup_thread.Wait();
	}
}

std::string CustomTexture::GetGameId()
{
   std::string game_id = reios_product_number;
   const size_t str_end = game_id.find_last_not_of(" ");
   if (str_end == std::string::npos)
	  return "";
   game_id = game_id.substr(0, str_end + 1);
   std::replace(game_id.begin(), game_id.end(), ' ', '_');

   return game_id;
}

bool CustomTexture::Init()
{
	if (!initialized)
	{
		initialized = true;
#ifndef TARGET_NO_THREADS
		std::string game_id = GetGameId();
		if (game_id.length() > 0)
		{
			textures_path = get_readonly_data_path("/data/") + "textures/" + game_id + "/";

			DIR *dir = opendir(textures_path.c_str());
			if (dir != NULL)
			{
				printf("Found custom textures directory: %s\n", textures_path.c_str());
				custom_textures_available = true;
				closedir(dir);
				loader_thread.Start();
			}
		}
#endif
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
#ifndef TARGET_NO_THREADS
		loader_thread.WaitToEnd();
#endif
	}
}

u8* CustomTexture::LoadCustomTexture(u32 hash, int& width, int& height)
{
	if (unknown_hashes.find(hash) != unknown_hashes.end())
		return NULL;
	std::stringstream path;
	path << textures_path << std::hex << hash << ".png";

	u8 *image_data = loadPNGData(path.str(), width, height);
	if (image_data == NULL)
		unknown_hashes.insert(hash);

	return image_data;
}

void CustomTexture::LoadCustomTextureAsync(TextureCacheData *texture_data)
{
	if (!Init())
		return;

	texture_data->custom_load_in_progress++;
	work_queue_mutex.Lock();
	work_queue.insert(work_queue.begin(), texture_data);
	work_queue_mutex.Unlock();
	wakeup_thread.Set();
}

void CustomTexture::DumpTexture(u32 hash, int w, int h, GLuint textype, void *temp_tex_buffer)
{
	std::string base_dump_dir = get_writable_data_path("/data/texdump/");
	if (!file_exists(base_dump_dir))
		make_directory(base_dump_dir);
	std::string game_id = GetGameId();
	if (game_id.length() == 0)
	   return;

	base_dump_dir += game_id + "/";
	if (!file_exists(base_dump_dir))
		make_directory(base_dump_dir);

	std::stringstream path;
	path << base_dump_dir << std::hex << hash << ".png";
	FILE *fp = fopen(path.str().c_str(), "wb");
	if (fp == NULL)
	{
		printf("Failed to open %s for writing\n", path.str().c_str());
		return;
	}

	u16 *src = (u16 *)temp_tex_buffer;

	png_bytepp rows = (png_bytepp)malloc(h * sizeof(png_bytep));
	for (int y = 0; y < h; y++)
	{
		rows[h - y - 1] = (png_bytep)malloc(w * 4);	// 32-bit per pixel
		u8 *dst = (u8 *)rows[h - y - 1];
		switch (textype)
		{
		case GL_UNSIGNED_SHORT_4_4_4_4:
			for (int x = 0; x < w; x++)
			{
				*dst++ = ((*src >> 12) & 0xF) << 4;
				*dst++ = ((*src >> 8) & 0xF) << 4;
				*dst++ = ((*src >> 4) & 0xF) << 4;
				*dst++ = (*src & 0xF) << 4;
				src++;
			}
			break;
		case GL_UNSIGNED_SHORT_5_6_5:
			for (int x = 0; x < w; x++)
			{
				*dst++ = ((*src >> 11) & 0x1F) << 3;
				*dst++ = ((*src >> 5) & 0x3F) << 2;
				*dst++ = (*src & 0x1F) << 3;
				*dst++ = 255;
				src++;
			}
			break;
		case GL_UNSIGNED_SHORT_5_5_5_1:
			for (int x = 0; x < w; x++)
			{
				*dst++ = ((*src >> 11) & 0x1F) << 3;
				*dst++ = ((*src >> 6) & 0x1F) << 3;
				*dst++ = ((*src >> 1) & 0x1F) << 3;
				*dst++ = (*src & 1) ? 255 : 0;
				src++;
			}
			break;
		case GL_UNSIGNED_BYTE:
			for (int x = 0; x < w; x++)
			{
				*(u32 *)dst = *(u32 *)src;
				dst += 4;
				src += 2;
			}
			break;
		default:
			printf("dumpTexture: unsupported picture format %x\n", textype);
			fclose(fp);
			free(rows[0]);
			free(rows);
			return;
		}
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);

	png_init_io(png_ptr, fp);


	// write header
	png_set_IHDR(png_ptr, info_ptr, w, h,
			 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
			 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);


	// write bytes
	png_write_image(png_ptr, rows);

	// end write
	png_write_end(png_ptr, NULL);
	fclose(fp);

	for (int y = 0; y < h; y++)
		free(rows[y]);
	free(rows);
}
