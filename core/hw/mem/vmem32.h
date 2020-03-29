/*
    Created on: Apr 11, 2019

	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "_vmem.h"

bool vmem32_init();
void vmem32_term();
bool vmem32_handle_signal(void *fault_addr, bool write, u32 exception_pc);
void vmem32_flush_mmu();
void vmem32_protect_vram(u32 addr, u32 size);
void vmem32_unprotect_vram(u32 addr, u32 size);

extern bool vmem32_inited;
static inline bool vmem32_enabled() {
	return vmem32_inited;
}
