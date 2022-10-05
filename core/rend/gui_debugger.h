#pragma once

#include "types.h"

void gui_debugger_control();

void gui_debugger_disasm();

extern u32 memoryDumpAddr;
void gui_debugger_memdump();

void gui_debugger_breakpoints();

void gui_debugger_sh4();

void gui_debugger_tbg();
