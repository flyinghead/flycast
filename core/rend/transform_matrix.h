/*
    Created on: Oct 22, 2019

	Copyright 2019 flyinghead

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
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/ta_ctx.h"
#include "cfg/option.h"
#include <glm/glm.hpp>

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

// Dreamcast:
// +Y is down
// OpenGL:
// +Y is up in clip, NDC and framebuffer coordinates. BUT we invert the Y coordinate in the renderer.
// Vulkan:
// +Y is down in clip, NDC and framebuffer coordinates
// DirectX9:
// +Y is up in clip and NDC coordinates, but down in framebuffer coordinates
// Y must also be flipped for render-to-texture so that the top of the texture comes first
class TransformMatrix
{
public:
	TransformMatrix(bool directx = false)
		: directx(directx)
	{}
	TransformMatrix(const rend_context& renderingContext, int width, int height, bool directx = false)
		: directx(directx)
	{
		CalcMatrices(&renderingContext, width, height);
	}

	const glm::mat4& GetNormalMatrix() const {
		return normalMatrix;
	}
	const glm::mat4& GetViewportMatrix() const {
		return viewportMatrix;
	}

	void CalcMatrices(const rend_context *renderingContext, int width = 0, int height = 0);
	Rect getBaseScissor() const;
	TileClipping getTileClip(u32 val, Rect& clipRect);

private:
	bool isClipped() const;
	void getScissorScaling(float& scale_x, float& scale_y) const;
	Rect intersectTileAndFBScissor() const;

	const rend_context *renderingContext = nullptr;
	const bool directx;

	glm::mat4 normalMatrix;
	glm::mat4 viewportMatrix;
};

void getPvrFramebufferSize(const rend_context& rendCtx, int& width, int& height);
void getScaledFramebufferSize(const rend_context& rendCtx, int& width, int& height);
float getOutputFramebufferAspectRatio();
void getDCFramebufferReadSize(const FramebufferInfo& info, int& width, int& height);
void getWriteFBToVramParams(const rend_context& ctx, glm::ivec2& scaledSize, Rect& finalClip);

inline static float getDCFramebufferAspectRatio() {
	float aspectRatio = config::Rotate90 ? 3.f / 4.f : 4.f / 3.f;
	return aspectRatio * config::ScreenStretching / 100.f;
}

void getWindowboxDimensions(int outwidth, int outheight, float renderAR, int& dx, int& dy, bool rotate);
void getVideoShift(float& x, float& y);
