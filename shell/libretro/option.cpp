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
#include "cfg/option.h"
#include "libretro_core_option_defines.h"

namespace config {

// Dynarec

Option<bool> DynarecEnabled("", true);
IntOption Sh4Clock(CORE_OPTION_NAME "_sh4clock", 200);

// General

Option<int> Cable("", 3);									// TV Composite
Option<int> Region(CORE_OPTION_NAME "_region", 1);			// USA
Option<int> Broadcast(CORE_OPTION_NAME "_broadcast", 0);	// NTSC
Option<int> Language(CORE_OPTION_NAME "_language", 1);		// English
Option<bool> AutoLoadState("");
Option<bool> AutoSaveState("");
Option<int, false> SavestateSlot("");
Option<bool> ForceFreePlay(CORE_OPTION_NAME "_force_freeplay", true);

// Sound

Option<bool> DSPEnabled(CORE_OPTION_NAME "_enable_dsp", false);
#if HOST_CPU == CPU_ARM
Option<int> AudioBufferSize("", 5644);	// 128 ms
#else
Option<int> AudioBufferSize("", 2822);	// 64 ms
#endif
Option<bool> AutoLatency("");

OptionString AudioBackend("", "auto");
Option<bool> VmuSound(CORE_OPTION_NAME "_vmu_sound", false);

// Rendering

RendererOption RendererType;
Option<bool> UseMipmaps(CORE_OPTION_NAME "_mipmapping", true);
Option<bool> Widescreen(CORE_OPTION_NAME "_widescreen_hack");
Option<bool> SuperWidescreen("");
Option<bool> ShowFPS("");
Option<bool> RenderToTextureBuffer(CORE_OPTION_NAME "_enable_rttb");
Option<bool> TranslucentPolygonDepthMask("");
Option<bool> ModifierVolumes(CORE_OPTION_NAME "_volume_modifier_enable", true);
IntOption TextureUpscale(CORE_OPTION_NAME "_texupscale", 1);
IntOption MaxFilteredTextureSize(CORE_OPTION_NAME "_texupscale_max_filtered_texture_size", 256);
Option<float> ExtraDepthScale("", 1.f);
Option<bool> CustomTextures(CORE_OPTION_NAME "_custom_textures");
Option<bool> PreloadCustomTextures(CORE_OPTION_NAME "_preload_custom_textures");
Option<bool> DumpTextures(CORE_OPTION_NAME "_dump_textures");
Option<bool> DumpReplacedTextures(CORE_OPTION_NAME "_dump_replaced_textures");
Option<int> ScreenStretching("", 100);
Option<bool> Fog(CORE_OPTION_NAME "_fog", true);
Option<bool> FloatVMUs("");
Option<bool> Rotate90("");
Option<bool> PerStripSorting("");
Option<bool> DelayFrameSwapping(CORE_OPTION_NAME "_delay_frame_swapping");
Option<bool> WidescreenGameHacks(CORE_OPTION_NAME "_widescreen_cheats");
std::array<Option<int>, 4> CrosshairColor {
	Option<int>(""),
	Option<int>(""),
	Option<int>(""),
	Option<int>(""),
};
Option<int> SkipFrame(CORE_OPTION_NAME "_frame_skipping");
Option<int> MaxThreads("", 3);
Option<int> AutoSkipFrame(CORE_OPTION_NAME "_auto_skip_frame", 0);
Option<int> RenderResolution("", 480);
Option<bool> IntegerScale("");
Option<bool> LinearInterpolation("", true);
Option<bool> VSync("", true);
Option<bool> ThreadedRendering(CORE_OPTION_NAME "_threaded_rendering", true);
Option<int> AnisotropicFiltering(CORE_OPTION_NAME "_anisotropic_filtering");
Option<int> TextureFiltering(CORE_OPTION_NAME "_texture_filtering");
Option<bool> PowerVR2Filter(CORE_OPTION_NAME "_pvr2_filtering");
Option<int64_t> PixelBufferSize("", 512_MB);
IntOption PerPixelLayers(CORE_OPTION_NAME "_oit_layers");
Option<bool> NativeDepthInterpolation(CORE_OPTION_NAME "_native_depth_interpolation");
Option<bool> EmulateFramebuffer(CORE_OPTION_NAME "_emulate_framebuffer", false);
Option<bool> FixUpscaleBleedingEdge(CORE_OPTION_NAME "_fix_upscale_bleeding_edge", true);

// Misc

Option<bool> SerialConsole("");
Option<bool> SerialPTY("");
Option<bool> UseReios(CORE_OPTION_NAME "_hle_bios");

Option<bool> OpenGlChecks("", false);
Option<bool> FastGDRomLoad(CORE_OPTION_NAME "_gdrom_fast_loading", false);
Option<bool> RamMod32MB(CORE_OPTION_NAME "_dc_32mb_mod", false);

//Option<std::vector<std::string>, false> ContentPath("");
//Option<bool, false> HideLegacyNaomiRoms("", true);

// Network

Option<bool> NetworkEnable("", false);
Option<bool> ActAsServer("", false);
OptionString DNS("", "dns.flyca.st");
OptionString NetworkServer("", "");
Option<int> LocalPort("", 0);
Option<bool> EmulateBBA(CORE_OPTION_NAME "_emulate_bba", false);
Option<bool> EnableUPnP(CORE_OPTION_NAME "_upnp", true);
Option<bool> GGPOEnable("", false);
Option<int> GGPODelay("", 0);
Option<bool> NetworkStats("", false);
Option<int> GGPOAnalogAxes("", 0);
Option<bool> NetworkOutput(CORE_OPTION_NAME "_network_output", false);
Option<int> MultiboardSlaves("", 0);
Option<bool> BattleCableEnable("", false);
Option<bool> UseDCNet(CORE_OPTION_NAME "_dcnet", false);
OptionString ISPUsername("", "flycast1");

// Maple

Option<int> MouseSensitivity("", 100);
Option<int> VirtualGamepadVibration("", 20);

std::array<Option<MapleDeviceType>, 4> MapleMainDevices {
	Option<MapleDeviceType>("", MDT_None),
	Option<MapleDeviceType>("", MDT_None),
	Option<MapleDeviceType>("", MDT_None),
	Option<MapleDeviceType>("", MDT_None),
};
std::array<std::array<Option<MapleDeviceType>, 2>, 4> MapleExpansionDevices {
	Option<MapleDeviceType>("", MDT_None),
	Option<MapleDeviceType>("", MDT_None),

	Option<MapleDeviceType>("", MDT_None),
	Option<MapleDeviceType>("", MDT_None),

	Option<MapleDeviceType>("", MDT_None),
	Option<MapleDeviceType>("", MDT_None),

	Option<MapleDeviceType>("", MDT_None),
	Option<MapleDeviceType>("", MDT_None),
};

std::array<std::array<Option<int>, 2>, 4> NetworkExpansionDevices{{
	{{Option<int>("", 0),
	Option<int>("", 0)}},

	{{Option<int>("", 0),
	Option<int>("", 0)}},

	{{Option<int>("", 0),
	Option<int>("", 0)}},

	{{Option<int>("", 0),
	Option<int>("", 0)}},
}};
Option<bool> UsePhysicalVmuMemory(CORE_OPTION_NAME "_linked_vmu_storage", false);

} // namespace config
