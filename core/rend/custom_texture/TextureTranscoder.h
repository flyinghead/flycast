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
#include <memory>
#include <vector>

namespace basist
{
class dds_transcoder;
class ktx2_transcoder;
}

struct TextureInspection
{
	CustomTextureCodec codec = CustomTextureCodec::Xubc7;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t levels = 0;
	uint32_t blockWidth = 1;
	uint32_t blockHeight = 1;
	bool hasAlpha = true;
};

class TextureTranscoder
{
public:
	using CancellationCheck = std::function<bool()>;

	TextureTranscoder();
	~TextureTranscoder();

	TextureInspection inspect(CustomTextureSourceKind hintedKind,
			std::vector<uint8_t> fileBytes);

	PreparedCustomTexture::Ptr prepare(NativeTextureFormat target,
			uint32_t replacementHash, const CancellationCheck& cancelled);

private:
	static void initializeOnce();

	std::vector<uint8_t> fileBytes;
	TextureInspection inspection;
	std::unique_ptr<basist::dds_transcoder> ddsTranscoder;
	std::unique_ptr<basist::ktx2_transcoder> ktx2Transcoder;
};
