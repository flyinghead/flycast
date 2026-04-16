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
#include "hw/pvr/ta_ctx.h"
#include "cfg/option.h"

static inline Rect intersect(const Rect& l, const Rect& r)
{
	Rect rect;
	rect.origin = glm::max(l.origin, r.origin);
	rect.size = glm::max(glm::min(l.bottomRight(), r.bottomRight()) - rect.origin + glm::ivec2(1, 1), glm::ivec2(0, 0));
	return rect;
}

enum class TileClipping {
	Inside,			// Render stuff outside the region
	Off,    		// Always passes
	Outside    		// Render stuff inside the region
};

// clip_rect[] will contain x, y, width, height
static inline TileClipping getTileClip(u32 val, const glm::mat4& viewport, Rect& clipRect, const rend_context& ctx)
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

	if (csx == 0 && csy == 0 && cex >= ctx.globClip.x && cey >= ctx.globClip.y)
		return TileClipping::Off;

	if (!ctx.isRTT)
	{
		if (tileClippingMode == TileClipping::Outside && !config::EmulateFramebuffer)
		{
			// Intersect with tile/fb clipping
			csx = std::max<float>(csx, ctx.fbClip.origin.x);
			csy = std::max<float>(csy, ctx.fbClip.origin.y);
			cex = std::min<float>(cex, ctx.fbClip.origin.x + ctx.fbClip.size.x);
			cey = std::min<float>(cey, ctx.fbClip.origin.y + ctx.fbClip.size.y);
		}
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
	clipRect = {
		glm::ivec2(std::max(0, (int)std::round(csx)), std::max(0, (int)std::round(std::min(csy, cey)))),
		glm::ivec2(std::max(0, (int)std::round(cex - csx)), std::max(0, (int)std::round(std::abs(cey - csy))))
	};
	return tileClippingMode;
}
