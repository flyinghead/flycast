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
#pragma once
#include "vulkan.h"

namespace swappyvk
{
#ifdef SWAPPY

void setQueueFamilyIndex(bool googleTimingSupported, vk::Device device, vk::Queue queue, u32 queueFamilyIndex);
void init(vk::PhysicalDevice phyDevice, vk::Device device, vk::SwapchainKHR swapchain);
void setSwapInterval(int i);
vk::Result present(vk::Queue queue, const vk::PresentInfoKHR& presentInfo);
void detachSwapchain();
void detachDevice();

#else

static inline void setQueueFamilyIndex(bool googleTimingSupported, vk::Device device, vk::Queue queue, u32 queueFamilyIndex) {}
static inline void init(vk::PhysicalDevice phyDevice, vk::Device device, vk::SwapchainKHR swapchain) {}
static inline void setSwapInterval(int i) {}

static inline vk::Result present(vk::Queue queue, const vk::PresentInfoKHR& presentInfo) {
	return queue.presentKHR(presentInfo);
}

static inline void detachSwapchain() {}
static inline void detachDevice() {}

#endif
}
