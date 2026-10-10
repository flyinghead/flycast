/*
    This file is part of Flycast.
    Flycast is free software under the GNU General Public License, version 2
    or (at your option) any later version.
*/
#pragma once
#include <cstdint>
#include <chrono>

// Output-only companion to SDL input. Uses Apple's controller APIs over USB or Bluetooth.
class DualSenseGameControllerOutput {
public:
    DualSenseGameControllerOutput();
    ~DualSenseGameControllerOutput();
    bool connect();
    void setDrivingProfile(bool enabled);
    bool setRumble(float intensity, uint32_t durationMs);
    // Returns true when a pending controller becomes available.
    bool update();
    bool isConnected() const { return native != nullptr; }

private:
    void* native = nullptr;
    std::chrono::steady_clock::time_point nextConnectAttempt{};
};
