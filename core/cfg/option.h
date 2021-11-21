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
#include <cmath>
#include "cfg.h"
#include "hw/maple/maple_cfg.h"
#ifdef LIBRETRO
#include <libretro.h>
#endif

namespace config {

class BaseOption {
public:
	virtual ~BaseOption() = default;
	virtual void save() const = 0;
	virtual void load() = 0;
	virtual void reset() = 0;
};

#ifdef LIBRETRO
#include "option_lr.h"
#else

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
			set(doLoad(settings.gameId, section + "." + name));
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
			doSave(settings.gameId, section + "." + name);
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
			if (paths[start] == '"')
			{
				std::string v;
				start++;
				while (true)
				{
					if (paths[start] == '"')
					{
						if (start + 1 >= paths.size())
						{
							newValue.push_back(v);
							return newValue;
						}
						if (paths[start + 1] == '"')
						{
							v += paths[start++];
							start++;
						}
						else if (paths[start + 1] == ';')
						{
							newValue.push_back(v);
							start += 2;
							break;
						}
						else
						{
							v += paths[start++];
						}
					}
					else
						v += paths[start++];
				}
			}
			else
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
		for (const auto& v : value)
		{
			if (!s.empty())
				s += ';';
			if (v.find(';') != std::string::npos || (!v.empty() && v[0] == '"'))
			{
				s += '"';
				std::string v2 = v;
				while (true)
				{
					auto pos = v2.find('"');
					if (pos != std::string::npos)
					{
						s += v2.substr(0, pos + 1) + '"';
						v2 = v2.substr(pos + 1);
					}
					else
					{
						s += v2;
						break;
					}
				}
				s += '"';
			}
			else
				s += v;
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
#endif

using OptionString = Option<std::string>;

// Dynarec

extern Option<bool> DynarecEnabled;
extern Option<bool> DynarecIdleSkip;
constexpr bool DynarecSafeMode = false;

// General

extern Option<int> Cable;		// 0 -> VGA, 1 -> VGA, 2 -> RGB, 3 -> TV Composite
extern Option<int> Region;		// 0 -> JP, 1 -> USA, 2 -> EU, 3 -> default
extern Option<int> Broadcast;	// 0 -> NTSC, 1 -> PAL, 2 -> PAL/M, 3 -> PAL/N, 4 -> default
extern Option<int> Language;	// 0 -> JP, 1 -> EN, 2 -> DE, 3 -> FR, 4 -> SP, 5 -> IT, 6 -> default
extern Option<bool> FullMMU;
extern Option<bool> ForceWindowsCE;
extern Option<bool> AutoLoadState;
extern Option<bool> AutoSaveState;
extern Option<int> SavestateSlot;

// Sound

constexpr bool LimitFPS = true;
extern Option<bool> DSPEnabled;
extern Option<int> AudioBufferSize;	//In samples ,*4 for bytes
extern Option<bool> AutoLatency;

extern OptionString AudioBackend;

class AudioVolumeOption : public Option<int> {
public:
	AudioVolumeOption() : Option<int>("aica.Volume", 100) {};
	float logarithmic_volume_scale = 1.0;

	void load() override {
		Option<int>::load();
		calcDbPower();
	}

	float dbPower()
	{
		return logarithmic_volume_scale;
	}
	void calcDbPower()
	{
		// dB scaling calculation: https://www.dr-lex.be/info-stuff/volumecontrols.html
		logarithmic_volume_scale = fmin(exp(4.605 * float(value) / 100.0) / 100.0, 1.0);
		if (value < 10)
			logarithmic_volume_scale *= value / 10.0;
	}
};
extern AudioVolumeOption AudioVolume;

// Rendering

class RendererOption : public Option<RenderType> {
public:
	RendererOption()
#ifdef USE_DX9
		: Option<RenderType>("pvr.rend", RenderType::DirectX9) {}
#else
		: Option<RenderType>("pvr.rend", RenderType::OpenGL) {}
#endif

	bool isDirectX() const {
		return value == RenderType::DirectX9;
	}

	RenderType& operator=(const RenderType& v) { set(v); return value; }
};
extern RendererOption RendererType;
extern Option<bool> UseMipmaps;
extern Option<bool> Widescreen;
extern Option<bool> SuperWidescreen;
extern Option<bool> ShowFPS;
extern Option<bool> RenderToTextureBuffer;
extern Option<bool> TranslucentPolygonDepthMask;
extern Option<bool> ModifierVolumes;
constexpr bool Clipping = true;
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
extern Option<bool> VSync;
extern Option<u64> PixelBufferSize;
extern Option<int> AnisotropicFiltering;
extern Option<bool> ThreadedRendering;
extern Option<bool> DupeFrames;

// Misc

extern Option<bool> SerialConsole;
extern Option<bool> SerialPTY;
extern Option<bool> UseReios;
extern Option<bool> FastGDRomLoad;

extern Option<bool> OpenGlChecks;

extern Option<std::vector<std::string>, false> ContentPath;
extern Option<bool, false> HideLegacyNaomiRoms;

// Network

extern Option<bool> NetworkEnable;
extern Option<bool> ActAsServer;
extern OptionString DNS;
extern OptionString NetworkServer;
extern Option<bool> EmulateBBA;
extern Option<bool> GGPOEnable;
extern Option<int> GGPODelay;
extern Option<bool> NetworkStats;
extern Option<int> GGPOAnalogAxes;
extern Option<bool> GGPOChat;

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
#ifdef _WIN32
extern Option<bool> UseRawInput;
#else
constexpr bool UseRawInput = false;
#endif

#ifdef USE_LUA
extern OptionString LuaFileName;
#endif 

} // namespace config

