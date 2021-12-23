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
#include "lua.h"

#ifdef USE_LUA
#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include "rend/gui.h"
#include "hw/mem/_vmem.h"
#include "cfg/option.h"
#include "emulator.h"
#include "input/gamepad_device.h"
#include "input/mouse.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "stdclass.h"
#include "imgui/imgui.h"

namespace lua
{
const char *CallbackTable = "flycast_callbacks";
static lua_State *L;
using namespace luabridge;

static void emuEventCallback(Event event, void *)
{
	if (L == nullptr)
		return;
	try {
		LuaRef v = LuaRef::getGlobal(L, CallbackTable);
		if (!v.isTable())
			return;
		const char *key = nullptr;
		switch (event)
		{
		case Event::Start:
			key = "start";
			break;
		case Event::Resume:
			key = "resume";
			break;
		case Event::Pause:
			key = "pause";
			break;
		case Event::Terminate:
			key = "terminate";
			break;
		case Event::LoadState:
			key = "loadState";
			break;
		}
		if (v[key].isFunction())
			v[key]();
	} catch (const LuaException& e) {
		WARN_LOG(COMMON, "Lua exception: %s", e.what());
	}
}

template<const char *Tag>
void eventCallback()
{
	if (L == nullptr)
		return;
	try {
		LuaRef v = LuaRef::getGlobal(L, CallbackTable);
		if (v.isTable() && v[Tag].isFunction())
			v[Tag]();
	} catch (const LuaException& e) {
		WARN_LOG(COMMON, "Lua exception[%s]: %s", Tag, e.what());
	}
}

const char VBlankEvent[] { "vblank" };
void vblank()
{
	eventCallback<VBlankEvent>();
}

const char OverlayEvent[] { "overlay" };
void overlay()
{
	eventCallback<OverlayEvent>();
}

template<typename T>
static LuaRef readMemoryTable(u32 address, int count, lua_State* L)
{
	LuaRef t(L);
	t = newTable(L);
	while (count > 0)
	{
		t[address] = _vmem_readt<T, T>(address);
		address += sizeof(T);
		count--;
	}

	return t;
}

#define CONFIG_ACCESSORS(Config) 	\
template<typename T>				\
static T get ## Config() {			\
	return config::Config.get();	\
}									\
template<typename T>				\
static void set ## Config(T v)		\
{									\
	config::Config.set(v);			\
}

// General
CONFIG_ACCESSORS(Cable);
CONFIG_ACCESSORS(Region);
CONFIG_ACCESSORS(Broadcast);
CONFIG_ACCESSORS(Language);
CONFIG_ACCESSORS(FullMMU);
CONFIG_ACCESSORS(ForceWindowsCE);
CONFIG_ACCESSORS(AutoLoadState);
CONFIG_ACCESSORS(AutoSaveState);
CONFIG_ACCESSORS(SavestateSlot);
// TODO Option<std::vector<std::string>, false> ContentPath;
CONFIG_ACCESSORS(HideLegacyNaomiRoms)

// Video
CONFIG_ACCESSORS(RendererType)
CONFIG_ACCESSORS(Widescreen)
CONFIG_ACCESSORS(UseMipmaps)
CONFIG_ACCESSORS(SuperWidescreen)
CONFIG_ACCESSORS(ShowFPS)
CONFIG_ACCESSORS(RenderToTextureBuffer)
CONFIG_ACCESSORS(TranslucentPolygonDepthMask)
CONFIG_ACCESSORS(ModifierVolumes)
CONFIG_ACCESSORS(TextureUpscale)
CONFIG_ACCESSORS(MaxFilteredTextureSize)
CONFIG_ACCESSORS(ExtraDepthScale)
CONFIG_ACCESSORS(CustomTextures)
CONFIG_ACCESSORS(DumpTextures)
CONFIG_ACCESSORS(ScreenStretching)
CONFIG_ACCESSORS(Fog)
CONFIG_ACCESSORS(FloatVMUs)
CONFIG_ACCESSORS(Rotate90)
CONFIG_ACCESSORS(PerStripSorting)
CONFIG_ACCESSORS(DelayFrameSwapping)
CONFIG_ACCESSORS(WidescreenGameHacks)
//TODO CrosshairColor;
CONFIG_ACCESSORS(SkipFrame)
CONFIG_ACCESSORS(MaxThreads)
CONFIG_ACCESSORS(AutoSkipFrame)
CONFIG_ACCESSORS(RenderResolution)
CONFIG_ACCESSORS(VSync)
CONFIG_ACCESSORS(PixelBufferSize)
CONFIG_ACCESSORS(AnisotropicFiltering)
CONFIG_ACCESSORS(ThreadedRendering)

// Audio
CONFIG_ACCESSORS(DSPEnabled)
CONFIG_ACCESSORS(AudioBufferSize)
CONFIG_ACCESSORS(AutoLatency)
CONFIG_ACCESSORS(AudioBackend)
CONFIG_ACCESSORS(AudioVolume)

// Advanced
CONFIG_ACCESSORS(DynarecEnabled)
CONFIG_ACCESSORS(DynarecIdleSkip)
CONFIG_ACCESSORS(SerialConsole)
CONFIG_ACCESSORS(SerialPTY)
CONFIG_ACCESSORS(UseReios)
CONFIG_ACCESSORS(FastGDRomLoad)
CONFIG_ACCESSORS(OpenGlChecks)

// Network
CONFIG_ACCESSORS(NetworkEnable)
CONFIG_ACCESSORS(ActAsServer)
CONFIG_ACCESSORS(DNS)
CONFIG_ACCESSORS(NetworkServer)
CONFIG_ACCESSORS(EmulateBBA)
CONFIG_ACCESSORS(GGPOEnable)
CONFIG_ACCESSORS(GGPODelay)
CONFIG_ACCESSORS(NetworkStats)
CONFIG_ACCESSORS(GGPOAnalogAxes)

// Maple devices

static int getMapleType(int bus, lua_State *L)
{
	luaL_argcheck(L, bus >= 1 && bus <= 4, 1, "bus must be between 1 and 4");
	if (MapleDevices[bus - 1][5] == nullptr)
		return MDT_None;
	return MapleDevices[bus - 1][5]->get_device_type();
}

static int getMapleSubType(int bus, int port, lua_State *L)
{
	luaL_argcheck(L, bus >= 1 && bus <= 4, 1, "bus must be between 1 and 4");
	luaL_argcheck(L, port >= 1 && port <= 2, 2, "port must be between 1 and 2");
	if (MapleDevices[bus - 1][port - 1] == nullptr)
		return MDT_None;
	return MapleDevices[bus - 1][port - 1]->get_device_type();
}

static void setMapleType(int bus, int type, lua_State *L)
{
	luaL_argcheck(L, bus >= 1 && bus <= 4, 1, "bus must be between 1 and 4");
	switch ((MapleDeviceType)type) {
	case MDT_SegaController:
	case MDT_AsciiStick:
	case MDT_Keyboard:
	case MDT_Mouse:
	case MDT_LightGun:
	case MDT_TwinStick:
	case MDT_None:
		config::MapleMainDevices[bus - 1] = (MapleDeviceType)type;
		maple_ReconnectDevices();
		break;
	default:
		luaL_argerror(L, 2, "Invalid device type");
		break;
	}
}

static void setMapleSubType(int bus, int port, int type, lua_State *L)
{
	luaL_argcheck(L, bus >= 1 && bus <= 4, 1, "bus must be between 1 and 4");
	luaL_argcheck(L, port >= 1 && port <= 2, 2, "port must be between 1 and 2");
	switch ((MapleDeviceType)type) {
	case MDT_SegaVMU:
	case MDT_PurupuruPack:
	case MDT_Microphone:
	case MDT_None:
		config::MapleExpansionDevices[bus - 1][port - 1] = (MapleDeviceType)type;
		maple_ReconnectDevices();
		break;
	default:
		luaL_argerror(L, 3, "Invalid device type");
		break;
	}
}

// Inputs

static void checkPlayerNum(lua_State *L, int player) {
	luaL_argcheck(L, player >= 1 && player <= 4, 1, "player must be between 1 and 4");
}

static u32 getButtons(int player, lua_State *L)
{
	checkPlayerNum(L, player);
	return kcode[player - 1];
}

static void pressButtons(int player, u32 buttons, lua_State *L)
{
	checkPlayerNum(L, player);
	kcode[player - 1] &= ~buttons;
}

static void releaseButtons(int player, u32 buttons, lua_State *L)
{
	checkPlayerNum(L, player);
	kcode[player - 1] |= buttons;
}

static int getAxis(int player, int axis, lua_State *L)
{
	checkPlayerNum(L, player);
	luaL_argcheck(L, axis >= 1 && axis <= 6, 2, "axis must be between 1 and 6");
	switch (axis - 1)
	{
	case 0:
		return joyx[player - 1];
	case 1:
		return joyy[player - 1];
	case 2:
		return joyrx[player - 1];
	case 3:
		return joyry[player - 1];
	case 4:
		return lt[player - 1];
	case 5:
		return rt[player - 1];
	default:
		return 0;
	}
}

static void setAxis(int player, int axis, int value, lua_State *L)
{
	checkPlayerNum(L, player);
	luaL_argcheck(L, axis >= 1 && axis <= 6, 2, "axis must be between 1 and 6");
	switch (axis - 1)
	{
	case 0:
		joyx[player - 1] = value;
		break;
	case 1:
		joyy[player - 1] = value;
		break;
	case 2:
		joyrx[player - 1] = value;
		break;
	case 3:
		joyry[player - 1] = value;
		break;
	case 4:
		lt[player - 1] = value;
		break;
	case 5:
		rt[player - 1] = value;
		break;
	default:
		break;
	}
}

static int getAbsCoordinates(lua_State *L)
{
	int player = luaL_checkinteger(L, 1);
	checkPlayerNum(L, player);
	lua_pushnumber(L, mo_x_abs[player - 1]);
	lua_pushnumber(L, mo_y_abs[player - 1]);
	return 2;
}

static void setAbsCoordinates(int player, int x, int y, lua_State *L)
{
	checkPlayerNum(L, player);
	SetMousePosition(x, y, settings.display.width, settings.display.height, player - 1);
}

static int getRelCoordinates(lua_State *L)
{
	int player = luaL_checkinteger(L, 1);
	checkPlayerNum(L, player);
	lua_pushnumber(L, mo_x_delta[player - 1]);
	lua_pushnumber(L, mo_y_delta[player - 1]);
	return 2;
}

static void setRelCoordinates(int player, float x, float y, lua_State *L)
{
	checkPlayerNum(L, player);
	SetRelativeMousePosition(x, y, player - 1);
}

// UI

static void beginWindow(const char *title, int x, int y, int w, int h)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::SetNextWindowPos(ImVec2(x, y));
	ImGui::SetNextWindowSize(ImVec2(w * scaling, h * scaling));
	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin(title, NULL, ImGuiWindowFlags_AlwaysAutoResize |  ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
}

static void endWindow()
{
	ImGui::PopStyleColor();
	ImGui::End();
	ImGui::PopStyleVar(2);
}

static void uiText(const std::string& text)
{
	ImGui::Text("%s", text.c_str());
}

static void uiTextRightAligned(const std::string& text)
{
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text.c_str()).x);
	uiText(text);
}

static void uiBargraph(float v)
{
	ImGui::ProgressBar(v, ImVec2(-1, 10.f * scaling), "");
}

static int uiButton(lua_State *L)
{
	const char *label = luaL_checkstring(L, 1);
	if (ImGui::Button(label))
	{
		LuaRef callback = LuaRef::fromStack(L, 2);
		if (callback.isFunction())
			callback();
	}
	return 0;
}

static void luaRegister(lua_State *L)
{
	getGlobalNamespace(L)
		.beginNamespace ("flycast")
	  		.beginNamespace("emulator")
				.addFunction("startGame", gui_start_game)	// FIXME threading!
				.addFunction("stopGame", std::function<void()>([]() { gui_stop_game(""); }))
				.addFunction("pause", std::function<void()>([]() {
					if (gui_state == GuiState::Closed)
						gui_open_settings();
				}))
				.addFunction("resume", std::function<void()>([]() {
					if (gui_state == GuiState::Commands)
						gui_open_settings();
				}))
				.addFunction("saveState", std::function<void(int)>([](int index) {
					bool restart = false;
					if (gui_state == GuiState::Closed) {
						gui_open_settings();
						restart = true;
					}
					dc_savestate(index);
					if (restart)
						gui_open_settings();
				}))
				.addFunction("loadState", std::function<void(int)>([](int index) {
					bool restart = false;
					if (gui_state == GuiState::Closed) {
						gui_open_settings();
						restart = true;
					}
					dc_loadstate(index);
					if (restart)
						gui_open_settings();
				}))
				.addFunction("exit", dc_exit)
				.addFunction("displayNotification", gui_display_notification)
			.endNamespace()

	  		.beginNamespace("config")
#define CONFIG_PROPERTY(Config, type) .addProperty<type>(#Config, get ## Config, set ## Config)
				.beginNamespace("general")
					CONFIG_PROPERTY(Cable, int)
					CONFIG_PROPERTY(Region, int)
					CONFIG_PROPERTY(Broadcast, int)
					CONFIG_PROPERTY(Language, int)
					CONFIG_PROPERTY(AutoLoadState, bool)
					CONFIG_PROPERTY(AutoSaveState, bool)
					CONFIG_PROPERTY(SavestateSlot, int)
					CONFIG_PROPERTY(HideLegacyNaomiRoms, bool)
				.endNamespace()

				.beginNamespace("video")
// FIXME			.addProperty<RenderType>("RendererType", getRendererType, setRendererType)
					CONFIG_PROPERTY(Widescreen, bool)
					CONFIG_PROPERTY(SuperWidescreen, bool)
					CONFIG_PROPERTY(UseMipmaps, bool)
					CONFIG_PROPERTY(ShowFPS, bool)
					CONFIG_PROPERTY(RenderToTextureBuffer, bool)
					CONFIG_PROPERTY(TranslucentPolygonDepthMask, bool)
					CONFIG_PROPERTY(ModifierVolumes, bool)
					CONFIG_PROPERTY(TextureUpscale, int)
					CONFIG_PROPERTY(MaxFilteredTextureSize, int)
					CONFIG_PROPERTY(ExtraDepthScale, float)
					CONFIG_PROPERTY(CustomTextures, bool)
					CONFIG_PROPERTY(DumpTextures, bool)
					CONFIG_PROPERTY(ScreenStretching, int)
					CONFIG_PROPERTY(Fog, bool)
					CONFIG_PROPERTY(FloatVMUs, bool)
					CONFIG_PROPERTY(Rotate90, bool)
					CONFIG_PROPERTY(PerStripSorting, bool)
					CONFIG_PROPERTY(DelayFrameSwapping, bool)
					CONFIG_PROPERTY(WidescreenGameHacks, bool)
					// TODO CrosshairColor;
					CONFIG_PROPERTY(SkipFrame, int)
					CONFIG_PROPERTY(MaxThreads, int)
					CONFIG_PROPERTY(AutoSkipFrame, int)
					CONFIG_PROPERTY(RenderResolution, int)
					CONFIG_PROPERTY(VSync, bool)
					CONFIG_PROPERTY(PixelBufferSize, u64)
					CONFIG_PROPERTY(AnisotropicFiltering, int)
					CONFIG_PROPERTY(ThreadedRendering, bool)
				.endNamespace()

				.beginNamespace("audio")
					CONFIG_PROPERTY(DSPEnabled, bool)
					CONFIG_PROPERTY(AudioBufferSize, int)
					CONFIG_PROPERTY(AutoLatency, bool)
					CONFIG_PROPERTY(AudioBackend, std::string)
					CONFIG_PROPERTY(AudioVolume, int)
				.endNamespace()

				.beginNamespace("advanced")
					CONFIG_PROPERTY(DynarecEnabled, bool)
					CONFIG_PROPERTY(DynarecIdleSkip, bool)
					CONFIG_PROPERTY(SerialConsole, bool)
					CONFIG_PROPERTY(SerialPTY, bool)
					CONFIG_PROPERTY(UseReios, bool)
					CONFIG_PROPERTY(FastGDRomLoad, bool)
					CONFIG_PROPERTY(OpenGlChecks, bool)
					CONFIG_PROPERTY(FullMMU, bool)
					CONFIG_PROPERTY(ForceWindowsCE, bool)
				.endNamespace()

				.beginNamespace("network")
					CONFIG_PROPERTY(NetworkEnable, bool)
					CONFIG_PROPERTY(ActAsServer, bool)
					CONFIG_PROPERTY(DNS, std::string)
					CONFIG_PROPERTY(NetworkServer, std::string)
					CONFIG_PROPERTY(EmulateBBA, bool)
					CONFIG_PROPERTY(GGPOEnable, bool)
					CONFIG_PROPERTY(GGPODelay, int)
					CONFIG_PROPERTY(NetworkStats, bool)
					CONFIG_PROPERTY(GGPOAnalogAxes, int)
				.endNamespace()

				.beginNamespace("maple")
					.addFunction("getDeviceType", getMapleType)
					.addFunction("getSubDeviceType", getMapleSubType)
					.addFunction("setDeviceType", setMapleType)
					.addFunction("setSubDeviceType", setMapleSubType)
				.endNamespace()
			.endNamespace()

	  		.beginNamespace("memory")
				.addFunction("read8", _vmem_readt<u8, u8>)
				.addFunction("read16", _vmem_readt<u16, u16>)
				.addFunction("read32", _vmem_readt<u32, u32>)
				.addFunction("read64", _vmem_readt<u64, u64>)
				.addFunction("readTable8", readMemoryTable<u8>)
				.addFunction("readTable16", readMemoryTable<u16>)
				.addFunction("readTable32", readMemoryTable<u32>)
				.addFunction("readTable64", readMemoryTable<u64>)
				.addFunction("write8", _vmem_writet<u8>)
				.addFunction("write16", _vmem_writet<u16>)
				.addFunction("write32", _vmem_writet<u32>)
				.addFunction("write64", _vmem_writet<u64>)
			.endNamespace()

			.beginNamespace("input")
				.addFunction("getButtons", getButtons)
				.addFunction("pressButtons", pressButtons)
				.addFunction("releaseButtons", releaseButtons)
				.addFunction("getAxis", getAxis)
				.addFunction("setAxis", setAxis)
				.addFunction("getAbsCoordinates", getAbsCoordinates)
				.addFunction("setAbsCoordinates", setAbsCoordinates)
				.addFunction("getRelCoordinates", getRelCoordinates)
				.addFunction("setRelCoordinates", setRelCoordinates)
			.endNamespace()

			.beginNamespace("state")
				.addProperty("system", &settings.platform.system, false)
				.addProperty("media", &settings.content.path, false)
				.addProperty("gameId", &settings.content.gameId, false)
				.beginNamespace("display")
					.addProperty("width", &settings.display.width, false)
					.addProperty("height", &settings.display.height, false)
				.endNamespace()
			.endNamespace()

			.beginNamespace("ui")
				.addFunction("beginWindow", beginWindow)
				.addFunction("endWindow", endWindow)
				.addFunction("text", uiText)
				.addFunction("rightText", uiTextRightAligned)
				.addFunction("bargraph", uiBargraph)
				.addFunction("button", uiButton)
			.endNamespace()
		.endNamespace();
}

static std::string getLuaFile()
{
	std::string initFile;
	if( !config::LuaFileName.get().empty()){
		initFile = get_readonly_config_path(config::LuaFileName.get());
	} else {
		initFile = get_readonly_config_path("flycast.lua");
	}

	return initFile;

} 

static void doExec(const std::string& path)
{
	if (L == nullptr)
		return;
	DEBUG_LOG(COMMON, "Executing script: %s", path.c_str());
	int err = luaL_dofile(L, path.c_str());
	if (err != 0)
		WARN_LOG(COMMON, "Lua error: %s", lua_tostring(L, -1));
}

void exec(const std::string& path)
{
	std::string file = get_readonly_config_path(path);
	if (!file_exists(file))
		return;
	doExec(file);
}

void init()
{
	std::string initFile = getLuaFile();
	if (!file_exists(initFile))
		return;
	L = luaL_newstate();
	luaL_openlibs(L);
	luaRegister(L);
    EventManager::listen(Event::Start, emuEventCallback);
    EventManager::listen(Event::Resume, emuEventCallback);
    EventManager::listen(Event::Pause, emuEventCallback);
    EventManager::listen(Event::Terminate, emuEventCallback);
    EventManager::listen(Event::LoadState, emuEventCallback);

	doExec(initFile);
}

void term()
{
	if (L == nullptr)
		return;
    EventManager::unlisten(Event::Start, emuEventCallback);
    EventManager::unlisten(Event::Resume, emuEventCallback);
    EventManager::unlisten(Event::Pause, emuEventCallback);
    EventManager::unlisten(Event::Terminate, emuEventCallback);
    EventManager::unlisten(Event::LoadState, emuEventCallback);
	lua_close(L);
	L = nullptr;
}

}
#endif
