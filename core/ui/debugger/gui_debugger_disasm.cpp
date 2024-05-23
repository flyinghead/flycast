/*
	Copyright 2024 lhs_azevedo, vkedwardli

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
#include "gui_debugger_disasm.h"
#include "debug/debug_agent.h"
#include "ui/gui_util.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"
#include "imgui/imgui.h"
#include "sh4asm/sh4asm_core/disas.h"
#include "types.h"

#define DISASM_LINE_LEN 128

extern ImFont *monospaceFont;

static u32 disasmAddress = 0x0c000000;
static bool followPc = true;
static char sh4DisasmLine[DISASM_LINE_LEN];

static void disas_emit(char ch) {
	size_t len = strlen(sh4DisasmLine);
	if (len >= DISASM_LINE_LEN - 1)
		return; // no more space
	sh4DisasmLine[len] = ch;
}

void gui_debugger_disasm()
{
	u32 pc = *GetRegPtr(reg_nextpc);
	u32 pcAddr = pc & 0x1fffffff;
	size_t rowIndex = 0;
	bool running = emu.running();

	ImGui::SetNextWindowPos(ScaledVec2(16, 110), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(440, 600), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ScaledVec2(-1, 200), ScaledVec2(-1, FLT_MAX));

	//ImGui::Begin("Disassembly", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
		ImGui::Begin("Disassembly", NULL, ImGuiWindowFlags_NoCollapse);

	{
		DisabledScope scope(running);
		ImGui::Checkbox("Follow PC", &followPc);
	}

	ImGui::PushFont(monospaceFont);
	//ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 2));

	if (!ImGui::BeginTable("DisassemblyTable", 4, ImGuiTableFlags_SizingFixedFit))
		return;

	ImGuiTable *table = ImGui::GetCurrentTable();
	ImGui::TableSetupColumn("bp", ImGuiTableColumnFlags_WidthFixed, 9.0f);

	for (rowIndex = 0; ; rowIndex++)
	{
		const u32 addr = (disasmAddress & 0x1fffffff) + rowIndex * 2;

		u16 instr = ReadMem16_nommu(addr);

		auto it = debugAgent.breakpoints[DebugAgent::Breakpoint::Type::BP_TYPE_SOFTWARE_BREAK].find(addr);
		const bool hasBreakpoint = it != debugAgent.breakpoints[DebugAgent::Breakpoint::Type::BP_TYPE_SOFTWARE_BREAK].end();

		if (hasBreakpoint)
			instr = it->second.savedOp;

		ImVec2 mousePos = ImGui::GetMousePos();

		ImGui::TableNextRow(0);
		ImGui::TableNextColumn();

		// Deffer breakpoint drawing because we don't know the cell height yet.
		ImGui::TableNextColumn();

		char buf[64];
		memset(sh4DisasmLine, 0, sizeof(sh4DisasmLine));
		sh4asm_disas_inst(instr, disas_emit, addr);

		if (!running && addr == pcAddr)
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 128, 0, 255));

		sprintf(buf, "%08X:", (u32)addr);
		ImGui::Text("%s", buf);

		ImGui::TableNextColumn();
		ImGui::TextDisabled("%04X", instr);
		
		ImGui::TableNextColumn();
		ImGui::Text("%s", sh4DisasmLine);

		// Render breakpoint icon
		ImRect bpCellRect = ImGui::TableGetCellBgRect(table, 0);
		bool isBreakpointCellHovered = bpCellRect.Contains(mousePos);
		bool isBreakpointCellClicked = isBreakpointCellHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		// Assume breakpoint color
		ImU32 bpColor = 0;
		if (hasBreakpoint)
			bpColor = IM_COL32(255, 0, 0, 255);
		else if (isBreakpointCellHovered)
			bpColor = IM_COL32(127, 0, 0, 255);

		if (bpColor) {
			// Draw breakpoint in center of cell
			ImVec2 center = bpCellRect.GetCenter();
			ImVec2 bpPos = ImVec2(center.x, center.y);
			ImGui::GetForegroundDrawList()->AddCircleFilled(bpPos, 4, bpColor);
		}

		if (isBreakpointCellClicked) {
			if (hasBreakpoint)
				debugAgent.removeMatchpoint(DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK, addr, 2);
			else
				debugAgent.insertMatchpoint(DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK, addr, 2);
		}

		// If there is no more space, stop drawing
		if (ImGui::GetContentRegionAvail().y < bpCellRect.GetHeight() + 2.0f)
			break;
	}
	ImGui::EndTable();

	bool isPcOutsideDisasm = (pcAddr >= disasmAddress + rowIndex * 2) || pcAddr < disasmAddress;
	if (!running && followPc && isPcOutsideDisasm)
		disasmAddress = pcAddr;

	ImGui::PopFont();
	//ImGui::PopStyleVar();
	ImGui::End();
}
