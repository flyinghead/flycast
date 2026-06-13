/*
	Copyright 2026 flyinghead

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
#include "rend/vulkan/swappyvk.h"
#include "jni_util.h"
#include "wsi/context.h"
#include "cfg/option.h"
#include <swappy/swappyVk.h>

#ifdef SWAPPY

extern jobject g_activity;

namespace swappyvk
{
static bool googleTimingSupported;
static vk::PhysicalDevice phyDevice;
static vk::Device device;
static vk::SwapchainKHR swapchain;
static bool swappyAvailable;
static bool enabled;

void setQueueFamilyIndex(bool googleTimingSupported, vk::Device device, vk::Queue queue, u32 queueFamilyIndex)
{
	swappyvk::googleTimingSupported = googleTimingSupported;
	if (googleTimingSupported)
		SwappyVk_setQueueFamilyIndex((VkDevice)device, (VkQueue)queue, queueFamilyIndex);
}

static void activate(bool enable)
{
	if (enabled == enable)
		return;
	enabled = enable;
	swappyAvailable = false;
	if (enable)
	{
		if (settings.input.fastForwardMode || !config::VSync || !googleTimingSupported)
			return;

		// Fake call to make swappy use google timing for this device
		VkExtensionProperties prop { { VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME } };
		u32 requiredExtensionCount = 1;
		char s[VK_MAX_EXTENSION_NAME_SIZE];
		char *requiredExtension = s;
		SwappyVk_determineDeviceExtensions((VkPhysicalDevice)phyDevice, 1, &prop, &requiredExtensionCount, &requiredExtension);

		u64 refreshDuration;
		SwappyVk_initAndGetRefreshCycleDuration(jni::env(), g_activity, (VkPhysicalDevice)phyDevice, (VkDevice)device,
				(VkSwapchainKHR)swapchain, &refreshDuration);
		SwappyVk_isEnabled((VkSwapchainKHR)swapchain, &swappyAvailable);
		NOTICE_LOG(RENDERER, "SwappyVk status: %s", swappyAvailable ? "enabled" : "disabled");
		if (!swappyAvailable) {
			SwappyVk_destroySwapchain((VkDevice)device, (VkSwapchainKHR)swapchain);
			return;
		}
		GraphicsContext *gc = GraphicsContext::Instance();
		if (gc != nullptr)
		{
			void *window, *display;
			gc->getWindow(&window, &display);
			SwappyVk_setWindow((VkDevice)device, (VkSwapchainKHR)swapchain, (ANativeWindow *)window);
		}
		else {
			ERROR_LOG(RENDERER, "SwappyVk: Graphics context is null");
		}
		SwappyVk_setAutoSwapInterval(false);
		SwappyVk_setAutoPipelineMode(false);
		SwappyVk_setSwapIntervalNS((VkDevice)device, (VkSwapchainKHR)swapchain, SWAPPY_SWAP_60FPS);
	}
	else {
		if (swapchain)
			SwappyVk_destroySwapchain((VkDevice)device, (VkSwapchainKHR)swapchain);
	}
}

void init(vk::PhysicalDevice phyDevice, vk::Device device, vk::SwapchainKHR swapchain)
{
	swappyvk::phyDevice = phyDevice;
	swappyvk::device = device;
	swappyvk::swapchain = swapchain;
	activate(config::FramePacing);
}

void setSwapInterval(int i) {
	if (swappyAvailable)
		SwappyVk_setSwapIntervalNS((VkDevice)device, (VkSwapchainKHR)swapchain, SWAPPY_SWAP_60FPS * i);
}

vk::Result present(vk::Queue queue, const vk::PresentInfoKHR& presentInfo)
{
	activate(config::FramePacing);

	if (!swappyAvailable)
		return queue.presentKHR(presentInfo);

	bool swapOnVSync = !settings.input.fastForwardMode && config::VSync;
	SwappyVk_enableFramePacing((VkSwapchainKHR)swapchain, swapOnVSync);
	SwappyVk_enableBlockingWait((VkSwapchainKHR)swapchain, swapOnVSync);
	return static_cast<vk::Result>(SwappyVk_queuePresent((VkQueue)queue, (const VkPresentInfoKHR *)presentInfo));
}

void detachSwapchain() {
	activate(false);
}

void detachDevice()
{
	if (device)
	{
		SwappyVk_destroyDevice((VkDevice)device);
		swapchain = nullptr;
		device = nullptr;
		swappyAvailable = false;
	}
}

}	// namespace swappyvk
#endif // SWAPPY
