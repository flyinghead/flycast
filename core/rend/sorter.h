/*
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
#include "types.h"
#include "hw/pvr/Renderer_if.h"

//Sort based on min-z of each strip
void SortPParams(int first, int count);

struct SortTrigDrawParam
{
	PolyParam* ppid;
	u32 first;
	u32 count;
};

// Sort based on min-z of each triangle
void GenSorted(int first, int count, vector<SortTrigDrawParam>& sorted_pp, vector<u32>& sorted_idx);
