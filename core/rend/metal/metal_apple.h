/*
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

// Apple's MacTypes.h and QuickDraw headers declare global Rect and RGBColor types,
// which clash with flycast's own Rect (hw/pvr/ta_ctx.h) and RGBColor (hw/pvr/pvr_regs.h).
// Import the Apple frameworks with these names renamed. This header must be included
// before any other Apple framework header.
#define Rect AppleRect
#define RGBColor AppleRGBColor
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#undef Rect
#undef RGBColor
