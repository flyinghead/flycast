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
#include <locale>
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
	CustomTextureSource(const std::string game_id)
	{
		textures_path = hostfs::getTextureLoadPath(game_id);
		if (!textures_path.empty())
		{
			try {
				hostfs::FileInfo fileInfo = hostfs::storage().getFileInfo(textures_path);
				if (fileInfo.isDirectory)
				{
					NOTICE_LOG(RENDERER, "Found custom textures directory: %s", textures_path.c_str());
					custom_textures_available = true;
				}
			} catch (const FlycastException& e) {
			}
		}
	}
	bool shouldReplace() const override { return config::CustomTextures && custom_textures_available; }
	bool shouldPreload() const override { return shouldReplace() && config::PreloadCustomTextures; }
	bool loadMap() override;
	size_t getTextureCount() const override { return texture_map.size(); }
	void preloadTextures(TextureCallback callback, std::atomic<bool>* stop_flag) override;
	u8* loadCustomTexture(u32 hash, int& width, int& height) override;
	bool isTextureReplaced(u32 hash) override final;
	
	

private:
	bool custom_textures_available = false;
	std::string textures_path;
	std::map<u32, std::string> texture_map;
};

bool CustomTextureSource::loadMap()
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

void CustomTextureSource::preloadTextures(TextureCallback callback, std::atomic<bool>* stop_flag)
{
	for (auto const& [hash, path] : texture_map)
	{
		if (stop_flag != nullptr && *stop_flag)
			return;
		int w, h;
		u8* data = loadCustomTexture(hash, w, h);
		if (data != nullptr)
		{
			size_t size = (size_t)w * h * 4;
			TextureData tex;
			tex.w = w;
			tex.h = h;
			tex.data.resize(size);
			memcpy(tex.data.data(), data, size);
			stbi_image_free(data);
			callback(hash, std::move(tex));
		}
	}
}

u8* CustomTextureSource::loadCustomTexture(u32 hash, int& width, int& height)
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

bool CustomTextureSource::isTextureReplaced(u32 hash)
{
	return texture_map.count(hash);
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
		stop_preload = false;
		resetPreloadProgress();
		pending_preloads = 0;
		initialized = true;

		std::string game_id = getGameId();
		if (game_id.length() > 0)
		{
			addSource(std::make_unique<CustomTextureSource>(game_id));
		}
	}

	return loaderThread != nullptr;
}

bool CustomTexture::enabled() {
	return loaderThread != nullptr;
}

bool CustomTexture::preloaded() {
	return preload_total > 0;
}

bool CustomTexture::isPreloading() {
	if (pending_preloads > 0)
		return true;

	int texLoaded = 0;
	int texTotal = 0;
	size_t loaded_size_b = 0;
	getPreloadProgress(texLoaded, texTotal, loaded_size_b);
	
	return (texTotal > 0 && texLoaded < texTotal);
}

void CustomTexture::addSource(std::unique_ptr<BaseCustomTextureSource> source)
{
	BaseCustomTextureSource* ptr = source.get();
	sources.emplace_back(std::move(source));
	
	if (initialized)
	{
		if (!loaderThread && ptr->shouldReplace())
		{
			loaderThread = std::make_unique<WorkerThread>("CustomTexLoader");
		}
		if (loaderThread)
		{
			if (ptr->shouldPreload())
				pending_preloads++;
			loaderThread->run([this, ptr]() {
				prepareSource(ptr);
			});
		}
	}
}

CustomTexture::~CustomTexture() {
	terminate();
}

void CustomTexture::terminate()
{
	stop_preload = true;
	if (loaderThread)
		loaderThread->stop();
	loaderThread.reset();
	for (auto& source : sources)
		source->terminate();
	sources.clear();
	preloaded_textures.clear();
	resetPreloadProgress();
	initialized = false;
}

u8* CustomTexture::loadTexture(u32 hash, int& width, int& height)
{
	auto it = preloaded_textures.find(hash);
	if (it != preloaded_textures.end())
	{
		width = it->second.w;
		height = it->second.h;
		size_t size = (size_t)width * height * 4;
		u8* buffer = (u8*)malloc(size);
		if (buffer == nullptr)
			return nullptr;
		memcpy(buffer, it->second.data.data(), size);
		return buffer;
	}

	for (auto it = sources.rbegin(); it != sources.rend(); ++it)
	{
		auto& source = *it;
		if (source->shouldReplace())
		{
			u8* data = source->loadCustomTexture(hash, width, height);
			if (data != nullptr)
				return data;
		}
	}
	return nullptr;
}

bool CustomTexture::isTextureReplaced(BaseTextureCacheData* texture)
{
	if (preloaded_textures.count(texture->texture_hash))
		return true;
	if (texture->old_vqtexture_hash != 0 && preloaded_textures.count(texture->old_vqtexture_hash))
		return true;
	if (texture->old_texture_hash != 0 && preloaded_textures.count(texture->old_texture_hash))
		return true;

	for (auto it = sources.rbegin(); it != sources.rend(); ++it)
	{
		auto& source = *it;
		if (source->shouldReplace())
		{
			if (source->isTextureReplaced(texture->texture_hash))
				return true;
			if (texture->old_vqtexture_hash != 0 && source->isTextureReplaced(texture->old_vqtexture_hash))
				return true;
			if (texture->old_texture_hash != 0 && source->isTextureReplaced(texture->old_texture_hash))
				return true;
		}
	}
	return false;
}

void CustomTexture::loadCustomTextureAsync(BaseTextureCacheData *texture_data)
{
	if (!init())
		return;

	texture_data->custom_load_in_progress++;
	loaderThread->run([this, texture_data]() {
		loadTexture(texture_data);
	});
}

void CustomTexture::dumpTexture(BaseTextureCacheData* texture, int w, int h, void *src_buffer)
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

	std::ostringstream path;
	path.imbue(std::locale::classic());
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

void CustomTexture::prepareSource(BaseCustomTextureSource* source)
{
	bool should_preload = source->shouldPreload();

	if (!stop_preload && source->loadMap())
	{
		if (should_preload)
		{
			int count = static_cast<int>(source->getTextureCount());
			if (count > 0)
			{
				preload_total += count;
				auto callback = [this](u32 hash, TextureData&& data) {
					size_t size = data.data.size();
					preloaded_textures[hash] = std::move(data);
					preload_loaded++;
					preload_loaded_size += size;
				};
				source->preloadTextures(callback, &stop_preload);
			}
		}
	}

	if (should_preload)
		pending_preloads--;
}

void CustomTexture::getPreloadProgress(int& completed, int& total, size_t& loaded_size) const
{
	total = preload_total;
	if (total == 0 && pending_preloads > 0)
		total = -1; // Prints Preparing... in UI
	completed = preload_loaded;
	loaded_size = preload_loaded_size;
}

void CustomTexture::resetPreloadProgress()
{
	preload_total = 0;
	preload_loaded = 0;
	preload_loaded_size = 0;
}
