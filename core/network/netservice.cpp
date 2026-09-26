/*
	Copyright 2025 flyinghead

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
#include "netservice.h"
#include "picoppp.h"
#include "dcnet.h"
#include "emulator.h"
#include "cfg/option.h"
#include "rawmodem.h"

namespace net::modbba
{

static Service *service;
static bool usingDCNet;
static bool usingRawModem;

bool start()
{
	const bool useRawModem = settings.content.gameId == "HDR0010"; // Sega Rally 2 (JP)
	if (service == nullptr || usingRawModem != useRawModem || (!useRawModem && usingDCNet != config::UseDCNet))
	{
		delete service;
		if (useRawModem)
			service = new RawModemService();
		else if (config::UseDCNet)
			service = new DCNetService();
		else
			service = new PicoTcpService();
		usingRawModem = useRawModem;
		usingDCNet = config::UseDCNet;
	}
	return service->start();
}

void stop() {
	if (service != nullptr)
		service->stop();
}

void writeModem(u8 b) {
	verify(service != nullptr);
	service->writeModem(b);
}
int readModem() {
	verify(service != nullptr);
	return service->readModem();
}
int modemAvailable() {
	verify(service != nullptr);
	return service->modemAvailable();
}

void receiveEthFrame(const u8 *frame, u32 size) {
	start();
	service->receiveEthFrame(frame, size);
}

}
