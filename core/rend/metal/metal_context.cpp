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

#include "metal_context.h"
#include "metal_driver.h"
#include "sdl/sdl.h"
#include "ui/imgui_driver.h"

MetalContext *MetalContext::contextInstance;

bool MetalContext::init() {
    GraphicsContext::instance = this;

#if defined(USE_SDL)
    if (!sdl_recreate_window(SDL_WINDOW_METAL))
        return false;

    auto view = SDL_Metal_CreateView((SDL_Window *)window);

    if (view == nullptr) {
        term();
        ERROR_LOG(RENDERER, "Failed to create SDL Metal View");
        return false;
    }

    layer = static_cast<CA::MetalLayer *>(SDL_Metal_GetLayer(view));
#endif

    if (!device) {
        term();
        NOTICE_LOG(RENDERER, "Metal Device is null.");
        return false;
    }

    layer->setDevice(device);
    queue = device->newCommandQueue();
    commandBuffer = queue->commandBuffer();

    NOTICE_LOG(RENDERER, "Created Metal view.");

    imguiDriver = std::unique_ptr<ImGuiDriver>(new MetalDriver());
    return true;
}

void MetalContext::resize() {

}

void MetalContext::Present() {

}

void MetalContext::term() {
    GraphicsContext::instance = nullptr;
    imguiDriver.reset();
}

MetalContext::MetalContext() {
    verify(contextInstance == nullptr);
    contextInstance = this;
}

MetalContext::~MetalContext() {
    verify(contextInstance == this);
    contextInstance = nullptr;
}

