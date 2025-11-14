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
#include "TexCache.h"
#include "oslib/directory.h"
#include "oslib/storage.h"
#include "cfg/option.h"
#include "oslib/oslib.h"
#include "stdclass.h"
#include "util/worker_thread.h"

#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

CustomTexture custom_texture;

class CustomTextureSource : public BaseCustomTextureSource
{
public:
	CustomTextureSource(const std::string& path) : textures_path(path) { }
	bool LoadMap() override;
	u8* LoadCustomTexture(u32 hash, int& width, int& height) override;
	bool IsTextureReplaced(u32 hash) override final;

private:
	std::string textures_path;
	std::map<u32, std::string> texture_map;
};

bool CustomTextureSource::LoadMap()
{
	texture_map.clear();
	hostfs::DirectoryTree tree(textures_path);
	for (const hostfs::FileInfo& item : tree)
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
		texture_map[hash] = item.path;
	}
	return !texture_map.empty();
}

u8* CustomTextureSource::LoadCustomTexture(u32 hash, int& width, int& height)
{
	auto it = texture_map.find(hash);
	if (it == texture_map.end())
		return nullptr;

	FILE *file = hostfs::storage().openFile(it->second, "rb");
	if (file == nullptr)
		return nullptr;
	int n;
	stbi_set_flip_vertically_on_load(1);
	u8 *imgData = stbi_load_from_file(file, &width, &height, &n, STBI_rgb_alpha);
	std::fclose(file);
	return imgData;
}

bool CustomTextureSource::IsTextureReplaced(u32 hash)
{
	return texture_map.count(hash);
}

void CustomTexture::AddSource(std::unique_ptr<BaseCustomTextureSource> source)
{
	sources.emplace_back(std::move(source));
}

void CustomTexture::loadTexture(BaseTextureCacheData *texture)
{
	if (texture->custom_image_data != nullptr) {
		free(texture->custom_image_data);
		texture->custom_image_data = nullptr;
	}
	if (!texture->dirty)
	{
		int width, height;
		u8 *image_data = loadTexture(texture->texture_hash, width, height);
		if (image_data == nullptr && texture->old_vqtexture_hash != 0)
			image_data = loadTexture(texture->old_vqtexture_hash, width, height);
		if (image_data == nullptr)
			image_data = loadTexture(texture->old_texture_hash, width, height);
		if (image_data != nullptr)
		{
			texture->custom_width = width;
			texture->custom_height = height;
			texture->custom_image_data = image_data;
		}
	}
	texture->custom_load_in_progress--;
}

std::string CustomTexture::getGameId()
{
   std::string game_id(settings.content.gameId);
   const size_t str_end = game_id.find_last_not_of(' ');
   if (str_end == std::string::npos)
	  return "";
   game_id = game_id.substr(0, str_end + 1);
   std::replace(game_id.begin(), game_id.end(), ' ', '_');

   return game_id;
}

bool CustomTexture::init()
{
	if (!initialized)
	{
		initialized = true;
		custom_textures_available = false;
		std::string game_id = getGameId();
		if (game_id.length() > 0)
		{
			textures_path = hostfs::getTextureLoadPath(game_id);

			if (!textures_path.empty())
			{
				try {
					hostfs::FileInfo fileInfo = hostfs::storage().getFileInfo(textures_path);
					if (fileInfo.isDirectory)
					{
						NOTICE_LOG(RENDERER, "Found custom textures directory: %s", textures_path.c_str());
						AddSource(std::make_unique<CustomTextureSource>(textures_path));
						for (auto& source : sources)
							custom_textures_available |= source->Init();
						
						if (custom_textures_available)
						{
							loaderThread = std::make_unique<WorkerThread>("CustomTexLoader");
							loaderThread->run([this]() {
								loadMap();
							});
						}
					}
				} catch (const FlycastException& e) {
				}
			}
		}
	}
	return custom_textures_available;
}

CustomTexture::~CustomTexture() {
	Terminate();
}

void CustomTexture::Terminate()
{
	if (loaderThread)
		loaderThread->stop();
	loaderThread.reset();
	for (auto& source : sources)
		source->Terminate();
	sources.clear();
	initialized = false;
}

u8* CustomTexture::loadTexture(u32 hash, int& width, int& height)
{
	for (auto& source : sources)
	{
		u8* data = source->LoadCustomTexture(hash, width, height);
		if (data != nullptr)
			return data;
	}
	return nullptr;
}

bool CustomTexture::isTextureReplaced(BaseTextureCacheData* texture)
{
	for (auto& source : sources)
	{
		if (source->IsTextureReplaced(texture->texture_hash) ||
			(texture->old_vqtexture_hash != 0 && source->IsTextureReplaced(texture->old_vqtexture_hash)) ||
			(texture->old_texture_hash != 0 && source->IsTextureReplaced(texture->old_texture_hash)))
			return true;
	}
	return false;
}

void CustomTexture::LoadCustomTextureAsync(BaseTextureCacheData *texture_data)
{
	if (!init())
		return;

	texture_data->custom_load_in_progress++;
	loaderThread->run([this, texture_data]() {
		loadTexture(texture_data);
	});
}

void CustomTexture::DumpTexture(BaseTextureCacheData* texture, int w, int h, void *src_buffer)
{
	if (!config::DumpReplacedTextures.get() && isTextureReplaced(texture))
		return;

	std::string base_dump_dir = hostfs::getTextureDumpPath();
	if (!file_exists(base_dump_dir))
		make_directory(base_dump_dir);
	std::string game_id = getGameId();
	if (game_id.length() == 0)
	   return;

	base_dump_dir += game_id + "/";
	if (!file_exists(base_dump_dir))
		make_directory(base_dump_dir);

	std::stringstream path;
	path << base_dump_dir << std::hex << texture->texture_hash << ".png";

	u16 *src = (u16 *)src_buffer;
	u8 *dst_buffer = (u8 *)malloc(w * h * 4);	// 32-bit per pixel
	if (dst_buffer == nullptr)
	{
		ERROR_LOG(RENDERER, "Dump texture: out of memory");
		return;
	}
	u8 *dst = dst_buffer;

	for (int y = 0; y < h; y++)
	{
		if (!isDirectX(config::RendererType))
		{
			switch (texture->tex_type)
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
				WARN_LOG(RENDERER, "dumpTexture: unsupported picture format %x", (u32)texture->tex_type);
				free(dst_buffer);
				return;
			}
		}
		else
		{
			switch (texture->tex_type)
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
				WARN_LOG(RENDERER, "dumpTexture: unsupported picture format %x", (u32)texture->tex_type);
				free(dst_buffer);
				return;
			}
		}
	}

	stbi_flip_vertically_on_write(1);
	const auto& savefunc = [](void *context, void *data, int size) {
		FILE *f = nowide::fopen((const char *)context, "wb");
		if (f == nullptr)
		{
			WARN_LOG(RENDERER, "Dump texture: can't save to file %s: error %d", (const char *)context, errno);
		}
		else
		{
			fwrite(data, 1, size, f);
			fclose(f);
		}
	};
	stbi_write_png_to_func(savefunc, (void *)path.str().c_str(), w, h, STBI_rgb_alpha, dst_buffer, 0);

	free(dst_buffer);
}

void CustomTexture::loadMap()
{
	bool available = false;
	for (auto& source : sources)
		available |= source->LoadMap();
	custom_textures_available = available;
}
