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
#include "types.h"
#include "emulator.h"
#include "hw/modem/modem.h"
#include "netservice.h"
#include "rawmodem.h"
#include "hw/mem/addrspace.h"
#include <stdlib.h>

//
// Emulates the Capcom Direct gaming service.
// After being matched, the two opponents' consoles disconnect from the lobby server.
// Then one peer sets his modem in answer mode and the other one calls it directly.
// Once the match is over, they disconnect and reconnect to the lobby server.
//
// Emulating all this requires a complex "dance" involving a special network service and server
// and a tight relationship with the modem.
// 1. Once a capcom direct game disconnects from the network, two modem callbacks are installed:
//    one when dialing out and one when detecting the modem is in answer mode.
// 2. The modem detects a dial out with a specific number, or detects answer mode.
//    The caller keeps the modem ringing.
// 3. Both peers connect to the battle server and register: game, caller/callee, battle code
// 4. The battle server matches the 2 connections and send a START message to the callee.
//    From this point on, the server only pipes data between peers without interfering.
// 5. On receiving START, the callee makes the phone ring.
// 6. The game detects the phone ringing and picks up the call to start handshaking/negotiation
// 7. The callee sends START to the caller, prompting him to start modem handshaking/negotiation
// 8. Both peers start exchanging game data

// TODO callee modem sometimes freezes when other party leaves
// need to detect disconnect and set modem regs?
// modem hang up still not detected...

// TODO some chars left in modem or buffer when starting a new match (invalid packet received from server/peer, mvsc2)

// TODO intermittent lock up at battle start after PING/PONG/START (zero3, mvsc2?)

// Capcom Direct games
//  1 Marvel vs. Capcom 2				T1215M
//  2 Power Stone 2						T1218M
//  3 Street Fighter III 3rd Strike		T1209M
//  4 Street Fighter Zero 3 for M.S.	T1230M
//  5 Spawn								T1216M
//  6 Vampire Chronicle for M.S.		T1235M
//  7 Netto De Tennis					T1234M
//  8 Capcom vs. SNK Pro				T1247M
//  9 Jojo's Bizarre Adventure for M.S. T1231M
// 10 Project Justice					T1221M
// 11 Super Street Fighter IIX for M.S.	T1236M
// 12 Tech Romancer for M.S.			T1232M
// 13 Taisen Net Gimmick				T1248M
// 14 Heavy Metal: Geomatrix			T1246M
// 15 Super Puzzle Fighter IIX for M.S.	T1250M

namespace net::modbba
{

const std::string games[] {
		"T1215M", "T1218M", "T1209M", "T1230M", "T1216M", "T1235M",
		"T1234M", "T1247M", "T1231M", "T1221M", "T1236M", "T1232M",
		"T1248M", "T1246M", "T1250M"
};

class CapcomDirectService;

class CapcomDirect
{
public:
	CapcomDirect();
	~CapcomDirect();

private:
	void init();
	void term();
	void onLoadGame();
	void startCapcomDirectService();
	void stopCapcomDirectService();
	void onNetworkChange();
	static void onEvent(Event event, void *arg);
	void resetModemCallbacks();
	std::string getBattleCode();

	bool active = false;
	bool caller = false;
	uint8_t gameId = 0;
	std::unique_ptr<CapcomDirectService> capcomService;
};

static CapcomDirect capcom;

class CapcomDirectService : public RawModemService
{
public:
	using super = RawModemService;

	CapcomDirectService()
		: RawModemService(7658)
	{}
	~CapcomDirectService() {
		super::stop();
	}

	void sendRegister(u8 gameId, bool caller, u32 battleCode)
	{
		u8 packet[12] {
				0xBA, 0x11, 0x1E, 0x01, 1, 6,
				gameId, caller
		};
		memcpy(&packet[8], &battleCode, 4);
		for (u8 b : packet)
			super::writeModem(b);
		recvBuf.clear();
	}

	void sendStart()
	{
		for (u8 b : startPacket)
			super::writeModem(b);
		startSync = true;
	}

	bool receiveStart()
	{
		while (super::modemAvailable())
		{
			recvBuf.push_back(super::readModem());
			if (recvBuf.size() == 6)
				break;
		}
		if (recvBuf.size() < 6)
			return false;

		bool result = !memcmp(startPacket, recvBuf.data(), recvBuf.size());
		if (!result)
		{
			WARN_LOG(NETWORK, "Received unknown packet from battle server/peer (size %zd)", recvBuf.size());
			/*
			for (u8 b : recvBuf)
				printf(" %02x", b);
			printf("\n");
			*/
		}
		else {
			startSync = true;
		}
		recvBuf.clear();
		return result;
	}

	void writeModem(u8 b) override
	{
		if (startSync && b != 0xa && (b < 'A' || b > 'Z'))
		{
			super::writeModem(b);
			// Crude initial synchronization
			u64 t0 = getTimeMs();
			while (!super::modemAvailable() && getTimeMs() -  t0 < 500)
				;
			startSync = false;
		}
		else {
			super::writeModem(b);
		}
	}
	int readModem() override
	{
		int c = super::readModem();
		if (startSync && c != -1 && c != 0xa && (c < 'A' || c > 'Z'))
			startSync = false;
		return c;
	}

private:
	std::vector<u8> recvBuf;
	bool startSync = false;
	static constexpr u8 startPacket[] { 0xBA, 0x11, 0x1E, 0x01, 2, 0 };
};

CapcomDirect::CapcomDirect() {
	init();
}
CapcomDirect::~CapcomDirect() {
	term();
}
void CapcomDirect::init()
{
	EventManager::listen(Event::Start, onEvent, this);
	EventManager::listen(Event::Terminate, onEvent, this);
	EventManager::listen(Event::Network, onEvent, this);
	EventManager::listen(Event::LoadState, onEvent, this);
}
void CapcomDirect::term()
{
	EventManager::unlisten(Event::Start, onEvent, this);
	EventManager::unlisten(Event::Terminate, onEvent, this);
	EventManager::unlisten(Event::Network, onEvent, this);
	EventManager::unlisten(Event::LoadState, onEvent, this);
}
void CapcomDirect::onLoadGame()
{
	gameId = 0;
	for (unsigned i = 0; i < std::size(games); i++)
		if (games[i] == settings.content.gameId) {
			gameId = i + 1;
			break;
		}
	active = gameId != 0;
}

void CapcomDirect::startCapcomDirectService()
{
	INFO_LOG(NETWORK, "CapcomDirectService started");
	capcomService = std::make_unique<CapcomDirectService>();
	setCustomService(capcomService.get());
	capcomService->start();
	std::string battleCode = getBattleCode();
	INFO_LOG(NETWORK, "Capcom battle code: %s (%s)", battleCode.c_str(), caller ? "caller" : "callee");
	u32 shortCode = strtol(battleCode.substr(0, 8).c_str(), nullptr, 16);
	capcomService->sendRegister(gameId, caller, shortCode);
	if (caller)
		// Keep the phone ringing until we receive Start from the callee
		modemKeepRinging(true);
	modemPeriodicCallback(100_sh4ms, [this]() {
		if (capcomService->receiveStart())
		{
			INFO_LOG(NETWORK, "%s: Start received", caller ? "caller" : "callee");
			// TODO p2p mode isn't working
			modemPeer2Peer(false);
			if (caller) {
				// Pick up the phone and start the handshake
				modemKeepRinging(false);
			}
			else
			{
				// Simulate an incoming call (ringing)
				modemIncomingCall();
				// Wait for the handshake to begin, then send Start to the caller
				modemOnHandshake([this]() {
					capcomService->sendStart();
					modemOnHandshake({});
				});
			}
			modemPeriodicCallback(0, {});
		}
	});
}
void CapcomDirect::stopCapcomDirectService()
{
	setCustomService(nullptr);
	if (capcomService != nullptr) {
		capcomService->stop();
		capcomService.reset();
	}
	resetModemCallbacks();
	INFO_LOG(NETWORK, "CapcomDirectService stopped");
}

void CapcomDirect::onNetworkChange()
{
	if (!active)
		return;
	if (!settings.network.online && capcomService == nullptr)
	{
		// detect answering mode
		modemOnAnswerMode([this]() {
			caller = false;
			startCapcomDirectService();
			modemOnAnswerMode({});
		});
		// detect calling party using phone#
		modemOnDial([this](const std::string& number) {
			if (number == "20130201")
			{
				caller = true;
				startCapcomDirectService();
				modemOnAnswerMode({});
			}
			else {
				stopCapcomDirectService();
			}
		});
		modemPeer2Peer(false);
	}
}

void CapcomDirect::onEvent(Event event, void *arg)
{
	CapcomDirect *self = (CapcomDirect *)arg;
	switch (event)
	{
	case Event::Start:
		// any but emu paused
		self->onLoadGame();
		break;
	case Event::Terminate:
		// any but emu paused
		self->active = false;
		self->stopCapcomDirectService();
		break;
	case Event::Network:
		// thread: emu thread with modem/bba
		self->onNetworkChange();
		break;
	case Event::LoadState:
		// thread: any but emu pause
		self->stopCapcomDirectService();
		break;
	default:
		break;
	}
}

void CapcomDirect::resetModemCallbacks()
{
	modemOnAnswerMode({});
	modemOnDial({});
	modemOnHandshake({});
	modemPeriodicCallback(0, {});
	modemPeer2Peer(false);
	modemKeepRinging(false);
}

std::string CapcomDirect::getBattleCode()
{
	constexpr u32 addresses[] {
		0x8c23b004, // mvsc2
		0x0c2eacb8, // pstone2
		0x0c052140, // sf3strike3 (05ffc0)
		0x8c225dfc, // sfzero3
		0x0c2e7368, // spawn
		0x0c2e712c, // vampire (340c68, 341e80, 345dac, 355f4c)
		0x0c236a9c, // tennis (237a1c, 23839c)
		0x0c2625c8, // cvspro (26ddac 30f46c 38cdb8)
		0x0c1051b6, // jojo (8997c8)
		0x0c3f2ee8, // pjustice
		0x0c31e6c8, // ssf8 (320afc 329b84 339d24)
		0x0c3f6904, // tech romancer
		0x0c42b320, // taisen net gimmick (430894 56a5a0)
		0x0c2dbaa4, // hmgeo
		0x0c2450dc, // puzzle8 (247510 26494c 2832b8 293458)
	};
	if (gameId == 0 || gameId > std::size(addresses))
		return {};
	const u32 address = addresses[(int)gameId - 1];
	std::string battleCode;
	for (int i = 0; i < 14; i++)
		battleCode += addrspace::read8(address + i);
	return battleCode;
}

} // namespace
