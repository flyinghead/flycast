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
#include "dx11_overlay.h"
#include "rend/osd.h"
#ifdef LIBRETRO
#include "vmu_xhair.h"
#endif

void DX11Overlay::draw(u32 width, u32 height, bool vmu, bool crosshair)
{
	RECT rect { 0, 0, (LONG)width, (LONG)height };
	deviceContext->RSSetScissorRects(1, &rect);
	if (vmu)
	{
#ifndef LIBRETRO
		float vmu_padding = 8.f * settings.display.uiScale;
		float vmu_height = 70.f * settings.display.uiScale;
		float vmu_width = 48.f / 32.f * vmu_height;

		const float blend_factor[4] = { 0.75f, 0.75f, 0.75f, 0.75f };
		deviceContext->OMSetBlendState(blendStates.getState(true, 8, 8), blend_factor, 0xffffffff);
#else
		float vmu_padding_x = 8.f * width / 640.f;
		float vmu_padding_y = 8.f * height / 480.f;
		float vmu_width = 48.f * width / 640.f;
		float vmu_height = 32.f * height / 480.f;
		if (config::Widescreen)
		{
			vmu_padding_x = vmu_padding_x / 4.f * 3.f;
			vmu_width = vmu_width / 4.f * 3.f;
		}
		deviceContext->OMSetBlendState(blendStates.getState(true, 4, 5), nullptr, 0xffffffff);
#endif

		for (size_t i = 0; i < vmuTextures.size(); i++)
		{
			if (!vmu_lcd_status[i])
			{
				vmuTextureViews[i].reset();
				vmuTextures[i].reset();
				continue;
			}
			if (vmuTextures[i] == nullptr || this->vmuLastChanged[i] != ::vmuLastChanged[i])
			{
				vmuTextureViews[i].reset();
				vmuTextures[i].reset();
				D3D11_TEXTURE2D_DESC desc{};
				desc.Width = 48;
				desc.Height = 32;
				desc.ArraySize = 1;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				desc.MipLevels = 1;

				if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &vmuTextures[i].get())))
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
					viewDesc.Format = desc.Format;
					viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					viewDesc.Texture2D.MipLevels = desc.MipLevels;
					device->CreateShaderResourceView(vmuTextures[i], &viewDesc, &vmuTextureViews[i].get());

					u32 data[48 * 32];
					for (int y = 0; y < 32; y++)
						memcpy(&data[y * 48], &vmu_lcd_data[i][(31 - y) * 48], sizeof(u32) * 48);
					deviceContext->UpdateSubresource(vmuTextures[i], 0, nullptr, data, 48 * 4, 48 * 4 * 32);
					this->vmuLastChanged[i] = ::vmuLastChanged[i];
				}
			}
			float x, y;
			float w = vmu_width;
			float h = vmu_height;
#ifdef LIBRETRO
			if (i & 1)
				continue;
			w *= (float)vmu_screen_params[i / 2].vmu_screen_size_mult / config::ScreenStretching * 100.f;
			h *= vmu_screen_params[i / 2].vmu_screen_size_mult;
			switch (vmu_screen_params[i / 2].vmu_screen_position)
			{
			case UPPER_LEFT:
			default:
				x = vmu_padding_x;
				y = vmu_padding_y;
				break;
			case UPPER_RIGHT:
				x = width - vmu_padding_x - w;
				y = vmu_padding_y;
				break;
			case LOWER_LEFT:
				x = vmu_padding_x;
				y = height - vmu_padding_y - h;
				break;
			case LOWER_RIGHT:
				x = width - vmu_padding_x - w;
				y = height - vmu_padding_y - h;
				break;
			}
#else
			if (i & 2)
				x = width - vmu_padding - vmu_width;
			else
				x = vmu_padding;
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
#endif
			D3D11_VIEWPORT vp{};
			vp.TopLeftX = x;
			vp.TopLeftY = y;
			vp.Width = w;
			vp.Height = h;
			vp.MinDepth = 0.f;
			vp.MaxDepth = 1.f;
			deviceContext->RSSetViewports(1, &vp);
			quad.draw(vmuTextureViews[i], samplers->getSampler(false));
		}
	}
	if (crosshair && crosshairsNeeded())
	{
		if (!xhairTexture)
		{
			const u32* texData = getCrosshairTextureData();
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = 16;
			desc.Height = 16;
			desc.ArraySize = 1;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.MipLevels = 1;

			if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &xhairTexture.get())))
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
				viewDesc.Format = desc.Format;
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MipLevels = desc.MipLevels;
				device->CreateShaderResourceView(xhairTexture, &viewDesc, &xhairTextureView.get());

				deviceContext->UpdateSubresource(xhairTexture, 0, nullptr, texData, 16 * 4, 16 * 4 * 16);
			}
		}
		for (u32 i = 0; i < config::CrosshairColor.size(); i++)
		{
			if (config::CrosshairColor[i] == 0)
				continue;
			if (settings.platform.isConsole()
					&& config::MapleMainDevices[i] != MDT_LightGun)
				continue;

			auto [x, y] = getCrosshairPosition(i);
#ifdef LIBRETRO
			float halfWidth = lightgun_crosshair_size / 2.f / config::ScreenStretching * 100.f * config::RenderResolution / 480.f;
			float halfHeight = lightgun_crosshair_size / 2.f * config::RenderResolution / 480.f;
			x /= config::ScreenStretching / 100.f;
#else
			float halfWidth = config::CrosshairSize * settings.display.uiScale / 2.f;
			float halfHeight = halfWidth;
#endif
			D3D11_VIEWPORT vp{};
			vp.TopLeftX = x - halfWidth;
			vp.TopLeftY = y - halfHeight;
			vp.Width = halfWidth * 2;
			vp.Height = halfHeight * 2;
			vp.MinDepth = 0.f;
			vp.MaxDepth = 1.f;
			deviceContext->RSSetViewports(1, &vp);

			const float colors[4] = {
					(config::CrosshairColor[i] & 0xff) / 255.f,
					((config::CrosshairColor[i] >> 8) & 0xff) / 255.f,
					((config::CrosshairColor[i] >> 16) & 0xff) / 255.f,
					((config::CrosshairColor[i] >> 24) & 0xff) / 255.f
			};
			deviceContext->OMSetBlendState(blendStates.getState(true, 4, 5), nullptr, 0xffffffff);
			quad.draw(xhairTextureView, samplers->getSampler(false), colors);
		}
	}
}
