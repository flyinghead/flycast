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

    void newFrame() override {
        MetalContext *context = MetalContext::Instance();

        // Use existing descriptor and encoder when available
        if (context->GetDescriptor() != nullptr) {
            ImGui_ImplMetal_NewFrame(MetalContext::Instance()->GetDescriptor());
        } else {
            drawable = [context->GetLayer() nextDrawable];
            descriptor = [[MTLRenderPassDescriptor alloc] init];

            [descriptor setDefaultRasterSampleCount:1];

            auto color = [descriptor colorAttachments][0];
            [color setClearColor:MTLClearColorMake(0.f, 0.f, 0.f, 1.f)];
            [color setTexture:[drawable texture]];
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
        [it->second.texture->GetTexture() setPurgeableState:MTLPurgeableStateEmpty];
        textures.erase(name);
    }

private:
    struct Texture {
        Texture() = default;
        Texture(std::unique_ptr<MetalTexture>&& texture) : texture(std::move(texture)) {}

        std::unique_ptr<MetalTexture> texture;
    };

    MTLRenderPassDescriptor* descriptor;
    id<CAMetalDrawable> drawable;
    std::unordered_map<std::string, Texture> textures;
    bool newFrameStarted = false;
};
