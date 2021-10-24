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

#define INPUT_2_BUTTONS(btn0, btn1) {	\
	{									\
		{ NAOMI_BTN0_KEY, btn0 },		\
		{ NAOMI_BTN1_KEY, btn1 },		\
		{ NAOMI_UP_KEY, "" },			\
		{ NAOMI_DOWN_KEY, "" },		\
		{ NAOMI_LEFT_KEY, "" },		\
		{ NAOMI_RIGHT_KEY, "" },	\
		NAO_START_DESC					\
		NAO_BASE_BTN_DESC				\
	}									\
}

#define INPUT_3_BUTTONS(btn0, btn1, btn2) {	\
	{									\
		{ NAOMI_BTN0_KEY, btn0 },		\
		{ NAOMI_BTN1_KEY, btn1 },		\
		{ NAOMI_BTN2_KEY, btn2 },		\
		{ NAOMI_UP_KEY, "" },			\
		{ NAOMI_DOWN_KEY, "" },		\
		{ NAOMI_LEFT_KEY, "" },		\
		{ NAOMI_RIGHT_KEY, "" },	\
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
		{ NAOMI_UP_KEY, "" },			\
		{ NAOMI_DOWN_KEY, "" },		\
		{ NAOMI_LEFT_KEY, "" },		\
		{ NAOMI_RIGHT_KEY, "" },	\
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
		{ NAOMI_UP_KEY, "" },			\
		{ NAOMI_DOWN_KEY, "" },		\
		{ NAOMI_LEFT_KEY, "" },		\
		{ NAOMI_RIGHT_KEY, "" },	\
		NAO_START_DESC					\
		NAO_BASE_BTN_DESC				\
	}									\
}

static InputDescriptors _18wheelr_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "HORN" },
			{ NAOMI_DOWN_KEY, "VIEW" },
			{ NAOMI_BTN2_KEY, "SHIFT L", 0, NAOMI_DOWN_KEY },	// This button uses P2 inputs for P1
			{ NAOMI_BTN1_KEY, "SHIFT H", 0, NAOMI_UP_KEY },		// This button uses P2 inputs for P1
			{ NAOMI_BTN3_KEY, "SHIFT R", 0, NAOMI_LEFT_KEY | NAOMI_DOWN_KEY },
																// This button uses P2 inputs for P1
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
			{ NAOMI_UP_KEY, "" },
			{ NAOMI_DOWN_KEY, "" },
			{ NAOMI_LEFT_KEY, "" },
			{ NAOMI_RIGHT_KEY, "" },
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
			{ NAOMI_UP_KEY, "" },
			{ NAOMI_DOWN_KEY, "" },
			{ NAOMI_LEFT_KEY, "" },
			{ NAOMI_RIGHT_KEY, "" },
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

static InputDescriptors trigger_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "TRIGGER" },
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
			{ NAOMI_UP_KEY, "" },
			{ NAOMI_DOWN_KEY, "" },
			{ NAOMI_LEFT_KEY, "" },
			{ NAOMI_RIGHT_KEY, "" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
};

static InputDescriptors ninjaslt_inputs = {
	  {
			{ NAOMI_BTN2_KEY, "ENTER", NAOMI_BTN0_KEY },
			{ NAOMI_START_KEY, "", NAOMI_BTN2_KEY, 0, NAOMI_BTN3_KEY },
			{ NAOMI_BTN0_KEY, "TRIGGER", NAOMI_BTN4_KEY, 0, NAOMI_BTN5_KEY },
			{ NAOMI_UP_KEY, "SELECT UP" },
			{ NAOMI_DOWN_KEY, "SELECT DOWN" },
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

static InputDescriptors radirgyn_inputs = INPUT_3_BUTTONS("SHOOT", "SWORD", "SHIELD/SPECIAL");

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

static InputDescriptors f355_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "ASSIST SC" },
			{ NAOMI_BTN1_KEY, "ASSIST TC" },
			{ NAOMI_BTN2_KEY, "ASSIST ABS" },
			{ NAOMI_BTN3_KEY, "ASSIST IBS", 0, NAOMI_BTN1_KEY },
			{ NAOMI_BTN4_KEY, "WING SHIFT L", 0, NAOMI_DOWN_KEY },
			{ NAOMI_BTN5_KEY, "WING SHIFT R", 0, NAOMI_UP_KEY },

			// Manual gearshift (Deluxe only)
			//    L   R
			//  U 1 3 5
			//
			//  D 2 4 6
			{ NAOMI_UP_KEY, "SPEED SHIFT UP" },
			{ NAOMI_DOWN_KEY, "SPEED SHIFT DOWN" },
			{ NAOMI_LEFT_KEY, "SPEED SHIFT LEFT" },
			{ NAOMI_RIGHT_KEY, "SPEED SHIFT RIGHT" },

			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "ACCEL", Half, 4 },
			{ "BRAKE", Half, 5 },
			{ "CLUTCH", Full, 2 },	// Deluxe only
			{ "unused", Full, 4 },
			{ "HANDLE", Full, 0 },
	  },
};

static InputDescriptors zombie_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "L" },
			{ NAOMI_BTN1_KEY, "R" },
			{ NAOMI_BTN2_KEY, "G" },
			{ NAOMI_UP_KEY, "" },
			{ NAOMI_DOWN_KEY, "" },
			{ NAOMI_LEFT_KEY, "" },
			{ NAOMI_RIGHT_KEY, "" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
	  },
	  {
			{ "UP - DOWN", Full, 1, true },
			{ "LEFT - RIGHT", Full, 0, true },
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

static InputDescriptors senkosp_inputs = INPUT_5_BUTTONS("MAIN", "SUB", "MAIN+SUB", "ACTION", "OVER DRIVE");

static InputDescriptors meltyb_inputs = INPUT_5_BUTTONS("LAttack", "MAttack", "HAttack", "Guard", "Quick Action");

static InputDescriptors toukon4_inputs = INPUT_5_BUTTONS("X", "Y", "R", "A", "B");

static InputDescriptors hmgeo_inputs = {
	{
		{ NAOMI_BTN0_KEY, "Fire" },
		{ NAOMI_BTN1_KEY, "Attack" },
		{ NAOMI_BTN3_KEY, "Jump" },
		{ NAOMI_BTN4_KEY, "Target" },
		{ NAOMI_UP_KEY, "" },
		{ NAOMI_DOWN_KEY, "" },
		{ NAOMI_LEFT_KEY, "" },
		{ NAOMI_RIGHT_KEY, "" },
		NAO_START_DESC
		NAO_BASE_BTN_DESC
	},
};

//
// AtomisWave games
//

#define AW_BASE_BTN_DESC { AWAVE_COIN_KEY, "" }, \
						 { AWAVE_TEST_KEY, "" }, \
						 { AWAVE_SERVICE_KEY, "" },
#define AW_START_DESC { AWAVE_START_KEY, "" },

#define AW_5_BUTTONS(btn0, btn1, btn2, btn3, btn4) {	\
	{									\
		{ AWAVE_BTN0_KEY, btn0 },		\
		{ AWAVE_BTN1_KEY, btn1 },		\
		{ AWAVE_BTN2_KEY, btn2 },		\
		{ AWAVE_BTN3_KEY, btn3 },		\
		{ AWAVE_BTN4_KEY, btn4 },		\
		{ AWAVE_UP_KEY, "" },			\
		{ AWAVE_DOWN_KEY, "" },		\
		{ AWAVE_LEFT_KEY, "" },		\
		{ AWAVE_RIGHT_KEY, "" },	\
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
		{ AWAVE_UP_KEY, "" },			\
		{ AWAVE_DOWN_KEY, "" },		\
		{ AWAVE_LEFT_KEY, "" },		\
		{ AWAVE_RIGHT_KEY, "" },	\
		AW_START_DESC					\
		AW_BASE_BTN_DESC				\
	}									\
}

#define AW_3_BUTTONS(btn0, btn1, btn2) {	\
	{									\
		{ AWAVE_BTN0_KEY, btn0 },		\
		{ AWAVE_BTN1_KEY, btn1 },		\
		{ AWAVE_BTN2_KEY, btn2 },		\
		{ AWAVE_UP_KEY, "" },			\
		{ AWAVE_DOWN_KEY, "" },		\
		{ AWAVE_LEFT_KEY, "" },		\
		{ AWAVE_RIGHT_KEY, "" },	\
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
