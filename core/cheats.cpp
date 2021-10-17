/*
    Created on: Sep 23, 2019

	Credits for cheats: Esppiral, S4pph4rad, yzb37859365, Shenmue_Trilogy, Radaron, Virgin KLM, Joel, Zorlon,
		ELOTROLADO.NET, SEGARETRO.ORG, Sakuragi @ emutalk.net
	Copyright 2019 flyinghead

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
#include "cheats.h"
#include "hw/sh4/sh4_mem.h"
#include "reios/reios.h"
#include "cfg/cfg.h"
#include "cfg/ini.h"

const WidescreenCheat CheatManager::widescreen_cheats[] =
{
		{ "MK-51064",   nullptr,    { 0x39EFF4, 0 }, { 0x43700000 } },		// 18 wheeler (USA)
		{ "MK-5106450", nullptr,    { 0x39EFF4, 0 }, { 0x43700000 } },		// 18 wheeler (PAL)
		{ "HDR-0080",   nullptr,    { 0x6625C0, 0 }, { 0x43700000 } },		// 18 wheeler (JP)
		{ "T-9708N",    nullptr,    { 0x0AD5B0, 0x112E00, 0 }, { 0x4A2671C4, 0x401CCCCD } },	// 4 Wheel Thunder (USA)
		{ "MK-5119050", nullptr,    { 0x104A34, 0 }, { 0x3F52CCCD } },		// 90 Minutes - Sega Championship Football (PAL)
		{ "T40209D 50", nullptr,    { 0x2A8750, 0 }, { 0x3F400000 } },		// AeroWings 2 (PAL)
		{ "T34101M",    nullptr,    { 0x24CF20, 0x24CEEC, 0 }, { 0x3FAAAAAB, 0x43F00000, 0 } },	// Animastar (JP)
		{ "MK-51171",   nullptr,    { 0xCA20B8, 0 }, { 0x43700000 } },		// Alien Front Online (USA)
		{ "T2101M",     nullptr,    { 0x13E8B4, 0x13E8E4, 0 }, { 0x43F00000, 0x3F400000 } },	// Berserk (JP)
		{ "T13001D 18", nullptr,    { 0x2DAC44, 0 }, { 0x3F400000 } },		// Blue Stinger (En, De) (PAL)
		{ "xxxxxxxxxx", nullptr,    { 0x2DAB84, 0 }, { 0x3F400000 } },		// Blue Stinger (Fr) (PAL)
		{ "T13001D-05", nullptr,    { 0x2D9C84, 0 }, { 0x3F400000 } },		// Blue Stinger (Es, It?) (PAL)
		{ "T13001N",    nullptr,    { 0x2D9CA4, 0 }, { 0x3F400000 } },		// Blue Stinger (USA)
		{ "HDR-0003",   nullptr,    { 0x2D6D80, 0 }, { 0x3F400000 } },		// Blue Stinger (JP)
		{ "T42903M",    nullptr,    { 0x286E2C, 0x286E5C, 0 }, { 0x43F00000, 0x3F400000 } },	// Bomber Hehhe! (JP)
		{ "MK-51065",   nullptr,    { 0x387B84, 0x387BB4, 0 }, { 0x43F00000, 0x3F400000 } },	// Bomberman Online (USA)
		{ "T6801M",     nullptr,    { 0x042F3C, 0 }, { 0x4384BC09 } },		// Buggy Heat (JP)
		{ "T46601D 05", nullptr,    { 0xBB3D14, 0 }, { 0x440A7C9A } },		// Cannon Spike (PAL)
		{ "T1215N",     nullptr,    { 0xBB3C74, 0 }, { 0x440A7C9A } },		// Cannon Spike (USA)
		{ "T44901D 50", nullptr,    { 0xB0109C, 0xB010CC, 0 }, { 0x43F00000, 0x3F400000 } },	// Carrier (PAL)
		{ "T5701N",     nullptr,    { 0xAFD93C, 0xAFD96C, 0 }, { 0x43F00000, 0x3F400000 } },	// Carrier (USA)
		// Capcom vs. SNK Pro (JP)
		// You can see the end of the backgrounds on each side. The HUD is shifted to the right.
		// Code 2 sets HUD and movements of the characters to a 4:3 zone
		{ "T1247M",     nullptr,    { 0x1E8238, 0x3D3658, 0x3D3628, 0 }, { 0x43F00000, 0x43D20000, 0x43700000 } },
		// Capcom vs. SNK 2 (JP)
		// You can see the end of the backgrounds on each side.
		{ "T1249M",     nullptr,    { 0x39BFD8, 0x34E9E8, 0x34EFBC, 0 }, { 0x43700000, 0x3F400000, 0x3F400000 } },
		{ "T44902D 50", nullptr,    { 0x1DBFF8, 0 }, { 0x43700000 } },		// Charge ‘n Blast (PAL)
		{ "T4402M",     nullptr,    { 0x1DA9E0, 0 }, { 0x43700000 } },		// Charge ‘n Blast (JP)
		{ "MK-5104950", nullptr,    { 0x2D2D40, 0x2D2D70, 0 }, { 0x3F400000, 0xC2700000 } },	// ChuChu Rocket! (PAL)
		{ "T44903D 50", nullptr,    { 0x315300, 0x315334, 0 }, { 0x43F00000, 0x3FAAAAAB } },	// Coaster Works (PAL)
		// Confidential Mission (PAL) 022F0D58 43700000 - Only works on real Dreamcast
		{ "T36901M",    nullptr,    { 0x1C8A98, 0 }, { 0x3F400000 } },		// Cool Boarders (JP)
		{ "T3106M",     nullptr,    { 0x60B59C, 0 }, { 0x3F400000 } },		// Cool Cool Toon (JP)
		{ "HDR-0176",   nullptr,    { 0x240FAC, 0 }, { 0x3F400000 } },		// Cosmic Smash (JP)
		{ "MK-51035",   " U      ", { 0x2B08B0, 0 }, { 0x43700000 } },		// Crazy Taxi (USA)
		{ "MK-51035",   "  E     ", { 0x2B3410, 0 }, { 0x43700000 } },		// Crazy Taxi (PAL)
		{ "MK-5113650", nullptr,    { 0x2BFB70, 0 }, { 0x43700000 } },		// Crazy Taxi 2 (PAL)
		{ "MK-51136",   nullptr,    { 0x2BDDD0, 0 }, { 0x43700000 } },		// Crazy Taxi 2 (USA)
//		{ "HDR-0159",   nullptr,    { 0x2FBBD0, 0 }, { 0x43700000 } },		// Crazy Taxi 2 (JP) not working
		{ "T13004N",    nullptr,    { 0x016D94, 0 }, { 0x44234E73 } },		// Cyber Troopers - Virtual On - Oratorio Tangram (USA)
		// D2 (USA)
		{ "MK-51036",   nullptr,    { 0x4B5CF4, 0x4B5CC4, 0x3E92A0, 0x3E92A8, 0x3E92C0, 0x3E92C8 },
				{ 0x3F400000, 0x43F00000, 0, 0, 0, 0 } },
		// D2 (JP)
		{ "T30006M",    nullptr,    { 0x4CF42C, 0x4CF45C, 0x3E1A36, 0x3E1A34, 0x3E1A3C, 0x3E1A54, 0x3E1A5C, 0 },
				{ 0x43F00000, 0x3F400000, 0x08010000, 0, 0, 0, 0 } },
		{ "MK-5103750", nullptr,    { 0x1FE270, 0 }, { 0x43700000 } },		// Daytona USA (PAL)
		{ "MK-51037",   nullptr,    { 0x1FC6D0, 0 }, { 0x43700000 } },		// Daytona USA (USA)
		{ "T9501N-50",  nullptr,    { 0x9821D4, 0 }, { 0x3F400000 } },		// Deadly Skies (PAL)
		{ "T8116D  50", nullptr,    { 0x2E5530, 0 }, { 0x43700000 } },		// Dead or Alive 2 (PAL)
		{ "T3601N",     nullptr,    { 0x2F0670, 0 }, { 0x43700000 } },		// Dead or Alive 2 (USA)
		{ "T3602M",     nullptr,    { 0x2FF798, 0 }, { 0x43700000 } },		// Dead or Alive 2 (JP)
		{ "T3601M",     nullptr,    { 0x2FBBD0, 0x1E6658, 0 }, { 0x43700000, 0x4414C000 } },	// Dead or Alive 2: Limited Edition (JP) (code 1 fixes HUD)
		{ "T2401N",     nullptr,    { 0x8BD5B4, 0x8BD5E4, 0 }, { 0x43F00000, 0x3F400000 } },	// Death Crimson OX (USA)
		{ "T23201M",    nullptr,    { 0x819F44, 0x819F74, 0 }, { 0x43F00000, 0x3F400000 } },	// Death Crimson 2 (JP)
		{ "T17714D50",  nullptr,    { 0x0D2ED0, 0x0D2ED4, 0 }, { 0x3FAAAAAB, 0x3F400000 } },	// Donald Duck: Quack Attack (PAL) (Code 1 corrects the HUD)
		{ "T12503D-50", nullptr,    { 0x49CB24, 0 }, { 0x3F0CCCCD } },		// Dragons Blood (PAL)
		{ "T17716D 50", nullptr,    { 0xF97F40, 0x08DCF8, 0 }, { 0x00000168, 0 } },		// Dragonriders: Chronicles of Pern (PAL)
		{ "T17720N",    nullptr,    { 0xF97F40, 0x08DCF4, 0 }, { 0x00000168, 0 } },		// Dragonriders: Chronicles of Pern (USA) (Code 1 removes black bars)
		{ "MK-5101350", nullptr,    { 0x4FCBC0, 0 }, { 0x43700000 } },		// Dynamite Cop  (PAL)
		{ "MK-51013",   nullptr,    { 0x4FCBC0, 0 }, { 0x43700000 } },		// Dynamite Cop  (USA)
		// Draconus: Cult of the Wyrm (USA)
		// Code 1-2 increases drawing distance
		{ "T40203N",    nullptr,    { 0x49D7F4, 0x49D8CC, 0x49D6F8, 0 }, { 0x3F07C3BB, 0x447A0000, 0x3FAAAAAB } },
		// Ecco the Dolphin: Defender of the Future (PAL)
		{ "MK-5103350", nullptr,    { 0x275418, 0x040E68, 0x040D1C, 0 }, { 0x49D9A5DA, 0x3F100000, 0x3F100000 } },
		{ "T17705D 05", nullptr,    { 0x304870, 0 }, { 0x3F400000 } },		// Evolution - The World of Sacred Device (PAL)
		{ "T45005D 50", nullptr,    { 0x36CE5C, 0x36CE8C, 0 }, { 0x43F00000, 0x3F400000 } },	// Evolution 2 - Far Off Promise (PAL)
		{ "T1711N",     nullptr,    { 0x36C76C, 0x36C73C, 0 }, { 0x3F400000, 0x43F00000 } },	// Evolution 2 - Far Off Promise (USA)
		{ "T8118D  50", nullptr,    { 0x2C6B7C, 0 }, { 0x00004000 } },		// Ferrari F355 Challenge (PAL) vga mode only
		{ "HDR-0100",   nullptr,    { 0x3235D4, 0 }, { 0x00004000 } },		// Ferrari F355 Challenge (JP) vga mode only
		{ "MK-5115450", nullptr,    { 0x3D3B10, 0 }, { 0x43700000 } },		// Fighting Vipers 2 (PAL)
		{ "HDR-0133",   nullptr,    { 0x3D3AF0, 0 }, { 0x43700000 } },		// Fighting Vipers 2 (JP)
		{ "MK-51114",   nullptr,    { 0x132DD8, 0xA26CA8, 0xA26738, 0xA275B8, 0xA26AD8, 0xA26908, 0 },
				{ 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000 } },	// Floigan Bros. Ep. 1 (PAL)
		{ "T34201M",    nullptr,    { 0x586290, 0x586260, 0 }, { 0x3F400000, 0x43F00000 } },	// Frame Gride (JP)
		{ "T-8113D-50", nullptr,    { 0x55A354, 0 }, { 0x3FAAAAAB } },		// Fur Fighters (PAL)
		{ "T-8107N",    nullptr,    { 0x55A374, 0 }, { 0x3FAAAAAB } },		// Fur Fighters (USA)
		{ "T9707D  50", nullptr,    { 0x2E7CD0, 0x2E7D04, 0x2E7D10, 0x2E7D0C, 0 },
				{ 0x43BEAE39, 0x3DCCCCCD , 0x44A00000, 0xC4200000 } },		// Gauntlet Legends (PAL)
		{ "HDR-0004",   nullptr,    { 0x364A64, 0 }, { 0x43BA0000 } },		// Godzilla Generations (JP)
		{ "HDR-0047",   nullptr,    { 0x7C3F24, 0x7C3F54, 0 }, { 0x43F00000, 0x3F400000 } },	// Godzilla Generations Maximum Impact (JP)
		{ "T41501M",    nullptr,    { 0x282604, 0x282634, 0 }, { 0x43F00000, 0x3F400000 } },	// Golem no Maigo (aka The Lost Golem) (JP)
		{ "T1219M",     nullptr,    { 0xBC3C94, 0 }, { 0x440A7C9A } },		// Gun Spike (JP)
		{ "T13301N",    nullptr,    { 0x88E780, 0 }, { 0x3F400000 } },		// Gundam Side Story (USA)
//		{ "T11001N",    nullptr,    { 0xC3D6BC, 0xD204A8, 0xD32FC8, 0xC7CF84, 0xD20548, 0 },
//				{ 0x00363031, 0x00000356, 0x00000280, 0x00000280, 0x00000280 } },		// Half-Life. Not working
		{ "MK-5104150", nullptr,    { 0x23FCC4, 0 }, { 0x44558000 } },		// Headhunter (PAL)
		{ "MK-5100250", nullptr,    { 0x4C6708, 0 }, { 0x43700000 } },		// House of the Dead 2, The (PAL)
		{ "MK-51002",   nullptr,    { 0x4C6088, 0 }, { 0x43700000 } },		// House of the Dead 2, The (USA)
		{ "T38706M",    nullptr,    { 0xC0CFA0, 0 }, { 0x3F400000 } },		// Ikaruga (JP)
		{ "T46001N",    nullptr,    { 0x1C8A98, 0 }, { 0x3F400000 } },		// Illbleed (USA)
		{ "T44904D 50", nullptr,    { 0x18C15C, 0x18C18C, 0 }, { 0x43F00000, 0x3F400000 } },	// Iron Aces (PAL)
		{ "MK-51058",   nullptr,    { 0x32E0FC, 0x32E12C, 0 }, { 0x43F00000, 0x3F400000 } },	// Jet Grind Radio (USA)
		{ "MK-5105850", nullptr,    { 0x32F9EC, 0x32F9BC, 0 }, { 0x3F400000, 0x43F00000 } },	// Jet Set Radio (PAL)
		{ "HDR-0078",   nullptr,    { 0x327A8C, 0x327A5C, 0 }, { 0x3F400000, 0x43F00000 } },	// Jet Set Radio (De La) (JP)
		{ "T22902D 50", nullptr,    { 0x278508, 0 }, { 0x43700000 } },		// Kao The Kangaroo (PAL)
		{ "T22903N",    nullptr,    { 0x2780A8, 0 }, { 0x43700000 } },		// Kao The Kangaroo (USA)
		{ "T47803M",    nullptr,    { 0x0FDFAC, 0 }, { 0x3F400000 } },		// Karous (JP)
//		{ "T41901N",    nullptr,    { 0x53F580, 0xEFB748, 0xEFB750, 0 }, { 0xC4200000, 0x43A00000, 0x43200000 } },	// KISS Psycho Circus – The Nightmare Child (USA)
		{ "T2501M",     nullptr,    { 0x24A878, 0x24A8A8, 0 }, { 0x43F00000, 0x3F400000 } },	// Langrisser Millenium (JP)
		{ "T15111D 50", nullptr,    { 0x29B90C, 0 }, { 0x3F400000 } },		// Le Mans 24 Hours (PAL)
		{ "T15116N",    nullptr,    { 0x2198EC, 0 }, { 0x3F400000 } },		// Looney Tunes: Space Race (USA)
		{ "MK-5105050", nullptr,    { 0x33818C, 0 }, { 0x3FA66666 } },		// Maken X (PAL)
		{ "MK-51050",   nullptr,    { 0x30CB4C, 0 }, { 0x3F400000 } },		// Maken X (USA)
		{ "T1212N",     nullptr,    { 0x2D6B18, 0x268390, 0x268ED8, 0x268934, 0x26947C, 0x269A20, 0x269FC4, 0 },
				{ 0x43700000, 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000 } },		// Marvel vs. Capcom 2 (USA)
		{ "T1215M",     nullptr,    { 0x2FE2C0, 0x28FB38, 0x290680, 0x2900DC, 0x290C24, 0x2911C8, 0x29176C, 0 },
				{ 0x43700000, 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000, 0x3F400000 } },		// Marvel vs. Capcom 2 (JP)
		{ "T41402N",    nullptr,    { 0x551B80, 0 }, { 0x3F400000 } },		// Max Steel - Covert Missions (USA)
		{ "MK-5102250", "V1.001",   { 0x107FDC, 0x11253C, 0 }, { 0x3F99999A, 0x3F900000 } },	// Metropolis Street Racer (v1.001) (PAL)
		{ "MK-5102250", "V1.009",   { 0x106B5C, 0x1111F4, 0 }, { 0x3F99999A, 0x3F900000 } },	// Metropolis Street Racer (v1.009) (PAL)
		{ "MK-51012",   nullptr,    { 0x10A01C, 0x1146FC, 0 }, { 0x3F99999A, 0x3F900000 } },	// Metropolis Street Racer (USA)
		{ "T0000M",     nullptr,    { 0x1CAEAC, 0x1CAEDC, 0 }, { 0x43F00000, 0x3F400000 } },	// Millenium Racer Y2K Fighters
		{ "T1221M",     nullptr,    { 0x426B74, 0 }, { 0x43F00000 } },		// Moero! Justice Gauken (JP)
		{ "T9701D",     nullptr,    { 0x31290C, 0 }, { 0x3F400000 } },		// Mortal Kombat Gold (PAL)
		{ "T-9701N",    nullptr,    { 0x337B8C, 0 }, { 0x3FA66666 } },		// Mortal Kombat Gold (USA)
		{ "T7020D",     nullptr,    { 0x1A5D40, 0x1A5D18, 0x1A5D38, 0x1A5D34, 0x1A5D30, 0x1A5D3C, 0x1A5D0C, 0x1A5D08, 0x1ACB78, 0 },
				{ 0x3F59999A, 0x44084000, 0x3FB1EB85, 0x3FAAAAAB, 0x3F800000, 0x3FA66666, 0xC31B0000, 0x42700000, 0x44160000 } },		// Mr Driller (PAL)
		{ "T7604M",     nullptr,    { 0x43BDA4, 0x43BDD8, 0x4319E8, 0x431A08, 0x431A28, 0x431A48, 0 },
				{ 0x43700000, 0x3F9AF5C2, 0x42200000, 0x42200000, 0x438C0000, 0x438C0000 } },	// Nanatsu no Hikan - Senritsu no Bishou (JP)
		{ "HDR-0079",   nullptr,    { 0x353744, 0x353774, 0x353778, 0 },
				{ 0x43F00000, 0x3F400000, 0x3F800000 } },					// Napple Tale: Arsia in Daydream (JP)
//crash		{ "MK-51178",   nullptr,    { 0x23AF00, 0x23B160, 0x144D40, 0x2105B4, 0x705B40, 0 },
//				{ 0xBFAAAAAB, 0xBFAAAAAB, 0xBFAAAAAB, 0xBFAAAAAB, 0x44558000 } },	// NBA 2K2
		{ "T9504M",     nullptr,    { 0xCDE848, 0xCDE844, 0 }, { 0x3F400000, 0x3FA00000 } },	// Nightmare Creatures II (USA)
		{ "T-9502D-50", nullptr,    { 0xBDE9B0, 0xBDE9C4, 0 }, { 0x3F400000, 0x3FA00000 } },	// Nightmare Creatures II (PAL)
		{ "MK-5110250", nullptr,    { 0x87B5A4, 0 }, { 0x43700000 } },		// Outtrigger (PAL)
		{ "HDR-0118",   nullptr,    { 0x83E284, 0 }, { 0x43700000 } },		// Outtrigger (JP)
		{ "T15103D 50", nullptr,    { 0x1EEE78, 0 }, { 0x3F400000 } },		// PenPen (PAL)
		{ "T17001M",    nullptr,    { 0x1C3828, 0 }, { 0x3F400000 } },		// PenPen TriIcelon (JP)
		{ "MK-5110050", nullptr,    { 0x548E04, 0x0923C0, 0 }, { 0x43E80000, 0x3F966666 } },	// Phantasy Star Online (PAL) TODO
		{ "MK-51100",   nullptr,    { 0x548E84, 0x0925A4, 0 }, { 0x43D20000, 0x3FA66666 } },	// Phantasy Star Online (USA) TODO
		{ "HDR-0129",   nullptr,    { 0x57E344, 0x091794, 0 }, { 0x43D20000, 0x3FA66666 } },	// Phantasy Star Online (JP) TODO
		{ "MK-5119350", nullptr,    { 0x57552C, 0x0A2DD4, 0 }, { 0x43F00000, 0x3F911111 } },	// Phantasy Star Online Ver. 2 (PAL) TODO
		{ "MK-51193",   nullptr,    { 0x0A3138, 0x58C8CC, 0 }, { 0x3F911111, 0x43F00000 } },	// Phantasy Star Online Ver. 2 (USA) TODO
		{ "T1207N",     nullptr,    { 0x7D463C, 0 }, { 0x3F400000 } },		// Plasma Sword - Nightmare of Bilstein (USA)
		{ "T36801D 50", nullptr,    { 0x7FD3C8, 0 }, { 0x43700000 } },		// Power Stone (PAL)
		{ "T1201N",     nullptr,    { 0x7FCEE8, 0 }, { 0x43700000 } },		// Power Stone (USA)
		{ "T36812D 50", nullptr,    { 0x868BA8, 0 }, { 0x43700000 } },		// Power Stone 2 (PAL)
		{ "T1211N",     nullptr,    { 0x8689A8, 0 }, { 0x43700000 } },		// Power Stone 2 (USA)
		{ "T1218M",     nullptr,    { 0x849AA0, 0 }, { 0x43700000 } },		// Power Stone 2 (JP)
		{ "T7022D  50", nullptr,    { 0x33414C, 0 }, { 0x43F00000 } },		// Project Justice (PAL)
		{ "T1219N",     nullptr,    { 0x33374C, 0 }, { 0x43F00000 } },		// Project Justice (USA)
		{ "MK-51162",   nullptr,    { 0x204308, 0 }, { 0x3FE38E39 } },		// Propeller Arena
		{ "T-8106D-50", nullptr,    { 0x7DA39C, 0 }, { 0x3F400000 } },		// Psychic Force 2012 (PAL)
		{ "T9901M",     nullptr,    { 0x1AD848, 0 }, { 0x3F400000 } },		// Rainbow Cotton (JP)
		{ "T9711D  50", nullptr,    { 0x1574D8, 0 }, { 0x43700000 } },		// Ready 2 Rumble Boxing Round 2 (PAL)
		{ "T7012D  05", nullptr,    { 0x4FF93C, 0x4FF96C, 0 }, { 0x43F00000, 0x3F400000 } },	// Record of Lodoss War (En) (PAL)
		{ "xxxxxxxxxx", nullptr,    { 0x4FF25C, 0x4FF28C, 0 }, { 0x43F00000, 0x3F400000 } },	// Record of Lodoss War (De) (PAL)
		{ "T7012D  09", nullptr,    { 0x50499C, 0x5049CC, 0 }, { 0x43F00000, 0x3F400000 } },	// Record of Lodoss War (Fr) (PAL)
		{ "MK-5102151", nullptr,    { 0x3511A0, 0 }, { 0x3FC58577 } },		// Red Dog (PAL)
		{ "HDR-0074",   nullptr,    { 0x1FF60C, 0x1FF610, 0x1FF5DC, 0 }, { 0x3F400000, 0x3F800000, 0x43DC0000 } },	//Rent a Hero n°1
		// Resident Evil: Code Veronica (De) (PAL)
		// Code 1-4 removes the black bars on top and bottom in FMV
		{ "xxxxxxxxxx", nullptr,    { 0x32A380, 0x383E18, 0x383E38, 0x383E58, 0x383E78, 0 },
				{ 0x3F400000, 0x43F00000, 0, 0x43F00000, 0 } },
		// Resident Evil: Code Veronica (Fr) (PAL)
		// Code 1-4 removes the black bars on top and bottom in FMV
		{ "T36806D 09", nullptr,    { 0x32A380, 0x383E18, 0x383E38, 0x383E58, 0x383E78, 0 },
				{ 0x3F400000, 0x43F00000, 0, 0x43F00000, 0 } },
		// Resident Evil: Code Veronica (Sp) (PAL)
		{ "xxxxxxxxxx", nullptr,    { 0x32A3A0, /* 0x383E18, 0x383E38, 0x383E58, 0x383E78, */ 0 },
				{ 0x3F400000, 0x43F00000, 0, 0x43F00000, 0 } },
		// Resident Evil: Code Veronica (USA)
		// Code 1-4 removes the black bars on top and bottom in FMV
		{ "T1204N",     nullptr,    { 0x329E40, 0x3838D8, 0x3838F8, 0x383918, 0x383938, 0 },
				{ 0x3F400000, 0x43F00000, 0, 0x43F00000, 0 } },
		{ "T8107D  50", nullptr,    { 0x0464FC, 0x046210, 0 }, { 0x3A888889, 0x44200000 } },	// Re-Volt (PAL) Code 1 is a render fix
		{ "MK-5119250", nullptr,    { 0x0C5EB4, 0 }, { 0x3A888889 } },		// Rez (PAL)
		{ "T15122N",    nullptr,    { 0x8E7A80, 0x8E7AB4, 0 }, { 0x43E10000, 0x3FAAAAAB } },	// Ring, The - Terror's Realm (USA)
		{ "MK-51010",   nullptr,    { 0x1A4D2C, 0x1A4D5C, 0 }, { 0x43F00000, 0x3F400000 } },	// Rippin' Riders (USA)
		{ "HDR-0044",   nullptr,    { 0x298A94, 0x298AC4, 0 }, { 0x43F00000, 0x3F400000 } },	// Roommania #203 (JP)
		{ "MK-5109250", nullptr,    { 0x29E6C4, 0 }, { 0x3F400000 } },		// Samba De Amigo (PAL)
		{ "T41301N",    nullptr,    { 0x77A178, 0 }, { 0x3F400000 } },		// Seventh Cross Evolution (USA)
		{ "T8104D  58", nullptr,    { 0x2C03F8, 0 }, { 0x44558000 } },		// Shadow Man (PAL)
		{ "T-8106N",    nullptr,    { 0x2C03F4, 0 }, { 0x3F400000 } },		// Shadow Man (USA)
		{ "MK-51048",   nullptr,    { 0x4AA4DC, 0x2B4E30, 0 }, { 0x3F400000, 0x3F400000 } },	// Seaman (USA)
		{ "MK-5105350", nullptr,    { 0x5D613C, 0 }, { 0x3F400000 } },		// Sega GT (PAL)
		{ "MK-51096",   nullptr,    { 0x495050, 0 }, { 0x43700000 } },		// Sega Marine Fishing (USA)
//		{ "MK-51019",   nullptr,    { 0xB83A48, 0 }, { 0x3F400000 } },		// Sega Rally 2 (USA) not working?
//		{ "HDR0010",    nullptr,    { 0xBD9BA0, 0 }, { 0x3F400000 } },		// Sega Rally 2 (JP) not working?
		{ "HDR-0151",   nullptr,    { 0xAF57DC, 0xAF580C, 0x2122A0, 0 },
				{ 0x43F00000, 0x3F400000, 0x3F400000 } },					// SGGG – Segagaga (JP)
		{ "MK-5105950", nullptr,    { 0x231EF8, 0x1EF370, 0 }, { 0x43800000, 0x7C1EF400 } },	// Shenmue (PAL) code 1 reduces clipping
		{ "MK-51059",   nullptr,    { 0x230250, 0 }, { 0x43800000 } },		// Shenmue (USA) Clipping
		{ "HDR-0016",   nullptr,    { 0x22E8A0, 0x1EBE70, 0 }, { 0x43800000, 0x7C1EBF00 } },	// Shenmue (JP) code 1 reduces clipping
		{ "MK-5118450", nullptr,    { 0x31186C, 0 }, { 0x43800000 } },		// Shenmue II (PAL) 01160FF4 0000E100 for black bars in cutscenes
		// Shenmue II (PAL) Alternative code without clipping or black bars. Might be demanding on real hardware.
		// 02311880 C3A00000, 0227E198 35AA359E, 01160FF4 0000E100
		{ "HDR-0164",   nullptr,    { 0x30D67C, 0 }, { 0x43700000 } },		// Shenmue II (JP) Clipping
		{ "T9505D",     nullptr,    { 0xD1FB14, 0x096A4C, 0 }, { 0x3F400000, 0x3FAAAAAB } },	// Silent Scope (PAL) Choose 60Hz in game options
		{ "MK-51052",   "  E     ", { 0x502A84, 0 }, { 0x3F400000 } },		// Skies of Arcadia (PAL)
		{ "MK-51052",   " U      ", { 0x599158, 0 }, { 0x3F400000 } },		// Skies of Arcadia (USA)
		{ "T15104D 50", nullptr,    { 0x17EF68, 0 }, { 0x43F00010 } },		// Slave Zero (PAL) Widescreen, but a bit zoomed in
		{ "MK-5101050", nullptr,    { 0x1A50EC, 0x1A511C, 0 }, { 0x43F00000, 0x3F400000 } },	// Snow Surfers (PAL)
		{ "MK-5100050", nullptr,    { 0x88F528, 0x88F55C, 0 }, { 0x43F00000, 0x3FA66666 } },	// Sonic Adventure (PAL)
		{ "MK-51000",   nullptr,    { 0x88F5E8, 0x88F61C, 0 }, { 0x43F00000, 0x3FAAAAAB } },	// Sonic Adventure (USA)
		{ "MK-51117",   nullptr,    { 0x28DEF8, 0x28DF28, 0 }, { 0x43F00000, 0x3f400000 } },	// Sonic Adventure 2 (USA)
		{ "HDR-0165",   nullptr,    { 0x28DF28, 0x28DEF8, 0 }, { 0x3F400000, 0x43F00000 } },	// Sonic Adventure 2 (JP)
		{ "MK-51060",   nullptr,    { 0x112A2C, 0 }, { 0x3F400000 } },		// Sonic Shuffle (US)
		{ "MK-5106050", nullptr,    { 0x110B4C, 0 }, { 0x3F400000 } },		// Sonic Shuffle (PAL)
		{ "T9103M",     nullptr,    { 0x25C714, 0x25C744, 0 }, { 0x43F00000, 0x3F400000 } },	// Sorcerian - Shichisei Mahou no Shito
		{ "T1401D  50", nullptr,    { 0x2D6138, 0 }, { 0x3F400000 } },		// Soul Calibur (PAL)
		{ "T1401N",     nullptr,    { 0x266C28, 0 }, { 0x3F400000 } },		// Soul Calibur (USA)
		{ "T36802N",    "  E     ", { 0x129FA0, 0x12A9BC, 0x1C9FDC, 0 },
				{ 0x3EF55555, 0x3EF55555, 0x000000F0 } },					// Soul Reaver (PAL) Code 2 is a Render Fix
		{ "HDR-0190",   nullptr,    { 0x14D3E0, 0 }, { 0x3F400000 } },		// Space Channel 5 Part 2 (JP)
		{ "T1216M",     nullptr, { 0x017C38, 0x17F00, 0 }, { 0x3A99999A, 0x3A99999A } }, // Spawn - In the Demon's Hand v1.003 (JP)
		{ "T1216N",     nullptr, { 0x017C58, 0x17F20, 0 }, { 0x3A99999A, 0x3A99999A } }, // Spawn - In the Demon's Hand v1.000 (US)
		{ "T36816D 50", nullptr, { 0x017C78, 0x17F40, 0 }, { 0x3A99999A, 0x3A99999A } }, // Spawn - In the Demon's Hand v1.000 (EU)
		// Star Wars Episode I Racer (USA)
		// Code 1-4 removes the black bars on top and bottom in FMV
		{ "T23001N",    nullptr,    { 0x17AE20, 0x29A96C, 0x29A98C, 0x29A9AC, 0x29A9CC, 0 },
				{ 0x3F400000, 0x42900000, 0x42900000, 0x43CE0000, 0x43CE0000 } },
		{ "T40206N",    nullptr,    { 0x43296C, 0 }, { 0x3F400000, 0 } },	// Super Magnetic Neo (US)
		{ "T40206D 50", nullptr,    { 0x43E34C, 0 }, { 0x3F400000, 0 } },	// Super Magnetic Neo (EU)
//		{ "T7014D  50", nullptr,    { 0xE2B234, 0 }, { 0x3F800000 } },		// Super Runabout (PAL) doesn't work?
		{ "T17721D 50", nullptr,    { 0x45CED4, 0 }, { 0x3F400000 } },		// Surf Rocket Racers (PAL) alt: 021EBF40 3F400000
		{ "T17703D 50", nullptr,    { 0xCD8950, 0 }, { 0x3F111111 } },		// Suzuki Alstare Extreme Racing
		{ "T36807D 05", nullptr,    { 0x140F74, 0x140FA4, 0 }, { 0x43FA0000, 0x3F400000 } },	// Sword of Bersek (PAL)
		{ "T-36805N",   nullptr,    { 0x13F1C4, 0x13F194, 0 }, { 0x3F400000, 0x43F00000 } },	// Sword of Bersek (USA)
		{ "MK-51186",   nullptr,    { 0x4A19B0, 0 }, { 0x43700000 } },		// Tennis 2K2 (USA)
		{ "T15123N",    nullptr,    { 0x29B7BC, 0 }, { 0x3F400000 } },		// Test Drive Le Mans (USA) doesn't work?
		{ "T20801M",    nullptr,    { 0x1AAC80, 0x1AACB0, 0 }, { 0x43F00000, 0x3F400000 } },	// Tetris 4D (JP)
		{ "MK-5101153", nullptr,    { 0x14EFA8, 0x14EFD8, 0 }, { 0x43F00000, 0x3F400000 } },	// Timestalkers (PAL)
		{ "T7009D50",   nullptr,    { 0x39173C, 0 }, { 0x3F400000 } },		// Tech Romancer (PAL)
		{ "T35402M",    nullptr,    { 0x315370, 0x3153A0, 0 }, { 0x43F00000, 0x3F400000 } },	// Tokyo Bus Guide (JP) doesn't work?
		{ "T40201D 50", nullptr,    { 0x1D9F10, 0 }, { 0x3F400000 } },		// Tokyo Highway Challenge (PAL)
		{ "T40210D 50", nullptr,    { 0x21E4F8, 0 }, { 0x43700000 } },		// Tokyo Highway Challenge 2 (PAL)
		{ "xxxxxxxxxx", nullptr,    { 0x21DEF8, 0 }, { 0x3F400000 } },		// Tokyo Street Racer 2 (USA)
//		{ "T36804D05",  nullptr,    { 0xB75E28, 0 }, { 0x3EC00000 } },		// Tomb Raider: The Last Revelation (UK) (PAL) clipping, use hex patch instead
		{ "T13008D 05", nullptr,    { 0x1D7C20, 0 }, { 0x3FA66666 } },		// Tony Hawk's Pro Skater 2 (PAL)
		{ "T13006N",    nullptr,    { 0x1D77A0, 0 }, { 0x3FA66666 } },		// Tony Hawk's Pro Skater 2 (USA)
		{ "MK-5102050", nullptr,    { 0x0D592C, 0 }, { 0x3FD00000 } },		// Toy Commander (PAL)
		{ "xxxxxxxxxx", nullptr,    { 0x469FCC, 0x469FFC, 0 }, { 0x44700000, 0x3F400000 } },	// Toyota Doricatch Series: Land Cruiser 100/Cygnus
		{ "MK-5109505", nullptr,    { 0x1B2718, 0 }, { 0x3F400000 } },		// UEFA Dream Soccer (PAL)
		{ "T40203D 50", nullptr,    { 0x1D74E8, 0x1D7518, 0 }, { 0x43F00000, 0x3F400000 } },	// Ultimate Fighting Championship (PAL)
		{ "T40204N",    nullptr,    { 0x1A9684, 0 }, { 0x3F400000 } },		// Ultimate Fighting Championship (USA) problems with cam in game
		{ "T-8110D-50", nullptr,    { 0x0C6B90, 0x0C6B94, 0 }, { 0x43F00000, 0x43870000 } },	// Vanishing Point (PAL)
//		{ "MK-5109450", nullptr,    { 0x244134, 0x7A73B8, 0x7830D8, 0x7A74B8, 0x783138, 0x7A85A0, 0x6C7928, 0x6C7930, 0x6C7948, 0x6C7950, 0 },
//crash				{ 0x43F00000, 0x3F9C932E, 0x3F9C932E, 0x3F9C932E, 0x3F9C932E, 0x3F400000, 0, 0, 0, 0 } },	// Virtua Athlete 2K (PAL)
		{ "MK-5100150", nullptr,    { 0x19D718, 0 }, { 0x43700000 } },		// Virtua Fighter 3 TB (PAL)
		{ "HDR-0002",   nullptr,    { 0x199FB0, 0 }, { 0x43700000 } },		// Virtua Fighter 3 TB (JP)
		{ "MK-5105450", nullptr,    { 0x456378, 0 }, { 0x43700000 } },		// Virtua Tennis (v1.001) (PAL)
		{ "MK-51054",   nullptr,    { 0x450A90, 0 }, { 0x43700000 } },		// Virtua Tennis (USA)
		{ "MK-5118650", nullptr,    { 0x4A4A20, 0 }, { 0x43700000 } },		// Virtua Tennis 2 (PAL)
//		{ "T0000",      nullptr,    { 0x3A514C, 0x3A6170, 0 }, { 0x3F400000, 0x00000356 } },	// Volgarr the Viking. Not working
		{ "xxxxxxxxxx", nullptr,    { 0x20BB68, 0x1ACBD0, 0x1B9ADC, 0 },	// Code 1 reduces clipping.	Code 2 fixes the clock.
				{ 0x43700000, 0x7C1ACC60, 0x3F400000 } },					// What's Shenmue (JP)
		{ "T40504D 50", nullptr,    { 0x75281C, 0 }, { 0x3F400000 } },		// Wetrix+ (PAL) not working?
		{ "MK-51152",   nullptr,    { 0x014E90, 0 }, { 0x43700000 } },		// World Series Baseball 2K2 (USA)
		{ "T20401M",    nullptr,    { 0x323CB0, 0x1ACBD0, 0x1B9ADC, 0 },	// Code 1 reduces clipping. Code 2 fixes the HUD.
				{ 0x43700000, 0x1ACC60, 0x3F400000 } },						// Zero Gunner 2 (JP)
		{ "MK-5103850", nullptr,    { 0x9484E8, 0 }, { 0x43700000 } },		// Zombie Revenge (PAL)
		{ "MK-51038",   nullptr,    { 0x948058, 0 }, { 0x43700000 } },		// Zombie Revenge (USA)
		{ "HDR-0026",   nullptr,    { 0x948B18, 0 }, { 0x43700000 } },		// Zombie Revenge (JP)
		{ "T43301M",    nullptr,    { 0x4B0218, 0 }, { 0x3F400000 } },		// Zusar Vasar (JP)

		{ nullptr },
};
const WidescreenCheat CheatManager::naomi_widescreen_cheats[] =
{
		{ "KNIGHTS OF VALOUR  THE 7 SPIRITS", nullptr, { 0x475B70, 0x475B40, 0 }, { 0x3F400000, 0x43F00000 } },
		{ "Dolphin Blue", nullptr, { 0x3F2E2C, 0x3F2190, 0x3F2E6C, 0x3F215C, 0 },
				{ 0x43B90000, 0x3FAA9FBE, 0x43B90000, 0x43F00000 } },
		{ "METAL SLUG 6", nullptr, { 0xE93478, 0xE9347C, 0 }, { 0x3F400000, 0x3F8872B0 } },
		{ "TOY FIGHTER", nullptr, { 0x133E58, 0 }, { 0x43700000 } },
		{ "LUPIN THE THIRD  -THE SHOOTING-", nullptr, { 0x045490 }, { 0x3F400000 } },

		{ nullptr },
};
CheatManager cheatManager;

void CheatManager::loadCheatFile(const std::string& filename)
{
#ifndef LIBRETRO
	FILE* cheatfile = nowide::fopen(filename.c_str(), "r");
	if (cheatfile == nullptr)
	{
		WARN_LOG(COMMON, "Cannot open cheat file '%s'", filename.c_str());
		return;
	}
	emucfg::ConfigFile cfg;
	cfg.parse(cheatfile);
	fclose(cheatfile);

	int count = cfg.get_int("", "cheats", 0);
	cheats.clear();
	for (int i = 0; i < count; i++)
	{
		std::string prefix = "cheat" + std::to_string(i) + "_";
		Cheat cheat{};
		cheat.description = cfg.get("", prefix + "desc", "Cheat " + std::to_string(i + 1));
		cheat.address = cfg.get_int("", prefix + "address", -1);
		if (cheat.address >= RAM_SIZE)
		{
			WARN_LOG(COMMON, "Invalid address %x", cheat.address);
			continue;
		}
		cheat.type = (Cheat::Type)cfg.get_int("", prefix + "cheat_type", (int)Cheat::Type::disabled);
		cheat.size = 1 << cfg.get_int("", prefix + "memory_search_size", 0);
		cheat.value = cfg.get_int("", prefix + "value", cheat.value);
		cheat.repeatCount = cfg.get_int("", prefix + "repeat_count", cheat.repeatCount);
		cheat.repeatValueIncrement = cfg.get_int("", prefix + "repeat_add_to_value", cheat.repeatValueIncrement);
		cheat.repeatAddressIncrement = cfg.get_int("", prefix + "repeat_add_to_address", cheat.repeatAddressIncrement);
		cheat.enabled = cfg.get_bool("", prefix + "enable", false);
		cheat.destAddress = cfg.get_int("", prefix + "dest_address", 0);
		if (cheat.destAddress >= RAM_SIZE)
		{
			WARN_LOG(COMMON, "Invalid address %x", cheat.destAddress);
			continue;
		}
		cheat.valueMask = cfg.get_int("", prefix + "address_bit_position", 0);
		if (cheat.type != Cheat::Type::disabled)
			cheats.push_back(cheat);
	}
	active = !cheats.empty();
	INFO_LOG(COMMON, "%d cheats loaded", (int)cheats.size());
	cfgSaveStr("cheats", gameId, filename);
#endif
}

void CheatManager::reset(const std::string& gameId)
{
	if (this->gameId != gameId)
	{
		cheats.clear();
		active = false;
		this->gameId = gameId;
#ifndef LIBRETRO
		std::string cheatFile = cfgLoadStr("cheats", gameId, "");
		if (!cheatFile.empty())
			loadCheatFile(cheatFile);
#endif
	}
	widescreen_cheat = nullptr;
	if (!config::WidescreenGameHacks)
		return;
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		for (int i = 0; widescreen_cheats[i].game_id != nullptr; i++)
		{
			if (!strcmp(gameId.c_str(), widescreen_cheats[i].game_id)
					&& (widescreen_cheats[i].area_or_version == nullptr
							|| !strncmp(ip_meta.area_symbols, widescreen_cheats[i].area_or_version, sizeof(ip_meta.area_symbols))
							|| !strncmp(ip_meta.product_version, widescreen_cheats[i].area_or_version, sizeof(ip_meta.product_version))))
			{
				widescreen_cheat = &widescreen_cheats[i];
				NOTICE_LOG(COMMON, "Applying widescreen hack to game %s", gameId.c_str());
				break;
			}
		}
	}
	else
	{
		for (int i = 0; naomi_widescreen_cheats[i].game_id != nullptr; i++)
		{
			if (!strcmp(gameId.c_str(), naomi_widescreen_cheats[i].game_id))
			{
				widescreen_cheat = &naomi_widescreen_cheats[i];
				NOTICE_LOG(COMMON, "Applying widescreen hack to game %s", gameId.c_str());
				break;
			}
		}
	}
	if (widescreen_cheat == nullptr)
		return;
	for (size_t i = 0; i < ARRAY_SIZE(widescreen_cheat->addresses) && widescreen_cheat->addresses[i] != 0; i++)
		verify(widescreen_cheat->addresses[i] < RAM_SIZE);
}

u32 CheatManager::readRam(u32 addr, u32 bits)
{
	switch (bits)
	{
	case 8:
	default:
		return ReadMem8_nommu(0x8C000000 + addr);
	case 16:
		return ReadMem16_nommu(0x8C000000 + addr);
	case 32:
		return ReadMem32_nommu(0x8C000000 + addr);
	}
}

void CheatManager::writeRam(u32 addr, u32 value, u32 bits)
{
	switch (bits)
	{
	case 8:
	default:
		WriteMem8_nommu(0x8C000000 + addr, (u8)value);
		break;
	case 16:
		WriteMem16_nommu(0x8C000000 + addr, (u16)value);
		break;
	case 32:
		WriteMem32_nommu(0x8C000000 + addr, value);
		break;
	}
}

void CheatManager::apply()
{
	if (widescreen_cheat != nullptr)
	{
		for (size_t i = 0; i < ARRAY_SIZE(widescreen_cheat->addresses) && widescreen_cheat->addresses[i] != 0; i++)
			writeRam(widescreen_cheat->addresses[i], widescreen_cheat->values[i], 32);
	}
	if (active && !settings.network.online)
	{
		bool skipCheat = false;
		for (const Cheat& cheat : cheats)
		{
			if (skipCheat) {
				skipCheat = false;
				continue;
			}
			if (!cheat.enabled)
				continue;

			bool setValue = false;
			u32 valueToSet = 0;
			switch (cheat.type)
			{
			case Cheat::Type::disabled:
			default:
				break;
			case Cheat::Type::setValue:
				setValue = true;
				valueToSet = cheat.value;
				break;
			case Cheat::Type::increase:
				setValue = true;
				valueToSet = readRam(cheat.address, cheat.size) + cheat.value;
				break;
			case Cheat::Type::decrease:
				setValue = true;
				valueToSet = readRam(cheat.address, cheat.size) - cheat.value;
				break;
			case Cheat::Type::runNextIfEq:
				skipCheat = readRam(cheat.address, cheat.size) != cheat.value;
				break;
			case Cheat::Type::runNextIfNeq:
				skipCheat = readRam(cheat.address, cheat.size) == cheat.value;
				break;
			case Cheat::Type::runNextIfGt:
				skipCheat = readRam(cheat.address, cheat.size) <= cheat.value;
				break;
			case Cheat::Type::runNextIfLt:
				skipCheat = readRam(cheat.address, cheat.size) >= cheat.value;
				break;
			case Cheat::Type::copy:
				for (u32 i = 0; i < cheat.repeatCount; i++)
					writeRam(cheat.destAddress + i, readRam(cheat.address + i, cheat.size), cheat.size);
				break;
			}
			if (setValue)
			{
				u32 address = cheat.address;
				for (u32 repeat = 0; repeat < cheat.repeatCount; repeat++)
				{
					if (cheat.size < 8)
					{
						u8 curVal = readRam(address, 8);
						for (int i = 0; i < 8; i++)
						{
							int bitmask = 1 << i;
							if ((cheat.valueMask & bitmask) == 0)
								// keep current bit value
								valueToSet = (valueToSet & ~bitmask) | (curVal & bitmask);
						}
					}
					writeRam(address, valueToSet, cheat.size);
					address += cheat.repeatAddressIncrement * cheat.size / 8;
					valueToSet += cheat.repeatValueIncrement;
				}
			}
		}
	}
}

static std::vector<u32> parseCodes(const std::string& s)
{
	std::vector<u32> codes;
	std::string curCode;
	for (u8 c : s)
	{
		if (std::isxdigit(c))
		{
			curCode += c;
			if (curCode.length() == 8)
			{
				codes.push_back(strtol(curCode.c_str(), nullptr, 16));
				curCode.clear();
			}
		}
		else if (!curCode.empty())
			throw FlycastException("Invalid cheat code");
	}
	if (!curCode.empty())
	{
		if (curCode.length() != 8)
			throw FlycastException("Invalid cheat code");
		codes.push_back(strtol(curCode.c_str(), nullptr, 16));
	}

	return codes;
}

void CheatManager::addGameSharkCheat(const std::string& name, const std::string& s)
{
	std::vector<u32> codes = parseCodes(s);
	Cheat conditionCheat;
	unsigned conditionLimit = 0;

	for (unsigned i = 0; i < codes.size(); i++)
	{
		if (i < conditionLimit)
			cheats.push_back(conditionCheat);
		Cheat cheat{};
		cheat.description = name;
		u32 code = (codes[i] & 0xff000000) >> 24;
		switch (code)
		{
		case 0:
		case 1:
		case 2:
			{
				// 8/16/32-bit write
				if (i + 1 >= codes.size())
					throw FlycastException("Missing value");
				cheat.type = Cheat::Type::setValue;
				cheat.size = code == 0 ? 8 : code == 1 ? 16 : 32;
				cheat.address = codes[i] & 0x00ffffff;
				cheat.value = codes[++i];
				cheats.push_back(cheat);
			}
			break;
		case 3:
			{
				u32 subcode = (codes[i] & 0x00ff0000) >> 16;
				switch (subcode)
				{
				case 0:
					{
						// Group write
						int count = codes[i] & 0xffff;
						if (i + count + 1 >= codes.size())
							throw FlycastException("Missing values");
						cheat.type = Cheat::Type::setValue;
						cheat.size = 32;
						cheat.address = codes[++i] & 0x00ffffff;
						for (int j = 0; j < count; j++)
						{
							if (j == 1)
								cheat.description += " (cont'd)";
							cheat.value = codes[++i];
							cheats.push_back(cheat);
							cheat.address += 4;
							if (j < count - 1 && i < conditionLimit)
								cheats.push_back(conditionCheat);
						}
					}
					break;
				case 1:
				case 2:
					{
						// 8-bit inc/decrement
						if (i + 1 >= codes.size())
							throw FlycastException("Missing value");
						cheat.type = subcode == 1 ? Cheat::Type::increase : Cheat::Type::decrease;
						cheat.size = 8;
						cheat.value = codes[i] & 0xff;
						cheat.address = codes[++i] & 0x00ffffff;
						cheats.push_back(cheat);
					}
					break;
				case 3:
				case 4:
					{
						// 16-bit inc/decrement
						if (i + 1 >= codes.size())
							throw FlycastException("Missing value");
						cheat.type = subcode == 3 ? Cheat::Type::increase : Cheat::Type::decrease;
						cheat.size = 16;
						cheat.value = codes[i] & 0xffff;
						cheat.address = codes[++i] & 0x00ffffff;
						cheats.push_back(cheat);
					}
					break;
				case 5:
				case 6:
					{
						// 32-bit inc/decrement
						if (i + 2 >= codes.size())
							throw FlycastException("Missing address or value");
						cheat.type = subcode == 5 ? Cheat::Type::increase : Cheat::Type::decrease;
						cheat.size = 32;
						cheat.address = codes[++i] & 0x00ffffff;
						cheat.value = codes[++i];
						cheats.push_back(cheat);
					}
					break;
				default:
					throw FlycastException("Unsupported cheat type");
				}
			}
			break;
		case 4:
			{
				// 32-bit repeat write
				if (i + 2 >= codes.size())
					throw FlycastException("Missing count or value");
				cheat.type = Cheat::Type::setValue;
				cheat.size = 32;
				cheat.address = codes[i] & 0x00ffffff;
				cheat.repeatCount = codes[++i] >> 16;
				cheat.repeatAddressIncrement = codes[i] & 0xffff;
				cheat.value = codes[++i];
				cheats.push_back(cheat);
			}
			break;
		case 5:
			{
				// copy bytes
				if (i + 2 >= codes.size())
					throw FlycastException("Missing count or destination address");
				cheat.type = Cheat::Type::copy;
				cheat.size = 8;
				cheat.address = codes[i] & 0x00ffffff;
				cheat.destAddress = codes[++i] & 0x00ffffff;
				cheat.repeatCount = codes[++i];
				cheats.push_back(cheat);
			}
			break;
		// TODO 7 change decryption type
		// TODO 0xb delay applying codes
		// TODO 0xc global enable test
		case 0xd:
			{
				// enable next code if eq/neq/lt/gt
				if (i + 1 >= codes.size())
					throw FlycastException("Missing count or destination address");
				cheat.size = 16;
				cheat.address = codes[i] & 0x00ffffff;
				switch (codes[++i] >> 16)
				{
				case 0:
					cheat.type = Cheat::Type::runNextIfEq;
					break;
				case 1:
					cheat.type = Cheat::Type::runNextIfNeq;
					break;
				case 2:
					cheat.type = Cheat::Type::runNextIfLt;
					break;
				case 3:
					cheat.type = Cheat::Type::runNextIfGt;
					break;
				default:
					throw FlycastException("Unsupported conditional code");
				}
				cheat.value = codes[i] & 0xffff;
				cheats.push_back(cheat);
			}
			break;
		case 0xe:
			{
				// multiline enable codes if eq/neq/lt/gt
				if (i + 1 >= codes.size())
					throw FlycastException("Missing test address");
				cheat.size = 16;
				cheat.value = codes[i] & 0xffff;
				conditionLimit = i + 1 + ((codes[i] >> 16) & 0xff);
				switch (codes[++i] >> 24)
				{
				case 0:
					cheat.type = Cheat::Type::runNextIfEq;
					break;
				case 1:
					cheat.type = Cheat::Type::runNextIfNeq;
					break;
				case 2:
					cheat.type = Cheat::Type::runNextIfLt;
					break;
				case 3:
					cheat.type = Cheat::Type::runNextIfGt;
					break;
				default:
					throw FlycastException("Unsupported conditional code");
				}
				cheat.address = codes[i] & 0x00ffffff;
				conditionCheat = cheat;
			}
			break;
		default:
			throw FlycastException("Unsupported cheat type");
		}
	}
	active = !cheats.empty();
#ifndef LIBRETRO
	std::string path = cfgLoadStr("cheats", gameId, "");
	if (path == "")
	{
		path = get_game_save_prefix() + ".cht";
		cfgSaveStr("cheats", gameId, path);
	}
	saveCheatFile(path);
#endif
}

void CheatManager::saveCheatFile(const std::string& filename)
{
#ifndef LIBRETRO
	emucfg::ConfigFile cfg;

	cfg.set_int("", "cheats", cheats.size());
	int i = 0;
	for (const Cheat& cheat : cheats)
	{
		std::string prefix = "cheat" + std::to_string(i) + "_";
		cfg.set_int("", prefix + "address", cheat.address);
		cfg.set_int("", prefix + "address_bit_position", cheat.valueMask);
		cfg.set_bool("", prefix + "big_endian", false);
		cfg.set_int("", prefix + "cheat_type", (int)cheat.type);
		cfg.set("", prefix + "code", "");
		cfg.set("", prefix + "desc", cheat.description);
		cfg.set_int("", prefix + "dest_address", cheat.destAddress);
		cfg.set_bool("", prefix + "enable", false);	// force all cheats disabled at start
		cfg.set_int("", prefix + "handler", 1);
		int memSize;
		switch (cheat.size) {
		case 1:
			memSize = 0;
			break;
		case 2:
			memSize = 1;
			break;
		case 4:
			memSize = 2;
			break;
		case 8:
			memSize = 3;
			break;
		case 16:
			memSize = 4;
			break;
		case 32:
		default:
			memSize = 5;
			break;
		}
		cfg.set_int("", prefix + "memory_search_size", memSize);
		cfg.set_int("", prefix + "value", cheat.value);
		cfg.set_int("", prefix + "repeat_count", cheat.repeatCount);
		cfg.set_int("", prefix + "repeat_add_to_value", cheat.repeatValueIncrement);
		cfg.set_int("", prefix + "repeat_add_to_address", cheat.repeatAddressIncrement);
		i++;
	}
	FILE *fp = nowide::fopen(filename.c_str(), "w");
	if (fp == nullptr)
		throw FlycastException("Can't save cheat file");
	cfg.save(fp);
	fclose(fp);
#endif
}
