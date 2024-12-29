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

import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;

import androidx.annotation.NonNull;

public class EditVirtualJoystickDelegate implements TouchEventHandler
{
    private View view;
    private ScaleGestureDetector scaleGestureDetector;
    private int currentElement = -1;
    private float lastX, lastY;

    public EditVirtualJoystickDelegate(View view) {
        this.view = view;
        scaleGestureDetector = new ScaleGestureDetector(view.getContext(), new EditVirtualJoystickDelegate.ScaleGestureListener());
    }

    @Override
    public void stop() {
    }
    @Override
    public void show() {
        VGamepad.show();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event, int width, int height)
    {
        scaleGestureDetector.onTouchEvent(event);
        if (scaleGestureDetector.isInProgress()) {
            currentElement = -1;
            return true;
        }

        int actionMasked = event.getActionMasked();
        int actionIndex = event.getActionIndex();
        switch (actionMasked)
        {
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                currentElement = -1;
                break;

            case MotionEvent.ACTION_DOWN:
                lastX = event.getX(actionIndex) / view.getWidth();
                lastY = event.getY(actionIndex) / view.getHeight();
                currentElement = VGamepad.layoutHitTest(lastX, lastY);
                return true; // must return true if we want the scale gesture detector to work

            case MotionEvent.ACTION_MOVE:
                if (currentElement != -1 && event.getPointerCount() == 1)
                {
                    float x = event.getX(actionIndex) / view.getWidth();
                    float y = event.getY(actionIndex) / view.getHeight();
                    VGamepad.translateElement(currentElement, x - lastX, y - lastY);
                    lastX = x;
                    lastY = y;
                    return true;
                }
                break;

            default:
                break;
        }

        return false;
    }

    private class ScaleGestureListener implements ScaleGestureDetector.OnScaleGestureListener {
        private int elemId = -1;
        @Override
        public boolean onScaleBegin(@NonNull ScaleGestureDetector detector)
        {
            elemId = VGamepad.layoutHitTest(detector.getFocusX() / view.getWidth(), detector.getFocusY() / view.getHeight());
            return elemId != -1;
        }

        @Override
        public boolean onScale(ScaleGestureDetector detector)
        {
            if (elemId == -1)
                return false;
            VGamepad.scaleElement(elemId, detector.getScaleFactor());
            return true;
        }

        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            elemId = -1;
        }
    }
}
