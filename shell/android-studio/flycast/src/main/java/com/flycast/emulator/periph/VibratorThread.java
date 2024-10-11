/*
	Copyright 2024 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
package com.flycast.emulator.periph;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.InputDevice;

import androidx.annotation.RequiresApi;

import com.flycast.emulator.Emulator;

public class VibratorThread extends Thread {
    private boolean stopping = false;
    private boolean click = false;
    private long nextRumbleUpdate = 0;
    @RequiresApi(Build.VERSION_CODES.O)
    private VibrationEffect clickEffect = null;
    int clickDuration = 0;
    private static VibratorThread INSTANCE = null;
    private static final int LEGACY_VIBRATION_DURATION = 20; // ms

    public static VibratorThread getInstance() {
        synchronized (VibratorThread.class) {
            if (INSTANCE == null)
                INSTANCE = new VibratorThread();
        }
        return INSTANCE;
    }

    private VibratorThread() {
        start();
    }

    private Vibrator getVibrator(int i)
    {
        if (i == InputDeviceManager.VIRTUAL_GAMEPAD_ID) {
            if (Emulator.vibrationPower > 0)
                return (Vibrator) Emulator.getAppContext().getSystemService(Context.VIBRATOR_SERVICE);
            else
                // vibration disabled
                return null;
        }
        else {
            InputDevice device = InputDevice.getDevice(i);
            if (device == null)
                return null;
            Vibrator vibrator = device.getVibrator();
            return vibrator.hasVibrator() ? vibrator : null;
        }
    }

    @Override
    public void run()
    {
        while (!stopping)
        {
            boolean doClick = false;
            synchronized (this) {
                try {
                    if (nextRumbleUpdate != 0) {
                        long waitTime = nextRumbleUpdate - System.currentTimeMillis();
                        if (waitTime > 0)
                            this.wait(waitTime);
                    }
                    else {
                        this.wait();
                    }
                } catch (InterruptedException e) {
                }
                if (click) {
                    doClick = true;
                    click = false;
                }
            }
            if (doClick)
                doClick();
            if (nextRumbleUpdate != 0 && nextRumbleUpdate - System.currentTimeMillis() < 5) {
                if (!InputDeviceManager.getInstance().updateRumble())
                    nextRumbleUpdate = 0;
                else
                    nextRumbleUpdate = System.currentTimeMillis() + 16667;
            }
        }
        InputDeviceManager.getInstance().stopRumble();
    }

    public void stopThread() {
        synchronized (this) {
            stopping = true;
            notify();
        }
        try {
            join();
        } catch (InterruptedException e) {
        }
        INSTANCE = null;
    }

    public void click() {
        if (Emulator.vibrationPower > 0) {
            synchronized (this) {
                click = true;
                notify();
            }
        }
    }

    private void doClick()
    {
        Vibrator vibrator = getVibrator(InputDeviceManager.VIRTUAL_GAMEPAD_ID);
        if (vibrator == null)
            return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
        {
            if (clickEffect == null)
            {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
                    clickEffect = VibrationEffect.createPredefined(VibrationEffect.EFFECT_HEAVY_CLICK);
                else
                    clickEffect = VibrationEffect.createOneShot(LEGACY_VIBRATION_DURATION, VibrationEffect.DEFAULT_AMPLITUDE);
            }
            vibrator.vibrate(clickEffect);
        } else {
            vibrator.vibrate(LEGACY_VIBRATION_DURATION);
        }
    }

    public void setVibrating()
    {
        // FIXME possible race condition
        synchronized (this) {
            nextRumbleUpdate = 1;
            notify();
        }
    }
}
