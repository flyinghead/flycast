/*
	Copyright 2019 flyinghead

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
#include "types.h"
#include "mapping.h"
#include "stdclass.h"

#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <set>

class GamepadDevice
{
public:
	typedef void (*input_detected_cb)(u32 code, bool analog, bool positive);

	const std::string& api_name() { return _api_name; }
	const std::string& name() { return _name; }
	int maple_port() const { return _maple_port; }
	virtual void set_maple_port(int port) { _maple_port = port; }
	const std::string& unique_id() { return _unique_id; }
	virtual bool gamepad_btn_input(u32 code, bool pressed);
	virtual bool gamepad_axis_input(u32 code, int value);
	virtual ~GamepadDevice() = default;

	void detect_btn_input(input_detected_cb button_pressed);
	void detect_axis_input(input_detected_cb axis_moved);
	void detectButtonOrAxisInput(input_detected_cb input_changed);
	void cancel_detect_input() {
		_input_detected = nullptr;
		_detecting_button = false;
		_detecting_axis = false;
		_detecting_combo = false;
		detectionInputs.clear();
	}
	bool is_input_detecting() const {
		return _input_detected != nullptr;
	}
	std::shared_ptr<InputMapping> get_input_mapping() { return input_mapper; }
	void save_mapping(int system = settings.platform.system);

	virtual const char *get_button_name(u32 code) { return nullptr; }
	virtual const char *get_axis_name(u32 code) { return nullptr; }
	bool remappable() { return _remappable && input_mapper; }
	virtual bool is_virtual_gamepad() { return false; }

	virtual void rumble(float power, float inclination, u32 duration_ms) {}
	virtual void update_rumble() {}
	bool is_rumble_enabled() const { return rumbleEnabled; }
	int get_rumble_power() const { return rumblePower; }
	void set_rumble_power(int power) {
		if (power != this->rumblePower)
		{
			this->rumblePower = power;
			if (input_mapper != nullptr)
			{
				input_mapper->rumblePower = power;
				input_mapper->set_dirty();
				save_mapping();
			}
		}
	}
	bool has_analog_stick() const { return hasAnalogStick; }
	float get_dead_zone() const { return input_mapper->dead_zone; }
	void set_dead_zone(float deadzone) {
		if (deadzone != input_mapper->dead_zone)
		{
			input_mapper->dead_zone = deadzone;
			input_mapper->set_dirty();
			save_mapping();
		}
	}
	float get_saturation() const { return input_mapper->saturation; }
	void set_saturation(float saturation)
	{
		if (saturation != input_mapper->saturation)
		{
			input_mapper->saturation = saturation;
			input_mapper->set_dirty();
			save_mapping();
		}
	}
	bool is_registered() const { return _is_registered; }

	static void Register(const std::shared_ptr<GamepadDevice>& gamepad);
	static void Unregister(const std::shared_ptr<GamepadDevice>& gamepad);
	static int GetGamepadCount();
	static std::shared_ptr<GamepadDevice> GetGamepad(int index);
	static void SaveMaplePorts();
	static void RampAnalog();

	template<typename T>
	static std::shared_ptr<T> GetGamepad()
	{
		Lock _(_gamepads_mutex);
		for (const auto& gamepad : _gamepads)
			if (dynamic_cast<T*>(gamepad.get()) != nullptr)
				return std::dynamic_pointer_cast<T>(gamepad);
		return {};
	}

	static void load_system_mappings();
	bool find_mapping(int system = settings.platform.system);
	virtual void resetMappingToDefault(bool arcade, bool gamepad) {
		input_mapper = getDefaultMapping();
	}

	void setPerGameMapping(bool enabled);
	bool isPerGameMapping() const { return perGameMapping; }

protected:
	GamepadDevice(int maple_port, const char *api_name, bool remappable = true)
		: _api_name(api_name), _maple_port(maple_port), _input_detected(nullptr), _remappable(remappable),
		  digitalToAnalogState{}
	{
		// Initialize pressedButtons sets
		for (int i = 0; i < 4; i++)
			currentInputs[i].clear();
	}

	void loadMapping() {
		if (!find_mapping())
			input_mapper = getDefaultMapping();
		else
			INFO_LOG(INPUT, "using custom mapping '%s'", input_mapper->name.c_str());
	}
	virtual std::shared_ptr<InputMapping> getDefaultMapping() {
		return std::make_shared<IdentityInputMapping>();
	}

	bool is_detecting_input() { return _input_detected != nullptr; }

	std::string _name;
	std::string _unique_id;
	std::shared_ptr<InputMapping> input_mapper;
	bool rumbleEnabled = false;
	bool hasAnalogStick = false;
	int rumblePower = 100;
	u32 leftTrigger = ~0;
	u32 rightTrigger = ~0;

private:
	virtual void registered() {}
	bool handleButtonInput(int port, DreamcastKey key, bool pressed);
	std::string make_mapping_filename(bool instance, int system, bool perGame = false);

	// Track which inputs are currently activated (for button combos)
	InputMapping::InputSet currentInputs[4];
	// Track inputs during the current mapping session
	InputMapping::InputSet detectionInputs;

	enum DigAnalog {
		DIGANA_LEFT   = 1 << 0,
		DIGANA_RIGHT  = 1 << 1,
		DIGANA_UP     = 1 << 2,
		DIGANA_DOWN   = 1 << 3,
		DIGANA2_LEFT  = 1 << 4,
		DIGANA2_RIGHT = 1 << 5,
		DIGANA2_UP    = 1 << 6,
		DIGANA2_DOWN  = 1 << 7,
		DIGANA3_LEFT  = 1 << 8,
		DIGANA3_RIGHT = 1 << 9,
		DIGANA3_UP    = 1 << 10,
		DIGANA3_DOWN  = 1 << 11,
	};

	template<DreamcastKey DcNegDir, DigAnalog NegDir, DigAnalog PosDir>
	void buttonToAnalogInput(int port, DreamcastKey key, bool pressed, s16& joystick)
	{
		if (port < 0)
			return;
		Lock _(rampMutex);
		DigAnalog axis = key == DcNegDir ? NegDir : PosDir;
		if (pressed)
			digitalToAnalogState[port] |= axis;
		else
			digitalToAnalogState[port] &= ~axis;
		rampAnalogState[port] |= NegDir;
		if (lastAnalogUpdate == 0)
			lastAnalogUpdate = getTimeMs();
	}

	s16 (&getTargetArray(DigAnalog axis))[4];
	void rampAnalog();

	std::string _api_name;
	int _maple_port;
	bool _detecting_button = false;
	bool _detecting_axis = false;
	bool _detecting_combo = false;  // For button combination detection
	u64 _detection_start_time = 0;
	input_detected_cb _input_detected;
	bool _remappable;
	bool _is_registered = false;
	u32 digitalToAnalogState[4];
	std::map<DreamcastKey, int> lastAxisValue[4];
	bool perGameMapping = false;
	bool instanceMapping = false;

	u64 lastAnalogUpdate = 0;
	u32 rampAnalogState[4] {};
	static constexpr float AnalogRamp = 32767.f / 100.f;		// 100 ms ramp time
	std::mutex rampMutex;

	static std::vector<std::shared_ptr<GamepadDevice>> _gamepads;
	static std::mutex _gamepads_mutex;

	using Lock = std::lock_guard<std::mutex>;
};

#ifdef TEST_AUTOMATION
void replay_input();
#endif

extern u32 kcode[4];
extern u16 rt[4], lt[4], rt2[4], lt2[4];
extern s16 joyx[4], joyy[4];
extern s16 joyrx[4], joyry[4];
extern s16 joy3x[4], joy3y[4];
