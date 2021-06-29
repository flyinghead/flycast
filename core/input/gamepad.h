/*
	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

enum DreamcastKey
{
	// Real buttons
	DC_BTN_C           = 1 << 0,
	DC_BTN_B           = 1 << 1,
	DC_BTN_A           = 1 << 2,
	DC_BTN_START       = 1 << 3,
	DC_DPAD_UP         = 1 << 4,
	DC_DPAD_DOWN       = 1 << 5,
	DC_DPAD_LEFT       = 1 << 6,
	DC_DPAD_RIGHT      = 1 << 7,
	DC_BTN_Z           = 1 << 8,
	DC_BTN_Y           = 1 << 9,
	DC_BTN_X           = 1 << 10,
	DC_BTN_D           = 1 << 11,
	DC_DPAD2_UP        = 1 << 12,
	DC_DPAD2_DOWN      = 1 << 13,
	DC_DPAD2_LEFT      = 1 << 14,
	DC_DPAD2_RIGHT     = 1 << 15,
	DC_BTN_RELOAD      = 1 << 16,	// Not a real button but handled like one

	// System buttons
	EMU_BTN_NONE			= 0,
	EMU_BTN_TRIGGER_LEFT	= 1 << 17,
	EMU_BTN_TRIGGER_RIGHT	= 1 << 18,
	EMU_BTN_MENU			= 1 << 19,
	EMU_BTN_FFORWARD		= 1 << 20,
	EMU_BTN_ANA_UP			= 1 << 21,
	EMU_BTN_ANA_DOWN		= 1 << 22,
	EMU_BTN_ANA_LEFT		= 1 << 23,
	EMU_BTN_ANA_RIGHT		= 1 << 24,
	EMU_BTN_ESCAPE			= 1 << 25,

	// Real axes
	DC_AXIS_LT		 = 0x10000,
	DC_AXIS_RT		 = 0x10001,
	DC_AXIS_X        = 0x20000,
	DC_AXIS_Y        = 0x20001,
	DC_AXIS_X2		 = 0x20002,
	DC_AXIS_Y2		 = 0x20003,

	// System axes
	EMU_AXIS_NONE        = 0x00000,
	EMU_AXIS_DPAD1_X     = DC_DPAD_LEFT,
	EMU_AXIS_DPAD1_Y     = DC_DPAD_UP,
	EMU_AXIS_DPAD2_X     = DC_DPAD2_LEFT,
	EMU_AXIS_DPAD2_Y     = DC_DPAD2_UP,
	EMU_AXIS_BTN_A       = 0x40000 | DC_BTN_A,
	EMU_AXIS_BTN_B       = 0x40000 | DC_BTN_B,
	EMU_AXIS_BTN_C       = 0x40000 | DC_BTN_C,
	EMU_AXIS_BTN_D       = 0x40000 | DC_BTN_D,
	EMU_AXIS_BTN_X       = 0x40000 | DC_BTN_X,
	EMU_AXIS_BTN_Y       = 0x40000 | DC_BTN_Y,
	EMU_AXIS_BTN_Z       = 0x40000 | DC_BTN_Z,
	EMU_AXIS_BTN_START   = 0x40000 | DC_BTN_START,
	EMU_AXIS_DPAD_UP     = 0x40000 | DC_DPAD_UP,
	EMU_AXIS_DPAD_DOWN   = 0x40000 | DC_DPAD_DOWN,
	EMU_AXIS_DPAD_LEFT   = 0x40000 | DC_DPAD_LEFT,
	EMU_AXIS_DPAD_RIGHT  = 0x40000 | DC_DPAD_RIGHT,
	EMU_AXIS_DPAD2_UP    = 0x40000 | DC_DPAD2_UP,
	EMU_AXIS_DPAD2_DOWN  = 0x40000 | DC_DPAD2_DOWN,
	EMU_AXIS_DPAD2_LEFT  = 0x40000 | DC_DPAD2_LEFT,
	EMU_AXIS_DPAD2_RIGHT = 0x40000 | DC_DPAD2_RIGHT,
};
