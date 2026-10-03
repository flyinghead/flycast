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
#pragma once
#include "types.h"
#include <string>

// Dreamcast Now presence reporting, doing inside the emulator what the
// DreamPi's dcnow.py daemon does beside it: when the emulated console is
// online, the SHA-256 of the last host name it resolved is posted every 15
// seconds to the dcnow service, which maps the domain to the game on
// dreamcast.online.
namespace dcnow
{
	// A folder the core can write the persistent player identity to; the
	// frontend hands it over once, before start(). Empty keeps the identity
	// in memory only.
	void setStateDir(const std::string& dir);

	// The core is loaded/unloaded: init runs the DreamPi config server on port
	// 1998 so dreamcast.online can find this player; deinit stops everything.
	// Both are no-ops when the DCNow option is off.
	void init();
	void deinit();

	// The emulated network link is up. No-op when the DCNow option is off.
	void start();
	void stop();

	// A DNS packet the Dreamcast sent out on the emulated link; the question
	// name becomes the next update's dns_query. Thread-safe.
	void dnsQuery(const void *packet, unsigned len);
}
