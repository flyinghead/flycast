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
#pragma once
#include "types.h"
#include "v42bis.h"
#include <vector>
#include <deque>
#include <unordered_map>

class V14Codec
{
public:
	int receive(u8 v);

	void setStopBits(int txStopBits) {
		this->txStopBits = txStopBits;
	}

	int receivedStopBits();
	void transmit(u8 v);
	void flush();

	bool txBufferEmpty() const {
		return txBuffer.empty();
	}

	int popTxBuffer();
	void reset();

private:
	enum RxState {
		RX_IDLE,
		RX_RECEIVING,
		RX_STOP_BIT
	};
	u8 rxCurByte = 0;
	RxState rxState = RX_IDLE;
	u32 rxBitCount = 0;
	int charsSinceMissingStop = 0;
	int rxStopBits = 0;
	int maxStopBits = 0;

	u8 txCurByte = 0;
	u32 txBitCount = 0;
	std::deque<u8> txBuffer;
	int txStopBits = 1;

	void transmitBit(u8 v);
};

class V42Protocol
{
public:
	int transmit();
	void receive(u8 v);
	void reset();

private:
	enum Phase {
		None,
		Detection,
		Establish,
		Connected,
		Release,
		V14
	};
	Phase phase = None;
	u8 lastRx = 0;
	int odpCount = 0;
	std::deque<u8> txBuffer;
	V14Codec v14codec;
	std::vector<u8> rxFrame;
	int rxOnes = 0;
	int rxPosition = 0;
	u8 rxCurByte = 0;
	u32 txBitBuffer = 0;
	int txBitCount = 0;
	u8 txSeqNum = 0;
	u8 rxSeqNum = 0;
	u8 txSeqAck = 0;
	unsigned txMaxSize = 128;
	int txWindow = 15;
	std::unordered_map<int, std::vector<u8>> sentIFrames;
	v42b::Compressor compressor;
	v42b::Decompressor decompressor;
	bool compressionEnabled = false;

	class Timer
	{
		u64 start_ = 0;
		u64 duration;

	public:
		Timer(u64 duration)
			: duration(duration * 200000)
		{}

		void start();
		bool expired();
	};
	Timer detectionTimer { 750 };	// Detection phase timer (T400)
	Timer inactivityTimer { 1000 }; // Inactivity timer (T403)

	void receiveHdlc(u8 v);
	u16 calcCrc16(const std::vector<u8>& data, size_t len);
	void handleFrame();
	void handleSabme();
	void handleDisc();
	void handleIFrame();
	void handleXid();
	void handleReject();
	void sendByte(u8 byte);
	void sendFlag();
	void sendFrame(const std::vector<u8>& data);
	void sendIFrame();
	void ackIFrame(int seqNum);

	friend class V42Test;
};

