package com.reicast.emulator.periph;

import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Vibrator;
import android.view.InputDevice;

import com.reicast.emulator.Emulator;

import java.util.ArrayList;
import java.util.List;
import org.apache.commons.lang3.ArrayUtils;

public final class InputDeviceManager implements InputManager.InputDeviceListener {
    public static final int VIRTUAL_GAMEPAD_ID = 0x12345678;

    static { System.loadLibrary("flycast"); }
    private static final InputDeviceManager INSTANCE = new InputDeviceManager();
    private InputManager inputManager;
    private int maple_port = 0;

    public InputDeviceManager()
    {
        init();
    }

    public void startListening(Context applicationContext)
    {
        maple_port = 0;
        if (applicationContext.getPackageManager().hasSystemFeature("android.hardware.touchscreen"))
            joystickAdded(VIRTUAL_GAMEPAD_ID, "Virtual Gamepad", 0, "virtual_gamepad_uid", new int[0], new int[0]);
        int[] ids = InputDevice.getDeviceIds();
        for (int id : ids)
            onInputDeviceAdded(id);
        inputManager = (InputManager)applicationContext.getSystemService(Context.INPUT_SERVICE);
        inputManager.registerInputDeviceListener(this, null);
    }

    public void stopListening()
    {
        if (inputManager != null) {
            inputManager.unregisterInputDeviceListener(this);
            inputManager = null;
        }
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
            List<InputDevice.MotionRange> axes = device.getMotionRanges();
            List<Integer> fullAxes = new ArrayList<>();
            List<Integer> halfAxes = new ArrayList<>();
            for (InputDevice.MotionRange range : axes) {
                if (range.getMin() == 0)
                    halfAxes.add(range.getAxis());
                else
                    fullAxes.add(range.getAxis());
            }
            joystickAdded(i, device.getName(), port, device.getDescriptor(),
                    ArrayUtils.toPrimitive(fullAxes.toArray(new Integer[0])), ArrayUtils.toPrimitive(halfAxes.toArray(new Integer[0])));
        }
    }

    @Override
    public void onInputDeviceRemoved(int i) {
        if (maple_port > 0)
            maple_port--;
        joystickRemoved(i);
    }

    @Override
    public void onInputDeviceChanged(int i) {
    }

    // Called from native code
    private boolean rumble(int i, float power, float inclination, int duration_ms) {
        Vibrator vibrator;
        if (i == VIRTUAL_GAMEPAD_ID) {
            vibrator = (Vibrator)Emulator.getAppContext().getSystemService(Context.VIBRATOR_SERVICE);
        }
        else {
            InputDevice device = InputDevice.getDevice(i);
            if (device == null)
                return false;
            vibrator = device.getVibrator();
            if (!vibrator.hasVibrator())
                return false;
        }
        // TODO API >= 26 (Android 8.0)
        if (power == 0)
            vibrator.cancel();
        else
            vibrator.vibrate(duration_ms);

        return true;
    }

    public static InputDeviceManager getInstance() {
        return INSTANCE;
    }

    public native void init();
    public native void virtualGamepadEvent(int kcode, int joyx, int joyy, int lt, int rt, boolean fastForward);
    public native boolean joystickButtonEvent(int id, int button, boolean pressed);
    public native boolean joystickAxisEvent(int id, int button, int value);
    public native void mouseEvent(int xpos, int ypos, int buttons);
    public native void mouseScrollEvent(int scrollValue);
    private native void joystickAdded(int id, String name, int maple_port, String uniqueId, int fullAxes[], int halfAxes[]);
    private native void joystickRemoved(int id);
}
