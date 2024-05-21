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
#include "imgui_impl_vulkan.h"
#include "vulkan_context.h"
#include "texture.h"
#include <unordered_map>

class VulkanDriver final : public ImGuiDriver
{
public:
	void reset() override
	{
		ImGuiDriver::reset();
		textures.clear();
		linearSampler.reset();
		pointSampler.reset();
		ImGui_ImplVulkan_Shutdown();
		justStarted = true;
	}

	~VulkanDriver() {
		reset();
	}

	void newFrame() override {
		ImGui_ImplVulkan_NewFrame();
	}

	void renderDrawData(ImDrawData *drawData, bool gui_open) override
	{
		VulkanContext *context = getContext();
		if (!context->IsValid())
			return;
		try {
			bool rendering = context->IsRendering();
			if (!rendering)
				context->NewFrame(); // may reset this driver
			if (!rendering || newFrameStarted)
			{
				context->BeginRenderPass();
				context->PresentLastFrame();
			}
			if (!justStarted)
				// Record Imgui Draw Data and draw funcs into command buffer
				ImGui_ImplVulkan_RenderDrawData(drawData, (VkCommandBuffer)getCommandBuffer());
			justStarted = false;
			if (!rendering || newFrameStarted)
				context->EndFrame();
			newFrameStarted = false;
		} catch (const InvalidVulkanContext& err) {
		}
	}

	void present() override {
		getContext()->Present(); // may reset this driver
	}

	ImTextureID getTexture(const std::string& name) override {
		auto it = textures.find(name);
		if (it != textures.end())
			return it->second.textureId;
		else
			return ImTextureID{};
	}

	ImTextureID updateTexture(const std::string& name, const u8 *data, int width, int height, bool nearestSampling) override
	{
		VkTexture vkTex(std::make_unique<Texture>());
		vkTex.texture->tex_type = TextureType::_8888;
		vkTex.texture->SetCommandBuffer(getCommandBuffer());
		vkTex.texture->UploadToGPU(width, height, data, false);
		vkTex.texture->SetCommandBuffer(nullptr);
		VkSampler sampler;
		if (nearestSampling)
		{
			if (!pointSampler)
			{
				pointSampler = getContext()->GetDevice().createSamplerUnique(
						vk::SamplerCreateInfo(vk::SamplerCreateFlags(),
								vk::Filter::eNearest, vk::Filter::eNearest,
								vk::SamplerMipmapMode::eNearest,
								vk::SamplerAddressMode::eClampToBorder,
								vk::SamplerAddressMode::eClampToBorder,
								vk::SamplerAddressMode::eClampToEdge, 0.0f, false,
								0.f, false, vk::CompareOp::eNever, 0.0f, VK_LOD_CLAMP_NONE,
								vk::BorderColor::eFloatTransparentBlack));
			}
			sampler = (VkSampler)*pointSampler;
		}
		else
		{
			if (!linearSampler)
			{
				linearSampler = getContext()->GetDevice().createSamplerUnique(
						vk::SamplerCreateInfo(vk::SamplerCreateFlags(),
								vk::Filter::eLinear, vk::Filter::eLinear,
								vk::SamplerMipmapMode::eLinear,
								vk::SamplerAddressMode::eClampToBorder,
								vk::SamplerAddressMode::eClampToBorder,
								vk::SamplerAddressMode::eClampToEdge, 0.0f, false,
								0.f, false, vk::CompareOp::eNever, 0.0f, VK_LOD_CLAMP_NONE,
								vk::BorderColor::eFloatTransparentBlack));
			}
			sampler = (VkSampler)*linearSampler;
		}
		ImTextureID texId = vkTex.textureId = ImGui_ImplVulkan_AddTexture(sampler, (VkImageView)vkTex.texture->GetImageView(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		// TODO update existing texture
		//auto it = textures.find(name);
		//if (it != textures.end() && it->second.texture != nullptr)
		//	textureCache.DestroyLater(it->second.texture.get());

		textures[name] = std::move(vkTex);

		return texId;
	}

	void deleteTexture(const std::string& name) override
	{
		auto it = textures.find(name);
		if (it != textures.end())
		{
			class DescSetDeleter : public Deletable
			{
			public:
				DescSetDeleter(VkDescriptorSet descSet) : descSet(descSet) {}
				~DescSetDeleter() {
					ImGui_ImplVulkan_RemoveTexture(descSet);
				}
				VkDescriptorSet descSet;
			};
			getContext()->addToFlight(new DescSetDeleter((VkDescriptorSet)it->second.textureId));
			if (it->second.texture != nullptr)
				it->second.texture->deferDeleteResource(getContext());
			textures.erase(it);
		}
	}

private:
	struct VkTexture {
		VkTexture() = default;
		VkTexture(std::unique_ptr<Texture>&& texture, ImTextureID textureId = ImTextureID())
			: texture(std::move(texture)), textureId(textureId) {}

		std::unique_ptr<Texture> texture;
		ImTextureID textureId{};
	};

	VulkanContext *getContext() {
		return VulkanContext::Instance();
	}

	vk::CommandBuffer getCommandBuffer()
	{
		VulkanContext *context = getContext();
		if (!context->IsRendering())
		{
			if (context->recreateSwapChainIfNeeded())
				throw InvalidVulkanContext();
			context->NewFrame();
			newFrameStarted = true;
		}
		return context->GetCurrentCommandBuffer();
	}

	std::unordered_map<std::string, VkTexture> textures;
	vk::UniqueSampler linearSampler;
	vk::UniqueSampler pointSampler;
	bool newFrameStarted = false;
	bool justStarted = true;
};
