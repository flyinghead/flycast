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

#include "metal_buffer.h"
#include "metal_context.h"

MetalBufferData::MetalBufferData(u64 size) : bufferSize(size) {
	auto device = MetalContext::Instance()->GetDevice();
	MTLResourceOptions opts = MTLResourceStorageModeShared;
#if TARGET_OS_IPHONE || TARGET_OS_TV
	// Prefer write-combined for CPU->GPU streams
	opts |= MTLResourceCPUCacheModeWriteCombined;
#endif
	buffer = [device newBufferWithLength:size options:opts];
	[buffer setLabel:@"Flycast Shared Ring Buffer"];
}

MetalBufferPacker::MetalBufferPacker() {}

MetalRingBuffer::MetalRingBuffer(u64 sliceSize, u32 sliceCount)
{
	// Round each slice up to the constant-buffer alignment so baseOffset()
	// (= sliceSize * index) is always aligned — otherwise constant buffers bound
	// at a slice boundary fail Metal validation on the Simulator (256-byte rule).
	this->sliceSize = sliceSize + MetalBufferPacker::align(sliceSize, kConstantBufferAlign);
	this->count = sliceCount;
	auto device = MetalContext::Instance()->GetDevice();
	MTLResourceOptions opts = MTLResourceStorageModeShared;
#if TARGET_OS_IPHONE || TARGET_OS_TV
	opts |= MTLResourceCPUCacheModeWriteCombined;
#endif
	buffer = [device newBufferWithLength:(sliceSize * sliceCount) options:opts];
	[buffer setLabel:@"Flycast Ring Buffer"];
}
