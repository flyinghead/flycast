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
#include "dualsense_usb.h"
#include <array>
#include <vector>

DualSenseUSBOutput::DualSenseUSBOutput()
{
    manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (manager != nullptr) {
        // Restrict enumeration to Sony devices so this output-only path never
        // attempts to monitor the Mac's keyboard or other input devices.
        CFMutableDictionaryRef matching = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        const int vendor = 0x054c;
        CFNumberRef vendorNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendor);
        CFDictionarySetValue(matching, CFSTR(kIOHIDVendorIDKey), vendorNumber);
        IOHIDManagerSetDeviceMatching(manager, matching);
        CFRelease(vendorNumber);
        CFRelease(matching);
        IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        (void)IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    }
}

DualSenseUSBOutput::~DualSenseUSBOutput()
{
    close();
    if (manager != nullptr) {
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        IOHIDManagerUnscheduleFromRunLoop(manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        CFRelease(manager);
    }
}

int DualSenseUSBOutput::number(IOHIDDeviceRef candidate, CFStringRef key)
{
    CFTypeRef value = IOHIDDeviceGetProperty(candidate, key);
    int result = 0;
    if (value != nullptr && CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberIntType, &result);
    return result;
}

bool DualSenseUSBOutput::connect()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (manager == nullptr)
        return false;
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (devices == nullptr)
        return false;
    std::vector<const void*> candidates(CFSetGetCount(devices));
    if (!candidates.empty())
        CFSetGetValues(devices, candidates.data());
    for (const void* candidateValue : candidates) {
        auto candidate = static_cast<IOHIDDeviceRef>(const_cast<void*>(candidateValue));
        const int product = number(candidate, CFSTR(kIOHIDProductIDKey));
        CFTypeRef transport = IOHIDDeviceGetProperty(candidate, CFSTR(kIOHIDTransportKey));
        if (number(candidate, CFSTR(kIOHIDVendorIDKey)) != 0x054c
            || (product != 0x0ce6 && product != 0x0df2)
            || transport == nullptr || CFGetTypeID(transport) != CFStringGetTypeID()
            || CFStringCompare(static_cast<CFStringRef>(transport), CFSTR("USB"), 0) != kCFCompareEqualTo)
            continue;
        if (IOHIDDeviceOpen(candidate, kIOHIDOptionsTypeNone) == kIOReturnSuccess) {
            device = candidate;
            CFRetain(device);
            break;
        }
    }
    CFRelease(devices);
    if (device == nullptr)
        return false;
    if (!sendReport(false, false, 0)) {
        IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
        CFRelease(device);
        device = nullptr;
        return false;
    }
    lastReport = Clock::now();
    return true;
}

void DualSenseUSBOutput::encodeFeedback(const uint8_t (&strengths)[10], uint8_t* output)
{
    uint16_t activeZones = 0;
    uint32_t packedStrengths = 0;
    for (int zone = 0; zone < 10; ++zone) {
        if (strengths[zone] == 0)
            continue;
        activeZones |= uint16_t(1) << zone;
        packedStrengths |= uint32_t(strengths[zone] - 1) << (zone * 3);
    }
    output[0] = 0x21;
    output[1] = activeZones & 0xff;
    output[2] = activeZones >> 8;
    for (int byte = 0; byte < 4; ++byte)
        output[3 + byte] = packedStrengths >> (byte * 8);
}

bool DualSenseUSBOutput::sendReport(bool brake, bool accelerator, uint8_t vibration)
{
    if (device == nullptr)
        return false;
    std::array<uint8_t, 48> report{};
    report[0] = 0x02;
    report[1] = 0x0f;
    report[3] = vibration;
    report[4] = vibration;
    if (accelerator) {
        const uint8_t strengths[10] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5};
        encodeFeedback(strengths, report.data() + 11);
    } else {
        report[11] = 0x05;
    }
    if (brake) {
        const uint8_t strengths[10] = {0, 2, 3, 4, 5, 6, 7, 8, 8, 8};
        encodeFeedback(strengths, report.data() + 22);
    } else {
        report[22] = 0x05;
    }
    return IOHIDDeviceSetReport(device, kIOHIDReportTypeOutput, 0x02,
        report.data(), report.size()) == kIOReturnSuccess;
}

void DualSenseUSBOutput::setDrivingProfile(bool enabled)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (device == nullptr || drivingProfile == enabled)
        return;
    drivingProfile = enabled;
    sendReport(enabled, enabled, rumble);
    lastReport = Clock::now();
}

void DualSenseUSBOutput::setRumble(uint8_t strength, uint32_t durationMs)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (device == nullptr)
        return;
    rumble = strength;
    rumbleUntil = Clock::now() + std::chrono::milliseconds(durationMs);
    sendReport(drivingProfile, drivingProfile, rumble);
    lastReport = Clock::now();
}

void DualSenseUSBOutput::update()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (device == nullptr)
        return;
    const auto now = Clock::now();
    if (rumble != 0 && now >= rumbleUntil)
        rumble = 0;
    if ((drivingProfile || rumble != 0) && now - lastReport >= std::chrono::milliseconds(500)) {
        sendReport(drivingProfile, drivingProfile, rumble);
        lastReport = now;
    }
}

void DualSenseUSBOutput::close()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (device != nullptr) {
        sendReport(false, false, 0);
        IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
        CFRelease(device);
        device = nullptr;
    }
}
