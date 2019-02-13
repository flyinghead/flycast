package com.reicast.emulator.periph;

import android.content.Context;
import android.hardware.input.InputManager;
import android.util.Log;
import android.view.InputDevice;

public final class InputDeviceManager implements InputManager.InputDeviceListener {
    public static final int VIRTUAL_GAMEPAD_ID = 0x12345678;

    static { System.loadLibrary("dc"); }
    private static final InputDeviceManager INSTANCE = new InputDeviceManager();
    private InputManager inputManager;

    public void startListening(Context applicationContext)
    {
        joystickAdded(VIRTUAL_GAMEPAD_ID, "Virtual Gamepad");
        int[] ids = InputDevice.getDeviceIds();
        for (int id : ids)
            onInputDeviceAdded(id);
        inputManager = (InputManager)applicationContext.getSystemService(Context.INPUT_SERVICE);
        inputManager.registerInputDeviceListener(this, null);
    }

    public void stopListening()
    {
        inputManager.unregisterInputDeviceListener(this);
        inputManager = null;
        joystickRemoved(VIRTUAL_GAMEPAD_ID);
    }

    @Override
    public void onInputDeviceAdded(int i) {
        InputDevice device = InputDevice.getDevice(i);
        if ((device.getSources() & InputDevice.SOURCE_CLASS_BUTTON) == InputDevice.SOURCE_CLASS_BUTTON) {
            joystickAdded(i, device.getName());
        }
    }

    @Override
    public void onInputDeviceRemoved(int i) {
        joystickRemoved(i);
    }

    @Override
    public void onInputDeviceChanged(int i) {
    }

    public static InputDeviceManager getInstance() {
        return INSTANCE;
    }

    public native void virtualGamepadEvent(int kcode, int joyx, int joyy, int lt, int rt);
    public native boolean joystickButtonEvent(int id, int button, boolean pressed);
    public native boolean joystickAxisEvent(int id, int button, int value);
    public native void mouseEvent(int xpos, int ypos, int buttons);
    private native void joystickAdded(int id, String name);
    private native void joystickRemoved(int id);
}
