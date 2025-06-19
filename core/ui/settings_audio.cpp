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
#include "audio/audiostream.h"

void gui_settings_audio()
{
	OptionCheckbox("Enable DSP", config::DSPEnabled,
			"Enable the Dreamcast Digital Sound Processor. Only recommended on fast platforms");
    OptionCheckbox("Enable VMU Sounds", config::VmuSound, "Play VMU beeps when enabled.");

	if (OptionSlider("Volume Level", config::AudioVolume, 0, 100, "Adjust the emulator's audio level", "%d%%"))
	{
		config::AudioVolume.calcDbPower();
	};
#ifdef __ANDROID__
	if (config::AudioBackend.get() == "auto" || config::AudioBackend.get() == "android")
		OptionCheckbox("Automatic Latency", config::AutoLatency,
				"Automatically set audio latency. Recommended");
#endif
    if (!config::AutoLatency
    		|| (config::AudioBackend.get() != "auto" && config::AudioBackend.get() != "android"))
    {
		int latency = (int)roundf(config::AudioBufferSize * 1000.f / 44100.f);
		ImGui::SliderInt("Latency", &latency, 12, 512, "%d ms");
		config::AudioBufferSize = (int)roundf(latency * 44100.f / 1000.f);
		ImGui::SameLine();
		ShowHelpMarker("Sets the maximum audio latency. Not supported by all audio drivers.");
    }

	AudioBackend *backend = nullptr;
	std::string backend_name = config::AudioBackend;
	if (backend_name != "auto")
	{
		backend = AudioBackend::getBackend(config::AudioBackend);
		if (backend != nullptr)
			backend_name = backend->slug;
	}

	AudioBackend *current_backend = backend;
	if (ImGui::BeginCombo("Audio Driver", backend_name.c_str(), ImGuiComboFlags_None))
	{
		bool is_selected = (config::AudioBackend.get() == "auto");
		if (ImGui::Selectable("auto - Automatic driver selection", &is_selected))
			config::AudioBackend.set("auto");

		for (u32 i = 0; i < AudioBackend::getCount(); i++)
		{
			backend = AudioBackend::getBackend(i);
			is_selected = (config::AudioBackend.get() == backend->slug);

			if (is_selected)
				current_backend = backend;

			if (ImGui::Selectable((backend->slug + " - " + backend->name).c_str(), &is_selected))
				config::AudioBackend.set(backend->slug);
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ShowHelpMarker("The audio driver to use");

	if (current_backend != nullptr)
	{
		// get backend specific options
		int option_count;
		const AudioBackend::Option *options = current_backend->getOptions(&option_count);

		for (int o = 0; o < option_count; o++)
		{
			std::string value = cfgLoadStr(current_backend->slug, options->name, "");

			if (options->type == AudioBackend::Option::integer)
			{
				int val = stoi(value);
				if (ImGui::SliderInt(options->caption.c_str(), &val, options->minValue, options->maxValue))
				{
					std::string s = std::to_string(val);
					cfgSaveStr(current_backend->slug, options->name, s);
				}
			}
			else if (options->type == AudioBackend::Option::checkbox)
			{
				bool check = value == "1";
				if (ImGui::Checkbox(options->caption.c_str(), &check))
					cfgSaveStr(current_backend->slug, options->name,
							check ? "1" : "0");
			}
			else if (options->type == AudioBackend::Option::list)
			{
				if (ImGui::BeginCombo(options->caption.c_str(), value.c_str(), ImGuiComboFlags_None))
				{
					bool is_selected = false;
					for (const auto& cur : options->values)
					{
						is_selected = value == cur;
						if (ImGui::Selectable(cur.c_str(), &is_selected))
							cfgSaveStr(current_backend->slug, options->name, cur);

						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
			else {
				WARN_LOG(RENDERER, "Unknown option");
			}

			options++;
		}
	}
}
