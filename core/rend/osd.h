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
#include <vector>

#define VJOY_VISIBLE 14
#define OSD_TEX_W 512
#define OSD_TEX_H 256

struct OSDVertex
{
	float x, y;
	float u, v;
	u8 r, g, b, a;
};

const std::vector<OSDVertex>& GetOSDVertices();

extern std::vector<u8> DefaultOSDButtons;
u8 *loadOSDButtons(int &width, int &height);
void HideOSD();
