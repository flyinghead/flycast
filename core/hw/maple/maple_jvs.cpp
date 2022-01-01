/*
	 Copyright 2020 flyinghead

	 This file is part of flycast.

	 flycast is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 2 of the License, or
	 (at your option) any later version.

	 flycast is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

	 You should have received a copy of the GNU General Public License
	 along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <array>
#include <memory>
#include "maple_devs.h"
#include "hw/naomi/naomi_cart.h"
#include <xxhash.h>
#include "oslib/oslib.h"
#include "stdclass.h"
#include "cfg/option.h"

#define LOGJVS(...) DEBUG_LOG(JVS, __VA_ARGS__)

u8 *EEPROM;

const u32 naomi_button_mapping[32] = {
		NAOMI_BTN2_KEY,		// DC_BTN_C
		NAOMI_BTN1_KEY,		// DC_BTN_B
		NAOMI_BTN0_KEY,		// DC_BTN_A
		NAOMI_START_KEY,	// DC_BTN_START
		NAOMI_UP_KEY,		// DC_DPAD_UP
		NAOMI_DOWN_KEY,		// DC_DPAD_DOWN
		NAOMI_LEFT_KEY,		// DC_DPAD_LEFT
		NAOMI_RIGHT_KEY,	// DC_DPAD_RIGHT
		NAOMI_BTN5_KEY,		// DC_BTN_Z
		NAOMI_BTN4_KEY,		// DC_BTN_Y
		NAOMI_BTN3_KEY,		// DC_BTN_X
		NAOMI_COIN_KEY,		// DC_BTN_D
		NAOMI_SERVICE_KEY,	// DC_DPAD2_UP
		NAOMI_TEST_KEY,		// DC_DPAD2_DOWN
		NAOMI_BTN6_KEY,		// DC_DPAD2_LEFT
		NAOMI_BTN7_KEY,		// DC_DPAD2_RIGHT

		NAOMI_RELOAD_KEY,	// DC_BTN_RELOAD
		NAOMI_BTN8_KEY,
};
extern u32 awave_button_mapping[32];
extern u32 awavelg_button_mapping[32];

const char *GetCurrentGameButtonName(DreamcastKey key)
{
	if (NaomiGameInputs == nullptr || key == EMU_BTN_NONE || key > DC_BTN_RELOAD)
		return nullptr;
	u32 pos = 0;
	u32 val = (u32)key;
	while ((val & 1) == 0)
	{
		pos++;
		val >>= 1;
	}
	u32 arcade_key;
	if (settings.platform.system == DC_PLATFORM_NAOMI)
	{
		if (pos >= ARRAY_SIZE(naomi_button_mapping))
			return nullptr;
		arcade_key = naomi_button_mapping[pos];
	}
	else
	{
		if (pos >= ARRAY_SIZE(awave_button_mapping))
			return nullptr;
		const u32* mapping = settings.input.JammaSetup == JVS::LightGun ? awavelg_button_mapping : awave_button_mapping;
		arcade_key = mapping[pos];
	}
	for (int i = 0; NaomiGameInputs->buttons[i].source != 0; i++)
		if (NaomiGameInputs->buttons[i].source == arcade_key)
			return NaomiGameInputs->buttons[i].name;

	return nullptr;
}

const char *GetCurrentGameAxisName(DreamcastKey axis)
{
	if (NaomiGameInputs == nullptr || axis == EMU_BTN_NONE)
		return nullptr;

	for (int i = 0; NaomiGameInputs->axes[i].name != nullptr; i++)
	{
		switch (NaomiGameInputs->axes[i].axis)
		{
		case 0:
			if (axis != DC_AXIS_LEFT && axis != DC_AXIS_RIGHT)
				continue;
			break;
		case 1:
			if (axis != DC_AXIS_UP && axis != DC_AXIS_DOWN)
				continue;
			break;
		case 2:
			if (axis != DC_AXIS2_LEFT && axis != DC_AXIS2_RIGHT)
				continue;
			break;
		case 3:
			if (axis != DC_AXIS2_UP && axis != DC_AXIS2_DOWN)
				continue;
			break;
		case 4:
			if (axis != DC_AXIS_RT)
				continue;
			break;
		case 5:
			if (axis != DC_AXIS_LT)
				continue;
			break;
		default:
			continue;
		}
		return NaomiGameInputs->axes[i].name;
	}

	return nullptr;
}

/*
 * Sega JVS I/O board
*/
static bool old_coin_chute[4];
static int coin_count[4];

class jvs_io_board
{
public:
	jvs_io_board(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
	{
		this->node_id = node_id;
		this->parent = parent;
		this->first_player = first_player;
		init_mappings();
	}
	virtual ~jvs_io_board() = default;

	u32 handle_jvs_message(u8 *buffer_in, u32 length_in, u8 *buffer_out);
	void serialize(Serializer& ser) const;
	void deserialize(Deserializer& deser);

	bool lightgun_as_analog = false;

protected:
	virtual const char *get_id() = 0;
	virtual u16 read_analog_axis(int player_num, int player_axis, bool inverted);

	virtual void read_digital_in(const u32 *buttons, u16 *v)
	{
		memset(v, 0, sizeof(u16) * 4);
		for (u32 player = first_player; player < 4; player++)
		{
			u32 keycode = buttons[player];
			if (keycode == 0)
				continue;
			if (keycode & NAOMI_RELOAD_KEY)
				keycode |= NAOMI_BTN0_KEY;

			// P1 mapping (only for P2)
			if (player == 1)
			{
				for (u32 i = 0; i < p1_mapping.size(); i++)
					if ((keycode & (1 << i)) != 0)
						v[0] |= p1_mapping[i];
			}
			// normal mapping
			for (u32 i = 0; i < cur_mapping.size(); i++)
				if ((keycode & (1 << i)) != 0)
					v[player - first_player] |= cur_mapping[i];
			// P2 mapping (only for P1)
			if (player == 0)
			{
				bool found = false;
				for (u32 i = 0; i < p2_mapping.size(); i++)
				{
					if ((keycode & (1 << i)) != 0)
						v[1] |= p2_mapping[i];
					if (p2_mapping[i] != 0)
						found = true;
				}
				if (found)
					// if there are P2 mappings for P1 then there's only 1 player
					break;
			}
		}
	}

	virtual void write_digital_out(int count, u8 *data) { }

	u32 player_count = 0;
	u32 digital_in_count = 0;
	u32 coin_input_count = 0;
	u32 analog_count = 0;
	u32 encoder_count = 0;
	u32 light_gun_count = 0;
	u32 output_count = 0;
	bool init_in_progress = false;

private:
	void init_mappings()
	{
		p1_mapping.fill(0);
		p2_mapping.fill(0);
		for (u32 i = 0; i < cur_mapping.size(); i++)
			cur_mapping[i] = 1 << i;
		if (NaomiGameInputs == nullptr)
			// Use default mapping
			return;

		for (int i = 0; NaomiGameInputs->buttons[i].source != 0; i++)
		{
			u32 source = NaomiGameInputs->buttons[i].source;
			int keyIdx = 0;
			for (; keyIdx < 32; keyIdx++)
				if (1u << keyIdx == source)
					break;
			verify(keyIdx < 32);
			p1_mapping[keyIdx] = NaomiGameInputs->buttons[i].p1_target;
			p2_mapping[keyIdx] = NaomiGameInputs->buttons[i].p2_target;
			u32 target = NaomiGameInputs->buttons[i].target;
			if (target != 0)
				cur_mapping[keyIdx] = target;
			else if (p1_mapping[keyIdx] != 0 || p2_mapping[keyIdx] != 0)
				cur_mapping[keyIdx] = 0;
		}
	}

	u8 node_id;
	maple_naomi_jamma *parent;
	u8 first_player;

	std::array<u32, 32> cur_mapping;
	std::array<u32, 32> p1_mapping;
	std::array<u32, 32> p2_mapping;
};

// Most common JVS board
class jvs_837_13551 : public jvs_io_board
{
public:
	jvs_837_13551(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 2;
		digital_in_count = 13;
		coin_input_count = 2;
		analog_count = 8;
		output_count = 6;
	}
protected:
	const char *get_id() override { return "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10"; }

};

class jvs_837_13551_noanalog : public jvs_837_13551
{
public:
	jvs_837_13551_noanalog(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13551(node_id, parent, first_player)
	{
		analog_count = 0;
	}
};

// Same in 4-player mode
class jvs_837_13551_4P : public jvs_837_13551
{
public:
	jvs_837_13551_4P(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13551(node_id, parent, first_player)
	{
		player_count = 4;
		digital_in_count = 12;
		coin_input_count = 4;
		analog_count = 0;
	}
};

// Rotary encoders 2nd board
// Virtua Golf, Outtrigger, Shootout Pool
class jvs_837_13938 : public jvs_io_board
{
public:
	jvs_837_13938(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 1;
		digital_in_count = 9;
		encoder_count = 4;
		output_count = 8;
	}
protected:
	const char *get_id() override { return "SEGA ENTERPRISES,LTD.;837-13938 ENCORDER BD  ;Ver0.01;99/08"; }

};

// Sega Marine Fishing, 18 Wheeler (TODO)
class jvs_837_13844 : public jvs_io_board
{
public:
	jvs_837_13844(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 2;
		coin_input_count = 2;
		digital_in_count = 12;
		analog_count = 8;
		output_count = 22;
	}
protected:
	const char *get_id() override { return "SEGA ENTERPRISES,LTD.;837-13844-01 I/O CNTL BD2 ;Ver1.00;99/07"; }

};

class jvs_837_13844_encoders : public jvs_837_13844
{
public:
	jvs_837_13844_encoders(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
		digital_in_count = 8;
		encoder_count = 4;
	}
};

class jvs_837_13844_touch : public jvs_837_13844
{
public:
	jvs_837_13844_touch(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
		light_gun_count = 1;
	}
};

// Wave Runner GP: fake the drive board
class jvs_837_13844_wrungp : public jvs_837_13844
{
public:
	jvs_837_13844_wrungp(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
	}
protected:
	void read_digital_in(const u32 *buttons, u16 *v) override
	{
		jvs_837_13844::read_digital_in(buttons, v);

		// The drive board RX0-7 is connected to the following player inputs
		v[0] |= NAOMI_BTN2_KEY | NAOMI_BTN3_KEY | NAOMI_BTN4_KEY | NAOMI_BTN5_KEY;
		if (drive_board & 16)
			v[0] &= ~NAOMI_BTN5_KEY;
		if (drive_board & 32)
			v[0] &= ~NAOMI_BTN4_KEY;
		if (drive_board & 64)
			v[0] &= ~NAOMI_BTN3_KEY;
		if (drive_board & 128)
			v[0] &= ~NAOMI_BTN2_KEY;
		v[1] |= NAOMI_BTN2_KEY | NAOMI_BTN3_KEY | NAOMI_BTN4_KEY | NAOMI_BTN5_KEY;
		if (drive_board & 1)
			v[1] &= ~NAOMI_BTN5_KEY;
		if (drive_board & 2)
			v[1] &= ~NAOMI_BTN4_KEY;
		if (drive_board & 4)
			v[1] &= ~NAOMI_BTN3_KEY;
		if (drive_board & 8)
			v[1] &= ~NAOMI_BTN2_KEY;
	}

	void write_digital_out(int count, u8 *data) override {
		if (count != 3)
			return;

		// The drive board TX0-7 is connected to outputs 15-22
		// shifting right by 2 to get the last 8 bits of the output
		u16 out = (data[1] << 6) | (data[2] >> 2);
		// reverse
		out = (out & 0xF0) >> 4 | (out & 0x0F) << 4;
		out = (out & 0xCC) >> 2 | (out & 0x33) << 2;
		out = (out & 0xAA) >> 1 | (out & 0x55) << 1;

		if (out == 0xff)
			drive_board = 0xff;
		else if ((out & 0xf) == 0xf)
		{
			out >>= 4;
			if (out > 7)
				drive_board = 0xff & ~(1 << (14 - out));
			else
				drive_board = 0xff & ~(1 << out);
		}
		else if ((out & 0xf0) == 0xf0)
		{
			out &= 0xf;
			if (out > 7)
				drive_board = 0xff & ~(1 << (out - 7));
			else
				drive_board = 0xff & ~(1 << (7 - out));
		}
	}

private:
	u8 drive_board = 0;
};

// Ninja assault
class jvs_namco_jyu : public jvs_io_board
{
public:
	jvs_namco_jyu(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 2;
		coin_input_count = 2;
		digital_in_count = 12;
		output_count = 16;
		light_gun_count = 2;
	}
protected:
	const char *get_id() override { return "namco ltd.;JYU-PCB;Ver1.00;JPN,2Coins 2Guns"; }
};

// Mazan
class jvs_namco_fcb : public jvs_io_board
{
public:
	jvs_namco_fcb(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 1;
		coin_input_count = 2;
		digital_in_count = 16;
		output_count = 6;
		light_gun_count = 1;
		analog_count = 7;
		encoder_count = 2;
	}
protected:
	const char *get_id() override { return "namco ltd.;FCB;Ver1.0;JPN,Touch Panel & Multipurpose"; }

	u16 read_analog_axis(int player_num, int player_axis, bool inverted) override {
		if (init_in_progress)
			return 0;
		const MapleInputState& inputState = mapleInputState[std::min(player_num, (int)ARRAY_SIZE(mapleInputState) - 1)];
		if (inputState.absPos.x < 0 || inputState.absPos.x > 639 || inputState.absPos.y < 0 || inputState.absPos.y > 479)
			return 0;
		else
			return 0x8000;
	}
};

// Gun Survivor
class jvs_namco_fca : public jvs_io_board
{
public:
	jvs_namco_fca(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 1;
		coin_input_count = 1; // 2 makes bios crash
		digital_in_count = 16;
		output_count = 6;
		analog_count = 7;
		encoder_count = 2;
	}
protected:
	const char *get_id() override { return "namco ltd.;FCA-1;Ver1.01;JPN,Multipurpose + Rotary Encoder"; }
};

// World Kicks
class jvs_namco_v226 : public jvs_io_board
{
public:
	jvs_namco_v226(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 1;
		digital_in_count = 16;
		coin_input_count = 1;
		analog_count = 12;
		output_count = 6;
	}
protected:
	const char *get_id() override { return "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10"; }

	void read_digital_in(const u32 *buttons, u16 *v) override
	{
		jvs_io_board::read_digital_in(buttons, v);
				// main button
		v[0] = ((v[0] & NAOMI_BTN0_KEY) << 6)		// start
				| ((v[1] & NAOMI_BTN0_KEY) << 2)	// left
				| ((v[2] & NAOMI_BTN0_KEY) << 1)	// right
				| ((v[3] & NAOMI_BTN0_KEY) >> 1)	// btn1
				// enter
				| ((v[0] & NAOMI_BTN3_KEY) << 3)	// btn0
				// service menu
				| (v[0] & (NAOMI_TEST_KEY | NAOMI_SERVICE_KEY | NAOMI_UP_KEY | NAOMI_DOWN_KEY));
	}

	u16 read_joystick_x(int joy_num)
	{
		s8 axis_x = mapleInputState[joy_num].fullAxes[PJAI_X1];
		axis_y = mapleInputState[joy_num].fullAxes[PJAI_Y1];
		limit_joystick_magnitude<64>(axis_x, axis_y);
		return std::min(0xff, 0x80 - axis_x) << 8;
	}

	u16 read_joystick_y(int joy_num)
	{
		return std::min(0xff, 0x80 - axis_y) << 8;
	}

	u16 read_analog_axis(int player_num, int player_axis, bool inverted) override {
		switch (player_axis)
		{
		case 0:
			return read_joystick_x(0);
		case 1:
			return read_joystick_y(0);
		case 2:
			return read_joystick_x(1);
		case 3:
			return read_joystick_y(1);
		case 4:
			return read_joystick_x(2);
		case 5:
			return read_joystick_y(2);
		case 6:
			return read_joystick_x(3);
		case 7:
			return read_joystick_y(3);
		case 8:
			return mapleInputState[0].halfAxes[PJTI_R] << 8;
		case 9:
			return mapleInputState[1].halfAxes[PJTI_R] << 8;
		case 10:
			return mapleInputState[2].halfAxes[PJTI_R] << 8;
		case 11:
			return mapleInputState[3].halfAxes[PJTI_R] << 8;
		default:
			return 0x8000;
		}
	}

private:
	s8 axis_y = 0;
};

// World Kicks PCB
class jvs_namco_v226_pcb : public jvs_io_board
{
public:
	jvs_namco_v226_pcb(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 2;
		digital_in_count = 16;
		coin_input_count = 1;
		analog_count = 12;
		output_count = 6;
	}
protected:
	const char *get_id() override { return "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10"; }

	void read_digital_in(const u32 *buttons, u16 *v) override
	{
		jvs_io_board::read_digital_in(buttons, v);
		for (u32 player = 0; player < player_count; player++)
		{
			u8 trigger = mapleInputState[player].halfAxes[PJTI_R] >> 2;
					// Ball button
			v[player] = ((trigger & 0x20) << 3) | ((trigger & 0x10) << 5) | ((trigger & 0x08) << 7)
					| ((trigger & 0x04) << 9) | ((trigger & 0x02) << 11) | ((trigger & 0x01) << 13)
					// other buttons
					| (v[player] & (NAOMI_SERVICE_KEY | NAOMI_TEST_KEY | NAOMI_START_KEY))
					| ((v[player] & NAOMI_BTN0_KEY) >> 4);		// remap button4 to button0 (change button)
		}
	}

	u16 read_joystick_x(int joy_num)
	{
		s8 axis_x = mapleInputState[joy_num].fullAxes[PJAI_X1];
		axis_y = mapleInputState[joy_num].fullAxes[PJAI_Y1];
		limit_joystick_magnitude<48>(axis_x, axis_y);
		return (axis_x + 128) << 8;
	}

	u16 read_joystick_y(int joy_num)
	{
		return std::min(0xff, 0x80 - axis_y) << 8;
	}

	u16 read_analog_axis(int player_num, int player_axis, bool inverted) override {
		switch (player_axis)
		{
		case 0:
			return read_joystick_x(0);
		case 1:
			return read_joystick_y(0);
		case 4:
			return read_joystick_x(1);
		case 5:
			return read_joystick_y(1);
		default:
			return 0x8000;
		}
	}

private:
	s8 axis_y = 0;
};

maple_naomi_jamma::maple_naomi_jamma()
{
	switch (settings.input.JammaSetup)
	{
	case JVS::Default:
	default:
		io_boards.push_back(std::unique_ptr<jvs_837_13551>(new jvs_837_13551(1, this)));
		break;
	case JVS::FourPlayers:
		io_boards.push_back(std::unique_ptr<jvs_837_13551_4P>(new jvs_837_13551_4P(1, this)));
		break;
	case JVS::RotaryEncoders:
		io_boards.push_back(std::unique_ptr<jvs_837_13938>(new jvs_837_13938(1, this)));
		io_boards.push_back(std::unique_ptr<jvs_837_13551>(new jvs_837_13551(2, this)));
		break;
	case JVS::OutTrigger:
		io_boards.push_back(std::unique_ptr<jvs_837_13938>(new jvs_837_13938(1, this)));
		io_boards.push_back(std::unique_ptr<jvs_837_13551_noanalog>(new jvs_837_13551_noanalog(2, this)));
		break;
	case JVS::SegaMarineFishing:
		io_boards.push_back(std::unique_ptr<jvs_837_13844>(new jvs_837_13844(1, this)));
		break;
	case JVS::DualIOBoards4P:
		io_boards.push_back(std::unique_ptr<jvs_837_13551>(new jvs_837_13551(1, this)));
		io_boards.push_back(std::unique_ptr<jvs_837_13551>(new jvs_837_13551(2, this, 2)));
		break;
	case JVS::LightGun:
		io_boards.push_back(std::unique_ptr<jvs_namco_jyu>(new jvs_namco_jyu(1, this)));
		break;
	case JVS::LightGunAsAnalog:
		// Regular board sending lightgun coords as axis 0/1
		io_boards.push_back(std::unique_ptr<jvs_837_13551>(new jvs_837_13551(1, this)));
		io_boards.back()->lightgun_as_analog = true;
		break;
	case JVS::Mazan:
		io_boards.push_back(std::unique_ptr<jvs_namco_fcb>(new jvs_namco_fcb(1, this)));
		io_boards.push_back(std::unique_ptr<jvs_namco_fcb>(new jvs_namco_fcb(2, this)));
		break;
	case JVS::GunSurvivor:
		io_boards.push_back(std::unique_ptr<jvs_namco_fca>(new jvs_namco_fca(1, this)));
		break;
	case JVS::DogWalking:
		io_boards.push_back(std::unique_ptr<jvs_837_13844_encoders>(new jvs_837_13844_encoders(1, this)));
		break;
	case JVS::TouchDeUno:
		io_boards.push_back(std::unique_ptr<jvs_837_13844_touch>(new jvs_837_13844_touch(1, this)));
		break;
	case JVS::WorldKicks:
		io_boards.push_back(std::unique_ptr<jvs_namco_v226>(new jvs_namco_v226(1, this)));
		break;
	case JVS::WorldKicksPCB:
		io_boards.push_back(std::unique_ptr<jvs_namco_v226_pcb>(new jvs_namco_v226_pcb(1, this)));
		break;
	case JVS::WaveRunnerGP:
		io_boards.push_back(std::unique_ptr<jvs_837_13844_wrungp>(new jvs_837_13844_wrungp(1, this)));
		break;
	}

	std::string eeprom_file = hostfs::getArcadeFlashPath() + ".eeprom";
	FILE* f = nowide::fopen(eeprom_file.c_str(), "rb");
	if (f)
	{
		if (std::fread(eeprom, 1, 0x80, f) != 0x80)
			WARN_LOG(MAPLE, "Failed or truncated read of EEPROM '%s'", eeprom_file.c_str());
		std::fclose(f);
		DEBUG_LOG(MAPLE, "Loaded EEPROM from %s", eeprom_file.c_str());
	}
	else if (naomi_default_eeprom != NULL)
	{
		DEBUG_LOG(MAPLE, "Using default EEPROM file");
		memcpy(eeprom, naomi_default_eeprom, 0x80);
	}
	else
		DEBUG_LOG(MAPLE, "EEPROM file not found at %s and no default found", eeprom_file.c_str());
	if (config::GGPOEnable)
		MD5Sum().add(eeprom, sizeof(eeprom))
				.getDigest(settings.network.md5.eeprom);
	EEPROM = eeprom;
}

maple_naomi_jamma::~maple_naomi_jamma()
{
	EEPROM = nullptr;
}

void maple_naomi_jamma::send_jvs_message(u32 node_id, u32 channel, u32 length, u8 *data)
{
	if (node_id - 1 < io_boards.size())
	{
		u8 temp_buffer[256];
		u32 out_len = io_boards[node_id - 1]->handle_jvs_message(data, length, temp_buffer);
		if (out_len > 0)
		{
			u8 *pbuf = &jvs_receive_buffer[channel][jvs_receive_length[channel]];
			if (jvs_receive_length[channel] + out_len + 3 <= sizeof(jvs_receive_buffer[0]))
			{
				if (crazy_mode)
				{
					pbuf[0] = 0x00;		// ? 0: ok, 2: timeout, 3: dest node !=0, 4: checksum failed
					pbuf[1] = out_len;
					memcpy(&pbuf[2], temp_buffer, out_len);
					jvs_receive_length[channel] += out_len + 2;
				}
				else
				{
					pbuf[0] = node_id;
					pbuf[1] = 0x00;		// 0: ok, 2: timeout, 3: dest node !=0, 4: checksum failed
					pbuf[2] = out_len;
					memcpy(&pbuf[3], temp_buffer, out_len);
					jvs_receive_length[channel] += out_len + 3;
				}
			}
		}
	}
}

void maple_naomi_jamma::send_jvs_messages(u32 node_id, u32 channel, bool use_repeat, u32 length, u8 *data, bool repeat_first)
{
	u8 temp_buffer[256];
	if (data)
	{
		memcpy(temp_buffer, data, length);
	}
	if (node_id == ALL_NODES)
	{
		for (u32 i = 0; i < io_boards.size(); i++)
			send_jvs_message(i + 1, channel, length, temp_buffer);
	}
	else if (node_id >= 1 && node_id <= 32)
	{
		u32 repeat_len = jvs_repeat_request[node_id - 1][0];
		if (use_repeat && repeat_len > 0)
		{
			if (repeat_first)
			{
				memmove(temp_buffer + repeat_len, temp_buffer, length);
				memcpy(temp_buffer, &jvs_repeat_request[node_id - 1][1], repeat_len);
			}
			else
			{
				memcpy(temp_buffer + length, &jvs_repeat_request[node_id - 1][1], repeat_len);
			}
			length += repeat_len;
		}
		send_jvs_message(node_id, channel, length, temp_buffer);
	}
}

bool maple_naomi_jamma::receive_jvs_messages(u32 channel)
{
	u32 dword_length = (jvs_receive_length[channel] + 0x10 + 3 - 1) / 4 + 1;

	w8(MDRS_JVSReply);
	w8(0x00);
	w8(0x20);
	if (jvs_receive_length[channel] == 0)
	{
		w8(0x05);
		w8(0x32);
	}
	else
	{
		w8(dword_length);
		w8(0x16);
	}
	w8(0xff);
	w8(0xff);
	w8(0xff);
	w32(0xffffff00);
	w32(0);
	w32(0);

	if (jvs_receive_length[channel] == 0)
	{
		w32(0);
		return false;
	}

	w8(0);
	w8(channel);
	if (crazy_mode)
		w8(0x8E);
	else
		w8(sense_line(jvs_receive_buffer[channel][0]));	// bit 0 is sense line level. If set during F1 <n>, more I/O boards need addressing

	memcpy(dma_buffer_out, jvs_receive_buffer[channel], jvs_receive_length[channel]);
	dma_buffer_out += dword_length * 4 - 0x10 - 3;
	*dma_count_out += dword_length * 4 - 0x10 - 3;
	jvs_receive_length[channel] = 0;

	return true;
}

void maple_naomi_jamma::handle_86_subcommand()
{
	if (dma_count_in == 0)
	{
		w8(MDRS_JVSReply);
		w8(0);
		w8(0x20);
		w8(0x00);

		return;
	}
	u32 subcode = dma_buffer_in[0];

	// CT fw uses 13 as a 17, and 17 as 13 and also uses 19
	if (crazy_mode)
	{
		switch (subcode)
		{
		case 0x13:
			subcode = 0x17;
			break;
		case 0x17:
			subcode = 0x13;
			break;
		}
	}
	u8 node_id = 0;
	u8 *cmd = NULL;
	u32 len = 0;
	u8 channel = 0;
	if (dma_count_in >= 3)
	{
		if (dma_buffer_in[1] > 31 && dma_buffer_in[1] != 0xff && dma_count_in >= 8)	// TODO what is this?
		{
			node_id = dma_buffer_in[6];
			len = dma_buffer_in[7];
			cmd = &dma_buffer_in[8];
			channel = dma_buffer_in[5] & 0x1f;
		}
		else
		{
			node_id = dma_buffer_in[1];
			len = dma_buffer_in[2];
			cmd = &dma_buffer_in[3];
		}
	}

	switch (subcode)
	{
		case 0x13:	// Store repeated request
			if (len > 0 && node_id > 0 && node_id <= 0x1f)
			{
				INFO_LOG(MAPLE, "JVS node %d: Storing %d cmd bytes", node_id, len);
				jvs_repeat_request[node_id - 1][0] = len;
				memcpy(&jvs_repeat_request[node_id - 1][1], cmd, len);
			}
			w8(MDRS_JVSReply);
			w8(0);
			w8(0x20);
			w8(0x01);
			w8(dma_buffer_in[0] + 1);	// subcommand + 1
			w8(0);
			w8(len + 1);
			w8(0);
			break;

		case 0x15:	// Receive JVS data
			receive_jvs_messages(dma_buffer_in[1]);
			break;

		case 0x17:	// Transmit without repeat
			jvs_receive_length[channel] = 0;
			send_jvs_messages(node_id, channel, false, len, cmd, false);
			w8(MDRS_JVSReply);
			w8(0);
			w8(0x20);
			w8(0x01);
			w8(0x18);	// always
			w8(channel);
			w8(0x8E);	//sense_line(node_id));
			w8(0);
			break;

		case 0x19:	// Transmit with repeat
			jvs_receive_length[channel] = 0;
			send_jvs_messages(node_id, channel, true, len, cmd, true);
			w8(MDRS_JVSReply);
			w8(0);
			w8(0x20);
			w8(0x01);
			w8(0x18);	// always
			w8(channel);
			w8(sense_line(node_id));
			w8(0);
			break;

		case 0x21:	// Transmit with repeat
			jvs_receive_length[channel] = 0;
			send_jvs_messages(node_id, channel, true, len, cmd, false);
			w8(MDRS_JVSReply);
			w8(0);
			w8(0x20);
			w8(0x01);
			w8(0x18);	// always
			w8(channel);
			w8(sense_line(node_id));
			w8(0);
			break;

		case 0x35:	// Receive then transmit with repeat (15 then 27)
			receive_jvs_messages(channel);
			// FALLTHROUGH

		case 0x27:	// Transmit with repeat
			{
				jvs_receive_length[channel] = 0;

				u32 cmd_count = dma_buffer_in[6];
				u32 idx = 7;
				for (u32 i = 0; i < cmd_count; i++)
				{
					node_id = dma_buffer_in[idx];
					len = dma_buffer_in[idx + 1];
					cmd = &dma_buffer_in[idx + 2];
					idx += len + 2;

					send_jvs_messages(node_id, channel, true, len, cmd, false);
				}

				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(0x26);
				w8(channel);
				w8(sense_line(node_id));
				w8(0);
			}
			break;

		case 0x33:	// Receive then transmit with repeat (15 then 21)
			receive_jvs_messages(channel);
			send_jvs_messages(node_id, channel, true, len, cmd, false);
			w8(MDRS_JVSReply);
			w8(0);
			w8(0x20);
			w8(0x01);
			w8(0x18);	// always
			w8(channel);
			w8(sense_line(node_id));
			w8(0);
			break;

		case 0x0B:	//EEPROM write
		{
			int address = dma_buffer_in[1];
			int size = dma_buffer_in[2];
			DEBUG_LOG(MAPLE, "EEprom write %08X %08X\n", address, size);
			//printState(Command,buffer_in,buffer_in_len);
			address = address % sizeof(eeprom);
			size = std::min((int)sizeof(eeprom) - address, size);
			memcpy(eeprom + address, dma_buffer_in + 4, size);

			std::string eeprom_file = hostfs::getArcadeFlashPath() + ".eeprom";
			FILE* f = nowide::fopen(eeprom_file.c_str(), "wb");
			if (f)
			{
				std::fwrite(eeprom, 1, sizeof(eeprom), f);
				std::fclose(f);
				INFO_LOG(MAPLE, "Saved EEPROM to %s", eeprom_file.c_str());
			}
			else
				WARN_LOG(MAPLE, "EEPROM SAVE FAILED to %s", eeprom_file.c_str());

			w8(MDRS_JVSReply);
			w8(0x00);
			w8(0x20);
			w8(0x01);
			memcpy(dma_buffer_out, eeprom, 4);
			dma_buffer_out += 4;
			*dma_count_out += 4;
		}
		break;

		case 0x3:	//EEPROM read
		{
			//printf("EEprom READ\n");
			int address = dma_buffer_in[1] % sizeof(eeprom);
			//printState(Command,buffer_in,buffer_in_len);
			w8(MDRS_JVSReply);
			w8(0x00);
			w8(0x20);
			w8(0x20);
			int size = sizeof(eeprom) - address;
			memcpy(dma_buffer_out, eeprom + address, size);
			dma_buffer_out += size;
			*dma_count_out += size;
		}
		break;

		case 0x31:	// DIP switches
		{
			w8(MDRS_JVSReply);
			w8(0x00);
			w8(0x20);
			w8(0x05);

			w8(0x32);
			w8(0xff);		// in(0)
			w8(0xff);		// in(1)
			w8(0xff);		// in(2)

			w8(0x00);
			w8(0xff);		// in(4)
			w8(0xff);		// in(5) bit0: 1=VGA, 0=NTSCi
			w8(0xff);		// in(6)

			w32(0x00);
			w32(0x00);
			w32(0x00);
		}
		break;

		//case 0x3:
		//	break;

		case 0x1:
			w8(MDRS_JVSReply);
			w8(0x00);
			w8(0x20);
			w8(0x01);

			w8(0x2);
			w8(0x0);
			w8(0x0);
			w8(0x0);
			break;

		default:
			INFO_LOG(MAPLE, "JVS: Unknown 0x86 sub-command %x", subcode);
			w8(MDRE_UnknownCmd);
			w8(0x00);
			w8(0x20);
			w8(0x00);
			break;
	}
}

u32 maple_naomi_jamma::RawDma(u32* buffer_in, u32 buffer_in_len, u32* buffer_out)
{
#ifdef DUMP_JVS
	printf("JVS IN: ");
	u8 *p = (u8*)buffer_in;
	for (int i = 0; i < buffer_in_len; i++) printf("%02x ", *p++);
	printf("\n");
#endif

	u32 out_len = 0;
	dma_buffer_out = (u8 *)buffer_out;
	dma_count_out = &out_len;

	dma_buffer_in = (u8 *)buffer_in + 4;
	dma_count_in = buffer_in_len - 4;

	u32 cmd = *(u8*)buffer_in;
	switch (cmd)
	{
		case MDC_JVSSelfTest:
			w8(MDRS_JVSSelfTestReply);
			w8(0x00);
			w8(0x20);
			w8(0x01);
			w8(0x00);
			break;

		case MDC_JVSCommand:
			handle_86_subcommand();
			break;

		case MDC_JVSUploadFirmware:
		{
			static u8 *ram;

			if (ram == NULL)
				ram = (u8 *)calloc(0x10000, 1);

			if (dma_buffer_in[1] == 0xff)
			{
				u32 hash = XXH32(ram, 0x10000, 0);
				LOGJVS("JVS Firmware hash %08x\n", hash);
				if (hash == 0xa7c50459	// CT
						|| hash == 0xae841e36)	// HOTD2
					crazy_mode = true;
				else
					crazy_mode = false;
#ifdef DUMP_JVS_FW
				FILE *fw_dump;
				char filename[128];
				for (int i = 0; ; i++)
				{
					sprintf(filename, "z80_fw_%d.bin", i);
					fw_dump = fopen(filename, "r");
					if (fw_dump == NULL)
					{
						fw_dump = fopen(filename, "w");
						INFO_LOG(MAPLE, "Saving JVS firmware to %s", filename);
						break;
					}
				}
				if (fw_dump)
				{
					fwrite(ram, 1, 0x10000, fw_dump);
					fclose(fw_dump);
				}
#endif
				free(ram);
				ram = NULL;
				for (int i = 0; i < 32; i++)
					jvs_repeat_request[i][0] = 0;

				return MDRS_DeviceReply;
			}
			int xfer_bytes;
			if (dma_buffer_in[0] == 0xff)
				xfer_bytes = 0x1C;
			else
				xfer_bytes = 0x18;
			u16 addr = (dma_buffer_in[2] << 8) + dma_buffer_in[3];
			memcpy(ram + addr, &dma_buffer_in[4], xfer_bytes);
			u8 sum = 0;
			for (int i = 0; i < 0x1C; i++)
				sum += dma_buffer_in[i];

			w8(0x80);	// or 0x81 on bootrom?
			w8(0);
			w8(0x20);
			w8(0x01);
			w8(sum);
			w8(0);
			w8(0);
			w8(0);

			w8(MDRS_DeviceReply);
			w8(0x00);
			w8(0x20);
			w8(0x00);
		}
		break;

	case MDC_JVSGetId:
		{
			const char ID1[] = "315-6149    COPYRIGHT SEGA E";
			const char ID2[] = "NTERPRISES CO,LTD.  1998    ";
			w8(0x83);
			w8(0x00);
			w8(0x20);
			w8(0x07);
			wstr(ID1, 28);

			w8(0x83);
			w8(0x00);
			w8(0x20);
			w8(0x05);
			wstr(ID2, 28);
		}
		break;

	case MDC_DeviceRequest:
		w8(MDRS_DeviceStatus);
		w8(0x00);
		w8(0x20);
		w8(0x00);
		break;

	case MDC_AllStatusReq:
		w8(MDRS_DeviceStatusAll);
		w8(0x00);
		w8(0x20);
		w8(0x00);
		break;

	case MDC_DeviceReset:
	case MDC_DeviceKill:
		w8(MDRS_DeviceReply);
		w8(0x00);
		w8(0x20);
		w8(0x00);
		break;

	case MDCF_GetCondition:
		w8(MDRE_UnknownCmd);
		w8(0x00);
		w8(0x00);
		w8(0x00);

		break;

	default:
		INFO_LOG(MAPLE, "Unknown Maple command %x", cmd);
		w8(MDRE_UnknownCmd);
		w8(0x00);
		w8(0x00);
		w8(0x00);

		break;
	}

#ifdef DUMP_JVS
	printf("JVS OUT: ");
	p = (u8 *)buffer_out;
	for (int i = 0; i < out_len; i++) printf("%02x ", p[i]);
	printf("\n");
#endif

	return out_len;
}

void maple_naomi_jamma::serialize(Serializer& ser) const
{
	maple_base::serialize(ser);
	ser << crazy_mode;
	ser << jvs_repeat_request;
	ser << jvs_receive_length;
	ser << jvs_receive_buffer;
	ser << eeprom;
	u32 board_count = io_boards.size();
	ser << board_count;
	for (u32 i = 0; i < io_boards.size(); i++)
		io_boards[i]->serialize(ser);
}
void maple_naomi_jamma::deserialize(Deserializer& deser)
{
	maple_base::deserialize(deser);
	deser >> crazy_mode;
	deser >> jvs_repeat_request;
	deser >> jvs_receive_length;
	deser >> jvs_receive_buffer;
	if (deser.version() >= Deserializer::V23)
		deser >> eeprom;
	u32 board_count;
	deser >> board_count;
	deser.skip(sizeof(size_t) - sizeof(u32), Deserializer::V23);
	for (u32 i = 0; i < board_count; i++)
		io_boards[i]->deserialize(deser);
}

u16 jvs_io_board::read_analog_axis(int player_num, int player_axis, bool inverted)
{
	u16 v;
	if (player_axis >= 0 && player_axis < 4)
		v = (mapleInputState[player_num].fullAxes[player_axis] + 128) << 8;
	else
		v = 0x8000;
	return inverted ? 0xff00 - v : v;
}

#define JVS_OUT(b) buffer_out[length++] = b
#define JVS_STATUS1() JVS_OUT(1)

u32 jvs_io_board::handle_jvs_message(u8 *buffer_in, u32 length_in, u8 *buffer_out)
{
	u8 jvs_cmd = buffer_in[0];
	if (jvs_cmd == 0xF0)		// JVS reset
		// Nothing to do
		return 0;
	if (jvs_cmd == 0xF1 && (length_in < 2 || buffer_in[1] != node_id))
		// Not for us
		return 0;

	u32 length = 0;
	JVS_OUT(0xE0);	// sync
	JVS_OUT(0);		// master node id
	u8& jvs_length = buffer_out[length++];

	switch (jvs_cmd)
	{
	case 0xF1:		// set board address
		JVS_STATUS1();	// status
		JVS_STATUS1();	// report
		JVS_OUT(5);		// ?
		LOGJVS("JVS Node %d address assigned\n", node_id);
		break;

	case 0x10:	// Read ID data
		JVS_STATUS1();	// status
		JVS_STATUS1();	// report
		for (const char *p = get_id(); *p != 0; )
			JVS_OUT(*p++);
		JVS_OUT(0);
		break;

	case 0x11:	// Get command format version
		JVS_STATUS1();
		JVS_STATUS1();
		JVS_OUT(0x11);	// 1.1
		break;

	case 0x12:	// Get JAMMA VIDEO version
		JVS_STATUS1();
		JVS_STATUS1();
		JVS_OUT(0x20);	// 2.0
		break;

	case 0x13:	// Get communication version
		JVS_STATUS1();
		JVS_STATUS1();
		JVS_OUT(0x10);	// 1.0
		break;

	case 0x14:	// Get slave features
		JVS_STATUS1();
		JVS_STATUS1();

		JVS_OUT(1);		// Digital inputs
		JVS_OUT(player_count);
		JVS_OUT(digital_in_count);
		JVS_OUT(0);

		if (coin_input_count > 0)
		{
			JVS_OUT(2);		// Coin inputs
			JVS_OUT(coin_input_count);
			JVS_OUT(0);
			JVS_OUT(0);
		}

		if (analog_count > 0)
		{
			JVS_OUT(3);		// Analog inputs
			JVS_OUT(analog_count);	//   channel count
			JVS_OUT(0x10);			//   16 bits per channel, 0: unknown
			JVS_OUT(0);
		}

		if (encoder_count > 0)
		{
			JVS_OUT(4);		// Rotary encoders
			JVS_OUT(encoder_count);
			JVS_OUT(0);
			JVS_OUT(0);
		}

		if (light_gun_count > 0)
		{
			JVS_OUT(6);		// Light gun
			JVS_OUT(16);		//   X bits
			JVS_OUT(16);		//   Y bits
			JVS_OUT(light_gun_count);
		}

		JVS_OUT(0x12);	// General output driver
		JVS_OUT(output_count);
		JVS_OUT(0);
		JVS_OUT(0);

		JVS_OUT(0);		// End of list
		break;

	case 0x15:	// Master board ID
		JVS_STATUS1();
		JVS_STATUS1();
		break;

	case 0x70:
		LOGJVS("JVS 0x70: %02x %02x %02x %02x", buffer_in[1], buffer_in[2], buffer_in[3], buffer_in[4]);
		init_in_progress = true;
		JVS_STATUS1();
		JVS_STATUS1();
		if (buffer_in[2] == 3)
		{
			JVS_OUT(0x10);
			for (int i = 0; i < 16; i++)
				JVS_OUT(0x7f);
			if (buffer_in[4] == 0x10 || buffer_in[4] == 0x11)
				init_in_progress = false;
		}
		else
		{
			JVS_OUT(2);
			JVS_OUT(3);
			JVS_OUT(1);
		}
		break;

	default:
		if (jvs_cmd >= 0x20 && jvs_cmd <= 0x38) // Read inputs and more
		{
			LOGJVS("JVS Node %d: ", node_id);
			u32 buttons[4] {};
#ifdef LIBRETRO
			for (int p = 0; p < 4; p++)
				buttons[p] = ~mapleInputState[p].kcode;
#else
			for (u32 i = 0; i < ARRAY_SIZE(naomi_button_mapping); i++)
				for (int p = 0; p < 4; p++)
					if ((mapleInputState[p].kcode & (1 << i)) == 0)
						buttons[p] |= naomi_button_mapping[i];
#endif
			for (u32& button : buttons)
			{
				if ((button & (NAOMI_UP_KEY | NAOMI_DOWN_KEY)) == (NAOMI_UP_KEY | NAOMI_DOWN_KEY))
					button &= ~(NAOMI_UP_KEY | NAOMI_DOWN_KEY);
				if ((button & (NAOMI_LEFT_KEY | NAOMI_RIGHT_KEY)) == (NAOMI_LEFT_KEY | NAOMI_RIGHT_KEY))
					button &= ~(NAOMI_LEFT_KEY | NAOMI_RIGHT_KEY);
			}

			JVS_STATUS1();	// status
			for (u32 cmdi = 0; cmdi < length_in; )
			{
				switch (buffer_in[cmdi])
				{
				case 0x20:	// Read digital input
					{
						JVS_STATUS1();	// report byte

						u16 inputs[4];
						read_digital_in(buttons, inputs);
						JVS_OUT((inputs[0] & NAOMI_TEST_KEY) ? 0x80 : 0x00); // test, tilt1, tilt2, tilt3, unused, unused, unused, unused
						LOGJVS("btns ");
						for (int player = 0; player < buffer_in[cmdi + 1]; player++)
						{
							LOGJVS("P%d %02x ", player + 1 + first_player, inputs[player] >> 8);
							JVS_OUT(inputs[player] >> 8);
							if (buffer_in[cmdi + 2] == 2)
							{
								LOGJVS("%02x ", inputs[player] & 0xFF);
								JVS_OUT(inputs[player]);
							}
						}
						cmdi += 3;
					}
					break;

				case 0x21:	// Read coins
					{
						JVS_STATUS1();	// report byte
						LOGJVS("coins ");
						for (int slot = 0; slot < buffer_in[cmdi + 1]; slot++)
						{
							bool coin_chute = false;
							if (buttons[first_player + slot] & NAOMI_COIN_KEY)
							{
								coin_chute = true;
								if (!old_coin_chute[first_player + slot])
									coin_count[first_player + slot] += 1;
							}
							old_coin_chute[first_player + slot] = coin_chute;

							LOGJVS("%d:%d ", slot + 1 + first_player, coin_count[first_player + slot]);
							// status (2 highest bits, 0: normal), coin count MSB
							JVS_OUT((coin_count[first_player + slot] >> 8) & 0x3F);
							// coin count LSB
							JVS_OUT(coin_count[first_player + slot]);
						}
						cmdi += 2;
					}
					break;

				case 0x22:	// Read analog inputs
					{
						JVS_STATUS1();	// report byte
						u32 axis = 0;

						LOGJVS("ana ");
						if (lightgun_as_analog)
						{
							for (; axis / 2 < player_count && axis < buffer_in[cmdi + 1]; axis += 2)
							{
								int playerNum = first_player + axis / 2;
								const MapleInputState& inputState = mapleInputState[std::min(playerNum, (int)ARRAY_SIZE(mapleInputState) - 1)];
								u16 x;
								u16 y;
								if (inputState.absPos.x < 0 || inputState.absPos.x > 639
										|| inputState.absPos.y < 0 || inputState.absPos.y > 479
										|| (buttons[playerNum] & NAOMI_RELOAD_KEY) != 0)
								{
									x = 0;
									y = 0;
								}
								else
								{
									x = inputState.absPos.x * 0xFFFF / 639;
									y = inputState.absPos.y * 0xFFFF / 479;
								}
								LOGJVS("x,y:%4x,%4x ", x, y);
								JVS_OUT(x >> 8);		// X, MSB
								JVS_OUT(x);				// X, LSB
								JVS_OUT(y >> 8);		// Y, MSB
								JVS_OUT(y);				// Y, LSB
							}
						}

						u32 player_num = first_player;
						u32 player_axis = axis;
						for (; axis < buffer_in[cmdi + 1]; axis++, player_axis++)
						{
							u16 axis_value;
							if (NaomiGameInputs != NULL)
							{
								if (player_axis >= ARRAY_SIZE(NaomiGameInputs->axes)
										|| NaomiGameInputs->axes[player_axis].name == NULL)
								{
									// Next player
									player_num++;
									player_axis = 0;
								}
								if (player_num < 4)
								{
									const AxisDescriptor& axisDesc = NaomiGameInputs->axes[player_axis];
									if (axisDesc.type == Half)
									{
										if (axisDesc.axis == 4)
											axis_value = mapleInputState[player_num].halfAxes[PJTI_R] << 8;
										else if (axisDesc.axis == 5)
											axis_value = mapleInputState[player_num].halfAxes[PJTI_L] << 8;
										else
											axis_value = 0;
										if (axisDesc.inverted)
											axis_value = 0xff00u - axis_value;
									}
									else
									{
										axis_value =  read_analog_axis(player_num, axisDesc.axis, axisDesc.inverted);
									}
								}
								else
								{
									axis_value = 0x8000;
								}
							}
							else
							{
								axis_value = read_analog_axis(player_num, player_axis, false);
							}
							LOGJVS("%d:%4x ", axis, axis_value);
							JVS_OUT(axis_value >> 8);
							JVS_OUT(axis_value);
						}
						cmdi += 2;
					}
					break;

				case 0x23:	// Read rotary encoders
					{
						JVS_STATUS1();	// report byte
						static s16 rotx = 0;
						static s16 roty = 0;
						// TODO Add more players.
						// I can't think of any naomi multiplayer game that uses rotary encoders
						rotx += mapleInputState[first_player].relPos.x * 5;
						roty -= mapleInputState[first_player].relPos.y * 5;
						LOGJVS("rotenc ");
						for (int chan = 0; chan < buffer_in[cmdi + 1]; chan++)
						{
							if (chan == 0)
							{
								LOGJVS("%d:%4x ", chan, rotx & 0xFFFF);
								JVS_OUT(rotx >> 8);	// MSB
								JVS_OUT(rotx);		// LSB
							}
							else if (chan == 1)
							{
								LOGJVS("%d:%4x ", chan, roty & 0xFFFF);
								JVS_OUT(roty >> 8);	// MSB
								JVS_OUT(roty);		// LSB
							}
							else
							{
								LOGJVS("%d:%4x ", chan, 0);
								JVS_OUT(0x00);		// MSB
								JVS_OUT(0x00);		// LSB
							}
						}
						cmdi += 2;
					}
					break;

				case 0x25:	// Read screen pos inputs
					{
						JVS_STATUS1();	// report byte

						// Channel number (1-based) is in jvs_request[cmdi + 1]
						int playerNum = first_player + buffer_in[cmdi + 1] - 1;
						s16 x;
						s16 y;
						if ((buttons[playerNum] & NAOMI_RELOAD_KEY) != 0)
						{
							x = 0;
							y = 0;
						}
						else
						{
							// specs:
							//u16 x = mo_x_abs * 0xFFFF / 639;
							//u16 y = (479 - mo_y_abs) * 0xFFFF / 479;
							// Ninja Assault:
							u32 xr = 0x19d - 0x37;
							u32 yr = 0x1fe - 0x40;
							x = mapleInputState[playerNum].absPos.x * xr / 639 + 0x37;
							y = mapleInputState[playerNum].absPos.y * yr / 479 + 0x40;
						}
						LOGJVS("lightgun %4x,%4x ", x, y);
						JVS_OUT(x >> 8);		// X, MSB
						JVS_OUT(x);				// X, LSB
						JVS_OUT(y >> 8);		// Y, MSB
						JVS_OUT(y);				// Y, LSB

						cmdi += 2;
					}
					break;

				case 0x32:	// switched outputs
				case 0x33:
					LOGJVS("output(%d) %x", buffer_in[cmdi + 1], buffer_in[cmdi + 2]);
					write_digital_out(buffer_in[cmdi + 1], &buffer_in[cmdi + 2]);
					JVS_STATUS1();	// report byte
					cmdi += buffer_in[cmdi + 1] + 2;
					break;

				case 0x30:	// substract coin
					if (buffer_in[cmdi + 1] > 0 && first_player + buffer_in[cmdi + 1] - 1 < (int)ARRAY_SIZE(coin_count))
						coin_count[first_player + buffer_in[cmdi + 1] - 1] -= (buffer_in[cmdi + 2] << 8) + buffer_in[cmdi + 3];
					JVS_STATUS1();	// report byte
					cmdi += 4;
					break;

				default:
					DEBUG_LOG(MAPLE, "JVS: Unknown input type %x", buffer_in[cmdi]);
					JVS_OUT(2);			// report byte: command error
					cmdi = length_in;	// Ignore subsequent commands
					break;
				}
			}
			LOGJVS("\n");
		}
		else
		{
			INFO_LOG(MAPLE, "JVS: Unknown JVS command %x", jvs_cmd);
			JVS_OUT(2);	// Unknown command
		}
		break;
	}
	jvs_length = length - 2;

	u8 calc_crc = 0;
	for (u32 i = 1; i < length; i++)
		calc_crc = ((calc_crc + buffer_out[i]) & 0xFF);

	JVS_OUT(calc_crc);
	LOGJVS("CRC %x", calc_crc);

	return length;
}

void jvs_io_board::serialize(Serializer& ser) const
{
	ser << node_id;
	ser << lightgun_as_analog;
}
void jvs_io_board::deserialize(Deserializer& deser)
{
	deser >> node_id;
	deser >> lightgun_as_analog;
}
