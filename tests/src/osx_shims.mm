/* Minimal Objective-C++ shims for macOS test runner.
   These satisfy Flycast core's mac-specific externs without
   dragging in SDL, Syphon, Vulkan-Metal, or Cocoa UI code. */
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

// Simple console printer – drop colours.
int darw_printf(const char* text, ...)
{
    va_list args; va_start(args, text);
    vprintf(text, args);
    va_end(args);
    return 0;
}

void os_DoEvents() {}
void os_RunInstance(int, const char**) {}

// Video-routing helpers – no-ops in head-less test runner
void os_VideoRoutingTermGL() {}
void os_VideoRoutingTermVk() {}
void os_VideoRoutingPublishFrameTexture(unsigned int, unsigned int, float, float) {}

namespace vk { class Device; class Image; class Queue; }
void os_VideoRoutingPublishFrameTexture(const vk::Device&, const vk::Image&, const vk::Queue&, float, float, float, float) {}

std::string os_Locale() { return "en"; }
std::string os_PrecomposedString(std::string s) { return s; }

namespace hostfs {
std::string getScreenshotsPath() { return "/tmp"; }
}
