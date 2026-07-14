/*
	Copyright 2026 Edward Li

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "CustomTextureTypes.h"

#include <functional>
#include <string>
#include <vector>

enum class TextureTranscodeError : uint8_t
{
	None,
	UnsupportedSource,
	MalformedSource,
	UnsupportedTarget,
	Cancelled,
	AllocationFailure,
	UpstreamFailure,
};

struct TextureTranscodeStatus
{
	TextureTranscodeError category = TextureTranscodeError::None;
	std::string message;

	explicit operator bool() const { return category == TextureTranscodeError::None; }
};

struct TextureInspection
{
	CustomTextureCodec codec = CustomTextureCodec::LegacyRgba;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t levels = 0;
	uint32_t blockWidth = 1;
	uint32_t blockHeight = 1;
	bool sourceSrgb = false;
	bool hasAlpha = true;
};

class TextureTranscoder
{
public:
	using CancellationCheck = std::function<bool()>;

	static void initializeOnce();

	TextureTranscodeStatus inspect(CustomTextureSourceKind hintedKind,
			const std::vector<uint8_t>& fileBytes, TextureInspection& inspection) const;

	TextureTranscodeStatus prepare(const TextureInspection& inspection,
			const std::vector<uint8_t>& fileBytes, NativeTextureFormat target,
			uint32_t replacementHash, const CancellationCheck& cancelled,
			PreparedCustomTexturePtr& texture) const;
};
