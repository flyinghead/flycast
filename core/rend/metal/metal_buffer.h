/*
    Copyright 2024 flyinghead

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
#include "types.h"
#include <Metal/Metal.hpp>

struct MetalBufferData
{
    MetalBufferData(u64 size);
    ~MetalBufferData()
    {
        buffer->setPurgeableState(MTL::PurgeableStateEmpty);
        buffer->release();
        buffer = nullptr;
    }

    void upload(u32 size, const void *data, u32 bufOffset = 0) const
    {
        verify(bufOffset + size <= bufferSize);

        void* dataPtr = (u8 *)buffer->contents() + bufOffset;
        memcpy(dataPtr, data, size);
    }

    void upload(size_t count, const u32 *sizes, const void * const *data, u32 bufOffset = 0) const
    {
        u32 totalSize = 0;
        for (size_t i = 0; i < count; ++i)
            totalSize += sizes[i];
        verify(bufOffset + totalSize <= bufferSize);
        void* dataPtr = (u8 *)buffer->contents() + bufOffset;
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

        void* dataPtr = (u8 *)buffer->contents() + bufOffset;
        memcpy(data, dataPtr, size);
    }

    MTL::Buffer *buffer;
    u64 bufferSize;
};

class BufferPacker
{
public:
    BufferPacker();

    u64 addUniform(const void *p, size_t size) {
        return add(p, size);
    }

    u64 addStorage(const void *p, size_t size) {
        return add(p, size);
    }

    u64 add(const void *p, size_t size)
    {
        u32 padding = align(offset, 16);
        if (padding != 0)
        {
            chunks.push_back(nullptr);
            chunkSizes.push_back(padding);
            offset += padding;
        }
        u64 start = offset;
        chunks.push_back(p);
        chunkSizes.push_back(size);
        offset += size;

        return start;
    }

    void upload(MetalBufferData& bufferData, u32 bufOffset = 0)
    {
        if (!chunks.empty())
            bufferData.upload(chunks.size(), &chunkSizes[0], &chunks[0], bufOffset);
    }

    u64 size() const {
        return offset;
    }

private:
    std::vector<const void *> chunks;
    std::vector<u32> chunkSizes;
    u64 offset = 0;

    static inline u32 align(u64 offset, u32 alignment)
    {
        u32 pad = (u32)(offset & (alignment - 1));
        return pad == 0 ? 0 : alignment - pad;
    }
};