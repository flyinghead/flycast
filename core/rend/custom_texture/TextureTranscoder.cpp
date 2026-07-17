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
#include "types.h"

#include <basis_universal/transcoder/basisu_transcoder.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <mutex>
#include <utility>

namespace
{
constexpr uint32_t MaxMipLevels = 16;

basist::transcoder_texture_format basisTarget(NativeTextureFormat format)
{
	using Format = basist::transcoder_texture_format;
	switch (format)
	{
	case NativeTextureFormat::Rgba8Unorm: return Format::cTFRGBA32;
	case NativeTextureFormat::Bc7Unorm: return Format::cTFBC7_RGBA;
	case NativeTextureFormat::Bc1Unorm: return Format::cTFBC1_RGB;
	case NativeTextureFormat::Bc3Unorm: return Format::cTFBC3_RGBA;
	// ETC1 blocks are valid ETC2 RGB blocks and use the same 8-byte layout.
	case NativeTextureFormat::Etc2Rgb8Unorm: return Format::cTFETC1_RGB;
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

void validateCommon(uint32_t width, uint32_t height, uint32_t levels)
{
	if (width == 0 || height == 0)
		throw FlycastException("texture dimensions are invalid");
	if (levels == 0 || levels > MaxMipLevels)
		throw FlycastException("invalid mip count");
	uint32_t maxLevels = 1;
	for (uint32_t dim = std::max(width, height); dim > 1; dim >>= 1)
		++maxLevels;
	if (levels > maxLevels)
		throw FlycastException("mip count exceeds the complete chain length");
}

void allocatePayload(const TextureInspection& inspection,
		NativeTextureFormat target, PreparedCustomTexture& output)
{
	uint64_t offset = 0;
	uint32_t width = inspection.width;
	uint32_t height = inspection.height;
	output.levels.reserve(inspection.levels);
	for (uint32_t levelIndex = 0; levelIndex < inspection.levels; ++levelIndex)
	{
		const PreparedMipLevel level = computeMipLayout(target, width, height, offset);
		output.levels.push_back(level);
		offset += level.byteSize;
		width = std::max(1u, width / 2);
		height = std::max(1u, height / 2);
	}
	output.bytes.resize(static_cast<size_t>(offset));
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

TextureTranscoder::TextureTranscoder() = default;
TextureTranscoder::~TextureTranscoder() = default;

TextureInspection TextureTranscoder::inspect(CustomTextureSourceKind hintedKind,
		std::vector<uint8_t> sourceBytes)
{
	initializeOnce();
	ddsTranscoder.reset();
	ktx2Transcoder.reset();
	fileBytes = std::move(sourceBytes);
	inspection = {};
	if (fileBytes.empty() || fileBytes.size() > std::numeric_limits<uint32_t>::max())
		throw FlycastException("source file is empty or exceeds the Basis input range");

	if (hintedKind == CustomTextureSourceKind::DdsBc7)
	{
		ddsTranscoder = std::make_unique<basist::dds_transcoder>();
		basist::dds_transcoder& transcoder = *ddsTranscoder;
		if (!transcoder.init(fileBytes.data(), static_cast<uint32_t>(fileBytes.size())))
			throw FlycastException("Basis rejected the DDS container");
		if (transcoder.get_source_kind() != basist::dds_transcoder::source_kind::cBC7
				|| transcoder.get_dds_format() != basist::dds_format::cBC7)
			throw FlycastException("DDS source is not BC7");
		if (transcoder.get_faces() != 1 || transcoder.get_layers() != 0 || transcoder.get_is_cubemap())
			throw FlycastException("DDS arrays and cubemaps are not supported");
		validateCommon(transcoder.get_width(), transcoder.get_height(), transcoder.get_levels());
		inspection = { CustomTextureCodec::DdsBc7, transcoder.get_width(), transcoder.get_height(),
				transcoder.get_levels(), 4, 4, transcoder.get_has_alpha() != 0 };
		return inspection;
	}

	ktx2Transcoder = std::make_unique<basist::ktx2_transcoder>();
	basist::ktx2_transcoder& transcoder = *ktx2Transcoder;
	if (!transcoder.init(fileBytes.data(), static_cast<uint32_t>(fileBytes.size())))
		throw FlycastException("Basis rejected the KTX2 container");
	const auto& header = transcoder.get_header();
	if (header.m_pixel_depth != 0 || transcoder.get_faces() != 1 || transcoder.get_layers() != 0)
		throw FlycastException("only non-array 2D KTX2 textures are supported");
	if (!transcoder.is_ldr())
		throw FlycastException("HDR KTX2 textures are not supported");
	CustomTextureCodec codec;
	if (transcoder.is_xubc7())
		codec = CustomTextureCodec::Xubc7;
	else if (transcoder.is_xuastc_ldr())
		codec = CustomTextureCodec::XuastcLdr;
	else if (transcoder.is_etc1s())
		codec = CustomTextureCodec::Etc1s;
	else
		throw FlycastException("KTX2 is not XUBC7, XUASTC LDR, or ETC1S");
	if ((hintedKind == CustomTextureSourceKind::Ktx2Xubc7 && codec != CustomTextureCodec::Xubc7)
			|| (hintedKind == CustomTextureSourceKind::Ktx2Xuastc && codec != CustomTextureCodec::XuastcLdr)
			|| (hintedKind == CustomTextureSourceKind::Ktx2Etc1s && codec != CustomTextureCodec::Etc1s))
		throw FlycastException("explicit KTX2 suffix does not match the encoded codec");
	if (hasUnsupportedOrientation(transcoder))
		throw FlycastException("KTXorientation must be ru for Flycast replacements");
	validateCommon(transcoder.get_width(), transcoder.get_height(), transcoder.get_levels());
	for (uint32_t levelIndex = 0; levelIndex < transcoder.get_levels(); ++levelIndex)
	{
		basist::ktx2_image_level_info info{};
		if (!transcoder.get_image_level_info(info, levelIndex, 0, 0)
				|| info.m_orig_width != std::max(1u, transcoder.get_width() >> levelIndex)
				|| info.m_orig_height != std::max(1u, transcoder.get_height() >> levelIndex))
			throw FlycastException("KTX2 mip geometry is inconsistent");
	}
	inspection = { codec, transcoder.get_width(), transcoder.get_height(), transcoder.get_levels(),
			transcoder.get_block_width(), transcoder.get_block_height(), transcoder.get_has_alpha() != 0 };
	return inspection;
}

PreparedCustomTexture::Ptr TextureTranscoder::prepare(NativeTextureFormat target,
		uint32_t replacementHash, const CancellationCheck& cancelled)
{
	if (cancelled && cancelled())
		throw LoadCancelledException();
	validateCommon(inspection.width, inspection.height, inspection.levels);
	if (inspection.codec != CustomTextureCodec::Xubc7
			&& inspection.codec != CustomTextureCodec::XuastcLdr
			&& inspection.codec != CustomTextureCodec::Etc1s
			&& inspection.codec != CustomTextureCodec::DdsBc7)
		throw FlycastException("inspection does not describe a supported compressed texture source");
	if (inspection.hasAlpha && (target == NativeTextureFormat::Bc1Unorm
			|| target == NativeTextureFormat::Etc2Rgb8Unorm))
		throw FlycastException("opaque target cannot preserve source alpha");
	const auto basisFormat = basisTarget(target);
	if (basisFormat == basist::transcoder_texture_format::cTFTotalTextureFormats)
		throw FlycastException("invalid native target format");

	auto output = std::make_shared<PreparedCustomTexture>();
	output->replacementHash = replacementHash;
	output->nativeFormat = target;
	output->width = inspection.width;
	output->height = inspection.height;

	if (inspection.codec == CustomTextureCodec::DdsBc7)
	{
		assert(ddsTranscoder);
		basist::dds_transcoder& transcoder = *ddsTranscoder;
		if (!transcoder.is_transcode_format_supported(basisFormat))
			throw FlycastException("DDS target is not supported by Basis");
		allocatePayload(inspection, target, *output);
		if (!transcoder.start_transcoding())
			throw FlycastException("Basis failed to start DDS transcoding");
		for (uint32_t levelIndex = 0; levelIndex < inspection.levels; ++levelIndex)
		{
			if (cancelled && cancelled())
				throw LoadCancelledException();
			const PreparedMipLevel& level = output->levels[levelIndex];
			const uint32_t capacity = outputCapacity(level, target);
			if (capacity == 0 || !transcoder.transcode_image_level(levelIndex, 0, 0,
					output->bytes.data() + level.byteOffset, capacity, basisFormat))
				throw FlycastException("Basis failed to transcode a DDS mip");
		}
	}
	else
	{
		assert(ktx2Transcoder);
		basist::ktx2_transcoder& transcoder = *ktx2Transcoder;
		if (!basist::basis_is_format_supported(basisFormat, transcoder.get_basis_tex_format()))
			throw FlycastException("KTX2 target is not supported by Basis");
		allocatePayload(inspection, target, *output);
		if (!transcoder.start_transcoding())
			throw FlycastException("Basis failed to start KTX2 transcoding");
		basist::ktx2_transcoder_state state;
		state.clear();
		for (uint32_t levelIndex = 0; levelIndex < inspection.levels; ++levelIndex)
		{
			if (cancelled && cancelled())
				throw LoadCancelledException();
			const PreparedMipLevel& level = output->levels[levelIndex];
			const uint32_t capacity = outputCapacity(level, target);
			if (capacity == 0 || !transcoder.transcode_image_level(levelIndex, 0, 0,
					output->bytes.data() + level.byteOffset, capacity, basisFormat, 0, 0, 0, -1, -1, &state))
				throw FlycastException("Basis failed to transcode a KTX2 mip");
		}
	}

	validatePreparedCustomTexture(*output);
	return output;
}
