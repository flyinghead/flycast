/*
 *  Created on: Nov 13, 2018

	Copyright 2018 flyinghead

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

#ifndef CORE_HW_NAOMI_NAOMI_ROMS_INPUT_H_
#define CORE_HW_NAOMI_NAOMI_ROMS_INPUT_H_

#include "hw/maple/maple_devs.h"

//
// NAOMI Games
//

#define NAO_BASE_BTN_DESC { NAOMI_COIN_KEY, "COIN" }, \
						 { NAOMI_TEST_KEY, "TEST" }, \
						 { NAOMI_SERVICE_KEY, "SERVICE" },
#define NAO_START_DESC { NAOMI_START_KEY, "START" },

InputDescriptors _18wheelr_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "HORN" },
			{ NAOMI_DOWN_KEY, "VIEW" },
			{ NAOMI_BTN1_KEY, "SHIFT L", NAOMI_DOWN_KEY },		// This button uses P2 inputs for P1
			{ NAOMI_BTN2_KEY, "SHIFT H", NAOMI_UP_KEY },		// This button uses P2 inputs for P1
			{ NAOMI_BTN3_KEY, "SHIFT R", NAOMI_LEFT_KEY },		// This button uses P2 inputs for P1
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ "HANDLE", Full },
			{ "ACCEL", Half },
			{ "BRAKE", Half },
			{ NULL },
	  },
};

InputDescriptors alienfnt_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "LEFT SHOT" },
			{ NAOMI_BTN1_KEY, "ROTATION R" },
			{ NAOMI_BTN2_KEY, "RIGHT SHOT" },
			{ NAOMI_BTN3_KEY, "ROTATION L" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ "WHEEL", Full },
			{ "RIGHT PEDAL", Half },
			{ "LEFT PEDAL", Half },
			{ NULL },
	  },
};

InputDescriptors capsnk_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "SHOT1" },
			{ NAOMI_BTN1_KEY, "SHOT2" },
			{ NAOMI_BTN3_KEY, "SHOT4" },
			{ NAOMI_BTN4_KEY, "SHOT5" },
			{ NAOMI_UP_KEY, "UP" },
			{ NAOMI_DOWN_KEY, "DOWN" },
			{ NAOMI_LEFT_KEY, "LEFT" },
			{ NAOMI_RIGHT_KEY, "RIGHT" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ NULL },
	  },
};

InputDescriptors crzytaxi_inputs = {
	  {
			{ NAOMI_UP_KEY, "DRIVE GEAR" },
			{ NAOMI_DOWN_KEY, "REVERSE GEAR" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ "HANDLE", Full },
			{ "ACCEL", Half },
			{ "BRAKE", Half },
			{ NULL },
	  },
};

InputDescriptors cspike_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "SHOT1" },
			{ NAOMI_BTN1_KEY, "SHOT2" },
			{ NAOMI_BTN2_KEY, "SHOT3" },
			{ NAOMI_BTN3_KEY, "SHOT4" },
			{ NAOMI_UP_KEY, "UP" },
			{ NAOMI_DOWN_KEY, "DOWN" },
			{ NAOMI_LEFT_KEY, "LEFT" },
			{ NAOMI_RIGHT_KEY, "RIGHT" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ NULL },
	  },
};

InputDescriptors trigger_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "TRIGGER" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ NULL },
	  },
};

InputDescriptors gunsur2_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "GUN BUTTON" },
			{ NAOMI_BTN1_KEY, "TRIGGER" },
			{ NAOMI_UP_KEY, "SELECT UP" },
			{ NAOMI_DOWN_KEY, "SELECT DOWN" },
			{ NAOMI_START_KEY, "ENTER" },
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ "ROLL", Full },
			{ "PITCH", Full },
			{ "YAW", Full },
			{ NULL },
	  },
};

InputDescriptors jambo_inputs = {
	  {
			{ NAOMI_UP_KEY, "LEVER UP", NAOMI_DOWN_KEY },	// This button uses P2 inputs for P1
			{ NAOMI_DOWN_KEY, "LEVER DOWN", NAOMI_UP_KEY },	// This button uses P2 inputs for P1
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ "HANDLE", Full },
			{ "ACCEL", Half },
			{ "BRAKE", Half },
			{ NULL },
	  },
};

InputDescriptors mvsc2_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "SHOT1" },
			{ NAOMI_BTN1_KEY, "SHOT2" },
			{ NAOMI_BTN2_KEY, "SHOT3" },
			{ NAOMI_BTN3_KEY, "SHOT4" },
			{ NAOMI_BTN4_KEY, "SHOT5" },
			{ NAOMI_BTN5_KEY, "SHOT6" },
			{ NAOMI_UP_KEY, "UP" },
			{ NAOMI_DOWN_KEY, "DOWN" },
			{ NAOMI_LEFT_KEY, "LEFT" },
			{ NAOMI_RIGHT_KEY, "RIGHT" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ NULL },
	  },
};

InputDescriptors ninjaslt_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "ENTER" },
			{ NAOMI_BTN2_KEY, "START 1P" },
			{ NAOMI_BTN3_KEY, "START 2P" },
			{ NAOMI_BTN4_KEY, "TRIGGER 1P" },
			{ NAOMI_BTN5_KEY, "TRIGGER 2P" },
			{ NAOMI_UP_KEY, "SELECT UP" },
			{ NAOMI_DOWN_KEY, "SELECT DOWN" },
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ NULL },
	  },
};

InputDescriptors vonot_inputs = {
	  {
			{ NAOMI_UP_KEY, "L UP" },
			{ NAOMI_DOWN_KEY, "L DOWN" },
			{ NAOMI_LEFT_KEY, "L LEFT" },
			{ NAOMI_RIGHT_KEY, "L RIGHT" },
			{ NAOMI_BTN0_KEY, "L TRIGGER" },
			{ NAOMI_BTN1_KEY, "L TURBO" },
			{ NAOMI_BTN2_KEY, "QM" },
			// These buttons use P2 inputs for P1
			{ NAOMI_BTN3_KEY, "R TRIGGER", NAOMI_BTN0_KEY },
			{ NAOMI_BTN4_KEY, "R TURBO", NAOMI_BTN1_KEY },
			{ NAOMI_BTN5_KEY, "R UP", NAOMI_UP_KEY },
			{ NAOMI_BTN6_KEY, "R DOWN", NAOMI_DOWN_KEY },
			{ NAOMI_BTN7_KEY, "R LEFT", NAOMI_LEFT_KEY },
			{ NAOMI_BTN8_KEY, "R RIGHT", NAOMI_RIGHT_KEY },

			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ NULL },
	  },
};

InputDescriptors vtenis2c_inputs = {
	  {
			{ NAOMI_BTN0_KEY, "SHOT1" },
			{ NAOMI_BTN1_KEY, "SHOT2" },
			{ NAOMI_UP_KEY, "UP" },
			{ NAOMI_DOWN_KEY, "DOWN" },
			{ NAOMI_LEFT_KEY, "LEFT" },
			{ NAOMI_RIGHT_KEY, "RIGHT" },
			NAO_START_DESC
			NAO_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ NULL },
	  },
};

//
// AtomisWave games
//

#define AW_BASE_BTN_DESC { AWAVE_COIN_KEY, "COIN" }, \
						 { AWAVE_TEST_KEY, "TEST" }, \
						 { AWAVE_SERVICE_KEY, "SERVICE" },
#define AW_START_DESC { AWAVE_START_KEY, "START" },

InputDescriptors ftspeed_inputs = {
	  {
			{ AWAVE_BTN0_KEY, "BOOST" },
			{ AWAVE_UP_KEY, "HIGH GEAR" },
			{ AWAVE_DOWN_KEY, "LOW GEAR" },
			AW_START_DESC
			AW_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ "STEERING WHEEL", Full },
			{ "GAS PEDAL", Half },
			{ "BRAKE PEDAL", Half },
			{ NULL },
	  },
};
InputDescriptors maxspeed_inputs = {
	  {
			{ AWAVE_UP_KEY, "HIGH SHIFT" },
			{ AWAVE_DOWN_KEY, "LOW SHIFT" },
			AW_START_DESC
			AW_BASE_BTN_DESC
			{ 0 },
	  },
	  {
			{ "STEERING", Full },
			{ "ACCELERATOR", Half },
			{ "BRAKE", Half },
			{ NULL },
	  },
};

#endif /* CORE_HW_NAOMI_NAOMI_ROMS_INPUT_H_ */
