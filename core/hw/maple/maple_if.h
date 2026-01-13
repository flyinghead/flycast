#pragma once
#include "maple_devs.h"
#include <memory>

extern std::shared_ptr<maple_device> MapleDevices[MAPLE_PORTS][6];

void maple_Init();
void maple_Reset(bool Manual);
void maple_Term();
void maple_ReconnectDevices();

void maple_vblank();
void maple_pre_serialize();
