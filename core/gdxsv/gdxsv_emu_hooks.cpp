#include "gdxsv_emu_hooks.h"
#include "imgui/imgui.h"
#include "gdxsv.h"

void gdxsv_emu_start() {
    gdxsv.Reset();

    if (gdxsv.Enabled()) {
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
    if (gdxsv.UpdateAvailable() && no_popup_opend)
    {
        ImGui::OpenPopup("New version");
        gdxsv.DismissUpdateDialog();
    }
    if (ImGui::BeginPopupModal("New version", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
        ImGui::TextWrapped("  %s is available for download!  ", gdxsv.LatestVersion().c_str());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
        float currentwidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x - 55.f * scaling);
        if (ImGui::Button("Download", ImVec2(100.f * scaling, 0.f)))
        {
            gdxsv.OpenDownloadPage();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX((currentwidth - 100.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x + 55.f * scaling);
        if (ImGui::Button("Cancel", ImVec2(100.f * scaling, 0.f)))
        {
            gdxsv.DismissUpdateDialog();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
}