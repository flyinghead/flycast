package com.flycast.emulator.periph;

import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.InputDevice;

import com.flycast.emulator.Emulator;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.lang3.ArrayUtils;

public final class InputDeviceManager implements InputManager.InputDeviceListener {
    public static final int VIRTUAL_GAMEPAD_ID = 0x12345678;

    static { System.loadLibrary("flycast"); }
    private static final InputDeviceManager INSTANCE = new InputDeviceManager();
    private InputManager inputManager;
    private int maple_port = 0;

    private static class VibrationParams {
        float power;
        float inclination;
        long stopTime;
    }
    private Map<Integer, VibrationParams> vibParams = new HashMap<>();

    public InputDeviceManager()
    {
        init();
    }

    public void startListening(Context applicationContext)
    {
        maple_port = 0;
        if (applicationContext.getPackageManager().hasSystemFeature("android.hardware.touchscreen"))
            joystickAdded(VIRTUAL_GAMEPAD_ID, "Virtual Gamepad", 0, "virtual_gamepad_uid",
                    new int[0], new int[0], getVibrator(VIRTUAL_GAMEPAD_ID) != null);
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
        if (device != null && (device.getSources() & InputDevice.SOURCE_CLASS_BUTTON) == InputDevice.SOURCE_CLASS_BUTTON) {
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
                    ArrayUtils.toPrimitive(fullAxes.toArray(new Integer[0])), ArrayUtils.toPrimitive(halfAxes.toArray(new Integer[0])),
                    getVibrator(i) != null);
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

    private Vibrator getVibrator(int i) {
        if (i == VIRTUAL_GAMEPAD_ID) {
            return (Vibrator)Emulator.getAppContext().getSystemService(Context.VIBRATOR_SERVICE);
        }
        else {
            InputDevice device = InputDevice.getDevice(i);
            if (device == null)
                return null;
            Vibrator vibrator = device.getVibrator();
            return vibrator.hasVibrator() ? vibrator : null;
        }
    }

    // Called from native code
    private boolean rumble(int i, float power, float inclination, int duration_ms)
    {
        Vibrator vibrator = getVibrator(i);
        if (vibrator == null)
            return false;

        VibrationParams params;
        synchronized (this) {
            params = vibParams.get(i);
            if (params == null) {
                params = new VibrationParams();
                vibParams.put(i, params);
            }
        }
        if (power == 0) {
            if (params.power != 0)
                vibrator.cancel();
        } else {
            if (inclination > 0) {
                params.inclination = inclination * power;
                params.stopTime = System.currentTimeMillis() + duration_ms;
            }
            else {
                params.inclination = 0;
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                vibrator.vibrate(VibrationEffect.createOneShot(duration_ms, (int)(power * 255)));
            else
                vibrator.vibrate(duration_ms);
            if (inclination > 0)
                VibratorThread.getInstance().setVibrating();
        }
        params.power = power;

        return true;
    }

    public boolean updateRumble()
    {
        List<Integer> ids;
        synchronized (this) {
            ids = new ArrayList<Integer>(vibParams.keySet());
        }
        boolean active = false;
        for (int id : ids) {
            if (updateRumble(id))
                active = true;
        }
        return active;
    }

    private boolean updateRumble(int i)
    {
        Vibrator vibrator = getVibrator(i);
        VibrationParams params;
        synchronized (this) {
            params = vibParams.get(i);
        }
        if (vibrator == null || params == null || params.power == 0 || params.inclination == 0)
            return false;
        long remTime = params.stopTime - System.currentTimeMillis();
        if (remTime <= 0) {
            params.power = 0;
            params.inclination = 0;
            vibrator.cancel();
            return false;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            vibrator.vibrate(VibrationEffect.createOneShot(remTime, (int)(params.inclination * remTime * 255)));
        else
            vibrator.vibrate(remTime);
        return true;
    }

    public void stopRumble()
    {
        List<Integer> ids;
        synchronized (this) {
            ids = new ArrayList<Integer>(vibParams.keySet());
        }
        for (int id : ids) {
            Vibrator vibrator = getVibrator(id);
            if (vibrator != null)
                vibrator.cancel();
        }
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
    private native void joystickAdded(int id, String name, int maple_port, String uniqueId, int[] fullAxes, int[] halfAxes, boolean rumbleEnabled);
    private native void joystickRemoved(int id);
    public native boolean keyboardEvent(int key, boolean pressed);
    public native void keyboardText(int c);
}
