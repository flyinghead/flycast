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
#include "cfg/cfg.h"
#include "oslib/directory.h"
#include "cfg/option.h"
#include "oslib/oslib.h"

#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

CustomTexture custom_texture;

void CustomTexture::LoaderThread()
{
	LoadMap();
	while (initialized)
	{
		BaseTextureCacheData *texture;
		
		do {
			texture = nullptr;
			{
				std::unique_lock<std::mutex> lock(work_queue_mutex);
				if (!work_queue.empty())
				{
					texture = work_queue.back();
					work_queue.pop_back();
				}
			}
			
			if (texture != nullptr)
			{
				texture->ComputeHash();
				if (texture->custom_image_data != nullptr)
				{
					free(texture->custom_image_data);
					texture->custom_image_data = nullptr;
				}
				if (!texture->dirty)
				{
					int width, height;
					u8 *image_data = LoadCustomTexture(texture->texture_hash, width, height);
					if (image_data == nullptr)
					{
						image_data = LoadCustomTexture(texture->old_texture_hash, width, height);
					}
					if (image_data != nullptr)
					{
						texture->custom_width = width;
						texture->custom_height = height;
						texture->custom_image_data = image_data;
					}
				}
				texture->custom_load_in_progress--;
			}

		} while (texture != nullptr);
		
		wakeup_thread.Wait();
	}
}

std::string CustomTexture::GetGameId()
{
   std::string game_id(settings.content.gameId);
   const size_t str_end = game_id.find_last_not_of(' ');
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
		std::string game_id = GetGameId();
		if (game_id.length() > 0)
		{
			textures_path = hostfs::getTextureLoadPath(game_id);

			if (!textures_path.empty())
			{
				DIR *dir = flycast::opendir(textures_path.c_str());
				if (dir != nullptr)
				{
					NOTICE_LOG(RENDERER, "Found custom textures directory: %s", textures_path.c_str());
					custom_textures_available = true;
					flycast::closedir(dir);
					loader_thread.Start();
				}
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
		{
			std::unique_lock<std::mutex> lock(work_queue_mutex);
			work_queue.clear();
		}
		wakeup_thread.Set();
		loader_thread.WaitToEnd();
		texture_map.clear();
	}
}

u8* CustomTexture::LoadCustomTexture(u32 hash, int& width, int& height)
{
	auto it = texture_map.find(hash);
	if (it == texture_map.end())
		return nullptr;

	FILE *file = nowide::fopen(it->second.c_str(), "rb");
	if (file == nullptr)
		return nullptr;
	int n;
	stbi_set_flip_vertically_on_load(1);
	u8 *imgData = stbi_load_from_file(file, &width, &height, &n, STBI_rgb_alpha);
	std::fclose(file);
	return imgData;
}

void CustomTexture::LoadCustomTextureAsync(BaseTextureCacheData *texture_data)
{
	if (!Init())
		return;

	texture_data->custom_load_in_progress++;
	{
		std::unique_lock<std::mutex> lock(work_queue_mutex);
		work_queue.insert(work_queue.begin(), texture_data);
	}
	wakeup_thread.Set();
}

void CustomTexture::DumpTexture(u32 hash, int w, int h, TextureType textype, void *src_buffer)
{
	std::string base_dump_dir = hostfs::getTextureDumpPath();
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

	u16 *src = (u16 *)src_buffer;
	u8 *dst_buffer = (u8 *)malloc(w * h * 4);	// 32-bit per pixel
	u8 *dst = dst_buffer;

	for (int y = 0; y < h; y++)
	{
		if (!config::RendererType.isDirectX())
		{
			switch (textype)
			{
			case TextureType::_4444:
				for (int x = 0; x < w; x++)
				{
					*dst++ = (((*src >> 12) & 0xF) << 4) | ((*src >> 12) & 0xF);
					*dst++ = (((*src >> 8) & 0xF) << 4) | ((*src >> 8) & 0xF);
					*dst++ = (((*src >> 4) & 0xF) << 4) | ((*src >> 4) & 0xF);
					*dst++ = ((*src & 0xF) << 4) | (*src & 0xF);
					src++;
				}
				break;
			case TextureType::_565:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker565_32<RGBAPacker>::unpack(*src);
					dst += 4;
					src++;
				}
				break;
			case TextureType::_5551:
				for (int x = 0; x < w; x++)
				{
					*dst++ = (((*src >> 11) & 0x1F) << 3) | ((*src >> 13) & 7);
					*dst++ = (((*src >> 6) & 0x1F) << 3) | ((*src >> 8) & 7);
					*dst++ = (((*src >> 1) & 0x1F) << 3) | ((*src >> 3) & 7);
					*dst++ = (*src & 1) ? 255 : 0;
					src++;
				}
				break;
			case TextureType::_8888:
				memcpy(dst, src, w * 4);
				dst += w * 4;
				src += w * 2;
				break;
			default:
				WARN_LOG(RENDERER, "dumpTexture: unsupported picture format %x", (u32)textype);
				free(dst_buffer);
				return;
			}
		}
		else
		{
			switch (textype)
			{
			case TextureType::_4444:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker4444_32<RGBAPacker>::unpack(*src);
					dst += 4;
					src++;
				}
				break;
			case TextureType::_565:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker565_32<RGBAPacker>::unpack(*src);
					dst += 4;
					src++;
				}
				break;
			case TextureType::_5551:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker1555_32<RGBAPacker>::unpack(*src);
					dst += 4;
					src++;
				}
				break;
			case TextureType::_8888:
				for (int x = 0; x < w; x++)
				{
					*(u32 *)dst = Unpacker8888<RGBAPacker>::unpack(*(u32 *)src);
					dst += 4;
					src += 2;
				}
				break;
			default:
				WARN_LOG(RENDERER, "dumpTexture: unsupported picture format %x", (u32)textype);
				free(dst_buffer);
				return;
			}
		}
	}

	stbi_flip_vertically_on_write(1);
	stbi_write_png(path.str().c_str(), w, h, STBI_rgb_alpha, dst_buffer, 0);

	free(dst_buffer);
}

void CustomTexture::LoadMap()
{
	texture_map.clear();
	DirectoryTree tree(textures_path);
	for (const DirectoryTree::item& item : tree)
	{
		std::string extension = get_file_extension(item.name);
		if (extension != "jpg" && extension != "jpeg" && extension != "png")
			continue;
		std::string::size_type dotpos = item.name.find_last_of('.');
		std::string basename = item.name.substr(0, dotpos);
		char *endptr;
		u32 hash = (u32)strtoll(basename.c_str(), &endptr, 16);
		if (endptr - basename.c_str() < (ptrdiff_t)basename.length())
		{
			INFO_LOG(RENDERER, "Invalid hash %s", basename.c_str());
			continue;
		}
		texture_map[hash] = item.parentPath + "/" + item.name;
	}
	custom_textures_available = !texture_map.empty();
}
