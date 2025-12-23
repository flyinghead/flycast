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
#include "v42.h"
#include "network/netservice.h"
#include "hw/sh4/sh4_sched.h"

void V14Codec::transmitBit(u8 v)
{
	txCurByte = (txCurByte >> 1) | (v << 7);
	txBitCount++;
	if (txBitCount == 8)
	{
		txBuffer.push_back(txCurByte);
		txCurByte = 0;
		txBitCount = 0;
	}
}

int V14Codec::receive(u8 v)
{
	int ret = -1;
	for (int i = 0; i < 8; i++)
	{
		if (rxState == RX_STOP_BIT)
		{
			// waiting for 1st stop bit
			if ((v & 1) == 0)
			{
				if (charsSinceMissingStop < 4)
					INFO_LOG(MODEM, "V.14 stop bit missing after %d chars", charsSinceMissingStop);
				charsSinceMissingStop = 0;
				rxState = RX_RECEIVING;
				rxBitCount = 0;
				maxStopBits = std::max(maxStopBits, rxStopBits);
			}
			else {
				rxStopBits = 1;
				rxState = RX_IDLE;
			}
		}
		else if (rxState == RX_IDLE)
		{
			// idle or more stop bits state
			if ((v & 1) == 0)
			{
				 // start bit => now receiving
				rxState = RX_RECEIVING;
				rxBitCount = 0;
				maxStopBits = std::max(maxStopBits, rxStopBits);
			}
			else {
				rxStopBits++;
			}
		}
		else
		{
			rxCurByte = (rxCurByte >> 1) | ((v & 1) << 7);
			rxBitCount++;
			if (rxBitCount == 8)
			{
				rxState = RX_STOP_BIT;
				ret = rxCurByte;
				charsSinceMissingStop++;
				rxStopBits = 0;
			}
		}
		v >>= 1;
	}
	return ret;
}

int V14Codec::receivedStopBits()
{
	int max = maxStopBits;
	maxStopBits = 0;
	return max;
}

void V14Codec::transmit(u8 v)
{
	transmitBit(0); // start bit
	for (int i = 0; i < 8; i++) {
		transmitBit(v & 1);
		v >>= 1;
	}
	 // stop bit(s)
	for (int i = 0; i < txStopBits; i++)
		transmitBit(1);
}

void V14Codec::flush()
{
	if (txBitCount != 0) {
		while (txBitCount != 0)
			transmitBit(1);
	}
	else {
		for (int i = 0; i < 8; i++)
			transmitBit(1);
	}
}

int V14Codec::popTxBuffer()
{
	if (txBuffer.empty())
		return -1;
	int c = txBuffer.front();
	txBuffer.pop_front();
	return c;
}

void V14Codec::reset()
{
	rxCurByte = 0;
	rxState = RX_IDLE;
	rxBitCount = 0;
	charsSinceMissingStop = 0;
	rxStopBits = 0;
	maxStopBits = 0;

	txCurByte = 0;
	txBitCount = 0;
	txBuffer.clear();
	txStopBits = 1;
}

inline static bool lteMod128(int left, int right)
{
	if (left == right)
		return true;
	int rem = 128 - left;
	return (right + rem) % 128 <= 64;
}

inline static int incMod128(int v) {
	return (v + 1) % 128;
}

inline static int decMod128(int v) {
	return v == 0 ? 127 : v - 1;
}

void V42Protocol::Timer::start() {
	start_ = sh4_sched_now64();
}
bool V42Protocol::Timer::expired() {
	return sh4_sched_now64() - start_ >= duration;
}

void V42Protocol::receiveHdlc(u8 v)
{
	for (int bit = 0; bit < 8; bit++)
	{
		if (rxOnes >= 5)
		{
			if ((v & 1) == 1)
			{
				// we either have a flag or an abort
				rxCurByte = (rxCurByte >> 1) | 0x80;
				rxPosition++;
			}
			else
			{
				if (rxOnes == 6)
				{
					// we have a flag
					handleFrame();
					rxCurByte = 0;
					rxPosition = 0;
					rxFrame.clear();
				}
				else if (rxOnes > 6)
				{
					// abort
					INFO_LOG(MODEM, "HDLC abort received");
					rxCurByte = 0;
					rxPosition = 1; // keep the last 0
					rxFrame.clear();
				}
				// ignore stuffed 0
				rxOnes = 0;
			}
		}
		else {
			rxCurByte = (rxCurByte >> 1) | ((v & 1) << 7);
			rxPosition++;
		}
		if (rxPosition == 8)
		{
			rxFrame.push_back(rxCurByte);
			rxCurByte = 0;
			rxPosition = 0;
		}
		if ((v & 1) == 1)
			rxOnes++;
		else
			rxOnes = 0;
		v >>= 1;
	}
}

u16 V42Protocol::calcCrc16(const std::vector<u8>& data, size_t len)
{
	u16 crc = 0xffff;
	for (size_t i = 0; i < len; i++)
	{
	    u16 word = (crc ^ data[i]) & 0xFFu;
	    word = (u16)(word ^ (u16)((word << 4u) & 0xFFu));
	    word = (u16)((word << 8u) ^ (word << 3u) ^ (word >> 4u));
	    crc = ((crc >> 8) ^ word);
	}
	return crc ^ 0xffff;
}

void V42Protocol::handleFrame()
{
	if (rxFrame.empty())
		return;
	if (rxFrame.size() < 4) {
		WARN_LOG(MODEM, "Invalid frame: %d bytes", (int)rxFrame.size());
		return;
	}
	u16 crc = calcCrc16(rxFrame, rxFrame.size() - 2);
	if (crc != ((rxFrame[rxFrame.size() - 1] << 8) | rxFrame[rxFrame.size() - 2]))
	{
		WARN_LOG(MODEM, "Invalid frame: wrong crc16 %04x should be %04x", crc, ((rxFrame[rxFrame.size() - 1] << 8) | rxFrame[rxFrame.size() - 2]));
		return;
	}
	//printf("Received frame: "); for (u8 v : rxFrame) printf("%02x ", v & 0xff); printf("\n");
	if ((rxFrame[0] & 1) == 0) {
		WARN_LOG(MODEM, "Invalid frame: Unexpected extended address");
		return;
	}
	if ((rxFrame[0] >> 2) != 0) {
		WARN_LOG(MODEM, "Invalid frame: Unknown address %02x", rxFrame[0] >> 2);
		return;
	}
	DEBUG_LOG(MODEM, "Received frame: addr %02x control %02x FCS %02x%02x", rxFrame[0], rxFrame[1], rxFrame[rxFrame.size() - 1], rxFrame[rxFrame.size() - 2]);

	if ((rxFrame[1] & 3) != 3 && rxFrame[1] != 0xd)
	{
		// Information and Supervisory commands (except SREJ) acknowledge sent frames
		ackIFrame(rxFrame[2] >> 1);
	}
	inactivityTimer.start();

	switch (rxFrame[1] & 0xef)
	{
	case 0x01:	// RR
		break;
	case 0x05:	// RNR
		WARN_LOG(MODEM, "Received RNR");
		break;
	case 0x09:	// REJECT
		handleReject();
		break;
	case 0x0d:	// SREJ
		WARN_LOG(MODEM, "Received SREJ");
		break;
	case 0x6f:	// SABME
		handleSabme();
		break;
	case 0x0f:	// DM
		break;
	case 0x03:	// UI
		WARN_LOG(MODEM, "Received UI");
		break;
	case 0x43:	// DISC
		handleDisc();
		break;
	case 0x63:	// UA
		break;
	case 0x87:	// FRMR
		WARN_LOG(MODEM, "Received FRMR");
		break;
	case 0xaf:	// XID
		handleXid();
		break;
	case 0xe3:	// TEST
		break;
	default:
		if (rxFrame[1] & 1)
		{
			WARN_LOG(MODEM, "Invalid HDLC command: %02x", rxFrame[1]);
			return;
		}
		// I-frame
		if (phase != Connected) {
			WARN_LOG(MODEM, "I-frame received but not connected");
			return;
		}
		handleIFrame();
		break;
	}
}

void V42Protocol::handleSabme()
{
	if (phase == Connected)
		WARN_LOG(MODEM, "V.42: SABME received while already connected");
	else
		INFO_LOG(MODEM, "V.42: Received SABME");
	std::vector<u8> ua { rxFrame[0], (u8)(0x63 | (rxFrame[1] & 0x10)) };
	sendFrame(ua);

	phase = Connected;
	txSeqNum = 0;
	rxSeqNum = 0;
	txSeqAck = 0;
}

void V42Protocol::handleDisc()
{
	INFO_LOG(MODEM, "V.42: Received DISC");
	phase = Release;
	std::vector<u8> ua { rxFrame[0], 0x73 };
	sendFrame(ua);
}

void V42Protocol::handleIFrame()
{
	int seqNum = rxFrame[1] >> 1;
	if (seqNum != rxSeqNum)
	{
		INFO_LOG(MODEM, "Received frame num out of sequence: expected %d but recv %d", seqNum, rxSeqNum);
		// send REJECT
		std::vector<u8> reject { rxFrame[0], 9, rxSeqNum };
		sendFrame(reject);
		return;
	}
	rxSeqNum = incMod128(rxSeqNum);
	for (auto it = rxFrame.begin() + 3; it < rxFrame.end() - 2; ++it)
		net::modbba::writeModem(*it);
	// if P bit set, respond with RR
	// if not set, respond with RR if no I-frame available to send
	if ((rxFrame[2] & 1) || net::modbba::modemAvailable() == 0)
	{
		std::vector<u8> rr { rxFrame[0], 1, u8((rxSeqNum << 1) | (rxFrame[2] & 1)) };
		sendFrame(rr);
	}
}

void V42Protocol::handleXid()
{
	// format identifier
	if (rxFrame[2] != 0x82) {
		WARN_LOG(MODEM, "Unexpected XID format: %02x", rxFrame[2]);
		return;
	}
	size_t userDataOffset = 0;
	// Iterate data link layer subfields
	for (size_t i = 3; i < rxFrame.size() - 2;)
	{
		u16 groupSize = rxFrame[i + 1] * 256 + rxFrame[i + 2];
		bool privParam;
		bool userDataParam;
		// Group identifier
		switch (rxFrame[i])
		{
		case 0x80: // parameter negotiation
			privParam = false;
			userDataParam = false;
			break;
		case 0xf0: // private parameter negotiation
			privParam = true;
			userDataParam = false;
			break;
		case 0xff: // user data negotiation
			privParam = false;
			userDataParam = true;
			userDataOffset = i;
			break;
		default:
			INFO_LOG(MODEM, "Unexpected XID GI: %02x", rxFrame[i]);
			i += groupSize + 3;
			continue;
			break;
		}
		i += 3;
		const size_t end = i + groupSize;
		while (i < end)
		{
			u8 paramId = rxFrame[i++];
			u8 paramSize = rxFrame[i++];
			switch (paramId)
			{
			case 0:	// (private) Parameter set identification
				break;
			case 1:	// (private) Data compression request
				if (privParam)
				{
					DEBUG_LOG(MODEM, "xid: Data compression request %x", rxFrame[i]);
					// TODO set compression to None for now
					rxFrame[i] = 0;
				}
				break;
			case 2:	// (private) Number of codewords
				if (privParam)
				{
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v = (v << 8) | rxFrame[i + j];
						DEBUG_LOG(MODEM, "xid: Number of codewords %d", v);
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				break;
			case 3:
				if (!privParam && !userDataParam)
				{
					// HDLC optional function (public)
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v |= rxFrame[i + j] << (8 * j);
						DEBUG_LOG(MODEM, "xid: HDLC optional function %x", v);
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				else if (privParam)
				{
					// (private) Maximum string length
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v = (v << 8) | rxFrame[i + j];
						DEBUG_LOG(MODEM, "xid: Maximum string length %d", v);
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				break;
			case 5:	// Maximum length of information field: tx
				if (!privParam && !userDataParam)
				{
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v = (v << 8) | rxFrame[i + j];
						txMaxSize = v / 8;
						DEBUG_LOG(MODEM, "xid: Max tx len %d bits", v);
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				break;
			case 6:	// Maximum length of information field: rx
				if (!privParam && !userDataParam)
				{
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v = (v << 8) | rxFrame[i + j];
						DEBUG_LOG(MODEM, "xid: Max rx len %d bits", v);
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				break;
			case 7:	// Window size: tx
				if (!privParam && !userDataParam)
				{
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v = (v << 8) | rxFrame[i + j];
						txWindow = v;
						DEBUG_LOG(MODEM, "xid: Tx window %d", v);
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				break;
			case 8:	// Window size: rx
				if (!privParam && !userDataParam)
				{
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v = (v << 8) | rxFrame[i + j];
						DEBUG_LOG(MODEM, "xid: Rx window %d", v);
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				break;
			default:
				WARN_LOG(MODEM, "Unexpected PI %d (size %x, private %d, user data %d)", paramId, paramSize, privParam, userDataParam);
				break;
			}
			i += paramSize;
		}
	}
	if (userDataOffset != 0)
		rxFrame.resize(userDataOffset);
	sendFrame(rxFrame);
}

void V42Protocol::handleReject()
{
	int seq = rxFrame[2] >> 1;
	INFO_LOG(MODEM, "Received REJECT %d", seq);
	while (lteMod128(seq, decMod128(txSeqNum)))
	{
		auto it = sentIFrames.find(seq);
		if (it == sentIFrames.end()) {
			WARN_LOG(MODEM, "REJ frame not found");
			continue;
		}
		std::vector<u8>& frame = it->second;
		frame[2] = rxSeqNum << 1;
		sendFrame(frame);
		seq = incMod128(seq);
	}
}

void V42Protocol::sendBit(u8 b)
{
	txCurByte = (txCurByte >> 1) | ((b & 1) << 7);
	if (++txPosition == 8)
	{
		txPosition = 0;
		txBuffer.push_back(txCurByte);
		txCurByte = 0;
	}
	if (b == 1) {
		if (++txOnes == 5)
			sendBit(0);
	}
	else {
		txOnes = 0;
	}
}

void V42Protocol::sendFlag()
{
	for (int i = 0; i < 8; i++) {
		sendBit((0x7e >> i) & 1);
		txOnes = 0;
	}
}

void V42Protocol::sendFrame(const std::vector<u8>& data)
{
	// opening flag
	sendFlag();
	// data
	for (u8 b : data)
	{
		for (int i = 0; i < 8; i++) {
			sendBit(b & 1);
			b >>= 1;
		}
	}
	// crc
	u16 crc = calcCrc16(data, data.size());
	for (int i = 0; i < 16; i++) {
		sendBit(crc & 1);
		crc >>= 1;
	}
	// closing flag
	sendFlag();
	inactivityTimer.start();
}

void V42Protocol::sendIFrame()
{
	using namespace net::modbba;
	if (modemAvailable() == 0)
		return;
	// check that tx window isn't reached
	int window = txSeqNum - txSeqAck;
	if (window < 0)
		window =+ 128;
	if (window >= txWindow)
		return;
	std::vector<u8> frame;
	frame.reserve(modemAvailable() + 3);
	frame.push_back(1);
	DEBUG_LOG(MODEM, "Sending I-frame %d", txSeqNum);
	frame.push_back(txSeqNum << 1);
	txSeqNum = incMod128(txSeqNum);
	frame.push_back(rxSeqNum << 1);
	while (frame.size() - 3u < txMaxSize)
	{
		int c = readModem();
		if (c == -1)
			break;
		frame.push_back(c & 0xff);
	}
	sendFrame(frame);
	sentIFrames[frame[1] >> 1] = frame;
}

int V42Protocol::transmit()
{
	if (phase == None || phase == Detection || phase == V14)
	{
		if (phase == V14)
		{
			int c = net::modbba::readModem();
			if (c != -1)
				v14codec.transmit(c);
			else
				v14codec.flush();
		}
		else {
			v14codec.flush();
		}
		return v14codec.popTxBuffer();
	}
	if (txBuffer.empty())
	{
		if (phase == Connected)
		{
			sendIFrame();
			if (txBuffer.empty() && inactivityTimer.expired())
			{
				// send an RR frame with P bit set
				std::vector<u8> rr { 1, 1, u8((rxSeqNum << 1) | 1) };
				sendFrame(rr);
			}
		}
		if (txBuffer.empty())
			sendFlag();
	}
	u8 v = txBuffer.front();
	txBuffer.pop_front();
	return v;
}

void V42Protocol::receive(u8 v)
{
	if (phase == None || phase == Detection)
	{
		if (detectionTimer.expired()) {
			INFO_LOG(MODEM, "Switching to V.14 mode");
			phase = V14;
		}
		else
		{
			int c = v14codec.receive(v);
			if (c == -1)
				return;
			v = c;
		}
	}
	if (phase == None)
	{
		if ((v == 0x11 || v == 0x91)
				&& (v14codec.receivedStopBits() == 9 || v14codec.receivedStopBits() == 17))
		{
			phase = Detection;
			lastRx = v;
			odpCount = 1;
		}
	}
	else if (phase == Detection)
	{
		// V.42 ODP: DC1 with alternating parity followed by 8+1 or 16+1 ones
		if ((v == 0x11 || v == 0x91)
				&& (v14codec.receivedStopBits() == 9 || v14codec.receivedStopBits() == 17)
				&& v != lastRx)
		{
			odpCount++;
			if (odpCount == 4)
			{
				odpCount = 0;
				// send ADP
				v14codec.setStopBits(9);
				v14codec.transmit('E');
				v14codec.transmit('C');
				//v14codec.transmit(0);	// switches to v14 only and start ppp nego
				v14codec.setStopBits(1);
			}
		}
		else
		{
			odpCount = 0;
			if (v == 0x7e)
				phase = Establish;
		}
		lastRx = v;
	}
	else if (phase == Establish || phase == Connected) {
		receiveHdlc(v);
	}
	else if (phase == V14)
	{
		int c = v14codec.receive(v);
		if (c != -1)
			net::modbba::writeModem(c);
	}
}

void V42Protocol::reset()
{
	phase = None;
	lastRx = 0;
	odpCount = 0;
	txBuffer.clear();
	v14codec.reset();
	rxFrame.clear();
	rxOnes = 0;
	rxPosition = 0;
	rxCurByte = 0;
	txOnes = 0;
	txPosition = 0;
	txCurByte = 0;
	txSeqNum = 0;
	rxSeqNum = 0;
	txSeqAck = 0;
	txMaxSize = 128;
	txWindow = 15;
	sentIFrames.clear();
	detectionTimer.start();
}

void V42Protocol::ackIFrame(int seqNum)
{
	if (lteMod128(txSeqAck, seqNum))
	{
		for (int seq = decMod128(txSeqAck); lteMod128(seq, decMod128(seqNum)); seq = incMod128(seq))
			sentIFrames.erase(seq);
		txSeqAck = seqNum;
	}
	else {
		WARN_LOG(MODEM, "Ack seq# %d < prev acked %d", seqNum, txSeqAck);
	}
}
