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
#include "types.h"
#include "capstone/capstone.h"
#include <unordered_set>

#define DC_RAM_BASE 0x0c000000
#define BYTES_PER_INSTRUCTION 2
#define DISASM_LINE_LEN 128

#define IM_MAX(A, B)            (((A) >= (B)) ? (A) : (B))

extern ImFont *monospaceFont;

static u32 disasmAddress = 0x0c010000;
static u32 lastPc = 0;
static bool followPc = true;
static std::unordered_set<unsigned int> branchIntructions = {
	SH_INS_BF_S,
	SH_INS_BF,
	SH_INS_BRA,
	SH_INS_BRAF,
	SH_INS_BSR,
	SH_INS_BSRF,
	SH_INS_BT_S,
	SH_INS_BT,
	SH_INS_JMP,
	SH_INS_JSR,
	SH_INS_RTS,
};
static std::unordered_set<unsigned int> logicalIntructions = {
	SH_INS_AND,
	SH_INS_BAND,
	SH_INS_NOT,
	SH_INS_OR,
	SH_INS_BOR,
	SH_INS_TAS,
	SH_INS_TST,
	SH_INS_XOR,
	SH_INS_BXOR,
};
static std::unordered_set<unsigned int> arithmeticIntructions = {
	SH_INS_ADD_r,
	SH_INS_ADD,
	SH_INS_ADDC,
	SH_INS_ADDV,
	SH_INS_CMP_EQ,
	SH_INS_CMP_HS,
	SH_INS_CMP_GE,
	SH_INS_CMP_HI,
	SH_INS_CMP_GT,
	SH_INS_CMP_PZ,
	SH_INS_CMP_PL,
	SH_INS_CMP_STR,
	SH_INS_DIV1,
	SH_INS_DIV0S,
	SH_INS_DIV0U,
	SH_INS_DMULS_L,
	SH_INS_DMULU_L,
	SH_INS_DT,
	SH_INS_EXTS_B,
	SH_INS_EXTS_W,
	SH_INS_EXTU_B,
	SH_INS_EXTU_W,
	SH_INS_MAC_L,
	SH_INS_MAC_W,
	SH_INS_MUL_L,
	SH_INS_MULS_W,
	SH_INS_MULU_W,
	SH_INS_NEG,
	SH_INS_NEGC,
	SH_INS_SUB,
	SH_INS_SUBC,
	SH_INS_SUBV,
};

static void drawMnemonic(cs_insn *instruction);
static void goTo(u32 address);

/**
 * Custom wrapper for cs_disasm_iter that does not modify arguments.
 */
static bool disasmInstruction(csh handle, u16 code, uint64_t address, cs_insn *insn)
{
	u16 *codePtr = &code;
	size_t instructionSize = 2;
	return cs_disasm_iter(handle, (const u8 **) &codePtr, &instructionSize, &address, insn);
}

// TODO: Extract into smaller functions
void gui_debugger_disasm()
{
	u32 pc = *GetRegPtr(reg_nextpc);
	bool pcChanged = pc != lastPc;
	u32 pcAddr = pc & 0x1fffffff;
	size_t rowIdx = 0;
	bool running = emu.running();
	ImGuiStyle &style = ImGui::GetStyle();

	ImGui::SetNextWindowPos(ScaledVec2(16, 110), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(440, 602), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ScaledVec2(-1, 200), ScaledVec2(-1, FLT_MAX));
	ImGui::Begin("Disassembly", NULL, ImGuiWindowFlags_NoCollapse);

	// Draw upper bar
	{
		// Use dense layout
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));

		// Go to address
		// TODO: Extract InputAddress function
		static u32 gotoAddress = disasmAddress;
		ImGui::PushItemWidth(ImGui::CalcTextSize("00000000").x + style.FramePadding.x * 2);
		ImGui::InputScalar("##gotoAddress", ImGuiDataType_U32, &gotoAddress, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_NoHorizontalScroll);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Go"))
			goTo(gotoAddress);

		// Follow PC
		{
			DisabledScope scope(running);
			ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 2);
			ImGui::Checkbox("Follow PC", &followPc);
		}

		ImGui::PopStyleVar();
	}

	// Draw disassembly table
	ImGui::Separator();

	ImGui::PushFont(monospaceFont);

	float footerHeight = ImGui::GetTextLineHeight() + style.ItemSpacing.y * 3;
	float avaliableHeight = ImGui::GetContentRegionAvail().y - footerHeight;
	float rowHeight = ImGui::GetTextLineHeight() + style.CellPadding.y * 2;
	unsigned int rowCount = std::ceil(avaliableHeight / rowHeight);
	unsigned int unclipedRowCount = std::floor(avaliableHeight / rowHeight);

	if (!ImGui::BeginTable("DisassemblyTable", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendY, ImVec2(0, -footerHeight)))
		return;

	// TODO: Capstone doesn't need to be initialized every frame
	csh capstoneHandle;
	if (cs_open(CS_ARCH_SH, (cs_mode) (CS_MODE_LITTLE_ENDIAN | CS_MODE_SH4 | CS_MODE_SHFPU), &capstoneHandle) != CS_ERR_OK) {
		ERROR_LOG(COMMON, "Failed to open Capstone: %s", cs_strerror(cs_errno(capstoneHandle)));
		return;
	}
	cs_option(capstoneHandle, CS_OPT_DETAIL, CS_OPT_ON);
	cs_insn *insn = cs_malloc(capstoneHandle);

	ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(0, 0, .9));

	ImGuiTable *table = ImGui::GetCurrentTable();
	ImGui::TableSetupColumn("bp", ImGuiTableColumnFlags_WidthFixed, uiScaled(9.0f));

	bool shouldResetDisasmAddress = false;
	for (rowIdx = 0; rowIdx < rowCount; rowIdx++)
	{
		const u32 addr = (disasmAddress & 0x1fffffff) + rowIdx * 2;

		// If we are out of bounds, stop drawing
		if ((addr - DC_RAM_BASE) >= RAM_SIZE) {
			shouldResetDisasmAddress = true;
			break;
		}

		u16 instr = ReadMem16_nommu(addr);

		ImVec2 mousePos = ImGui::GetMousePos();
		bool shouldHighlightRow = !running && addr == pcAddr;

		ImGui::TableNextRow(0, rowHeight);
		ImGui::TableNextColumn();

		// Render breakpoint icon
		DebugAgent::Breakpoint *breakpoint = debugAgent.findSoftwareBreakpoint(addr);
		if (breakpoint) {
			instr = breakpoint->savedOp;
		}
		const bool shouldDrawBreakpoint = breakpoint && !breakpoint->singleShot;

		ImRect bpCellRect = ImGui::TableGetCellBgRect(table, 0);
		bool isBreakpointCellHovered = bpCellRect.Contains(mousePos);
		bool isBreakpointCellClicked = isBreakpointCellHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		ImU32 bpColor = 0;
		if (shouldDrawBreakpoint) {
			bpColor = IM_COL32(255, 0, 0, 255);
			if (!breakpoint->enabled)
				bpColor = IM_COL32(100, 100, 100, 255);
		} else if (isBreakpointCellHovered)
			bpColor = IM_COL32(127, 0, 0, 255);

		if (bpColor) {
			// Draw breakpoint in center of cell
			ImVec2 center = bpCellRect.GetCenter();
			ImVec2 bpPos = ImVec2(center.x, center.y + uiScaled(1));
			ImGui::GetForegroundDrawList()->AddCircleFilled(bpPos, uiScaled(4), bpColor);
		}

		if (isBreakpointCellClicked) {
			if (shouldDrawBreakpoint)
				debugAgent.removeBreakpoint(addr);
			else
				debugAgent.insertSoftwareBreakpoint(addr);
		}

		if (shouldHighlightRow)
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
		ImGui::TableNextColumn();

		if (shouldHighlightRow) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0x92, 0xee, 0x61, 255));
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_WindowBg));
		}
		ImGui::Text("%08X", (u32) addr);
		if (shouldHighlightRow)
			ImGui::PopStyleColor();

		ImGui::TableNextColumn();
		ImGui::TextDisabled("%04X", instr);

		ImGui::TableNextColumn();

		if (!disasmInstruction(capstoneHandle, instr, addr, insn)) {
			ImGui::TextDisabled("Invalid instruction");
		} else {
			drawMnemonic(insn);
			ImGui::SameLine();
			ImGui::Text("%s", insn->op_str);
		}
	}

	ImGui::PopStyleColor();
	ImGui::EndTable();
	ImGui::PopFont();
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

	ImS64 scrollPosition = (disasmAddress - DC_RAM_BASE) / 2;
	bool scrollbarScrolled = ImGui::ScrollbarEx(
		bb,
		ImGui::GetID("DisassemblyScrollBar"),
		ImGuiAxis_Y,
		&scrollPosition,
		unclipedRowCount,
		numRowsTotal,
		ImDrawFlags_RoundCornersNone
	);

	// Update disasm address based on scrollbar position
	if (scrollbarScrolled) {
		disasmAddress = DC_RAM_BASE + scrollPosition * 2;
	} else if (isTableHovered) {
		ImS64 wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0)
			disasmAddress += wheel * -1;
	}

	ImGui::Separator();

	// Use dense layout
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 0));

	// TODO: Use key bindings instead
	if (ImGui::Button("Page Up"))
		disasmAddress -= unclipedRowCount * 2;
	ImGui::SameLine();
	if (ImGui::Button("Page Down"))
		disasmAddress += unclipedRowCount * 2;

	ImGui::PopStyleVar();

	bool isPcOutsideDisasm = (pcAddr >= disasmAddress + unclipedRowCount * 2) || pcAddr < disasmAddress;

	// Follow PC when stepping
	if (!running && followPc && pcChanged && isPcOutsideDisasm)
		disasmAddress = pcAddr;
	// Ensure disasm address doesn't go out of bounds
	if (disasmAddress <= DC_RAM_BASE || shouldResetDisasmAddress)
		// FIXME: Scrolling past the end of the disassembly table resets PC to the beginning
		disasmAddress = DC_RAM_BASE;

	// TODO: Capstone doesn't need to be initialized every frame
	if (capstoneHandle)
		cs_close(&capstoneHandle);

	ImGui::End();

	lastPc = pc;
}

static void drawMnemonic(cs_insn *instruction)
{
	if (branchIntructions.find(instruction->id) != branchIntructions.end())
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(305 / 360.0, .4, .85));
	else if (logicalIntructions.find(instruction->id) != branchIntructions.end())
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(25 / 360.0, .3, 1));
	else if (arithmeticIntructions.find(instruction->id) != branchIntructions.end())
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(25 / 360.0, .3, 1));
	else if (instruction->id == SH_INS_NOP)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
	else
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(0, 0, .9));

	ImGui::Text("%-8s", instruction->mnemonic);
	ImGui::PopStyleColor();
}

static void goTo(u32 address)
{
	// Ignore modifier bits and align to 2 bytes
	disasmAddress = address & 0x1ffffffe;
}
