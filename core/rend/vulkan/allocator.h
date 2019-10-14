/*
 *  Created on: Oct 11, 2019

	Copyright 2019 flyinghead

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
#include <list>
#include <unordered_map>
#include "vulkan.h"
#include "utils.h"

class VulkanAllocator;

// Manages a large chunk of memory using buddy memory allocation algorithm
class Chunk
{
public:
	Chunk(vk::DeviceSize size)
	{
		verify(size >= SmallestBlockSize);
		freeBlocks.push_back(std::make_pair(0, PowerOf2(size)));
		INFO_LOG(RENDERER, "Allocated memory chunk of size %zd", PowerOf2(size));
	}
	Chunk(const Chunk& other) = delete;
	Chunk(Chunk&& other) = default;
	Chunk& operator=(const Chunk& other) = delete;
	Chunk& operator=(Chunk&& other) = default;

	~Chunk()
	{
		verify(usedBlocks.empty());
		verify(freeBlocks.size() <= 1);
		if (!freeBlocks.empty())
			INFO_LOG(RENDERER, "Freeing memory chunk of size %zd", freeBlocks.front().second);
	}

private:
	vk::DeviceSize PowerOf2(vk::DeviceSize size)
	{
		vk::DeviceSize alignedSize = SmallestBlockSize;
		while (alignedSize < size)
			alignedSize *= 2;
		return alignedSize;
	}

	vk::DeviceSize Allocate(vk::DeviceSize size, vk::DeviceSize alignment)
	{
		alignment--;
		const vk::DeviceSize alignedSize = PowerOf2(size);

		auto smallestFreeBlock = freeBlocks.end();
		for (auto it = freeBlocks.begin(); it != freeBlocks.end(); it++)
		{
			if (it->second == alignedSize && (it->first & alignment) == 0)
			{
				// Free block of the right size and alignment found -> return it
				usedBlocks.insert(*it);
				vk::DeviceSize offset = it->first;
				freeBlocks.erase(it);

				return offset;
			}
			if (it->second > alignedSize &&  (it->first & alignment) == 0
					&& (smallestFreeBlock == freeBlocks.end() || smallestFreeBlock->second > it->second))
				smallestFreeBlock = it;
		}
		if (smallestFreeBlock == freeBlocks.end())
			return OutOfMemory;

		// We need to split larger blocks until we have one of the required size
		vk::DeviceSize offset = smallestFreeBlock->first;
		smallestFreeBlock->second /= 2;
		smallestFreeBlock->first += smallestFreeBlock->second;
		while (smallestFreeBlock->second > alignedSize)
		{
			freeBlocks.push_front(std::make_pair(offset + smallestFreeBlock->second / 2, smallestFreeBlock->second / 2));
			smallestFreeBlock = freeBlocks.begin();
		}
		usedBlocks[offset] = alignedSize;

		return offset;
	}

	void Free(vk::DeviceSize offset)
	{
		auto usedBlock = usedBlocks.find(offset);
		verify(usedBlock != usedBlocks.end());
		vk::DeviceSize buddyOffset = offset ^ usedBlock->second;
		vk::DeviceSize buddySize = usedBlock->second;
		auto buddy = freeBlocks.end();
		while (true)
		{
			auto it = freeBlocks.begin();
			for (; it != freeBlocks.end(); it++)
			{
				if (it->first == buddyOffset && it->second == buddySize)
				{
					it->second *= 2;
					it->first &= ~(it->second - 1);
					if (buddy != freeBlocks.end())
						freeBlocks.erase(buddy);
					buddy = it;
					buddyOffset = it->first ^ buddy->second;
					buddySize = it->second;
					break;
				}
			}
			if (buddy == freeBlocks.end())
			{
				// Initial order buddy not found -> add block to free list
				freeBlocks.push_front(std::make_pair(offset, usedBlock->second));
				break;
			}
			if (it == freeBlocks.end())
				break;
		}
		usedBlocks.erase(usedBlock);
	}

	// first object/key is offset, second one/value is size
	std::list<std::pair<vk::DeviceSize, vk::DeviceSize>> freeBlocks;
	std::unordered_map<vk::DeviceSize, vk::DeviceSize> usedBlocks;
	vk::UniqueDeviceMemory deviceMemory;

	static const vk::DeviceSize OutOfMemory = (vk::DeviceSize)-1;
	static const vk::DeviceSize SmallestBlockSize = 256;

	friend class VulkanAllocator;
};

class Allocator
{
public:
	virtual ~Allocator() {}
	virtual vk::DeviceSize Allocate(vk::DeviceSize size, vk::DeviceSize alignment, u32 memoryType, vk::DeviceMemory& deviceMemory) = 0;
	virtual void Free(vk::DeviceSize offset, u32 memoryType, vk::DeviceMemory deviceMemory) = 0;
};

class VulkanAllocator : public Allocator
{
public:
	vk::DeviceSize Allocate(vk::DeviceSize size, vk::DeviceSize alignment, u32 memoryType, vk::DeviceMemory& deviceMemory) override
	{
		std::vector<Chunk>& chunks = findChunks(memoryType);
		for (auto& chunk : chunks)
		{
			vk::DeviceSize offset = chunk.Allocate(size, alignment);
			if (offset != Chunk::OutOfMemory)
			{
				deviceMemory = *chunk.deviceMemory;
				return offset;
			}
		}
		const vk::DeviceSize newChunkSize = std::max(chunkSize, size);
		chunks.emplace_back(newChunkSize);
		Chunk& chunk = chunks.back();
		chunk.deviceMemory = VulkanContext::Instance()->GetDevice()->allocateMemoryUnique(vk::MemoryAllocateInfo(newChunkSize, memoryType));
		vk::DeviceSize offset = chunk.Allocate(size, alignment);
		verify(offset != Chunk::OutOfMemory);

		deviceMemory = *chunk.deviceMemory;
		return offset;
	}

	void Free(vk::DeviceSize offset, u32 memoryType, vk::DeviceMemory deviceMemory) override
	{
		std::vector<Chunk>& chunks = findChunks(memoryType);
		for (auto chunkIt = chunks.begin(); chunkIt < chunks.end(); chunkIt++)
		{
			if (*chunkIt->deviceMemory == deviceMemory)
			{
				chunkIt->Free(offset);
				if (chunks.size() > 1 && chunkIt->usedBlocks.empty())
				{
					chunks.erase(chunkIt);
				}
				return;
			}
		}
		die("Invalid free");
	}

	void SetChunkSize(vk::DeviceSize chunkSize) {
		this->chunkSize = chunkSize;
	}

private:
	std::vector<Chunk>& findChunks(u32 memoryType)
	{
		for (auto& pair : chunksPerMemType)
			if (pair.first == memoryType)
				return pair.second;
		chunksPerMemType.push_back(std::make_pair(memoryType, std::vector<Chunk>()));
		return chunksPerMemType.back().second;
	}

	vk::DeviceSize chunkSize;
	std::vector<std::pair<u32, std::vector<Chunk>>> chunksPerMemType;
};

class SimpleAllocator : public Allocator
{
public:
	vk::DeviceSize Allocate(vk::DeviceSize size, vk::DeviceSize alignment, u32 memoryType, vk::DeviceMemory& deviceMemory) override
	{
		deviceMemory = VulkanContext::Instance()->GetDevice()->allocateMemory(vk::MemoryAllocateInfo(size, memoryType));

		return 0;
	}

	void Free(vk::DeviceSize offset, u32 memoryType, vk::DeviceMemory deviceMemory) override
	{
		VulkanContext::Instance()->GetDevice()->free(deviceMemory);
	}

	static SimpleAllocator instance;
};
