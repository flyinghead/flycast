/*
	Copyright 2025 flyinghead

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
#include "metal_apple.h"
#include "ui/imgui_driver.h"
#include "imgui_impl_metal.h"
#include "metal_context.h"
#include <unordered_map>

#include "metal_texture.h"

class MetalDriver final : public ImGuiDriver {
public:
	MetalDriver() {
		ImGui_ImplMetal_Init(MetalContext::Instance()->GetDevice());
	}

	void reset() override
	{
		ImGuiDriver::reset();
		textures.clear();
		ImGui_ImplMetal_Shutdown();
	}

	~MetalDriver() {
		reset();
	}

	void newFrame() override {
		MetalContext *context = MetalContext::Instance();

		// Use existing descriptor and encoder when available
		if (context->GetDescriptor() != nullptr) {
			ImGui_ImplMetal_NewFrame(MetalContext::Instance()->GetDescriptor());
		} else {
			// ImGui only reads the attachment formats from this descriptor.
			// Use a 1x1 texture in the layer's format rather than taking a
			// swapchain drawable that would never be presented.
			const MTLPixelFormat format = [context->GetLayer() pixelFormat];
			if (formatTexture == nil || [formatTexture pixelFormat] != format)
			{
				MTLTextureDescriptor *texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
						width:1 height:1 mipmapped:NO];
				[texDesc setUsage:MTLTextureUsageRenderTarget];
				[texDesc setStorageMode:MTLStorageModePrivate];
				formatTexture = [context->GetDevice() newTextureWithDescriptor:texDesc];
			}
			descriptor = [[MTLRenderPassDescriptor alloc] init];

			[descriptor setDefaultRasterSampleCount:1];

			auto color = [descriptor colorAttachments][0];
			[color setClearColor:MTLClearColorMake(0.f, 0.f, 0.f, 1.f)];
			[color setTexture:formatTexture];
			[color setLoadAction:MTLLoadActionClear];
			[color setStoreAction:MTLStoreActionStore];

			ImGui_ImplMetal_NewFrame(descriptor);
		}
	}

	void renderDrawData(ImDrawData *drawData, bool gui_open) override {
		MetalContext *context = MetalContext::Instance();

		if (!context->IsValid())
			return;

		bool rendering = context->IsRendering();
		if (!rendering)
			context->NewFrame();
		if (!rendering || newFrameStarted)
		{
			context->BeginRenderPass();
			if (renderer->RenderLastFrame())
				context->PresentLastFrame();
		}

		// GetEncoder() is nil when BeginRenderPass skipped the frame (no drawable
		// available) — don't hand ImGui a nil encoder.
		if (context->GetEncoder() != nil)
			ImGui_ImplMetal_RenderDrawData(drawData, context->GetCommandBuffer(), context->GetEncoder());

		if (!rendering || newFrameStarted)
			context->EndFrame();
		newFrameStarted = false;
	}

	void present() override {
		MetalContext::Instance()->Present();
	}

	ImTextureID getTexture(const std::string &name) override {
		auto it = textures.find(name);
		if (it != textures.end())
			return (ImTextureID)(intptr_t)(__bridge void*)it->second.texture->GetTexture();

		return ImTextureID{};
	}

	ImTextureID updateTexture(const std::string &name, const u8 *data, int width, int height, bool nearestSampling) override {
		Texture texture(std::make_unique<MetalTexture>());
		texture.texture->tex_type = TextureType::_8888;
		texture.texture->UploadToGPU(width, height, data, false);

		auto textureID = (ImTextureID)(intptr_t)(__bridge void*)texture.texture->GetTexture();

		textures[name] = std::move(texture);

		return textureID;
	}

	void deleteTexture(const std::string &name) override {
		auto it = textures.find(name);
		// Guard the lookup before dereferencing — deleteTexture is called from
		// ImguiStateTexture::invalidate() during the unload/savestate path, which
		// can fire when the named texture was already removed by a prior game's
		// unload (game re-load) or the driver was reinitialized. Without this,
		// it->second dereferences end() and aborts under libc++ hardening
		// (__libcpp_verbose_abort). Mirrors the end() guard in getTexture().
		if (it == textures.end())
			return;
		textures.erase(name);
	}

private:
	struct Texture {
		Texture() = default;
		Texture(std::unique_ptr<MetalTexture>&& texture) : texture(std::move(texture)) {}

		std::unique_ptr<MetalTexture> texture;
	};

	MTLRenderPassDescriptor* descriptor;
	id<MTLTexture> formatTexture;
	std::unordered_map<std::string, Texture> textures;
	bool newFrameStarted = false;
};
