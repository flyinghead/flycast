#include "gdxsv_emu_hooks.h"
#include "gdxsv.h"

#include <regex>
#include "version.h"
#include "oslib/oslib.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "rend/gui_util.h"
#include "hw/maple/maple_if.h"

void gdxsv_latest_version_check();
static bool gdxsv_update_available = false;
static std::string gdxsv_latest_version_tag;

void gdxsv_emu_start() {
    gdxsv.Reset();

    if (gdxsv.Enabled()) {
        gdxsv.StartPingTest();

        auto replay = cfgLoadStr("gdxsv", "replay", "");
        if (!replay.empty()) {
            dc_loadstate(99);
        }
    }
}

void gdxsv_emu_reset() {
    gdxsv.Reset();
}

void gdxsv_emu_update() {
    if (gdxsv.Enabled()) {
        gdxsv.Update();
    }
}

void gdxsv_emu_rpc() {
    if (gdxsv.Enabled()) {
        gdxsv.HandleRPC();
    }
}

void gdxsv_emu_savestate(int slot) {
    if (gdxsv.Enabled()) {
        gdxsv.RestoreOnlinePatch();
    }
}

void gdxsv_emu_loadstate(int slot) {
    if (gdxsv.Enabled()) {
        auto replay = cfgLoadStr("gdxsv", "replay", "");
        if (!replay.empty() && slot == 99) {
            gdxsv.StartReplayFile(replay.c_str());
        }
    }
}

bool gdxsv_emu_ingame() {
    return gdxsv.InGame();
}

void gdxsv_update_popup() {
    gdxsv_latest_version_check();
    bool no_popup_opened = !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    if (gdxsv_update_available && no_popup_opened) {
        ImGui::OpenPopup("New version");
        gdxsv_update_available = false;
    }
    if (ImGui::BeginPopupModal("New version", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
        ImGui::TextWrapped("  %s is available for download!  ", gdxsv_latest_version_tag.c_str());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
        float currentwidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(
                (currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x - 55.f * scaling);
        if (ImGui::Button("Download", ImVec2(100.f * scaling, 0.f))) {
            gdxsv_update_available = false;
            os_LaunchFromURL("https://github.com/inada-s/flycast/releases/latest/");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(
                (currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x + 55.f * scaling);
        if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f))) {
            gdxsv_update_available = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
}

inline static void gui_header(const char *title) {
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
    ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
    ImGui::PopStyleVar();
}

void gdxsv_emu_settings() {
    gui_header("gdxsv Shortcut Settings");
    if (ImGui::Button("Apply Recommended Settings")) {
        // Video
#if defined(_WIN32)
        config::RendererType.set(RenderType::DirectX11);
#else
        config::RendererType.set(RenderType::OpenGL);
#endif
        config::VSync = true;
        config::AutoSkipFrame = 2;
        config::SkipFrame = 0;
        config::RenderResolution = 960;
        config::DelayFrameSwapping = false;
        config::ThreadedRendering = true;
        // Controls
        config::MapleMainDevices[0].set(MapleDeviceType::MDT_SegaController);
        config::MapleExpansionDevices[0][0].set(MDT_SegaVMU);
        // Audio
        config::AudioBufferSize = 706;
        config::AudioVolume.set(50);
        config::AudioVolume.calcDbPower();
        // Others
        config::DynarecEnabled = true;
        config::DynarecIdleSkip = true;
        config::DSPEnabled = false;

        maple_ReconnectDevices();
    }
    ImGui::SameLine();
    ShowHelpMarker(R"(Use gdxsv recommended settings:
    Renderer=DirectX11(Windows) OpenGL(OtherOS)
    VSync=yes
    AutoSkipFrame=2
    SkipFrame=0
    RenderResolution=960
    DelayFrameSwapping=no
    ThreadedRendering=yes
    AudioLatency=16ms
    AudioVolume=50
    DynarecEnabled=yes
    DynarecIdleSkip=yes
    DSPEnabled=no)");

    bool widescreen = config::Widescreen.get() && config::WidescreenGameHacks.get();
    bool pressed = ImGui::Checkbox("Enable 16:9 Widescreen Hack", &widescreen);
    if (pressed) {
        config::Widescreen.set(widescreen);
        config::WidescreenGameHacks.set(widescreen);
    }
    ImGui::SameLine();
    ShowHelpMarker(R"(Use the following rendering options:
    rend.WideScreen=true
    rend.WidescreenGameHacks=true)");

    ImGui::NewLine();
    gui_header("Flycast Settings");
}

void gdxsv_handle_release_json(const std::string &json) {
    const std::string version_prefix = "gdxsv-";
    const std::regex tag_name_regex(R"|#|("tag_name":"(.*?)")|#|");
    const std::regex semver_regex(
            R"|#|(^([0-9]+)\.([0-9]+)\.([0-9]+)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+)?$)|#|");

    auto trim_prefix = [&version_prefix](const std::string &s) {
        if (s.size() < version_prefix.size())
            return s;
        if (version_prefix == s.substr(0, version_prefix.size()))
            return s.substr(version_prefix.size());
        return s;
    };

    std::smatch match;

    auto current_version_str = trim_prefix(std::string(GIT_VERSION));
    if (!std::regex_match(current_version_str, match, semver_regex)) return;

    if (match.size() < 4) return;
    auto current_version = std::tuple<int, int, int>(
            std::stoi(match.str(1)), std::stoi(match.str(2)), std::stoi(match.str(3)));

    if (!std::regex_search(json, match, tag_name_regex)) return;
    if (match.size() < 2) return;
    auto tag_name = match.str(1);
    auto latest_version_str = trim_prefix(tag_name);

    if (!std::regex_match(latest_version_str, match, semver_regex)) return;
    if (match.size() < 4) return;
    auto latest_version = std::tuple<int, int, int>(
            std::stoi(match.str(1)), std::stoi(match.str(2)), std::stoi(match.str(3)));

    gdxsv_latest_version_tag = tag_name;

    if (current_version < latest_version) {
        gdxsv_update_available = true;
    }
}

void gdxsv_latest_version_check() {
    static std::once_flag once;
    std::call_once(once, [] {
        std::thread([]() {
            const std::string json = os_FetchStringFromURL(
                    "https://api.github.com/repos/inada-s/flycast/releases/latest");
            if (json.empty()) return;
            gdxsv_handle_release_json(json);
        }).detach();
    });
}
