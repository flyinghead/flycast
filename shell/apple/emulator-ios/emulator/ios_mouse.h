//
//  ios_mouse.h
//  flycast
//
//  Created by Cameron Bates on 9/6/21.
//
#pragma once
#import <GameController/GameController.h>

#include "input/mouse.h"

class API_AVAILABLE(ios(14.0)) IOSMouse : public SystemMouse
{
public:
    IOSMouse(int port, GCMouse *mouse) : SystemMouse("iOS", port), gcMouse(mouse)
    {
        set_maple_port(port);
        loadMapping();

        [gcMouse.mouseInput setMouseMovedHandler:^(GCMouseInput * _Nonnull mouse, float deltaX, float deltaY) { 
            setRelPos(deltaX, -deltaY);
        }];

        [gcMouse.mouseInput.leftButton setValueChangedHandler:
         ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed) {
            setButton(Mouse::LEFT_BUTTON, pressed);
        }];
        
        [gcMouse.mouseInput.rightButton setValueChangedHandler:
         ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed) {
            setButton(Mouse::RIGHT_BUTTON, pressed);
        }];
        
        [gcMouse.mouseInput.middleButton setValueChangedHandler:
         ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed) {
            setButton(Mouse::MIDDLE_BUTTON, pressed);
        }];
        
        [gcMouse.mouseInput.scroll.yAxis setValueChangedHandler:^(GCControllerAxisInput * _Nonnull axis, float value) {
            setWheel(value);
        }];
    }

    void set_maple_port(int port) override
    {
        SystemMouse::set_maple_port(port);
    }

    static void addMouse(GCMouse *mouse)
    {
        if (mice.count(mouse) > 0)
            return;
        
        int port = std::min((int)mice.size(), 3);
        mice[mouse] = std::make_shared<IOSMouse>(port, mouse);
        SystemMouse::Register(mice[mouse]);
    }

    static void removeMouse(GCMouse *mouse)
    {
        auto it = mice.find(mouse);
        if (it == mice.end())
            return;
        SystemMouse::Unregister(it->second);
        mice.erase(it);
    }

private:
    GCMouse * __weak gcMouse = nullptr;
    static std::map<GCMouse *, std::shared_ptr<IOSMouse>> mice;
};
