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
#include "gui_debugger.h"
#include "types.h"
#include "debug/debug_agent.h"
#include "emulator.h"
#include "ui/gui_util.h"
#include "hw/sh4/sh4_if.h"
#include "imgui/imgui.h"
#include "input/gamepad_device.h"
#include "gui_debugger_disasm.h"

// TODO: Add sh4asm as a submodule

extern ImFont *monospaceFont;

static bool disasmWindowOpen = true;
static bool memdumpWindowOpen = false;
static bool breakpointsWindowOpen = false;
static bool sh4WindowOpen = true;

static void gui_debugger_control()
{
	ImGui::SetNextWindowPos(ScaledVec2(16, 16), ImGuiCond_FirstUseEver);
	ImGui::Begin("Control", NULL, ImGuiWindowFlags_NoResize);

	bool running = emu.running();

	if (running) {
		if (ImGui::Button("Suspend", ScaledVec2(80, 0)))
		{
			// config::DynarecEnabled = false;
			debugAgent.interrupt();
			// The debugger is rendered as GUI when the emulation is suspended.
			gui_state = GuiState::Debugger;
		}
	} else {
		if (ImGui::Button("Resume", ScaledVec2(80, 0)))
		{
			// config::DynarecEnabled = false;
			// Step possible breakpoint in the next instruction
			debugAgent.step();
			emu.start();
			// The debugger is rendered as OSD when the emulation is suspended.
			gui_state = GuiState::Closed;
		}
	}

	{
		DisabledScope scope(running);
		ImGui::SameLine();
		ImGui::PushButtonRepeat(true);
		if (ImGui::Button("Step Into"))
		{
			debugAgent.step();
		}

		ImGui::SameLine();
		if (ImGui::Button("Step Over"))
		{
			// If is subroutine instruction
			u16 instruction = ReadMem16_nommu(Sh4cntx.pc);
			if (debugAgent.hasEnabledSoftwareBreakpoint(Sh4cntx.pc))
				instruction = debugAgent.findBreakpoint(Sh4cntx.pc)->savedOp;

			if ((instruction & 0xf000) == 0xb000 // bsr
				|| (instruction & 0xf0ff) == 0x0003 // bsrf
				|| (instruction & 0xf0ff) == 0x400b) // jsr
			{
				if (debugAgent.insertSoftwareBreakpoint(Sh4cntx.pc + 4)) {
					debugAgent.findBreakpoint(Sh4cntx.pc + 4)->singleShot = true;
					// Step possible breakpoint in the next instruction
					debugAgent.step();
					emu.start();
					// The debugger is rendered as OSD when the emulation is suspended.
					gui_state = GuiState::Closed;
				}
			}
			else {
				debugAgent.step();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Step Out"))
		{
			if (debugAgent.insertSoftwareBreakpoint(Sh4cntx.pr)) {
				debugAgent.findBreakpoint(Sh4cntx.pr)->singleShot = true;
				// Step possible breakpoint in the next instruction
				debugAgent.step();
				emu.start();
				// The debugger is rendered as OSD when the emulation is suspended.
				gui_state = GuiState::Closed;
			}
		}
		ImGui::PopButtonRepeat();
	}

	// TODO: Implement step out

	{
		// FIXME: Allow reset while running
		DisabledScope scope(!running);
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
			emu.requestReset();
	}

	ImGui::Checkbox("Disassembly", &disasmWindowOpen);

	ImGui::SameLine();
	ImGui::Checkbox("Memory Dump", &memdumpWindowOpen);

	ImGui::SameLine();
	ImGui::Checkbox("SH4", &sh4WindowOpen);

	ImGui::SameLine();
	ImGui::Checkbox("Breakpoints", &breakpointsWindowOpen);

	ImGui::End();
}

u32 memoryDumpAddr = 0x0c010000;
ImU32 vslider_value = 0x10000 / 16;

static void gui_debugger_memdump()
{
	if (!memdumpWindowOpen) return;

	ImGui::SetNextWindowPos(ScaledVec2(600, 450), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(540, 0));
	ImGui::Begin("Memory Dump", &memdumpWindowOpen, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	ImGui::PushItemWidth(80 * settings.display.uiScale);
	static char patchAddressBuffer[8 + 1] = "";
	static char patchWordBuffer[4 + 1] = "";
	ImGui::InputTextWithHint("##patchAddr", "Patch Addr", patchAddressBuffer, 8 + 1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::SameLine();
	ImGui::InputTextWithHint("##patchWord", "WORD", patchWordBuffer, 4 + 1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	if (ImGui::Button("Write"))
	{
		char* tmp;
		long patchAddress = strtoul(patchAddressBuffer, &tmp, 16);
		long patchWord = strtoul(patchWordBuffer, &tmp, 16);
		// debugAgent.insertBreakpoint(0, (u32) patchAddress, 2);
		WriteMem16_nommu(patchAddress, patchWord);
	}

	ImGui::PushItemWidth(80);
	static char memDumpAddrBuf[8 + 1] = "";
	ImGui::InputText("##memDumpAddr", memDumpAddrBuf, 8 + 1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	if (ImGui::Button("Go"))
	{
		// TODO: Validate input
		char* tmp;
		memoryDumpAddr = ((strtoul(memDumpAddrBuf, &tmp, 16) / 16) * 16) & 0x1fffffff;
		vslider_value = (memoryDumpAddr - 0x0c000000) / 0x10;
	}

	ImGui::PushFont(monospaceFont);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ScaledVec2(8,2));

	char hexbuf[256];

	float dumpTopPosY = ImGui::GetCursorPosY();

	// TODO: Extract 24 constant (used to clamp memoryDumpAddr further below)
	for (size_t i = 0; i < 24; i++) {
		memset(hexbuf, 0, sizeof(hexbuf));
		size_t hexbuflen = 0;

		// hexbuflen += sprintf(hexbuf, "%08lX: ", memoryDumpAddr + i * 16);
		ImGui::Text("%08lX: ", memoryDumpAddr + i * 16);
		ImGui::SameLine();

		for (size_t j = 0; j < 16; j++) {
			int byte = ReadMem8_nommu(memoryDumpAddr + i * 16 + j);
			// hexbuflen += sprintf(hexbuf + hexbuflen, "%02X", byte);
			if (byte == 0) {
				ImGui::TextDisabled("%02X", byte);
			} else {
				ImGui::Text("%02X", byte);
			}

			if ((j + 1) % 4 == 0) {
				// hexbuflen += sprintf(hexbuf + hexbuflen, "|");
				ImGui::SameLine(0, 8);
			} else {
				// hexbuflen += sprintf(hexbuf + hexbuflen, " ");
				ImGui::SameLine(0, 4);
			}
		}
		// hexbuflen += sprintf(hexbuf + hexbuflen, " ");
		for (size_t j = 0; j < 16; j++) {
			int c = ReadMem8_nommu(memoryDumpAddr + i * 16 + j);
			hexbuflen += sprintf(hexbuf + hexbuflen, "%c", (c >= 33 && c <= 126 ? c : '.'));
		}

		ImGui::Text("%s", hexbuf);
	}

	/* ImGui::SetNextItemWidth(-FLT_MIN); */
	
	const ImU64 min = 0x0;
	const ImU64 max = (RAM_SIZE / 0x10);

	float sliderHeight = ImGui::GetCursorPosY() - dumpTopPosY;
	ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth()-uiScaled(20), dumpTopPosY));
	if (ImGui::VSliderScalar("##scroll", ImVec2(uiScaled(20), sliderHeight), ImGuiDataType_U32, &vslider_value, &max, &min, "", ImGuiSliderFlags_NoInput))
	{
		memoryDumpAddr = 0x0c000000 + vslider_value * 0x10;
	}
	else if (ImGui::IsWindowHovered())
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.MouseWheel > 0 && memoryDumpAddr >= 0x0c000000 + 0x10) {
			memoryDumpAddr -= 0x10;
			vslider_value = (memoryDumpAddr - 0x0c000000) / 0x10;
		} else if (io.MouseWheel < 0 && memoryDumpAddr <= 0x0c000000 + RAM_SIZE - 0x10) {
			memoryDumpAddr += 0x10;
			vslider_value = (memoryDumpAddr - 0x0c000000) / 0x10;
		}
	}

	// TODO: Remove modifier bits from address;

	if (memoryDumpAddr > 0x0c000000 + RAM_SIZE - 24 * 16)
	{
		memoryDumpAddr = 0x0c000000 + RAM_SIZE - 24 * 16;
		vslider_value = (memoryDumpAddr - 0x0c000000) / 0x10;
	}

	ImGui::PopFont();
	ImGui::PopStyleVar();
	ImGui::End();
}

static void gui_debugger_breakpoints()
{
	// TODO: This won't be needed when memory breakpoints are implemented.
	debugAgent.disableOverwrittenBreakpoints();

	if (!breakpointsWindowOpen) return;

	ImGui::SetNextWindowPos(ScaledVec2(700, 16), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(304, 0));
	ImGui::Begin("Breakpoints", &breakpointsWindowOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
	ImGui::PushFont(monospaceFont);

	static u32 bpaddr = 0x8c010000;
	// TODO: Extract InputAddress function
	ImGui::PushItemWidth(ImGui::CalcTextSize("00000000").x + ImGui::GetStyle().FramePadding.x * 2);
	ImGui::InputScalar("##bpddress", ImGuiDataType_U32, &bpaddr, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_NoHorizontalScroll);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFrameHeight() + ImGui::CalcTextSize("Execute").x + ImGui::GetStyle().FramePadding.x * 2.0f);
	const char* bp_options[] = { "Execute", "Write", "Read", "Access" };
	const DebugAgent::Breakpoint::Type bp_types[] = {
		DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK,
		DebugAgent::Breakpoint::BP_TYPE_WRITE_WATCHPOINT,
		DebugAgent::Breakpoint::BP_TYPE_READ_WATCHPOINT,
		DebugAgent::Breakpoint::BP_TYPE_ACCESS_WATCHPOINT
	};
	static int selected_bp_type = 0;
	const char* bp_preview = bp_options[selected_bp_type];
	if (ImGui::BeginCombo("##combo", bp_preview))
	{
		for (int n = 0; n < IM_ARRAYSIZE(bp_options); n++)
		{
			const bool is_selected = (selected_bp_type == n);
			if (ImGui::Selectable(bp_options[n], is_selected))
				selected_bp_type = n;

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Add"))
		debugAgent.insertBreakpoint(bp_types[selected_bp_type], (u32) bpaddr, 2);

	ImGui::Separator();

	//ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ScaledVec2(8,2));

	ImGui::BeginTable("##breakpoints", 4);
	{
		ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
		//float windowWidth = ImGui::GetWindowContentRegionMax().x;
		DebugAgent::Breakpoint *breakpointToDelete = nullptr;
		for (auto& [address, breakpoint] : debugAgent.breakpoints)
		{
			ImGui::PushID(address);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			if (breakpoint.singleShot)
				continue;

			if (ImGui::Checkbox("##breakpoint", &breakpoint.enabled)) {
				if (breakpoint.enabled)
					debugAgent.enableBreakpoint(address);
				else
					debugAgent.disableBreakpoint(address);
			}

			ImGui::TableNextColumn();
			ImGui::Text("0x%08x", address);

			ImGui::TableNextColumn();
			switch (breakpoint.type)
			{
				case DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK:
					ImGui::Text("Exec");
					break;
				// BP_TYPE_HARDWARE_BREAK,
				case DebugAgent::Breakpoint::BP_TYPE_WRITE_WATCHPOINT:
					ImGui::Text("Write");
					break;
				case DebugAgent::Breakpoint::BP_TYPE_READ_WATCHPOINT:
					ImGui::Text("Read");
					break;
				case DebugAgent::Breakpoint::BP_TYPE_ACCESS_WATCHPOINT:
					ImGui::Text("Access");
					break;
			}

			ImGui::TableNextColumn();
			ImGui::PushStyleColor(ImGuiCol_Button, 0);
			if (ImGui::Button("X"))
				breakpointToDelete = &breakpoint;
			ImGui::PopStyleColor();

			ImGui::PopID();
		}

		if (breakpointToDelete)
			debugAgent.removeBreakpoint(breakpointToDelete->addr);
	}
	ImGui::EndTable();

	// ImGui::PopStyleVar();
	ImGui::PopFont();
	ImGui::End();
}

static void gui_debugger_sh4()
{
	if (!sh4WindowOpen) return;

	ImGui::SetNextWindowPos(ScaledVec2(900, 16), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(260, 0));
	ImGui::Begin("SH4", &sh4WindowOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
	ImGui::PushFont(monospaceFont);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ScaledVec2(8,2));

	u32 pc = *GetRegPtr(reg_nextpc);

	u32 regValue;
	f32 floatRegValue;

	ImGui::Text("PC:  %08X", pc);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
		memoryDumpAddr = (pc / 16) * 16;
	}

	ImGui::SameLine();
	regValue = *GetRegPtr(reg_pr);
	ImGui::Text(" PR:      %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	regValue = *GetRegPtr(reg_r0);
	ImGui::Text("r0:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_0);
	ImGui::Text(" fr0:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r1);
	ImGui::Text("r1:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_1);
	ImGui::Text(" fr1:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r2);
	ImGui::Text("r2:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_2);
	ImGui::Text(" fr2:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r3);
	ImGui::Text("r3:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_3);
	ImGui::Text(" fr3:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r4);
	ImGui::Text("r4:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_4);
	ImGui::Text(" fr4:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r5);
	ImGui::Text("r5:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_5);
	ImGui::Text(" fr5:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r6);
	ImGui::Text("r6:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_6);
	ImGui::Text(" fr6:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r7);
	ImGui::Text("r7:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_7);
	ImGui::Text(" fr7:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r8);
	ImGui::Text("r8:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_8);
	ImGui::Text(" fr8:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r9);
	ImGui::Text("r9:  %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_9);
	ImGui::Text(" fr9:  %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r10);
	ImGui::Text("r10: %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_10);
	ImGui::Text(" fr10: %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r11);
	ImGui::Text("r11: %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_11);
	ImGui::Text(" fr11: %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r12);
	ImGui::Text("r12: %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_12);
	ImGui::Text(" fr12: %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r13);
	ImGui::Text("r13: %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_13);
	ImGui::Text(" fr13: %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r14);
	ImGui::Text("r14: %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_14);
	ImGui::Text(" fr14: %11.5f", floatRegValue);

	regValue = *GetRegPtr(reg_r15);
	ImGui::Text("r15: %08X", regValue);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && (regValue & 0xFF000000) == 0x8c000000) {
		memoryDumpAddr = (regValue / 16) * 16;
	}

	ImGui::SameLine();
	floatRegValue = *GetFloatRegPtr(reg_fr_15);
	ImGui::Text(" fr15: %11.5f", floatRegValue);

	ImGui::PopStyleVar();
	ImGui::PopFont();
	ImGui::End();
}

void gui_debugger()
{
	if (config::ThreadedRendering) {
		return;
	}

	gui_debugger_control();

	if (disasmWindowOpen)
		gui_debugger_disasm();

	gui_debugger_memdump();

	gui_debugger_breakpoints();

	gui_debugger_sh4();
}