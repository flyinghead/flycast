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
#include "option.h"
#include "network/naomi_network.h"
#include "debug/gdb_server.h"

namespace config {

// Dynarec

Option<bool> DynarecEnabled("Dynarec.Enabled", true);
Option<int> Sh4Clock("Sh4Clock", 200);

// General

Option<int> Cable("Dreamcast.Cable", 3);			// TV Composite
Option<int> Region("Dreamcast.Region", 1);			// USA
Option<int> Broadcast("Dreamcast.Broadcast", 0);	// NTSC
Option<int> Language("Dreamcast.Language", 1);		// English
Option<bool> AutoLoadState("Dreamcast.AutoLoadState");
Option<bool> AutoSaveState("Dreamcast.AutoSaveState");
Option<int, false> SavestateSlot("Dreamcast.SavestateSlot");
Option<bool> ForceFreePlay("ForceFreePlay", true);
Option<bool, false> FetchBoxart("FetchBoxart", true);
Option<bool, false> BoxartDisplayMode("BoxartDisplayMode", true);
Option<int, false> UIScaling("UIScaling", 100);
Option<int, false> UITheme("UITheme", 0);

// Sound

Option<bool> DSPEnabled("aica.DSPEnabled", false);
#if HOST_CPU == CPU_ARM
Option<int> AudioBufferSize("aica.BufferSize", 5644);	// 128 ms
#else
Option<int> AudioBufferSize("aica.BufferSize", 2822);	// 64 ms
#endif
Option<bool> AutoLatency("aica.AutoLatency",
#ifdef __ANDROID__
		true
#else
		false
#endif
		);

OptionString AudioBackend("backend", "auto", "audio");
AudioVolumeOption AudioVolume;
Option<bool> VmuSound("VmuSound", false, "audio");

// Rendering

RendererOption RendererType;
Option<bool> UseMipmaps("rend.UseMipmaps", true);
Option<bool> Widescreen("rend.WideScreen");
Option<bool> SuperWidescreen("rend.SuperWideScreen");
Option<bool> ShowFPS("rend.ShowFPS");
Option<bool> RenderToTextureBuffer("rend.RenderToTextureBuffer");
Option<bool> TranslucentPolygonDepthMask("rend.TranslucentPolygonDepthMask");
Option<bool> ModifierVolumes("rend.ModifierVolumes", true);
Option<int> TextureUpscale("rend.TextureUpscale2", 1);
Option<int> MaxFilteredTextureSize("rend.MaxFilteredTextureSize", 256);
Option<float> ExtraDepthScale("rend.ExtraDepthScale", 1.f);
Option<bool> CustomTextures("rend.CustomTextures");
Option<bool> PreloadCustomTextures("rend.PreloadCustomTextures");
Option<bool> DumpTextures("rend.DumpTextures");
Option<bool> DumpReplacedTextures("rend.DumpReplacedTextures");
Option<int> ScreenStretching("rend.ScreenStretching", 100);
Option<bool> Fog("rend.Fog", true);
Option<bool> FloatVMUs("rend.FloatVMUs");
Option<bool> Rotate90("rend.Rotate90");
Option<bool> PerStripSorting("rend.PerStripSorting");
#ifdef __APPLE__
Option<bool> DelayFrameSwapping("rend.DelayFrameSwapping", false);
#else
Option<bool> DelayFrameSwapping("rend.DelayFrameSwapping", true);
#endif
Option<bool> WidescreenGameHacks("rend.WidescreenGameHacks");
std::array<Option<int>, 4> CrosshairColor {
	Option<int>("rend.CrossHairColor1"),
	Option<int>("rend.CrossHairColor2"),
	Option<int>("rend.CrossHairColor3"),
	Option<int>("rend.CrossHairColor4"),
};
Option<int> CrosshairSize("rend.CrosshairSize", 40);
Option<int> SkipFrame("ta.skip");
Option<int> MaxThreads("pvr.MaxThreads", 3);
Option<int> AutoSkipFrame("pvr.AutoSkipFrame", 0);
Option<int> RenderResolution("rend.Resolution", 480);
Option<bool> IntegerScale("rend.IntegerScale", false);
Option<bool> LinearInterpolation("rend.LinearInterpolation", true);
Option<bool> VSync("rend.vsync", true);
Option<int64_t> PixelBufferSize("rend.PixelBufferSize", 512_MB);
Option<int> AnisotropicFiltering("rend.AnisotropicFiltering", 1);
Option<int> TextureFiltering("rend.TextureFiltering", 0); // Default
Option<bool> ThreadedRendering("rend.ThreadedRendering", true);
Option<bool> DupeFrames("rend.DupeFrames", false);
Option<int> PerPixelLayers("rend.PerPixelLayers", 32);
#ifdef TARGET_UWP
Option<bool> NativeDepthInterpolation("rend.NativeDepthInterpolation", true);
#else
Option<bool> NativeDepthInterpolation("rend.NativeDepthInterpolation", false);
#endif
Option<bool> EmulateFramebuffer("rend.EmulateFramebuffer", false);
Option<bool> FixUpscaleBleedingEdge("rend.FixUpscaleBleedingEdge", true);
Option<bool> CustomGpuDriver("rend.CustomGpuDriver", false);
#ifdef VIDEO_ROUTING
Option<bool, false> VideoRouting("rend.VideoRouting", false);
Option<bool, false> VideoRoutingScale("rend.VideoRoutingScale", false);
Option<int, false> VideoRoutingVRes("rend.VideoRoutingVRes", 720);
#endif

// Misc

Option<bool> SerialConsole("Debug.SerialConsoleEnabled");
Option<bool> SerialPTY("Debug.SerialPTY");
Option<bool> GDB("Debug.GDBEnabled");
Option<int> GDBPort("Debug.GDBPort", debugger::DEFAULT_PORT);
Option<bool> GDBWaitForConnection("Debug.GDBWaitForConnection");
Option<bool> UseReios("UseReios");
Option<bool> FastGDRomLoad("FastGDRomLoad", false);
Option<bool> RamMod32MB("Dreamcast.RamMod32MB", false);

Option<bool> OpenGlChecks("OpenGlChecks", false, "validate");

Option<std::vector<std::string>, false> ContentPath("Dreamcast.ContentPath");
Option<std::vector<std::string>, false> BiosPath("Dreamcast.BiosPath");
Option<std::string, false> VMUPath("Dreamcast.VMUPath");
Option<std::vector<std::string>, false> SavestatePath("Dreamcast.SavestatePath");
Option<std::string, false> SavePath("Dreamcast.SavePath");
Option<std::vector<std::string>, false> TexturePath("Dreamcast.TexturePath");
Option<std::string, false> TextureDumpPath("Dreamcast.TextureDumpPath");
Option<std::string, false> BoxartPath("Dreamcast.BoxartPath");
Option<std::vector<std::string>, false> MappingsPath("Dreamcast.MappingsPath");
Option<std::vector<std::string>, false> CheatPath("Dreamcast.CheatPath");
Option<bool, false> HideLegacyNaomiRoms("Dreamcast.HideLegacyNaomiRoms", true);
Option<bool, false> UploadCrashLogs("UploadCrashLogs", true);
Option<bool, false> DiscordPresence("DiscordPresence", true);
#if defined(__ANDROID__) && !defined(LIBRETRO)
Option<bool, false> UseSafFilePicker("UseSafFilePicker", true);
#endif
OptionString LogServer("LogServer", "", "log");

// Profiler
Option<bool> ProfilerEnabled("Profiler.Enabled");
Option<bool> ProfilerDrawToGUI("Profiler.DrawGUI");
Option<bool> ProfilerOutputTTY("Profiler.OutputTTY");
Option<float> ProfilerFrameWarningTime("Profiler.FrameWarningTime", 1.0f / 55.0f);

// Network

Option<bool> NetworkEnable("Enable", false, "network");
Option<bool> ActAsServer("ActAsServer", false, "network");
OptionString DNS("DNS", "dns.flyca.st", "network");
OptionString NetworkServer("server", "", "network");
Option<int> LocalPort("LocalPort", NaomiNetwork::SERVER_PORT, "network");
Option<bool> EmulateBBA("EmulateBBA", false, "network");
Option<bool> EnableUPnP("EnableUPnP", true, "network");
Option<bool> GGPOEnable("GGPO", false, "network");
Option<int> GGPODelay("GGPODelay", 0, "network");
Option<bool> NetworkStats("Stats", true, "network");
Option<int> GGPOAnalogAxes("GGPOAnalogAxes", 0, "network");
Option<bool> GGPOChat("GGPOChat", true, "network");
Option<bool> GGPOChatTimeoutToggle("GGPOChatTimeoutToggle", true, "network");
Option<int> GGPOChatTimeout("GGPOChatTimeout", 10, "network");
Option<bool> NetworkOutput("NetworkOutput", false, "network");
Option<int> MultiboardSlaves("MultiboardSlaves", 1, "network");
Option<bool> BattleCableEnable("BattleCable", false, "network");
Option<bool> UseDCNet("DCNet", false, "network");
OptionString ISPUsername("ISPUsername", "flycast1", "network");

#ifdef USE_OMX
Option<int> OmxAudioLatency("audio_latency", 100, "omx");
Option<bool> OmxAudioHdmi("audio_hdmi", true, "omx");
#endif

// Maple

Option<int> MouseSensitivity("MouseSensitivity", 100, "input");
Option<int> VirtualGamepadVibration("VirtualGamepadVibration", 20, "input");
Option<int> VirtualGamepadTransparency("VirtualGamepadTransparency", 37, "input");

std::array<Option<MapleDeviceType>, 4> MapleMainDevices {
	Option<MapleDeviceType>("device1", MDT_SegaController, "input"),
	Option<MapleDeviceType>("device2", MDT_None, "input"),
	Option<MapleDeviceType>("device3", MDT_None, "input"),
	Option<MapleDeviceType>("device4", MDT_None, "input"),
};
std::array<std::array<Option<MapleDeviceType>, 2>, 4> MapleExpansionDevices {{
	{{Option<MapleDeviceType>("device1.1", MDT_SegaVMU, "input"),
	Option<MapleDeviceType>("device1.2", MDT_SegaVMU, "input")}},

	{{Option<MapleDeviceType>("device2.1", MDT_None, "input"),
	Option<MapleDeviceType>("device2.2", MDT_None, "input")}},

	{{Option<MapleDeviceType>("device3.1", MDT_None, "input"),
	Option<MapleDeviceType>("device3.2", MDT_None, "input")}},

	{{Option<MapleDeviceType>("device4.1", MDT_None, "input"),
	Option<MapleDeviceType>("device4.2", MDT_None, "input")}},
}};

std::array<std::array<Option<int>, 2>, 4> NetworkExpansionDevices{{
	{{Option<int>("device1.1.net", 0, "input"),
	Option<int>("device1.2.net", 0, "input")}},

	{{Option<int>("device2.1.net", 0, "input"),
	Option<int>("device2.2.net", 0, "input")}},

	{{Option<int>("device3.1.net", 0, "input"),
	Option<int>("device3.2.net", 0, "input")}},

	{{Option<int>("device4.1.net", 0, "input"),
	Option<int>("device4.2.net", 0, "input")}},
}};
Option<bool> PerGameVmu("PerGameVmu", true, "config");
#ifdef _WIN32
Option<bool, false> UseRawInput("RawInput", false, "input");
#endif
Option<bool> UsePhysicalVmuMemory("UsePhysicalVmuMemory", false);

#ifdef USE_LUA
Option<std::string, false> LuaFileName("LuaFileName", "flycast.lua");
#endif

// RetroAchievements

Option<bool> EnableAchievements("Enabled", false, "achievements");
Option<bool> AchievementsHardcoreMode("HardcoreMode", false, "achievements");
OptionString AchievementsUserName("UserName", "", "achievements");
OptionString AchievementsToken("Token", "", "achievements");

} // namespace config
