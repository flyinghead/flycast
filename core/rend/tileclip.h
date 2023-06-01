/*
	Copyright 2020 flyinghead

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
#include <glm/glm.hpp>

#include "types.h"
#include "hw/pvr/Renderer_if.h"
#include "cfg/option.h"

enum class TileClipping {
	Inside,			// Render stuff outside the region
	Off,    		// Always passes
	Outside    		// Render stuff inside the region
};

// clip_rect[] will contain x, y, width, height
static inline TileClipping GetTileClip(u32 val, const glm::mat4& viewport, int *clip_rect)
{
	if (!config::Clipping)
		return TileClipping::Off;

	u32 clipmode = val >> 28;
	if (clipmode < 2)
		return TileClipping::Off;	//always passes

	TileClipping tileClippingMode;
	if (clipmode & 1)
		tileClippingMode = TileClipping::Inside;   //render stuff outside the region
	else
		tileClippingMode = TileClipping::Outside;  //render stuff inside the region

	float csx = (float)(val & 63);
	float cex = (float)((val >> 6) & 63);
	float csy = (float)((val >> 12) & 31);
	float cey = (float)((val >> 17) & 31);
	csx = csx * 32;
	cex = (cex + 1) * 32;
	csy = csy * 32;
	cey = (cey + 1) * 32;

	if (csx <= 0 && csy <= 0 && cex >= 640 && cey >= 480 && tileClippingMode == TileClipping::Outside)
		return TileClipping::Off;

	if (!pvrrc.isRTT)
	{
		glm::vec4 clip_start(csx, csy, 0, 1);
		glm::vec4 clip_end(cex, cey, 0, 1);
		clip_start = viewport * clip_start;
		clip_end = viewport * clip_end;

		csx = clip_start[0];
		csy = clip_start[1];
		cey = clip_end[1];
		cex = clip_end[0];
	}
	else if (!config::RenderToTextureBuffer)
	{
		float scale = config::RenderResolution / 480.f;
		csx *= scale;
		csy *= scale;
		cex *= scale;
		cey *= scale;
	}
	clip_rect[0] = std::max(0, (int)lroundf(csx));
	clip_rect[1] = std::max(0, (int)lroundf(std::min(csy, cey)));
	clip_rect[2] = std::max(0, (int)lroundf(cex - csx));
	clip_rect[3] = std::max(0, (int)lroundf(std::abs(cey - csy)));

	return tileClippingMode;
}
