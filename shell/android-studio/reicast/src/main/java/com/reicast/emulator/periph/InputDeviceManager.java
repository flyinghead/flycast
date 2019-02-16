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
    private int maple_port = 0;

    public void startListening(Context applicationContext)
    {
        if (applicationContext.getPackageManager().hasSystemFeature("android.hardware.touchscreen"))
            joystickAdded(VIRTUAL_GAMEPAD_ID, "Virtual Gamepad", maple_port == 3 ? 3 : maple_port++);
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
            int port = 0;
            if ((device.getSources() & InputDevice.SOURCE_CLASS_JOYSTICK) == InputDevice.SOURCE_CLASS_JOYSTICK) {
                port = this.maple_port == 3 ? 3 : this.maple_port++;
            }
            joystickAdded(i, device.getName(), port);
        }
    }

    @Override
    public void onInputDeviceRemoved(int i) {
        if (maple_port > 0)
            // TODO The removed device might not be a gamepad/joystick
            maple_port--;
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
    private native void joystickAdded(int id, String name, int maple_port);
    private native void joystickRemoved(int id);
}
