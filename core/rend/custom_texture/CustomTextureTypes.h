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

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class CustomTextureSourceKind : uint8_t
{
	Ktx2Xubc7,
	Ktx2Xuastc,
	Ktx2Etc1s,
	Ktx2Generic,
	DdsBc7,
	Png,
	Jpeg,
};

enum class CustomTextureCodec : uint8_t
{
	Xubc7,
	XuastcLdr,
	Etc1s,
	DdsBc7,
};

enum class NativeTextureFormat : uint8_t
{
	Rgba8Unorm,
	Bc7Unorm,
	Bc1Unorm,
	Bc3Unorm,
	Etc2Rgb8Unorm,
	Etc2Rgba8Unorm,
	Astc4x4Unorm,
	Astc5x4Unorm,
	Astc5x5Unorm,
	Astc6x5Unorm,
	Astc6x6Unorm,
	Astc8x5Unorm,
	Astc8x6Unorm,
	Astc10x5Unorm,
	Astc10x6Unorm,
	Astc8x8Unorm,
	Astc10x8Unorm,
	Astc10x10Unorm,
	Astc12x10Unorm,
	Astc12x12Unorm,
	Count,
};

enum class CustomTextureBackend : uint8_t
{
	Unknown,
	Vulkan,
	Direct3D11,
	Direct3D9,
	OpenGL,
	OpenGLES,
};

struct CustomTextureCandidate
{
	CustomTextureSourceKind kind = CustomTextureSourceKind::Png;
	std::string path;
};

struct ParsedCustomTextureFilename
{
	uint32_t hash = 0;
	CustomTextureSourceKind kind = CustomTextureSourceKind::Png;
};

std::optional<ParsedCustomTextureFilename> parseCustomTextureFilename(const std::string& filename);

struct BlockGeometry
{
	uint32_t blockWidth = 1;
	uint32_t blockHeight = 1;
	uint32_t bytesPerBlockOrPixel = 4;
	bool compressed = false;
};

struct PreparedMipLevel
{
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t blocksX = 0;
	uint32_t blocksY = 0;
	uint64_t byteOffset = 0;
	uint64_t byteSize = 0;
	uint32_t rowPitchBytes = 0;
};

struct CustomTextureRequestId
{
	uint64_t value = 0;

	explicit operator bool() const { return value != 0; }
};

struct PreparedCustomTexture
{
	using Ptr = std::shared_ptr<const PreparedCustomTexture>;

	uint32_t replacementHash = 0;
	NativeTextureFormat nativeFormat = NativeTextureFormat::Rgba8Unorm;
	uint32_t width = 0;
	uint32_t height = 0;
	// Legacy images contain only level 0. Renderers generate the rest when mipmapping is used.
	bool generateMipmaps = false;
	std::vector<PreparedMipLevel> levels;
	std::vector<uint8_t> bytes;
};

struct CustomTextureCapabilities
{
	CustomTextureBackend backend = CustomTextureBackend::Unknown;
	uint32_t max2DWidth = 16384;
	uint32_t max2DHeight = 16384;
	std::array<bool, static_cast<size_t>(NativeTextureFormat::Count)> sampledFormats{};

	bool supports(NativeTextureFormat format) const;
	bool canUpload(NativeTextureFormat format, uint32_t width, uint32_t height,
			uint32_t levels) const;
	void setSupported(NativeTextureFormat format, bool supported = true);

	static CustomTextureCapabilities rgbaOnly(CustomTextureBackend backend,
			uint32_t maxDimension = 16384);
};

BlockGeometry getBlockGeometry(NativeTextureFormat format);
uint32_t mipmapLevelCount(uint32_t width, uint32_t height);
PreparedMipLevel computeMipLayout(NativeTextureFormat format, uint32_t width, uint32_t height,
		uint64_t offset);
void validatePreparedCustomTexture(const PreparedCustomTexture& texture);
std::vector<NativeTextureFormat> selectNativeTextureTargets(const CustomTextureCapabilities& capabilities,
		CustomTextureCodec codec, uint32_t sourceBlockWidth = 4, uint32_t sourceBlockHeight = 4,
		bool hasAlpha = true);
const char *customTextureCodecName(CustomTextureCodec codec);
const char *nativeTextureFormatName(NativeTextureFormat format);
