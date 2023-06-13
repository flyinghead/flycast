/*
	Copyright 2023 flyinghead

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
#include "gui_util.h"
#include "hw/sh4/sh4_if.h"
#include "imgui/imgui.h"
#include "input/gamepad_device.h"
#include "sh4asm/sh4asm_core/disas.h"

static bool disasm_window_open = true;
static bool memdump_window_open = false;
static bool breakpoints_window_open = false;
static bool sh4_window_open = true;


#define DISAS_LINE_LEN 128
static char sh4_disas_line[DISAS_LINE_LEN];

static void disas_emit(char ch) {
	size_t len = strlen(sh4_disas_line);
	if (len >= DISAS_LINE_LEN - 1)
		return; // no more space
	sh4_disas_line[len] = ch;
}

void gui_debugger_control()
{
	ImGui::SetNextWindowPos(ScaledVec2(16, 16), ImGuiCond_FirstUseEver);
	ImGui::Begin("Control", NULL, ImGuiWindowFlags_NoResize);

	bool running = emu.running();

	if (!running)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}
	if (ImGui::Button("Suspend"))
	{
		// config::DynarecEnabled = false;
		debugAgent.interrupt();
	}
	if (!running)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	if (running)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	ImGui::SameLine();
	ImGui::PushButtonRepeat(true);
	if (ImGui::Button("Step"))
	{
		debugAgent.step();
	}
	ImGui::PopButtonRepeat();

	ImGui::SameLine();
	if (ImGui::Button("Resume"))
	{
		debugAgent.step();
		emu.start();
	}
	if (running)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	ImGui::SameLine();
	if (ImGui::Button("Close"))
	{
		gui_state = GuiState::Closed;
		GamepadDevice::load_system_mappings();
		if(!emu.running())
			emu.start();
	}

	ImGui::Checkbox("Disassembly", &disasm_window_open);

	ImGui::SameLine();
	ImGui::Checkbox("Memory Dump", &memdump_window_open);

	ImGui::SameLine();
	ImGui::Checkbox("SH4", &sh4_window_open);

	ImGui::SameLine();
	ImGui::Checkbox("Breakpoints", &breakpoints_window_open);

	ImGui::End();
}

void gui_debugger_disasm()
{
	if (!disasm_window_open) return;

	u32 pc = *GetRegPtr(reg_nextpc);

	ImGui::SetNextWindowPos(ScaledVec2(16, 110), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(440, 0), ImGuiCond_FirstUseEver);
	
	if (!ImGui::Begin("Disassembly", &disasm_window_open)) {
		ImGui::End();
		return;
	}

	ImGui::PushItemWidth(80 * settings.display.uiScale);
	static char bpBuffer[9] = "";
	ImGui::InputTextWithHint("##bpAddr", "BP Addr", bpBuffer, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	if (ImGui::Button("Add BP"))
	{
		char* tmp;
		long bpaddr = strtoul(bpBuffer, &tmp, 16);
		debugAgent.insertMatchpoint(DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK, (u32) bpaddr, 2);
	}

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

	// if (Sh4cntx.pc == 0x8C010000 || Sh4cntx.spc == 0x8C010000)
	// {
	// 	NOTICE_LOG(COMMON, "1ST_READ.bin entry");
	// 	dc_stop();
	// }

	ImGuiIO& io = ImGui::GetIO();
	ImFontAtlas* atlas = io.Fonts;
	ImFont* defaultFont = atlas->Fonts[1];

	ImGui::PushFont(defaultFont);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,2));


	for (size_t i = 0; i < 20; i++)
	{
		const u32 addr = pc + i * 2;

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

		memset(sh4_disas_line, 0, sizeof(sh4_disas_line));
		sh4asm_disas_inst(instr, disas_emit, addr);
		//dasmbuf = decode(instr, pc);
		sprintf(buf, "%08X:", (u32) addr);
		ImGui::Text("%s", buf);
		ImGui::SameLine();
		ImGui::TextDisabled("%04X", instr);
		ImGui::SameLine();
		ImGui::Text("%s", sh4_disas_line);
	}

	ImGui::PopFont();
	ImGui::PopStyleVar();
	ImGui::End();
}

u32 memoryDumpAddr = 0x0c010000;

void gui_debugger_memdump()
{
	if (!memdump_window_open) return;

	ImGui::SetNextWindowPos(ImVec2(600, 450), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(540, 0));
	ImGui::Begin("Memory Dump", &memdump_window_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::PushItemWidth(80);
	static char memDumpAddrBuf[8 + 1] = "";
	ImGui::InputText("##memDumpAddr", memDumpAddrBuf, 8 + 1, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	if (ImGui::Button("Go"))
	{
		char* tmp;
		memoryDumpAddr = (strtoul(memDumpAddrBuf, &tmp, 16) / 16) * 16;
	}

	ImGui::SameLine();
	if (ImGui::Button("---"))
	{
		memoryDumpAddr -= 16 * 16 * 16;
	}

	ImGui::SameLine();
	if (ImGui::Button("--"))
	{
		memoryDumpAddr -= 16 * 16;
	}

	ImGui::SameLine();
	if (ImGui::Button("-"))
	{
		memoryDumpAddr -= 16;
	}

	ImGui::SameLine();
	if (ImGui::Button("+"))
	{
		memoryDumpAddr += 16;
	}

	ImGui::SameLine();
	if (ImGui::Button("++"))
	{
		memoryDumpAddr += 16 * 16;
	}

	ImGui::SameLine();
	if (ImGui::Button("+++"))
	{
		memoryDumpAddr += 16 * 16 * 16;
	}

	ImGuiIO& io = ImGui::GetIO();
	ImFontAtlas* atlas = io.Fonts;
	ImFont* defaultFont = atlas->Fonts[1];

	ImGui::PushFont(defaultFont);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,2));

	ImGui::BeginChild("##content", ImVec2(-FLT_MIN, 360));
	char hexbuf[256];
	for (size_t i = 0; i < 24; i++) {
		memset(hexbuf, 0, sizeof(hexbuf));
		size_t hexbuflen = 0;

		hexbuflen += sprintf(hexbuf, "%08lX: ", memoryDumpAddr + i * 16);

		for (size_t j = 0; j < 16; j++) {
			int byte = ReadMem8_nommu(memoryDumpAddr + i * 16 + j);
			hexbuflen += sprintf(hexbuf + hexbuflen, "%02X", byte);

			if ((j + 1) % 4 == 0) {
				hexbuflen += sprintf(hexbuf + hexbuflen, "|");
			} else {
				hexbuflen += sprintf(hexbuf + hexbuflen, " ");
			}
		}
		hexbuflen += sprintf(hexbuf + hexbuflen, " ");
		for (size_t j = 0; j < 16; j++) {
			int c = ReadMem8_nommu(memoryDumpAddr + i * 16 + j);
			// fprintf(fp_out, "%c", )
			hexbuflen += sprintf(hexbuf + hexbuflen, "%c", (c >= 33 && c <= 126 ? c : '.'));
		}

		ImGui::Text("%s", hexbuf);
	}

	if (ImGui::IsWindowHovered())
	{
		memoryDumpAddr -= (int) io.MouseWheel * 16;
	}

	if (memoryDumpAddr < 0x8c000000)
	{
		memoryDumpAddr = 0x8c000000;
	}
	else if (memoryDumpAddr > 0x8c000000 + RAM_SIZE)
	{
		memoryDumpAddr = 0x8c000000 + RAM_SIZE;
	}
	ImGui::EndChild();

	ImGui::PopFont();
	ImGui::PopStyleVar();
	ImGui::End();
}

void gui_debugger_breakpoints()
{
	if (!breakpoints_window_open) return;

	ImGuiIO& io = ImGui::GetIO();
	ImFontAtlas* atlas = io.Fonts;
	ImFont* defaultFont = atlas->Fonts[1];

	ImGui::SetNextWindowPos(ImVec2(700, 16), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(150, 0));
	ImGui::Begin("Breakpoints", &breakpoints_window_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::PushFont(defaultFont);
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

void gui_debugger_sh4()
{
	if (!sh4_window_open) return;

	ImGuiIO& io = ImGui::GetIO();
	ImFontAtlas* atlas = io.Fonts;
	ImFont* defaultFont = atlas->Fonts[1];

	ImGui::SetNextWindowPos(ImVec2(900, 16), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ScaledVec2(260, 0));
	ImGui::Begin("SH4", &sh4_window_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::PushFont(defaultFont);

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
