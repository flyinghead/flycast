/*
	Copyright 2021 flyinghead

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

#ifdef GDB_SERVER
#include "gdb_server.h"
#include "debug_agent.h"
#include "network/net_platform.h"
#include <stdexcept>
#include <thread>
#include <chrono>
#include <mutex>

#define SERVER_PORT 3263

namespace debugger {

static void emuEventCallback(Event event, void *);

class GdbServer
{
public:
	struct Error : public std::runtime_error {
		Error(const char *reason) : std::runtime_error(reason) {}
	};

	void init()
	{
		if (VALID(serverSocket))
			return;

		serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (!VALID(serverSocket))
			throw Error("gdb: Cannot create server socket");

		int option = 1;
		setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(option));

		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(SERVER_PORT);

		if (::bind(serverSocket, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
		{
			closesocket(serverSocket);
			throw Error("gdb: bind() failed");
		}
		if (::listen(serverSocket, 5) < 0)
		{
			closesocket(serverSocket);
			throw Error("gdb: listen() failed");
		}
		EventManager::listen(Event::Resume, emuEventCallback);
		EventManager::listen(Event::Terminate, emuEventCallback);
	}

	void term()
	{
		EventManager::unlisten(Event::Resume, emuEventCallback);
		EventManager::unlisten(Event::Terminate, emuEventCallback);
		stop();
		if (VALID(clientSocket))
		{
			closesocket(clientSocket);
			clientSocket = INVALID_SOCKET;
		}
		if (VALID(serverSocket))
		{
			closesocket(serverSocket);
			serverSocket = INVALID_SOCKET;
		}
	}

	void run()
	{
		DEBUG_LOG(COMMON, "GdbServer starting");
		thread = std::thread(&GdbServer::serverThread, this);
	}

	void stop()
	{
		if (thread.joinable())
		{
			DEBUG_LOG(COMMON, "GdbServer stopping");
			agent.resetAgent();
			stopRequested = true;
			thread.join();
		}
	}

	bool isRunning() const {
		return thread.joinable();
	}

	// called on the emu thread
	void debugTrap(u32 event)
	{
		if (!attached)
			return;
		agent.debugTrap(event);
		reportException();
		postDebugTrapNeeded = true;
		throw Stop();
	}

private:
	const u32 EXCEPT_NONE = 1;

	void serverThread()
	{
		while (!stopRequested)
		{
			fd_set fds;
			FD_ZERO(&fds);
			sock_t max_fd = serverSocket;
			FD_SET(serverSocket, &fds);
			if (VALID(clientSocket))
			{
				max_fd = std::max(max_fd, clientSocket);
				FD_SET(clientSocket, &fds);
			}
			timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 100 * 1000;
			if (::select(max_fd + 1, &fds, nullptr, nullptr, &tv) > 0)
			{
				if (FD_ISSET(serverSocket, &fds))
				{
					try {
						acceptClientConnection();
					} catch (const Error& e) {
						ERROR_LOG(COMMON, "%s", e.what());
						closesocket(serverSocket);
						serverSocket = INVALID_SOCKET;
						break;
					}
				}
				else if (FD_ISSET(clientSocket, &fds))
				{
					readCommand();
				}
			}
		}
		if (VALID(clientSocket))
		{
			closesocket(clientSocket);
			clientSocket = INVALID_SOCKET;
		}
		attached = false;
		stopRequested = false;
	}

	void acceptClientConnection()
	{
		if (VALID(clientSocket))
			closesocket(clientSocket);
		sockaddr_in src_addr{};
		socklen_t addr_len = sizeof(src_addr);
		clientSocket = ::accept(serverSocket, (sockaddr *)&src_addr, &addr_len);
		if (!VALID(clientSocket))
		{
			if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
				throw Error("accept failed");
		}
		else
		{
			NOTICE_LOG(NETWORK, "gdb: client connection");
			attached = true;
			agent.interrupt();
		}
	}

	void readCommand()
	{
		if (postDebugTrapNeeded)
		{
			postDebugTrapNeeded = false;
			agent.postDebugTrap();
		}
		try {
			std::string packet = recvPacket();
			if (packet.empty())
				return;

			DEBUG_LOG(NETWORK, "gdb: recv %s", packet.c_str());
			switch (packet[0])
			{
			case '!':	// Enable extended mode
				sendPacket("OK");
				break;
			case '?':	// Sent when connection is first established to query the reason the target halted
				reportException();
				break;
			case 'A':	// Initialized argv[] array passed into program. not supported
				sendPacket("E01");
				break;;
			case 'b':	// Change the serial line speed to baud. deprecated
				break;
			case 'B':	// Set or clear a breakpoint at addr. deprecated
				break;
			case 'c':	// Continue at addr, which is the address to resume.
						// If addr is omitted, resume at current address
				sendContinue(packet);
				break;
			case 'C':	// Continue with signal sig
				sendContinue(packet);
				break;
			case 'd':	// Toggle debug flag. deprecated
				break;
			case 'D':	// Detach GDB from the remote system
				sendPacket("OK");
				agent.detach();
				break;
			case 'F':	// File-I/O protocol extension not currently supported
				break;;
			case 'g':	// Read general registers
				readAllRegs();
				break;
			case 'G':	// Write general registers
				writeAllRegs(packet);
				break;
			case 'H':	// Set thread for subsequent operations
				sendPacket("OK");
				break;
			case 'i':	// Step the remote target by a single clock cycle
			case 'I':	// Signal, then cycle step
				// not supported
				sendPacket("");
				break;
			case 'k':	// Kill request. Stop process/system
				agent.kill();
				break;
			case 'm':	// Read length addressable memory units
				readMem(packet);
				break;
			case 'M':	// Write length addressable memory units
				writeMem(packet);
				break;
			case 'p':	// Read the value of register
				readReg(packet);
				break;
			case 'P':	// Write register
				writeReg(packet);
				break;
			case 'q':	// General query packets
				query(packet);
				break;
			case 'Q':	// General set packets
				set(packet);
				break;
			case 'r':	// Reset the entire system. Deprecated (use 'R' instead)
				break;
			case 'R':	// Restart the program being debugged.
				restart();
				break;
			case 's':	// Single step
				step(EXCEPT_NONE);
				break;
			case 'S':	// Step with signal
				step();
				break;
			case 't':	// Search backwards. unsupported
				break;
			case 'T':	// Find out if the thread is alive
				sendPacket("OK");
				break;
			case 'v':	// 'v' packets to control execution
				vpacket(packet);
				break;
			case 'X':	// Write binary data to memory
				writeMemBin(packet);
				break;
			case 'z':	// Remove a breakpoint/watchpoint.
				removeMatchpoint(packet);
				break;
			case 'Z':	// Insert a breakpoint/watchpoint.
				insertMatchpoint(packet);
				break;
			case 3:
				interrupt();
				break;
			default:
				// Unknown commands are ignored
				WARN_LOG(COMMON, "Unknown gdb command: %s", packet.c_str());
				break;;
			}
		} catch (const Error& e) {
			ERROR_LOG(COMMON, "%s", e.what());
			closesocket(clientSocket);
			clientSocket = INVALID_SOCKET;
			attached = false;
		}
	}

	void reportException()
	{
		char s[4];
		sprintf(s, "S%02X", agent.currentException());
		sendPacket(s);
	}

	void sendContinue(const std::string& pkt)
	{
		if (pkt[0] != 'c') {
			WARN_LOG(COMMON, "Continue with signal not supported");
			return;
		}

		if (pkt == "c")
			agent.doContinue();
		else
		{
			// Get the pc at which to resume
			u32 addr;
			if (sscanf(pkt.c_str(), "c%x", &addr) != 1)
			{
				WARN_LOG(COMMON, "Continue address invalid %s", pkt.c_str());
				return;
			}
			agent.doContinue(addr);
		}
	}

	void readAllRegs()
	{
		u32 *regs;
		int c = agent.readAllRegs(&regs);
		std::string outpkt;
		for (int i = 0; i < c; i++)
			outpkt += pack(regs[i]);
		sendPacket(outpkt);
	}

	void writeAllRegs(const std::string& pkt)
	{
		std::vector<u32> regs;
		for (auto it = pkt.begin() + 1; it <= pkt.end() - 8; it += 8)
			regs.push_back(unpack(&*it, 8));

		agent.writeAllRegs(regs);
		sendPacket("OK");
	}

	void readMem(const std::string& pkt)
	{
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "m%x,%x:", &addr, &len) != 2)
		{
			WARN_LOG(COMMON, "readMem: invalid packet %s", pkt.c_str());
			sendPacket("E01");
			return;
		}
		const u8 *mem = agent.readMem(addr, len);
		std::string outpkt;
		for (u32 i = 0; i < len; i++)
		{
			char s[3];
			sprintf(s,"%02x", mem[i]);
			outpkt += s;
		}
		sendPacket(outpkt);
	}

	void writeMem(const std::string& pkt)
	{
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "M%x,%x:", &addr, &len) != 2)
		{
			WARN_LOG(COMMON, "writeMem: invalid packet %s", pkt.c_str());
			sendPacket("E01");
			return;
		}
		std::vector<u8> data(len);
		const char *p = &pkt[pkt.find(':')] + 1;
		for (u32 i = 0; i < len; i++, p += 2)
		{
			u32 b;
			sscanf(p,"%2x", &b);
			data[i] = (u8)b;
		}
		agent.writeMem(addr, data);
		sendPacket("OK");
	}

	void writeMemBin(const std::string& pkt)
	{
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "X%x,%x:", &addr, &len) != 2)
		{
			WARN_LOG(COMMON, "writeMemBin invalid command: %s", pkt.c_str());
			sendPacket("E01");
			return;
		}
		const char *p = &pkt[pkt.find(':')] + 1;
		std::vector<u8> data;
		data.reserve(pkt.length() - (p - &pkt[0]));
		for (; p <= &pkt.back(); p++)
		{
			u8 b = *p;
			if (b == '}')
			{
				b = *++p ^ 0x20;
			}
			data.push_back(b);
		}
		agent.writeMem(addr, data);
		sendPacket("OK");
	}

	void readReg(const std::string& pkt)
	{
		u32 regNum;
		if (sscanf(pkt.c_str(), "p%x", &regNum) != 1)
		{
			WARN_LOG(COMMON, "readReg: invalid packet %s", pkt.c_str());
			sendPacket("E01");
			return;
		}
		u32 v = agent.readReg(regNum);
		sendPacket(pack(v));
	}

	void writeReg(const std::string& pkt)
	{
		u32 regNum;
		char vstr[9];
		if (sscanf(pkt.c_str(), "P%x=%8s", &regNum, vstr) != 2)
		{
			WARN_LOG(COMMON, "writeReg: invalid packet %s", pkt.c_str());
			sendPacket("E01");
			return;
		}
		agent.writeReg(regNum, unpack(vstr, 8));
		sendPacket("OK");
	}

	void query(const std::string& pkt)
	{
		if (pkt == "qC")
			// Return the current thread ID. 0 is "any thread"
			sendPacket("QC0.01");
		else if (pkt.rfind("qCRC", 0) == 0)
		{
			WARN_LOG(COMMON, "CRC compute not supported %s", pkt.c_str());
			sendPacket("E01");
		}
		else if (pkt == "qfThreadInfo")
			// Obtain a list of all active thread IDs (first call)
			sendPacket("m0");
		else if (pkt == "qsThreadInfo")
			// Obtain a list of all active thread IDs (subsequent calls -> 'l' == end of list)
			sendPacket("l");
		else if (pkt.rfind("qGetTLSAddr:", 0) == 0)
			// Fetch the address associated with thread local storage
			sendPacket("");
		else if (pkt.rfind("qL", 0) == 0)
			// Obtain thread information. deprecated
			sendPacket("qM001");
		else if (pkt == "qOffsets")
			// Get section offsets. Not supported
			sendPacket("");
		else if (pkt.rfind("qP", 0) == 0)
			// Returns information on thread. deprecated
			sendPacket("");
		else if (pkt.rfind("qRcmd,", 0) == 0)
		{
			std::string customCmd;
			for (const char *p = pkt.c_str() + 6; *p != '\0'; p += 2)
				customCmd += (char)unpack(p, 2);
			DEBUG_LOG(COMMON, "query: custom command %s", customCmd.c_str());
			if (customCmd == "reset")
				restart();
			else if (customCmd == "stack")
			{
				u32 len;
				const u32 *data = agent.getStack(len);
				len /= 4;

				char reply[len * 9 * 2 + 1];
				char *r = reply;
				for (u32 i = 0; i < len; i++)
				{
					char n[10];
					sprintf(n, "%08x ", *data++);
					for (char *p = n; *p != 0; p++)
					{
						*r++ = packnb((*p >> 4) & 0xf);
						*r++ = packnb(*p & 0xf);
					}
				}
				*r = 0;
				sendPacket(reply);
			}
			else
				sendPacket("");
		}
		else if (pkt.rfind("qSupported", 0) == 0)
			// Tell the remote stub about features supported by GDB,
			// and query the stub for features it supports
			sendPacket("PacketSize=10000");
		else if (pkt.rfind("qSymbol:", 0) == 0)
			// Notify the target that GDB is prepared to serve symbol lookup requests
			sendPacket("OK");
		else if (pkt.rfind("qThreadExtraInfo,", 0) == 0)
		{
			// Obtain from the target OS a printable string description of thread attributes
			char s[19];
			sprintf(s, "%02x%02x%02x%02x%02x%02x%02x%02x%02x", 'R', 'u', 'n', 'n', 'a', 'b', 'l', 'e', 0);
			sendPacket(std::string(s, 18));
		}
		else if (pkt.rfind("qXfer:", 0) == 0)
			// Read uninterpreted bytes from the target’s special data area identified by the keyword object
			sendPacket("");
		else if (pkt.rfind("qAttached", 0) == 0)
			// Return an indication of whether the remote server attached to an existing process
			// or created a new process
			sendPacket("1");  // existing process
		else if (pkt.rfind("qTfV", 0) == 0)
			// request data about trace state variables
			sendPacket("");
		else if (pkt.rfind("qTfP", 0) == 0)
			// request data about tracepoints
			sendPacket("");
		else if (pkt.rfind("qTStatus", 0) == 0)
			// Ask the stub if there is a trace experiment running right now
			sendPacket("");
		else
			WARN_LOG(COMMON, "query not supported %s", pkt.c_str());
	}

	void set(const std::string& pkt)
	{
		if (pkt.rfind("QPassSignals:", 0) == 0)
			// Passing signals not supported
			sendPacket("");
		else if (pkt.rfind("QTDP", 0) == 0
				|| pkt.rfind("QFrame", 0) == 0
				|| pkt.rfind("QTStart", 0) == 0
				|| pkt.rfind("QTStop", 0) == 0
				|| pkt.rfind("QTinit", 0) == 0
				|| pkt.rfind("QTro", 0) == 0)
			// No tracepoint feature supported
			sendPacket("");
		else
			WARN_LOG(COMMON, "set not supported %s", pkt.c_str());
	}

	void vpacket(const std::string& pkt)
	{
		if (pkt.rfind("vAttach;", 0) == 0)
			sendPacket("S05");
		else if (pkt.rfind("vCont?", 0) == 0)
			// not supported
			sendPacket("vCont;s;c");
		else if (pkt.rfind("vCont", 0) == 0)
			// not supported
			WARN_LOG(COMMON, "vCont not supported %s", pkt.c_str());
		else if (pkt.rfind("vFile:", 0) == 0)
			// not supported
			sendPacket("");
		else if (pkt.rfind("vFlashErase:", 0) == 0)
			// not supported
			sendPacket("E01");
		else if (pkt.rfind("vFlashWrite:", 0) == 0)
			// not supported
			sendPacket("E01");
		else if (pkt.rfind("vFlashDone:", 0) == 0)
			// not supported
			sendPacket("E01");
		else if (pkt.rfind("vRun;", 0) == 0)
		{
			if (pkt != "vRun;")
				WARN_LOG(COMMON, "unexpected vRun args ignored: %s", pkt.c_str());
			agent.restart();
			sendPacket("S05");
		}
		else if (pkt.rfind("vKill", 0) == 0)
		{
			sendPacket("OK");
			agent.kill();
		}
		else if (pkt.rfind("vMustReplyEmpty", 0) == 0)
			// Reply empty packet
			sendPacket("");
		else
			WARN_LOG(COMMON, "unknown v packet: %s", pkt.c_str());
	}

	void restart()
	{
		agent.restart();
	}

	void step(u32 what = 0)
	{
		agent.step();
		sendPacket("S05");
	}

	void insertMatchpoint(const std::string& pkt)
	{
		u32 type;
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "Z%1d,%x,%1d", &type, &addr, &len) != 3) {
			WARN_LOG(COMMON, "insertMatchpoint: unknown packet: %s", pkt.c_str());
			sendPacket("E01");
		}
		switch (type) {
		    case 0:		// soft bp
		    	if (agent.insertMatchpoint(0, addr, len))
		    		sendPacket("OK");
		    	else
		    		sendPacket("E01");
		    	break;
		    case 1:		// hardware bp
		    	sendPacket("");
		    	break;
		    case 2:		// write watchpoint
		    	sendPacket("");
		    	break;
		    case 3:		// read watchpoint
		    	sendPacket("");
		    	break;
		    case 4:		// access watchpoint
		    	sendPacket("");
		    	break;
		    default:
		    	sendPacket("");
		    	break;
		}
	}

	void removeMatchpoint(const std::string& pkt)
	{
		u32 type;
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "z%1d,%x,%1d", &type, &addr, &len) != 3) {
			WARN_LOG(COMMON, "removeMatchpoint: unknown packet: %s", pkt.c_str());
			sendPacket("E01");
		}
		switch (type) {
		    case 0:		// soft bp
		    	if (agent.removeMatchpoint(0, addr, len))
		    		sendPacket("OK");
		    	else
		    		sendPacket("E01");
		    	break;
		    case 1:		// hardware bp
		    	sendPacket("");
		    	break;
		    case 2:		// write watchpoint
		    	sendPacket("");
		    	break;
		    case 3:		// read watchpoint
		    	sendPacket("");
		    	break;
		    case 4:		// access watchpoint
		    	sendPacket("");
		    	break;
		    default:
		    	sendPacket("");
		    	break;
		}
	}

	void interrupt()
	{
		u32 signal = agent.interrupt();
		char s[10];
		sprintf(s, "S%02x", signal);
		sendPacket(s);
	}

	char recvChar()
	{
		char c;
		int rc = ::recv(clientSocket, &c, 1, 0);
		if (rc <= 0)
			throw Error("gdb: I/O error");
		return c;
	}

	void sendChar(char c)
	{
		std::unique_lock<std::mutex> lock(outMutex);
		int rc = ::send(clientSocket, &c, 1, 0);
		if (rc <= 0)
			throw Error("gdb: I/O error");
	}

	u8 unpack(char c)
	{
		c = std::tolower(c);
		if (c <= '9')
			return c - '0';
		else
			return c - 'a' + 10;
	}

	char packnb(u8 b)
	{
		if (b < 10)
			return '0' + b;
		else
			return 'a' + b - 10;
	}

	std::string packb(u8 v)
	{
		std::string s(1, packnb((v >> 4) & 0xf));
		s += packnb(v & 0xf);
		return s;
	}

	std::string pack(u32 v)
	{
		return packb(v & 0xff) + packb((v >> 8) & 0xff)
				+ packb((v >> 16) & 0xff) + packb((v >> 24) & 0xff);
	}

	u32 unpack(const char *s, int l)
	{
		u32 r = 0;
		for (int i = 0; i < l && *s != '\0'; i += 2, s += 2)
		{
			r |= (unpack(s[0]) << 4 | unpack(s[1])) << (i * 4);
		}
		return r;
	}

	std::string recvPacket()
	{
		std::string pkt;
		// look for start character ('$') or BREAK
		char c = recvChar();
		if (c == 3)
			return std::string("\03");
		if (c != '$')
			return pkt;

		// read until '#'
		u8 checksum = 0;
		while (!stopRequested)
		{
			c = recvChar();
			if (c == '$')
			{
				checksum = 0;
				pkt.clear();

				continue;
			}

			if (c == '#')
				break;

			checksum += (u8)c;
			pkt.push_back(c);
		}
		if (stopRequested)
		{
			pkt.clear();
			return pkt;
		}
		u8 recvchk = unpack(recvChar()) << 4;
		recvchk |= unpack(recvChar());

		// If the checksums don't match print a warning, and put the
		// negative ack back to the client. Otherwise put a positive ack.
		if (checksum != recvchk)
		{
			sendChar('-');	// Failed checksum
			return "";
		}
		else
		{
			sendChar('+');	// Successful transfer
			return pkt;
		}
	}

	void sendPacket(const std::string& pkt)
	{	DEBUG_LOG(NETWORK, "gdb: sending pkt");
		std::unique_lock<std::mutex> lock(outMutex);
		std::string data{'$'};
		u8 checksum = 0;
		for (char c : pkt)
		{
			if (c == '$' || c == '#' || c == '*' || c == '}')
			{
				c ^= 0x20;
				checksum += (u8)'}';
				data.push_back('}');
			}
			checksum += (u8)c;
			data.push_back(c);
		}
		data.push_back('#');
		char s[9];
		sprintf(s, "%02x", checksum);
		data += s;
		DEBUG_LOG(NETWORK, "gdb: sent %s", data.c_str());
		int ret = ::send(clientSocket, data.c_str(), data.length(), 0);
		if (ret < (int)data.length())
			throw Error("I/O error");
	}

	bool stopRequested = false;
	bool attached = false;
	bool postDebugTrapNeeded = false;
	sock_t serverSocket = INVALID_SOCKET;
	sock_t clientSocket = INVALID_SOCKET;
	std::thread thread;
	std::mutex outMutex;
public:
	DebugAgent agent;
};

static GdbServer gdbServer;

void init()
{
	gdbServer.init();
}

void term()
{
	gdbServer.term();
}

void debugTrap(u32 event)
{
	gdbServer.debugTrap(event);
}

void subroutineCall()
{
	gdbServer.agent.subroutineCall();
}

void subroutineReturn()
{
	gdbServer.agent.subroutineReturn();
}

static void emuEventCallback(Event event, void *)
{
	switch (event)
	{
	case Event::Resume:
		if (!gdbServer.isRunning())
			gdbServer.run();
		break;
	case Event::Terminate:
		gdbServer.stop();
		break;
	default:
		break;
	}
}

}
#endif
