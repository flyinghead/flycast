/*
    Copyright 2021 flyinghead

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

class Settings {
public:
	void reset() {}

	void load(bool gameSpecific)
	{
		if (gameSpecific)
			return;
		for (const auto& o : options)
			o->load();
	}

	void save() {}

	void setGameId(const std::string& gameId) {
		this->gameId = gameId;
	}

	bool hasPerGameConfig() const {
		return false;
	}
	void setPerGameConfig(bool perGameConfig) {}

	void setRetroEnvironment(retro_environment_t retroEnv) {
		this->retroEnv = retroEnv;
	}
	void setOptionDefinitions(const retro_core_option_definition *optionDefs) {
		this->optionDefs = optionDefs;
	}

	static Settings& instance() {
		static Settings *_instance = new Settings();
		return *_instance;
	}

private:
	std::vector<BaseOption *> options;
	std::string gameId;
	retro_environment_t retroEnv = nullptr;
	const retro_core_option_definition *optionDefs;

	template<typename T, bool>
	friend class Option;
};

// Missing in C++11
template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

template<typename T, bool PerGameOption = true>
class Option : public BaseOption {
public:
	Option(const std::string& name, T defaultValue = T())
		: name(name), value(defaultValue), defaultValue(defaultValue),
		  settings(Settings::instance())
	{
		settings.options.push_back(this);
	}

	void reset() override {
		set(defaultValue);
		overridden = false;
	}

	void load() override {
		if (!name.empty())
			set(doLoad(name));
	}

	void save() const override {}

	T& get() { return value; }
	void set(T v) { value = v; }

	void override(T v) {
		overriddenDefault = v;
		overridden = true;
		value = v;
	}
	bool isReadOnly() const {
		return overridden;
	}

	explicit operator T() const { return value; }
	operator T&() { return value; }
	T& operator=(const T& v) { set(v); return value; }

protected:
	const retro_core_option_definition *findDefinition(const std::string& name) const {
		for (const retro_core_option_definition *pDef = settings.optionDefs; pDef->key != nullptr; pDef++)
			if (name == pDef->key)
				return pDef;
		return nullptr;
	}

	template <typename U = T>
	enable_if_t<std::is_same<U, bool>::value, T>
	doLoad(const std::string& name) const
	{
		retro_variable var { name.c_str() };
		if (settings.retroEnv(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != nullptr)
		{
			const retro_core_option_definition *def = findDefinition(name);
			verify(def != nullptr);
			if (!strcmp(var.value, def->values[1].value)) // TODO change defs so that choice 1 is true (and choice 0 is false)
				return true;
			else
				return false;
		}
		return value;
	}

	template <typename U = T>
	enable_if_t<(std::is_integral<U>::value || std::is_enum<U>::value)
			&& !std::is_same<U, bool>::value, T>
	doLoad(const std::string& name) const
	{
		retro_variable var { name.c_str() };
		if (settings.retroEnv(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != nullptr)
		{
			const retro_core_option_definition *def = findDefinition(name);
			verify(def != nullptr);
			for (int i = 0; def->values[i].value != nullptr; i++)
				if (!strcmp(var.value, def->values[i].value))
					return (T)i;
		}
		return value;
	}

	template <typename U = T>
	enable_if_t<std::is_same<U, std::string>::value, T>
	doLoad(const std::string& name) const
	{
		retro_variable var { name.c_str() };
		if (settings.retroEnv(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != nullptr)
			return var.value;
		return value;
	}

	template <typename U = T>
	enable_if_t<std::is_same<float, U>::value, T>
	doLoad(const std::string& name) const
	{
		retro_variable var { name.c_str() };
		if (settings.retroEnv(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value != nullptr)
			return atof(var.value);
		return value;
	}

	template <typename U = T>
	enable_if_t<std::is_same<std::vector<std::string>, U>::value, T>
	doLoad(const std::string& section, const std::string& name) const
	{
		return value;
	}

	std::string name;
	T value;
	T defaultValue;
	T overriddenDefault = T();
	bool overridden = false;
	Settings& settings;
};

extern Option<bool> PowerVR2Filter;
