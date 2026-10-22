// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(USE_METAL) || defined(FLYCAST_METAL_STANDALONE)
#include "metal4_preload.h"
#import <objc/runtime.h>
#include <algorithm>
#include <stdexcept>

// The image itself owns the reservation. Clearing a renderer map, a base-class
// owner or an adopted MetalTexture cannot release bytes while GPU commands
// still retain that same MTLTexture. This is also independent of backend reset.
@interface FCMetalPreloadBudgetOwner : NSObject {
@public
	std::shared_ptr<metal_preload::Budget> budget;
}
@end
@implementation FCMetalPreloadBudgetOwner @end
@interface FCMetalPreloadTicketOwner : NSObject {
@public
	std::shared_ptr<metal_preload::Budget::Ticket> ticket;
	std::shared_ptr<metal_upload::State> upload;
}
@end
@implementation FCMetalPreloadTicketOwner @end

namespace metal_preload {
static char DeviceBudgetKey, ImageTicketKey;
std::shared_ptr<Budget> deviceBudget(id<MTLDevice> device)
{
	if (!device) return {};
	@synchronized(device) {
		FCMetalPreloadBudgetOwner *owner = objc_getAssociatedObject(device,&DeviceBudgetKey);
		if (!owner) {
			owner = [FCMetalPreloadBudgetOwner new];
			if (!owner) throw std::bad_alloc();
			const uint64_t recommended = device.recommendedMaxWorkingSetSize;
			const uint64_t limit = recommended ? std::min<uint64_t>(512ull<<20,recommended/8) : 128ull<<20;
			owner->budget = std::make_shared<Budget>(limit,4096);
			objc_setAssociatedObject(device,&DeviceBudgetKey,owner,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
		}
		return owner->budget;
	}
}
Allocation allocate(id<MTLDevice> device, const TextureData& texture,
	const std::shared_ptr<Budget>& budget)
{
	metal_custom::validate(texture);
	if (!device || !budget) return {};
	auto descriptor = metal_custom::descriptor(texture,true);
	const uint64_t estimate = [device heapTextureSizeAndAlignWithDescriptor:descriptor].size;
	if (!estimate) return {};
	// Reserve before asking the driver to allocate, including the full mip chain.
	auto ticket = budget->reserve(estimate);
	if (!ticket) return {nil,true};
	id<MTLTexture> image = [device newTextureWithDescriptor:descriptor];
	if (!image) return {};
	// A standalone allocation may be padded differently from the heap estimate.
	// Never admit it at an underestimated byte charge. A rejected allocation
	// has not been submitted to the GPU and is released on return.
	if (!ticket->resize(image.allocatedSize)) return {nil,true};
	FCMetalPreloadTicketOwner *owner = [FCMetalPreloadTicketOwner new];
	if (!owner) return {};
	owner->ticket = std::move(ticket);
	objc_setAssociatedObject(image,&ImageTicketKey,owner,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	return {image,false};
}
void trackUpload(id<MTLTexture> image,const std::shared_ptr<metal_upload::State>& state)
{
	FCMetalPreloadTicketOwner *owner=objc_getAssociatedObject(image,&ImageTicketKey);
	if(!owner || owner->upload || !state || !state->producerQueue() || state->phase()!=metal_upload::Phase::Pending)
		throw std::invalid_argument("Invalid Metal preload upload state");
	owner->upload=state;
}
bool usable(id<MTLTexture> image,FCMetal4Queue *consumer)
{
	if(!image || !consumer || image.device!=consumer.device)return false;
	FCMetalPreloadTicketOwner *owner=objc_getAssociatedObject(image,&ImageTicketKey);
	return owner && owner->upload && owner->upload->usableOn(reinterpret_cast<uintptr_t>((__bridge void *)consumer));
}
}
#endif
