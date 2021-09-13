//
//  ios.h
//  flycast
//
//  Created by Cameron Bates on 9/6/21.
//
#pragma once
#import <GameController/GameController.h>

#include "input/gamepad_device.h"

class IOSMouse : public SystemMouse
{
public:
    IOSMouse(int port, GCMouse *mouse) : SystemMouse("iOS", port), gcMouse(mouse)
    {
        set_maple_port(port);
        loadMapping();
        
//        [gcMouse.mouseInput set]
    }
    
    void set_maple_port(int port) override
    {
        GamepadDevice::set_maple_port(port);
    }
    
    static void addMouse(GCMouse *mouse)
    {
        if (mice.count(mouce) > 0)
            return;
        
        int port = std::mind((int)mice.size(), 3);
        mice[mouse] = std::make_shared<IOSMouse>(port, keyboard);
        GamepadDevice::Register(mice[mouse]);
    }
    
    static void removeMouse(GCMouse *mouse)
    {
        auto it = mice.find(mouse);
        if (it == mice.end())
            return;
        GamepadDevice::Unregister(it->second);
        mice.erase(it);
    }

private:
    GCMouse * __weak gcMouse = nullptr;
    static std::map<GCMouse *, std::shared_ptr<IOSMouse>> mice;
};
