// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <Metal/Metal.h>

API_AVAILABLE(macos(26.0))
inline MTLCompileOptions *metalShaderOptions(id<MTLDevice> device, bool allowRelaxedDivisions = true)
{
	MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
	options.languageVersion = MTLLanguageVersion4_0;
	options.mathMode = MTLMathModeSafe;
	options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
	// Only the existing explicitly scoped divisions may use relaxed math.
	// The Metal 4 migration does not relax depth, alpha or UV arithmetic.
	bool relaxed = allowRelaxedDivisions;
	options.preprocessorMacros = @{
		@"FLYCAST_METAL_RELAXED_DIVISIONS": @(relaxed),
		@"FLYCAST_METAL_SHARED_PRECISE_DIVISOR": @(relaxed),
		@"FLYCAST_METAL_RASTER_ORDERED_MODIFIERS": @(device.rasterOrderGroupsSupported)
	};
	return options;
}
