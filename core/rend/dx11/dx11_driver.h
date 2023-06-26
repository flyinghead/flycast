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
#include "imgui/backends/imgui_impl_dx11.h"
#include "dx11context.h"
#include "rend/gui.h"
#include <unordered_map>

class DX11Driver final : public ImGuiDriver
{
public:
	DX11Driver(ID3D11Device* device, ID3D11DeviceContext* deviceContext) {
		ImGui_ImplDX11_Init(device, deviceContext);
	}

	~DX11Driver() {
		ImGui_ImplDX11_Shutdown();
	}

    void newFrame() override {
    	ImGui_ImplDX11_NewFrame();
	}

	void renderDrawData(ImDrawData *drawData, bool gui_open) override {
		theDX11Context.EndImGuiFrame();
		if (gui_open)
			frameRendered = true;
	}

	void present() override {
		if (frameRendered)
			theDX11Context.Present();
		frameRendered = false;
	}

	void setFrameRendered() override {
		frameRendered = true;
	}

	ImTextureID getTexture(const std::string& name) override {
		auto it = textures.find(name);
		if (it != textures.end())
			return (ImTextureID)it->second.textureView.get();
		else
			return ImTextureID{};
	}

	ImTextureID updateTexture(const std::string& name, const u8 *data, int width, int height) override
	{
		Texture& texture = textures[name];
		texture.texture.reset();
		texture.textureView.reset();

		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.MipLevels = 1;
		theDX11Context.getDevice()->CreateTexture2D(&desc, nullptr, &texture.texture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = desc.MipLevels;
		theDX11Context.getDevice()->CreateShaderResourceView(texture.texture, &viewDesc, &texture.textureView.get());

		theDX11Context.getDeviceContext()->UpdateSubresource(texture.texture, 0, nullptr, data, width * 4, width * 4 * height);

	    return (ImTextureID)texture.textureView.get();
	}

private:
	struct Texture
	{
		ComPtr<ID3D11Texture2D> texture;
		ComPtr<ID3D11ShaderResourceView> textureView;
	};

	bool frameRendered = false;
	std::unordered_map<std::string, Texture> textures;
};
