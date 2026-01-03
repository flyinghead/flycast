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

namespace modem
{

void V14Encoder::write(u8 v)
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

void V14Encoder::flush()
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

void V14Encoder::reset()
{
	BufferedTransformer::reset();
	txCurByte = 0;
	txBitCount = 0;
	txStopBits = 1;
}

void V14Encoder::transmitBit(u8 v)
{
	txCurByte = (txCurByte >> 1) | (v << 7);
	txBitCount++;
	if (txBitCount == 8)
	{
		buffer.push_back(txCurByte);
		txCurByte = 0;
		txBitCount = 0;
	}
}

void V14Decoder::write(u8 v)
{
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
				buffer.push_back(rxCurByte);
				charsSinceMissingStop++;
				rxStopBits = 0;
			}
		}
		v >>= 1;
	}
}

int V14Decoder::receivedStopBits()
{
	int max = maxStopBits;
	maxStopBits = 0;
	return max;
}

void V14Decoder::reset()
{
	BufferedTransformer::reset();
	rxCurByte = 0;
	rxState = RX_IDLE;
	rxBitCount = 0;
	charsSinceMissingStop = 0;
	rxStopBits = 0;
	maxStopBits = 0;
}

static u16 calcCrc16(const std::vector<u8>& data, size_t len = 0)
{
	if (len == 0)
		len = data.size();
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

void HdlcEncoder::sendFrame(const std::vector<u8>& data)
{
	// opening flag
	sendFlag();
	// data
	for (u8 b : data)
		sendByte(b);
	// crc
	u16 crc = calcCrc16(data);
	sendByte(crc & 0xff);
	sendByte(crc >> 8);
	// closing flag
	sendFlag();
}

void HdlcEncoder::sendFlag()
{
	txBitBuffer |= 0x7e << (8 + txBitCount);
	txBitBuffer >>= 8;
	buffer.push_back(txBitBuffer & 0xff);
}

void HdlcEncoder::sendByte(u8 byte)
{
	txBitBuffer |= (u32)byte << (8 + txBitCount);
	u32 mask = 0x1f0 << txBitCount;
	for (int i = 0; i < 8; i++)
	{
		if ((txBitBuffer & mask) == mask)
		{
			u32 lShiftMask = 0xffffffff << (8 + txBitCount + i + 1);
			txBitBuffer = ((txBitBuffer & lShiftMask) << 1) | (txBitBuffer & ~lShiftMask);
			txBitCount++;
			mask <<= 1;
		}
		mask <<= 1;
	}
	txBitCount += 8;
	while (txBitCount >= 8)
	{
		txBitBuffer >>= 8;
		buffer.push_back(txBitBuffer & 0xff);
		txBitCount -= 8;
	}
}

void HdlcEncoder::reset()
{
	BufferedTransformer::reset();
	txBitCount = 0;
	txBitBuffer = 0;
}

void HdlcDecoder::write(u8 v)
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
					if (!rxFrame.empty())
					{
						u16 crc = calcCrc16(rxFrame, rxFrame.size() - 2);
						if (crc != *(u16 *)(&rxFrame.back() - 1)) {
							WARN_LOG(MODEM, "Invalid CRC in received frame");
						}
						else
						{
							rxFrame.pop_back();
							rxFrame.pop_back();
							frames.push_back(std::move(rxFrame));
						}
						rxFrame.clear();
					}
					rxCurByte = 0;
					rxPosition = 0;
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

std::vector<u8> HdlcDecoder::getFrame()
{
	if (frames.empty())
		return {};
	std::vector<u8> frame = std::move(frames.front());
	frames.pop_front();
	return frame;
}

void HdlcDecoder::reset()
{
	frames.clear();
	rxFrame.clear();
	rxPosition = 0;
	rxCurByte = 0;
	rxOnes = 0;
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

void V42Protocol::handleFrame()
{
	if (!hdlcDecoder.frameAvailable())
		return;
	std::vector<u8> rxFrame = hdlcDecoder.getFrame();
	if (rxFrame.size() < 2) {
		WARN_LOG(MODEM, "Invalid frame: %d bytes", (int)rxFrame.size());
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
	//DEBUG_LOG(MODEM, "Received frame: addr %02x control %02x FCS %02x%02x", rxFrame[0], rxFrame[1], rxFrame[rxFrame.size() - 1], rxFrame[rxFrame.size() - 2]);

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
		handleReject(rxFrame[2]);
		break;
	case 0x0d:	// SREJ
		WARN_LOG(MODEM, "Received SREJ");
		break;
	case 0x6f:	// SABME
		handleSabme(rxFrame[0], rxFrame[1]);
		break;
	case 0x0f:	// DM
		break;
	case 0x03:	// UI
		WARN_LOG(MODEM, "Received UI");
		break;
	case 0x43:	// DISC
		handleDisc(rxFrame[0]);
		break;
	case 0x63:	// UA
		break;
	case 0x87:	// FRMR
		WARN_LOG(MODEM, "Received FRMR");
		break;
	case 0xaf:	// XID
		handleXid(rxFrame);
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
		handleIFrame(rxFrame);
		break;
	}
}

void V42Protocol::handleSabme(u8 address, u8 control)
{
	if (phase == Connected)
		WARN_LOG(MODEM, "V.42: SABME received while already connected");
	else
		INFO_LOG(MODEM, "V.42: Received SABME");
	std::vector<u8> ua { address, (u8)(0x63 | (control & 0x10)) };
	sendFrame(ua);

	phase = Connected;
	txSeqNum = 0;
	rxSeqNum = 0;
	txSeqAck = 0;
}

void V42Protocol::handleDisc(u8 address)
{
	INFO_LOG(MODEM, "V.42: Received DISC");
	phase = Release;
	std::vector<u8> ua { address, 0x73 };
	sendFrame(ua);
}

void V42Protocol::handleIFrame(const std::vector<u8>& rxFrame)
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
	if (compressionEnabled)
	{
		try {
			for (auto it = rxFrame.begin() + 3; it < rxFrame.end(); ++it)
				decompressor.write(*it);
			while (true)
			{
				int c = decompressor.read();
				if (c == -1)
					break;
				outputStream.write(c);
			}
		} catch (const v42b::Exception& e) {
			// send DISC
			std::vector<u8> disc { 1, 0x43 };
			sendFrame(disc);
			phase = Release;
			sentIFrames.clear();
			return;
		}
	}
	else {
		for (auto it = rxFrame.begin() + 3; it < rxFrame.end(); ++it)
			outputStream.write(*it);
	}

	// if P bit set, respond with RR
	// if not set, respond with RR if no I-frame available to send
	if ((rxFrame[2] & 1) || inputStream.available() == 0)
	{
		std::vector<u8> rr { rxFrame[0], 1, u8((rxSeqNum << 1) | (rxFrame[2] & 1)) };
		sendFrame(rr);
	}
}

void V42Protocol::handleXid(std::vector<u8> rxFrame)
{
	u32 maxCodeWords = 512;
	int maxStringLength = 6;
	compressionEnabled = false;

	// format identifier
	if (rxFrame[2] != 0x82) {
		WARN_LOG(MODEM, "Unexpected XID format: %02x", rxFrame[2]);
		return;
	}
	size_t userDataOffset = 0;
	// Iterate over parameter groups
	for (size_t i = 3; i < rxFrame.size() - 2;)
	{
		u16 groupSize = rxFrame[i + 1] * 256 + rxFrame[i + 2];
		// Group identifier
		enum GroupId : u8 {
			GID_PARAM = 0x80,
			GID_PRIVATE = 0xf0,
			GID_USER_DATA = 0xff,
		};
		GroupId gid = (GroupId)rxFrame[i];
		if (gid == GID_USER_DATA) {
			userDataOffset = i;
		}
		else if (gid != GID_PARAM && gid != GID_PRIVATE)
		{
			INFO_LOG(MODEM, "Unexpected XID GI: %02x", rxFrame[i]);
			i += groupSize + 3;
			continue;
		}
		i += 3;
		// Parameters
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
				if (gid == GID_PRIVATE)
				{
					DEBUG_LOG(MODEM, "xid: Data compression request %x", rxFrame[i]);
					if (rxFrame[i] == 3)
						compressionEnabled = true;
					else
						// Disable all compression if only one direction is compressed (not used anyway)
						rxFrame[i] = 0;
				}
				break;
			case 2:	// (private) Number of codewords
				if (gid == GID_PRIVATE)
				{
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v = (v << 8) | rxFrame[i + j];
						DEBUG_LOG(MODEM, "xid: Number of codewords %d", v);
						maxCodeWords = v;
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				break;
			case 3:
				if (gid == GID_PARAM)
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
				else if (gid == GID_PRIVATE)
				{
					// (private) Maximum string length
					if (paramSize <= 4)
					{
						u32 v = 0;
						for (u8 j = 0; j < paramSize; j++)
							v = (v << 8) | rxFrame[i + j];
						DEBUG_LOG(MODEM, "xid: Maximum string length %d", v);
						maxStringLength = v;
					}
					else {
						WARN_LOG(MODEM, "Unexpected param length for PI %d: %x", paramId, paramSize);
					}
				}
				break;
			case 5:	// Maximum length of information field: tx
				if (gid == GID_PARAM)
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
				if (gid == GID_PARAM)
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
				if (gid == GID_PARAM)
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
				if (gid == GID_PARAM)
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
				WARN_LOG(MODEM, "Unexpected PI %d (size %x, group id %x)", paramId, paramSize, gid);
				break;
			}
			i += paramSize;
		}
	}
	if (userDataOffset != 0)
		rxFrame.resize(userDataOffset);
	sendFrame(rxFrame);

	if (compressionEnabled)
	{
		NOTICE_LOG(MODEM, "V.42bis compression enabled: max dictionary size %d", maxCodeWords);
		compressor = { maxCodeWords, maxStringLength };
		decompressor = { maxCodeWords, maxStringLength };
	}
}

void V42Protocol::handleReject(u8 control2)
{
	int seq = control2 >> 1;
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

void V42Protocol::sendIFrame()
{
	if (inputStream.available() == 0)
		return;
	// check that tx window isn't reached
	int window = txSeqNum - txSeqAck;
	if (window < 0)
		window =+ 128;
	if (window >= txWindow)
		return;

	std::vector<u8> frame;
	frame.reserve(inputStream.available() + 3);
	frame.push_back(1);
	frame.push_back(txSeqNum << 1);
	txSeqNum = incMod128(txSeqNum);
	frame.push_back(rxSeqNum << 1);
	if (compressionEnabled)
	{
		while (compressor.available() < (int)txMaxSize)
		{
			int c = inputStream.read();
			if (c == -1)
				break;
			compressor.write(c);
		}
		compressor.flush();
		while (frame.size() - 3u < txMaxSize)
		{
			int cc = compressor.read();
			if (cc == -1)
				break;
			frame.push_back(cc & 0xff);
		}
		if (frame.size() == 3)
		{
			// don't send an empty frame
			txSeqNum = decMod128(txSeqNum);
			return;
		}
	}
	else
	{
		while (frame.size() - 3u < txMaxSize)
		{
			int c = inputStream.read();
			if (c == -1)
				break;
			frame.push_back(c & 0xff);
		}
	}
	//DEBUG_LOG(MODEM, "Sending I-frame %d", frame[1] >> 1);
	sendFrame(frame);
	sentIFrames[frame[1] >> 1] = frame;
}

int V42Protocol::read()
{
	if (phase == None || phase == Detection) {
		v14Encoder.flush();
		return v14Encoder.read();
	}
	else if (phase == V14)
	{
		if (v14PipeIn.available() == 0)
			v14Encoder.flush();
		return v14PipeIn.read();
	}
	if (hdlcEncoder.available() == 0)
	{
		if (phase == Connected)
		{
			sendIFrame();
			if (hdlcEncoder.available() != 0)
				return hdlcEncoder.read();
			if (inactivityTimer.expired())
			{
				// send an RR frame with P bit set
				std::vector<u8> rr { 1, 1, u8((rxSeqNum << 1) | 1) };
				sendFrame(rr);
				if (hdlcEncoder.available() != 0)
					return hdlcEncoder.read();
			}
		}
		sendFlag();
	}
	return hdlcEncoder.read();
}

void V42Protocol::write(u8 v)
{
	if (phase == None || phase == Detection)
	{
		if (detectionTimer.expired()) {
			INFO_LOG(MODEM, "Switching to V.14 mode");
			phase = V14;
		}
		else
		{
			v14Decoder.write(v);
			int c = v14Decoder.read();
			if (c == -1)
				return;
			v = c;
		}
	}
	if (phase == None)
	{
		if ((v == 0x11 || v == 0x91)
				&& (v14Decoder.receivedStopBits() == 9 || v14Decoder.receivedStopBits() == 17))
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
				&& (v14Decoder.receivedStopBits() == 9 || v14Decoder.receivedStopBits() == 17)
				&& v != lastRx)
		{
			odpCount++;
			if (odpCount == 4)
			{
				odpCount = 0;
				// send ADP
				v14Encoder.setStopBits(9);
				v14Encoder.write('E');
				v14Encoder.write('C');
				//v14Encoder.write(0);	// switches to v14 only and start ppp nego
				v14Encoder.setStopBits(1);
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
		hdlcDecoder.write(v);
		handleFrame();
	}
	else if (phase == V14) {
		v14PipeOut.write(v);
	}
}

void V42Protocol::reset()
{
	phase = None;
	lastRx = 0;
	odpCount = 0;
	v14Encoder.reset();
	v14Decoder.reset();
	hdlcEncoder.reset();
	hdlcDecoder.reset();
	txSeqNum = 0;
	rxSeqNum = 0;
	txSeqAck = 0;
	txMaxSize = 128;
	txWindow = 15;
	sentIFrames.clear();
	detectionTimer.start();
	compressor.reset();
	decompressor.reset();
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

u8 V8bisProtocol::detectTone()
{
	switch (toneState)
	{
	case 0:
		if (toneTime == 0) {
			toneTime = sh4_sched_now64();
		}
		else if (sh4_sched_now64() - toneTime >= SH4_MAIN_CLOCK / 1000 * 400) {
			toneState++;
			toneTime = sh4_sched_now64();
		}
		break;

	case 1: // Segment 1 dual tone received and single tone segment 2 is being detected
		if (sh4_sched_now64() - toneTime >= SH4_MAIN_CLOCK / 1000 * 100) {
			toneState++;
			toneTime = sh4_sched_now64();
		}
		return 0xE0;

	case 2:
		return 0x20; // Mode Request (MRe) received
	}
	return 0;
}

void V8bisProtocol::sendCL()
{
	dataMode = true;
	std::vector<u8> cl = {
			0x12,	// CL, rev.1
			//0x13,	// CLR, rev.1
					// identification field (I)
			0xc3,	// NPar1: V.8, short V.8, non-standard field
			0x80,	// SPar1: network type not specified
					// standard information field (S)
			0x80,	// NPar1
			0x01,	// SPar2[1]: data
			0x80,	// SPar2[2]
			  0x0F,	// NPar2[1]: transparent data, V.42, V.42 bis, V.14
			  0x30,	// NPar2[2]: V.34, V.32 bis
			  0xFF,	// NPar2[3]: V.32, V.22 bis, V.22, V.21, V.90 digital+ana
			  //0xC0,	// V.8 bis rev.2: NPar2[4]
			  	  	// non-standard information (NS)
			0x09,	// size
			0xb5,	// country code (USA)
			0x02,	// manuf. code len
			0x00,	// manufacturer code (K56flex)
			0x94,
			0x81,	// licensee code (Rockwell)
			0x83,	// capabilities (K56flex, V90?)
			0x43,	// K56flex version number
			0x47,	// Rockwell MDP version number
			0xC4,	// u-law (none) and controller version number (4)
	};
	encoder.sendFlag();
	encoder.sendFrame(cl);
	encoder.sendFlag();
	encoder.sendFlag();
}

void V8bisProtocol::sendAck(int n)
{
	encoder.sendFlag();
	encoder.sendFrame({ u8(0x13 + n) }); // 0x14: ACK(1), 0x15: ACK(2)
	encoder.sendFlag();
	encoder.sendFlag();
}

void V8bisProtocol::sendNak(int n)
{
	encoder.sendFlag();
	encoder.sendFrame({ u8(7 + n) }); // 8: NAK(1), 9: NAK(2), ...
	encoder.sendFlag();
	encoder.sendFlag();
}

void V8bisProtocol::emitTone(u8 tone)
{
	if (toneState == 2 && tone == 0x33) // emit Capabilities Request (CRd)
		// should send CL or CLR
		sendCL();
}

int V8bisProtocol::read()
{
	if (!dataMode)
		return -1;
	else
		return encoder.read();
}

int V8bisProtocol::available()
{
	if (!dataMode)
		return 0;
	else
		return encoder.available();
}

static unsigned paramSize(const u8 *begin, const u8 *end, unsigned delimMask = 0x80)
{
	const u8 *p = begin;
	while (p < end && (*p & delimMask) == 0)
		p++;
	return p - begin + 1;
}

void V8bisProtocol::write(u8 v)
{
	if (!dataMode)
		return;

	decoder.write(v);
	if (!decoder.frameAvailable())
		return;

	std::vector<u8> frame = decoder.getFrame();
	u8 frameType = frame[0] & 0xf;
	switch (frameType)
	{
	case 1: // MS
		{
			// identification field (I)
			// NPar1
			const u8 *p = &frame[1];
			bool v8 = *p & 1;
			bool shortV8 = *p & 2;
			bool txAck = *p & 8;
			bool hasNSF = *p & 0x40;
			unsigned sz = paramSize(p, &frame.back() + 1);
			p += sz;
			sz = paramSize(p, &frame.back() + 1);
			if (*p & 0x7f) {
				p += sz;
				p += paramSize(p, &frame.back() + 1); // NPar2's
			}
			else {
				p += sz;
			}
			// standard info field (S)
			p += paramSize(p, &frame.back() + 1); // NPar1
			sz = paramSize(p, &frame.back() + 1);
			u8 spar1 = *p;
			if ((spar1 & 1) == 0)
			{
				WARN_LOG(MODEM, "V.8 bis MS: Data bit not set in field S - SPar1");
				sendNak();
				break;
			}
			p += sz;
			bool v42 = *p & 2;
			bool v42bis = *p & 4;
			bool v14 = *p & 8;
			int speed = 0;
			if ((*p & 0xc0) == 0)
			{
				++p;
				if (*p & 0x10)
					// V34
					speed = 33600;
				else if (*p & 0x20)
					// V32 bis
					speed = 14400;
				if (speed == 0 && (*p & 0xc0) == 0)
				{
					++p;
					if (*p & 1)
						// V32
						speed = 9600;
					else if (*p & 2)
						// V22 bis
						speed = 2400;
					else if (*p & 4)
						// V22
						speed = 1200;
					else if (*p & 8)
						// V21
						speed = 300;
				}
			}
			p += paramSize(p, &frame.back() + 1);
			if (hasNSF && *p >= 5 && p[2] == 2)
			{
				if (p[3] == 0 && p[4] == 0x94)
					// K56flex / V90
					speed = 56000;
			}
			p += *p;
			if (txAck)
				sendAck(1);
			DEBUG_LOG(MODEM, "V8bis: received MS: V8 %d sV8 %d V14 %d V42 %d V42bis %d speed %d",
					v8, shortV8, v14, v42, v42bis, speed);
		}
		break;
	default:
		WARN_LOG(MODEM, "Unhandled V.8 bis frame type %d", frameType);
		break;
	}
	done = true;
}

void V8bisProtocol::reset()
{
	encoder.reset();
	decoder.reset();
	dataMode = false;
	toneState = 0;
	toneTime = 0;
	done = false;
}

}	// namespace modem
