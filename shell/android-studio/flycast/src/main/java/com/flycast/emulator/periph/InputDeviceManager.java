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
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.commons.lang3.ArrayUtils;

public final class InputDeviceManager implements InputManager.InputDeviceListener {
    public static final int VIRTUAL_GAMEPAD_ID = 0x12345678;

    static { System.loadLibrary("flycast"); }
    private static final InputDeviceManager INSTANCE = new InputDeviceManager();
    private InputManager inputManager;
    private int maple_port = 0;

    private boolean hasTouchscreen = false;

    private static class VibrationParams {
        float power;
        float inclination;
        long stopTime;
    }
    private Map<Integer, VibrationParams> vibParams = new HashMap<>();
    private Set<Integer> knownDevices = new HashSet<>();

    public InputDeviceManager()
    {
        init();
    }

    public void startListening(Context applicationContext)
    {
        maple_port = 0;
        hasTouchscreen = applicationContext.getPackageManager().hasSystemFeature("android.hardware.touchscreen");
        if (hasTouchscreen)
            joystickAdded(VIRTUAL_GAMEPAD_ID, null, 0, null,
                    null, null, getVibrator(VIRTUAL_GAMEPAD_ID) != null);
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
    }

    @Override
    public void onInputDeviceRemoved(int i) {
        if (maple_port > 0)
            maple_port--;
        joystickRemoved(i);
        knownDevices.remove(i);
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

    private void vibrate(Vibrator vibrator, long duration_ms, float power)
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            int ipow = Math.min((int)(power * 255), 255);
            if (ipow >= 1)
                vibrator.vibrate(VibrationEffect.createOneShot(duration_ms, ipow));
            else
                vibrator.cancel();
        }
        else
            vibrator.vibrate(duration_ms);
    }

    // Called from native code
    // returns false if the device has no vibrator
    private boolean rumble(int i, float power, float inclination, int duration_ms)
    {
        Vibrator vibrator = getVibrator(i);
        if (vibrator == null)
            return false;
        if (i == VIRTUAL_GAMEPAD_ID) {
            if (Emulator.vibrationPower == 0)
                return true;
            power *= Emulator.vibrationPower / 100.f;
        }

        VibrationParams params;
        synchronized (this) {
            params = vibParams.get(i);
            if (params == null) {
                params = new VibrationParams();
                vibParams.put(i, params);
            }
        }
        if (power != 0) {
            params.stopTime = System.currentTimeMillis() + duration_ms;
            if (inclination > 0)
                params.inclination = inclination * power;
            else
                params.inclination = 0;
        }
        params.power = power;
        VibratorThread.getInstance().setVibrating();

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
        if (vibrator == null || params == null)
            return false;
        long remTime = params.stopTime - System.currentTimeMillis();
        if (remTime <= 0 || params.power == 0) {
            params.power = 0;
            params.inclination = 0;
            vibrator.cancel();
            return false;
        }
        if (params.inclination > 0)
            vibrate(vibrator, remTime, params.inclination * remTime);
        else
            vibrate(vibrator, remTime, params.power);
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

    public boolean hasTouchscreen() {
        return hasTouchscreen;
    }

    private boolean createDevice(int id)
    {
        if (id == 0)
            return false;
        if (knownDevices.contains(id))
            return true;
        InputDevice device = InputDevice.getDevice(id);
        if (device == null || (device.getSources() & InputDevice.SOURCE_CLASS_BUTTON) != InputDevice.SOURCE_CLASS_BUTTON)
            return false;
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
        joystickAdded(id, device.getName(), port, device.getDescriptor(),
                ArrayUtils.toPrimitive(fullAxes.toArray(new Integer[0])), ArrayUtils.toPrimitive(halfAxes.toArray(new Integer[0])),
                getVibrator(id) != null);
        knownDevices.add(id);
        return true;
    }
    public boolean buttonEvent(int id, int button, boolean pressed)
    {
        if (!createDevice(id))
            return false;
        return joystickButtonEvent(id, button, pressed);
    }
    public boolean axisEvent(int id, int button, int value)
    {
        if (!createDevice(id))
            return false;
        return joystickAxisEvent(id, button, value);
    }

    public static InputDeviceManager getInstance() {
        return INSTANCE;
    }

    public native void init();
    public native void virtualReleaseAll();
    public native void virtualJoystick(float x, float y);
    public native void virtualButtonInput(int key, boolean pressed);
    private native boolean joystickButtonEvent(int id, int button, boolean pressed);
    private native boolean joystickAxisEvent(int id, int button, int value);
    public native void mouseEvent(int xpos, int ypos, int buttons);
    public native void mouseScrollEvent(int scrollValue);
    public native void touchMouseEvent(int xpos, int ypos, int buttons);
    private native void joystickAdded(int id, String name, int maple_port, String uniqueId, int[] fullAxes, int[] halfAxes, boolean rumbleEnabled);
    private native void joystickRemoved(int id);
    public native boolean keyboardEvent(int key, boolean pressed);
    public native void keyboardText(int c);
    public static native boolean isMicPluggedIn();
}
