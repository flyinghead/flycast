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
#include "cfg/option.h"
#include "oslib/oslib.h"
#include "util/shared_this.h"
#include <asio.hpp>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cassert>
#include <memory>

namespace debugger {

constexpr u32 MAX_PACKET_LEN = 4096;

static u8 unpack(char c)
{
	c = std::tolower(c);
	if (c <= '9')
		return c - '0';
	else
		return c - 'a' + 10;
}

u32 unpack(const char *s, int l)
{
	u32 r = 0;
	for (int i = 0; i < l && *s != '\0'; i += 2, s += 2) {
		r |= (unpack(s[0]) << 4 | unpack(s[1])) << (i * 4);
	}
	return r;
}

class GdbServer;

class Connection : public SharedThis<Connection>
{
public:
	asio::ip::tcp::socket& getSocket() {
		return socket;
	}

	void start() {
		asio::async_read_until(socket, asio::dynamic_string_buffer(message, MAX_PACKET_LEN), packetMatcher,
				std::bind(&Connection::handlePacket, shared_from_this(),
								asio::placeholders::error,
								asio::placeholders::bytes_transferred));
	}

private:
	Connection(GdbServer& server, asio::io_context& io_context)
		: server(server), io_context(io_context), socket(io_context) {
	}

	using iterator = asio::buffers_iterator<asio::const_buffers_1>;

	std::pair<iterator, bool>
	static packetMatcher(iterator begin, iterator end)
	{
		if (begin == end)
			return std::make_pair(begin, false);
		iterator i = begin;
		if (*i == '\03')
			// break
			return std::make_pair(i + 1, true);
		if (*i != '$') {
			// unexpected, or ack/nack ('+', '-')
			return std::make_pair(i + 1, true);
		}
		++i;
		while (i != end && *i != '#')
			++i;
		if (i + 3 <= end)
			// 2 chars for CRC
			return std::make_pair(i + 3, true);

		return std::make_pair(begin, false);
	}

	void handlePacket(const std::error_code& ec, size_t len);

	void send(const std::string& msg)
	{
		if (msg.empty())
			start();
		else
			asio::async_write(socket, asio::buffer(msg),
				std::bind(&Connection::writeDone, shared_from_this(),
						asio::placeholders::error,
						asio::placeholders::bytes_transferred));

	}

	void writeDone(const std::error_code& ec, size_t len)
	{
		if (ec)
			WARN_LOG(COMMON, "Write error: %s", ec.message().c_str());
		else
			start();
	}

	GdbServer& server;
	asio::io_context& io_context;
	asio::ip::tcp::socket socket;
	std::string message;
	friend super;
};

class TcpAcceptor
{
public:
	TcpAcceptor(GdbServer& server, asio::io_context& io_context, u16 port)
		: server(server), io_context(io_context),
		  acceptor(asio::ip::tcp::acceptor(io_context,
				asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)))
	{
		asio::socket_base::reuse_address option(true);
		acceptor.set_option(option);
		start();
	}

private:
	void start()
	{
		Connection::Ptr newConnection = Connection::create(server, io_context);

		acceptor.async_accept(newConnection->getSocket(),
				std::bind(&TcpAcceptor::handleAccept, this, newConnection, asio::placeholders::error));
	}
	void handleAccept(Connection::Ptr newConnection, const std::error_code& error);

	GdbServer& server;
	asio::io_context& io_context;
	asio::ip::tcp::acceptor acceptor;
};

class GdbServer
{
public:
	struct Error : public std::runtime_error {
		Error(const char *reason) : std::runtime_error(reason) {}
	};

	void init(int port)
	{
		this->port = port;
		EventManager::listen(EmuEvent::Resume, emuEventCallback, this);
		EventManager::listen(EmuEvent::Terminate, emuEventCallback, this);
	}

	void term()
	{
		EventManager::unlisten(EmuEvent::Resume, emuEventCallback, this);
		EventManager::unlisten(EmuEvent::Terminate, emuEventCallback, this);
		stop();
	}

	void run()
	{
		if (thread.joinable())
			return;
		DEBUG_LOG(COMMON, "GdbServer starting");
		io_context = std::make_unique<asio::io_context>();
		thread = std::thread(&GdbServer::serverThread, this);
		if (config::GDBWaitForConnection)
		{
			DEBUG_LOG(COMMON, "Waiting for GDB connection...");
			agentInterrupt();
		}
	}

	void stop()
	{
		if (thread.joinable())
		{
			DEBUG_LOG(COMMON, "GdbServer stopping");
			agent.resetAgent();
			io_context->stop();
			thread.join();
			io_context.reset();
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
		ThreadName _("GdbServer");
		try
		{
			TcpAcceptor server(*this, *io_context, port);
			io_context->run();
		}
		catch (const std::exception& e)
		{
			ERROR_LOG(COMMON, "Gdb server exception: %s", e.what());
		}
		attached = false;
	}

	std::string handleCommand(const std::string& packet)
	{
        try {
			if (postDebugTrapNeeded)
			{
				postDebugTrapNeeded = false;
				try {
					agent.postDebugTrap();
				} catch (const FlycastException& e) {
					throw Error(e.what());
				}
			}
			if (packet.empty())
				return "";

			DEBUG_LOG(NETWORK, "gdb: recv %s", packet.c_str());
			std::vector<std::string> replies;
			switch (packet[0])
			{
			case '!':	// Enable extended mode
				replies.push_back("OK");
				break;
			case '?':	// Sent when connection is first established to query the reason the target halted
				replies.push_back(reportException());
				break;
			case 'A':	// Initialized argv[] array passed into program. not supported
				replies.push_back("E01");
				break;;
			case 'b':	// Change the serial line speed to baud. deprecated
				break;
			case 'B':	// Set or clear a breakpoint at addr. deprecated
				break;
			case 'c':	// Continue at addr, which is the address to resume.
						// If addr is omitted, resume at current address
				doContinue(packet);
				break;
			case 'C':	// Continue with signal sig
				doContinue(packet);
				break;
			case 'd':	// Toggle debug flag. deprecated
				break;
			case 'D':	// Detach GDB from the remote system
				replies.push_back("OK");
				agent.detach();
				break;
			case 'F':	// File-I/O protocol extension not currently supported
				break;;
			case 'g':	// Read general registers
				replies.push_back(readAllRegs());
				break;
			case 'G':	// Write general registers
				replies.push_back(writeAllRegs(packet));
				break;
			case 'H':	// Set thread for subsequent operations
				replies.push_back("OK");
				break;
			case 'i':	// Step the remote target by a single clock cycle
			case 'I':	// Signal, then cycle step
				// not supported
				replies.push_back("");
				break;
			case 'k':	// Kill request. Stop process/system
				agent.kill();
				break;
			case 'm':	// Read length addressable memory units
				replies.push_back(readMem(packet));
				break;
			case 'M':	// Write length addressable memory units
				replies.push_back(writeMem(packet));
				break;
			case 'p':	// Read the value of register
				replies.push_back(readReg(packet));
				break;
			case 'P':	// Write register
				replies.push_back(writeReg(packet));
				break;
			case 'q':	// General query packets
				{
					auto v = query(packet);
					replies.insert(replies.end(), v.begin(), v.end());
				}
				break;
			case 'Q':	// General set packets
				{
					auto v = set(packet);
					replies.insert(replies.end(), v.begin(), v.end());
				}
				break;
			case 'r':	// Reset the entire system. Deprecated (use 'R' instead)
				break;
			case 'R':	// Restart the program being debugged.
				restart();
				break;
			case 's':	// Single step
				replies.push_back(step(EXCEPT_NONE));
				break;
			case 'S':	// Step with signal
				replies.push_back(step());
				break;
			case 't':	// Search backwards. unsupported
				break;
			case 'T':	// Find out if the thread is alive
				replies.push_back("OK");
				break;
			case 'v':	// 'v' packets to control execution
				{
					auto v = vpacket(packet);
					replies.insert(replies.end(), v.begin(), v.end());
				}
				break;
			case 'X':	// Write binary data to memory
				replies.push_back(writeMemBin(packet));
				break;
			case 'z':	// Remove a breakpoint/watchpoint.
				replies.push_back(removeMatchpoint(packet));
				break;
			case 'Z':	// Insert a breakpoint/watchpoint.
				replies.push_back(insertMatchpoint(packet));
				break;
			case 3:
				replies.push_back(interrupt());
				break;
			default:
				// Unknown commands are ignored
				WARN_LOG(COMMON, "Unknown gdb command: %s", packet.c_str());
				break;;
			}
			std::string data;
			for (const std::string& pkt : replies)
			{
				data.push_back('$');
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
			}
			DEBUG_LOG(NETWORK, "gdb: sent %s", data.c_str());

			return data;
		} catch (const Error& e) {
			ERROR_LOG(COMMON, "%s", e.what());
			attached = false;
			throw e;
		}
	}

	std::string reportException()
	{
		char s[4];
		sprintf(s, "S%02X", agent.currentException());
		return s;
	}

	void doContinue(const std::string& pkt)
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

	std::string readAllRegs()
	{
		u32 *regs;
		int c = agent.readAllRegs(&regs);
		std::string outpkt;
		for (int i = 0; i < c; i++)
			outpkt += pack(regs[i]);
		return outpkt;
	}

	std::string writeAllRegs(const std::string& pkt)
	{
		std::vector<u32> regs;
		for (auto it = pkt.begin() + 1; it <= pkt.end() - 8; it += 8)
			regs.push_back(unpack(&*it, 8));

		agent.writeAllRegs(regs);
		return "OK";
	}

	std::string readMem(const std::string& pkt)
	{
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "m%x,%x:", &addr, &len) != 2)
		{
			WARN_LOG(COMMON, "readMem: invalid packet %s", pkt.c_str());
			return "E01";
		}
		const u8 *mem = agent.readMem(addr, len);
		std::string outpkt;
		for (u32 i = 0; i < len; i++)
		{
			char s[3];
			sprintf(s,"%02x", mem[i]);
			outpkt += s;
		}
		return outpkt;
	}

	std::string writeMem(const std::string& pkt)
	{
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "M%x,%x:", &addr, &len) != 2)
		{
			WARN_LOG(COMMON, "writeMem: invalid packet %s", pkt.c_str());
			return "E01";
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
		return "OK";
	}

	std::string writeMemBin(const std::string& pkt)
	{
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "X%x,%x:", &addr, &len) != 2)
		{
			WARN_LOG(COMMON, "writeMemBin invalid command: %s", pkt.c_str());
			return "E01";
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
		return "OK";
	}

	std::string readReg(const std::string& pkt)
	{
		u32 regNum;
		if (sscanf(pkt.c_str(), "p%x", &regNum) != 1)
		{
			WARN_LOG(COMMON, "readReg: invalid packet %s", pkt.c_str());
			return "E01";
		}
		u32 v = agent.readReg(regNum);
		return pack(v);
	}

	std::string writeReg(const std::string& pkt)
	{
		u32 regNum;
		char vstr[9];
		if (sscanf(pkt.c_str(), "P%x=%8s", &regNum, vstr) != 2)
		{
			WARN_LOG(COMMON, "writeReg: invalid packet %s", pkt.c_str());
			return "E01";
		}
		agent.writeReg(regNum, unpack(vstr, 8));
		return "OK";
	}

	std::vector<std::string> query(const std::string& pkt)
	{
		if (pkt == "qC")
			// Return the current thread ID. 0 is "any thread"
			return { "QC0.01" };
		else if (pkt.rfind("qCRC", 0) == 0)
		{
			WARN_LOG(COMMON, "CRC compute not supported %s", pkt.c_str());
			return { "E01" };
		}
		else if (pkt == "qfThreadInfo")
			// Obtain a list of all active thread IDs (first call)
			return { "m0" };
		else if (pkt == "qsThreadInfo")
			// Obtain a list of all active thread IDs (subsequent calls -> 'l' == end of list)
			return { "l" };
		else if (pkt.rfind("qGetTLSAddr:", 0) == 0)
			// Fetch the address associated with thread local storage
			return { "" };
		else if (pkt.rfind("qL", 0) == 0)
			// Obtain thread information. deprecated
			return { "qM001" };
		else if (pkt == "qOffsets")
			// Get section offsets. Not supported
			return { "" };
		else if (pkt.rfind("qP", 0) == 0)
			// Returns information on thread. deprecated
			return { "" };
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

#if _MSC_VER // Non-const array size is a GCC extension
				assert((len * 9 * 2 + 1) < MAX_PACKET_LEN);
				char reply[MAX_PACKET_LEN];
#else
				char reply[len * 9 * 2 + 1];
#endif
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
				return { reply };
			}
			else
				return { "" };
		}
		else if (pkt.rfind("qSupported", 0) == 0)
		{
			// Tell the remote stub about features supported by GDB,
			// and query the stub for features it supports
			char qsupported[128];
			snprintf(qsupported, 128, "PacketSize=%i;vContSupported+", MAX_PACKET_LEN);
			return { qsupported };
		}
		else if (pkt.rfind("qSymbol:", 0) == 0)
			// Notify the target that GDB is prepared to serve symbol lookup requests
			return { "OK" };
		else if (pkt.rfind("qThreadExtraInfo,", 0) == 0)
		{
			// Obtain from the target OS a printable string description of thread attributes
			char s[19];
			sprintf(s, "%02x%02x%02x%02x%02x%02x%02x%02x%02x", 'R', 'u', 'n', 'n', 'a', 'b', 'l', 'e', 0);
			return { std::string(s, 18) };
		}
		else if (pkt.rfind("qXfer:", 0) == 0)
			// Read uninterpreted bytes from the target’s special data area identified by the keyword object
			return { "" };
		else if (pkt.rfind("qAttached", 0) == 0)
			// Return an indication of whether the remote server attached to an existing process
			// or created a new process
			return { "1" };  // existing process
		else if (pkt.rfind("qTfV", 0) == 0)
			// request data about trace state variables
			return { "" };
		else if (pkt.rfind("qTfP", 0) == 0)
			// request data about tracepoints
			return { "" };
		else if (pkt.rfind("qTStatus", 0) == 0)
			// Ask the stub if there is a trace experiment running right now
			return { "" };
		WARN_LOG(COMMON, "query not supported %s", pkt.c_str());
		return {};
	}

	std::vector<std::string> set(const std::string& pkt)
	{
		if (pkt.rfind("QPassSignals:", 0) == 0)
			// Passing signals not supported
			return { "" };
		else if (pkt.rfind("QTDP", 0) == 0
				|| pkt.rfind("QFrame", 0) == 0
				|| pkt.rfind("QTStart", 0) == 0
				|| pkt.rfind("QTStop", 0) == 0
				|| pkt.rfind("QTinit", 0) == 0
				|| pkt.rfind("QTro", 0) == 0)
			// No tracepoint feature supported
			return { "" };
		WARN_LOG(COMMON, "set not supported %s", pkt.c_str());
		return {};
	}

	std::vector<std::string> vpacket(const std::string& pkt)
	{
		if (pkt.rfind("vAttach;", 0) == 0)
			return { "S05" };
		else if (pkt.rfind("vCont?", 0) == 0)
			// supported vCont actions - (c)ontinue, (C)ontinue with signal, (s)tep, (S)tep with signal, (r)ange-step
			return { "vCont;c;C;s;S;t;r" };
		else if (pkt.rfind("vCont", 0) == 0)
		{
			std::string vContCmd = pkt.substr(strlen("vCont;"));
			std::vector<std::string> replies;
			switch (vContCmd[0])
			{
			case 'c':
			case 'C':
				doContinue(vContCmd);
				return {};
			case 's':
				return { step(EXCEPT_NONE) };
			case 'S':
				replies.push_back(step());
				[[fallthrough]];
			case 'r':
			{
				u32 from, to;
				if (sscanf(vContCmd.c_str(), "r%x,%x", &from, &to) == 2)
				{
					auto v = stepRange(from, to);
					replies.insert(replies.end(), v.begin(), v.end());
				}
				else
				{
					WARN_LOG(COMMON, "Unsupported vCont:r format %s", pkt.c_str());
					doContinue("c");
				}
				return replies;
			}
			default:
				WARN_LOG(COMMON, "vCont action not supported %s", pkt.c_str());
				return {};
			}
		}
		else if (pkt.rfind("vFile:", 0) == 0)
			// not supported
			return { "" };
		else if (pkt.rfind("vFlashErase:", 0) == 0)
			// not supported
			return { "E01" };
		else if (pkt.rfind("vFlashWrite:", 0) == 0)
			// not supported
			return { "E01" };
		else if (pkt.rfind("vFlashDone:", 0) == 0)
			// not supported
			return { "E01" };
		else if (pkt.rfind("vRun;", 0) == 0)
		{
			if (pkt != "vRun;")
				WARN_LOG(COMMON, "unexpected vRun args ignored: %s", pkt.c_str());
			agent.restart();
			return { "S05" };
		}
		else if (pkt.rfind("vKill", 0) == 0)
		{
			agent.kill();
			return { "OK" };
		}
		else
		{
			WARN_LOG(COMMON, "unknown v packet: %s", pkt.c_str());
			return { "" };
		}
	}

	void restart() {
		agent.restart();
	}

	std::string step(u32 what = 0)
	{
		try {
			agent.step();
			return "S05";
		} catch (const FlycastException& e) {
			throw Error(e.what());
		}
	}

	std::vector<std::string> stepRange(u32 from, u32 to)
	{
		try {
			agent.stepRange(from, to);
			return { "OK", "S05" };
		} catch (const FlycastException& e) {
			throw Error(e.what());
		}
	}

	std::string insertMatchpoint(const std::string& pkt)
	{
		u32 type;
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "Z%1d,%x,%1d", &type, &addr, &len) != 3) {
			WARN_LOG(COMMON, "insertMatchpoint: unknown packet: %s", pkt.c_str());
			return "E01";
		}
		switch (type) {
			case DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK:		// soft bp
		    	if (agent.insertMatchpoint(DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK,
		    			addr, len))
		    		return "OK";
		    	else
		    		return "E01";
		    	break;
		    case DebugAgent::Breakpoint::BP_TYPE_HARDWARE_BREAK:		// hardware bp
		    	return "";
		    	break;
		    case DebugAgent::Breakpoint::BP_TYPE_WRITE_WATCHPOINT:	// write watchpoint
		    	return "";
		    	break;
		    case DebugAgent::Breakpoint::BP_TYPE_READ_WATCHPOINT:		// read watchpoint
		    	return "";
		    	break;
		    case DebugAgent::Breakpoint::BP_TYPE_ACCESS_WATCHPOINT:	// access watchpoint
		    	return "";
		    	break;
		    default:
		    	return "";
		    	break;
		}
	}

	std::string removeMatchpoint(const std::string& pkt)
	{
		u32 type;
		u32 addr;
		u32 len;
		if (sscanf(pkt.c_str(), "z%1d,%x,%1d", &type, &addr, &len) != 3) {
			WARN_LOG(COMMON, "removeMatchpoint: unknown packet: %s", pkt.c_str());
			return "E01";
		}
		switch (type) {
		    case 0:		// soft bp
		    	if (agent.removeMatchpoint(DebugAgent::Breakpoint::BP_TYPE_SOFTWARE_BREAK,
		    			addr, len))
		    		return "OK";
		    	else
		    		return "E01";
		    	break;
		    case 1:		// hardware bp
		    	return "";
		    	break;
		    case 2:		// write watchpoint
		    	return "";
		    	break;
		    case 3:		// read watchpoint
		    	return "";
		    	break;
		    case 4:		// access watchpoint
		    	return "";
		    	break;
		    default:
		    	return "";
		    	break;
		}
	}

	std::string interrupt()
	{
		u32 signal = agentInterrupt();
		char s[10];
		sprintf(s, "S%02x", signal);
		return s;
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

	std::string pack(u32 v) {
		return packb(v & 0xff) + packb((v >> 8) & 0xff)
				+ packb((v >> 16) & 0xff) + packb((v >> 24) & 0xff);
	}

	u32 agentInterrupt()
	{
		try {
			return agent.interrupt();
		} catch (const FlycastException& e) {
			throw Error(e.what());
		}
	}

	void clientConnected() {
		attached = true;
		agentInterrupt();
	}

	static void emuEventCallback(EmuEvent event, void *arg)
	{
		GdbServer *gdbServer = static_cast<GdbServer*>(arg);
		switch (event)
		{
		case EmuEvent::Resume:
			try {
				if (!gdbServer->isRunning())
					gdbServer->run();
			} catch (const GdbServer::Error& e) {
				ERROR_LOG(COMMON, "%s", e.what());
			}
			break;
		case EmuEvent::Terminate:
			gdbServer->stop();
			break;
		default:
			break;
		}
	}

	bool attached = false;
	bool postDebugTrapNeeded = false;
	std::thread thread;
	std::unique_ptr<asio::io_context> io_context;
	int port = DEFAULT_PORT;
	friend class TcpAcceptor;
	friend class Connection;

public:
	DebugAgent agent;
};

static GdbServer gdbServer;

void TcpAcceptor::handleAccept(Connection::Ptr newConnection, const std::error_code& error)
{
	if (!error) {
		server.clientConnected();
		newConnection->start();
	}
	start();
}

void Connection::handlePacket(const std::error_code& ec, size_t len)
{
	std::string msg = message.substr(0, len);
	message = message.substr(len);
	if (ec || len == 0)
	{
		// terminate the connection
		if (ec != asio::error::eof && ec != asio::error::operation_aborted)
			WARN_LOG(NETWORK, "Read error %s", ec.message().c_str());
		return;
	}
	try {
		if (msg[0] == '\03') {	// break
			send(server.handleCommand(msg));
			return;
		}
		if (msg[0] != '$') {
			// Ignore unexpected chars
			send("");
			return;
		}
		u8 cksum = 0;
		for (unsigned i = 1; i < len - 3; i++)
			cksum += (u8)msg[i];
		if (cksum != (unpack(msg[len - 2]) << 4 | unpack(msg[len - 1]))) {
			// Invalid checksum
			WARN_LOG(COMMON, "Connection::handlePacket: invalid checksum: [%s]", msg.c_str());
			send("-");
		}
		else {
			// Positive ack
			std::string reply = "+";
			reply += server.handleCommand(msg.substr(1, msg.length() - 4));
			send(reply);
		}
	} catch (...) {
		// terminate the connection
	}
}

void init(int port)
{
	gdbServer.init(port);
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

}
#endif
