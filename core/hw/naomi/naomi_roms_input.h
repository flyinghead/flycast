/*
 *  Created on: Nov 13, 2018

	Copyright 2018 flyinghead

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
#pragma once
#include "hw/maple/maple_devs.h"
#include "naomi_cart.h"

//
// NAOMI Games
//

#define NAO_BASE_BTN_DESC { NAOMI_COIN_KEY, "" }, \
						 { NAOMI_TEST_KEY, "" }, \
						 { NAOMI_SERVICE_KEY, "" },
#define NAO_START_DESC { NAOMI_START_KEY, "" },
#define NAO_DPAD_DESC	{ NAOMI_UP_KEY, "" },		\
						{ NAOMI_DOWN_KEY, "" },		\
						{ NAOMI_LEFT_KEY, "" },		\
						{ NAOMI_RIGHT_KEY, "" },	\

#define INPUT_1_BUTTON(btn0) {			\
	{									\
		{ NAOMI_BTN0_KEY, btn0 },		\
		NAO_DPAD_DESC					\
		NAO_START_DESC					\
		NAO_BASE_BTN_DESC				\
	}									\
}

#define INPUT_2_BUTTONS(btn0, btn1) {	\
	{									\
		{ NAOMI_BTN0_KEY, btn0 },		\
		{ NAOMI_BTN1_KEY, btn1 },		\
		NAO_DPAD_DESC					\
		NAO_START_DESC					\
		NAO_BASE_BTN_DESC				\
	}									\
}

#define INPUT_3_BUTTONS(btn0, btn1, btn2) {	\
	{									\
		{ NAOMI_BTN0_KEY, btn0 },		\
		{ NAOMI_BTN1_KEY, btn1 },		\
		{ NAOMI_BTN2_KEY, btn2 },		\
		NAO_DPAD_DESC					\
		NAO_START_DESC					\
		NAO_BASE_BTN_DESC				\
	}									\
}

#define INPUT_4_BUTTONS(btn0, btn1, btn2, btn3) {	\
	{									\
		{ NAOMI_BTN0_KEY, btn0 },		\
		{ NAOMI_BTN1_KEY, btn1 },		\
		{ NAOMI_BTN2_KEY, btn2 },		\
		{ NAOMI_BTN3_KEY, btn3 },		\
		NAO_DPAD_DESC					\
		NAO_START_DESC					\
		NAO_BASE_BTN_DESC				\
	}									\
}

#define INPUT_5_BUTTONS(btn0, btn1, btn2, btn3, btn4) {	\
	{									\
		{ NAOMI_BTN0_KEY, btn0 },		\
		{ NAOMI_BTN1_KEY, btn1 },		\
		{ NAOMI_BTN2_KEY, btn2 },		\
		{ NAOMI_BTN3_KEY, btn3 },		\
		{ NAOMI_BTN5_KEY, btn4, NAOMI_BTN4_KEY },		\
		NAO_DPAD_DESC					\
		NAO_START_DESC					\
		NAO_BASE_BTN_DESC				\
	}									\
}

static InputDescriptors service_btns_inputs = {
	{
		NAO_BASE_BTN_DESC
	}
};

static InputDescriptors _18wheelr_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "HORN" },
			{ NAOMI_DOWN_KEY, "VIEW" },
			{ NAOMI_BTN1_KEY, "SHIFT L/H" },
			{ NAOMI_BTN2_KEY, "SHIFT R" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "HANDLE", Full, 0 },
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
	  },
};

static InputDescriptors alienfnt_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "LEFT SHOT" },
			{ NAOMI_BTN1_KEY, "ROTATION R" },
			{ NAOMI_BTN2_KEY, "RIGHT SHOT" },
			{ NAOMI_BTN3_KEY, "ROTATION L" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "WHEEL", Full, 0 },
			{ "RIGHT PEDAL", Half, 4 },
			{ "LEFT PEDAL", Half, 5 },
	  },
};

static InputDescriptors alpilot_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "LANDING GEAR SW" },
			{ NAOMI_BTN1_KEY, "VIEW CHANGE" },
			{ NAOMI_BTN2_KEY, "FLAP SWITCH" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "ELEVATOR", Full, 1 },
			{ "AILERON", Full, 0 },
			{ "", Full, 3 },
			{ "RUDDER PEDAL", Full, 2 },
			{ "THRUST LEVER L", Half, 5 },
			{ "THRUST LEVER R", Half, 4 },
	  },
};

static InputDescriptors capcom_4btn_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "LIGHT PUNCH" },
			{ NAOMI_BTN1_KEY, "HEAVY PUNCH" },
			{ NAOMI_BTN3_KEY, "LIGHT KICK" },
			{ NAOMI_BTN4_KEY, "HEAVY KICK" },
			NAO_DPAD_DESC					\
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
};

static InputDescriptors capcom_6btn_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "LIGHT PUNCH" },
			{ NAOMI_BTN1_KEY, "MEDIUM PUNCH" },
			{ NAOMI_BTN2_KEY, "HEAVY PUNCH" },
			{ NAOMI_BTN3_KEY, "LIGHT KICK" },
			{ NAOMI_BTN4_KEY, "MEDIUM KICK" },
			{ NAOMI_BTN5_KEY, "HEAVY KICK" },
			NAO_DPAD_DESC					\
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
};

static InputDescriptors crzytaxi_inputs = {
	  {
			{ NAOMI_UP_KEY, "DRIVE GEAR" },
			{ NAOMI_DOWN_KEY, "REVERSE GEAR" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "HANDLE", Full, 0 },
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
	  },
};

static InputDescriptors cspike_inputs = INPUT_3_BUTTONS("Shoot", "Attack", "Mark");

static InputDescriptors doa2_inputs = INPUT_3_BUTTONS("Free", "Punch", "Kick");

static InputDescriptors toyfight_inputs = INPUT_3_BUTTONS("Punch", "Kick", "Dodge");

static InputDescriptors ausfache_inputs = INPUT_3_BUTTONS("Weak Attack", "Medium Attack", "Strong Attack");

static InputDescriptors lightgun_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "TRIGGER" },
			{ NAOMI_RELOAD_KEY, "" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
};

static InputDescriptors giant_gram_inputs = INPUT_4_BUTTONS("Attack", "Hold", "Throw", "Move");

static InputDescriptors gunsur2_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "GUN BUTTON" },
			{ NAOMI_BTN1_KEY, "TRIGGER" },
			{ NAOMI_UP_KEY, "SELECT UP" },
			{ NAOMI_DOWN_KEY, "SELECT DOWN" },
			{ NAOMI_START_KEY, "ENTER" },
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "PITCH", Full, 1 },
			{ "ROLL", Full, 2, true },
			{ "YAW", Full, 0 },
	  },
};

static InputDescriptors jambo_inputs = {
	  {
			{ NAOMI_BTN1_KEY, "LEVER UP", 0, NAOMI_DOWN_KEY },		// This button uses P2 inputs for P1
			{ NAOMI_BTN0_KEY, "LEVER DOWN", 0, NAOMI_UP_KEY },		// This button uses P2 inputs for P1
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "HANDLE", Full, 0 },
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
	  },
};

static InputDescriptors mvsc2_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "LIGHT PUNCH" },
			{ NAOMI_BTN1_KEY, "STRONG PUNCH" },
			{ NAOMI_BTN2_KEY, "ASSIST A" },
			{ NAOMI_BTN3_KEY, "LIGHT KICK" },
			{ NAOMI_BTN4_KEY, "STRONG KICK" },
			{ NAOMI_BTN5_KEY, "ASSIST B" },
			NAO_DPAD_DESC					\
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
};

static InputDescriptors ninjaslt_inputs = {
	  {
			{ NAOMI_BTN2_KEY, "ENTER", NAOMI_BTN0_KEY },
			{ NAOMI_START_KEY, "", NAOMI_BTN2_KEY, 0, NAOMI_BTN3_KEY },
			{ NAOMI_BTN0_KEY, "TRIGGER", NAOMI_BTN4_KEY, 0, NAOMI_BTN5_KEY },
			{ NAOMI_RELOAD_KEY, "" },
			{ NAOMI_UP_KEY, "SELECT UP" },
			{ NAOMI_DOWN_KEY, "SELECT DOWN" },
			NAO_BASE_BTN_DESC
	  },
};

static InputDescriptors mazan_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "TRIGGER" },
			{ NAOMI_UP_KEY, "SELECT UP" },
			{ NAOMI_DOWN_KEY, "SELECT DOWN" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
};

static InputDescriptors vonot_inputs = {
	  {
			{ NAOMI_UP_KEY, "L UP" },
			{ NAOMI_DOWN_KEY, "L DOWN" },
			{ NAOMI_LEFT_KEY, "L LEFT" },
			{ NAOMI_RIGHT_KEY, "L RIGHT" },
			{ NAOMI_BTN0_KEY, "L TRIGGER" },
			{ NAOMI_BTN1_KEY, "L TURBO" },
			{ NAOMI_BTN8_KEY, "QM" },
			// These buttons use P2 inputs for P1
			{ NAOMI_BTN2_KEY, "R TRIGGER", 0, NAOMI_BTN0_KEY },
			{ NAOMI_BTN3_KEY, "R TURBO", 0, NAOMI_BTN1_KEY },
			{ NAOMI_BTN4_KEY, "R UP", 0, NAOMI_UP_KEY },
			{ NAOMI_BTN5_KEY, "R DOWN", 0, NAOMI_DOWN_KEY },
			{ NAOMI_BTN6_KEY, "R LEFT", 0, NAOMI_LEFT_KEY },
			{ NAOMI_BTN7_KEY, "R RIGHT", 0, NAOMI_RIGHT_KEY },

			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
};

static InputDescriptors shot12_inputs = INPUT_2_BUTTONS("SHOT1", "SHOT2");

static InputDescriptors pstone_inputs = INPUT_3_BUTTONS("Punch", "Kick", "Jump");

static InputDescriptors pstone2_inputs = INPUT_3_BUTTONS("Punch", "Jump", "Attack");

static InputDescriptors shot1234_inputs = INPUT_4_BUTTONS("SHOT1", "SHOT2", "SHOT3", "SHOT4");

static InputDescriptors radirgy_inputs = INPUT_3_BUTTONS("SHOOT", "SWORD", "SHIELD/SPECIAL");

static InputDescriptors mamonoro_inputs = INPUT_2_BUTTONS("SHOOT", "SPECIAL");

static InputDescriptors monkeyba_inputs = {
	  {
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "STICK V", Full, 1 },
			{ "STICK H", Full, 0 },
	  },
};

static InputDescriptors slashout_inputs = INPUT_4_BUTTONS("Blade", "Charge", "Jump", "Shift");

static InputDescriptors tokyobus_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "HORN" },
			{ NAOMI_DOWN_KEY, "VIEW CHANGE" },
			{ NAOMI_LEFT_KEY, "ANNOUNCE" },
			{ NAOMI_RIGHT_KEY, "DOOR CLOSE" },
			// These buttons uses P2 inputs for P1
			{ NAOMI_BTN5_KEY, "WINKER RIGHT", 0, NAOMI_BTN0_KEY },
			{ NAOMI_BTN4_KEY, "WINKER LEFT", 0, NAOMI_BTN1_KEY },
			{ NAOMI_BTN2_KEY, "SHIFT FRONT", 0, NAOMI_UP_KEY },
			{ NAOMI_BTN1_KEY, "SHIFT REVERSE", 0, NAOMI_DOWN_KEY },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "HANDLE", Full, 0 },
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
	  },
};

static InputDescriptors wrungp_inputs = {
	  {
			{ NAOMI_UP_KEY, "VIEW" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "HANDLE BAR", Full, 0 },
			{ "THROTTLE LEVER", Half, 4, true },
			{ "ROLL", Full, 2 },
			{ "PITCH", Full, 3 },
	  },
};

// Standard cabinet. The Deluxe version has different (and more) inputs.
static InputDescriptors marine_fishing_inputs = {
	  {
			{ NAOMI_START_KEY, "CAST" },
			{ NAOMI_UP_KEY, "LURE" },
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "ROD Y", Full, 1 },
			{ "ROD X", Full, 0 },
			{ "STICK X", Full, 2 },
			{ "STICK Y", Full, 3 },
			{ "REEL SPEED", Half, 4 },
	  },
};

#ifdef NAOMI_MULTIBOARD
static InputDescriptors f355_inputs = {
	  {
			{ NAOMI_UP_KEY, "ASSIST SC" },
			{ NAOMI_DOWN_KEY, "ASSIST TC" },
			{ NAOMI_LEFT_KEY, "ASSIST ABS" },
			{ NAOMI_RIGHT_KEY, "ASSIST IBS" },

			{ NAOMI_BTN0_KEY, "WING SHIFT L", 0, NAOMI_BTN1_KEY },
			{ NAOMI_BTN1_KEY, "WING SHIFT R", 0, NAOMI_BTN0_KEY },

			// manual gear shift on P2 DPad
			//   L   R
			// U 1 3 5
			// D 2 4 6
			{ NAOMI_BTN2_KEY, "GEAR 1", 0, NAOMI_UP_KEY | NAOMI_LEFT_KEY },
			{ NAOMI_BTN3_KEY, "GEAR 2", 0, NAOMI_DOWN_KEY | NAOMI_LEFT_KEY },
			{ NAOMI_BTN4_KEY, "GEAR 3", 0, NAOMI_UP_KEY },
			{ NAOMI_BTN5_KEY, "GEAR 4", 0, NAOMI_DOWN_KEY },
			{ NAOMI_BTN6_KEY, "GEAR 5", 0, NAOMI_UP_KEY | NAOMI_RIGHT_KEY },
			{ NAOMI_BTN7_KEY, "GEAR 6", 0, NAOMI_DOWN_KEY | NAOMI_RIGHT_KEY },

			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "HANDLE", Full, 0 },
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
			{ "CLUTCH", Half, 6 },	// Deluxe only
	  },
};
#endif

static InputDescriptors zombie_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "L" },
			{ NAOMI_BTN1_KEY, "R" },
			{ NAOMI_BTN2_KEY, "G" },
			NAO_DPAD_DESC					\
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "UP - DOWN", Full, 1, true },
			{ "LEFT - RIGHT", Full, 0, true },
			{ "", Half, 4 },	// unused but P2 starts at axis 4
			{ "", Half, 5 },	// unused but P2 starts at axis 4
	  },
};

// World Series 99 / Super Major League 99
// World Series Baseball / Super Major League
static InputDescriptors wsbb_inputs = {
	{
		{ NAOMI_BTN0_KEY, "BTN A" },
		{ NAOMI_BTN1_KEY, "BTN B" },
		NAO_START_DESC
		NAO_BASE_BTN_DESC
	},
	{
		{ "STICK Y", Full, 1, true },
		{ "STICK X", Full, 0, true },
		{ "BAT", Half, 4, true },
		{ "", Half, 5 },	// unused but P2 starts at axis 4
	},
};

static InputDescriptors ringout_inputs  = {
	{
		{ NAOMI_BTN0_KEY, "BUMPER" },
		{ NAOMI_BTN1_KEY, "BACK" },
		NAO_START_DESC
		NAO_BASE_BTN_DESC
	},
	{
		{ "STEER", Full, 0 },
		{ "ACCEL", Half, 4 },
	},
};

static InputDescriptors sstrkfgt_inputs = {
	{
		{ NAOMI_BTN0_KEY, "GUN TRIGGER" },
		{ NAOMI_BTN1_KEY, "MISSILE BTN" },
		{ NAOMI_BTN2_KEY, "AIR BRAKE" },
		{ NAOMI_BTN3_KEY, "VIEW CHANGE" },
		NAO_START_DESC
		NAO_BASE_BTN_DESC
	},
	{
		{ "ELEVATOR", Full, 1 },
		{ "AILERON", Full, 0 },
		{ "THRUST LEVER", Half, 4 },
		{ "RUDDER PEDAL", Full, 2 },
	},
};

static InputDescriptors guilty_gear_inputs = INPUT_5_BUTTONS("KICK", "SLASH", "HSLASH", "PUNCH", "DUST ATTACK");

static InputDescriptors ggx_inputs = INPUT_4_BUTTONS("PUNCH", "KICK", "SLASH", "HSLASH");

static InputDescriptors senko_inputs = INPUT_3_BUTTONS("ACTION", "MAIN", "SUB");
static InputDescriptors senkosp_inputs = INPUT_5_BUTTONS("MAIN", "SUB", "MAIN+SUB", "ACTION", "OVER DRIVE");

static InputDescriptors meltyb_inputs = INPUT_5_BUTTONS("LAttack", "MAttack", "HAttack", "Guard", "Quick Action");

static InputDescriptors toukon4_inputs = INPUT_5_BUTTONS("X", "Y", "R", "A", "B");

static InputDescriptors hmgeo_inputs = INPUT_4_BUTTONS("Fire", "Attack", "Jump", "Target");

static InputDescriptors shootout_inputs = {
	{
		{ NAOMI_START_KEY, "START/MODE", NAOMI_BTN2_KEY },
		{ NAOMI_BTN0_KEY, "TOP/VIEW" },				// !prize
		{ NAOMI_BTN1_KEY, "BET", NAOMI_BTN6_KEY },	// prize only
		{ NAOMI_BTN3_KEY, "CUE ROLLER" },			// only used by emulator. press to use cue roller instead of cue aim
		{ NAOMI_UP_KEY, "ZOOM IN" },
		{ NAOMI_DOWN_KEY, "ZOOM OUT" },
		NAO_BASE_BTN_DESC
	},
	{
		{ "CUE TIP U/D", Full, 1, true },
		{ "CUE TIP L/R", Full, 0, true },
	}
};

static InputDescriptors vf4_inputs = INPUT_3_BUTTONS("PUNCH", "KICK", "GUARD");

static InputDescriptors crackindj_inputs = {
	{
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	},
	{
			{ "FADER", Full, 0, true },
	},
};

static InputDescriptors shaktam_inputs = {
	{
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ NAOMI_BTN0_KEY, "SHAKE L" },
			{ NAOMI_BTN1_KEY, "SHAKE R" },
			{ NAOMI_BTN2_KEY, "KNOCK", NAOMI_DOWN_KEY },
			{ NAOMI_DOWN_KEY, "DOWN", NAOMI_LEFT_KEY },
			{ NAOMI_UP_KEY, "UP", NAOMI_RIGHT_KEY },
	},
	{
			{ "TAMBOURINE X", Full, 0 },
			{ "TAMBOURINE Y", Full, 1 },
			{ "", Full, 2 }, // unused but P2 starts at axis 4
			{ "", Full, 3 }, // unused but P2 starts at axis 4
	},
};

static InputDescriptors mushik_inputs = {
	{
		{ NAOMI_BTN0_KEY, "HIT" },
		{ NAOMI_BTN1_KEY, "PINCH" },
		{ NAOMI_BTN2_KEY, "THROW" },
		{ NAOMI_BTN3_KEY, "HIT P2", 0, NAOMI_BTN0_KEY },
		NAO_BASE_BTN_DESC
	},
};

static InputDescriptors csmash_inputs = INPUT_2_BUTTONS("SMASH", "JUMP");
static InputDescriptors otrigger_inputs = INPUT_3_BUTTONS("TRIGGER", "CHANGE", "JUMP");
static InputDescriptors puyoda_inputs = INPUT_1_BUTTON("STAR");
static InputDescriptors sgtetris_inputs = INPUT_2_BUTTONS("SW1", "SW2");
static InputDescriptors virnba_inputs = INPUT_2_BUTTONS("PASS", "SHOOT");
static InputDescriptors vs2_2k_inputs = INPUT_3_BUTTONS("LONG PASS", "SHOOT", "SHORT PASS");
static InputDescriptors wwfroyal_inputs = INPUT_3_BUTTONS("ATTACK", "GRAPPLE", "SUPPORT");
static InputDescriptors asndynmt_inputs = INPUT_3_BUTTONS("PUNCH", "KICK", "JUMP");
static InputDescriptors illvelo_inputs = INPUT_3_BUTTONS("SHOT", "DOLL", "SPECIAL");
static InputDescriptors rhytngk_inputs = INPUT_2_BUTTONS("SHOT A", "SHOT B");
static InputDescriptors sl2007_inputs = INPUT_3_BUTTONS("PUSH 1", "PUSH 2", "PUSH 3");
static InputDescriptors azumanga_inputs = INPUT_1_BUTTON("BUTTON A");
static InputDescriptors bdrdown_inputs = INPUT_3_BUTTONS("SHOT", "LASER", "SPEED");
static InputDescriptors cfield_inputs = INPUT_3_BUTTONS("TRG1", "TRG2", "TRG3");
static InputDescriptors button12_inputs = INPUT_2_BUTTONS("BUTTON 1", "BUTTON 2");
static InputDescriptors ikaruga_inputs = INPUT_2_BUTTONS("SHOT", "CHANGE");
static InputDescriptors jingystm_inputs = INPUT_3_BUTTONS("GUARD", "PUNCH", "KICK");
static InputDescriptors psyvariar_inputs = INPUT_2_BUTTONS("SHOT", "BOMB");
static InputDescriptors puyofev_inputs = INPUT_2_BUTTONS("ROTATE1", "ROTATE2");
static InputDescriptors spkrbtl_inputs = INPUT_4_BUTTONS("BEAT", "CHARGE", "JUMP", "SHIFT");
static InputDescriptors trgheart_inputs = INPUT_3_BUTTONS("SHOT", "ANCHOR", "BOMB");
static InputDescriptors vathlete_inputs = INPUT_3_BUTTONS("RUN1", "ACTION", "RUN2");

static InputDescriptors samba_inputs = {
	{
		{ NAOMI_BTN0_KEY, "MARACAS R" },
		{ NAOMI_BTN1_KEY, "MARACAS L" },
		NAO_START_DESC
		NAO_BASE_BTN_DESC
	},
	{
		{ "MARACAS R X", Full, 0 },
		{ "MARACAS R Y", Full, 1 },
		{ "MARACAS L X", Full, 2 },
		{ "MARACAS L Y", Full, 3 },
	}
};

static InputDescriptors wldkicks_inputs = {
	{
		{ NAOMI_BTN0_KEY, "BUTTON" },
		{ NAOMI_BTN3_KEY, "ENTER" },	// service mode
		{ NAOMI_UP_KEY, "" },			// service mode
		{ NAOMI_DOWN_KEY, "" },			// service mode
		NAO_BASE_BTN_DESC
	},
	{
		{ "STICK L/R", Full, 0 },	// P1
		{ "STICK U/D", Full, 1 },
		{ "", Full, 2 },			// P2
		{ "", Full, 3 },
		{ "", Full, 4 },			// P3
		{ "", Full, 5 },
		{ "", Full, 6 },			// P4
		{ "", Full, 7 },
		{ "KICK", Full, 8 },		// P1	FIXME need to set Full here to have read_analog_axis() called but not seen as trigger
		{ "", Full, 9 },			// P2
		{ "", Full, 10 },			// P3
		{ "", Full, 11 },			// P4
	}
};
static InputDescriptors wldkickspcb_inputs = {
	{
		{ NAOMI_BTN0_KEY, "CHANGE" },	// original label: C BUTTON
		NAO_START_DESC
		NAO_BASE_BTN_DESC
	},
	{
		{ "STICK L/R", Full, 0 },
		{ "STICK U/D", Full, 1 },
		{ "", Full, 2 },
//		{ "", Full, 3 },
		{ "BALL", Half, 4 },	// this is wrong, just to indicate RT is used
	}
};

static InputDescriptors dygolf_inputs = {
	{
		NAO_DPAD_DESC
		NAO_START_DESC
		NAO_BASE_BTN_DESC
	}
};

static InputDescriptors kick4csh_inputs = {
	{
		{ NAOMI_BTN1_KEY, "VIEW" },
		{ NAOMI_BTN2_KEY, "CHANCE" },
		{ NAOMI_START_KEY, "START/DECIDE" },
		NAO_BASE_BTN_DESC
	}
};
//
// AtomisWave games
//

#define AW_BASE_BTN_DESC { AWAVE_COIN_KEY, "" }, \
						 { AWAVE_TEST_KEY, "" }, \
						 { AWAVE_SERVICE_KEY, "" },
#define AW_START_DESC { AWAVE_START_KEY, "" },
#define AW_DPAD_DESC	{ AWAVE_UP_KEY, "" },		\
						{ AWAVE_DOWN_KEY, "" },		\
						{ AWAVE_LEFT_KEY, "" },		\
						{ AWAVE_RIGHT_KEY, "" },


#define AW_5_BUTTONS(btn0, btn1, btn2, btn3, btn4) {	\
	{									\
		{ AWAVE_BTN0_KEY, btn0 },		\
		{ AWAVE_BTN1_KEY, btn1 },		\
		{ AWAVE_BTN2_KEY, btn2 },		\
		{ AWAVE_BTN3_KEY, btn3 },		\
		{ AWAVE_BTN4_KEY, btn4 },		\
		AW_DPAD_DESC					\
		AW_START_DESC					\
		AW_BASE_BTN_DESC				\
	}									\
}

#define AW_4_BUTTONS(btn0, btn1, btn2, btn3) {	\
	{									\
		{ AWAVE_BTN0_KEY, btn0 },		\
		{ AWAVE_BTN1_KEY, btn1 },		\
		{ AWAVE_BTN2_KEY, btn2 },		\
		{ AWAVE_BTN3_KEY, btn3 },		\
		AW_DPAD_DESC					\
		AW_START_DESC					\
		AW_BASE_BTN_DESC				\
	}									\
}

#define AW_3_BUTTONS(btn0, btn1, btn2) {	\
	{									\
		{ AWAVE_BTN0_KEY, btn0 },		\
		{ AWAVE_BTN1_KEY, btn1 },		\
		{ AWAVE_BTN2_KEY, btn2 },		\
		AW_DPAD_DESC					\
		AW_START_DESC					\
		AW_BASE_BTN_DESC				\
	}									\
}

static InputDescriptors kov7sprt_inputs = AW_3_BUTTONS("LIGHT ATTACK","HEAVY ATTACK","JUMP");

static InputDescriptors dolphin_inputs = AW_3_BUTTONS("SHOOT","JUMP","SPECIAL");

static InputDescriptors demofist_inputs = AW_3_BUTTONS("GUARD","ATTACK","JUMP");

static InputDescriptors guilty_gear_aw_inputs = AW_5_BUTTONS("KICK", "SLASH", "HSLASH", "PUNCH", "DUST ATTACK");

static InputDescriptors ggx15_inputs = AW_4_BUTTONS("KICK", "SLASH", "HSLASH", "PUNCH");

static InputDescriptors ftspeed_inputs = {
	  {
			{ AWAVE_BTN0_KEY, "BOOST" },
			{ AWAVE_UP_KEY, "HIGH GEAR" },
			{ AWAVE_DOWN_KEY, "LOW GEAR" },
			AW_START_DESC
			AW_BASE_BTN_DESC
	  },
	  {
			{ "STEERING WHEEL", Full, 0 },
			{ "GAS PEDAL", Half, 4 },
			{ "BRAKE PEDAL", Half, 5 },
	  },
};

static InputDescriptors kofnw_inputs = AW_5_BUTTONS("LP", "SP", "Heat mode", "LK", "SK");

static InputDescriptors kofxi_inputs = AW_5_BUTTONS("LP", "SP", "Blow-off", "LK", "SK");

static InputDescriptors maxspeed_inputs = {
	  {
			{ AWAVE_UP_KEY, "HIGH SHIFT" },
			{ AWAVE_DOWN_KEY, "LOW SHIFT" },
			AW_START_DESC
			AW_BASE_BTN_DESC
	  },
	  {
			{ "STEERING", Full, 0 },
			{ "ACCELERATOR", Half, 4 },
			{ "BRAKE", Half, 5 },
	  },
};

static InputDescriptors ngbc_inputs = AW_5_BUTTONS("LP", "SP", "SWAP", "LK", "SK");

static InputDescriptors samsptk_inputs = AW_5_BUTTONS("LIGHT SLASH", "MEDIUM SLASH", "STRONG SLASH", "KICK", "SPECIAL EVASION");

static InputDescriptors blokpong_inputs = {
	  {
			AW_START_DESC
			AW_BASE_BTN_DESC
	  },
	  {
			{ "ANALOG X", Full, 0, true },
			{ "ANALOG Y", Full, 1 },
			{ "ANALOG X", Full, 0 },	// for P2
			{ "ANALOG Y", Full, 1 },	// for P2
	  },
};

static InputDescriptors fotns_inputs = AW_5_BUTTONS("LP", "HP", "BOOST", "LK", "HK");

static InputDescriptors mslug6_inputs = AW_5_BUTTONS("SHOOT", "JUMP", "GRENADE", "METAL SLUG ATTACK", "SWITCH WEAPONS");

static InputDescriptors rumblef_inputs = AW_5_BUTTONS("LP", "SP", "Dodge", "LK", "SK");

static InputDescriptors basschal_inputs = {
	{
		{ AWAVE_BTN0_KEY, "ROTATE LEFT" },
		{ AWAVE_BTN1_KEY, "ROTATE RIGHT" },
		{ AWAVE_LEFT_KEY, "LEFT POINT" },
		{ AWAVE_RIGHT_KEY, "RIGHT POINT" },
		AW_START_DESC
		AW_BASE_BTN_DESC
	},
};

static InputDescriptors aw_lightgun_inputs = {
	{
		{ AWAVE_TRIGGER_KEY, "TRIGGER" },
		{ AWAVE_BTN0_KEY, "PUMP" },
		AW_START_DESC
		AW_BASE_BTN_DESC
	},
};

static InputDescriptors aw_shot123_inputs = AW_3_BUTTONS("SHOT1", "SHOT2", "SHOT3");

//
// Naomi 2
//

static InputDescriptors kingrt66_inputs = {
	{
			{ NAOMI_BTN0_KEY, "HORN" },
			{ NAOMI_BTN1_KEY, "WIPER" },
			{ NAOMI_DOWN_KEY, "VIEW" },
			{ NAOMI_BTN2_KEY, "SHIFT L", 0, NAOMI_DOWN_KEY },	// This button uses P2 inputs for P1
			{ NAOMI_BTN3_KEY, "SHIFT H", 0, NAOMI_UP_KEY },		// This button uses P2 inputs for P1
			{ NAOMI_BTN4_KEY, "SHIFT R", 0, NAOMI_LEFT_KEY | NAOMI_DOWN_KEY },
																// This button uses P2 inputs for P1
			{ NAOMI_BTN5_KEY, "MIC SWITCH", NAOMI_BTN2_KEY },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	},
	{
			{ "HANDLE", Full, 0 },
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
	},
};

static InputDescriptors clubkart_inputs = {
	{
			{ NAOMI_DOWN_KEY, "VIEW" },	// !prize (start is used instead)
			{ NAOMI_BTN1_KEY, "BET" },	// prize only
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	},
	{
			{ "HANDLE", Full, 0 },
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
	},
};

static InputDescriptors initd_inputs = {
	{
			{ NAOMI_DOWN_KEY, "VIEW" },
			{ NAOMI_BTN0_KEY, "GEAR UP", 0, NAOMI_UP_KEY },		// This button uses P2 inputs for P1
			{ NAOMI_BTN1_KEY, "GEAR DOWN", 0, NAOMI_DOWN_KEY },	// This button uses P2 inputs for P1
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	},
	{
			{ "HANDLE", Full, 0 },
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
	},
};

static InputDescriptors wldrider_inputs = {
	{
			{ NAOMI_UP_KEY, "PUSH", NAOMI_LEFT_KEY },
			{ NAOMI_DOWN_KEY, "PULL", NAOMI_RIGHT_KEY },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	},
	{
			{ "Handlebar", Full, 0 },
			{ "Accelerator", Half, 4 },
			{ "Front Brake", Half, 5 },
			{ "Rear Brake", Half, 6 },	// not mapped
	},
};

static InputDescriptors soulsurfer_inputs = {
	{
			{ NAOMI_LEFT_KEY, "LEFT", NAOMI_BTN0_KEY },
			{ NAOMI_RIGHT_KEY, "RIGHT", NAOMI_BTN1_KEY },
			{ NAOMI_BTN4_KEY, "FLOORMAT", 0xffffffff },		// always on
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	},
	{
			{ "SWING", Full, 2 },
			{ "ROLL", Full, 0 },
			{ "PITCH", Full, 1, true },
	},
};

#ifdef NAOMI_MULTIBOARD
static InputDescriptors drvsim_inputs = {
	{
			{ NAOMI_BTN0_KEY, "Turn R" },
			{ NAOMI_BTN1_KEY, "Turn L" },
			{ NAOMI_BTN2_KEY, "Shift 2" },
			{ NAOMI_BTN3_KEY, "Shift 3" },
			{ NAOMI_BTN4_KEY, "O/D Switch" },
			{ NAOMI_BTN5_KEY, "S-Brake" },
			{ NAOMI_BTN6_KEY, "IG-START", 0, NAOMI_LEFT_KEY },
			{ NAOMI_BTN7_KEY, "Shift 0", 0, NAOMI_BTN0_KEY },
			{ NAOMI_UP_KEY, "Horn" },
			{ NAOMI_DOWN_KEY, "Light 0" },
			{ NAOMI_LEFT_KEY, "Light 1" },
			{ NAOMI_RIGHT_KEY, "Light 2" },
			// always on unmappable buttons
			{ NAOMI_BTN8_KEY, "BELT", 0xffffffff, NAOMI_BTN2_KEY },
			{ NAOMI_RELOAD_KEY, "IG-ON", 0xffffffff, NAOMI_DOWN_KEY },

			NAO_START_DESC
			NAO_BASE_BTN_DESC
			// P2 inputs:
			// BTN0		Shift 0
			// BTN1		Shift 1
			// BTN2		BELT
			// UP		Washer
			// DOWN		IG-ON
			// LEFT		IG_START
			// RIGHT	HAZARD
			// START	WIPER-LO
			// Unknown:
			// STOP
			// WIPER-HI
	},
	{
			{ "", Full, ~0u, true },	// Master audio volume
			{ "Accelerator", Half, 4 },
			{ "Brake", Half, 5 },
			{ "Clutch", Half, 6 },
			{ "Wheel", Full, 0, true },	// only read via ffb board, not jvs
	},
};
#endif

static InputDescriptors beachspi_inputs = INPUT_2_BUTTONS("A", "B");

//
// System SP games
//

static InputDescriptors dinok_inputs = {
	{
		{ DC_BTN_A, "ROCK" },
		{ DC_BTN_B, "SCISSORS" },
		{ DC_BTN_C, "PAPER" },
		{ DC_BTN_X, "ROCK P2", 0, DC_BTN_A },
		NAO_BASE_BTN_DESC
	},
};

static InputDescriptors lovebery_inputs = {
	{
		{ DC_BTN_A, "P1 BUTTON" },
		{ DC_BTN_B, "P2 BUTTON", 0, DC_BTN_A },
		NAO_BASE_BTN_DESC
	},
};

static InputDescriptors tetgiant_inputs = {
	{
		{ DC_BTN_A, "BUTTON L" },
		{ DC_BTN_B, "BUTTON R" },
		{ DC_DPAD_UP, "" },
		{ DC_DPAD_DOWN, "" },
		{ DC_DPAD_LEFT, "" },
		{ DC_DPAD_RIGHT, "" },
		{ DC_BTN_START, "" },
		NAO_BASE_BTN_DESC
	},
};

static InputDescriptors btlracer_inputs = {
	{
		{ DC_BTN_A, "BUTTON" },
		NAO_BASE_BTN_DESC
	},
	{
		{ "WHEEL", Full, 0 },
	},
};
