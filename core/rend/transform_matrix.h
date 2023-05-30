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

inline static void getTAViewport(const rend_context& rendCtx, int& width, int& height)
{
	width = (rendCtx.ta_GLOB_TILE_CLIP.tile_x_num + 1) * 32;
	height = (rendCtx.ta_GLOB_TILE_CLIP.tile_y_num + 1) * 32;
}

inline static void getPvrFramebufferSize(const rend_context& rendCtx, int& width, int& height)
{
	getTAViewport(rendCtx, width, height);
	if (!config::EmulateFramebuffer)
	{
		int maxHeight = FB_R_CTRL.vclk_div == 0 && SPG_CONTROL.interlace == 0 ? 240 : 480;
		if (rendCtx.scaler_ctl.vscalefactor != 0
				&& (rendCtx.scaler_ctl.vscalefactor > 1025 || rendCtx.scaler_ctl.vscalefactor < 1024)
				&& SPG_CONTROL.interlace == 0)
			maxHeight /= 1024.f / rendCtx.scaler_ctl.vscalefactor;
		if (FB_R_CTRL.fb_line_double)
			maxHeight /= 2;
		height = std::min(maxHeight, height);
		// TODO Use FB_R_SIZE too?
	}
}

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

	bool IsClipped() const
	{
		int width, height;
		getTAViewport(*renderingContext, width, height);
		float sx, sy;
		GetScissorScaling(sx,  sy);
		return renderingContext->fb_X_CLIP.min != 0
				|| lroundf((renderingContext->fb_X_CLIP.max + 1) / sx) != width;
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
		const int screenFlipY = (System == COORD_OPENGL  && !config::EmulateFramebuffer) || System == COORD_DIRECTX ? -1 : 1;
		constexpr int rttFlipY = System == COORD_DIRECTX ? -1 : 1;
		constexpr int framebufferFlipY = System == COORD_DIRECTX ? -1 : 1;

		renderViewport = { width == 0 ? settings.display.width : width, height == 0 ? settings.display.height : height };
		this->renderingContext = renderingContext;

		if (renderingContext->isRTT)
		{
			dcViewport.x = (float)(renderingContext->fb_X_CLIP.max - renderingContext->fb_X_CLIP.min + 1);
			if (renderingContext->scaler_ctl.hscale)
				dcViewport.x *= 2;
			dcViewport.y = (float)(renderingContext->fb_Y_CLIP.max - renderingContext->fb_Y_CLIP.min + 1);
			normalMatrix = glm::translate(glm::vec3(-1, -rttFlipY, 0))
				* glm::scale(glm::vec3(2.0f / dcViewport.x, 2.0f / dcViewport.y * rttFlipY, 1.f));
			scissorMatrix = normalMatrix;
			sidebarWidth = 0;
		}
		else
		{
			int w, h;
			getPvrFramebufferSize(*renderingContext, w, h);
			dcViewport.x = w;
			dcViewport.y = h;

			float scissoring_scale_x, scissoring_scale_y;
			GetScissorScaling(scissoring_scale_x, scissoring_scale_y);

			if (config::Widescreen && !config::Rotate90 && !config::EmulateFramebuffer)
			{
				sidebarWidth = (1 - dcViewport.x / dcViewport.y * renderViewport.y / renderViewport.x) / 2;
				if (config::SuperWidescreen)
					dcViewport.x *= (float)settings.display.width / settings.display.height / 4.f * 3.f;
				else
					dcViewport.x *= 4.f / 3.f;
			}
			else
				sidebarWidth = 0;
			float x_coef = 2.0f / dcViewport.x;
			float y_coef = 2.0f / dcViewport.y * screenFlipY;

			glm::mat4 trans = glm::translate(glm::vec3(-1 + 2 * sidebarWidth, -screenFlipY, 0));

			normalMatrix = trans
				* glm::scale(glm::vec3(x_coef, y_coef, 1.f));
			scissorMatrix = trans
				* glm::scale(glm::vec3(x_coef * scissoring_scale_x, y_coef * scissoring_scale_y, 1.f));
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
	void GetScissorScaling(float& scale_x, float& scale_y) const
	{
		scale_x = 1.f;
		scale_y = 1.f;

		if (!renderingContext->isRTT && !config::EmulateFramebuffer)
		{
			if (renderingContext->scaler_ctl.vscalefactor > 0x400)
				scale_y *= std::round(renderingContext->scaler_ctl.vscalefactor / 1024.f);
			if (renderingContext->scaler_ctl.hscale)
				scale_x *= 2.f;
		}
		else if (config::EmulateFramebuffer)
		{
			if (renderingContext->scaler_ctl.hscale)
				scale_x *= 2.f;
			// vscalefactor is applied after scissoring if > 1
			if (renderingContext->scaler_ctl.vscalefactor > 0x401 || renderingContext->scaler_ctl.vscalefactor < 0x400)
			{
				float vscalefactor = 1024.f / renderingContext->scaler_ctl.vscalefactor;
				if (vscalefactor < 1)
					scale_y /= vscalefactor;
			}
		}
	}

	const rend_context *renderingContext = nullptr;

	glm::mat4 normalMatrix;
	glm::mat4 scissorMatrix;
	glm::mat4 viewportMatrix;
	glm::vec2 dcViewport;
	glm::vec2 renderViewport;
	float sidebarWidth = 0;
};

inline static void getScaledFramebufferSize(const rend_context& rendCtx, int& width, int& height)
{
	getPvrFramebufferSize(rendCtx, width, height);
	if (!config::EmulateFramebuffer)
	{
		float upscaling = config::RenderResolution / 480.f;
		float w = width * upscaling;
		float h = height * upscaling;
		if (config::Widescreen && !config::Rotate90)
		{
			if (config::SuperWidescreen)
				w *= (float)settings.display.width / settings.display.height / 4.f * 3.f;
			else
				w *= 4.f / 3.f;
		}
		if (!config::Rotate90)
			w = std::round(w / 2.f) * 2.f;
		h = std::round(h);
		width = w;
		height = h;
	}
}

inline static float getOutputFramebufferAspectRatio()
{
	float aspectRatio;
	if (config::Rotate90)
	{
		aspectRatio = 3.f / 4.f;
	}
	else
	{
		if (config::Widescreen && !config::EmulateFramebuffer)
		{
			if (config::SuperWidescreen)
				aspectRatio = (float)settings.display.width / settings.display.height;
			else
				aspectRatio = 16.f / 9.f;
		}
		else
		{
			aspectRatio = 4.f / 3.f;
		}
	}
	return aspectRatio * config::ScreenStretching / 100.f;
}

inline static void getDCFramebufferReadSize(const FramebufferInfo& info, int& width, int& height)
{
	width = (info.fb_r_size.fb_x_size + 1) * 2;     // in 16-bit words
	height = info.fb_r_size.fb_y_size + 1;
	int modulus = (info.fb_r_size.fb_modulus - 1) * 2;

	int bpp;
	switch (info.fb_r_ctrl.fb_depth)
	{
		case fbde_0555:
		case fbde_565:
			bpp = 2;
			break;
		case fbde_888:
			bpp = 3;
			width = (width * 2) / 3;		// in pixels
			modulus = (modulus * 2) / 3;	// in pixels
			break;
		case fbde_C888:
			bpp = 4;
			width /= 2;             // in pixels
			modulus /= 2;           // in pixels
			break;
		default:
			bpp = 2;
			break;
	}

	if (info.spg_control.interlace && width == modulus && info.fb_r_sof2 == info.fb_r_sof1 + width * bpp)
		// Typical case alternating even and odd lines -> take the whole buffer at once
		height *= 2;
}

inline static float getDCFramebufferAspectRatio()
{
	float aspectRatio = config::Rotate90 ? 3.f / 4.f : 4.f / 3.f;
	return aspectRatio * config::ScreenStretching / 100.f;
}

inline static void getVideoShift(float& x, float& y)
{
	const bool vga = FB_R_CTRL.vclk_div == 1;
	switch (SPG_LOAD.hcount)
	{
		case 857: // NTSC, VGA
			x = VO_STARTX.HStart - (vga ? 0xa8 : 0xa4);
			break;
		case 863: // PAL
			x = VO_STARTX.HStart - 0xae;
			break;
		case 851: // Naomi
			x = VO_STARTX.HStart - 0xa5; // a0 for 15 kHz
			break;
		default:
			x = 0;
			INFO_LOG(RENDERER, "unknown video mode: hcount %d", SPG_LOAD.hcount);
			break;
	}
	switch (SPG_LOAD.vcount)
	{
		case 524: // NTSC, VGA
			y = VO_STARTY.VStart_field1 - (vga ? 0x28 : 0x12);
			break;
		case 262: // NTSC 240p
			y = VO_STARTY.VStart_field1 - 0x11;
			break;
		case 624: // PAL
			y = VO_STARTY.VStart_field1 - 0x2d;
			break;
		case 312: // PAL 240p
			y = VO_STARTY.VStart_field1 - 0x2e;
			break;
		case 529: // Naomi 31 kHz
			y = VO_STARTY.VStart_field1 - 0x24;
			break;
		case 536: // Naomi 15 kHz 480i
		case 268: // Naomi 15 kHz 240p
			y = VO_STARTY.VStart_field1 - 0x17; // 16 for 240p
			break;
		default:
			y = 0;
			INFO_LOG(RENDERER, "unknown video mode: vcount %d", SPG_LOAD.vcount);
			break;
	}
	if (!config::EmulateFramebuffer)
	{
		x *= config::RenderResolution / 480.f;
		y *= config::RenderResolution / 480.f;
	}
	x *= config::ScreenStretching / 100.f;
}
