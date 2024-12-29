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
package com.flycast.emulator.emu;

import android.content.Context;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;

import com.flycast.emulator.periph.InputDeviceManager;
import com.flycast.emulator.periph.VibratorThread;

import java.util.HashMap;
import java.util.Map;

public class VirtualJoystickDelegate implements TouchEventHandler
{
    private static final int CTLID_ANARING = 11;
    private static final int CTLID_ANASTICK = 12;

    private VibratorThread vibratorThread;
    private Handler handler = new Handler();
    private Runnable hideVGamepadRunnable = new Runnable() {
        @Override
        public void run() {
            VGamepad.hide();
        }
    };
    private Context context;
    private View view;
    private int joyPointerId = -1;
    private float joyBiasX, joyBiasY;
    private Map<Integer, Integer> pidToControlId = new HashMap<>();
    private int mouseButtons = 0;
    private int[] mousePos = { -32768, -32768 };
    private int mousePid = -1;

    public VirtualJoystickDelegate(View view) {
        this.view = view;
        this.context = view.getContext();

        vibratorThread = VibratorThread.getInstance();
    }

    @Override
    public void stop() {
        vibratorThread.stopThread();
        vibratorThread = null;
    }

    private boolean touchMouseEvent(MotionEvent event)
    {
        int actionMasked = event.getActionMasked();
        int actionIndex = event.getActionIndex();
        switch (actionMasked)
        {
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                mousePid = -1;
                mouseButtons = 0;
                InputDeviceManager.getInstance().touchMouseEvent(mousePos[0], mousePos[1], mouseButtons);
                return true;

            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_DOWN:
                if (mousePid == -1 || actionMasked == MotionEvent.ACTION_DOWN)
                {
                    mousePid = event.getPointerId(actionIndex);
                    mousePos[0] = Math.round(event.getX(actionIndex));
                    mousePos[1] = Math.round(event.getY(actionIndex));
                    mouseButtons = MotionEvent.BUTTON_PRIMARY;    // Mouse left button down
                    InputDeviceManager.getInstance().touchMouseEvent(mousePos[0], mousePos[1], mouseButtons);
                    return true;
                }
                return false;

            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++)
                {
                    if (event.getPointerId(i) == mousePid) {
                        mousePos[0] = Math.round(event.getX(i));
                        mousePos[1] = Math.round(event.getY(i));
                        InputDeviceManager.getInstance().touchMouseEvent(mousePos[0], mousePos[1], mouseButtons);
                        break;
                    }
                }
                break;
            case MotionEvent.ACTION_POINTER_UP:
                if (event.getPointerId(actionIndex) == mousePid)
                {
                    mousePid = -1;
                    mousePos[0] = Math.round(event.getX(actionIndex));
                    mousePos[1] = Math.round(event.getY(actionIndex));
                    mouseButtons = 0;
                    InputDeviceManager.getInstance().touchMouseEvent(mousePos[0], mousePos[1], mouseButtons);
                    return true;
                }
                break;
            default:
                break;
        }

        return false;
    }

    static class Point
    {
        Point() {
            this.x = 0.f;
            this.y = 0.f;
        }
        Point(float x, float y) {
            this.x = x;
            this.y = y;
        }
        float x;
        float y;
    }

    static Point translateCoords(Point pos, Point size)
    {
        float hscale = 480.f / size.y;
        Point p = new Point();
        p.y = pos.y * hscale;
        p.x = (pos.x - (size.x - 640.f / hscale) / 2.f) * hscale;
        return p;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event, int width, int height)
    {
        if (JNIdc.guiIsOpen())
            return touchMouseEvent(event);
        show();

        int actionIndex = event.getActionIndex();
        switch (event.getActionMasked())
        {
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                // Release all
                pidToControlId.clear();
                joyPointerId = -1;
                InputDeviceManager.getInstance().virtualReleaseAll();
                break;

            case MotionEvent.ACTION_DOWN:
                // First release all
                pidToControlId.clear();
                joyPointerId = -1;
                InputDeviceManager.getInstance().virtualReleaseAll();
                // Release the mouse too
                mousePid = -1;
                mouseButtons = 0;
                InputDeviceManager.getInstance().touchMouseEvent(mousePos[0], mousePos[1], mouseButtons);
                // Then fall through
            case MotionEvent.ACTION_POINTER_DOWN:
            {
                Point p = new Point(event.getX(actionIndex), event.getY(actionIndex));
                p = translateCoords(p, new Point(width, height));
                int control = VGamepad.hitTest(p.x, p.y);
                if (control != -1)
                {
                    int pid = event.getPointerId(actionIndex);
                    if (control == CTLID_ANARING || control == CTLID_ANASTICK)
                    {
                         if (joyPointerId == -1)
                         {
                             // Analog stick down
                             joyPointerId = pid;
                             joyBiasX = p.x;
                             joyBiasY = p.y;
                             InputDeviceManager.getInstance().virtualJoystick(0, 0);
                             return true;
                         }
                    }
                    else
                    {
                        // Button down
                        InputDeviceManager.getInstance().virtualButtonInput(control, true);
                        pidToControlId.put(pid, control);
                        vibratorThread.click();
                        return true;
                    }
                }
                break;
            }

            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++)
                {
                    int pid = event.getPointerId(i);
                    Point p = new Point(event.getX(i), event.getY(i));
                    p = translateCoords(p, new Point(width, height));
                    if (joyPointerId == pid)
                    {
                        // Analog stick
                        float dx = p.x - joyBiasX;
                        float dy = p.y - joyBiasY;
                        float sz = VGamepad.getControlWidth(CTLID_ANASTICK);
                        dx = Math.max(Math.min(1.f, dx / sz), -1.f);
                        dy = Math.max(Math.min(1.f, dy / sz), -1.f);
                        InputDeviceManager.getInstance().virtualJoystick(dx, dy);
                        continue;
                    }
                    // Buttons
                    int control = VGamepad.hitTest(p.x, p.y);
                    int oldControl = pidToControlId.containsKey(pid) ? pidToControlId.get(pid) : -1;
                    if (oldControl == control)
                        // same button still pressed, or none at all
                        continue;
                    if (oldControl != -1) {
                        // Previous button up
                        InputDeviceManager.getInstance().virtualButtonInput(oldControl, false);
                        pidToControlId.remove(pid);
                    }
                    if (control != -1 && control != CTLID_ANARING && control != CTLID_ANASTICK)
                    {
                        // New button down
                        InputDeviceManager.getInstance().virtualButtonInput(control, true);
                        pidToControlId.put(pid, control);
                        vibratorThread.click();
                    }
                }
                break;

            case MotionEvent.ACTION_POINTER_UP:
            {
                int pid = event.getPointerId(actionIndex);
                if (joyPointerId == pid)
                {
                    // Analog up
                    InputDeviceManager.getInstance().virtualJoystick(0, 0);
                    joyPointerId = -1;
                    return true;
                }
                if (pidToControlId.containsKey(pid))
                {
                    // Button up
                    int controlId = pidToControlId.get(pid);
                    InputDeviceManager.getInstance().virtualButtonInput(controlId, false);
                    return true;
                }
                break;
            }
        }
        return touchMouseEvent(event);
    }

    @Override
    public void show()
    {
        VGamepad.show();
        this.handler.removeCallbacks(hideVGamepadRunnable);
        this.handler.postDelayed(hideVGamepadRunnable, 10000);
    }
}
