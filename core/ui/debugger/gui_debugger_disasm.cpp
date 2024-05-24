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

#define DC_RAM_BASE 0x0c000000
#define BYTES_PER_INSTRUCTION 2
#define DISASM_LINE_LEN 128

#define IM_MAX(A, B)            (((A) >= (B)) ? (A) : (B))

extern ImFont *monospaceFont;

static u32 disasmAddress = DC_RAM_BASE;
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
	ImGui::Begin("Disassembly", NULL, ImGuiWindowFlags_NoCollapse);

	{
		DisabledScope scope(running);
		ImGui::Checkbox("Follow PC", &followPc);
	}

	ImGui::PushFont(monospaceFont);

	// Render disassembly table
	if (!ImGui::BeginTable("DisassemblyTable", 4, ImGuiTableFlags_SizingFixedFit))
		return;

	ImGuiTable *table = ImGui::GetCurrentTable();
	ImGui::TableSetupColumn("bp", ImGuiTableColumnFlags_WidthFixed, 9.0f);

	bool shouldResetDisasmAddress = false;
	for (rowIndex = 0; ; rowIndex++)
	{
		const u32 addr = (disasmAddress & 0x1fffffff) + rowIndex * 2;

		// If we are out of bounds, stop drawing
		if ((addr - DC_RAM_BASE) >= RAM_SIZE) {
			shouldResetDisasmAddress = true;
			break;
		}

		u16 instr = ReadMem16_nommu(addr);
		const DebugAgent::Breakpoint *breakpoint = nullptr;

		auto it = debugAgent.breakpoints[DebugAgent::Breakpoint::Type::BP_TYPE_SOFTWARE_BREAK].find(addr);
		const bool hasBreakpoint = it != debugAgent.breakpoints[DebugAgent::Breakpoint::Type::BP_TYPE_SOFTWARE_BREAK].end();

		if (hasBreakpoint) {
			breakpoint = &it->second;
			instr = breakpoint->savedOp;
		}

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
		ImU32 bpColor = 0;
		if (hasBreakpoint) {
			bpColor = IM_COL32(255, 0, 0, 255);
			if (!breakpoint->enabled)
				bpColor = IM_COL32(100, 100, 100, 255);
		} else if (isBreakpointCellHovered)
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
	bool isTableHovered = ImGui::IsItemHovered();

	// Draw scrollbar
	// TODO: Extract scrollbar drawing to a separate function
	size_t numRowsTotal = RAM_SIZE / BYTES_PER_INSTRUCTION;
	ImRect outerRect = table->OuterRect;
	ImRect innerRect = table->InnerRect;
	float borderSize = 0;
	float scrollbarWidth = ImGui::GetStyle().ScrollbarSize;
	ImRect bb = ImRect(
		IM_MAX(outerRect.Min.x, outerRect.Max.x - borderSize - scrollbarWidth),
		innerRect.Min.y,
		outerRect.Max.x,
		innerRect.Max.y
	);

	size_t rowCount = rowIndex + 1;
	ImS64 scrollPosition = (disasmAddress - DC_RAM_BASE) / 2;
	bool wheelScrolled = false;
	bool scrollbarScrolled = ImGui::ScrollbarEx(
		bb,
		ImGui::GetID("DisassemblyScrollBar"),
		ImGuiAxis_Y,
		&scrollPosition,
		rowCount,
		numRowsTotal,
		ImDrawFlags_RoundCornersNone
	);

	// Update disasm address based on scrollbar position
	if (scrollbarScrolled) {
		disasmAddress = DC_RAM_BASE + scrollPosition * 2;
	} else if (isTableHovered) {
		ImS64 wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0) {
			disasmAddress += wheel * -1;
			wheelScrolled = true;
		}
	}


	bool isPcOutsideDisasm = (pcAddr >= disasmAddress + rowCount * 2) || pcAddr < disasmAddress;
	// Stop following the PC if the user scrolled the PC out of view
	if (isPcOutsideDisasm && (wheelScrolled || scrollbarScrolled))
		followPc = false;
	// Follow PC when stepping
	if (!running && followPc && isPcOutsideDisasm)
		disasmAddress = pcAddr;
	// Ensure disasm address doesn't go out of bounds
	if (disasmAddress <= DC_RAM_BASE || shouldResetDisasmAddress)
		// FIXME: Scrolling past the end of the disassembly table resets PC to the beginning
		disasmAddress = DC_RAM_BASE;

	ImGui::PopFont();
	ImGui::End();
}
