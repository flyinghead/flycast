/*
	Copyright 2026 flyinghead

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
#include <asio.hpp>
#include "maple_devs.h"
#include "oslib/oslib.h"
#include "oslib/directory.h"
#include "oslib/http_server.h"
#include "hw/sh4/sh4_mem.h"
#include "json.hpp"
#include "hw/naomi/netdimm.h"
#include "rend/texconv.h"
#include <stb_image_write.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>

#define WCCFLOG(...) INFO_LOG(MAPLE, __VA_ARGS__)

struct WccfCameraImpl : public WccfCamera
{
	struct Player
	{
		Player() = default;
		Player(u16 playerId, u32 x, u32 y)
			: playerId(playerId), x(x), y(y)
		{}

		u16 playerId;
		u32 x;
		u32 y;
		std::string name;
	};

	~WccfCameraImpl();
	MapleDeviceType get_device_type() override {
		return MDT_WccfCamera;
	}

	void OnSetup() override;
	u32 dma(u32 command) override;
	void serialize(Serializer& ser) const override;
	void deserialize(Deserializer& deser) override;
	u8 getExtDeviceMap() const override { return 0x1f; }

	MapleDeviceRV sendDeviceStatus(bool full, u8 dstAP);
	MapleDeviceRV setConditionBoot(u8 dstAP, const u8 mode);
	MapleDeviceRV setConditionA002(u8 dstAP, const u8 mode);
	MapleDeviceRV setConditionA010(u8 dstAP, const u8 mode);
	MapleDeviceRV setConditionA014(u8 dstAP, const u8 mode);

	void makeTeamFilePath() {
		Lock _(mutex);
		teamFilePath = hostfs::getArcadeFlashPath() + "-team.json";
	}
	void checkTeamFile();
	void saveTeamFile();
	void findPlayerName(Player& player);

	std::vector<Player> players;
	std::mutex mutex;
	using Lock = std::lock_guard<std::mutex>;
	int overlay = 0;
	char mode = 0;
	u8 status = '1';
	int mode0dataSize = 16;
	int condCounter = 0;
	std::string teamFilePath;
	time_t teamFileTime = 0;
	std::unique_ptr<asio::io_context> io_context;
	std::unique_ptr<HttpServer> httpServer;
	std::thread httpThread;

	static constexpr size_t MAX_CARDS = 16;
};

std::shared_ptr<maple_device> WccfCamera::Create() {
	return std::make_shared<WccfCameraImpl>();
}

MapleDeviceRV WccfCameraImpl::sendDeviceStatus(bool full, u8 dstAP)
{
	if (dstAP & 0x1f)
		w32(0);
	else
		w32(MFID_0_Input);
	w32(0);
	w32(0);
	w32(0);
	w8(0xff);
	w8(0);
	if (dstAP & 0x1f)
		wstr("Card Ban Reader LMDevice 1.00", 30);
	else
		wstr("Card Ban Reader Device 1.00", 30);
	wstr("Produced By or Under License From SEGA ENTERPRISES,LTD.", 60);
	w16(0);
	w16(0);

	if (full)
	{
		if (dstAP & 0x1f)
		{
			if (overlay == 0)
			{
				wstr("Version 1.211,2005/11/21,315-6341-B        ,", 44);
				w32(0);
				// total 160 bytes
			}
			else
			{
				// overlay A002, A010
				w32(3);
				w32(0);
				memset(dma_buffer_out, 0, 512);
				*dma_count_out += 512;
				// total 632 bytes
			}
		}
		else
		{
			wstr("Version 1.000,2002/04/29,315-6341-A        ,", 44);
			if (overlay == 0)
			{
				w32(mode);
				if (mode == 'J') {
					for (int i = 0; i < 70; i++)
						w32(0);
					// total 440 bytes
				}
				else if (mode == 'P') {
					for (int i = 0; i < 5; i++)
						w32(0);
					// total 180 bytes
				}
			}
			else if (overlay == 0xa002)
			{
				w32(mode);
				switch (mode)
				{
				case 0x5c:
					memset(dma_buffer_out, 0, 292);
					*dma_count_out += 292;
					// total 448 bytes
					break;
				case 'J':
				case 'K':
					memset(dma_buffer_out, 0, 284);
					*dma_count_out += 284;
					// total 440 bytes
					break;
				default:
					w32(0);
					// total 160 bytes
					break;
				}
			}
			else
			{
				// overlay A010
				w8(mode);
				switch (mode)
				{
				case 'C':
				case 'D':
					memset(dma_buffer_out, 0, 283);
					*dma_count_out += 283;
					// total 440 bytes
					break;
				case 'F':
					memset(dma_buffer_out, 0, 227);
					*dma_count_out += 227;
					// total 384 bytes
					break;
				case 'G':
					memset(dma_buffer_out, 0, 267);
					*dma_count_out += 267;
					// total 424 bytes
					break;
				case 'H':
					memset(dma_buffer_out, 0, 259);
					*dma_count_out += 259;
					// total 416 bytes
					break;
				case 'J':
					memset(dma_buffer_out, 0, 283);
					*dma_count_out += 283;
					// total 440 bytes
					break;
				case 7:
					memset(dma_buffer_out, 0, 19);
					*dma_count_out += 19;
					// total 176 bytes
					break;
				default:
					{
						Lock _(mutex);
						w8(players.size());	// card count
						w8(0);				// constant
						w8(0xff);
						w16(42);
						w16(48);
						w16(41);
						w16(469);
						w16(594);
						w16(46);
						w16(596);
						w16(468);
#ifdef HARDCODED_TEAM
						for (int i = 0; i < mode0dataSize; i++)
						{
							if (i < 11)
							{
								// pitch
								w16((i % 4) * 120 + 100);	// y
								w16((i / 4) * 120 + 120);	// x
							}
							else
							{
								// bench
								w16((i - 11) * 80 + 100);
								w16(0);
							}
							w16(0);
							// test cards
							//w16(i);
							// real cards
							// card ranges:
							// 24-130, 201-509, 550-581, 582-595*, 600-887, 888-939*, 940-971, 972-1019*, 1020-1023*
							// 1100-1323, 1324-1355*, 1356-1367*,
							// 2005-2006 edition:
							// 	 1368*, 1401-1736, 1737-1778*, 1779-1788*
							// (* indicates the index also has bit 14 set)
							w16(i + 1401);
						}
#else
						for (Player& p : players)
						{
							w16(p.y);
							w16(p.x);
							w16(0); // not used
							w16(p.playerId);
						}
#endif
						break;
					}
				}
			}
		}
		return MDRS_DeviceStatusAll;
	} else {
		return MDRS_DeviceStatus;
	}
}

MapleDeviceRV WccfCameraImpl::setConditionBoot(u8 dstAP, const u8 mode)
{
	if (dstAP & 0x1f)
		return MDRE_UnknownCmd;
	switch (mode)
	{
	case 0:
		// set version 0, reset
		this->mode = 0;
		status = '1'; // self test successful
		break;
	case 'P': // cam pos?
		// set version P, reset
		this->mode = 'P';
		status = '2';
		break;
	case '0': // select flash section
		// Copy the selected FLASH area to RAM and execute it.
		switch (r8())
		{
		case 0:
		default:
			WCCFLOG("Switching to overlay A010 (card reader)");
			overlay = 0xa010;
			status = 2; // 5 -> 2
			this->mode = 0;
			break;
		case 1:
			overlay = 0xa002;
			status = '3';
			this->mode = 0;
			WCCFLOG("Switching to overlay A002 (firmware update)");
			break;
		case 2:
			overlay = 0xa014;
			this->mode = 0;
			status = 0x82; // 0x85 -> 0x82
			WCCFLOG("Switching to overlay A014 (settings)");
			break;
		}
		break;
	default:
		return MDRE_UnknownCmd;
	}
	return MDRS_DeviceReply;
}

MapleDeviceRV WccfCameraImpl::setConditionA002(u8 dstAP, const u8 mode)
{
    if (dstAP & 0x1f)
    {
    	if (mode == 0x3c)
    	{
    		if (status == '3' || status == 0x3b || status == 0x3c)
    		{
    			/*
    			u8 count = r8();
    			r16();
    			u32 p2 = r32(); // address
    			u32 p3 = r32(); // u32[count]
    			WCCFLOG("A002[%x] mode 3c params: %x %x %x...", dstAP, count, p2, p3);
    			*/
    		}
    		return MDRS_DeviceReply;
    	}
    	// also mode 1 and 7 on Ext dev 1 only
    	ERROR_LOG(MAPLE, "Mode %x [%c] (ext) not handled", mode, mode);
    }
    else
    {
    	switch (mode)
    	{
    	case '8':
    		if (status == '3')
    		{
    			u32 p1 = r8(); // count
    			p1 += r8() << 8;
    			p1 += r8() << 16;
    			u32 p2 = r32();	// address
    			u32 p3 = r32(); // value
    			WCCFLOG("A002 mode 8 params: %x %x %x", p1, p2, p3);
    			status = '8'; // then switches to '3' after data xfer from main device
    			status = '3';
    		}
    		break;
    	case 0x3f:
    		if (status == '3')
    		{
    			u32 p1 = r8();
    			WCCFLOG("A002 mode 3F param: %x", p1);
    			switch (p1)
    			{
    			case 1:
    			case 2:
    			case 3:
    			case 4:
    			case 5:
    			case 6:
    			default:
    				break;
    			}
				status = 0x3f;
    		}
    		break;
    	default:
    		ERROR_LOG(MAPLE, "Mode %x [%c] not handled", mode, mode);
    		return MDRE_UnknownCmd;
    	}
    	return MDRS_DeviceReply;
    }
	return MDRE_UnknownCmd;
}

MapleDeviceRV WccfCameraImpl::setConditionA010(u8 dstAP, const u8 mode)
{
	if ((dstAP & 0x1f) == 1)
	{
		// Ext device 1
		switch (mode)
		{
		case 0xc:
			// TODO additional condition: DAT_8c024ed5 = 1
			if (this->mode == 8)
				this->mode = 3;
			break;
		case 0xd:
			// TODO additional condition: DAT_8c024ed5 = 1 (set by setCond 10 or 11 on main)
			// FIXME freeze
			if (this->mode == 3 && false) {
				this->mode = 8;
				status = 12;
			}
			break;
		case 1:
			// reset imgBlock# and more...
			break;
		default:
			return MDRE_UnknownCmd;
		}
	}
	else
	{
		// Main device
		switch (mode)
		{
		case 0:
//			mode0dataSize = r8();
//			this->mode = 0;
			//status = 2;
//			status = '1'; // TODO after successful self tests?
			break;
		case 3:
			// if status==2 &&  DAT_8c024ed2 != 1 && DAT_8c024ed5 != 1 then DAT_8c024ed5 = 1
			break;
		case 4:
			this->mode = 0;
			if (status != 2)
				status = 2; // 5 -> 2
				// sets SPC -> execute special code after int handler?
				//	init stack ptr, DAT_8c024ed2=0, DAT_8c024ed3=0, mode=0, DAT_8c024ed5=0, DAT_8c024ed6=1, status=5
				//  status 5 -> 2
			break;
		case 7:
			if (status == 2) // && DAT_8c024ed2 != 1
				this->mode = 7;
			break;
		case 8:
			if (status == 2) // && DAT_8c024ed2 != 1 && DAT_8c024ed5 != 1
				//  FUN_8c011b6e(1)
				this->mode = 8;
			break;
		case 10: // detect cards
			if (status == 2) // && DAT_8c024ed2 != 1 && DAT_8c024ed5 != 1
			{
				this->mode = 3;
				// we need to alternate GetCond with status 11 and 12
				status = 11; // -> 10 -> 11 -> 12
				// DAT_8c024ed5 = 1
			}
			break;
		case 11:
			if (status == 2) // && DAT_8c024ed2 != 1 && DAT_8c024ed5 != 1
				this->mode = 11;
			break;
		case 12:
			if (mode == 8) // && DAT_8c024ed5 == 1
				this->mode = 3;
			break;
		case 13:
			if (mode == 3) // && DAT_8c024ed5 == 1
				this->mode = 8;
			break;
		case 'C':
			if (status == 2) // && FUN_8c011b6e() != 1
				this->mode = 'C';
			break;
		case 'D':
			if (status == 2) // && DAT_8c024ed2 != 1
				this->mode = 'D';
			break;
		case 'E':
			//if (status == 0x82 || status == 2) // && DAT_8c024ed2 != 1
			//	FUN_8c01a538(&DAT_8c024ad6);
			break;
		case 'F':
			if (status == 2) // && DAT_8c024ed2 != 1
				this->mode = 'F';
			break;
		case 'G':
			if (status == 2) // && DAT_8c024ed2 != 1
				this->mode = 'G';
			break;
		case 'H':
			if (status == 2) // && FUN_8c011b6e() != 1
				this->mode = 'H';
			break;
		case 'J':
			{
				//u8 b = r8(); // TODO 0, 1, 2, 3 or else...
				this->mode = 'J';
				break;
			}
		case 'L':
			if (status == 2)
				status = 'L';
			break;
		case 'M':
			if (status == 2)
				status = 'L';
			break;
		case 'N':
			if (status == 2)
				status = 'L';
			break;
		case 0x81:
			if (status == 2) // && FUN_8c011b6e() != 1
				this->mode = 0x81;
			break;
		default:
			ERROR_LOG(MAPLE, "Mode %x [%c] not handled", mode, mode != 0 ? mode : ' ');
			return MDRE_UnknownCmd;
		}
	}
	return MDRS_DeviceReply;
}

MapleDeviceRV WccfCameraImpl::setConditionA014(u8 dstAP, const u8 mode)
{
	if (dstAP & 0x1f)
	{
		// TODO
		ERROR_LOG(MAPLE, "Mode %x [%c] (ext) not handled", mode, mode);
	}
	else
	{
		switch (mode)
		{
		case 'F':
			if (status == 0x82) { // && FUN_8c011d30() != 1
				this->mode = 'F';
				status = 0x84; // -> 0x83 -> 0x84
			}
			else {
				WCCFLOG("mode F requested but status is %x", status);
			}
			break;
		default:
    		ERROR_LOG(MAPLE, "Mode %x [%c] not handled", mode, mode);
			return MDRE_UnknownCmd;
		}
		return MDRS_DeviceReply;
	}
	return MDRE_UnknownCmd;
}

u32 WccfCameraImpl::dma(u32 command)
{
	const u32 reci = dma_buffer_in[-3];
	switch (command)
	{
	case MDC_DeviceRequest:
	case MDC_AllStatusReq:
		WCCFLOG("[%d] Cam %s. reci %x", bus_id, command == MDC_DeviceRequest ? "DeviceRequest" :  "AllStatusReq", reci);
		return sendDeviceStatus(command == MDC_AllStatusReq, reci);

	case MDCF_GetCondition:
		w32(MFID_0_Input);
		w8(status);
		w8(0);
		w16(0);
		w32(0);
		if (mode == 3 && (status == 11 || status == 12))
		{
			// alternate between status 11 and 12
			if (condCounter % 3 == 0)
				status = 11;
			else
				status = 12;
			condCounter++;
			WCCFLOG("[%d] Cam GetCondition. sz %d reci %x mode 3 status %d", bus_id, dma_count_in, reci, status);
		}
		return MDRS_DataTransfer;

	case MDCF_SetCondition:
	{
		r32(); // function
		u8 newMode = r8();
		if (newMode != 0x3c || (reci & 0x1f) == 0)
			WCCFLOG("[%d] Cam SetCondition: sz %d reci %x mode %02x", bus_id, dma_count_in, reci, newMode);
		switch (overlay)
		{
		case 0xa002: return setConditionA002(reci, newMode);
		case 0xa010: return setConditionA010(reci, newMode);
		case 0xa014: return setConditionA014(reci, newMode);
		default:     return setConditionBoot(reci, newMode);
		}
	}

	default:
		return WccfCamera::dma(command);
	}

}

void WccfCameraImpl::serialize(Serializer& ser) const
{
	WccfCamera::serialize(ser);
	ser << overlay;
	ser << mode;
	ser << status;
	ser << mode0dataSize;
}

void WccfCameraImpl::deserialize(Deserializer& deser)
{
	WccfCamera::deserialize(deser);
	deser >> overlay;
	deser >> mode;
	deser >> status;
	deser >> mode0dataSize;
}

static int getPlayerIndex(int playerId)
{
	static constexpr int CardRanges420[][3] = {
		// 2001-2002 (462 cards)
		{ 0x18, 0x83, 0 },
		{ 0xc9, 0x1fe, 0 },
		{ 0x226, 0x246, 0 },
		{ 0x246, 0x254, 1 },
		// 2002-2003 (340 cards)
		{ 0x258, 0x378, 0 },
		{ 0x378, 0x3ac, 1 },
		// 2004-2005 (352 cards)
		{ 0x3ac, 0x3cc, 0 },
		{ 0x3cc, 0x3fc, 1 },
		{ 0x3fc, 0x400, 1 },
		{ 0x44c, 0x52c, 0 },
		{ 0x52c, 0x54c, 1 },
		{ 0x54c, 0x558, 1 },
		// 2005-2006 (389 cards)
		{ 0x558, 0x559, 1 },
		{ 0x579, 0x6c9, 0 },
		{ 0x6c9, 0x6f3, 1 },
		{ 0x6f3, 0x6fd, 1 },
	};

	const int baseIndex = playerId & 0x3fff;
	int newIdx = 1;
	unsigned range = 0;
	while (true)
	{
		if (baseIndex >= CardRanges420[range][0]
				&& baseIndex < CardRanges420[range][1])
			break;
		newIdx += CardRanges420[range][1] - CardRanges420[range][0];
		range++;
		if (range == std::size(CardRanges420))
			return -1;
	}
	if (((playerId >> 14) & 1) != CardRanges420[range][2])
		return -1;
	return newIdx + baseIndex - CardRanges420[range][0];
}

void WccfCameraImpl::findPlayerName(Player& player)
{
	if (player.playerId == 0xffff) {
		player.name.clear();
		return;
	}
	if (player.playerId <= 15) {
		player.name = "TEST CARD " + std::to_string(player.playerId + 1);
		return;
	}
	int idx = getPlayerIndex(player.playerId);
	if (idx <= 0) {
		INFO_LOG(MAPLE, "Invalid player ID %x", player.playerId);
		player.name = "PID:" + std::to_string(player.playerId);
		return;
	}
	u32 playerTable, valueCheck, size;
	switch (settings.content.fileName[4])
	{
	case '1': // 2001-2002
		playerTable = 0x0c186d84;
		valueCheck = 0x0c14e1bc;
		size = 0x18;
		break;
	case '2': // 2002-2003 TODO 234j
		playerTable = 0x0c1e9dcc;
		valueCheck = 0x0c1b7948;
		size = 0x1c;
		break;
	case '3': // 2004-2005 TODO wccf310j, wccf331j, wccf341j
		if (settings.content.fileName[5] == '2')
		{
			// 322e
			playerTable = 0x0c202ecc;
			valueCheck = 0x0c1c88ec;
			size = 0x24;
		}
		else if (settings.content.fileName[5] == '3')
		{
			// 331e
			playerTable = 0x0c2039e0;
			valueCheck = 0x0c1c8ff0;
			size = 0x24;
		}
		else {
			return;
		}
		break;
	case '4': // 2005-2006 TODO wccf400j
		playerTable = 0x0c20b448;
		valueCheck = 0x0c1ce448;
		size = 0x24;
		break;
	default:
		return;
	}
	if (addrspace::read32(playerTable) != valueCheck)
		// player table not loaded yet or unsupported version
		return;
	u32 addr = playerTable + idx * size;
	u32 nameAddr = addrspace::read32(addr);
	u8 *name = GetMemPtr(nameAddr, 1);
	if (name == nullptr)
	{
		INFO_LOG(MAPLE, "Can't find name of player ID %x", player.playerId);
		player.name = "PID:" + std::to_string(player.playerId);
		return;
	}
	player.name.clear();
	while (*name)
	{
		switch (*name)
		{
		case 0xA7: player.name += "Á"; break;
		case 0xA9: player.name += "Á"; break;
		case 0xAA: player.name += "Ä"; break;
		case 0xAB: player.name += "Ć"; break;
		case 0xAC: player.name += "Ç"; break;
		case 0xAD: player.name += "È"; break;
		case 0xAE: player.name += "É"; break;
		case 0xB2: player.name += "Í"; break;
		case 0xB4: player.name += "Ï"; break;
		case 0xB6: player.name += "Ñ"; break;
		case 0xB7: player.name += "Ò"; break;
		case 0xB8: player.name += "Ó"; break;
		case 0xBA: player.name += "Õ"; break;
		case 0xBB: player.name += "Ö"; break;
		case 0xBC: player.name += "Ø"; break;
		case 0xBE: player.name += "Ú"; break;
		case 0xC0: player.name += "Ü"; break;
		case 0xC1: player.name += "Ñ"; break;
		case 0xC3: player.name += "á"; break;
		default:
			player.name += (char)*name;
		}
		name++;
		if (player.name.length() >= 32)
			break;
	}
}

void WccfCameraImpl::checkTeamFile()
{
	makeTeamFilePath();
	struct stat st;
	if (flycast::stat(teamFilePath.c_str(), &st)) {
		DEBUG_LOG(MAPLE, "wccf_team.json file not found");
		return;
	}
	if (teamFileTime == st.st_mtime)
		return;
	teamFileTime = st.st_mtime;
	FILE *tf = nowide::fopen(teamFilePath.c_str(), "rt");
	if (tf == nullptr) {
		WARN_LOG(MAPLE, "Can't open %s", teamFilePath.c_str());
		return;
	}
	std::string all_data;
	char buf[4096];
	while (true)
	{
		int s = fread(buf, 1, sizeof(buf), tf);
		if (s <= 0)
			break;
		all_data.append(buf, s);
	}
	fclose(tf);
	using namespace nlohmann;
	try {
		json v = json::parse(all_data);
		Lock _(mutex);
		players.clear();
		for (const auto& o : v)
		{
			u16 pid = 0xffff;
			try {
				pid = o.at("pid").get<u16>();
			} catch (const json::exception& e) {
			}
			u32 x = 0;
			try {
				x = o.at("x").get<u32>();
			} catch (const json::exception& e) {
			}
			u32 y = 0;
			try {
				y = o.at("y").get<u32>();
			} catch (const json::exception& e) {
			}
			std::string name;
			try {
				name = o.at("name").get<std::string>();
			} catch (const json::exception& e) {
			}
			players.emplace_back(pid, x, y);
			players.back().name = name;
			findPlayerName(players.back());
		}
	} catch (const json::exception& e) {
		WARN_LOG(MAPLE, "Corrupted team file: %s", e.what());
	}
}

void WccfCameraImpl::saveTeamFile()
{
	makeTeamFilePath();
	FILE *tf = nowide::fopen(teamFilePath.c_str(), "wt");
	if (tf == nullptr) {
		WARN_LOG(MAPLE, "Can't open %s for saving", teamFilePath.c_str());
		return;
	}
	using namespace nlohmann;
	json array = json();
	{
		Lock _(mutex);
		for (const Player& player : players)
		{
			json jp = {
				{ "pid", player.playerId },
				{ "x", player.x },
				{ "y", player.y },
				{ "name", player.name },
			};
			array.push_back(jp);
		}
	}
	std::string data = array.dump(-1, ' ', false, json::error_handler_t::replace);
	fwrite(data.c_str(), 1, data.size(), tf);
	fclose(tf);
	struct stat st;
	if (flycast::stat(teamFilePath.c_str(), &st) == 0)
		teamFileTime = st.st_mtime;
}

void WccfCameraImpl::OnSetup()
{
	WccfCamera::OnSetup();
	players.reserve(MAX_CARDS);
	checkTeamFile();
	io_context = std::make_unique<asio::io_context>();
	try {
		httpServer = std::make_unique<HttpServer>(*io_context, "localhost", 17222);
		httpServer->addUriHandler("/api/team", [this](const Request& req, Reply& rep)
			{
				rep.addHeader("Access-Control-Allow-Origin", "*");
				rep.addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, DELETE");
				rep.addHeader("Access-Control-Allow-Headers", "*");
				using namespace nlohmann;
				if (req.method == "GET")
				{
					checkTeamFile();
					// Note: if team file is loaded before the game player table,
					// player names will not be checked against playerId
					json array = json();
					{
						Lock _(mutex);
						for (const Player& player : players)
						{
							json jp = {
								{ "pid", player.playerId },
								{ "x", player.x },
								{ "y", player.y },
								{ "name", player.name },
							};
							array.push_back(jp);
						}
					}
					rep.setContent(array.dump(-1, ' ', false, json::error_handler_t::replace),
							"application/json");
				}
				else if (req.method == "POST")
				{
					try {
						json v = json::parse(req.content);
						for (const auto& o : v)
						{
							u16 pid = 0xffff;
							try {
								pid = o.at("pid").get<u16>();
							} catch (const json::exception& e) {
							}
							if (pid == 0xffff)
								continue;
							u32 x = 0;
							try {
								x = o.at("x").get<u32>();
							} catch (const json::exception& e) {
							}
							u32 y = 0;
							try {
								y = o.at("y").get<u32>();
							} catch (const json::exception& e) {
							}
							{
								Lock _(mutex);
								bool found = false;
								for (Player& player : players)
								{
									if (player.playerId == pid)
									{
										player.x = x;
										player.y = y;
										found = true;
										break;
									}
								}
								if (!found && players.size() < MAX_CARDS) {
									players.emplace_back(pid, x, y);
									findPlayerName(players.back());
								}
							}
						}
						saveTeamFile();
						rep.status = Reply::ok;
					} catch (const json::exception& e) {
						WARN_LOG(MAPLE, "Invalid json request: %s", e.what());
						rep.status = Reply::internal_server_error;
					}
				}
				else if (req.method == "DELETE")
				{
					if (req.uri == "/api/team") {
						// Delete all players
						Lock _(mutex);
						players.clear();
					}
					else
					{
						// Delete one player
						auto slash = req.uri.rfind('/');
						if (slash == std::string::npos) {
							rep.status = Reply::not_found;
							return;
						}
						int playerId = atoi(req.uri.substr(slash + 1).c_str());
						Lock _(mutex);
						players.erase(std::remove_if(players.begin(), players.end(),
								[playerId](const Player& p) { return p.playerId == playerId; }), players.end());
					}
					saveTeamFile();
					rep.status = Reply::ok;
				}
				else if (req.method == "OPTIONS") {
					rep.status = Reply::ok;
				}
			});
		httpServer->addUriHandler("/image/", [this](const Request& req, Reply& rep)
			{
				for (const Header& header : req.headers)
				{
					std::string name = header.name;
					string_tolower(name);
					if (name == "if-none-match") {
						rep.status = Reply::not_modified;
						return;
					}
				}
				auto slash = req.uri.rfind('/');
				if (slash == std::string::npos) {
					rep = Reply::stockReply(Reply::not_found);
					return;
				}
				auto dot = req.uri.find(slash + 1, '.');
				if (dot == std::string::npos)
					dot = req.uri.size();
				int playerId = atoi(req.uri.substr(slash + 1, dot - slash - 1).c_str());
				int idx = getPlayerIndex(playerId);
				if (idx <= 0) {
					rep = Reply::stockReply(Reply::not_found);
					return;
				}

				/*
				 * TODO
				 * wccf234j: (2002-2003)
				 * wccf310j: (2004-2005)
				 * wccf331j: (2004-2005, v1.1)
				 * wccf400j: (2005-2006)
				 */
				u32 offset;
				switch (settings.content.fileName[4])
				{
				case '1':
					// file "models/sprite_card_data.bin"
					offset = 0x31AAB00 + idx * 0x8000;
					break;
				case '2':
					// file "models/sprite_card_data.bin"
					offset = 0x3386920 + idx * 0x8000;
					break;
				case '3':
					// file "data/model/sprite_card_data.bin"
					// wccf322e and wccf331e have same offset
					offset = 0x34A4A00 + idx * 0x5b00;
					break;
				case '4':
					// file "data/model/sprite_card_data.bin"
					offset = 0x3726480 + idx * 0x1800;
					break;
				default:
					rep = Reply::stockReply(Reply::not_found);
					return;
				}
				const u8 *picData = ((NetDimm *)CurrentCartridge)->getDimmData(offset);

				PixelBuffer<u32> pb;
				pb.init(128, 128);
				switch (settings.content.fileName[4])
				{
				case '1':
				case '2':
					opengl::tex565_TW32(&pb, picData, 128, 128);
					break;
				case '3':
					{
						opengl::tex565_PL32(&pb, picData, 128, 128);
						// Image is rotated left 90° and only 91 px are used to save some space
						PixelBuffer<u32> pb2;
						pb2.init(128, 128);
						for (int y = 0; y < 128; y++) {
							for (int x = 0; x < 128; x++)
								*pb2.data(x, y) = *pb.data(127 - y, x);
						}
						pb.steal_data(pb2);
						break;
					}
				case '4':
					pb.setVQCodebook(picData);
					opengl::tex565_VQ32(&pb, picData + 256 * 8, 128, 128);
					break;
				default:
					rep = Reply::stockReply(Reply::not_found);
					return;
				}
				stbi_flip_vertically_on_write(1);
				const auto& savefunc = [](void *context, void *data, int size) {
					std::string *content = (std::string *)context;
					*content += std::string((char *)data, (char *)data + size);
				};
				std::string content;
				stbi_write_png_to_func(savefunc, &content, 128, 128, 4, pb.data(), 0);
				rep.setContent(content, "image/png");
				rep.addHeader("ETag", settings.content.fileName + "-" + std::to_string(playerId));
			});
		httpServer->addUriHandler("/version", [this](const Request& req, Reply& rep)
			{
				rep.addHeader("Access-Control-Allow-Origin", "*");
				rep.addHeader("Access-Control-Allow-Methods", "GET");
				rep.addHeader("Access-Control-Allow-Headers", "*");
				auto dot = settings.content.fileName.rfind('.');
				if (dot == std::string::npos)
					dot = settings.content.fileName.size();
				rep.setContent(settings.content.fileName.substr(4, dot - 4), "text/plain");
			});
		httpThread = std::thread([this]() {
			try {
				io_context->run();
			} catch (const std::exception& e) {
				ERROR_LOG(MAPLE, "WCCF web server error: %s", e.what());
			}
		});
	} catch (const std::exception& e) {
		ERROR_LOG(MAPLE, "Can't start WCCF web server: %s", e.what());
	}
}

WccfCameraImpl::~WccfCameraImpl()
{
	if (httpThread.joinable()) {
		io_context->stop();
		httpThread.join();
	}
}
