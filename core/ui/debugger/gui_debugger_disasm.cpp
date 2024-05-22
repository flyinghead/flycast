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

// TODO: Fix delay slot stepping

#define DISASM_LINE_LEN 128
#define DISASM_LEN 40

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

	ImGui::SetNextWindowPos(ScaledVec2(16, 110), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(440, 0), ImGuiCond_FirstUseEver);

	ImGui::Begin("Disassembly", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

	ImGui::Checkbox("Follow PC", &followPc);

	ImGui::PushFont(monospaceFont);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,2));

	u32 pcAddr = pc & 0x1fffffff;
	bool isPcOutsideDisasm = (pcAddr >= disasmAddress + DISASM_LEN * 2) || pcAddr < disasmAddress;
	if (followPc && isPcOutsideDisasm)
		disasmAddress = pcAddr;

	for (size_t i = 0; i < DISASM_LEN; i++)
	{
		const u32 addr = (disasmAddress & 0x1fffffff) + i * 2;

		u16 instr = ReadMem16_nommu(addr);

		auto it = debugAgent.breakpoints[DebugAgent::Breakpoint::Type::BP_TYPE_SOFTWARE_BREAK].find(addr);
		const bool isBreakpoint = it != debugAgent.breakpoints[DebugAgent::Breakpoint::Type::BP_TYPE_SOFTWARE_BREAK].end();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,2));
		if (isBreakpoint) {
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
			ImGui::Text("B ");
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
				debugAgent.removeMatchpoint(DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK, addr, 2);
			}

			ImGui::PopStyleColor();

			instr = it->second.savedOp;
		} else {
			ImGui::Text("  ");
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
				debugAgent.insertMatchpoint(DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK, addr, 2);
			}
		}
		ImGui::SameLine();
		ImGui::PopStyleVar();

		char buf [64];

		memset(sh4DisasmLine, 0, sizeof(sh4DisasmLine));
		sh4asm_disas_inst(instr, disas_emit, addr);
		if (addr == pcAddr) {
			// TODO: Handle scaling
			// TODO: Calculate rect size based on font size
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x - 2, p.y), ImVec2(p.x + 56, p.y + 16), IM_COL32(0, 128, 0, 255));
		}
		sprintf(buf, "%08X:", (u32) addr);
		ImGui::Text("%s", buf);
		ImGui::SameLine();
		ImGui::TextDisabled("%04X", instr);
		ImGui::SameLine();
		ImGui::Text("%s", sh4DisasmLine);
	}

	ImGui::PopFont();
	ImGui::PopStyleVar();
	ImGui::End();
}
