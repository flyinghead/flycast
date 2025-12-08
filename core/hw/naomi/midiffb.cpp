/*
	Copyright 2025 flyinghead

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
#include "midiffb.h"
#include "naomi_cart.h"
#include "hw/aica/aica_if.h"
#include "hw/maple/maple_cfg.h"
#include "serialize.h"
#include "input/haptic.h"
#include "oslib/oslib.h"
#include "network/output.h"
#include "oslib/i18n.h"

namespace midiffb {

static bool initialized;
static u8 midiTxBuf[4];
static u32 midiTxBufIndex;
static bool calibrating;
static float damperParam;
static float damperSpeed;
static float power = 0.8f;
static bool active;
static float position = 8192.f;
static float torque;
static float springForce;
static u8 maxSpring = 0x7f;

static void midiSend(u8 b1, u8 b2, u8 b3)
{
	aica::midiSend(b1);
	aica::midiSend(b2);
	aica::midiSend(b3);
	aica::midiSend((b1 ^ b2 ^ b3) & 0x7f);
}

// Thanks to njz3 and boomslangnz for their help
// https://www.arcade-projects.com/threads/force-feedback-translator-sega-midi-sega-rs422-and-namco-rs232.924/
static void midiReceiver(u8 data)
{
	// fake position used during calibration
	position = std::min(16383.f, std::max(0.f, position + torque));
	if (data & 0x80)
		midiTxBufIndex = 0;
	midiTxBuf[midiTxBufIndex] = data;
	if (midiTxBufIndex == 3 && ((midiTxBuf[0] ^ midiTxBuf[1] ^ midiTxBuf[2]) & 0x7f) == midiTxBuf[3])
	{
		const u8 cmd = midiTxBuf[0] & 0x7f;
		switch (cmd)
		{
		case 0:
			// FFB on/off
			// b1 & 1 => Temporary (a few 10th of a second) else permanently
			// b2 & 1 => FFB enabled
			if (midiTxBuf[2] == 0)
			{
				active = false;
				haptic::stopAll(0);
				if (calibrating) {
					calibrating = false;
					os_notify(i18n::T("Calibration done"), 2000);
				}
			}
			else if (midiTxBuf[2] == 1)
			{
				active = true;
				haptic::setDamper(0, damperSpeed * power, damperParam);
				haptic::setSpring(0, springForce * power, 1.f);
			}
			break;

		case 1:
			// Set force
			// b1: max torque forward (centering force)
			// b2: max torque backward (anti-centering force or friction)
			// Ex: 30 40 then 7f 40 (sgdrvsim search base pos)
			//     30 40 (*2 initdv3e init, clubk after init)
			//     7f 7f (kingrt init)
			//     1f 1f kingrt test menu
			maxSpring = midiTxBuf[1];
			break;

		case 2:
			// Unknown, seems to be used to enable something, usually follows 1 or 6
			// b1: 00 or 04 or 1F or 7F
			// b2: 54
			// Ex: 00 54 (sgdrvsim search base pos, kingrt)
			//     7f 54 (*2 initdv3e init)
			//     04 54 (clubk after init)
			break;

		case 3:
			// Drive power
			// b1: power level [0-7f]
			// b2: ? initdv3, clubk2k3: 4, kingrt: 0, sgdrvsim: 28
			power = (midiTxBuf[1] >> 3) / 15.f;
			break;

		case 4:
			// Torque
			// from 0 (max torque to the right) to 17f (max torque to the left)
			torque = ((midiTxBuf[1] << 7) | midiTxBuf[2]) - 0x80;
			if (active && !calibrating)
				haptic::setTorque(0, torque / 128.f * power);
			break;

		case 5:
			// Rumble
			// b1: frequency in half Hz
			// b2: amplitude
			// Examples:
			// * 02 40: large sine of 1Hz
			// * 0A 20: smaller sine of 5 Hz
			// decoding from FFB Arcade Plugin (by Boomslangnz)
			// https://github.com/Boomslangnz/FFBArcadePlugin/blob/master/Game%20Files/Demul.cpp
			if (active)
			{
				const float intensity = std::clamp((float)(midiTxBuf[2] - 1) / 80.f * power, 0.f, 1.f);
				const float freq = midiTxBuf[1] / 2.f;
				haptic::setSine(0, intensity, freq, 1000);
			}
			break;

		case 6:
			// Damper effect expressed as a ratio param1/param2, or a pole of
            // a transfer function.
			// Examples:
			// * 10 7F: strong damper
			// * 40 40: light damper
			// * XX 00: disabled
			damperSpeed = midiTxBuf[1] / 127.f;
			damperParam = midiTxBuf[2] / 127.f;
			// clubkart sets it to 28 40 in menus, and 04 12 when in game (not changing in between)
			// initdv3 starts with 02 2c		// FIXME no effect with sat=2
			//	changes it in-game: 02 11-2c
			// 	finish line(?): 02 5a
			//	ends with 00 00
			// kingrt66: 60 0a (start), 40 0a (in game)
			// sgdrvsim: 08 20 when calibrating center
			//           18 40 init
			//           28 nn in menus (nn is viscoSpeed<<6 >>8 from table: 20,28,30,38,...,98)
			//           1e+n 0+m in game (n and m are set in test menu, default 1e, 0))
			//           also: 8+n 0a+m and 0+n 3c+m
			if (active && !calibrating)
				haptic::setDamper(0, damperSpeed * power, damperParam);
			break;

		// 07 nn nn: Spring angle offset from center.
		// b1: 00 right, 01 left
		// b2: offset [0-7f] max 90 deg

		// 08: Spring effect inverted gain or limit?
		// 09 00 00: kingrt init
		//    03 40: end init
		// 0A 10 58: kingrt end init

		case 0xB:
			// Spring force
			// b1: force
			// b2: 00
			//  sgdrvsim: 20 10 in menus, else 00 00. Also [0-7f] 7f
			//  kingrt: 1b 00 (boot)
			//  clubk2k: 00 00 (boot) 20 10 (menus) 00 00 (drive)
			springForce = std::min(midiTxBuf[1], maxSpring) / 127.f;
			if (active && !calibrating)
				haptic::setSpring(0, springForce * power, 1.f);
			break;

		// 70 00 00: Set wheel center
		//    kingrt init (before 7f & 7a), kingrt test menu (after 7f)
		// 7A 00 nn: Set reply mode
		//       10: long status
		//       14: long status + encoder feedback (club2k, drvsimm)
		//       1f: short status
		// 00 10,14: clubk,initv3e,kingrt init/test menu

		// 7C 00 nn Start phase alignment. From 0 (disabled) to 7f (very strong)
		//    00 3f: initdv3e init
		//       20: clubk init
		//       30: kingrt init
		// 7D 00 00: Query status

		case 0x7f:
			// Reset board
			// FIXME also set when entering the service menu (kingrt66)
			os_notify(i18n::T("Calibrating the wheel. Keep it centered."), 10000);
			calibrating = true;
			haptic::setSpring(0, 0.8f, 1.f);
			position = 8192.f;
			break;
		}


		if (!calibrating)
		{
			int direction = -1;
			if (NaomiGameInputs != nullptr)
				direction = NaomiGameInputs->axes[0].inverted ? 1 : -1;

			position = std::clamp(mapleInputState[0].fullAxes[0] * direction / 4.f + 8192.f, 0.f, 16383.f);
		}
		// required: b1 & 0x1f == 0x10 && b1 & 0x40 == 0
		midiSend(0x90, ((int)position >> 7) & 0x7f, (int)position & 0x7f);

		if (cmd != 0x7d) {
			networkOutput.output("midiffb", (midiTxBuf[0] << 16) | (midiTxBuf[1]) << 8 | midiTxBuf[2]);
			DEBUG_LOG(NAOMI, "midiFFB: %02x %02x %02x", cmd, midiTxBuf[1], midiTxBuf[2]);
		}
	}
	midiTxBufIndex = (midiTxBufIndex + 1) % std::size(midiTxBuf);
}

void init()
{
	aica::setMidiReceiver(midiReceiver);
	initialized = true;
	reset();
}

void reset()
{
	active = false;
	calibrating = false;
	midiTxBufIndex = 0;
	power = 0.8f;
	damperParam = 0.f;
	damperSpeed = 0.f;
	torque = 0.f;
}

void term()
{
	aica::setMidiReceiver(nullptr);
	initialized = false;
}

void serialize(Serializer& ser)
{
	if (initialized)
	{
		ser << midiTxBuf;
		ser << midiTxBufIndex;
		ser << calibrating;
		ser << active;
		ser << power;
		ser << damperParam;
		ser << damperSpeed;
		ser << position;
		ser << torque;
		ser << maxSpring;
		ser << springForce;
	}
}

void deserialize(Deserializer& deser)
{
	if (deser.version() >= Deserializer::V27)
	{
		if (initialized) {
			deser >> midiTxBuf;
			deser >> midiTxBufIndex;
		}
		else if (deser.version() < Deserializer::V51) {
			deser.skip(4);		// midiTxBuf
			deser.skip<u32>();	// midiTxBufIndex
		}
	}
	else {
		midiTxBufIndex = 0;
	}
	if (deser.version() >= Deserializer::V34)
	{
		if (initialized)
			deser >> calibrating;
		else if (deser.version() < Deserializer::V51)
			deser.skip<bool>();	// calibrating
	}
	else {
		calibrating = false;
	}
	if (initialized)
	{
		maxSpring = 0x7f;
		springForce = 0;
		if (deser.version() >= Deserializer::V51)
		{
			deser >> active;
			deser >> power;
			deser >> damperParam;
			deser >> damperSpeed;
			deser >> position;
			deser >> torque;
			if (deser.version() >= Deserializer::V55) {
				deser >> maxSpring;
				deser >> springForce;
			}
			if (active && !calibrating) {
				haptic::setDamper(0, damperSpeed * power, damperParam);
				haptic::setSpring(0, springForce * power, 1.f);
			}
		}
		else
		{
			active = false;
			power = 0.8f;
			damperParam = 0.f;
			damperSpeed = 0.f;
			position = 8192.f;
			torque = 0.f;
		}
	}
}

}	// namespace midiffb
