// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal_commands.h"

bool os_VideoRoutingMetalHasClients(id<MTLDevice> device) API_AVAILABLE(macos(26.0));
void os_VideoRoutingPublishMetalTexture(id<MTLTexture> texture, FCMetal4Commands *command) API_AVAILABLE(macos(26.0));
void os_VideoRoutingTermMetal() API_AVAILABLE(macos(26.0));

#ifdef METAL_RENDERER_TESTS
NSDictionary *os_VideoRoutingMetalServerDescriptionForTests() API_AVAILABLE(macos(26.0));
#endif
