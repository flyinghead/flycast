// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_custom_texture.h"
#include "metal4_preload_budget.h"
#include "metal4_upload_state.h"

namespace metal_preload {
struct API_AVAILABLE(macos(26.0)) Allocation {
	id<MTLTexture> image;
	bool deferred = false;
};
API_AVAILABLE(macos(26.0))
std::shared_ptr<Budget> deviceBudget(id<MTLDevice> device);
// A budget denial encodes no GPU work. The caller keeps the ordinary
// on-demand custom-texture path available instead of reporting an upload error.
API_AVAILABLE(macos(26.0))
Allocation allocate(id<MTLDevice> device, const PreparedCustomTexture& texture,
	const std::shared_ptr<Budget>& budget);
// Attach inside the texture-update scope, before its command can be detached.
API_AVAILABLE(macos(26.0))
void trackUpload(id<MTLTexture> image,const std::shared_ptr<metal_upload::State>& state);
API_AVAILABLE(macos(26.0))
bool usable(id<MTLTexture> image,FCMetal4Queue *consumer);
}
