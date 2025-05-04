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
#include "gamepad.h"

#include <map>
#include <set>
#include <memory>
#include <list>
#include <limits>

namespace emucfg {
struct ConfigFile;
}

class InputMapping
{
public:
	//! Uniquely identifies any input, be it button or axis
	struct InputDef
	{
		//! Enumerates the type of input
		enum class InputType : u8
		{
			BUTTON = 0x00,	//!< Digital button type
			AXIS_POS,		//!< Axis, positive inclination
			AXIS_NEG		//!< Axis, negative inclination
		};

		//! Initialized and invalid value for code
		static const u32 INVALID_CODE = std::numeric_limits<u32>::max();

		//! The unique code for the set type
		u32 code = INVALID_CODE;
		//! The type of input
		InputType type = InputType::BUTTON;

		//! Creates a 1:1 hash which uniquely identifies this input
		u64 get_hash() const;

		//! @return true iff this InputType is less than rhs
		bool operator<(const InputDef& rhs) const;

		//! @return true iff this InputType is equivalent to rhs
		bool operator==(const InputDef& rhs) const;

		//! @return true iff this InputType is not equivalent to rhs
		bool operator!=(const InputDef& rhs) const;

		//! @return true iff this InputType is a button
		bool is_button() const;

		//! @return true iff this InputType is an axis
		bool is_axis() const;

		//! @return true iff this input definition is valid
		bool is_valid() const;

		//! @return the suffix for this InputDef which signifies the type
		const char* get_suffix() const;

		//! @return the string equivalent to this InputDef
		std::string to_str() const;

		//! Converts a string to an InputDef
		static InputDef from_str(const std::string& str);

		//! Converts a button code to an InputDef
		static InputDef from_button(u32 code);

		//! Converts an axis code plus direction to an InputDef
		static InputDef from_axis(u32 code, bool positive);
	};

	//! A group of inputs used to link to multiple keys to a function.
	//! This class acts like a set with insertion order maintained.
	class InputSet : public std::list<InputDef>
	{
	public:
		InputSet();

		explicit InputSet(std::initializer_list<value_type> l);

		//! Insert new element to the back only if it doesn't already exist in the set
		//! @param[in] val The value to insert at the back
		//! @return true iff new item inserted
		bool insert_back(const InputMapping::InputDef& val);

		//! Insert new element to the back only if it doesn't already exist in the set
		//! @param[in,out] val The value to insert at the back (move operation)
		bool insert_back(InputMapping::InputDef&& val);

		//! Removes all elements that match val
		//! @param[in] val The value to remove
		//! @return number of elements removed (normally 0 or 1)
		std::size_t remove(const InputMapping::InputDef& val);

		//! @return true iff this set contains the given val
		bool contains(const InputMapping::InputDef& val) const;

		//! @return true if this InputSet ends with the given rhs
		bool ends_with(const InputSet& rhs, bool sequential) const;

	private:
		// Make push functionality private
		using std::list<InputDef>::push_back;
		using std::list<InputDef>::push_front;

		//! Removes inverse axis value if val is an axis
		void remove_inverse_axis(const InputMapping::InputDef& val);
	};

	//! Contains all settings for a button combination
	struct ButtonCombo
	{
		//! The set of inputs that make up a button combination
		InputSet inputs;
		//! Set to true if the above input set must be pressed in the given sequence to activate the combo.
		//! Set to false if all of the buttons may be pressed in any sequence to activate the combo.
		bool sequential = true;
	};

	InputMapping() = default;
	inline InputMapping(const InputMapping& other) {
		name = other.name;
		dead_zone = other.dead_zone;
		saturation = other.saturation;
		rumblePower = other.rumblePower;
		for (int port = 0; port < 4; port++)
		{
			multiEmuButtonMap[port] = other.multiEmuButtonMap[port];
			reverseMultiEmuButtonMap[port] = other.reverseMultiEmuButtonMap[port];
		}
	}

	//! Number of controller ports there are
	static constexpr const u32 NUM_PORTS = 4;
	//! Version 4 adds button combos
	static constexpr const int CURRENT_FILE_VERSION = 4;

	std::string name;
	float dead_zone = 0.1f;
	float saturation = 1.0f;
	int rumblePower = 100;
	int version = CURRENT_FILE_VERSION;

	//! Resolves an ordered set of inputs into keys from multi-input lookup
	//! @param[in] port The port that the given inputs belong to [0,NUM_PORTS)
	//! @param[in] inputSet The input set to look for
	//! @return all keys that should be activated due to the given input set (all combos inputSet ends with)
	std::list<DreamcastKey> get_button_ids(u32 port, const InputSet& inputSet) const;

	//! Resolves currently active keys with a released input into keys that should be released
	//! @param[in] port The port that the given inputs belong to [0,NUM_PORTS)
	//! @param[in] activeKeys A list of active keys compiled from get_button_ids()
	//! @param[in] releasedInput The released input to check for
	//! @return all keys that should be released due to the released input
	std::list<DreamcastKey> get_button_released_ids(
		u32 port,
		const std::list<DreamcastKey>& activeKeys,
		const InputDef& releasedInput) const;

	//! Clears combo setting for a given key
	//! @param[in] port The port [0,NUM_PORTS)
	//! @param[in] id The key
	void clear_button(u32 port, DreamcastKey id);

	//! Sets DreamcastKey to a set of inputs
	//! @param[in] port The port that the id should be mapped [0,NUM_PORTS)
	//! @param[in] id The key to map
	//! @param[in] combo Combination of inputs that should activate the key
	//! @return true iff the combo has been set
	bool set_button(u32 port, DreamcastKey id, const ButtonCombo& combo);
	inline bool set_button(DreamcastKey id, const ButtonCombo& combo) { return set_button(0, id, combo); }
	inline bool set_button(u32 port, DreamcastKey id, u32 buttonCode)
	{
		return set_button(0, id, ButtonCombo{InputSet{InputDef{buttonCode, InputDef::InputType::BUTTON}}, false});
	}
	inline bool set_button(DreamcastKey id, u32 buttonCode) { return set_button(0, id, buttonCode); }

	//! @param[in] port The port [0,NUM_PORTS)
	//! @param[in] key The key
	//! @return the ButtonCombo associated with the given key at the given port
	ButtonCombo get_button_combo(u32 port, DreamcastKey key) const;

	//! Get the button code if and only if the given key is mapped to a single code
	//! @param[in] port The port [0,NUM_PORTS)
	//! @param[in] key The key
	//! @return the button code for the given key if found
	//! @return InputDef::INVALID_CODE if multiple inputs are mapped to this key or none are mapped
	u32 get_button_code(u32 port, DreamcastKey key) const;

	//! @param[in] port The port [0,NUM_PORTS)
	//! @param[in] key The key
	//! @return pointer to the ButtonCombo associated with the given key at the given port if found
	//! @return nullptr otherwise
	ButtonCombo* get_button_ptr(u32 port, DreamcastKey key);

	inline DreamcastKey get_axis_id(u32 port, u32 code, bool pos)
	{
		auto it = axes[port].find(std::make_pair(code, pos));
		if (it != axes[port].end())
			return it->second;
		else
			return EMU_AXIS_NONE;
	}
	std::pair<u32, bool> get_axis_code(u32 port, DreamcastKey key);

	void clear_axis(u32 port, DreamcastKey id);
	void set_axis(u32 port, DreamcastKey id, u32 code, bool positive);
	void set_axis(DreamcastKey id, u32 code, bool positive) { set_axis(0, id, code, positive); }

	void load(FILE* fp);
	bool save(const std::string& name);

	void set_dirty();
	bool is_dirty() const { return dirty; }

	static std::shared_ptr<InputMapping> LoadMapping(const std::string& name);
	static void SaveMapping(const std::string& name, const std::shared_ptr<InputMapping>& mapping);
	static void DeleteMapping(const std::string& name);

	void ClearMappings();

protected:
	bool dirty = false;

private:
	void loadv1(emucfg::ConfigFile& mf);

	std::map<std::pair<u32, bool>, DreamcastKey> axes[NUM_PORTS];

	//! Maps an DreamcastKey to one or more inputs that need to be activated
	std::map<DreamcastKey, ButtonCombo> multiEmuButtonMap[NUM_PORTS];
	//! Maps an input to one or more DreamcastKeys which that input is tied to
	std::multimap<InputDef, DreamcastKey> reverseMultiEmuButtonMap[NUM_PORTS];

	static std::map<std::string, std::shared_ptr<InputMapping>> loaded_mappings;
};

class IdentityInputMapping : public InputMapping
{
public:
	IdentityInputMapping() {
		name = "Default";
		dead_zone = 0.1f;

		for (int i = 0; i < 32; i++)
			set_button(0, (DreamcastKey)(1 << i), 1 << i);
		set_button(0, EMU_BTN_FFORWARD, EMU_BTN_FFORWARD);
		set_button(0, EMU_BTN_MENU, EMU_BTN_MENU);
		set_button(0, EMU_BTN_ESCAPE, EMU_BTN_ESCAPE);
		set_axis(0, DC_AXIS_LEFT, DC_AXIS_LEFT, true);
		set_axis(0, DC_AXIS_RIGHT, DC_AXIS_RIGHT, true);
		set_axis(0, DC_AXIS_UP, DC_AXIS_UP, true);
		set_axis(0, DC_AXIS_DOWN, DC_AXIS_DOWN, true);
		set_axis(0, DC_AXIS_LT, DC_AXIS_LT, true);
		set_axis(0, DC_AXIS_RT, DC_AXIS_RT, true);
		set_axis(0, DC_AXIS_LT2, DC_AXIS_LT2, true);
		set_axis(0, DC_AXIS_RT2, DC_AXIS_RT2, true);
		set_axis(0, DC_AXIS2_LEFT, DC_AXIS2_LEFT, true);
		set_axis(0, DC_AXIS2_RIGHT, DC_AXIS2_RIGHT, true);
		set_axis(0, DC_AXIS2_UP, DC_AXIS2_UP, true);
		set_axis(0, DC_AXIS2_DOWN, DC_AXIS2_DOWN, true);
		set_axis(0, DC_AXIS3_LEFT, DC_AXIS3_LEFT, true);
		set_axis(0, DC_AXIS3_RIGHT, DC_AXIS3_RIGHT, true);
		set_axis(0, DC_AXIS3_UP, DC_AXIS3_UP, true);
		set_axis(0, DC_AXIS3_DOWN, DC_AXIS3_DOWN, true);
	}
};
