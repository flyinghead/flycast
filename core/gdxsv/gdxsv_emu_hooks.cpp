#include "gdxsv_emu_hooks.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "rend/gui_util.h"
#include "hw/maple/maple_if.h"
#include "gdxsv.h"

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
    bool no_popup_opend = !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    if (gdxsv.UpdateAvailable() && no_popup_opend) {
        ImGui::OpenPopup("New version");
        gdxsv.DismissUpdateDialog();
    }
    if (ImGui::BeginPopupModal("New version", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
        ImGui::TextWrapped("  %s is available for download!  ", gdxsv.LatestVersion().c_str());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
        float currentwidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(
                (currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x - 55.f * scaling);
        if (ImGui::Button("Download", ImVec2(100.f * scaling, 0.f))) {
            gdxsv.OpenDownloadPage();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(
                (currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x + 55.f * scaling);
        if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f))) {
            gdxsv.DismissUpdateDialog();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
}

inline static void gui_header(const char *title)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
    ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
    ImGui::PopStyleVar();
}

void gdxsv_emu_settings() {
    gui_header("gdxsv Shortcut Settings");
    if (ImGui::Button("Apply Recommended Settings")) {
        // Video
        config::RendererType.set(RenderType::OpenGL);
        config::VSync = true;
        config::AutoSkipFrame = 2;
        config::SkipFrame = 0;
        config::RenderResolution = 960;
        config::DelayFrameSwapping = false;
        // Controls
        config::MapleMainDevices[0].set(MapleDeviceType::MDT_SegaController);
        config::MapleExpansionDevices[0][0].set(MDT_SegaVMU);
        // Audio
        config::AudioBufferSize = 706;
        // Others
        config::DynarecEnabled = true;
        config::DynarecIdleSkip = true;
        config::DynarecSafeMode = false;
        config::DSPEnabled = false;

        maple_ReconnectDevices();
    }
    ImGui::SameLine();
    ShowHelpMarker(R"(Use gdxsv recommended settings:
    Renderer=OpenGL
    VSync=yes
    AutoSkipFrame=2
    SkipFrame=0
    RenderResolution=960
    DelayFrameSwapping=no
    AudioLatency=16ms
    DynarecEnabled=yes
    DynarecIdleSkip=yes
    DynarecSafeMode=no
    DSPEnabled=no)");
    
    bool widescreen = config::Widescreen.get() && config::WidescreenGameHacks.get();
    bool pressed = ImGui::Checkbox("Enable 16:9 Widescreen Hack", &widescreen);
    if (pressed){
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
