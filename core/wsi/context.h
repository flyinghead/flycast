/*
    Created on: Oct 18, 2019

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
#include "gl_context.h"
#ifdef USE_VULKAN
#include "rend/vulkan/vulkan_context.h"

extern VulkanContext theVulkanContext;
#endif
void InitRenderApi();
void TermRenderApi();

static inline void SwitchRenderApi()
{
	TermRenderApi();
	InitRenderApi();
}

