// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal_commands.h"

bool os_VideoRoutingMetalHasClients(id<MTLDevice> device);
void os_VideoRoutingPublishMetalTexture(id<MTLTexture> texture, FCMetal4Commands *command);
void os_VideoRoutingTermMetal();

#ifdef METAL_RENDERER_TESTS
NSDictionary *os_VideoRoutingMetalServerDescriptionForTests();
#endif
