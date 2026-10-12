// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_METAL
#include "metal_support.h"
#import <Metal/Metal.h>

bool metal4Available()
{
    static const bool supported = [] {
        @autoreleasepool {
            if (@available(macOS 26.0, *)) {
                id<MTLDevice> device = MTLCreateSystemDefaultDevice();
                return device && [device supportsFamily:MTLGPUFamilyApple7]
                    && [device supportsFamily:MTLGPUFamilyMetal4];
            }
            return false;
        }
    }();
    return supported;
}
#endif
