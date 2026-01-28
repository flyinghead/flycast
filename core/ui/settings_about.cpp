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
#include "version.h"
#include "wsi/context.h"
#include "oslib/storage.h"
#include "mainui.h"
#ifdef __ANDROID__
#if HOST_CPU == CPU_ARM64 && USE_VULKAN
#include "rend/vulkan/adreno.h"
#endif
#endif

#if defined(__ANDROID__) && HOST_CPU == CPU_ARM64 && USE_VULKAN
static bool driverDirty;

static void customDriverCallback(bool cancelled, std::string selection)
{
	if (!cancelled) {
		try {
			uploadCustomGpuDriver(selection);
			config::CustomGpuDriver = true;
			driverDirty = true;
		} catch (const FlycastException& e) {
			gui_error(e.what());
			config::CustomGpuDriver = false;
		}
	}
}
#endif

void gui_settings_about()
{
    header("Flycast");
    {
		ImGui::Text(T("Version: %s"), GIT_VERSION);
		ImGui::Text(T("Git Hash: %s"), GIT_HASH);
		ImGui::Text(T("Build Date: %s"), BUILD_DATE);
    }
	ImGui::Spacing();
    header(T("Platform"));
    {
    	ImGui::Text(T("CPU: %s"),
#if HOST_CPU == CPU_X86
			"x86"
#elif HOST_CPU == CPU_ARM
			"ARM"
#elif HOST_CPU == CPU_X64
			"x86/64"
#elif HOST_CPU == CPU_ARM64
			"ARM64"
#else
			T("Unknown")
#endif
				);
    	ImGui::Text(T("Operating System: %s"),
#ifdef __ANDROID__
			"Android"
#elif defined(__unix__)
			"Linux"
#elif defined(__APPLE__)
#ifdef TARGET_IPHONE
    		"iOS"
#else
			"macOS"
#endif
#elif defined(TARGET_UWP)
			T("Windows Universal Platform")
#elif defined(_WIN32)
			"Windows"
#elif defined(__SWITCH__)
			"Switch"
#elif defined(__vita__)
			"PSVita"
#else
			T("Unknown")
#endif
				);
#ifdef TARGET_IPHONE
		const char *getIosJitStatus();
		ImGui::Text(T("JIT Status: %s"), getIosJitStatus());
#endif
    }
	ImGui::Spacing();
	if (isOpenGL(config::RendererType))
		header("OpenGL");
	else if (isVulkan(config::RendererType))
		header("Vulkan");
	else if (isDirectX(config::RendererType))
		header("DirectX");
	ImGui::Text(T("Driver Name: %s"), GraphicsContext::Instance()->getDriverName().c_str());
	ImGui::Text(T("Version: %s"), GraphicsContext::Instance()->getDriverVersion().c_str());

#if defined(__ANDROID__) && HOST_CPU == CPU_ARM64 && USE_VULKAN
	if (isVulkan(config::RendererType))
	{
		const char *fileSelectTitle = T("Select a custom GPU driver");
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 10));
			if (config::CustomGpuDriver)
			{
				std::string name, description, vendor, version;
				if (getCustomGpuDriverInfo(name, description, vendor, version))
				{
					ImGui::Text("%s", T("Custom Driver:"));
					ImGui::Indent();
					ImGui::Text("%s - %s", name.c_str(), description.c_str());
					ImGui::Text("%s - %s", vendor.c_str(), version.c_str());
					ImGui::Unindent();
				}

				if (ImGui::Button(T("Use Default Driver"))) {
					config::CustomGpuDriver = false;
					ImGui::OpenPopup(T("Reset Vulkan"));
				}
			}
			else if (ImGui::Button(T("Upload Custom Driver"))) {
				if (!hostfs::addStorage(false, false, fileSelectTitle, customDriverCallback))
					ImGui::OpenPopup(fileSelectTitle);
			}

			if (driverDirty) {
				ImGui::OpenPopup(T("Reset Vulkan"));
				driverDirty = false;
			}

			ImguiStyleVar _1(ImGuiStyleVar_WindowPadding, ScaledVec2(20, 20));
			if (ImGui::BeginPopupModal(T("Reset Vulkan"), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
			{
				ImGui::Text("%s", T("Do you want to reset Vulkan to use new driver?"));
				ImGui::NewLine();
				ImguiStyleVar _(ImGuiStyleVar_ItemSpacing, ImVec2(uiScaled(20), ImGui::GetStyle().ItemSpacing.y));
				ImguiStyleVar _1(ImGuiStyleVar_FramePadding, ScaledVec2(10, 10));
				if (ImGui::Button(T("Yes")))
				{
					mainui_reinit();
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button(T("No")))
					ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
		}
		select_file_popup(fileSelectTitle, [](bool cancelled, std::string selection) {
				customDriverCallback(cancelled, selection);
				return true;
			}, true, "zip");
	}
#endif
}
