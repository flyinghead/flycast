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

enum RenderAPI {
	OpenGL,
	Vulkan,
	DirectX9,
	DirectX11
};

void gui_settings_video()
{
	int renderApi;
	bool perPixel;
	switch (config::RendererType)
	{
	default:
	case RenderType::OpenGL:
		renderApi = OpenGL;
		perPixel = false;
		break;
	case RenderType::OpenGL_OIT:
		renderApi = OpenGL;
		perPixel = true;
		break;
	case RenderType::Vulkan:
		renderApi = Vulkan;
		perPixel = false;
		break;
	case RenderType::Vulkan_OIT:
		renderApi = Vulkan;
		perPixel = true;
		break;
	case RenderType::DirectX9:
		renderApi = DirectX9;
		perPixel = false;
		break;
	case RenderType::DirectX11:
		renderApi = DirectX11;
		perPixel = false;
		break;
	case RenderType::DirectX11_OIT:
		renderApi = DirectX11;
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
		header(T("Graphics API"));
		{
			ImGui::Columns(apiCount, "renderApi", false);
#ifdef USE_OPENGL
			ImGui::RadioButton("OpenGL", &renderApi, OpenGL);
			ImGui::NextColumn();
#endif
#ifdef USE_VULKAN
#ifdef __APPLE__
			ImGui::RadioButton("Vulkan (Metal)", &renderApi, Vulkan);
			ImGui::SameLine(0, innerSpacing);
			ShowHelpMarker(T("MoltenVK: An implementation of Vulkan that runs on Apple's Metal graphics framework"));
#else
			ImGui::RadioButton("Vulkan", &renderApi, Vulkan);
#endif // __APPLE__
			ImGui::NextColumn();
#endif
#ifdef USE_DX9
			{
				DisabledScope _(settings.platform.isNaomi2());
				ImGui::RadioButton("DirectX 9", &renderApi, DirectX9);
				ImGui::NextColumn();
			}
#endif
#ifdef USE_DX11
			ImGui::RadioButton("DirectX 11", &renderApi, DirectX11);
			ImGui::NextColumn();
#endif
			ImGui::Columns(1, nullptr, false);
    	}
    }
    header(T("Transparent Sorting"));
    {
		const bool has_per_pixel = GraphicsContext::Instance()->hasPerPixel();
    	int renderer = perPixel ? 2 : config::PerStripSorting ? 1 : 0;
    	ImGui::Columns(has_per_pixel ? 3 : 2, "renderers", false);
    	ImGui::RadioButton(T("Per Triangle"), &renderer, 0);
        ImGui::SameLine();
        ShowHelpMarker(T("Sort transparent polygons per triangle. Fast but may produce graphical glitches"));
    	ImGui::NextColumn();
    	ImGui::RadioButton(T("Per Strip"), &renderer, 1);
        ImGui::SameLine();
        ShowHelpMarker(T("Sort transparent polygons per strip. Faster but may produce graphical glitches"));
        if (has_per_pixel)
        {
        	ImGui::NextColumn();
        	ImGui::RadioButton(T("Per Pixel"), &renderer, 2);
        	ImGui::SameLine();
        	ShowHelpMarker(T("Sort transparent polygons per pixel. Slower but accurate"));
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
#ifdef __vita__
	OptionCheckbox(T("Fast Sorting"), config::FastSorting, T("Use a more unsafe but faster algorithm for transparency sorting"));
#endif
	ImGui::Spacing();

    header(T("Rendering Options"));
    {
        const std::array<float, 13> scalings{ 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 4.5f, 5.f, 6.f, 7.f, 8.f, 9.f };
        const std::array<std::string, 13> scalingsText{ T("Half"), T("Native"), "x1.5", "x2", "x2.5", "x3", "x4", "x4.5", "x5", "x6", "x7", "x8", "x9" };
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

        ImGui::Text("%s", T("Internal Resolution"));
        ImGui::SameLine();
        ShowHelpMarker(T("Internal render resolution. Higher is better, but more demanding on the GPU. Values higher than your display resolution (but no more than double your display resolution) can be used for supersampling, which provides high-quality antialiasing without reducing sharpness."));
		OptionCheckbox(T("Integer Scaling"), config::IntegerScale, T("Scales the output by the maximum integer multiple allowed by the display resolution."));
		OptionCheckbox(T("Linear Interpolation"), config::LinearInterpolation, T("Scales the output with linear interpolation. Will use nearest neighbor interpolation otherwise. Disable with integer scaling."));
#ifndef TARGET_IPHONE
    	OptionCheckbox(T("VSync"), config::VSync, T("Synchronizes the frame rate with the screen refresh rate. Recommended"));
    	if (isVulkan(config::RendererType))
    	{
	    	ImGui::Indent();
			{
				DisabledScope scope(!config::VSync);

				OptionCheckbox(T("Duplicate frames"), config::DupeFrames, T("Duplicate frames on high refresh rate monitors (120 Hz and higher)"));
	    	}
	    	ImGui::Unindent();
    	}
#endif
    	OptionCheckbox(T("Show VMU In-game"), config::FloatVMUs, T("Show the VMU LCD screens while in-game"));
    	OptionCheckbox(T("Full Framebuffer Emulation"), config::EmulateFramebuffer,
    			T("Fully accurate VRAM framebuffer emulation. Helps games that directly access the framebuffer for special effects. "
    			"Very slow and incompatible with upscaling and wide screen."));
		{
			DisabledScope scope(game_started);
			OptionCheckbox(T("Load Custom Textures"), config::CustomTextures,
					T("Load custom/high-res textures from data/textures/<game id>"));
			ImGui::Indent();
			{
				DisabledScope scope(!config::CustomTextures.get());
				OptionCheckbox(T("Preload Custom Textures"), config::PreloadCustomTextures,
						T("Preload custom textures at game start. May improve performance but increases memory usage"));
			}
			ImGui::Unindent();
		}
    }
	ImGui::Spacing();
    header(T("Aspect Ratio"));
    {
    	OptionCheckbox(T("Widescreen"), config::Widescreen,
    			T("Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas.\nAspect Fit and shows the full 16:9 content."));
		{
			DisabledScope scope(!config::Widescreen || config::IntegerScale);

			ImGui::Indent();
			OptionCheckbox(T("Super Widescreen"), config::SuperWidescreen,
					T("Use the full width of the screen or window when its aspect ratio is greater than 16:9.\nAspect Fill and remove black bars. Not compatible with integer scaling."));
			ImGui::Unindent();
    	}
    	OptionCheckbox(T("Widescreen Game Cheats"), config::WidescreenGameHacks,
    			T("Modify the game so that it displays in 16:9 anamorphic format and use horizontal screen stretching. Only some games are supported."));
    	OptionSlider(T("Horizontal Stretching"), config::ScreenStretching, 100, 250,
    			T("Stretch the screen horizontally"), "%d%%");
    	OptionCheckbox(T("Rotate Screen 90°"), config::Rotate90, T("Rotate the screen 90° counterclockwise"));
    }
	if (perPixel)
	{
		ImGui::Spacing();
		header(T("Per Pixel Settings"));

		const std::array<int64_t, 4> bufSizes{ 512_MB, 1_GB, 2_GB, 4_GB };
		const std::array<std::string, 4> bufSizesText{ T("512 MB"), T("1 GB"), T("2 GB"), T("4 GB") };
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

        ImGui::Text("%s", T("Pixel Buffer Size"));
        ImGui::SameLine();
        ShowHelpMarker(T("The size of the pixel buffer. May need to be increased when upscaling by a large factor."));

        OptionSlider(T("Maximum Layers"), config::PerPixelLayers, 8, 128,
        		T("Maximum number of transparent layers. May need to be increased for some complex scenes. Decreasing it may improve performance."));
	}
	ImGui::Spacing();
    header(T("Performance"));
    {
    	ImGui::Text("%s", T("Automatic Frame Skipping:"));
    	ImGui::Columns(3, "autoskip", false);
    	OptionRadioButton(T("Disabled"), config::AutoSkipFrame, 0, T("No frame skipping"));
    	ImGui::NextColumn();
    	OptionRadioButton(T("Normal"), config::AutoSkipFrame, 1, T("Skip a frame when the GPU and CPU are both running slow"));
    	ImGui::NextColumn();
    	OptionRadioButton(T("Maximum"), config::AutoSkipFrame, 2, T("Skip a frame when the GPU is running slow"));
    	ImGui::Columns(1, nullptr, false);

    	OptionArrowButtons(T("Frame Skipping"), config::SkipFrame, 0, 6,
    			T("Number of frames to skip between two actually rendered frames"));
    	OptionCheckbox(T("Shadows"), config::ModifierVolumes,
    			T("Enable modifier volumes, usually used for shadows"));
    	OptionCheckbox(T("Fog"), config::Fog, T("Enable fog effects"));
    }
    ImGui::Spacing();
	header(T("Advanced"));
    {
    	OptionCheckbox(T("Delay Frame Swapping"), config::DelayFrameSwapping,
    			T("Useful to avoid flashing screen or glitchy videos. Not recommended on slow platforms"));
    	OptionCheckbox(T("Fix Upscale Bleeding Edge"), config::FixUpscaleBleedingEdge,
    			T("Helps with texture bleeding case when upscaling. Disabling it can help if pixels are warping when upscaling in 2D games (MVC2, CVS, KOF, etc.)"));
    	OptionCheckbox(T("Native Depth Interpolation"), config::NativeDepthInterpolation,
    			T("Helps with texture corruption and depth issues on AMD GPUs. Can also help Intel GPUs in some cases."));
    	OptionCheckbox(T("Copy Rendered Textures to VRAM"), config::RenderToTextureBuffer,
    			T("Copy rendered-to textures back to VRAM. Slower but accurate"));
#ifndef __vita__
		const std::array<int, 5> aniso{ 1, 2, 4, 8, 16 };
        const std::array<std::string, 5> anisoText{ T("Disabled"), "2x", "4x", "8x", "16x" };
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

        ImGui::Text("%s", T("Anisotropic Filtering"));
        ImGui::SameLine();
        ShowHelpMarker(T("Higher values make textures viewed at oblique angles look sharper, but are more demanding on the GPU. This option only has a visible impact on mipmapped textures."));
#endif

    	ImGui::Text("%s", T("Texture Filtering:"));
    	ImGui::Columns(3, "textureFiltering", false);
    	OptionRadioButton(T("Default"), config::TextureFiltering, 0, T("Use the game's default texture filtering"));
    	ImGui::NextColumn();
    	OptionRadioButton(T("Force Nearest-Neighbor"), config::TextureFiltering, 1, T("Force nearest-neighbor filtering for all textures. Crisper appearance, but may cause various rendering issues. This option usually does not affect performance."));
    	ImGui::NextColumn();
    	OptionRadioButton(T("Force Linear"), config::TextureFiltering, 2, T("Force linear filtering for all textures. Smoother appearance, but may cause various rendering issues. This option usually does not affect performance."));
    	ImGui::Columns(1, nullptr, false);

#ifdef __vita__
		OptionCheckbox(T("Use Mipmaps"), config::UseMipmaps, T("Enables the generation and use of texture mipmaps"));
		OptionCheckbox(T("Use Simple Shaders"), config::UseSimpleShaders, T("Enables usage of simplified shaders"));
#endif

    	OptionCheckbox(T("Show FPS Counter"), config::ShowFPS, T("Show on-screen frame/sec counter"));
    }
#ifdef VIDEO_ROUTING
	ImGui::Spacing();
#ifdef __APPLE__
	header(T("Video Routing (Syphon)"));
#elif defined(_WIN32)
	if (renderApi == OpenGL || renderApi == DirectX11)
		header(T("Video Routing (Spout)"));
	else
		header(T("Video Routing (Only available with OpenGL or DirectX 11)"));
#endif
	{
#ifdef _WIN32
		DisabledScope scope(!(renderApi == OpenGL || renderApi == DirectX11));
#endif
		OptionCheckbox(T("Send video content to another program"), config::VideoRouting,
				T("e.g. Route GPU texture to OBS Studio directly instead of using CPU intensive Display/Window Capture"));

		{
			DisabledScope scope(!config::VideoRouting);
			OptionCheckbox(T("Scale down before sending"), config::VideoRoutingScale, T("Could increase performance when sharing a smaller texture, YMMV"));
			{
				DisabledScope scope(!config::VideoRoutingScale);
				static int vres = config::VideoRoutingVRes;
				if (ImGui::InputInt(T("Output vertical resolution"), &vres))
				{
					config::VideoRoutingVRes = vres;
				}
			}
			ImGui::Text(T("Output texture size: %d x %d"), config::VideoRoutingScale ? config::VideoRoutingVRes * settings.display.width / settings.display.height : settings.display.width, config::VideoRoutingScale ? config::VideoRoutingVRes : settings.display.height);
		}
	}
#endif

    switch (renderApi)
    {
    case OpenGL:
    	config::RendererType = perPixel ? RenderType::OpenGL_OIT : RenderType::OpenGL;
    	break;
    case Vulkan:
    	config::RendererType = perPixel ? RenderType::Vulkan_OIT : RenderType::Vulkan;
    	break;
    case DirectX9:
    	config::RendererType = RenderType::DirectX9;
    	break;
    case DirectX11:
    	config::RendererType = perPixel ? RenderType::DirectX11_OIT : RenderType::DirectX11;
    	break;
    }
}
