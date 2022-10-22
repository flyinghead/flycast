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
#include "rend/imgui_driver.h"
#include "imgui_impl_dx9.h"
#include "dxcontext.h"
#include <unordered_map>

class DX9Driver final : public ImGuiDriver
{
public:
	DX9Driver(IDirect3DDevice9* device) {
		ImGui_ImplDX9_Init(device);
	}

	~DX9Driver() {
		ImGui_ImplDX9_Shutdown();
	}

	void newFrame() override {
    	ImGui_ImplDX9_NewFrame();
	}

	void renderDrawData(ImDrawData *drawData) override {
		theDXContext.EndImGuiFrame();
		if (gui_is_open())
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

	ImTextureID getTexture(const std::string& name) override {
		auto it = textures.find(name);
		if (it != textures.end())
			return (ImTextureID)it->second.get();
		else
			return ImTextureID{};
	}

	ImTextureID updateTexture(const std::string& name, const u8 *data, int width, int height) override
	{
		ComPtr<IDirect3DTexture9>& texture = textures[name];
		texture.reset();
		theDXContext.getDevice()->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture.get(), 0);

		width *= 4;

		D3DLOCKED_RECT rect;
		texture->LockRect(0, &rect, nullptr, 0);
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
		texture->UnlockRect(0);

	    return (ImTextureID)texture.get();
	}

private:
	bool frameRendered = false;
	std::unordered_map<std::string, ComPtr<IDirect3DTexture9>> textures;
};
