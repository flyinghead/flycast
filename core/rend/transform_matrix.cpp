/*
	Copyright 2026 flyinghead

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
#include "transform_matrix.h"
#include "tileclip.h"
#include <glm/gtx/transform.hpp>

inline static void getTAViewport(const rend_context& rendCtx, int& width, int& height) {
	width = rendCtx.globClip.x;
	height = rendCtx.globClip.y;
}

inline static void getPvrFramebufferSize(const rend_context& rendCtx, int& width, int& height)
{
	getTAViewport(rendCtx, width, height);
	if (!config::EmulateFramebuffer)
	{
		int maxHeight = FB_R_CTRL.vclk_div == 0 && SPG_CONTROL.interlace == 0 ? 240 : 480;
		// we ignore vscalefactor when interlaced because it's used for Stretched PAL (factor in ]1, 1.3]),
		// or Flicker-free interlace mode B (factor [0.5, 0.6]), which are only emulated in Full FB emu.
		// In 240p, some games (Cho - Hatsumei Boy Kanipan) use a 1/2 factor from rendering at 480 to a 240-line framebuffer.
		// So we render at 480 in that case.
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

bool TransformMatrix::IsClipped() const
{
	int width, height;
	getTAViewport(*renderingContext, width, height);
	Rect clip = intersectTileAndFBScissor();
	return clip.origin.x > 0
			|| clip.bottomRight().x < width - 1;
}

void TransformMatrix::CalcMatrices(const rend_context *renderingContext, int width, int height)
{
	const int flipY = directx ? -1 : 1;

	glm::vec2 renderViewport = { width == 0 ? settings.display.width : width, height == 0 ? settings.display.height : height };
	glm::vec2 dcViewport;
	this->renderingContext = renderingContext;

	if (renderingContext->isRTT)
	{
		// TODO unscale fbClip values
		const Rect globClip(glm::ivec2(0, 0), renderingContext->globClip);
		const Rect clip = intersect(globClip, renderingContext->fbClip);
		dcViewport.x = (float)clip.size.x;
		if (renderingContext->scaler_ctl.hscale)
			dcViewport.x *= 2;
		dcViewport.y = (float)clip.size.y;
		normalMatrix = glm::translate(glm::vec3(-1, -flipY, 0))
			* glm::scale(glm::vec3(2.0f / dcViewport.x, 2.0f / dcViewport.y * flipY, 1.f));
		sidebarWidth = 0;
	}
	else
	{
		int w, h;
		getPvrFramebufferSize(*renderingContext, w, h);
		dcViewport.x = w;
		dcViewport.y = h;

		if (config::Widescreen && !config::Rotate90 && !config::EmulateFramebuffer)
		{
			sidebarWidth = (1 - dcViewport.x / dcViewport.y * renderViewport.y / renderViewport.x) / 2;
			if (config::SuperWidescreen)
				dcViewport.x *= (float)settings.display.width / settings.display.height / 4.f * 3.f;
			else
				dcViewport.x *= 4.f / 3.f;
		}
		else {
			sidebarWidth = 0;
		}
		glm::mat4 trans = glm::translate(glm::vec3(-1 + 2 * sidebarWidth, -flipY, 0));
		float x_coef = 2.0f / dcViewport.x;
		float y_coef = 2.0f / dcViewport.y * flipY;
		normalMatrix = trans * glm::scale(glm::vec3(x_coef, y_coef, 1.f));
	}
	normalMatrix = glm::scale(glm::vec3(1, 1, 1 / config::ExtraDepthScale)) * normalMatrix;

	glm::mat4 vp_trans = glm::translate(glm::vec3(1, flipY, 0));
	if (renderingContext->isRTT) {
		vp_trans = glm::scale(glm::vec3(dcViewport.x / 2, dcViewport.y / 2 * flipY, 1.f))
			* vp_trans;
	}
	else {
		vp_trans = glm::scale(glm::vec3(renderViewport.x / 2, renderViewport.y / 2 * flipY, 1.f))
			* vp_trans;
	}
	viewportMatrix = vp_trans * normalMatrix;
}

Rect TransformMatrix::intersectTileAndFBScissor() const
{
	// Framebuffer clipping is applied after scaling (SCALER_CTL).
	// Since we want to apply it before, we need to "unscale" the clipping rectangle.
	glm::ivec2 botRight = renderingContext->fbClip.origin + renderingContext->fbClip.size;
	float xscale, yscale;
	getScissorScaling(xscale, yscale);
	Rect rect;
	rect.origin = glm::max(glm::ivec2(std::round(renderingContext->fbClip.origin.x * xscale), std::round(renderingContext->fbClip.origin.y * yscale)),
			renderingContext->tileClip.origin);
	botRight = glm::min(glm::ivec2(std::round(botRight.x * xscale), std::round(botRight.y * yscale)),
			renderingContext->tileClip.origin + renderingContext->tileClip.size);
	rect.size = glm::max(botRight - rect.origin, glm::ivec2(0, 0));
	return rect;
}

void TransformMatrix::getScissorScaling(float& scale_x, float& scale_y) const
{
	scale_x = 1.f;
	scale_y = 1.f;

	if (!config::EmulateFramebuffer)
	{
		if (renderingContext->scaler_ctl.vscalefactor < 1024 || renderingContext->scaler_ctl.vscalefactor > 1025)
			scale_y *= renderingContext->scaler_ctl.vscalefactor / 1024.f;
		if (renderingContext->scaler_ctl.hscale == 1)
			scale_x *= 2.f;
	}
}

void getScaledFramebufferSize(const rend_context& rendCtx, int& width, int& height)
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

float getOutputFramebufferAspectRatio()
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

void getDCFramebufferReadSize(const FramebufferInfo& info, int& width, int& height)
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

void getWindowboxDimensions(int outwidth, int outheight, float renderAR, int& dx, int& dy, bool rotate)
{
	if (outwidth == 0 || outheight == 0 || renderAR == 0.f) {
		dx = dy = 0;
		return;
	}
	if (config::IntegerScale)
	{
		int fbh = config::RenderResolution;
		if (fbh == 0) {
			dx = dy = 0;
			return;
		}
		int fbw = (int)((rotate ? 1 / renderAR : renderAR) * fbh);
		if (rotate)
			std::swap(fbw, fbh);

		int scale = std::min(outwidth / fbw, outheight / fbh);
		if (scale == 0) {
			scale = std::max(fbw / outwidth, fbh / outheight) + 1;
			dx = (outwidth - fbw / scale) / 2;
			dy = (outheight - fbh / scale) / 2;
		}
		else {
			dx = (outwidth - fbw * scale) / 2;
			dy = (outheight - fbh * scale) / 2;
		}
	}
	else
	{
		float screenAR = (float)outwidth / outheight;
		if (renderAR > screenAR)
			dy = (int)roundf(outheight * (1 - screenAR / renderAR) / 2.f);
		else
			dx = (int)roundf(outwidth * (1 - renderAR / screenAR) / 2.f);
	}
}

void getVideoShift(float& x, float& y)
{
	const bool vga = FB_R_CTRL.vclk_div == 1;
	switch (SPG_LOAD.hcount)
	{
		case 857: // NTSC, VGA
			x = (int)VO_STARTX.HStart - (vga ? 0xa8 : 0xa4);
			break;
		case 863: // PAL
			x = (int)VO_STARTX.HStart - 0xae;
			break;
		case 851: // Naomi
		case 850: // meltyb
			x = (int)VO_STARTX.HStart - 0xa5; // a0 for 15 kHz
			break;
		default:
			x = 0;
			INFO_LOG(RENDERER, "unknown video mode: hcount %d", SPG_LOAD.hcount);
			break;
	}
	switch (SPG_LOAD.vcount)
	{
		case 524: // NTSC, VGA
			y = (int)VO_STARTY.VStart_field1 - (vga ? 0x28 : 0x12);
			break;
		case 262: // NTSC 240p
			y = (int)VO_STARTY.VStart_field1 - 0x11;
			break;
		case 624: // PAL
			y = (int)VO_STARTY.VStart_field1 - 0x2d;
			break;
		case 312: // PAL 240p
			y = (int)VO_STARTY.VStart_field1 - 0x2e;
			break;
		case 529: // Naomi 31 kHz
		case 528: // meltyb
			y = (int)VO_STARTY.VStart_field1 - 0x24;
			break;
		case 536: // Naomi 15 kHz 480i
		case 268: // Naomi 15 kHz 240p
			y = (int)VO_STARTY.VStart_field1 - 0x17; // 16 for 240p
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
