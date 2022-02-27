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
Option<bool> DynarecIdleSkip("", true);

// General

Option<int> Cable("", 3);									// TV Composite
Option<int> Region(CORE_OPTION_NAME "_region", 1);			// USA
Option<int> Broadcast(CORE_OPTION_NAME "_broadcast", 0);	// NTSC
Option<int> Language(CORE_OPTION_NAME "_language", 1);		// English
Option<bool> FullMMU("");
Option<bool> ForceWindowsCE(CORE_OPTION_NAME "_force_wince");
Option<bool> AutoLoadState("");
Option<bool> AutoSaveState("");
Option<int> SavestateSlot("");

// Sound

Option<bool> DSPEnabled(CORE_OPTION_NAME "_enable_dsp", false);
#if HOST_CPU == CPU_ARM
Option<int> AudioBufferSize("", 5644);	// 128 ms
#else
Option<int> AudioBufferSize("", 2822);	// 64 ms
#endif
Option<bool> AutoLatency("");

OptionString AudioBackend("", "auto");

// Rendering

RendererOption RendererType;
Option<bool> UseMipmaps(CORE_OPTION_NAME "_mipmapping", true);
Option<bool> Widescreen(CORE_OPTION_NAME "_widescreen_hack");
Option<bool> SuperWidescreen("");
Option<bool> ShowFPS("");
Option<bool> RenderToTextureBuffer(CORE_OPTION_NAME "_enable_rttb");
Option<bool> TranslucentPolygonDepthMask("");
Option<bool> ModifierVolumes(CORE_OPTION_NAME "_volume_modifier_enable", true);
Option<int> TextureUpscale(CORE_OPTION_NAME "_texupscale", 1);
Option<int> MaxFilteredTextureSize(CORE_OPTION_NAME "_texupscale_max_filtered_texture_size", 256);
Option<float> ExtraDepthScale("", 1.f);
Option<bool> CustomTextures(CORE_OPTION_NAME "_custom_textures");
Option<bool> DumpTextures(CORE_OPTION_NAME "_dump_textures");
Option<int> ScreenStretching("", 100);
Option<bool> Fog(CORE_OPTION_NAME "_fog", true);
Option<bool> FloatVMUs("");
Option<bool> Rotate90("");
Option<bool> PerStripSorting("rend.PerStripSorting");
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
Option<bool> VSync("", true);
Option<bool> ThreadedRendering(CORE_OPTION_NAME "_threaded_rendering", true);
Option<int> AnisotropicFiltering(CORE_OPTION_NAME "_anisotropic_filtering");
Option<bool> PowerVR2Filter(CORE_OPTION_NAME "_pvr2_filtering");
Option<u64> PixelBufferSize("", 512 * 1024 * 1024);

// Misc

Option<bool> SerialConsole("");
Option<bool> SerialPTY("");
Option<bool> UseReios(CORE_OPTION_NAME "_hle_bios");

Option<bool> OpenGlChecks("", false);
Option<bool> FastGDRomLoad(CORE_OPTION_NAME "_gdrom_fast_loading", false);

//Option<std::vector<std::string>, false> ContentPath("");
//Option<bool, false> HideLegacyNaomiRoms("", true);

// Network

Option<bool> NetworkEnable("", false);
Option<bool> ActAsServer("", false);
OptionString DNS("", "46.101.91.123");
OptionString NetworkServer("", "");
Option<bool> EmulateBBA("", false); // TODO
Option<bool> GGPOEnable("", false);
Option<int> GGPODelay("", 0);
Option<bool> NetworkStats("", false);
Option<int> GGPOAnalogAxes("", 0);

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

} // namespace config
