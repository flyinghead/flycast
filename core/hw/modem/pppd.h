/*
	pppd.h

	Created on: Sep 10, 2018

	Copyright 2018 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CORE_HW_MODEM_PPPD_H_
#define CORE_HW_MODEM_PPPD_H_
#include "types.h"

void start_pppd();
void stop_pppd();
void write_pppd(u8 b);
int read_pppd();
int avail_pppd();

#endif /* CORE_HW_MODEM_PPPD_H_ */
