/*
	Copyright 2025 flyinghead

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
#include "settings.h"
#include "gui.h"
#include "wsi/context.h"

void gui_settings_video()
{
	int renderApi;
	bool perPixel;
	switch (config::RendererType)
	{
	default:
	case RenderType::OpenGL:
		renderApi = 0;
		perPixel = false;
		break;
	case RenderType::OpenGL_OIT:
		renderApi = 0;
		perPixel = true;
		break;
	case RenderType::Vulkan:
		renderApi = 1;
		perPixel = false;
		break;
	case RenderType::Vulkan_OIT:
		renderApi = 1;
		perPixel = true;
		break;
	case RenderType::DirectX9:
		renderApi = 2;
		perPixel = false;
		break;
	case RenderType::DirectX11:
		renderApi = 3;
		perPixel = false;
		break;
	case RenderType::DirectX11_OIT:
		renderApi = 3;
		perPixel = true;
		break;
	}

	constexpr int apiCount = 0
		#ifdef USE_VULKAN
			+ 1
		#endif
		#ifdef USE_DX9
			+ 1
		#endif
		#ifdef USE_OPENGL
			+ 1
		#endif
		#ifdef USE_DX11
			+ 1
		#endif
			;

    float innerSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
	if (apiCount > 1)
	{
		header("Graphics API");
		{
			ImGui::Columns(apiCount, "renderApi", false);
#ifdef USE_OPENGL
			ImGui::RadioButton("OpenGL", &renderApi, 0);
			ImGui::NextColumn();
#endif
#ifdef USE_VULKAN
#ifdef __APPLE__
			ImGui::RadioButton("Vulkan (Metal)", &renderApi, 1);
			ImGui::SameLine(0, innerSpacing);
			ShowHelpMarker("MoltenVK: An implementation of Vulkan that runs on Apple's Metal graphics framework");
#else
			ImGui::RadioButton("Vulkan", &renderApi, 1);
#endif // __APPLE__
			ImGui::NextColumn();
#endif
#ifdef USE_DX9
			{
				DisabledScope _(settings.platform.isNaomi2());
				ImGui::RadioButton("DirectX 9", &renderApi, 2);
				ImGui::NextColumn();
			}
#endif
#ifdef USE_DX11
			ImGui::RadioButton("DirectX 11", &renderApi, 3);
			ImGui::NextColumn();
#endif
			ImGui::Columns(1, nullptr, false);
    	}
    }
    header("Transparent Sorting");
    {
		const bool has_per_pixel = GraphicsContext::Instance()->hasPerPixel();
    	int renderer = perPixel ? 2 : config::PerStripSorting ? 1 : 0;
    	ImGui::Columns(has_per_pixel ? 3 : 2, "renderers", false);
    	ImGui::RadioButton("Per Triangle", &renderer, 0);
        ImGui::SameLine();
        ShowHelpMarker("Sort transparent polygons per triangle. Fast but may produce graphical glitches");
    	ImGui::NextColumn();
    	ImGui::RadioButton("Per Strip", &renderer, 1);
        ImGui::SameLine();
        ShowHelpMarker("Sort transparent polygons per strip. Faster but may produce graphical glitches");
        if (has_per_pixel)
        {
        	ImGui::NextColumn();
        	ImGui::RadioButton("Per Pixel", &renderer, 2);
        	ImGui::SameLine();
        	ShowHelpMarker("Sort transparent polygons per pixel. Slower but accurate");
        }
    	ImGui::Columns(1, NULL, false);
    	switch (renderer)
    	{
    	case 0:
    		perPixel = false;
    		config::PerStripSorting.set(false);
    		break;
    	case 1:
    		perPixel = false;
    		config::PerStripSorting.set(true);
    		break;
    	case 2:
    		perPixel = true;
    		break;
    	}
    }
	ImGui::Spacing();

    header("Rendering Options");
    {
        const std::array<float, 13> scalings{ 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 4.5f, 5.f, 6.f, 7.f, 8.f, 9.f };
        const std::array<std::string, 13> scalingsText{ "Half", "Native", "x1.5", "x2", "x2.5", "x3", "x4", "x4.5", "x5", "x6", "x7", "x8", "x9" };
        std::array<int, scalings.size()> vres;
        std::array<std::string, scalings.size()> resLabels;
        u32 selected = 0;
        for (u32 i = 0; i < scalings.size(); i++)
        {
        	vres[i] = scalings[i] * 480;
        	if (vres[i] == config::RenderResolution)
        		selected = i;
        	if (!config::Widescreen)
        		resLabels[i] = std::to_string((int)(scalings[i] * 640)) + "x" + std::to_string((int)(scalings[i] * 480));
        	else
        		resLabels[i] = std::to_string((int)(scalings[i] * 480 * 16 / 9)) + "x" + std::to_string((int)(scalings[i] * 480));
        	resLabels[i] += " (" + scalingsText[i] + ")";
        }

        ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
        if (ImGui::BeginCombo("##Resolution", resLabels[selected].c_str(), ImGuiComboFlags_NoArrowButton))
        {
        	for (u32 i = 0; i < scalings.size(); i++)
            {
                bool is_selected = vres[i] == config::RenderResolution;
                if (ImGui::Selectable(resLabels[i].c_str(), is_selected))
                	config::RenderResolution = vres[i];
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0, innerSpacing);

        if (ImGui::ArrowButton("##Decrease Res", ImGuiDir_Left))
        {
            if (selected > 0)
            	config::RenderResolution = vres[selected - 1];
        }
        ImGui::SameLine(0, innerSpacing);
        if (ImGui::ArrowButton("##Increase Res", ImGuiDir_Right))
        {
            if (selected < vres.size() - 1)
            	config::RenderResolution = vres[selected + 1];
        }
        ImGui::SameLine(0, innerSpacing);

        ImGui::Text("Internal Resolution");
        ImGui::SameLine();
        ShowHelpMarker("Internal render resolution. Higher is better, but more demanding on the GPU. Values higher than your display resolution (but no more than double your display resolution) can be used for supersampling, which provides high-quality antialiasing without reducing sharpness.");
		OptionCheckbox("Integer Scaling", config::IntegerScale, "Scales the output by the maximum integer multiple allowed by the display resolution.");
		OptionCheckbox("Linear Interpolation", config::LinearInterpolation, "Scales the output with linear interpolation. Will use nearest neighbor interpolation otherwise. Disable with integer scaling.");
#ifndef TARGET_IPHONE
    	OptionCheckbox("VSync", config::VSync, "Synchronizes the frame rate with the screen refresh rate. Recommended");
    	if (isVulkan(config::RendererType))
    	{
	    	ImGui::Indent();
			{
				DisabledScope scope(!config::VSync);

				OptionCheckbox("Duplicate frames", config::DupeFrames, "Duplicate frames on high refresh rate monitors (120 Hz and higher)");
	    	}
	    	ImGui::Unindent();
    	}
#endif
    	OptionCheckbox("Show VMU In-game", config::FloatVMUs, "Show the VMU LCD screens while in-game");
    	OptionCheckbox("Full Framebuffer Emulation", config::EmulateFramebuffer,
    			"Fully accurate VRAM framebuffer emulation. Helps games that directly access the framebuffer for special effects. "
    			"Very slow and incompatible with upscaling and wide screen.");
		{
			DisabledScope scope(game_started);
			OptionCheckbox("Load Custom Textures", config::CustomTextures,
						   "Load custom/high-res textures from data/textures/<game id>");
			ImGui::Indent();
			{
				DisabledScope scope(!config::CustomTextures.get());
				OptionCheckbox("Preload Custom Textures", config::PreloadCustomTextures,
							   "Preload custom textures at game start. May improve performance but increases memory usage");
			}
			ImGui::Unindent();
		}
    }
	ImGui::Spacing();
    header("Aspect Ratio");
    {
    	OptionCheckbox("Widescreen", config::Widescreen,
    			"Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas.\nAspect Fit and shows the full 16:9 content.");
		{
			DisabledScope scope(!config::Widescreen || config::IntegerScale);

			ImGui::Indent();
			OptionCheckbox("Super Widescreen", config::SuperWidescreen,
					"Use the full width of the screen or window when its aspect ratio is greater than 16:9.\nAspect Fill and remove black bars. Not compatible with integer scaling.");
			ImGui::Unindent();
    	}
    	OptionCheckbox("Widescreen Game Cheats", config::WidescreenGameHacks,
    			"Modify the game so that it displays in 16:9 anamorphic format and use horizontal screen stretching. Only some games are supported.");
    	OptionSlider("Horizontal Stretching", config::ScreenStretching, 100, 250,
    			"Stretch the screen horizontally", "%d%%");
    	OptionCheckbox("Rotate Screen 90°", config::Rotate90, "Rotate the screen 90° counterclockwise");
    }
	if (perPixel)
	{
		ImGui::Spacing();
		header("Per Pixel Settings");

		const std::array<int64_t, 4> bufSizes{ 512_MB, 1_GB, 2_GB, 4_GB };
		const std::array<std::string, 4> bufSizesText{ "512 MB", "1 GB", "2 GB", "4 GB" };
        ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
		u32 selected = 0;
		for (; selected < bufSizes.size(); selected++)
			if (bufSizes[selected] == config::PixelBufferSize)
				break;
		if (selected == bufSizes.size())
			selected = 0;
		if (ImGui::BeginCombo("##PixelBuffer", bufSizesText[selected].c_str(), ImGuiComboFlags_NoArrowButton))
		{
			for (u32 i = 0; i < bufSizes.size(); i++)
			{
				bool is_selected = i == selected;
				if (ImGui::Selectable(bufSizesText[i].c_str(), is_selected))
					config::PixelBufferSize = bufSizes[i];
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
					selected = i;
				}
			}
			ImGui::EndCombo();
		}
        ImGui::PopItemWidth();
		ImGui::SameLine(0, innerSpacing);

		if (ImGui::ArrowButton("##Decrease BufSize", ImGuiDir_Left))
		{
			if (selected > 0)
				config::PixelBufferSize = bufSizes[selected - 1];
		}
		ImGui::SameLine(0, innerSpacing);
		if (ImGui::ArrowButton("##Increase BufSize", ImGuiDir_Right))
		{
			if (selected < bufSizes.size() - 1)
				config::PixelBufferSize = bufSizes[selected + 1];
		}
		ImGui::SameLine(0, innerSpacing);

        ImGui::Text("Pixel Buffer Size");
        ImGui::SameLine();
        ShowHelpMarker("The size of the pixel buffer. May need to be increased when upscaling by a large factor.");

        OptionSlider("Maximum Layers", config::PerPixelLayers, 8, 128,
        		"Maximum number of transparent layers. May need to be increased for some complex scenes. Decreasing it may improve performance.");
	}
	ImGui::Spacing();
    header("Performance");
    {
    	ImGui::Text("Automatic Frame Skipping:");
    	ImGui::Columns(3, "autoskip", false);
    	OptionRadioButton("Disabled", config::AutoSkipFrame, 0, "No frame skipping");
    	ImGui::NextColumn();
    	OptionRadioButton("Normal", config::AutoSkipFrame, 1, "Skip a frame when the GPU and CPU are both running slow");
    	ImGui::NextColumn();
    	OptionRadioButton("Maximum", config::AutoSkipFrame, 2, "Skip a frame when the GPU is running slow");
    	ImGui::Columns(1, nullptr, false);

    	OptionArrowButtons("Frame Skipping", config::SkipFrame, 0, 6,
    			"Number of frames to skip between two actually rendered frames");
    	OptionCheckbox("Shadows", config::ModifierVolumes,
    			"Enable modifier volumes, usually used for shadows");
    	OptionCheckbox("Fog", config::Fog, "Enable fog effects");
    }
    ImGui::Spacing();
	header("Advanced");
    {
    	OptionCheckbox("Delay Frame Swapping", config::DelayFrameSwapping,
    			"Useful to avoid flashing screen or glitchy videos. Not recommended on slow platforms");
    	OptionCheckbox("Fix Upscale Bleeding Edge", config::FixUpscaleBleedingEdge,
    			"Helps with texture bleeding case when upscaling. Disabling it can help if pixels are warping when upscaling in 2D games (MVC2, CVS, KOF, etc.)");
    	OptionCheckbox("Native Depth Interpolation", config::NativeDepthInterpolation,
    			"Helps with texture corruption and depth issues on AMD GPUs. Can also help Intel GPUs in some cases.");
    	OptionCheckbox("Copy Rendered Textures to VRAM", config::RenderToTextureBuffer,
    			"Copy rendered-to textures back to VRAM. Slower but accurate");
		const std::array<int, 5> aniso{ 1, 2, 4, 8, 16 };
        const std::array<std::string, 5> anisoText{ "Disabled", "2x", "4x", "8x", "16x" };
        u32 afSelected = 0;
        for (u32 i = 0; i < aniso.size(); i++)
        {
        	if (aniso[i] == config::AnisotropicFiltering)
        		afSelected = i;
        }

        ImGui::PushItemWidth(ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f);
        if (ImGui::BeginCombo("##Anisotropic Filtering", anisoText[afSelected].c_str(), ImGuiComboFlags_NoArrowButton))
        {
        	for (u32 i = 0; i < aniso.size(); i++)
            {
                bool is_selected = aniso[i] == config::AnisotropicFiltering;
                if (ImGui::Selectable(anisoText[i].c_str(), is_selected))
                	config::AnisotropicFiltering = aniso[i];
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0, innerSpacing);

        if (ImGui::ArrowButton("##Decrease Anisotropic Filtering", ImGuiDir_Left))
        {
            if (afSelected > 0)
            	config::AnisotropicFiltering = aniso[afSelected - 1];
        }
        ImGui::SameLine(0, innerSpacing);
        if (ImGui::ArrowButton("##Increase Anisotropic Filtering", ImGuiDir_Right))
        {
            if (afSelected < aniso.size() - 1)
            	config::AnisotropicFiltering = aniso[afSelected + 1];
        }
        ImGui::SameLine(0, innerSpacing);

        ImGui::Text("Anisotropic Filtering");
        ImGui::SameLine();
        ShowHelpMarker("Higher values make textures viewed at oblique angles look sharper, but are more demanding on the GPU. This option only has a visible impact on mipmapped textures.");

    	ImGui::Text("Texture Filtering:");
    	ImGui::Columns(3, "textureFiltering", false);
    	OptionRadioButton("Default", config::TextureFiltering, 0, "Use the game's default texture filtering");
    	ImGui::NextColumn();
    	OptionRadioButton("Force Nearest-Neighbor", config::TextureFiltering, 1, "Force nearest-neighbor filtering for all textures. Crisper appearance, but may cause various rendering issues. This option usually does not affect performance.");
    	ImGui::NextColumn();
    	OptionRadioButton("Force Linear", config::TextureFiltering, 2, "Force linear filtering for all textures. Smoother appearance, but may cause various rendering issues. This option usually does not affect performance.");
    	ImGui::Columns(1, nullptr, false);

    	OptionCheckbox("Show FPS Counter", config::ShowFPS, "Show on-screen frame/sec counter");
    }
#ifdef VIDEO_ROUTING
	ImGui::Spacing();
#ifdef __APPLE__
	header("Video Routing (Syphon)");
#elif defined(_WIN32)
	((renderApi == 0) || (renderApi == 3)) ? header("Video Routing (Spout)") : header("Video Routing (Only available with OpenGL or DirectX 11)");
#endif
	{
#ifdef _WIN32
		DisabledScope scope(!((renderApi == 0) || (renderApi == 3)));
#endif
		OptionCheckbox("Send video content to another program", config::VideoRouting,
			"e.g. Route GPU texture to OBS Studio directly instead of using CPU intensive Display/Window Capture");

		{
			DisabledScope scope(!config::VideoRouting);
			OptionCheckbox("Scale down before sending", config::VideoRoutingScale, "Could increase performance when sharing a smaller texture, YMMV");
			{
				DisabledScope scope(!config::VideoRoutingScale);
				static int vres = config::VideoRoutingVRes;
				if (ImGui::InputInt("Output vertical resolution", &vres))
				{
					config::VideoRoutingVRes = vres;
				}
			}
			ImGui::Text("Output texture size: %d x %d", config::VideoRoutingScale ? config::VideoRoutingVRes * settings.display.width / settings.display.height : settings.display.width, config::VideoRoutingScale ? config::VideoRoutingVRes : settings.display.height);
		}
	}
#endif

    switch (renderApi)
    {
    case 0:
    	config::RendererType = perPixel ? RenderType::OpenGL_OIT : RenderType::OpenGL;
    	break;
    case 1:
    	config::RendererType = perPixel ? RenderType::Vulkan_OIT : RenderType::Vulkan;
    	break;
    case 2:
    	config::RendererType = RenderType::DirectX9;
    	break;
    case 3:
    	config::RendererType = perPixel ? RenderType::DirectX11_OIT : RenderType::DirectX11;
    	break;
    }
}
