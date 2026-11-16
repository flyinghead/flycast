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
#include "dcnow.h"
#include "net_platform.h"
#include "cfg/option.h"
#include "oslib/oslib.h"
#include "stdclass.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace dcnow
{
namespace
{

// SHA-256, the standard FIPS 180-4 algorithm.
struct Sha256
{
	u32 state[8] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
					 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u };
	u64 length = 0;
	u8 chunk[64];
	unsigned used = 0;

	static u32 rotr(u32 v, unsigned n) { return (v >> n) | (v << (32 - n)); }

	void block(const u8 *p)
	{
		static const u32 k[64] = {
			0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
			0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
			0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
			0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
			0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
			0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
			0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
			0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
		};
		u32 w[64];
		for (unsigned i = 0; i < 16; i++)
			w[i] = (u32(p[4 * i]) << 24) | (u32(p[4 * i + 1]) << 16) | (u32(p[4 * i + 2]) << 8) | p[4 * i + 3];
		for (unsigned i = 16; i < 64; i++)
		{
			const u32 s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
			const u32 s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}
		u32 a = state[0], b = state[1], c = state[2], d = state[3];
		u32 e = state[4], f = state[5], g = state[6], h = state[7];
		for (unsigned i = 0; i < 64; i++)
		{
			const u32 s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
			const u32 ch = (e & f) ^ (~e & g);
			const u32 t1 = h + s1 + ch + k[i] + w[i];
			const u32 s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
			const u32 maj = (a & b) ^ (a & c) ^ (b & c);
			const u32 t2 = s0 + maj;
			h = g; g = f; f = e; e = d + t1;
			d = c; c = b; b = a; a = t1 + t2;
		}
		state[0] += a; state[1] += b; state[2] += c; state[3] += d;
		state[4] += e; state[5] += f; state[6] += g; state[7] += h;
	}

	void update(const void *data, size_t len)
	{
		const u8 *p = static_cast<const u8 *>(data);
		length += len;
		while (len > 0)
		{
			const unsigned take = std::min<unsigned>(64 - used, (unsigned)len);
			memcpy(chunk + used, p, take);
			used += take;
			p += take;
			len -= take;
			if (used == 64)
			{
				block(chunk);
				used = 0;
			}
		}
	}

	void final(u8 out[32])
	{
		const u64 bits = length * 8;
		const u8 end = 0x80;
		update(&end, 1);
		const u8 zero = 0;
		while (used != 56)
			update(&zero, 1);
		u8 tail[8];
		for (unsigned i = 0; i < 8; i++)
			tail[i] = u8(bits >> (56 - 8 * i));
		update(tail, 8);
		for (unsigned i = 0; i < 8; i++)
		{
			out[4 * i] = u8(state[i] >> 24);
			out[4 * i + 1] = u8(state[i] >> 16);
			out[4 * i + 2] = u8(state[i] >> 8);
			out[4 * i + 3] = u8(state[i]);
		}
	}
};

std::string sha256Hex(const std::string& text)
{
	Sha256 ctx;
	u8 out[32];
	ctx.update(text.data(), text.size());
	ctx.final(out);
	char hex[65];
	for (unsigned i = 0; i < 32; i++)
		snprintf(hex + 2 * i, 3, "%02x", out[i]);
	hex[64] = 0;
	return hex;
}

// Host names several games share. The service maps one domain to one game, so
// re-sending one of these would keep overwriting the entry; each is posted
// once per session (dcnow.py's SHARED_DOMAINS).
const char *const sharedDomains[] = {
	"gameloft",
	"onsen0.overworks.isao.net",
};

constexpr unsigned updateIntervalMs = 15000;

std::mutex mutex;
std::condition_variable wake;
std::thread worker;
std::thread server;
std::string lastDomain;
std::set<std::string> sharedSent;
std::string stateDir;
std::string playerId;
std::string macText;
std::atomic<sock_t> currentSocket{ INVALID_SOCKET };
std::atomic<sock_t> listenSocket{ INVALID_SOCKET };
bool stopping = false;
std::atomic<bool> serverStop{ false };

constexpr unsigned short configServerPort = 1998;

std::string stateFile()
{
	return stateDir.empty() ? "flycast_dcnow.id" : stateDir + "/flycast_dcnow.id";
}

// Any MAC-shaped text becomes this player's identity: uppercased like
// DreamPi's "%012X", hashed, kept for the config page and the POSTs, and
// persisted so the identity survives a restart.
bool setIdentity(const std::string& text)
{
	std::string mac = text;
	for (char& c : mac)
		c = (char)std::toupper((unsigned char)c);
	if (mac.size() != 17)
		return false;
	{
		std::lock_guard<std::mutex> lock(mutex);
		macText = mac;
		playerId = sha256Hex(mac);
	}
	FILE *f = fopen(stateFile().c_str(), "wb");
	if (f != nullptr)
	{
		fputs(mac.c_str(), f);
		fclose(f);
	}
	NOTICE_LOG(NETWORK, "Dreamcast Now id set (%s)", mac.c_str());
	return true;
}

// The dcnow player's key is the SHA-256 of a MAC-shaped string, the Pi's NIC
// on DreamPi. Order of choice: an explicit MAC set in the core option (the
// options file takes a literal AA:BB:CC:DD:EE:FF even though the menu only
// shows "auto"), then the identity file, then a generated one persisted here.
// Register the logged hash on dreamcast.online.
void loadIdentity()
{
	std::string wanted = config::DCNowMac;
	for (char& c : wanted)
		c = (char)std::toupper((unsigned char)c);
	if (wanted.size() == 17)
	{
		NOTICE_LOG(NETWORK, "Dreamcast Now id from the core option (%s)", wanted.c_str());
		std::lock_guard<std::mutex> lock(mutex);
		macText = wanted;
		playerId = sha256Hex(wanted);
		return;
	}
	const std::string path = stateFile();
	char mac[32] = {};
	FILE *f = fopen(path.c_str(), "rb");
	if (f != nullptr)
	{
		if (fgets(mac, sizeof(mac), f) != nullptr)
			mac[strcspn(mac, "\r\n")] = 0;
		fclose(f);
		if (strlen(mac) == 17)
		{
			INFO_LOG(NETWORK, "Dreamcast Now id from %s", path.c_str());
			std::lock_guard<std::mutex> lock(mutex);
			macText = mac;
			for (char& c : macText)
				c = (char)std::toupper((unsigned char)c);
			playerId = sha256Hex(macText);
			return;
		}
	}
	u32 seed = (u32)time(nullptr) ^ (u32)(uintptr_t)&f;
	snprintf(mac, sizeof(mac), "5A:%02X:%02X:%02X:%02X:%02X",
			 (seed >> 3) & 0xff, (seed >> 11) & 0xff, (seed >> 19) & 0xff,
			 (seed >> 27) & 0xff, (seed ^ (seed >> 16)) & 0xff);
	f = fopen(path.c_str(), "wb");
	if (f != nullptr)
	{
		fputs(mac, f);
		fclose(f);
		NOTICE_LOG(NETWORK, "Dreamcast Now identity saved to %s", path.c_str());
	}
	std::lock_guard<std::mutex> lock(mutex);
	macText = mac;
	playerId = sha256Hex(mac);
}

// application/x-www-form-urlencoded: '+' is space, %XX a byte.
std::string urldecode(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++)
	{
		if (s[i] == '+')
			out += ' ';
		else if (s[i] == '%' && i + 2 < s.size())
		{
			out += (char)strtoul(s.substr(i + 1, 2).c_str(), nullptr, 16);
			i += 2;
		}
		else
			out += s[i];
	}
	return out;
}

// A DNS question section: header, then the query name as counted labels.
std::string parseQueryName(const u8 *p, unsigned len)
{
	if (len < 13)
		return {};
	if ((p[2] & 0x80) != 0 || (p[4] << 8 | p[5]) == 0)
		// A response, or no questions
		return {};
	std::string name;
	unsigned at = 12;
	while (at < len && p[at] != 0)
	{
		const unsigned label = p[at];
		if ((label & 0xc0) != 0 || at + 1 + label > len)
			return {};
		if (!name.empty())
			name += '.';
		name.append(reinterpret_cast<const char *>(p + at + 1), label);
		at += 1 + label;
	}
	return name;
}

void post(const std::string& domainHash)
{
	std::string id;
	{
		std::lock_guard<std::mutex> lock(mutex);
		id = playerId;
	}
	struct addrinfo hints {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo *addrs = nullptr;
	if (getaddrinfo("dcnow-2016.appspot.com", "80", &hints, &addrs) != 0
			|| addrs == nullptr)
	{
		DEBUG_LOG(NETWORK, "dcnow: cannot resolve dcnow-2016.appspot.com");
		return;
	}
	sock_t fd = INVALID_SOCKET;
	for (struct addrinfo *a = addrs; a != nullptr; a = a->ai_next)
	{
		fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (!VALID(fd))
			continue;
		set_recv_timeout(fd, 10000);
		set_send_timeout(fd, 10000);
		currentSocket = fd;
		if (connect(fd, a->ai_addr, (socklen_t)a->ai_addrlen) == 0)
			break;
		closesocket(fd);
		currentSocket = INVALID_SOCKET;
		fd = INVALID_SOCKET;
	}
	freeaddrinfo(addrs);
	if (!VALID(fd))
		return;

	std::string body;
	if (!domainHash.empty())
		body = "dns_query=" + domainHash;
	char request[512];
	const int size = snprintf(request, sizeof(request),
		"POST /api/update/%s/ HTTP/1.0\r\n"
		"Host: dcnow-2016.appspot.com\r\n"
		"User-Agent: Mozilla/4.0 (compatible; MSIE 5.5; Windows NT), Dreamcast Now\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: %u\r\n\r\n%s",
		id.c_str(), (unsigned)body.size(), body.c_str());
	if (size > 0)
	{
		const char *p = request;
		int left = size;
		while (left > 0)
		{
			const int sent = (int)send(fd, p, left, 0);
			if (sent <= 0)
				break;
			p += sent;
			left -= sent;
		}
		char answer[256];
		while (recv(fd, answer, sizeof(answer), 0) > 0)
			;
	}
	closesocket(fd);
	currentSocket = INVALID_SOCKET;
}

void run()
{
	ThreadName _("DCNow");
	while (true)
	{
		std::string domain;
		{
			std::unique_lock<std::mutex> lock(mutex);
			wake.wait_for(lock, std::chrono::milliseconds(updateIntervalMs),
						  [] { return stopping; });
			if (stopping)
				return;
			domain = lastDomain;
			lastDomain.clear();
		}
		std::string hash;
		if (!domain.empty())
		{
			bool skip = domain.find("appspot") != std::string::npos;
			for (const char *shared : sharedDomains)
				if (domain.find(shared) != std::string::npos)
				{
					if (!sharedSent.insert(shared).second)
						skip = true;
					break;
				}
			if (!skip)
				hash = sha256Hex(domain);
		}
		post(hash);
	}
}

// dreamcast.online finds a DreamPi by probing port 1998 on the LAN (its page
// fetches it cross-origin, hence the open CORS). The JSON is the one the Pi's
// config_server.py answers, with this player's dcnow identity, so the site
// claims the emulator exactly as it would a Pi.
void runServer()
{
	ThreadName _("DCNowCfg");
	sock_t fd = socket(AF_INET, SOCK_STREAM, 0);
	if (!VALID(fd))
		return;
	int one = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof(one));
	sockaddr_in addr {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(configServerPort);
	if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0
			|| listen(fd, 4) != 0)
	{
		WARN_LOG(NETWORK, "dcnow: cannot listen on port %u", configServerPort);
		closesocket(fd);
		return;
	}
	listenSocket = fd;
	NOTICE_LOG(NETWORK, "dcnow: DreamPi config server on port %u", configServerPort);
	while (!serverStop)
	{
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET((unsigned)fd, &rfds);
		timeval tv { 0, 250000 };
		if (select((int)fd + 1, &rfds, nullptr, nullptr, &tv) <= 0)
			continue;
		sock_t conn = accept(fd, nullptr, nullptr);
		if (!VALID(conn))
			continue;
		set_recv_timeout(conn, 3000);
		set_send_timeout(conn, 3000);
		char request[2048];
		const int got = (int)recv(conn, request, sizeof(request) - 1, 0);
		if (got <= 0)
		{
			closesocket(conn);
			continue;
		}
		request[got] = 0;
		// The DreamPi's POSTs carry a form body: 'disable'/'enable' toggles the
		// service, 'mac=AA:BB:CC:DD:EE:FF' sets the identity. The same port is
		// the config UI: a browser's GET (Accept: text/html) gets a form, the
		// site's XHR gets the JSON like the Pi's.
		const bool isPost = strncmp(request, "POST", 4) == 0;
		const char *form = strstr(request, "\r\n\r\n");
		form = form != nullptr ? form + 4 : "";
		if (isPost)
		{
			if (strstr(form, "disable") != nullptr)
				config::DCNow = false;
			else if (strstr(form, "enable") != nullptr)
				config::DCNow = true;
			const char *m = strstr(form, "mac=");
			if (m != nullptr)
			{
				const char *end = strchr(m + 4, '&');
				setIdentity(urldecode(std::string(m + 4,
						end != nullptr ? end : form + strlen(form))));
			}
		}
		char body[640];
		int bodyLen;
		const char *type = "application/json";
		if (!isPost && strstr(request, "text/html") != nullptr)
		{
			type = "text/html";
			std::string mac, id;
			{
				std::lock_guard<std::mutex> lock(mutex);
				mac = macText;
				id = playerId;
			}
			bodyLen = snprintf(body, sizeof(body),
				"<!DOCTYPE html><title>Flycast DreamPi</title>"
				"<body><h2>Flycast DreamPi</h2>"
				"<p>Player id: <b>%s</b><br>Register it on dreamcast.online.</p>"
				"<form method=post>MAC: <input name=mac value=\"%s\" maxlength=17 size=18> "
				"<button>Save</button></form>"
				"<form method=post><button name=enable>Enable</button> "
				"<button name=disable>Disable</button></form>"
				"<p>Status: %s</p></body>",
				id.c_str(), mac.c_str(), config::DCNow ? "enabled" : "disabled");
		}
		else
		{
			std::string id;
			{
				std::lock_guard<std::mutex> lock(mutex);
				id = playerId;
			}
			bodyLen = snprintf(body, sizeof(body),
				"{\"mac_address\":\"%s\",\"is_enabled\":%s}",
				id.c_str(), config::DCNow ? "true" : "false");
		}
		char response[1024];
		const int len = snprintf(response, sizeof(response),
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: %s\r\n"
			"Access-Control-Allow-Origin: *\r\n"
			"Content-Length: %d\r\n"
			"Connection: close\r\n\r\n%s", type, bodyLen, body);
		if (len > 0)
			send(conn, response, len, 0);
		closesocket(conn);
	}
	closesocket(fd);
	listenSocket = INVALID_SOCKET;
}

} // namespace

void setStateDir(const std::string& dir)
{
	stateDir = dir;
}

void init()
{
	if (!config::DCNow)
		return;
	if (playerId.empty())
	{
		loadIdentity();
		NOTICE_LOG(NETWORK, "Dreamcast Now player id %s - claim it on dreamcast.online",
				   playerId.c_str());
	}
	if (server.joinable())
		return;
	serverStop = false;
	server = std::thread(runServer);
}

void start()
{
	if (!config::DCNow)
		return;
	std::lock_guard<std::mutex> lock(mutex);
	if (worker.joinable())
		return;
	stopping = false;
	sharedSent.clear();
	if (playerId.empty())
	{
		loadIdentity();
		NOTICE_LOG(NETWORK, "Dreamcast Now player id %s - claim it on dreamcast.online",
				   playerId.c_str());
	}
	worker = std::thread(run);
}

void stop()
{
	sock_t fd;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (!worker.joinable())
			return;
		stopping = true;
	}
	// Wake the sleeper and cut any in-flight POST short.
	fd = currentSocket.exchange(INVALID_SOCKET);
	if (VALID(fd))
		closesocket(fd);
	wake.notify_all();
	worker.join();
}

void deinit()
{
	stop();
	if (!server.joinable())
		return;
	serverStop = true;
	// shutdown() wakes the select() loop; the thread owns the close.
	const sock_t fd = listenSocket.exchange(INVALID_SOCKET);
	if (VALID(fd))
		shutdown(fd, SHUT_RDWR);
	server.join();
}

void dnsQuery(const void *packet, unsigned len)
{
	if (!config::DCNow)
		return;
	std::string name = parseQueryName(static_cast<const u8 *>(packet), len);
	if (name.empty())
		return;
	DEBUG_LOG(NETWORK, "dcnow: Dreamcast asked for %s", name.c_str());
	std::lock_guard<std::mutex> lock(mutex);
	lastDomain = name;
}

} // namespace dcnow
