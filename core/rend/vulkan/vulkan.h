/*
    Created on: Oct 2, 2019

	Copyright 2019 flyinghead

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
#include "types.h"

#ifdef LIBRETRO
#include <vulkan/vulkan_symbol_wrapper.h>

static inline VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *name) {
	return (*vulkan_symbol_wrapper_instance_proc_addr())(instance, name);
}

#elif !defined(TARGET_IPHONE)
#include "volk/volk.h"
#endif

#if !defined(TARGET_IPHONE)
#undef VK_NO_PROTOTYPES
#endif
#include "vulkan/vulkan.hpp"

//#define VK_DEBUG
