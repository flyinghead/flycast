/*
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
#include "types.h"

enum vmu_screen_position_enum
{
	UPPER_LEFT = 0,
	UPPER_RIGHT,
	LOWER_LEFT,
	LOWER_RIGHT
};

#define VMU_SCREEN_WIDTH 48
#define VMU_SCREEN_HEIGHT 32
#define DEFAULT_VMU_PIXEL_ON_R 50
#define DEFAULT_VMU_PIXEL_ON_G 54
#define DEFAULT_VMU_PIXEL_ON_B 93
#define DEFAULT_VMU_PIXEL_OFF_R 135
#define DEFAULT_VMU_PIXEL_OFF_G 161
#define DEFAULT_VMU_PIXEL_OFF_B 134
#define VMU_NUM_COLORS 29

enum VMU_SCREEN_COLORS {
	VMU_DEFAULT_ON,
	VMU_DEFAULT_OFF,
	VMU_BLACK,
	VMU_BLUE,
	VMU_LIGHT_BLUE,
	VMU_GREEN,
	VMU_GREEN_BLUE,
	VMU_GREEN_LIGHT_BLUE,
	VMU_LIGHT_GREEN,
	VMU_LIGHT_GREEN_BLUE,
	VMU_LIGHT_GREEN_LIGHT_BLUE,
	VMU_RED,
	VMU_RED_BLUE,
	VMU_RED_LIGHT_BLUE,
	VMU_RED_GREEN,
	VMU_RED_GREEN_BLUE,
	VMU_RED_GREEN_LIGHT_BLUE,
	VMU_RED_LIGHT_GREEN,
	VMU_RED_LIGHT_GREEN_BLUE,
	VMU_RED_LIGHT_GREEN_LIGHT_BLUE,
	VMU_LIGHT_RED,
	VMU_LIGHT_RED_BLUE,
	VMU_LIGHT_RED_LIGHT_BLUE,
	VMU_LIGHT_RED_GREEN,
	VMU_LIGHT_RED_GREEN_BLUE,
	VMU_LIGHT_RED_GREEN_LIGHT_BLUE,
	VMU_LIGHT_RED_LIGHT_GREEN,
	VMU_LIGHT_RED_LIGHT_GREEN_BLUE,
	VMU_WHITE
};

struct rgb_t {
	u8 r ;
	u8 g ;
	u8 b ;
};

struct vmu_screen_params_t {
	u8 vmu_pixel_on_R = 128 ;
	u8 vmu_pixel_on_G = 0 ;
	u8 vmu_pixel_on_B = 0 ;
	u8 vmu_pixel_off_R = 0;
	u8 vmu_pixel_off_G = 0;
	u8 vmu_pixel_off_B = 0;
	u8 vmu_screen_size_mult = 2 ;
	u8 vmu_screen_opacity = 0xBF ;
	vmu_screen_position_enum vmu_screen_position = LOWER_RIGHT ;
};

extern const rgb_t VMU_SCREEN_COLOR_MAP[VMU_NUM_COLORS] ;
extern const char* VMU_SCREEN_COLOR_NAMES[VMU_NUM_COLORS] ;
extern vmu_screen_params_t vmu_screen_params[4] ;

#define LIGHTGUN_CROSSHAIR_SIZE 16

enum LIGHTGUN_COLORS {
	LIGHTGUN_COLOR_OFF,
	LIGHTGUN_COLOR_WHITE,
	LIGHTGUN_COLOR_RED,
	LIGHTGUN_COLOR_GREEN,
	LIGHTGUN_COLOR_BLUE,
	LIGHTGUN_COLORS_COUNT
};

struct lightgun_params_t {
	bool offscreen;
	float x;
	float y;
	bool dirty;
	int colour;
};

extern u8 lightgun_palette[LIGHTGUN_COLORS_COUNT*3];
extern u8 lightgun_img_crosshair[LIGHTGUN_CROSSHAIR_SIZE*LIGHTGUN_CROSSHAIR_SIZE];
extern lightgun_params_t lightgun_params[4] ;
