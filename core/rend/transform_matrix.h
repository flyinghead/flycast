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

// Dreamcast:
// +Y is down
// OpenGL:
// +Y is up in clip, NDC and framebuffer coordinates
// Vulkan:
// +Y is down in clip, NDC and framebuffer coordinates
// DirectX9:
// +Y is up in clip and NDC coordinates, but down in framebuffer coordinates
// Y must also be flipped for render-to-texture so that the top of the texture comes first
enum CoordSystem { COORD_OPENGL, COORD_VULKAN, COORD_DIRECTX };
template<CoordSystem System>
class TransformMatrix
{
public:
	TransformMatrix() = default;
	TransformMatrix(const rend_context& renderingContext, int width, int height)
	{
		CalcMatrices(&renderingContext, width, height);
	}

	bool IsClipped() const;

	const glm::mat4& GetNormalMatrix() const {
		return normalMatrix;
	}
	const glm::mat4& GetScissorMatrix() const {
		return scissorMatrix;
	}
	const glm::mat4& GetViewportMatrix() const {
		return viewportMatrix;
	}

	// Return the width of the black bars when the screen is wider than 4:3. Returns a negative number when the screen is taller than 4:3,
	// whose inverse is the height of the top and bottom bars.
	float GetSidebarWidth() const {
		return sidebarWidth;
	}

	void CalcMatrices(const rend_context *renderingContext, int width = 0, int height = 0);

private:
	void GetScissorScaling(float& scale_x, float& scale_y) const;

	const rend_context *renderingContext = nullptr;

	glm::mat4 normalMatrix;
	glm::mat4 scissorMatrix;
	glm::mat4 viewportMatrix;
	glm::vec2 dcViewport;
	glm::vec2 renderViewport;
	float sidebarWidth = 0;
};
template class TransformMatrix<COORD_OPENGL>;
template class TransformMatrix<COORD_VULKAN>;
template class TransformMatrix<COORD_DIRECTX>;

void getScaledFramebufferSize(const rend_context& rendCtx, int& width, int& height);
float getOutputFramebufferAspectRatio();
void getDCFramebufferReadSize(const FramebufferInfo& info, int& width, int& height);

inline static float getDCFramebufferAspectRatio()
{
	float aspectRatio = config::Rotate90 ? 3.f / 4.f : 4.f / 3.f;
	return aspectRatio * config::ScreenStretching / 100.f;
}

void getWindowboxDimensions(int outwidth, int outheight, float renderAR, int& dx, int& dy, bool rotate);
void getVideoShift(float& x, float& y);
