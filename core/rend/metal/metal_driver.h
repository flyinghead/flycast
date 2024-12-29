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

class MetalDriver final : public ImGuiDriver {
public:
    void reset() override
    {
        ImGuiDriver::reset();
        ImGui_ImplMetal_Shutdown();
    }

    void newFrame() override {

    }

    void renderDrawData(ImDrawData *drawData, bool gui_open) override {

    }

    void present() override {

    }

    ImTextureID getTexture(const std::string &name) override {
        auto it = textures.find(name);
        if (it != textures.end())
            return &it->second.texture;
        else
            return ImTextureID{};
    }

    ImTextureID updateTexture(const std::string &name, const u8 *data, int width, int height, bool nearestSampling) override {
        Texture& texture = textures[name];
        texture.texture->setPurgeableState(MTL::PurgeableStateEmpty);

        MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
        desc->setWidth(width);
        desc->setHeight(height);

        MTL::Region region = MTL::Region { 0, 0, static_cast<NS::UInteger>(width), static_cast<NS::UInteger>(height) };
        texture.texture = MetalContext::Instance()->GetDevice()->newTexture(desc);
        texture.texture->replaceRegion(region, 0, data, width * 4);

        return texture.texture;
    }

    void deleteTexture(const std::string &name) override {
        auto it = textures.find(name);
        it->second.texture->setPurgeableState(MTL::PurgeableStateEmpty);
        textures.erase(name);
    }

private:
    struct Texture {
        MTL::Texture *texture;
    };

    bool frameRendered = false;
    std::unordered_map<std::string, Texture> textures;
};