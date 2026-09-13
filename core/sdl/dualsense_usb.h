/*
    This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/
#pragma once

#include <IOKit/hid/IOHIDManager.h>
#include <chrono>
#include <cstdint>
#include <mutex>

// Wired DualSense output used by the macOS SDL gamepad path. Input still uses SDL.
class DualSenseUSBOutput {
public:
    DualSenseUSBOutput();
    ~DualSenseUSBOutput();

    bool connect();
    void setDrivingProfile(bool enabled);
    void setRumble(uint8_t strength, uint32_t durationMs);
    void update();
    void close();

private:
    using Clock = std::chrono::steady_clock;

    bool sendReport(bool brake, bool accelerator, uint8_t rumble);
    static void encodeFeedback(const uint8_t (&strengths)[10], uint8_t* destination);
    static int number(IOHIDDeviceRef device, CFStringRef key);

    IOHIDManagerRef manager = nullptr;
    IOHIDDeviceRef device = nullptr;
    std::mutex mutex;
    bool drivingProfile = false;
    uint8_t rumble = 0;
    Clock::time_point rumbleUntil{};
    Clock::time_point lastReport{};
};
