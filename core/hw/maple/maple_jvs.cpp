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
#include "maple_devs.h"
#include "maple_if.h"
#include "hw/naomi/naomi_cart.h"
#include <xxhash.h>
#include "oslib/oslib.h"
#include "oslib/i18n.h"
#include "stdclass.h"
#include "cfg/option.h"
#include "network/output.h"
#include "hw/naomi/printer.h"
#include "input/haptic.h"

#include <algorithm>
#include <array>
#include <memory>

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
	if (NaomiGameInputs == nullptr || key == EMU_BTN_NONE || key > DC_BTN_BITMAPPED_LAST)
		return nullptr;
	u32 pos = 0;
	u32 val = (u32)key;
	while ((val & 1) == 0)
	{
		pos++;
		val >>= 1;
	}
	u32 arcade_key;
	if (settings.platform.isNaomi())
	{
		if (pos >= std::size(naomi_button_mapping))
			return nullptr;
		arcade_key = naomi_button_mapping[pos];
	}
	else
	{
		if (pos >= std::size(awave_button_mapping))
			return nullptr;
		const u32* mapping = settings.input.lightgunGame ? awavelg_button_mapping : awave_button_mapping;
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
		case 6:
			if (axis != DC_AXIS_RT2)
				continue;
			break;
		case 7:
			if (axis != DC_AXIS_LT2)
				continue;
			break;
		default:
			continue;
		}
		return NaomiGameInputs->axes[i].name;
	}

	return nullptr;
}

class jvs_io_board;

struct MIEImpl : public MIE
{
	static constexpr u8 ALL_NODES = 0xff;

	std::vector<std::unique_ptr<jvs_io_board>> io_boards;
	bool crazy_mode = false;
	bool hotd2p = false;

	u8 jvs_repeat_request[32][256];
	u8 jvs_receive_buffer[32][258];
	u32 jvs_receive_length[32] = { 0 };
	u8 eeprom[128];

	MIEImpl();
	~MIEImpl() {
		EEPROM = nullptr;
	}

	MapleDeviceType get_device_type() override {
		return MDT_NaomiJamma;
	}

	u8 sense_line(u32 node_id) {
		bool last_node = node_id == io_boards.size();
		return last_node ? 0x8E : 0x8F;
	}

	void send_jvs_message(u32 node_id, u32 channel, u32 length, const u8 *data);
	void send_jvs_messages(u32 node_id, u32 channel, bool use_repeat, u32 length, const u8 *data, bool repeat_first);
	bool receive_jvs_messages(u32 channel);

	void handle_86_subcommand() override;
	void firmwareLoaded(u32 hash) override;

	void serialize(Serializer& ser) const override;
	void deserialize(Deserializer& deser) override;

	void setPipe(Pipe *pipe) override {
		serialPipe = pipe;
	}
	void updateStatus() override {}

	Pipe *serialPipe = nullptr;
};

std::shared_ptr<maple_device> MIE::Create() {
	return std::make_shared<MIEImpl>();
}

/*
 * Sega JVS I/O board
*/
static bool old_coin_chute[4];

class jvs_io_board
{
public:
	jvs_io_board(u8 node_id, MIEImpl *parent, int first_player = 0)
	{
		this->node_id = node_id;
		this->parent = parent;
		this->first_player = first_player;
		init_mappings();
	}
	virtual ~jvs_io_board() = default;

	u32 handle_jvs_message(const u8 *buffer_in, u32 length_in, u8 *buffer_out);
	virtual void serialize(Serializer& ser) const;
	virtual void deserialize(Deserializer& deser);

	u32 getDigitalOutput() const {
		return digOutput;
	}

	bool lightgun_as_analog = false;

protected:
	virtual const char *get_id() = 0;
	virtual u16 read_analog_axis(int player_num, int player_axis, bool inverted);

	virtual void read_digital_in(const u32 *buttons, u32 *v)
	{
		memset(v, 0, sizeof(u32) * 4);
		for (u32 player = first_player; player < 4; player++)
		{
			// always-on mapping
			for (u32 i = 0; i < cur_mapping.size(); i++)
				if (cur_mapping[i] == 0xffffffff)
				{
					if (p2_mapping[i] == 0)
						v[player - first_player] |= 1 << i;
					else if (player == 0)
						v[1] |= p2_mapping[i];
				}
			u32 keycode = buttons[player];
			if (keycode == 0)
				continue;
			if (keycode & NAOMI_RELOAD_KEY)
				keycode |= NAOMI_BTN0_KEY;
			if (lightgun_as_analog && (keycode & NAOMI_BTN0_KEY))
			{
				const MapleInputState& inputState = mapleInputState[player];
				if (inputState.absPos.x < 0 || inputState.absPos.x > 639
						|| inputState.absPos.y < 0 || inputState.absPos.y > 479
						|| (keycode & NAOMI_RELOAD_KEY)) {
					keycode |= NAOMI_BTN1_KEY;	// offscreen sensor, not used by deathcox
				}
			}

			// P1 mapping (only for P2)
			if (player == 1)
			{
				for (u32 i = 0; i < p1_mapping.size(); i++)
					if ((keycode & (1 << i)) != 0)
						v[0] |= p1_mapping[i];
			}
			// normal mapping
			for (u32 i = 0; i < cur_mapping.size(); i++)
				if ((keycode & (1 << i)) != 0 && cur_mapping[i] != 0xffffffff)
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

	virtual void write_digital_out(int count, const u8 *data)
	{
		u32 newOutput = digOutput;
		for (int i = 0; i < count && i < 4; i++)
		{
			u32 mask = 0xff << (i * 8);
			newOutput = (newOutput & ~mask) | (data[i] << (i * 8));
		}
		u32 changes = newOutput ^ digOutput;
		for (int i = 0; i < 32; i++)
			if (changes & (1 << i))
			{
				std::string name = "lamp" + std::to_string(i);
				networkOutput.output(name.c_str(), (newOutput >> i) & 1);
			}
		digOutput = newOutput;
	}

	virtual void read_lightgun(int playerNum, u32 buttons, u16& x, u16& y)
	{
		if ((buttons & NAOMI_RELOAD_KEY) != 0)
		{
			x = 0;
			y = 0;
		}
		else
		{
			x = mapleInputState[playerNum].absPos.x;
			y = mapleInputState[playerNum].absPos.y;
		}
	}
	
	virtual s16 readRotaryEncoders(int channel, s16 relX, s16 relY)
	{
		switch (channel)
		{
			case 0: return relX;
			case 1: return relY;
			default: return 0;
		}
	}

	u32 player_count = 0;
	u32 digital_in_count = 0;
	u32 coin_input_count = 0;
	u32 analog_count = 0;
	u32 encoder_count = 0;
	u32 light_gun_count = 0;
	u32 output_count = 0;
	bool init_in_progress = false;
	MIEImpl *parent;
	u8 first_player;

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

		for (size_t i = 0; i < std::size(NaomiGameInputs->buttons) && NaomiGameInputs->buttons[i].source != 0; i++)
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

	std::array<u32, 32> cur_mapping;
	std::array<u32, 32> p1_mapping;
	std::array<u32, 32> p2_mapping;
	u32 digOutput = 0;
	int coin_count[4] {};
};

// Most common JVS board
class jvs_837_13551 : public jvs_io_board
{
public:
	jvs_837_13551(u8 node_id, MIEImpl *parent, int first_player = 0)
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
	jvs_837_13551_noanalog(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13551(node_id, parent, first_player)
	{
		analog_count = 0;
	}
};

// Same in 4-player mode
class jvs_837_13551_4P : public jvs_837_13551
{
public:
	jvs_837_13551_4P(u8 node_id, MIEImpl *parent, int first_player = 0)
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
	jvs_837_13938(u8 node_id, MIEImpl *parent, int first_player = 0)
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

// Uses btn1 to switch between cue aim and cue roller encoders
class jvs_837_13938_shootout : public jvs_837_13938
{
public:
	jvs_837_13938_shootout(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13938(node_id, parent, first_player)
	{
		memset(lastValue, 0, sizeof(lastValue));
	}

protected:
	void read_digital_in(const u32 *buttons, u32 *v) override
	{
		jvs_837_13938::read_digital_in(buttons, v);
		btn3down = v[0] & NAOMI_BTN3_KEY;
	}

	s16 readRotaryEncoders(int channel, s16 relX, s16 relY) override
	{
		switch (channel)
		{
			case 0: // CUE AIM L/R
				if (!btn3down)
					lastValue[0] = relX;
				break;
			case 1: // CUE AIM U/D
				if (!btn3down)
					lastValue[1] = relY;
				break;
			case 2: // CUE ROLLER
				if (btn3down)
					lastValue[2] = relY;
				break;
			default:
				return 0;
		}
		return lastValue[channel];
	}

	bool btn3down = false;
	s16 lastValue[3];
};

//
// The encoders are rotated by 45° so coordinates must be converted.
// Polling is done twice per frame so we only handle half the delta per poll.
//
class jvs_837_13938_kick4cash : public jvs_837_13938
{
public:
	jvs_837_13938_kick4cash(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13938(node_id, parent, first_player)
	{}

protected:
	s16 readRotaryEncoders(int channel, s16 relX, s16 relY) override
	{
		const s16 deltaX = (relX - prevRelX) / 2;
		const s16 deltaY = (relY - prevRelY) / 2;
		s16 rv;
		switch (channel)
		{
		case 0: // x
			rotX += (deltaX - deltaY) * 0.7071f;
			rv = (int)std::round(rotX);
			break;
		case 1: // y
			rotY += (deltaX + deltaY) * 0.7071f;
			rv = (int)std::round(rotY);
			break;
		default:
			rv = 0;
			break;
		}
		if (channel == 1)
		{
			prevRelX += deltaX;
			prevRelY += deltaY;
		}
		return rv;
	}

	s16 prevRelX = 0;
	s16 prevRelY = 0;
	float rotX = 0.f;
	float rotY = 0.f;
};

class jvs_837_13938_crackindj : public jvs_837_13938
{
public:
	jvs_837_13938_crackindj(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13938(node_id, parent, first_player)
	{}

	void serialize(Serializer& ser) const override
	{
		jvs_837_13938::serialize(ser);
		ser << motorRotation;
	}
	void deserialize(Deserializer& deser) override
	{
		jvs_837_13938::deserialize(deser);
		if (deser.version() >= Deserializer::V46)
			deser >> motorRotation;
	}

protected:
	s16 readRotaryEncoders(int channel, s16 relX, s16 relY) override
	{
		jvs_io_board& outputBoard = *parent->io_boards[1];
		bool turntableOn = outputBoard.getDigitalOutput() & 0x10;
		switch (channel)
		{
			case 0:	// Left turntable
				if (turntableOn && relX == lastRel[0])
					motorRotation[0] -= 10;
				lastRel[0] = relX;
				return -relX + motorRotation[0];
			case 2: // Right turntable
				if (turntableOn && relY == lastRel[1])
					motorRotation[1] -= 10;
				lastRel[1] = relY;
				return relY + motorRotation[1];
			default:
				return 0;
		}
	}

private:
	s16 motorRotation[2]{};
	s16 lastRel[2]{};
};

// Sega Marine Fishing, 18 Wheeler (TODO)
class jvs_837_13844 : public jvs_io_board
{
public:
	jvs_837_13844(u8 node_id, MIEImpl *parent, int first_player = 0)
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
	jvs_837_13844_encoders(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
		digital_in_count = 8;
		encoder_count = 4;
	}
};

class jvs_837_13844_touch : public jvs_837_13844
{
public:
	jvs_837_13844_touch(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
		light_gun_count = 1;
	}

	void read_lightgun(int playerNum, u32 buttons, u16& x, u16& y) override
	{
		jvs_837_13844::read_lightgun(playerNum, buttons, x, y);

		if ((buttons & NAOMI_BTN0_KEY) != 0)
		{
			// any >= 0x1000 value works after calibration (tduno, tduno2)
			// no value seems to fully work before :(
			x |= 0xf000;
			y |= 0xf000;
		}
	}
};

class jvs_837_13844_motor_board : public jvs_837_13844
{
public:
	jvs_837_13844_motor_board(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
	}

	void serialize(Serializer& ser) const override
	{
		ser << out;
		jvs_837_13844::serialize(ser);
	}
	void deserialize(Deserializer& deser) override
	{
		if (deser.version() >= Deserializer::V31)
			deser >> out;
		else
			out = 0xff;
		jvs_837_13844::deserialize(deser);
	}

protected:
	void read_digital_in(const u32 *buttons, u32 *v) override
	{
		jvs_837_13844::read_digital_in(buttons, v);

		// The drive board RX0-7 is connected to the following player inputs
		v[0] |= NAOMI_BTN2_KEY | NAOMI_BTN3_KEY | NAOMI_BTN4_KEY | NAOMI_BTN5_KEY;
		if (out & 16)
			v[0] &= ~NAOMI_BTN5_KEY;
		if (out & 32)
			v[0] &= ~NAOMI_BTN4_KEY;
		if (out & 64)
			v[0] &= ~NAOMI_BTN3_KEY;
		if (out & 128)
			v[0] &= ~NAOMI_BTN2_KEY;
		v[1] |= NAOMI_BTN2_KEY | NAOMI_BTN3_KEY | NAOMI_BTN4_KEY | NAOMI_BTN5_KEY;
		if (out & 1)
			v[1] &= ~NAOMI_BTN5_KEY;
		if (out & 2)
			v[1] &= ~NAOMI_BTN4_KEY;
		if (out & 4)
			v[1] &= ~NAOMI_BTN3_KEY;
		if (out & 8)
			v[1] &= ~NAOMI_BTN2_KEY;
	}

	void write_digital_out(int count, const u8 *data) override
	{
		if (count != 3)
			return;

		// The drive board TX0-7 is connected to outputs 15-22
		// shifting right by 2 to get the last 8 bits of the output
		u16 in = (data[1] << 6) | (data[2] >> 2);
		// reverse
		in = (in & 0xF0) >> 4 | (in & 0x0F) << 4;
		in = (in & 0xCC) >> 2 | (in & 0x33) << 2;
		in = (in & 0xAA) >> 1 | (in & 0x55) << 1;

		out = process(in);
		// The rest of the bits are for lamps
		u8 lamps[2] = { data[0], (u8)(data[1] & 0xfc) };
		jvs_837_13844::write_digital_out(2, lamps);
	}

	virtual u8 process(u8 in) = 0;

protected:
	u8 out = 0;	// output from motor board
};

// Wave Runner GP: fake the drive board
class jvs_837_13844_wrungp : public jvs_837_13844_motor_board
{
public:
	jvs_837_13844_wrungp(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13844_motor_board(node_id, parent, first_player)
	{
	}

protected:
	u8 process(u8 in) override
	{
		u8 out = ~this->out;
		if (in == 0xff)
			out = 0;
		else if ((in & 0xf) == 0xf)
		{
			in >>= 4;
			if (in > 7)
				out = 1 << (14 - in);
			else
				out = 1 << in;
		}
		else if ((in & 0xf0) == 0xf0)
		{
			in &= 0xf;
			if (in > 7)
				out = 1 << (in - 7);
			else
				out = 1 << (7 - in);
		}

		return ~out;
	}
};

// 837-13844 jvs board wired to force-feedback drive board
// 838-13843: f355 (ROM EPR-21867)
// 838-13992: 18 wheeler (ROM EPR-23000)
// with 838-12912-01 servo board (same as model3)
// https://github.com/njz3/vJoyIOFeederWithFFB/blob/master/DRIVEBOARD.md
class jvs_837_13844_racing : public jvs_837_13844_motor_board
{
public:
	jvs_837_13844_racing(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13844_motor_board(node_id, parent, first_player)
	{
	}

	void serialize(Serializer& ser) const override
	{
		ser << testMode;
		ser << damper_high;
		ser << active;
		ser << motorPower;
		ser << springSat;
		ser << springSpeed;
		ser << torque;
		ser << damper;
		jvs_837_13844_motor_board::serialize(ser);
	}
	void deserialize(Deserializer& deser) override
	{
		if (deser.version() >= Deserializer::V31)
			deser >> testMode;
		else
			testMode = false;
		if (deser.version() >= Deserializer::V51)
		{
			deser >> damper_high;
			deser >> active;
			deser >> motorPower;
			deser >> springSat;
			deser >> springSpeed;
			deser >> torque;
			deser >> damper;
		}
		else
		{
			damper_high = 8;
			active = false;
			motorPower = 1.f;
			springSat = 0.f;
			springSpeed = 0.f;
			torque = 0.f;
			damper = 0.f;
		}
		jvs_837_13844_motor_board::deserialize(deser);
		if (active)
		{
			haptic::setSpring(0, springSat, springSpeed);
			haptic::setTorque(0, torque);
			haptic::setDamper(0, damper, damper / 2.f);
		}
		else {
			haptic::stopAll(0);
		}
	}

	void setTokyoBusMode(bool enable) {
		tokyoBus = enable;
	}

protected:
	u8 process(u8 in) override
	{
		in = ~in;
		networkOutput.output("m3ffb", in);

		u8 out = 0;
		switch (in)
		{
		case 0:
		case 1:
		case 2:
			// 18wheeler: 0 light (60%), 1 normal (80%), 2 heavy (100%)
			if (!active)
				motorPower = 0.6f + in * 0.2f;
			break;

		case 0xf0:
			active = false;
			testMode = true;
			break;

		case 0xfe:
			active = true;
			break;

		case 0xff:
			testMode = false;
			active = false;
			haptic::stopAll(0);
			break;

		case 0xf1:
			out = 0x10; // needed by f355 and tokyobus
			break;

		case 0xfa:
			motorPower = 1.f;	// f355: 100%
			break;
		case 0xfb:
			motorPower = 0.9f;	// f355: 90%
			break;
		case 0xfc:
			motorPower = 0.8f;	// f355: 80%
			break;
		case 0xfd:
			motorPower = 0.6f;	// f355: 60%
			break;

		default:
			if (active)
			{
				if (in >= 0x40 && in <= 0x7f)
				{
					if (tokyoBus)
					{
						if (in <= 0x4a)
							// heavy: 4a
							// normal: 47
							// light: 44
							motorPower = (in & 0xf) / 10.f;
					}
					else
					{
						// Spring
						// bits 0-3 sets the strength of the spring effect
						// bits 4-5 selects a table scaling the effect given the deflection:
						//     (from the f355 rom EPR-21867)
						//     0: large deadzone then abrupt scaling (0 (* 129), 10, 20, 30, 40, 50, 60, 70, 7F)
						//     used by 18wheeler in game but with a different rom (TODO reveng)
						//     other tables scale linearly:
						//     1: light speed (96 steps from 0 to 7f)
						//     2: medium speed (48 steps, default)
						//     3: high speed (32 steps)
						springSat = (in & 0xf) / 15.f * motorPower;
						const int speedSel = (in >> 4) & 3;
						springSpeed = speedSel == 3 ? 1.f : speedSel == 2 ? 0.67f : speedSel == 1 ? 0.33f : 0.67f;
						haptic::setSpring(0, springSat, springSpeed);
					}
				}
				else if (in >= 0x80 && in <= 0xbf)
				{
					// Rumble
					const float v = (in & 0x3f) / 63.f * motorPower / 2.f;	// additional 0.5 factor to soften it
					haptic::setSine(0, v, 25.f, 650);	// 25 Hz, 650 ms
				}
				else if (in >= 0xe0 && in <= 0xef)
				{
					// Test menu roll left/right (not used in game)
					torque = (in < 0xe8 ? (0xe0 - in) : (in - 0xe8)) / 7.f;
					haptic::setTorque(0, torque);
				}
				else if ((in & 0xf0) == 0xc0)
				{
					// Damper? more likely Friction
					// activated in f355 when turning the wheel while stopped
					// high-order bits are set with Dn, low-order bits with Cn. Only the later commits the change.
					const u8 v = (damper_high << 4) | (in & 0xf);
					damper = std::abs(v - 0x80) / 128.f * motorPower;
					haptic::setDamper(0, damper, damper / 2.f);
				}
				else if ((in & 0xf0) == 0xd0) {
					damper_high = in & 0xf;
				}
			}
			break;
		}
		if (testMode)
			out = in;

		// reverse
		out = (out & 0xF0) >> 4 | (out & 0x0F) << 4;
		out = (out & 0xCC) >> 2 | (out & 0x33) << 2;
		out = (out & 0xAA) >> 1 | (out & 0x55) << 1;

		return out;
	}

private:
	bool testMode = false;
	u8 damper_high = 8;
	bool active = false;
	float motorPower = 1.f;
	float springSat = 0.f;
	float springSpeed = 0.f;
	float torque = 0.f;
	float damper = 0.f;
	bool tokyoBus = false;
};

// 18 Wheeler: fake the drive board and limit the wheel analog value
class jvs_837_13844_18wheeler : public jvs_837_13844_racing
{
public:
	jvs_837_13844_18wheeler(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13844_racing(node_id, parent, first_player)
	{
	}

	void serialize(Serializer& ser) const override
	{
		ser << gear;
		jvs_837_13844_racing::serialize(ser);
	}
	void deserialize(Deserializer& deser) override
	{
		if (deser.version() >= Deserializer::V31)
			deser >> gear;
		jvs_837_13844_racing::deserialize(deser);
	}

protected:
	void read_digital_in(const u32 *buttons, u32 *v) override
	{
		jvs_837_13844_racing::read_digital_in(buttons, v);
		if (buttons[0] & NAOMI_BTN2_KEY)
		{
			gear = -1;
		}
		else if (buttons[0] & NAOMI_BTN1_KEY)
		{
			if (!transitionWait)
			{
				gear = gear == 0 ? 1 : 0;
				transitionWait = true;
			}
		}
		else
		{
			transitionWait = false;
		}

		switch (gear)
		{
		case -1:
			v[1] |= NAOMI_LEFT_KEY | NAOMI_DOWN_KEY;
			v[1] &= ~NAOMI_UP_KEY;
			break;
		case 0:
		default:
			v[1] |= NAOMI_DOWN_KEY;
			v[1] &= ~(NAOMI_UP_KEY | NAOMI_LEFT_KEY);
			break;
		case 1:
			v[1] &= ~(NAOMI_LEFT_KEY | NAOMI_DOWN_KEY);
			v[1] |= NAOMI_UP_KEY;
			break;
		}
	}

	u16 read_analog_axis(int player_num, int player_axis, bool inverted) override
	{
		u16 v = jvs_837_13844_racing::read_analog_axis(player_num, player_axis, inverted);
		if (player_axis == 0)
			return std::min<u16>(0xefff, std::max<u16>(0x1000, v));
		else
			return v;
	}

private:
	int8_t gear = 0;	// 0: low, 1: high, -1: reverse
	bool transitionWait = false;
};

// Ninja assault
class jvs_namco_jyu : public jvs_io_board
{
public:
	jvs_namco_jyu(u8 node_id, MIEImpl *parent, int first_player = 0)
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
	jvs_namco_fcb(u8 node_id, MIEImpl *parent, int first_player = 0)
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
		const MapleInputState& inputState = mapleInputState[std::min(player_num, (int)std::size(mapleInputState) - 1)];
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
	jvs_namco_fca(u8 node_id, MIEImpl *parent, int first_player = 0)
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
	jvs_namco_v226(u8 node_id, MIEImpl *parent, int first_player = 0)
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

	void read_digital_in(const u32 *buttons, u32 *v) override
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
		s8 axis_x = mapleInputState[joy_num].fullAxes[PJAI_X1] >> 8;
		axis_y = mapleInputState[joy_num].fullAxes[PJAI_Y1] >> 8;
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
			return mapleInputState[0].halfAxes[PJTI_R];
		case 9:
			return mapleInputState[1].halfAxes[PJTI_R];
		case 10:
			return mapleInputState[2].halfAxes[PJTI_R];
		case 11:
			return mapleInputState[3].halfAxes[PJTI_R];
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
	jvs_namco_v226_pcb(u8 node_id, MIEImpl *parent, int first_player = 0)
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

	void read_digital_in(const u32 *buttons, u32 *v) override
	{
		jvs_io_board::read_digital_in(buttons, v);
		for (u32 player = first_player; player < first_player + player_count && player < 4; player++)
		{
			u8 trigger = mapleInputState[player].halfAxes[PJTI_R] >> 10;
			const u32 idx = player - first_player;
					// Ball button
			v[idx] = ((trigger & 0x20) << 3) | ((trigger & 0x10) << 5) | ((trigger & 0x08) << 7)
					| ((trigger & 0x04) << 9) | ((trigger & 0x02) << 11) | ((trigger & 0x01) << 13)
					// other buttons
					| (v[idx] & (NAOMI_SERVICE_KEY | NAOMI_TEST_KEY | NAOMI_START_KEY))
					| ((v[idx] & NAOMI_BTN0_KEY) >> 4);		// remap button4 to button0 (change button)
		}
	}

	u16 read_joystick_x(int joy_num)
	{
		s8 axis_x = mapleInputState[joy_num].fullAxes[PJAI_X1] >> 8;
		axis_y = mapleInputState[joy_num].fullAxes[PJAI_Y1] >> 8;
		limit_joystick_magnitude<48>(axis_x, axis_y);
		return (axis_x + 128) << 8;
	}

	u16 read_joystick_y(int joy_num)
	{
		return std::min(0xff, 0x80 - axis_y) << 8;
	}

	u16 read_analog_axis(int player_num, int player_axis, bool inverted) override
	{
		switch (player_axis)
		{
		// P1
		case 0:
			return read_joystick_x(player_num + 0);
		case 1:
			return read_joystick_y(player_num + 0);
		// P2 & P4
		case 4:
			return read_joystick_x(player_num + 1);
		case 5:
			return read_joystick_y(player_num + 1);
		// P3
		case 8:
			if (player_num == 0)
				return read_joystick_x(player_num + 2);
			else
				return 0x8000;
		case 9:
			if (player_num == 0)
				return read_joystick_y(player_num + 2);
			else
				return 0x8000;
		default:
			return 0x8000;
		}
	}

private:
	s8 axis_y = 0;
};

// Mushiking
class jvs_837_13551_mushiking : public jvs_837_13551
{
public:
	jvs_837_13551_mushiking(u8 node_id, MIEImpl *parent, int first_player = 0)
		: jvs_837_13551(node_id, parent, first_player) { }

protected:
	void read_digital_in(const u32 *buttons, u32 *v) override
	{
		jvs_837_13551::read_digital_in(buttons, v);
		if (!(v[0] & NAOMI_TEST_KEY))
		{
			v[0] |= NAOMI_START_KEY;	// Card dispenser OK
			testDown = false;
		}
		else if (!testDown)
		{
			// Delay the TEST button being down until the next read,
			// otherwise it can't be used in the BIOS menu
			testDown = true;
			v[0] &= ~NAOMI_TEST_KEY;
		}
		v[0] &= ~NAOMI_RIGHT_KEY;	// Cancel card dispenser empty signal
		v[1] &= ~NAOMI_START_KEY;	// Cancel jammed card signal
	}

private:
	bool testDown = false;
};

//
// Base class for the regular MIE and the RFID card reader/writer
//
void BaseMIE::reply(u8 code, u8 sizew)
{
	w8(code);
	w8(0x00);
	w8(0x20);
	w8(sizew);
}

u32 BaseMIE::RawDma(const u32 *buffer_in, u32 buffer_in_len, u32 *buffer_out)
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

	const u32 cmd = *(u8*)buffer_in;

	// delegate to subclass first
	dma_buffer_out += 4;
	u32 resp = dma(cmd);
	if (resp != MDRE_UnknownCmd)
	{
		const u32 reci = (buffer_in[0] >> 8) & 0xFF;
		const u32 sender = (buffer_in[0] >> 16) & 0xFF;
		buffer_out[0] = (resp << 0 ) | (sender << 8) | (reci << 16) | ((out_len / 4) << 24);
		return out_len + 4;
	}
	else {
		// reset buffer index and continue
		dma_buffer_out -= 4;
	}

	switch (cmd)
	{
		case MDC_JVSCommand:
			handle_86_subcommand();
			break;

		case MDC_JVSUploadFirmware:
		{
			static u8 *ram;

			if (ram == nullptr)
				ram = (u8 *)calloc(0x10000, 1);

			if (dma_buffer_in[1] == 0xff)
			{
				u32 hash = XXH32(ram, 0x10000, 0);
				LOGJVS("JVS Firmware hash %08x", hash);
				firmwareLoaded(hash);
#ifdef DUMP_JVS_FW
				FILE *fw_dump;
				char filename[128];
				for (int i = 0; ; i++)
				{
					snprintf(filename, sizeof(filename), "z80_fw_%d.bin", i);
					fw_dump = fopen(filename, "r");
					if (fw_dump == NULL)
					{
						fw_dump = fopen(filename, "w");
						INFO_LOG(JVS, "Saving JVS firmware to %s", filename);
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
				ram = nullptr;

				reply(MDRS_DeviceReply);
				break;
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

			reply(MDC_JVSUploadFirmware, 1);	// or 0x81 on bootrom?
			w32(sum);

			reply(MDRS_DeviceReply);
		}
		break;

	case MDC_JVSGetId:
		{
			DEBUG_LOG(JVS, "bus[%d] JVS Get Id", bus_id);
			static const char ID[56] = "315-6149    COPYRIGHT SEGA ENTERPRISES CO,LTD.  1998";
			reply(MDRS_JVSGetIdReply, 7);
			wptr(ID, 28);

			// TODO 6 or 7 breaks card reader... It looks like the reader returns 2*7 words so why?
			reply(MDRS_JVSGetIdReply, 5);
			wptr(ID + 28, 20);
		}
		break;

	default:
		INFO_LOG(MAPLE, "BaseMIE: Unknown Maple command %x", cmd);
		reply(MDRE_UnknownCmd);
		break;;
	}

#ifdef DUMP_JVS
	printf("JVS OUT: ");
	p = (u8 *)buffer_out;
	for (int i = 0; i < out_len; i++) printf("%02x ", p[i]);
	printf("\n");
#endif

	return out_len;
}

u32 BaseMIE::dma(u32 cmd)
{
	switch (cmd)
	{
	case MDC_JVSSelfTest:
		w32(0);
		return MDRS_JVSSelfTestReply;

	case MDC_DeviceRequest:
		return MDRS_DeviceStatus;

	case MDC_AllStatusReq:
		return MDRS_DeviceStatusAll;

	case MDC_DeviceReset:
	case MDC_DeviceKill:
		return MDRS_DeviceReply;

	default:
		return MDRE_UnknownCmd;
	}
}

void BaseMIE::handle_86_subcommand()
{
	INFO_LOG(JVS, "BaseMIE: Unhandled JVS command (0x86) subcode %x", dma_buffer_in[0]);
	reply(MDRE_UnknownCmd);
}

MIEImpl::MIEImpl()
{
	if (settings.naomi.drivingSimSlave == 0 && !settings.naomi.slave)
	{
		const std::string& gameId = settings.content.gameId;
		if (gameId == "POWER STONE 2 JAPAN")
		{
			// 4 players
			INFO_LOG(MAPLE, "Enabling 4-player setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13551_4P>(1, this));
			settings.input.fourPlayerGames = true;
		}
		else if (gameId == "DYNAMIC GOLF"
				|| gameId.substr(0, 13) == "SHOOTOUT POOL"
				|| gameId.substr(0, 10) == "CRACKIN'DJ"
				|| gameId == "KICK '4' CASH")
		{
			// Rotary encoders
			INFO_LOG(MAPLE, "Enabling JVS rotary encoders for game %s", gameId.c_str());
			if (gameId.substr(0, 13) == "SHOOTOUT POOL")
				io_boards.push_back(std::make_unique<jvs_837_13938_shootout>(1, this));
			else if (gameId == "KICK '4' CASH")
				io_boards.push_back(std::make_unique<jvs_837_13938_kick4cash>(1, this));
			else if (gameId.substr(0, 10) == "CRACKIN'DJ")
				io_boards.push_back(std::make_unique<jvs_837_13938_crackindj>(1, this));
			else
				io_boards.push_back(std::make_unique<jvs_837_13938>(1, this));
			io_boards.push_back(std::make_unique<jvs_837_13551>(2, this));
			settings.input.mouseGame = true;
		}
		else if (gameId == "OUTTRIGGER     JAPAN")
		{
			INFO_LOG(MAPLE, "Enabling JVS rotary encoders for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13938>(1, this));
			io_boards.push_back(std::make_unique<jvs_837_13551_noanalog>(2, this));
		}
		else if (gameId == "SEGA MARINE FISHING JAPAN")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13844>(1, this));
		}
		else if (gameId == "RINGOUT 4X4 JAPAN")
		{
			// Dual I/O boards 4P
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13551>(1, this));
			io_boards.push_back(std::make_unique<jvs_837_13551>(2, this, 2));
			settings.input.fourPlayerGames = true;
		}
		else if (gameId == "VIRTUA ATHLETE"
					|| gameId == "ROYAL RUMBLE"
					|| gameId == "BEACH SPIKERS JAPAN"
					|| gameId == "MJ JAPAN")
		{
			// Dual I/O boards 4P
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			// reverse the board order so that P1 is P1
			io_boards.push_back(std::make_unique<jvs_837_13551>(1, this, 2));
			io_boards.push_back(std::make_unique<jvs_837_13551>(2, this, 0));
			settings.input.fourPlayerGames = true;
		}
		else if (gameId == "NINJA ASSAULT")
		{
			// Light-gun game
			INFO_LOG(MAPLE, "Enabling lightgun setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_namco_jyu>(1, this));
			settings.input.lightgunGame = true;
		}
		else if (gameId == "THE MAZE OF THE KINGS"
				|| gameId == " CONFIDENTIAL MISSION ---------"
				|| gameId == "DEATH CRIMSON OX"
				|| gameId.substr(0, 5) == "hotd2"	// House of the Dead 2
				|| gameId == "LUPIN THE THIRD  -THE SHOOTING-")
		{
			INFO_LOG(MAPLE, "Enabling lightgun as analog setup for game %s", gameId.c_str());
			// Regular board sending lightgun coords as axis 0/1
			io_boards.push_back(std::make_unique<jvs_837_13551>(1, this));
			io_boards.back()->lightgun_as_analog = true;
			settings.input.lightgunGame = true;
		}
		else if (gameId == "MAZAN")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_namco_fcb>(1, this));
			io_boards.push_back(std::make_unique<jvs_namco_fcb>(2, this));
			settings.input.lightgunGame = true;
		}
		else if (gameId == " BIOHAZARD  GUN SURVIVOR2")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_namco_fca>(1, this));
		}
		else if (gameId == "INU NO OSANPO")	// Dog Walking
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13844_encoders>(1, this));
		}
		else if (gameId == " TOUCH DE UNOH -------------"
				|| gameId == " TOUCH DE UNOH 2 -----------"
				|| gameId == "MIRAI YOSOU STUDIO")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13844_touch>(1, this));
			settings.input.lightgunGame = true;
		}
		else if (gameId == "WORLD KICKS")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_namco_v226>(1, this));
			settings.input.fourPlayerGames = true;
		}
		else if (gameId == "WORLD KICKS PCB")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_namco_v226_pcb>(1, this, 2));
			io_boards.push_back(std::make_unique<jvs_namco_v226_pcb>(2, this));
			settings.input.fourPlayerGames = true;
		}
		else if (gameId == "WAVE RUNNER GP")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13844_wrungp>(1, this));
		}
		else if (gameId == "  18WHEELER")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13844_18wheeler>(1, this));
		}
		else if (gameId == "F355 CHALLENGE JAPAN" || gameId == "TOKYO BUS GUIDE")
		{
			INFO_LOG(MAPLE, "Enabling specific JVS setup for game %s", gameId.c_str());
			io_boards.push_back(std::make_unique<jvs_837_13844_racing>(1, this));
			if (gameId == "TOKYO BUS GUIDE")
				static_cast<jvs_837_13844_racing *>(io_boards.back().get())->setTokyoBusMode(true);
		}
		else if (gameId.substr(0, 8) == "MKG TKOB"
				|| gameId.substr(0, 9) == "MUSHIKING"
				|| gameId == "MUSHIUSA '04 1ST VER0.900-")
		{
			io_boards.push_back(std::make_unique<jvs_837_13551_mushiking>(1, this));
		}
		else if (gameId == "ANPANMAN POPCORN KOUJOU 2")
		{
			io_boards.push_back(std::make_unique<jvs_837_13844>(1, this));
		}
		else
		{
			if (gameId == "POKASUKA GHOST (JAPANESE)"	// Manic Panic Ghosts
				|| gameId == "TOUCH DE ZUNO (JAPAN)")
			{
				settings.input.lightgunGame = true;
			}
			// Default JVS I/O board
			io_boards.push_back(std::make_unique<jvs_837_13551>(1, this));
		}
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

void MIEImpl::send_jvs_message(u32 node_id, u32 channel, u32 length, const u8 *data)
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

void MIEImpl::send_jvs_messages(u32 node_id, u32 channel, bool use_repeat, u32 length, const u8 *data, bool repeat_first)
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

bool MIEImpl::receive_jvs_messages(u32 channel)
{
	constexpr u32 headerLength = sizeof(u32) * 5 + 3;
	u32 dword_length = (jvs_receive_length[channel] + headerLength - 1) / 4 + 1;

	if (jvs_receive_length[channel] == 0) {
		reply(MDRS_JVSReply, 5);
		w8(0x32);
	}
	else {
		reply(MDRS_JVSReply, dword_length);
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
	memset(dma_buffer_out + jvs_receive_length[channel], 0, dword_length * 4 - headerLength - jvs_receive_length[channel]);
	dma_buffer_out += dword_length * 4 - headerLength;
	*dma_count_out += dword_length * 4 - headerLength;
	jvs_receive_length[channel] = 0;

	return true;
}

void MIEImpl::handle_86_subcommand()
{
	if (dma_count_in == 0) {
		reply(MDRS_JVSReply);
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
	const u8 *cmd = NULL;
	u32 len = 0;
	u8 channel = 0;
	if (dma_count_in >= 3)
	{
		if (subcode != 0x13 && dma_count_in >= 8)
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
			reply(MDRS_JVSReply, 1);
			w8(dma_buffer_in[0] + 1);	// subcommand + 1
			w8(0);
			w8(len + 1);
			w8(0);
			break;

		case 0x15:	// Receive JVS data
			receive_jvs_messages(dma_buffer_in[1]);
			if (hotd2p)
			{
				send_jvs_messages(node_id, channel, true, len, cmd, false);
				reply(MDRS_JVSReply, 1);
				w8(0x18);	// always
				w8(channel);
				w8(sense_line(node_id));
				w8(0);
			}
			break;

		case 0x17:	// Transmit without repeat
			jvs_receive_length[channel] = 0;
			send_jvs_messages(node_id, channel, false, len, cmd, false);
			reply(MDRS_JVSReply, 1);
			w8(0x18);	// always
			w8(channel);
			w8(0x8E);	//sense_line(node_id));
			w8(0);
			break;

		case 0x19:	// Transmit with repeat
			jvs_receive_length[channel] = 0;
			send_jvs_messages(node_id, channel, true, len, cmd, true);
			reply(MDRS_JVSReply, 1);
			w8(0x18);	// always
			w8(channel);
			w8(sense_line(node_id));
			w8(0);
			break;

		case 0x21:	// Transmit with repeat
			jvs_receive_length[channel] = 0;
			send_jvs_messages(node_id, channel, true, len, cmd, false);
			reply(MDRS_JVSReply, 1);
			w8(0x18);	// always
			w8(channel);
			w8(sense_line(node_id));
			w8(0);
			break;

		case 0x35:	// Receive then transmit with repeat (15 then 27)
			receive_jvs_messages(channel);
			[[fallthrough]];

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

				reply(MDRS_JVSReply, 1);
				w8(0x26);
				w8(channel);
				w8(sense_line(node_id));
				w8(0);
			}
			break;

		case 0x33:	// Receive then transmit with repeat (15 then 21)
			receive_jvs_messages(channel);
			send_jvs_messages(node_id, channel, true, len, cmd, false);
			reply(MDRS_JVSReply, 1);
			w8(0x18);	// always
			w8(channel);
			w8(sense_line(node_id));
			w8(0);
			break;

		case 0x0B:	//EEPROM write
		{
			int address = dma_buffer_in[1];
			int size = dma_buffer_in[2];
			DEBUG_LOG(MAPLE, "EEprom write %08x %08x", address, size);
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

			reply(MDRS_JVSReply, 1);
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
			reply(MDRS_JVSReply, sizeof(eeprom) / 4);
			int size = sizeof(eeprom) - address;
			memcpy(dma_buffer_out, eeprom + address, size);
			dma_buffer_out += size;
			*dma_count_out += size;
		}
		break;

		case 0x31:	// DIP switches
		{
			reply(MDRS_JVSReply, 5);

			w8(0x32);
			w8(0xff);		// in(0)
			w8(0xff);		// in(1)
			w8(0xff);		// in(2)

			w8(0x00);
			w8(0xff);		// in(4)
			u8 v = 0xf9;
			if (settings.naomi.drivingSimSlave == 1)
				v |= 2;
			else if (settings.naomi.drivingSimSlave == 2)
				v |= 4;
			w8(v);          // in(5) bit0: 1=VGA, 0=NTSCi
			w8(0xff);		// in(6)

			w32(0x00);
			w32(0x00);
			w32(0x00);
		}
		break;

		//case 0x3:
		//	break;

		case 0x1:
			reply(MDRS_JVSReply, 1);
			w8(0x2);
			w8(0x0);
			w8(0x0);
			w8(0x0);
			break;

		// RS422 port
		case 0x41: // reset?
			DEBUG_LOG(MAPLE, "JVS: RS422 reset");
			if (serialPipe != nullptr)
				while (serialPipe->available())
					serialPipe->read();
			break;

		case 0x47: // send data
			DEBUG_LOG(MAPLE, "JVS: RS422 send %02x", dma_buffer_in[4]);
			if (serialPipe != nullptr)
				serialPipe->write(dma_buffer_in[4]);
			break;

		case 0x4d: // receive data
			{
				int avail = 0;
				if (serialPipe != nullptr)
					avail = std::min(serialPipe->available(), 0xfe);
				DEBUG_LOG(MAPLE, "JVS: RS422 receive %d bytes", avail);
				reply(MDRS_JVSReply, 1 + (avail + 3) / 4);
				w8(0);
				w8(0);
				w8(0);
				w8(avail == 0 ? 0xff : avail); // 0xff => no data, else byte count

				for (int i = 0; i < ((avail + 3) / 4) * 4; i++)
					w8(i >= avail ? 0 : serialPipe->read());
				break;
			}

		case 0x49: // I?
		case 0x4b: // K?
		case 0x4f: // O?
			//DEBUG_LOG(MAPLE, "JVS: 0x86,%02x RS422 len %d", subcode, dma_count_in - 3);
			break;

		default:
			INFO_LOG(MAPLE, "JVS: Unknown 0x86 sub-command %x", subcode);
			reply(MDRE_UnknownCmd);
			break;
	}
}

void MIEImpl::firmwareLoaded(u32 hash)
{
	hotd2p = hash == 0xa6784e26;
	if (hash == 0xa7c50459	// CT
			|| hash == 0xae841e36	// HOTD2
			|| hotd2p)
		crazy_mode = true;
	else
		crazy_mode = false;
	for (int i = 0; i < 32; i++)
		jvs_repeat_request[i][0] = 0;
}

void MIEImpl::serialize(Serializer& ser) const
{
	maple_base::serialize(ser);
	ser << crazy_mode;
	ser << hotd2p;
	ser << jvs_repeat_request;
	ser << jvs_receive_length;
	ser << jvs_receive_buffer;
	ser << eeprom;
	u32 board_count = io_boards.size();
	ser << board_count;
	for (u32 i = 0; i < io_boards.size(); i++)
		io_boards[i]->serialize(ser);
}
void MIEImpl::deserialize(Deserializer& deser)
{
	maple_base::deserialize(deser);
	deser >> crazy_mode;
	if (deser.version() >= Deserializer::V35)
		deser >> hotd2p;
	else
		hotd2p = settings.content.gameId == "hotd2p";
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
		v = mapleInputState[player_num].fullAxes[player_axis] + 0x8000;
	else
		v = 0x8000;
	return inverted ? 0xffff - v : v;
}

#define JVS_OUT(b) buffer_out[length++] = b
#define JVS_STATUS1() JVS_OUT(1)

u32 jvs_io_board::handle_jvs_message(const u8 *buffer_in, u32 length_in, u8 *buffer_out)
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
		if ((jvs_cmd >= 0x20 && jvs_cmd <= 0x38) || jvs_cmd == 0x74) // Read inputs and more
		{
			LOGJVS("JVS Node %d: ", node_id);
			u32 buttons[4] {};
#ifdef LIBRETRO
			for (int p = 0; p < 4; p++)
				buttons[p] = ~mapleInputState[p].kcode;
#else
			for (u32 i = 0; i < std::size(naomi_button_mapping); i++)
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

						u32 inputs[4];
						read_digital_in(buttons, inputs);
						JVS_OUT((inputs[0] & NAOMI_TEST_KEY) ? 0x80 : 0x00); // test, tilt1, tilt2, tilt3, unused, unused, unused, unused
						LOGJVS("btns ");
						for (int player = 0; player < buffer_in[cmdi + 1]; player++)
						{
							LOGJVS("P%d %02x ", player + 1 + first_player, (inputs[player] >> 8) & 0xFF);
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
									coin_count[slot] += 1;
							}
							old_coin_chute[first_player + slot] = coin_chute;

							LOGJVS("%d:%d ", slot + 1 + first_player, coin_count[first_player + slot]);
							// status (2 highest bits, 0: normal), coin count MSB
							JVS_OUT((coin_count[slot] >> 8) & 0x3F);
							// coin count LSB
							JVS_OUT(coin_count[slot]);
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
								const MapleInputState& inputState = mapleInputState[std::min(playerNum, (int)std::size(mapleInputState) - 1)];
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
								if (player_axis >= std::size(NaomiGameInputs->axes)
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
										switch (axisDesc.axis)
										{
										case 4:
											axis_value = mapleInputState[player_num].halfAxes[PJTI_R];
											break;
										case 5:
											axis_value = mapleInputState[player_num].halfAxes[PJTI_L];
											break;
										case 6:
											axis_value = mapleInputState[player_num].halfAxes[PJTI_R2];
											break;
										case 7:
											axis_value = mapleInputState[player_num].halfAxes[PJTI_L2];
											break;
										default:
											axis_value = 0;
											break;
										}
										if (axisDesc.inverted)
											axis_value = 0xffffu - axis_value;
										// this fixes kingrt66 immediate win
										if (axis_value >= 0x8000 && axis_value < 0x8100)
											axis_value = 0x8100;
									}
									else
									{
										axis_value = read_analog_axis(player_num, axisDesc.axis, axisDesc.inverted);
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
							// Strangely, the least significant byte appears to be handled as signed,
							// so we compensate when it's negative.
							// Avoid overflow (wild riders)
							axis_value = std::min<u16>(0xff7f, axis_value);
							if (axis_value & 0x80)
								axis_value += 0x100;
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
						rotx += mapleInputState[first_player].relPos.x * 3;
						roty -= mapleInputState[first_player].relPos.y * 3;
						mapleInputState[first_player].relPos.x = 0;
						mapleInputState[first_player].relPos.y = 0;
						LOGJVS("rotenc ");
						for (int chan = 0; chan < buffer_in[cmdi + 1]; chan++)
						{
							s16 v = readRotaryEncoders(chan, rotx, roty);
							LOGJVS("%d:%4x ", chan, v & 0xFFFF);
							JVS_OUT(v >> 8);	// MSB
							JVS_OUT(v);			// LSB
						}
						cmdi += 2;
					}
					break;

				case 0x25:	// Read screen pos inputs
					{
						JVS_STATUS1();	// report byte

						// Channel number (1-based) is in jvs_request[cmdi + 1]
						int playerNum = first_player + buffer_in[cmdi + 1] - 1;
						u16 x;
						u16 y;
						read_lightgun(playerNum, buttons[playerNum], x, y);
						LOGJVS("lightgun %4x,%4x ", x, y);
						JVS_OUT(x >> 8);		// X, MSB
						JVS_OUT(x);				// X, LSB
						JVS_OUT(y >> 8);		// Y, MSB
						JVS_OUT(y);				// Y, LSB

						cmdi += 2;
					}
					break;

				case 0x32:	// switched outputs
					LOGJVS("output(%d) %x", buffer_in[cmdi + 1], buffer_in[cmdi + 2]);
					write_digital_out(buffer_in[cmdi + 1], &buffer_in[cmdi + 2]);
					JVS_STATUS1();	// report byte
					cmdi += buffer_in[cmdi + 1] + 2;
					break;

				case 0x33:  // Analog output
					LOGJVS("analog output(%d) %x", buffer_in[cmdi + 1], buffer_in[cmdi + 2]);
					JVS_STATUS1();	// report byte
					cmdi += buffer_in[cmdi + 1] + 2;
					break;

				case 0x30:	// substract coin
					if (buffer_in[cmdi + 1] > 0 && buffer_in[cmdi + 1] - 1 < (int)std::size(coin_count))
						coin_count[buffer_in[cmdi + 1] - 1] -= (buffer_in[cmdi + 2] << 8) + buffer_in[cmdi + 3];
					JVS_STATUS1();	// report byte
					cmdi += 4;
					break;

				case 0x74:  // Custom: used to read and write the board serial port (touch de uno)
					{
						u32 len = buffer_in[cmdi + 1];
						for (u32 i = 0; i < len; i++)
							printer::print(buffer_in[cmdi + 2 + i]);

						cmdi += len + 2;
						JVS_STATUS1();	// report
						// data
						// 00 hardware error
						// 01 head up error
						// 02 Vp Volt error
						// 03 auto cutter error
						// 04 head temp error
						// 3* paper end error
						JVS_OUT(0xf); // printer ok
					}
					break;

				default:
					INFO_LOG(MAPLE, "JVS: Unknown input type %x", buffer_in[cmdi]);
					JVS_OUT(2);			// report byte: command error
					cmdi = length_in;	// Ignore subsequent commands
					break;
				}
			}
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
	ser << coin_count;
}
void jvs_io_board::deserialize(Deserializer& deser)
{
	deser >> node_id;
	deser >> lightgun_as_analog;
	if (deser.version() >= Deserializer::V46)
		deser >> coin_count;
	else
		memset(coin_count, 0, sizeof(coin_count));
}

// Emulates a 838-14245-92 maple to RS232 converter
// wired to a 838-14243 RFID reader/writer (apparently Saxa HW210)
struct RFIDReaderWriterImpl : public RFIDReaderWriter
{
	MapleDeviceType get_device_type() override {
		return MDT_RFIDReaderWriter;
	}
	void OnSetup() override;

	u32 RawDma(const u32 *buffer_in, u32 buffer_in_len, u32 *buffer_out) override;
	u32 dma(u32 command) override;
	void serialize(Serializer& ser) const override;
	void deserialize(Deserializer& deser) override;

	void insertCard();
	const u8 *getCardData();
	void setCardData(u8 *data);

private:
	u32 getStatus() const;
	std::string getCardPath() const;
	void loadCard();
	void saveCard() const;

	u8 cardData[128];
	bool d4Seen = false;
	bool cardInserted = false;
	bool cardLocked = false;
	bool transientData = false;
};

std::shared_ptr<maple_device> RFIDReaderWriter::Create() {
	return std::make_shared<RFIDReaderWriterImpl>();
}

u32 RFIDReaderWriterImpl::getStatus() const
{
	// b0: !card switch
	// b1: state=4	errors?
	// b2: state=5
	// b3: state=6
	// b4: state=7
	// b5: state=8
	// b6: card lock
	// when 0x40 trying to read the card
	u32 status = 1;
	if (cardInserted)
		status &= ~1;
	if (cardLocked)
		status |= 0x40;
	return status;
}

// Surprisingly recipient and sender aren't swapped in the 0xFE responses so we override RawDma for this reason
// vf4tuned and mushiking do care
u32 RFIDReaderWriterImpl::RawDma(const u32 *buffer_in, u32 buffer_in_len, u32 *buffer_out)
{
	const u32 len = BaseMIE::RawDma(buffer_in, buffer_in_len, buffer_out);
	const u8 reply = *(u8 *)buffer_out;
	if (reply == 0xfe)
		std::swap(*((u8 *)buffer_out + 1), *((u8 *)buffer_out + 2));
	return len;
}

u32 RFIDReaderWriterImpl::dma(u32 cmd)
{
	switch (cmd)
	{
	case MDC_DeviceRequest:
	case MDC_AllStatusReq:
		// custom function
		w32(0x00100000);
		// function flags
		w32(0);
		w32(0);
		w32(0);
		//1	area code
		w8(0xff);				// FF: Worldwide, 01: North America
		//1	direction
		w8(0);
		// Product name (totally made up)
		wstr("MAPLE/232C CONVERT BD", 30);

		// License (60)
		wstr("Produced By or Under License From SEGA ENTERPRISES,LTD.", 60);

		// Low-consumption standby current (2)
		w16(0x0069);	// 10.5 mA

		// Maximum current consumption (2)
		w16(0x0120);	// 28.8 mA

		return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

	case MDCF_GetCondition:
		w32(0x00100000); // custom function
		return MDRS_DataTransfer;

	// 90	get status
	//
	// read test:
	// d0	?
	// 91	get last cmd status?
	// a0	?
	// 91
	// a1	read md5 in data
	//			or data itself if after D4 xx xx xx xx
	// d4	in=d2 03 aa db
	// 91
	//
	// d9	lock
	// da	unlock
	//
	// write test:
	// D0
	// 91
	// B1 05 06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (28 bytes)
	// 91
	// B1 0b 06 00 00 00 00 c6 41 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (28 bytes)
	// 91
	// B1 11 06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (28 bytes)
	// 91
	// B1 17 06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (28 bytes)
	// 91
	// B1 1d 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (16 bytes)
	// 91
	// C1: 00 00 00 00
	// 91

	case 0xD0:
		d4Seen = false;
		[[fallthrough]];
	case 0x90:
	case 0x91:
	case 0xA0:
	case 0xD4:
	case 0xC1:
		w32(getStatus());
		if (cmd == 0xd4)
			d4Seen = true;
		return (MapleDeviceRV)0xfe;

	case 0xA1:	// read card data
		DEBUG_LOG(JVS, "RFID card read (data? %d)", d4Seen);
		w32(getStatus());
		if (!d4Seen)
			// serial0 and serial1 only
			wptr(cardData, 8);
		else
			wptr(cardData, sizeof(cardData));
		return (MapleDeviceRV)0xfe;

	case 0xD9:	// lock card
		cardLocked = true;
		w32(getStatus());
		INFO_LOG(JVS, "RFID card %d locked", player_num);
		return (MapleDeviceRV)0xfe;

	case 0xDA:	// unlock card
		cardLocked = false;
		cardInserted = false;
		w32(getStatus());
		NOTICE_LOG(JVS, "RFID card %d unlocked", player_num);
		os_notify(i18n::T("Card ejected"), 2000);
		return (MapleDeviceRV)0xfe;

	case 0xB1:	// write to card
		{
			w32(getStatus());
			u32 offset = r8() * 4;
			size_t size = r8() * 4;
			skip(2);
			DEBUG_LOG(JVS, "RFID card write: offset 0x%x len %d", offset, (int)size);
			rptr(cardData + offset, std::min(size, sizeof(cardData) - offset));
			saveCard();
			return (MapleDeviceRV)0xfe;
		}

	case 0xD1:	// decrement counter
		{
			int counter = r8();
			switch (counter) {
			case 0x03:
				counter = 0;
				break;
			case 0x0c:
				counter = 1;
				break;
			case 0x30:
				counter = 2;
				break;
			case 0xc0:
				counter = 3;
				break;
			default:
				WARN_LOG(JVS, "Unknown counter selector %x", counter);
				counter = 0;
				break;
			}
			DEBUG_LOG(JVS, "RFID decrement %d", counter);
			cardData[19 - counter]--;
			saveCard();
			w32(getStatus());
			return (MapleDeviceRV)0xfe;
		}

	default:
		return BaseMIE::dma(cmd);
	}
}

void RFIDReaderWriterImpl::OnSetup()
{
	memset(cardData, 0, sizeof(cardData));
	transientData = false;
}

std::string RFIDReaderWriterImpl::getCardPath() const
{
	int playerNum;
	if (config::GGPOEnable && !config::ActAsServer)
		// Always load P1 card with GGPO to be consistent with P1 inputs being used
		playerNum = 1;
	else
		playerNum = player_num + 1;
	return hostfs::getArcadeFlashPath() + "-p" + std::to_string(playerNum) + ".card";
}

void RFIDReaderWriterImpl::loadCard()
{
	if (transientData)
		return;
	std::string path = getCardPath();
	FILE *fp = nowide::fopen(path.c_str(), "rb");
	if (fp == nullptr)
	{
		if (settings.content.gameId.substr(0, 8) == "MKG TKOB")
		{
			constexpr u8 MUSHIKING_CHIP_DATA[128] = {
				0x12, 0x34, 0x56, 0x78, // Serial No.0
				0x31, 0x00, 0x86, 0x07, // Serial No.1
				0x00, 0x00, 0x00, 0x00, // Key
				0x04, 0xf6, 0x00, 0xAA, // Extend  Extend  Access  Mode
				0x23, 0xFF, 0xFF, 0xFF, // Counter4  Counter3  Counter2  Counter1
				0x00, 0x00, 0x00, 0x00, // User Data (first set date: day bits 0-4, month bits 5-8, year bits 9-... + 2000)
				0x00, 0x00, 0x00, 0x00, // User Data
				0x00, 0x00, 0x00, 0x00, // User Data
				0x00, 0x00, 0x00, 0x00, // User Data
				0x00, 0x00, 0x00, 0x00, // User Data
				0x00, 0x00, 0x00, 0x00, // User Data
				0x23, 0xFF, 0xFF, 0xFF, // User Data (max counters)
			};
			memcpy(cardData, MUSHIKING_CHIP_DATA, sizeof(MUSHIKING_CHIP_DATA));
			for (int i = 0; i < 8; i++)
				cardData[i] = rand() & 0xff;
			u32 mask = 0;
			if (settings.content.gameId == "MKG TKOB 2 JPN VER2.001-"			// mushik2e
					|| settings.content.gameId == "MKG TKOB 4 JPN VER2.000-")	// mushik4e
				mask = 0x40;
			cardData[4] &= ~0xc0;
			cardData[4] |= mask;

			u32 serial1 = (cardData[4] << 24) | (cardData[5] << 16) | (cardData[6] << 8) | cardData[7];
			u32 key = ~serial1;
			key = ((key >> 4) & 0x0f0f0f0f)
				  | ((key << 4) & 0xf0f0f0f0);
			cardData[8] = key >> 24;
			cardData[9] = key >> 16;
			cardData[10] = key >> 8;
			cardData[11] = key;
		}
		else
		{
			constexpr u8 VF4_CARD_DATA[128] = {
					0x10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,0x6c,   0,   0,
					   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
					   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
					   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
					   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
					   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
					   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
					   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,0xff
			};
			memcpy(cardData, VF4_CARD_DATA, sizeof(VF4_CARD_DATA));
			// Generate random bytes used by vf4 vanilla to make the card id
			srand(time(0));
			cardData[2] = rand() & 0xff;
			cardData[4] = rand() & 0xff;
			cardData[5] = rand() & 0xff;
			cardData[6] = rand() & 0xff;
			cardData[7] = rand() & 0xff;
		}
		INFO_LOG(NAOMI, "Card P%d initialized", player_num + 1);
	}
	else
	{
		INFO_LOG(NAOMI, "Loading card file from %s", path.c_str());
		if (fread(cardData, 1, sizeof(cardData), fp) != sizeof(cardData))
			WARN_LOG(NAOMI, "Truncated or empty card file: %s" ,path.c_str());
		fclose(fp);
	}
}

void RFIDReaderWriterImpl::saveCard() const
{
	if (transientData)
		return;
	std::string path = getCardPath();
	FILE *fp = nowide::fopen(path.c_str(), "wb");
	if (fp == nullptr)
	{
		WARN_LOG(NAOMI, "Can't create card file %s: errno %d", path.c_str(), errno);
		return;
	}
	INFO_LOG(NAOMI, "Saving card file to %s", path.c_str());
	if (fwrite(cardData, 1, sizeof(cardData), fp) != sizeof(cardData))
		WARN_LOG(NAOMI, "Truncated write to file: %s", path.c_str());
	fclose(fp);
}

void RFIDReaderWriterImpl::serialize(Serializer& ser) const
{
	BaseMIE::serialize(ser);
	ser << cardData;
	ser << d4Seen;
	ser << cardInserted;
	ser << cardLocked;
}
void RFIDReaderWriterImpl::deserialize(Deserializer& deser)
{
	BaseMIE::deserialize(deser);
	deser >> cardData;
	deser >> d4Seen;
	deser >> cardInserted;
	deser >> cardLocked;
}

void RFIDReaderWriterImpl::insertCard()
{
	if (!cardInserted) {
		cardInserted = true;
		loadCard();
	}
	else if (!cardLocked)
	{
		cardInserted = false;
		if (!transientData)
			memset(cardData, 0, sizeof(cardData));
	}
}

const u8 *RFIDReaderWriterImpl::getCardData() {
	loadCard();
	return cardData;
}

void RFIDReaderWriterImpl::setCardData(u8 *data) {
	memcpy(cardData, data, sizeof(cardData));
	transientData = true;
}

void insertRfidCard(int playerNum)
{
	std::shared_ptr<maple_device> mapleDev = MapleDevices[1 + playerNum][5];
	if (mapleDev != nullptr && mapleDev->get_device_type() == MDT_RFIDReaderWriter)
		std::static_pointer_cast<RFIDReaderWriterImpl>(mapleDev)->insertCard();
}

void setRfidCardData(int playerNum, u8 *data)
{
	std::shared_ptr<maple_device> mapleDev = MapleDevices[1 + playerNum][5];
	if (mapleDev != nullptr && mapleDev->get_device_type() == MDT_RFIDReaderWriter)
		std::static_pointer_cast<RFIDReaderWriterImpl>(mapleDev)->setCardData(data);
}

const u8 *getRfidCardData(int playerNum)
{
	std::shared_ptr<maple_device> mapleDev = MapleDevices[1 + playerNum][5];
	if (mapleDev != nullptr && mapleDev->get_device_type() == MDT_RFIDReaderWriter)
		return std::static_pointer_cast<RFIDReaderWriterImpl>(mapleDev)->getCardData();
	else
		return nullptr;
}
