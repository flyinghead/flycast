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
#include "TextureTranscoder.h"

#include <basis_universal/transcoder/basisu_transcoder.h>

#include <algorithm>
#include <limits>
#include <mutex>
#include <new>

namespace
{
constexpr uint32_t MaxMipLevels = 16;

TextureTranscodeStatus error(TextureTranscodeError category, const std::string& message)
{
	return { category, message };
}

basist::transcoder_texture_format basisTarget(NativeTextureFormat format)
{
	using Format = basist::transcoder_texture_format;
	switch (format)
	{
	case NativeTextureFormat::Rgba8Unorm: return Format::cTFRGBA32;
	case NativeTextureFormat::Bc7Unorm:
	case NativeTextureFormat::Bc7Srgb: return Format::cTFBC7_RGBA;
	case NativeTextureFormat::Bc3Unorm: return Format::cTFBC3_RGBA;
	case NativeTextureFormat::Etc2Rgba8Unorm: return Format::cTFETC2_RGBA;
	case NativeTextureFormat::Astc4x4Unorm: return Format::cTFASTC_LDR_4x4_RGBA;
	case NativeTextureFormat::Astc5x4Unorm: return Format::cTFASTC_LDR_5x4_RGBA;
	case NativeTextureFormat::Astc5x5Unorm: return Format::cTFASTC_LDR_5x5_RGBA;
	case NativeTextureFormat::Astc6x5Unorm: return Format::cTFASTC_LDR_6x5_RGBA;
	case NativeTextureFormat::Astc6x6Unorm: return Format::cTFASTC_LDR_6x6_RGBA;
	case NativeTextureFormat::Astc8x5Unorm: return Format::cTFASTC_LDR_8x5_RGBA;
	case NativeTextureFormat::Astc8x6Unorm: return Format::cTFASTC_LDR_8x6_RGBA;
	case NativeTextureFormat::Astc10x5Unorm: return Format::cTFASTC_LDR_10x5_RGBA;
	case NativeTextureFormat::Astc10x6Unorm: return Format::cTFASTC_LDR_10x6_RGBA;
	case NativeTextureFormat::Astc8x8Unorm: return Format::cTFASTC_LDR_8x8_RGBA;
	case NativeTextureFormat::Astc10x8Unorm: return Format::cTFASTC_LDR_10x8_RGBA;
	case NativeTextureFormat::Astc10x10Unorm: return Format::cTFASTC_LDR_10x10_RGBA;
	case NativeTextureFormat::Astc12x10Unorm: return Format::cTFASTC_LDR_12x10_RGBA;
	case NativeTextureFormat::Astc12x12Unorm: return Format::cTFASTC_LDR_12x12_RGBA;
	case NativeTextureFormat::Count: break;
	}
	return Format::cTFTotalTextureFormats;
}

TranscodePathClass pathClass(CustomTextureCodec codec, NativeTextureFormat target,
		uint32_t sourceBlockWidth, uint32_t sourceBlockHeight)
{
	if (target == NativeTextureFormat::Rgba8Unorm)
		return TranscodePathClass::DecodeToRgba;
	if (codec == CustomTextureCodec::DdsBc7 &&
			(target == NativeTextureFormat::Bc7Unorm || target == NativeTextureFormat::Bc7Srgb))
		return TranscodePathClass::PassthroughCopy;
	if (codec == CustomTextureCodec::Xubc7 && target == NativeTextureFormat::Bc7Unorm)
		return TranscodePathClass::FastNativeReconstruction;
	if (codec == CustomTextureCodec::Xubc7 && target == NativeTextureFormat::Astc4x4Unorm)
		return TranscodePathClass::LatentToLatent;
	if (codec == CustomTextureCodec::XuastcLdr)
	{
		const BlockGeometry geometry = getBlockGeometry(target);
		if (geometry.compressed && geometry.blockWidth == sourceBlockWidth
				&& geometry.blockHeight == sourceBlockHeight
				&& target >= NativeTextureFormat::Astc4x4Unorm)
			return TranscodePathClass::FastNativeReconstruction;
	}
	return TranscodePathClass::DecodeAndReencode;
}

bool hasUnsupportedOrientation(const basist::ktx2_transcoder& transcoder)
{
	const basisu::uint8_vec *value = transcoder.find_key("KTXorientation");
	if (value == nullptr)
		return false;
	if (value->empty())
		return true;

	// Flycast replacements use the same bottom-up payload ordering as PNG/JPEG
	// after stb's load-time flip. "ru" describes that ordering without changing
	// the compressed bytes, allowing orientation-aware tools to display it upright.
	// Missing metadata remains valid for existing Basis files encoded with -y_flip.
	size_t length = 0;
	while (length < value->size() && (*value)[length] != 0)
		++length;
	const auto lower = [](uint8_t c) {
		return c >= 'A' && c <= 'Z' ? static_cast<uint8_t>(c + ('a' - 'A')) : c;
	};
	if (length == 2)
		return lower((*value)[0]) != 'r' || lower((*value)[1]) != 'u';

	// Also recognize the KTX1-style spelling used by the previous check.
	return length != 7
			|| lower((*value)[0]) != 's' || (*value)[1] != '=' || lower((*value)[2]) != 'r'
			|| (*value)[3] != ','
			|| lower((*value)[4]) != 't' || (*value)[5] != '=' || lower((*value)[6]) != 'u';
}

TextureTranscodeStatus validateCommon(uint32_t width, uint32_t height, uint32_t levels)
{
	if (width == 0 || height == 0)
		return error(TextureTranscodeError::MalformedSource, "texture dimensions are invalid");
	if (levels == 0 || levels > MaxMipLevels)
		return error(TextureTranscodeError::MalformedSource, "invalid mip count");
	uint32_t maxLevels = 1;
	for (uint32_t dim = std::max(width, height); dim > 1; dim >>= 1)
		++maxLevels;
	if (levels > maxLevels)
		return error(TextureTranscodeError::MalformedSource, "mip count exceeds the complete chain length");
	return {};
}

TextureTranscodeStatus allocatePayload(const TextureInspection& inspection,
		NativeTextureFormat target, PreparedCustomTexture& output)
{
	uint64_t offset = 0;
	uint32_t width = inspection.width;
	uint32_t height = inspection.height;
	try
	{
		output.levels.reserve(inspection.levels);
		for (uint32_t levelIndex = 0; levelIndex < inspection.levels; ++levelIndex)
		{
			PreparedMipLevel level;
			std::string layoutError;
			if (!computeMipLayout(target, width, height, offset, level, layoutError))
				return error(TextureTranscodeError::MalformedSource, layoutError);
			output.levels.push_back(level);
			offset += level.byteSize;
			width = std::max(1u, width / 2);
			height = std::max(1u, height / 2);
		}
		output.bytes.resize(static_cast<size_t>(offset));
	}
	catch (const std::bad_alloc&)
	{
		return error(TextureTranscodeError::AllocationFailure, "unable to allocate prepared texture payload");
	}
	return {};
}

uint32_t outputCapacity(const PreparedMipLevel& level, NativeTextureFormat target)
{
	const BlockGeometry geometry = getBlockGeometry(target);
	const uint64_t units = geometry.compressed
			? static_cast<uint64_t>(level.blocksX) * level.blocksY
			: static_cast<uint64_t>(level.width) * level.height;
	return units <= std::numeric_limits<uint32_t>::max() ? static_cast<uint32_t>(units) : 0;
}
}

void TextureTranscoder::initializeOnce()
{
	static std::once_flag once;
	std::call_once(once, []() { basist::basisu_transcoder_init(); });
}

TextureTranscodeStatus TextureTranscoder::inspect(CustomTextureSourceKind hintedKind,
		const std::vector<uint8_t>& fileBytes, TextureInspection& inspection) const
{
	initializeOnce();
	if (fileBytes.empty() || fileBytes.size() > std::numeric_limits<uint32_t>::max())
		return error(TextureTranscodeError::MalformedSource, "source file is empty or exceeds the Basis input range");

	if (hintedKind == CustomTextureSourceKind::DdsBc7)
	{
		basist::dds_transcoder transcoder;
		if (!transcoder.init(fileBytes.data(), static_cast<uint32_t>(fileBytes.size())))
			return error(TextureTranscodeError::MalformedSource, "Basis rejected the DDS container");
		if (transcoder.get_source_kind() != basist::dds_transcoder::source_kind::cBC7
				|| transcoder.get_dds_format() != basist::dds_format::cBC7)
			return error(TextureTranscodeError::UnsupportedSource, "DDS source is not BC7");
		if (transcoder.get_faces() != 1 || transcoder.get_layers() != 0 || transcoder.get_is_cubemap())
			return error(TextureTranscodeError::UnsupportedSource, "DDS arrays and cubemaps are not supported");
		if (TextureTranscodeStatus status = validateCommon(transcoder.get_width(), transcoder.get_height(), transcoder.get_levels()); !status)
			return status;
		inspection = { CustomTextureCodec::DdsBc7, transcoder.get_width(), transcoder.get_height(),
				transcoder.get_levels(), 4, 4, transcoder.is_srgb(), transcoder.get_has_alpha() != 0 };
		return {};
	}

	basist::ktx2_transcoder transcoder;
	if (!transcoder.init(fileBytes.data(), static_cast<uint32_t>(fileBytes.size())))
		return error(TextureTranscodeError::MalformedSource, "Basis rejected the KTX2 container");
	const auto& header = transcoder.get_header();
	if (header.m_pixel_depth != 0 || transcoder.get_faces() != 1 || transcoder.get_layers() != 0)
		return error(TextureTranscodeError::UnsupportedSource, "only non-array 2D KTX2 textures are supported");
	if (!transcoder.is_ldr())
		return error(TextureTranscodeError::UnsupportedSource, "HDR KTX2 textures are not supported");
	CustomTextureCodec codec;
	if (transcoder.is_xubc7())
		codec = CustomTextureCodec::Xubc7;
	else if (transcoder.is_xuastc_ldr())
		codec = CustomTextureCodec::XuastcLdr;
	else
		return error(TextureTranscodeError::UnsupportedSource, "KTX2 is neither XUBC7 nor XUASTC LDR");
	if ((hintedKind == CustomTextureSourceKind::Ktx2Xubc7 && codec != CustomTextureCodec::Xubc7)
			|| (hintedKind == CustomTextureSourceKind::Ktx2Xuastc && codec != CustomTextureCodec::XuastcLdr))
		return error(TextureTranscodeError::UnsupportedSource, "explicit KTX2 suffix does not match the encoded codec");
	if (hasUnsupportedOrientation(transcoder))
		return error(TextureTranscodeError::UnsupportedSource, "KTXorientation must be ru for Flycast replacements");
	if (TextureTranscodeStatus status = validateCommon(transcoder.get_width(), transcoder.get_height(), transcoder.get_levels()); !status)
		return status;
	for (uint32_t levelIndex = 0; levelIndex < transcoder.get_levels(); ++levelIndex)
	{
		basist::ktx2_image_level_info info{};
		if (!transcoder.get_image_level_info(info, levelIndex, 0, 0)
				|| info.m_orig_width != std::max(1u, transcoder.get_width() >> levelIndex)
				|| info.m_orig_height != std::max(1u, transcoder.get_height() >> levelIndex))
			return error(TextureTranscodeError::MalformedSource, "KTX2 mip geometry is inconsistent");
	}
	inspection = { codec, transcoder.get_width(), transcoder.get_height(), transcoder.get_levels(),
			transcoder.get_block_width(), transcoder.get_block_height(), transcoder.is_srgb(),
			transcoder.get_has_alpha() != 0 };
	return {};
}

TextureTranscodeStatus TextureTranscoder::prepare(const TextureInspection& inspection,
		const std::vector<uint8_t>& fileBytes, NativeTextureFormat target,
		uint32_t replacementHash, const CancellationCheck& cancelled,
		PreparedCustomTexturePtr& texture) const
{
	initializeOnce();
	if (cancelled && cancelled())
		return error(TextureTranscodeError::Cancelled, "texture preparation was cancelled");
	if (fileBytes.empty() || fileBytes.size() > std::numeric_limits<uint32_t>::max())
		return error(TextureTranscodeError::MalformedSource, "source file is empty or exceeds the Basis input range");
	if (TextureTranscodeStatus status = validateCommon(inspection.width, inspection.height,
			inspection.levels); !status)
		return status;
	if (inspection.codec != CustomTextureCodec::Xubc7
			&& inspection.codec != CustomTextureCodec::XuastcLdr
			&& inspection.codec != CustomTextureCodec::DdsBc7)
		return error(TextureTranscodeError::UnsupportedSource, "inspection does not describe a supported compressed texture source");
	const auto basisFormat = basisTarget(target);
	if (basisFormat == basist::transcoder_texture_format::cTFTotalTextureFormats)
		return error(TextureTranscodeError::UnsupportedTarget, "invalid native target format");

	std::shared_ptr<PreparedCustomTexture> output;
	try
	{
		output = std::make_shared<PreparedCustomTexture>();
	}
	catch (const std::bad_alloc&)
	{
		return error(TextureTranscodeError::AllocationFailure, "unable to allocate prepared texture descriptor");
	}
	output->replacementHash = replacementHash;
	output->sourceCodec = inspection.codec;
	output->nativeFormat = target;
	output->pathClass = pathClass(inspection.codec, target, inspection.blockWidth, inspection.blockHeight);
	output->width = inspection.width;
	output->height = inspection.height;
	output->sourceSrgb = inspection.sourceSrgb;
	output->hasAlpha = inspection.hasAlpha;

	if (inspection.codec == CustomTextureCodec::DdsBc7)
	{
		basist::dds_transcoder transcoder;
		if (!transcoder.init(fileBytes.data(), static_cast<uint32_t>(fileBytes.size())))
			return error(TextureTranscodeError::MalformedSource, "Basis rejected the DDS container during preparation");
		if (transcoder.get_source_kind() != basist::dds_transcoder::source_kind::cBC7
				|| transcoder.get_dds_format() != basist::dds_format::cBC7
				|| transcoder.get_width() != inspection.width
				|| transcoder.get_height() != inspection.height
				|| transcoder.get_levels() != inspection.levels
				|| transcoder.is_srgb() != inspection.sourceSrgb
				|| (transcoder.get_has_alpha() != 0) != inspection.hasAlpha)
			return error(TextureTranscodeError::MalformedSource, "DDS no longer matches its inspection result");
		if (TextureTranscodeStatus status = allocatePayload(inspection, target, *output); !status)
			return status;
		if (!transcoder.is_transcode_format_supported(basisFormat)
				|| !transcoder.start_transcoding())
			return error(TextureTranscodeError::UnsupportedTarget, "DDS target is not supported by Basis");
		for (uint32_t levelIndex = 0; levelIndex < inspection.levels; ++levelIndex)
		{
			if (cancelled && cancelled())
				return error(TextureTranscodeError::Cancelled, "texture preparation was cancelled between mips");
			const PreparedMipLevel& level = output->levels[levelIndex];
			const uint32_t capacity = outputCapacity(level, target);
			if (capacity == 0 || !transcoder.transcode_image_level(levelIndex, 0, 0,
					output->bytes.data() + level.byteOffset, capacity, basisFormat))
				return error(TextureTranscodeError::UpstreamFailure, "Basis failed to transcode a DDS mip");
		}
	}
	else
	{
		basist::ktx2_transcoder transcoder;
		if (!transcoder.init(fileBytes.data(), static_cast<uint32_t>(fileBytes.size())))
			return error(TextureTranscodeError::MalformedSource, "Basis rejected the KTX2 container during preparation");
		const bool codecMatches = (inspection.codec == CustomTextureCodec::Xubc7 && transcoder.is_xubc7())
				|| (inspection.codec == CustomTextureCodec::XuastcLdr && transcoder.is_xuastc_ldr());
		if (!codecMatches || transcoder.get_width() != inspection.width
				|| transcoder.get_height() != inspection.height
				|| transcoder.get_levels() != inspection.levels
				|| transcoder.get_block_width() != inspection.blockWidth
				|| transcoder.get_block_height() != inspection.blockHeight
				|| transcoder.is_srgb() != inspection.sourceSrgb
				|| (transcoder.get_has_alpha() != 0) != inspection.hasAlpha)
			return error(TextureTranscodeError::MalformedSource, "KTX2 no longer matches its inspection result");
		if (TextureTranscodeStatus status = allocatePayload(inspection, target, *output); !status)
			return status;
		if (!basist::basis_is_format_supported(basisFormat, transcoder.get_basis_tex_format())
				|| !transcoder.start_transcoding())
			return error(TextureTranscodeError::UnsupportedTarget, "KTX2 target is not supported by Basis");
		basist::ktx2_transcoder_state state;
		state.clear();
		for (uint32_t levelIndex = 0; levelIndex < inspection.levels; ++levelIndex)
		{
			if (cancelled && cancelled())
				return error(TextureTranscodeError::Cancelled, "texture preparation was cancelled between mips");
			const PreparedMipLevel& level = output->levels[levelIndex];
			const uint32_t capacity = outputCapacity(level, target);
			if (capacity == 0 || !transcoder.transcode_image_level(levelIndex, 0, 0,
					output->bytes.data() + level.byteOffset, capacity, basisFormat, 0, 0, 0, -1, -1, &state))
				return error(TextureTranscodeError::UpstreamFailure, "Basis failed to transcode a KTX2 mip");
		}
	}

	std::string validationError;
	if (!validatePreparedCustomTexture(*output, validationError))
		return error(TextureTranscodeError::UpstreamFailure, validationError);
	texture = std::move(output);
	return {};
}
