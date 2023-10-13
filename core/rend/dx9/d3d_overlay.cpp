/*
	Copyright 2021 flyinghead

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
#include "d3d_overlay.h"
#include "rend/osd.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <tuple>

void D3DOverlay::drawQuad(const RECT& rect, D3DCOLOR color)
{
	device->SetTextureStageState(0, D3DTSS_CONSTANT, color);
	Vertex quad[] {
		{ (float)(rect.left),  (float)(rect.top),    0.5f, 0.f, 0.f },
		{ (float)(rect.left),  (float)(rect.bottom), 0.5f, 0.f, 1.f },
		{ (float)(rect.right), (float)(rect.top),    0.5f, 1.f, 0.f },
		{ (float)(rect.right), (float)(rect.bottom), 0.5f, 1.f, 1.f }
	};
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));
}

void D3DOverlay::draw(u32 width, u32 height, bool vmu, bool crosshair)
{
	setupRenderState(width, height);
	if (vmu)
	{
		float vmu_padding = 8.f * settings.display.uiScale;
		float vmu_height = 70.f * settings.display.uiScale;
		float vmu_width = 48.f / 32.f * vmu_height;

		for (size_t i = 0; i < vmuTextures.size(); i++)
		{
			ComPtr<IDirect3DTexture9>& texture = vmuTextures[i];
			if (!vmu_lcd_status[i])
			{
				texture.reset();
				continue;
			}
			if (texture == nullptr || vmu_lcd_changed[i])
			{
				texture.reset();
				device->CreateTexture(48, 32, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture.get(), 0);
				D3DLOCKED_RECT rect;
				if (SUCCEEDED(texture->LockRect(0, &rect, nullptr, 0)))
				{
					u8 *dst = (u8 *) rect.pBits;
					for (int y = 0; y < 32; y++)
						memcpy(dst + y * rect.Pitch, vmu_lcd_data[i] + (31 - y) * 48, 48 * 4);
					texture->UnlockRect(0);
				}
				vmu_lcd_changed[i] = false;
			}
			float x;
			if (i & 2)
				x = width - vmu_padding - vmu_width;
			else
				x = vmu_padding;
			float y;
			if (i & 4)
			{
				y = height - vmu_padding - vmu_height;
				if (i & 1)
					y -= vmu_padding + vmu_height;
			}
			else
			{
				y = vmu_padding;
				if (i & 1)
					y += vmu_padding + vmu_height;
			}
			device->SetTexture(0, texture);
			RECT rect { (long)x, (long)y, (long)(x + vmu_width), (long)(y + vmu_height) };
			drawQuad(rect, D3DCOLOR_ARGB(192, 255, 255, 255));
		}
	}
	if (crosshair)
	{
		if (!xhairTexture)
		{
			const u32* texData = getCrosshairTextureData();
			device->CreateTexture(16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &xhairTexture.get(), 0);
			D3DLOCKED_RECT rect;
			if (SUCCEEDED(xhairTexture->LockRect(0, &rect, nullptr, 0)))
			{
				if (rect.Pitch == 16 * sizeof(u32))
					memcpy(rect.pBits, texData, 16 * 16 * sizeof(u32));
				else
				{
					u8 *dst = (u8 *) rect.pBits;
					for (int y = 0; y < 16; y++)
						memcpy(dst + y * rect.Pitch, texData + y * 16, 16 * sizeof(u32));
				}
				xhairTexture->UnlockRect(0);
			}
		}
		device->SetTexture(0, xhairTexture);
		for (u32 i = 0; i < config::CrosshairColor.size(); i++)
		{
			if (config::CrosshairColor[i] == 0)
				continue;
			if (settings.platform.isConsole()
					&& config::MapleMainDevices[i] != MDT_LightGun)
				continue;

			auto [x, y] = getCrosshairPosition(i);
			float halfWidth = XHAIR_WIDTH * settings.display.uiScale / 2.f;
			RECT rect { (long) (x - halfWidth), (long) (y - halfWidth), (long) (x + halfWidth), (long) (y + halfWidth) };
			D3DCOLOR color = (config::CrosshairColor[i] & 0xFF00FF00)
					| ((config::CrosshairColor[i] >> 16) & 0xFF)
					| ((config::CrosshairColor[i] & 0xFF) << 16);
			drawQuad(rect, color);
		}
	}
}

void D3DOverlay::setupRenderState(u32 displayWidth, u32 displayHeight)
{
	device->SetPixelShader(NULL);
	device->SetVertexShader(NULL);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CONSTANT);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CONSTANT);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

	glm::mat4 identity = glm::identity<glm::mat4>();
	glm::mat4 projection = glm::translate(glm::vec3(-1.f - 1.f / displayWidth, 1.f + 1.f / displayHeight, 0))
		* glm::scale(glm::vec3(2.f / displayWidth, -2.f / displayHeight, 1.f));

	device->SetTransform(D3DTS_WORLD, (const D3DMATRIX *)&identity[0][0]);
	device->SetTransform(D3DTS_VIEW, (const D3DMATRIX *)&identity[0][0]);
	device->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX *)&projection[0][0]);

	device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
}
