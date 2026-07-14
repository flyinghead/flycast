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
#include "CustomTextureTypes.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>

namespace
{
constexpr uint64_t MaxPreparedTextureBytes = 512ull * 1024 * 1024;

bool checkedAdd(uint64_t a, uint64_t b, uint64_t& result)
{
	if (a > std::numeric_limits<uint64_t>::max() - b)
		return false;
	result = a + b;
	return true;
}

bool checkedMultiply(uint64_t a, uint64_t b, uint64_t& result)
{
	if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a)
		return false;
	result = a * b;
	return true;
}

std::string lowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value;
}

bool endsWith(const std::string& value, const char *suffix)
{
	const size_t suffixLength = std::char_traits<char>::length(suffix);
	return value.size() >= suffixLength
			&& value.compare(value.size() - suffixLength, suffixLength, suffix) == 0;
}

std::optional<NativeTextureFormat> astcFormat(uint32_t width, uint32_t height)
{
	if (width == 4 && height == 4) return NativeTextureFormat::Astc4x4Unorm;
	if (width == 5 && height == 4) return NativeTextureFormat::Astc5x4Unorm;
	if (width == 5 && height == 5) return NativeTextureFormat::Astc5x5Unorm;
	if (width == 6 && height == 5) return NativeTextureFormat::Astc6x5Unorm;
	if (width == 6 && height == 6) return NativeTextureFormat::Astc6x6Unorm;
	if (width == 8 && height == 5) return NativeTextureFormat::Astc8x5Unorm;
	if (width == 8 && height == 6) return NativeTextureFormat::Astc8x6Unorm;
	if (width == 10 && height == 5) return NativeTextureFormat::Astc10x5Unorm;
	if (width == 10 && height == 6) return NativeTextureFormat::Astc10x6Unorm;
	if (width == 8 && height == 8) return NativeTextureFormat::Astc8x8Unorm;
	if (width == 10 && height == 8) return NativeTextureFormat::Astc10x8Unorm;
	if (width == 10 && height == 10) return NativeTextureFormat::Astc10x10Unorm;
	if (width == 12 && height == 10) return NativeTextureFormat::Astc12x10Unorm;
	if (width == 12 && height == 12) return NativeTextureFormat::Astc12x12Unorm;
	return std::nullopt;
}

void appendIfSupported(std::vector<NativeTextureFormat>& targets,
		const CustomTextureCapabilities& capabilities, NativeTextureFormat format)
{
	if (capabilities.supports(format)
			&& std::find(targets.begin(), targets.end(), format) == targets.end())
		targets.push_back(format);
}
}

std::optional<ParsedCustomTextureFilename> parseCustomTextureFilename(const std::string& filename)
{
	const std::string lower = lowerAscii(filename);
	CustomTextureSourceKind kind;
	size_t suffixLength = 0;
	if (endsWith(lower, ".xubc7.ktx2"))
	{
		kind = CustomTextureSourceKind::Ktx2Xubc7;
		suffixLength = sizeof(".xubc7.ktx2") - 1;
	}
	else if (endsWith(lower, ".xuastc.ktx2"))
	{
		kind = CustomTextureSourceKind::Ktx2Xuastc;
		suffixLength = sizeof(".xuastc.ktx2") - 1;
	}
	else if (endsWith(lower, ".ktx2"))
	{
		kind = CustomTextureSourceKind::Ktx2Generic;
		suffixLength = sizeof(".ktx2") - 1;
	}
	else if (endsWith(lower, ".dds"))
	{
		kind = CustomTextureSourceKind::DdsBc7;
		suffixLength = sizeof(".dds") - 1;
	}
	else if (endsWith(lower, ".png"))
	{
		kind = CustomTextureSourceKind::Png;
		suffixLength = sizeof(".png") - 1;
	}
	else if (endsWith(lower, ".jpeg"))
	{
		kind = CustomTextureSourceKind::Jpeg;
		suffixLength = sizeof(".jpeg") - 1;
	}
	else if (endsWith(lower, ".jpg"))
	{
		kind = CustomTextureSourceKind::Jpeg;
		suffixLength = sizeof(".jpg") - 1;
	}
	else
		return std::nullopt;

	const std::string hashText = filename.substr(0, filename.size() - suffixLength);
	if (hashText.empty() || hashText.size() > 8)
		return std::nullopt;
	uint32_t hash = 0;
	const auto result = std::from_chars(hashText.data(), hashText.data() + hashText.size(), hash, 16);
	if (result.ec != std::errc() || result.ptr != hashText.data() + hashText.size())
		return std::nullopt;
	return ParsedCustomTextureFilename { hash, kind };
}

bool CustomTextureCapabilities::supports(NativeTextureFormat format) const
{
	const size_t index = static_cast<size_t>(format);
	return index < sampledFormats.size() && sampledFormats[index];
}

bool CustomTextureCapabilities::canUpload(NativeTextureFormat format, uint32_t width,
		uint32_t height, uint32_t levels) const
{
	return supports(format) && width != 0 && height != 0 && levels != 0 && levels <= 16
			&& width <= max2DWidth && height <= max2DHeight;
}

void CustomTextureCapabilities::setSupported(NativeTextureFormat format, bool supported)
{
	const size_t index = static_cast<size_t>(format);
	if (index < sampledFormats.size())
		sampledFormats[index] = supported;
}

CustomTextureCapabilities CustomTextureCapabilities::rgbaOnly(CustomTextureBackend backend,
		uint32_t maxDimension)
{
	CustomTextureCapabilities capabilities;
	capabilities.backend = backend;
	capabilities.max2DWidth = maxDimension;
	capabilities.max2DHeight = maxDimension;
	capabilities.setSupported(NativeTextureFormat::Rgba8Unorm);
	return capabilities;
}

BlockGeometry getBlockGeometry(NativeTextureFormat format)
{
	switch (format)
	{
	case NativeTextureFormat::Rgba8Unorm: return { 1, 1, 4, false };
	case NativeTextureFormat::Bc7Unorm:
	case NativeTextureFormat::Bc7Srgb:
	case NativeTextureFormat::Bc3Unorm:
	case NativeTextureFormat::Etc2Rgba8Unorm: return { 4, 4, 16, true };
	case NativeTextureFormat::Astc4x4Unorm: return { 4, 4, 16, true };
	case NativeTextureFormat::Astc5x4Unorm: return { 5, 4, 16, true };
	case NativeTextureFormat::Astc5x5Unorm: return { 5, 5, 16, true };
	case NativeTextureFormat::Astc6x5Unorm: return { 6, 5, 16, true };
	case NativeTextureFormat::Astc6x6Unorm: return { 6, 6, 16, true };
	case NativeTextureFormat::Astc8x5Unorm: return { 8, 5, 16, true };
	case NativeTextureFormat::Astc8x6Unorm: return { 8, 6, 16, true };
	case NativeTextureFormat::Astc10x5Unorm: return { 10, 5, 16, true };
	case NativeTextureFormat::Astc10x6Unorm: return { 10, 6, 16, true };
	case NativeTextureFormat::Astc8x8Unorm: return { 8, 8, 16, true };
	case NativeTextureFormat::Astc10x8Unorm: return { 10, 8, 16, true };
	case NativeTextureFormat::Astc10x10Unorm: return { 10, 10, 16, true };
	case NativeTextureFormat::Astc12x10Unorm: return { 12, 10, 16, true };
	case NativeTextureFormat::Astc12x12Unorm: return { 12, 12, 16, true };
	case NativeTextureFormat::Count: break;
	}
	return {};
}

uint32_t mipmapLevelCount(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return 0;
	uint32_t levels = 1;
	while (width > 1 || height > 1)
	{
		width = std::max(1u, width / 2);
		height = std::max(1u, height / 2);
		++levels;
	}
	return levels;
}

bool computeMipLayout(NativeTextureFormat format, uint32_t width, uint32_t height,
		uint64_t offset, PreparedMipLevel& level, std::string& error)
{
	if (width == 0 || height == 0 || format == NativeTextureFormat::Count)
	{
		error = "invalid mip dimensions or format";
		return false;
	}
	const BlockGeometry geometry = getBlockGeometry(format);
	const uint64_t blocksX = (static_cast<uint64_t>(width) + geometry.blockWidth - 1) / geometry.blockWidth;
	const uint64_t blocksY = (static_cast<uint64_t>(height) + geometry.blockHeight - 1) / geometry.blockHeight;
	uint64_t rowPitch = 0;
	uint64_t byteSize = 0;
	uint64_t end = 0;
	if (!checkedMultiply(blocksX, geometry.bytesPerBlockOrPixel, rowPitch)
			|| !checkedMultiply(rowPitch, blocksY, byteSize)
			|| !checkedAdd(offset, byteSize, end)
			|| rowPitch > std::numeric_limits<uint32_t>::max()
			|| blocksX > std::numeric_limits<uint32_t>::max()
			|| blocksY > std::numeric_limits<uint32_t>::max()
			|| end > MaxPreparedTextureBytes)
	{
		error = "mip layout exceeds supported allocation limits";
		return false;
	}
	level = { width, height, static_cast<uint32_t>(blocksX), static_cast<uint32_t>(blocksY),
			offset, byteSize, static_cast<uint32_t>(rowPitch) };
	return true;
}

bool validatePreparedCustomTexture(const PreparedCustomTexture& texture, std::string& error)
{
	if (texture.width == 0 || texture.height == 0 || texture.levels.empty()
			|| texture.levels.size() > 16 || texture.nativeFormat == NativeTextureFormat::Count)
	{
		error = "invalid prepared texture header";
		return false;
	}
	if (texture.generateMipmaps && (texture.nativeFormat != NativeTextureFormat::Rgba8Unorm
			|| texture.levels.size() != 1))
	{
		error = "generated mipmaps require one RGBA source level";
		return false;
	}
	uint64_t expectedOffset = 0;
	uint32_t expectedWidth = texture.width;
	uint32_t expectedHeight = texture.height;
	for (const PreparedMipLevel& level : texture.levels)
	{
		PreparedMipLevel expected;
		if (!computeMipLayout(texture.nativeFormat, expectedWidth, expectedHeight, expectedOffset,
				expected, error))
			return false;
		if (level.width != expected.width || level.height != expected.height
				|| level.blocksX != expected.blocksX || level.blocksY != expected.blocksY
				|| level.byteOffset != expected.byteOffset || level.byteSize != expected.byteSize
				|| level.rowPitchBytes != expected.rowPitchBytes)
		{
			error = "prepared mip layout is inconsistent";
			return false;
		}
		expectedOffset += expected.byteSize;
		expectedWidth = std::max(1u, expectedWidth / 2);
		expectedHeight = std::max(1u, expectedHeight / 2);
	}
	if (expectedOffset != texture.bytes.size())
	{
		error = "prepared byte vector does not match mip ranges";
		return false;
	}
	return true;
}

std::vector<NativeTextureFormat> selectNativeTextureTargets(const CustomTextureCapabilities& capabilities,
		CustomTextureCodec codec, uint32_t sourceBlockWidth, uint32_t sourceBlockHeight)
{
	std::vector<NativeTextureFormat> targets;
	if (codec == CustomTextureCodec::XuastcLdr)
	{
		if (const auto matchingAstc = astcFormat(sourceBlockWidth, sourceBlockHeight))
			appendIfSupported(targets, capabilities, *matchingAstc);
		appendIfSupported(targets, capabilities, NativeTextureFormat::Bc7Unorm);
		appendIfSupported(targets, capabilities, NativeTextureFormat::Etc2Rgba8Unorm);
		appendIfSupported(targets, capabilities, NativeTextureFormat::Bc3Unorm);
	}
	else if (codec == CustomTextureCodec::Xubc7 || codec == CustomTextureCodec::DdsBc7)
	{
		appendIfSupported(targets, capabilities, NativeTextureFormat::Bc7Unorm);
		appendIfSupported(targets, capabilities, NativeTextureFormat::Astc4x4Unorm);
		appendIfSupported(targets, capabilities, NativeTextureFormat::Etc2Rgba8Unorm);
		appendIfSupported(targets, capabilities, NativeTextureFormat::Bc3Unorm);
	}
	appendIfSupported(targets, capabilities, NativeTextureFormat::Rgba8Unorm);
	return targets;
}

const char *customTextureCodecName(CustomTextureCodec codec)
{
	switch (codec)
	{
	case CustomTextureCodec::LegacyRgba: return "PNG/JPEG";
	case CustomTextureCodec::Xubc7: return "XUBC7";
	case CustomTextureCodec::XuastcLdr: return "XUASTC LDR";
	case CustomTextureCodec::DdsBc7: return "DDS BC7";
	}
	return "unknown";
}

const char *nativeTextureFormatName(NativeTextureFormat format)
{
	switch (format)
	{
	case NativeTextureFormat::Rgba8Unorm: return "RGBA8 UNORM";
	case NativeTextureFormat::Bc7Unorm: return "BC7 UNORM";
	case NativeTextureFormat::Bc7Srgb: return "BC7 SRGB";
	case NativeTextureFormat::Bc3Unorm: return "BC3 UNORM";
	case NativeTextureFormat::Etc2Rgba8Unorm: return "ETC2 RGBA8 UNORM";
	case NativeTextureFormat::Astc4x4Unorm: return "ASTC 4x4 UNORM";
	case NativeTextureFormat::Astc5x4Unorm: return "ASTC 5x4 UNORM";
	case NativeTextureFormat::Astc5x5Unorm: return "ASTC 5x5 UNORM";
	case NativeTextureFormat::Astc6x5Unorm: return "ASTC 6x5 UNORM";
	case NativeTextureFormat::Astc6x6Unorm: return "ASTC 6x6 UNORM";
	case NativeTextureFormat::Astc8x5Unorm: return "ASTC 8x5 UNORM";
	case NativeTextureFormat::Astc8x6Unorm: return "ASTC 8x6 UNORM";
	case NativeTextureFormat::Astc10x5Unorm: return "ASTC 10x5 UNORM";
	case NativeTextureFormat::Astc10x6Unorm: return "ASTC 10x6 UNORM";
	case NativeTextureFormat::Astc8x8Unorm: return "ASTC 8x8 UNORM";
	case NativeTextureFormat::Astc10x8Unorm: return "ASTC 10x8 UNORM";
	case NativeTextureFormat::Astc10x10Unorm: return "ASTC 10x10 UNORM";
	case NativeTextureFormat::Astc12x10Unorm: return "ASTC 12x10 UNORM";
	case NativeTextureFormat::Astc12x12Unorm: return "ASTC 12x12 UNORM";
	case NativeTextureFormat::Count: break;
	}
	return "unknown";
}
