/*
	Copyright 2025 flyinghead

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
#include "metal_apple.h"
#include "types.h"
#include <limits>
#include <vector>
#include <TargetConditionals.h>

// Minimum alignment for an offset bound to a `constant` buffer argument. Mac-family
// GPUs require 256-byte alignment; Apple-family GPUs only need 16. Binding a constant
// buffer at a misaligned offset is a Metal validation error and silently reads the
// wrong uniforms without validation. macOS (including Apple silicon) uses the Mac rule.
#if TARGET_OS_OSX || TARGET_OS_SIMULATOR
static constexpr u32 kConstantBufferAlign = 256;
#else
static constexpr u32 kConstantBufferAlign = 16;
#endif

struct MetalBufferData
{
	MetalBufferData(u64 size);
	~MetalBufferData()
	{
		buffer = nil;
	}

	void upload(u32 size, const void *data, u32 bufOffset = 0) const
	{
		verify(bufOffset + size <= bufferSize);

		void* dataPtr = (u8 *)[buffer contents] + bufOffset;
		memcpy(dataPtr, data, size);
	}

	void upload(size_t count, const u32 *sizes, const void * const *data, u32 bufOffset = 0) const
	{
		u32 totalSize = 0;
		for (size_t i = 0; i < count; ++i)
			totalSize += sizes[i];
		verify(bufOffset + totalSize <= bufferSize);
		void* dataPtr = (u8 *)[buffer contents] + bufOffset;
		for (size_t i = 0; i < count; ++i)
		{
			if (data[i] != nullptr)
				memcpy(dataPtr, data[i], sizes[i]);
			dataPtr = (u8 *)dataPtr + sizes[i];
		}
	}

	void download(u32 size, void *data, u32 bufOffset = 0) const
	{
		verify(bufOffset + size <= bufferSize);

		void* dataPtr = (u8 *)[buffer contents] + bufOffset;
		memcpy(data, dataPtr, size);
	}

	id<MTLBuffer> buffer;
	u64 bufferSize;
};

class MetalBufferPacker
{
public:
	MetalBufferPacker();

	u64 addUniform(const void *p, size_t size) {
		// Constant buffers need 256-byte offset alignment on the Simulator.
		return add(p, size, kConstantBufferAlign);
	}

	u64 addStorage(const void *p, size_t size) {
		return add(p, size, kConstantBufferAlign);
	}

	u64 add(const void *p, size_t size, u32 alignment = 16)
	{
		u32 padding = align(offset, alignment);
		if (padding != 0)
		{
			chunks.push_back(nullptr);
			chunkSizes.push_back(padding);
			offset += padding;
		}
		u64 start = offset;
		chunks.push_back(p);
		verify(size <= (size_t)std::numeric_limits<u32>::max());
		chunkSizes.push_back((u32)size);
		offset += size;

		return start;
	}

	void upload(MetalBufferData& bufferData, u32 bufOffset = 0)
	{
		if (!chunks.empty())
			bufferData.upload(chunks.size(), &chunkSizes[0], &chunks[0], bufOffset);
	}

	void uploadToBuffer(id<MTLBuffer> buffer, u32 bufOffset = 0)
	{
		if (chunks.empty())
			return;
		u8 *dst = (u8 *)[buffer contents] + bufOffset;
		for (size_t i = 0; i < chunks.size(); ++i)
		{
			const void *src = chunks[i];
			if (src != nullptr)
				memcpy(dst, src, chunkSizes[i]);
			dst += chunkSizes[i];
		}
	}

	u64 size() const {
		return offset;
	}

	static inline u32 align(u64 offset, u32 alignment)
	{
		u32 pad = (u32)(offset & (alignment - 1));
		return pad == 0 ? 0 : alignment - pad;
	}

private:
	std::vector<const void *> chunks;
	std::vector<u32> chunkSizes;
	u64 offset = 0;
};

// Simple ring buffer backed by a single Metal buffer split into N slices.
// Each frame, advance to the next slice to avoid CPU/GPU hazards.
class MetalRingBuffer
{
public:
	MetalRingBuffer(u64 sliceSize, u32 sliceCount);
	~MetalRingBuffer() = default;

	void advance()
	{
		index = (index + 1) % count;
	}

	u64 baseOffset() const { return sliceSize * index; }
	u64 getSliceSize() const { return sliceSize; }
	id<MTLBuffer> getBuffer() const { return buffer; }

private:
	id<MTLBuffer> buffer = nil;
	u64 sliceSize = 0;
	u32 count = 0;
	u32 index = 0;
};
