/*
    Copyright 2024 flyinghead

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
        ImGui_ImplMetal_Shutdown();
    }

    void newFrame() override {
        MetalContext *context = MetalContext::Instance();
        drawable = context->GetLayer()->nextDrawable();

        MTL::RenderPassDescriptor *descriptor = MTL::RenderPassDescriptor::alloc()->init();

        descriptor->setDefaultRasterSampleCount(1);

        auto color = descriptor->colorAttachments()->object(0);
        color->setClearColor(MTL::ClearColor(0.f, 0.f, 0.f, 1.f));
        color->setTexture(drawable->texture());
        color->setLoadAction(MTL::LoadActionClear);
        color->setStoreAction(MTL::StoreActionStore);

        commandEncoder = context->commandBuffer->renderCommandEncoder(descriptor);

        ImGui_ImplMetal_NewFrame(descriptor);

        // descriptor->release();
    }

    void renderDrawData(ImDrawData *drawData, bool gui_open) override {
        MetalContext *context = MetalContext::Instance();
        MTL::CommandBuffer *buffer = context->commandBuffer;

        ImGui_ImplMetal_RenderDrawData(drawData, buffer, commandEncoder);

        commandEncoder->endEncoding();
        buffer->presentDrawable(drawable);
        buffer->commit();

        buffer->release();
        commandEncoder->release();
        commandEncoder = nullptr;

        context->commandBuffer = context->GetQueue()->commandBuffer();

        if (gui_open)
            frameRendered = true;
    }

    void present() override {
        if (frameRendered)
            //MetalContext::Instance()->GetDevice().pre
        frameRendered = false;
    }

    ImTextureID getTexture(const std::string &name) override {
        auto it = textures.find(name);
        if (it != textures.end())
            return &it->second.texture;

        return ImTextureID{};
    }

    ImTextureID updateTexture(const std::string &name, const u8 *data, int width, int height, bool nearestSampling) override {
        Texture texture(std::make_unique<MetalTexture>());
        texture.texture->tex_type = TextureType::_8888;
        texture.texture->UploadToGPU(width, height, data, false);

        ImTextureID textureID = texture.texture->texture;

        textures[name] = std::move(texture);

        return textureID;
    }

    void deleteTexture(const std::string &name) override {
        auto it = textures.find(name);
        it->second.texture->texture->setPurgeableState(MTL::PurgeableStateEmpty);
        textures.erase(name);
    }

private:
    struct Texture {
        Texture() = default;
        Texture(std::unique_ptr<MetalTexture>&& texture) : texture(std::move(texture)) {}

        std::unique_ptr<MetalTexture> texture;
    };

    bool frameRendered = false;
    MTL::RenderCommandEncoder *commandEncoder;
    CA::MetalDrawable *drawable;
    std::unordered_map<std::string, Texture> textures;
};
