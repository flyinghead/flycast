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
		inline u64 get_hash() const
		{
			return (static_cast<u64>(type) << 32) | code;
		}

		//! @return true iff this InputType is less than rhs
		inline bool operator<(const InputDef& rhs) const
		{
			return (get_hash() < rhs.get_hash());
		}

		//! @return true iff this InputType is equivalent to rhs
		inline bool operator==(const InputDef& rhs) const
		{
			return (get_hash() == rhs.get_hash());
		}

		//! @return true iff this InputType is not equivalent to rhs
		inline bool operator!=(const InputDef& rhs) const
		{
			return (get_hash() != rhs.get_hash());
		}

		//! @return true iff this InputType is a button
		inline bool is_button() const
		{
			return (type == InputType::BUTTON);
		}

		//! @return true iff this InputType is an axis
		inline bool is_axis() const
		{
			return (type == InputType::AXIS_POS || type == InputType::AXIS_NEG);
		}

		//! @return true iff this input definition is valid
		inline bool is_valid() const
		{
			return (code != INVALID_CODE && (is_button() || is_axis()));
		}

		//! @return true iff this InputType is an axis, rhs is an axis, and axis codes are equivalent
		inline bool axis_equivalent(const InputDef& rhs)
		{
			return (code == rhs.code && is_axis() && rhs.is_axis());
		}

		//! @return the string equivalent to this InputDef
		inline std::string to_str() const
		{
			std::string result = std::to_string(code);
			if (type == InputType::AXIS_POS)
			{
				result += "+";
			}
			else if (type == InputType::AXIS_NEG)
			{
				result += "-";
			}
			return result;
		}

		//! Converts a string to an InputDef
		static inline InputDef from_str(const std::string& str)
		{
			InputDef inputDef;
			if (!str.empty())
			{
				std::string tmp = str;
				if (tmp.back() == '+')
				{
					inputDef.type = InputType::AXIS_POS;
					tmp.erase(tmp.size() - 1);
				}
				else if (tmp.back() == '-')
				{
					inputDef.type = InputType::AXIS_NEG;
					tmp.erase(tmp.size() - 1);
				}
				else
				{
					inputDef.type = InputType::BUTTON;
				}

				try
				{
					inputDef.code = std::stoul(tmp);
				}
				catch (const std::exception&)
				{
					inputDef.code = INVALID_CODE;
				}
			}
			return inputDef;
		}
	};

	//! A group of inputs used to link to multiple keys to a function.
	//! This class acts like a set with insertion order maintained.
	class InputSet : public std::list<InputDef>
	{
	public:
		InputSet() : std::list<InputDef>() {}

		explicit InputSet(const allocator_type& a) : std::list<InputDef>(a) {}

		explicit InputSet(std::initializer_list<value_type> l) : std::list<InputDef>(l) {}

		//! Insert new element to the back, ensuring uniqueness
		//! @param[in] val The value to insert at the back
		//! @return true iff new item inserted
		inline bool insert_back(const InputMapping::InputDef& val)
		{
			remove_inverse_axis(val);
			const_iterator iter = std::find(cbegin(), cend(), val);
			bool removed = (iter != cend());
            if (removed)
            {
                erase(iter);
            }
			push_back(val);
            return !removed;
		}

		//! Insert new element to the back, ensuring uniqueness
		//! @param[in,out] val The value to insert at the back (move operation)
		inline bool insert_back(InputMapping::InputDef&& val)
		{
			remove_inverse_axis(val);
			const_iterator iter = std::find(cbegin(), cend(), val);
			bool removed = (iter != cend());
            if (removed)
            {
                erase(iter);
            }
			push_back(std::move(val));
            return !removed;
		}

		//! @return true iff this set contains the given val
		inline bool contains(const InputMapping::InputDef& val) const
		{
			return (std::find(cbegin(), cend(), val) != cend());
		}

		//! @return true if this InputSet ends with the given rhs
		inline bool ends_with(const InputSet& rhs) const
		{
			InputSet::const_reverse_iterator iter = crbegin();
			InputSet::const_reverse_iterator riter = rhs.crbegin();

			while (iter != crend() && riter != rhs.crend())
			{
				if (*iter != *riter)
				{
					break;
				}
				++iter;
				++riter;
			}

			return (riter == rhs.crend());
		}

	private:
		// Make push functionality private
		using std::list<InputDef>::push_back;
		using std::list<InputDef>::push_front;

		//! Removes inverse axis value if val is an axis
		void remove_inverse_axis(const InputMapping::InputDef& val)
		{
			if (val.is_axis())
			{
				InputMapping::InputDef::InputType inversetype(
					val.type == InputMapping::InputDef::InputType::AXIS_POS
					? InputMapping::InputDef::InputType::AXIS_NEG
					: InputMapping::InputDef::InputType::AXIS_POS);
				remove(InputMapping::InputDef{val.code, inversetype});
			}
		}
	};

	InputMapping() = default;
	InputMapping(const InputMapping& other) {
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

	DreamcastKey get_button_id(u32 port, u32 code)
	{
		std::list<DreamcastKey> ids = get_combo_ids(port, InputSet{InputDef{code, InputDef::InputType::BUTTON}});
		if (!ids.empty())
			return *(ids.begin());
		else
			return EMU_BTN_NONE;
	}
	void clear_button(u32 port, DreamcastKey id);
	void set_button(u32 port, DreamcastKey id, u32 code);
	void set_button(DreamcastKey id, u32 code) { set_button(0, id, code); }
	u32 get_button_code(u32 port, DreamcastKey key);

	DreamcastKey get_axis_id(u32 port, u32 code, bool pos)
	{
		InputDef::InputType type = (pos ? InputDef::InputType::AXIS_POS : InputDef::InputType::AXIS_NEG);
		std::list<DreamcastKey> ids = get_combo_ids(port, InputSet{InputDef{code, type}});
		if (!ids.empty())
			return *(ids.begin());
		else
			return EMU_BTN_NONE;
	}
	std::pair<u32, bool> get_axis_code(u32 port, DreamcastKey key);

	//! Resolves existing inputs plus a new input into a key from multi-input lookup
	//! @param[in] port The port that the given inputs belong to [0,NUM_PORTS)
	//! @param[in] inputSet The input set to look for
	//! @return all keys that should be activated due to the new input (all combos this set ends with)
	std::list<DreamcastKey> get_combo_ids(u32 port, const InputSet& inputSet) const;

	//! Clears combo setting for a given key
	//! @param[in] port The port [0,NUM_PORTS)
	//! @param[in] id The key
	void clear_combo(u32 port, DreamcastKey id);

	//! Sets DreamcastKey to a set of inputs
	//! @param[in] port The port that the id should be mapped [0,NUM_PORTS)
	//! @param[in] id The key to map
	//! @param[in] combo Combination of inputs that should activate the key
	//! @return true iff the combo has been set
	bool set_combo(u32 port, DreamcastKey id, const InputSet& combo);
	inline bool set_combo(DreamcastKey id, const InputSet& combo) { return set_combo(0, id, combo); }

	//! @param[in] port The port [0,NUM_PORTS)
	//! @param[in] key The key
	//! @return the InputSet associated with the given key at the given port
	InputSet get_combo_codes(u32 port, DreamcastKey key);

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

	//! Maps an DreamcastKey to one or more inputs that need to be activated
	std::map<DreamcastKey, InputSet> multiEmuButtonMap[NUM_PORTS];
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
