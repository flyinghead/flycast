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

// TODO: Use camelCase for variable names
// TODO: Add sh4asm as a submodule
// TODO: Rename debugger clearly indicate that it is a Dreamcast (guest) debugger. (FC_DC_DEBUGGER?)

extern ImFont *monospaceFont;

static bool disasm_window_open = true;
static bool memdump_window_open = false;
static bool breakpoints_window_open = false;
static bool sh4_window_open = true;

static void gui_debugger_control()
{
	ImGui::SetNextWindowPos(ScaledVec2(16, 16), ImGuiCond_FirstUseEver);
	ImGui::Begin("Control", NULL, ImGuiWindowFlags_NoResize);

	bool running = emu.running();

	if (running) {
		if (ImGui::Button("Suspend", ImVec2(80, 0)))
		{
			// config::DynarecEnabled = false;
			debugAgent.interrupt();
			// The debugger is rendered as GUI when the emulation is suspended.
			gui_state = GuiState::Debugger;
		}
	} else {
		if (ImGui::Button("Resume", ImVec2(80, 0)))
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
		ImGui::PopButtonRepeat();
	}

	// TODO: Implement step over and step out

	// TODO: Decide if debugger should have an closed state
	// ImGui::SameLine();
	// if (ImGui::Button("Close"))
	// {
	// 	gui_state = GuiState::Closed;
	// 	GamepadDevice::load_system_mappings();
	// 	if(!emu.running())
	// 		emu.start();
	// }

	ImGui::Checkbox("Disassembly", &disasm_window_open);

	ImGui::SameLine();
	ImGui::Checkbox("Memory Dump", &memdump_window_open);

	ImGui::SameLine();
	ImGui::Checkbox("SH4", &sh4_window_open);

	ImGui::SameLine();
	ImGui::Checkbox("Breakpoints", &breakpoints_window_open);

	ImGui::End();
}

u32 memoryDumpAddr = 0x0c010000;
ImU32 vslider_value = 0x10000 / 16;

static void gui_debugger_memdump()
{
	if (!memdump_window_open) return;

	ImGui::SetNextWindowPos(ImVec2(600, 450), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(540, 0));
	ImGui::Begin("Memory Dump", &memdump_window_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

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
		// debugAgent.insertMatchpoint(0, (u32) patchAddress, 2);
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
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,2));

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
	ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth()-20, dumpTopPosY));
	if (ImGui::VSliderScalar("##scroll", ImVec2(20, sliderHeight), ImGuiDataType_U32, &vslider_value, &max, &min, "", ImGuiSliderFlags_NoInput))
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
	if (!breakpoints_window_open) return;

	ImGui::SetNextWindowPos(ImVec2(700, 16), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(150, 0));
	ImGui::Begin("Breakpoints", &breakpoints_window_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

	ImGui::PushItemWidth(80 * settings.display.uiScale);
	static char bpBuffer[9] = "";
	ImGui::InputTextWithHint("##bpAddr", "Address", bpBuffer, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	if (ImGui::Button("Add"))
	{
		char* tmp;
		long bpaddr = strtoul(bpBuffer, &tmp, 16);
		debugAgent.insertMatchpoint(DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK, (u32) bpaddr, 2);
	}

	ImGui::PushFont(monospaceFont);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,2));

	auto it = debugAgent.breakpoints[DebugAgent::Breakpoint::Type::BP_TYPE_SOFTWARE_BREAK].begin();

	while (it != debugAgent.breakpoints[DebugAgent::Breakpoint::Type::BP_TYPE_SOFTWARE_BREAK].end())
	{
		ImGui::Text("0x%08x", it->first);

		it++;
	}

	ImGui::PopStyleVar();
	ImGui::PopFont();
	ImGui::End();
}

static void gui_debugger_sh4()
{
	if (!sh4_window_open) return;

	ImGui::SetNextWindowPos(ImVec2(900, 16), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(260, 0));
	ImGui::Begin("SH4", &sh4_window_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
	ImGui::PushFont(monospaceFont);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,2));

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

// TODO: Move to a separate file
void gui_debugger()
{
	if (config::ThreadedRendering) {
		return;
	}

	gui_debugger_control();

	if (disasm_window_open)
		gui_debugger_disasm();

	gui_debugger_memdump();

	gui_debugger_breakpoints();

	gui_debugger_sh4();
}