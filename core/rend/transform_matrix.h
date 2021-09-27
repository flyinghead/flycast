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
#include "TexCache.h"
#include "hw/pvr/ta_ctx.h"
#include "cfg/option.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

// Dreamcast:
// +Y is down
// Open GL:
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
	TransformMatrix(const rend_context& renderingContext, int width = 0, int height = 0)
	{
		CalcMatrices(&renderingContext, width, height);
	}

	bool IsClipped() const
	{
		return renderingContext->fb_X_CLIP.min != 0
				|| lroundf((renderingContext->fb_X_CLIP.max + 1) / scale_x) != 640L
				|| renderingContext->fb_Y_CLIP.min != 0
				|| lroundf((renderingContext->fb_Y_CLIP.max + 1) / scale_y) != 480L;
	}

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

	glm::vec2 GetDreamcastViewport() const {
		return dcViewport;
	}

	void CalcMatrices(const rend_context *renderingContext, int width = 0, int height = 0)
	{
		constexpr int screenFlipY = System == COORD_OPENGL || System == COORD_DIRECTX ? -1 : 1;
		constexpr int rttFlipY = System == COORD_DIRECTX ? -1 : 1;
		constexpr int framebufferFlipY = System == COORD_DIRECTX ? -1 : 1;

		renderViewport = { width == 0 ? settings.display.width : width, height == 0 ? settings.display.height : height };
		this->renderingContext = renderingContext;

		GetFramebufferScaling(false, scale_x, scale_y);

		if (renderingContext->isRTT)
		{
			dcViewport.x = renderingContext->fb_X_CLIP.max - renderingContext->fb_X_CLIP.min + 1;
			dcViewport.y = renderingContext->fb_Y_CLIP.max - renderingContext->fb_Y_CLIP.min + 1;
			normalMatrix = glm::translate(glm::vec3(-1, -rttFlipY, 0))
				* glm::scale(glm::vec3(2.0f / dcViewport.x, 2.0f / dcViewport.y * rttFlipY, 1.f));
			scissorMatrix = normalMatrix;
			sidebarWidth = 0;
		}
		else
		{
			dcViewport.x = 640.f * scale_x;
			dcViewport.y = 480.f * scale_y;

			float startx = 0;
			float starty = 0;
#if 0
			const bool vga = FB_R_CTRL.vclk_div == 1;
			switch (SPG_LOAD.hcount)
			{
				case 857: // NTSC, VGA
					startx = VO_STARTX.HStart - (vga ? 0xa8 : 0xa4);
					break;
				case 863: // PAL
					startx = VO_STARTX.HStart - 0xae;
					break;
				default:
					INFO_LOG(PVR, "unknown video mode: hcount %d", SPG_LOAD.hcount);
					break;
			}
			switch (SPG_LOAD.vcount)
			{
				case 524: // NTSC, VGA
					starty = VO_STARTY.VStart_field1 - (vga ? 0x28 : 0x12);
					break;
				case 262: // NTSC 240p
					starty = VO_STARTY.VStart_field1 - 0x11;
					break;
				case 624: // PAL
					starty = VO_STARTY.VStart_field1 - 0x2d;
					break;
				case 312: // PAL 240p
					starty = VO_STARTY.VStart_field1 - 0x2e;
					break;
				default:
					INFO_LOG(PVR, "unknown video mode: vcount %d", SPG_LOAD.vcount);
					break;
			}
			// some heuristic...
			startx *= 0.8;
			starty *= 1.1;
			// TODO need to adjust lightgun coordinates
#endif
			normalMatrix = glm::translate(glm::vec3(startx, starty, 0));
			scissorMatrix = normalMatrix;

			const float screen_stretching = config::ScreenStretching / 100.f;
			float scissoring_scale_x, scissoring_scale_y;
			GetFramebufferScaling(true, scissoring_scale_x, scissoring_scale_y);

			float x_coef;
			float y_coef;
			glm::mat4 trans_rot;

			if (config::Rotate90)
			{
				float dc2s_scale_h = renderViewport.x / 640.0f;

				sidebarWidth = 0;
				y_coef = 2.0f / (renderViewport.y / dc2s_scale_h * scale_y) * screen_stretching * screenFlipY;
				x_coef = 2.0f / dcViewport.x;
			}
			else
			{
				float dc2s_scale_h = renderViewport.y / 480.0f;

				sidebarWidth =  (renderViewport.x - dc2s_scale_h * 640.0f * screen_stretching) / 2;
				x_coef = 2.0f / (renderViewport.x / dc2s_scale_h * scale_x) * screen_stretching;
				y_coef = 2.0f / dcViewport.y * screenFlipY;
			}
			trans_rot = glm::translate(glm::vec3(-1 + 2 * sidebarWidth / renderViewport.x, -screenFlipY, 0));

			normalMatrix = trans_rot
				* glm::scale(glm::vec3(x_coef, y_coef, 1.f))
				* normalMatrix;
			scissorMatrix = trans_rot
				* glm::scale(glm::vec3(x_coef * scissoring_scale_x, y_coef * scissoring_scale_y, 1.f))
				* scissorMatrix;
		}
		normalMatrix = glm::scale(glm::vec3(1, 1, 1 / config::ExtraDepthScale))
				* normalMatrix;

		glm::mat4 vp_trans = glm::translate(glm::vec3(1, framebufferFlipY, 0));
		if (renderingContext->isRTT)
		{
			vp_trans = glm::scale(glm::vec3(dcViewport.x / 2, dcViewport.y / 2 * framebufferFlipY, 1.f))
				* vp_trans;
		}
		else
		{
			vp_trans = glm::scale(glm::vec3(renderViewport.x / 2, renderViewport.y / 2 * framebufferFlipY, 1.f))
				* vp_trans;
		}
		viewportMatrix = vp_trans * normalMatrix;
		scissorMatrix = vp_trans * scissorMatrix;
	}

private:
	void GetFramebufferScaling(bool scissor, float& scale_x, float& scale_y)
	{
		scale_x = 1.f;
		scale_y = 1.f;

		if (!renderingContext->isRTT && !renderingContext->isRenderFramebuffer)
		{
			if (!scissor)
			{
				scale_x = fb_scale_x;
				scale_y = fb_scale_y;
			}
			if (SCALER_CTL.vscalefactor > 0x400)
			{
				// Interlace mode A (single framebuffer)
				if (SCALER_CTL.interlace == 0)
					scale_y *= roundf((float)SCALER_CTL.vscalefactor / 0x400);
				else if (SCALER_CTL.interlace == 1 && scissor)
					// Interlace mode B (alternating framebuffers)
					scale_y *= roundf((float)SCALER_CTL.vscalefactor / 0x400);
			}

			// VO pixel doubling is done after fb rendering/clipping
			// so it should be used for scissoring as well
			if (VO_CONTROL.pixel_double && !scissor)
				scale_x *= 0.5f;

			// the X Scaler halves the horizontal resolution but
			// before clipping/scissoring
			if (SCALER_CTL.hscale)
				scale_x *= 2.f;
		}
	}

	const rend_context *renderingContext = nullptr;

	glm::mat4 normalMatrix;
	glm::mat4 scissorMatrix;
	glm::mat4 viewportMatrix;
	glm::vec2 dcViewport;
	glm::vec2 renderViewport;
	float scale_x = 0;
	float scale_y = 0;
	float sidebarWidth = 0;
};
