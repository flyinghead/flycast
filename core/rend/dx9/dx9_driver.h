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
#pragma once
#include "ui/imgui_driver.h"
#include "imgui_impl_dx9.h"
#include "dxcontext.h"
#include <unordered_map>

class DX9Driver final : public ImGuiDriver
{
public:
	DX9Driver(IDirect3DDevice9* device) {
		ImGui_ImplDX9_Init(device);
	}

	~DX9Driver() override {
		ImGui_ImplDX9_Shutdown();
	}

	void newFrame() override {
    	ImGui_ImplDX9_NewFrame();
	}

	void renderDrawData(ImDrawData *drawData, bool gui_open) override {
		theDXContext.EndImGuiFrame();
		if (gui_open)
			frameRendered = true;
	}

	void present() override {
		if (frameRendered)
			theDXContext.Present();
		frameRendered = false;
	}

	void setFrameRendered() override {
		frameRendered = true;
	}

	ImTextureID getTexture(const std::string& name) override
	{
		auto it = textures.find(name);
		if (it != textures.end())
			return (ImTextureID)&it->second.imTexture;
		else
			return ImTextureID{};
	}

	ImTextureID updateTexture(const std::string& name, const u8 *data, int width, int height, bool nearestSampling) override
	{
		Texture& texture = textures[name];
		texture.tex.reset();
		HRESULT hr = theDXContext.getDevice()->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture.tex.get(), 0);
		if (FAILED(hr) || !texture.tex)
		{
			WARN_LOG(RENDERER, "CreateTexture failed (%d x %d): error %x", width, height, hr);
			textures.erase(name);
			return ImTextureID();
		}

		width *= 4;

		D3DLOCKED_RECT rect;
		texture.tex->LockRect(0, &rect, nullptr, 0);
		u8 *dst = (u8 *)rect.pBits;
		const u8 *src = data;
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x += 4)
			{
				// BGRA <- RGBA
				dst[x + 0] = src[x + 2];
				dst[x + 1] = src[x + 1];
				dst[x + 2] = src[x + 0];
				dst[x + 3] = src[x + 3];
			}
			dst += rect.Pitch;
			src += width;
		}
		texture.tex->UnlockRect(0);
		texture.imTexture.d3dTexture = texture.tex.get();
		texture.imTexture.pointSampling = nearestSampling;

	    return (ImTextureID)&texture.imTexture;
	}

	void deleteTexture(const std::string& name) override {
		textures.erase(name);
	}

private:
	bool frameRendered = false;
	struct Texture
	{
		ComPtr<IDirect3DTexture9> tex;
		ImTextureDX9 imTexture;
	};
	std::unordered_map<std::string, Texture> textures;
};
