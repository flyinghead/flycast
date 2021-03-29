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
#include <string>
#include <vector>
#include <array>
#include "cfg.h"
#include "hw/maple/maple_cfg.h"

namespace config {

class BaseOption {
public:
	virtual ~BaseOption() = default;
	virtual void save() const = 0;
	virtual void load() = 0;
	virtual void reset() = 0;
};

class Settings {
public:
	void reset() {
		for (const auto& o : options)
			o->reset();
		gameId.clear();
		perGameConfig = false;
	}

	void load(bool gameSpecific) {
		if (gameSpecific)
		{
			if (gameId.empty())
				return;
			if (!cfgHasSection(gameId))
				return;
			perGameConfig = true;
		}
		for (const auto& o : options)
			o->load();
	}

	void save() {
		cfgSetAutoSave(false);
		for (const auto& o : options)
			o->save();
		cfgSetAutoSave(true);
	}

	const std::string& getGameId() const {
		return gameId;
	}

	void setGameId(const std::string& gameId) {
		this->gameId = gameId;
	}

	bool hasPerGameConfig() const {
		return perGameConfig;
	}
	void setPerGameConfig(bool perGameConfig) {
		this->perGameConfig = perGameConfig;
		if (!perGameConfig) {
			if (!gameId.empty())
				cfgDeleteSection(gameId);
			reset();
		}
	}

	static Settings& instance() {
		static Settings *_instance = new Settings();
		return *_instance;
	}

private:
	std::vector<BaseOption *> options;
	std::string gameId;
	bool perGameConfig = false;

	template<typename T, bool PerGameOption>
	friend class Option;
};

// Missing in C++11
template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

template<typename T, bool PerGameOption = true>
class Option : public BaseOption {
public:
	Option(const std::string& name, T defaultValue = T(), const std::string& section = "config")
		: section(section), name(name), value(defaultValue), defaultValue(defaultValue),
		  settings(Settings::instance())
	{
		settings.options.push_back(this);
	}

	void reset() override {
		set(defaultValue);
		overridden = false;
	}

	void load() override {
		if (PerGameOption && settings.hasPerGameConfig())
			set(doLoad(settings.getGameId(), section + "." + name));
		else
		{
			set(doLoad(section, name));
			if (cfgIsVirtual(section, name))
				override(value);
		}
	}

	void save() const override
	{
		if (overridden) {
			if (value == overriddenDefault)
				return;
			if (!settings.hasPerGameConfig())
				// overridden options can only be saved in per-game settings
				return;
		}
		else if (PerGameOption && settings.hasPerGameConfig())
		{
			if (value == doLoad(section, name))
				return;
		}
		if (PerGameOption && settings.hasPerGameConfig())
			doSave(settings.getGameId(), section + "." + name);
		else
			doSave(section, name);
	}

	T& get() { return value; }
	void set(T v) { value = v; }

	void override(T v) {
		verify(PerGameOption);
		overriddenDefault = v;
		overridden = true;
		value = v;
	}
	bool isReadOnly() const {
		return overridden && !settings.hasPerGameConfig();
	}

	explicit operator T() const { return value; }
	operator T&() { return value; }
	T& operator=(const T& v) { set(v); return value; }

protected:
	template <typename U = T>
	enable_if_t<std::is_same<U, bool>::value, T>
	doLoad(const std::string& section, const std::string& name) const
	{
		return cfgLoadBool(section, name, value);
	}

	template <typename U = T>
	enable_if_t<(std::is_integral<U>::value || std::is_enum<U>::value)
			&& !std::is_same<U, bool>::value, T>
	doLoad(const std::string& section, const std::string& name) const
	{
		return (T)cfgLoadInt(section, name, (int)value);
	}

	template <typename U = T>
	enable_if_t<std::is_same<U, std::string>::value, T>
	doLoad(const std::string& section, const std::string& name) const
	{
		return cfgLoadStr(section, name, value);
	}

	template <typename U = T>
	enable_if_t<std::is_same<float, U>::value, T>
	doLoad(const std::string& section, const std::string& name) const
	{
		std::string strValue = cfgLoadStr(section, name, "");
		if (strValue.empty())
			return value;
		else
			return atof(strValue.c_str());
	}

	template <typename U = T>
	enable_if_t<std::is_same<std::vector<std::string>, U>::value, T>
	doLoad(const std::string& section, const std::string& name) const
	{
		std::string paths = cfgLoadStr(section, name, "");
		if (paths.empty())
			return value;
		std::string::size_type start = 0;
		std::vector<std::string> newValue;
		while (true)
		{
			std::string::size_type end = paths.find(';', start);
			if (end == std::string::npos)
				end = paths.size();
			if (start != end)
				newValue.push_back(paths.substr(start, end - start));
			if (end == paths.size())
				break;
			start = end + 1;
		}
		return newValue;
	}

	template <typename U = T>
	enable_if_t<std::is_same<U, bool>::value>
	doSave(const std::string& section, const std::string& name) const
	{
		cfgSaveBool(section, name, value);
	}

	template <typename U = T>
	enable_if_t<(std::is_integral<U>::value || std::is_enum<U>::value)
		&& !std::is_same<U, bool>::value>
	doSave(const std::string& section, const std::string& name) const
	{
		cfgSaveInt(section, name, (int)value);
	}

	template <typename U = T>
	enable_if_t<std::is_same<U, std::string>::value>
	doSave(const std::string& section, const std::string& name) const
	{
		cfgSaveStr(section, name, value);
	}

	template <typename U = T>
	enable_if_t<std::is_same<float, U>::value>
	doSave(const std::string& section, const std::string& name) const
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%f", value);
		cfgSaveStr(section, name, buf);
	}

	template <typename U = T>
	enable_if_t<std::is_same<std::vector<std::string>, U>::value>
	doSave(const std::string& section, const std::string& name) const
	{
		std::string s;
		for (auto& v : value)
		{
			if (s.empty())
				s = v;
			else
				s += ";" + v;
		}
		cfgSaveStr(section, name, s);
	}

	std::string section;
	std::string name;
	T value;
	T defaultValue;
	T overriddenDefault = T();
	bool overridden = false;
	Settings& settings;
};

using OptionString = Option<std::string>;

template<typename T, T value = T()>
class ConstOption {
public:
	operator T() const { return value; }
};

// Dynarec

extern Option<bool> DynarecEnabled;
extern Option<bool> DynarecIdleSkip;
extern Option<bool> DynarecSafeMode;
extern Option<bool> DisableVmem32;

// General

extern Option<int> Cable;		// 0 -> VGA, 1 -> VGA, 2 -> RGB, 3 -> TV Composite
extern Option<int> Region;		// 0 -> JP, 1 -> USA, 2 -> EU, 3 -> default
extern Option<int> Broadcast;	// 0 -> NTSC, 1 -> PAL, 2 -> PAL/M, 3 -> PAL/N, 4 -> default
extern Option<int> Language;	// 0 -> JP, 1 -> EN, 2 -> DE, 3 -> FR, 4 -> SP, 5 -> IT, 6 -> default
extern Option<bool> FullMMU;
extern Option<bool> ForceWindowsCE;
extern Option<bool> AutoSavestate;

// Sound

constexpr ConstOption<bool, true> LimitFPS;
extern Option<bool> DSPEnabled;
extern Option<bool> DisableSound;
extern Option<int> AudioBufferSize;	//In samples ,*4 for bytes
extern Option<bool> AutoLatency;

extern OptionString AudioBackend;

// Rendering

class RendererOption : public Option<RenderType> {
public:
	RendererOption()
		: Option<RenderType>("pvr.rend", RenderType::OpenGL) {}

	bool isOpenGL() const {
		return value == RenderType::OpenGL || value == RenderType::OpenGL_OIT;
	}
	void set(RenderType v)
	{
		newValue = v;
	}
	RenderType& operator=(const RenderType& v) { set(v); return value; }

	void load() override {
		RenderType current = value;
		Option<RenderType>::load();
		newValue = value;
		value = current;
	}

	void reset() override {
		// don't reset the value to avoid vk -> gl -> vk quick switching
		overridden = false;
	}

	bool pendingChange() {
		return newValue != value;
	}
	void commit() {
		value = newValue;
	}

private:
	RenderType newValue = RenderType();
};
extern RendererOption RendererType;
extern Option<bool> UseMipmaps;
extern Option<bool> Widescreen;
extern Option<bool> ShowFPS;
extern Option<bool> RenderToTextureBuffer;
extern Option<int> RenderToTextureUpscale;
extern Option<bool> TranslucentPolygonDepthMask;
extern Option<bool> ModifierVolumes;
constexpr ConstOption<bool, true> Clipping;
extern Option<int> TextureUpscale;
extern Option<int> MaxFilteredTextureSize;
extern Option<float> ExtraDepthScale;
extern Option<bool> CustomTextures;
extern Option<bool> DumpTextures;
extern Option<int> ScreenStretching;	// in percent. 150 means stretch from 4/3 to 6/3
extern Option<bool> Fog;
extern Option<bool> FloatVMUs;
extern Option<bool> Rotate90;
extern Option<bool> PerStripSorting;
extern Option<bool> DelayFrameSwapping;	// Delay swapping frame until FB_R_SOF matches FB_W_SOF
extern Option<bool> WidescreenGameHacks;
extern std::array<Option<int>, 4> CrosshairColor;
extern Option<int> SkipFrame;
extern Option<int> MaxThreads;
extern Option<int> AutoSkipFrame;		// 0: none, 1: some, 2: more
extern Option<int> RenderResolution;

// Misc

extern Option<bool> SerialConsole;
extern Option<bool> SerialPTY;
extern Option<bool> UseReios;

extern Option<bool> OpenGlChecks;

extern Option<std::vector<std::string>, false> ContentPath;
extern Option<bool, false> HideLegacyNaomiRoms;

// Network

extern Option<bool> NetworkEnable;
extern Option<bool> ActAsServer;
extern OptionString DNS;
extern OptionString NetworkServer;
extern Option<bool> EmulateBBA;

#ifdef SUPPORT_DISPMANX
extern Option<bool> DispmanxMaintainAspect;
#endif

#ifdef USE_OMX
extern Option<int> OmxAudioLatency;
extern Option<bool> OmxAudioHdmi;
#endif

// Maple

extern Option<int> MouseSensitivity;
extern Option<int> VirtualGamepadVibration;
extern std::array<Option<MapleDeviceType>, 4> MapleMainDevices;
extern std::array<std::array<Option<MapleDeviceType>, 2>, 4> MapleExpansionDevices;

} // namespace config

