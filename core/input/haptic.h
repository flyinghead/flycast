/*
	Copyright 2024 flyinghead

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
#pragma once
#include "build.h"

#ifdef USE_SDL
void sdl_setTorque(int, float);
void sdl_setDamper(int, float, float);
void sdl_setSpring(int, float, float);
void sdl_stopHaptic(int);
#endif

namespace haptic {

inline static void setTorque(int port, float v) {
#ifdef USE_SDL
	sdl_setTorque(port, v);
#endif
}

inline static void setDamper(int port, float param, float speed) {
#ifdef USE_SDL
	sdl_setDamper(port, param, speed);
#endif
}

inline static void setSpring(int port, float saturation, float speed) {
#ifdef USE_SDL
	sdl_setSpring(port, saturation, speed);
#endif
}

inline static void stopAll(int port) {
#ifdef USE_SDL
	sdl_stopHaptic(port);
#endif
}

}
