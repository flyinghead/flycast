/*
    This file is part of Flycast.
    Flycast is free software under the GNU General Public License, version 2
    or (at your option) any later version.
*/
#pragma once
#include <cstdint>

// Output-only companion to SDL input. Uses Apple's controller APIs over USB or Bluetooth.
class DualSenseGameControllerOutput {
public:
    DualSenseGameControllerOutput();
    ~DualSenseGameControllerOutput();
    bool connect();
    void setDrivingProfile(bool enabled);
    bool setRumble(float intensity, uint32_t durationMs);
    void update();

private:
    void* native = nullptr;
};
