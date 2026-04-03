#pragma once

#include <cstdio>
#include <utility>

#include <stb_image.h>

namespace STBI
{
	// Moved out here to avoid being duplicated by the template.
	inline const stbi_io_callbacks callbacks = {
		[](void *user, char *data, int size) -> int
		{
			auto file = static_cast<std::FILE*>(user);
			return std::fread(data, 1, size, file);
		},
		[](void *user, int n)
		{
			auto file = static_cast<std::FILE*>(user);
			std::fseek(file, n, SEEK_CUR);
		},
		[](void *user) -> int
		{
			auto file = static_cast<std::FILE*>(user);
			return std::feof(file);
		}
	};
}

template <typename... Ts>
inline auto stbi_load_from_file(std::FILE *file, Ts &&...args)
{
	return stbi_load_from_callbacks(&STBI::callbacks, file, std::forward<Ts>(args)...);
}
