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

public class VGamepad
{
    static { System.loadLibrary("flycast"); }

    public static native int getVibrationPower();

    public static native void show();
    public static native void hide();
    public static native int hitTest(float x, float y);
    public static native float getControlWidth(int controlId);

    public static native int layoutHitTest(float x, float y);
    public static native void scaleElement(int elemId, float scale);
    public static native void translateElement(int elemId, float x, float y);
}
