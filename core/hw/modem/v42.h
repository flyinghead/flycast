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
#include "internal.h"
#include "v42bis.h"
#include "network/netservice.h"
#include <vector>
#include <deque>
#include <unordered_map>

namespace modem
{

class V14Encoder : public BufferedTransformer
{
public:
	void write(u8 v) override;
	void flush();
	void reset() override;

	void setStopBits(int txStopBits) {
		this->txStopBits = txStopBits;
	}

private:
	void transmitBit(u8 v);

	u8 txCurByte = 0;
	u32 txBitCount = 0;
	int txStopBits = 1;
};

class V14Decoder : public BufferedTransformer
{
public:
	void write(u8 v) override;
	int receivedStopBits();
	void reset() override;

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
};

class HdlcDecoder : public OutStream
{
public:
	void write(u8 v);
	bool frameAvailable() const { return !frames.empty(); }
	std::vector<u8> getFrame();
	void reset();

private:
	std::deque<std::vector<u8>> frames;
	std::vector<u8> rxFrame;
	int rxOnes = 0;
	int rxPosition = 0;
	u8 rxCurByte = 0;
};

class HdlcEncoder : public BufferedTransformer
{
public:
	void sendFrame(const std::vector<u8>& data);
	void sendFlag();
	void reset() override;

private:
	void sendByte(u8 byte);
	void write(u8 v) override {} // unused

	u32 txBitBuffer = 0;
	int txBitCount = 0;
};

class V42Protocol : public InStream, public OutStream
{
public:
	V42Protocol(InStream& in, OutStream& out)
	: inputStream(in), outputStream(out)
	{}

	int read() override;
	void write(u8 v) override;
	void reset();

	int available() override {
		return 1;
	}

private:
	InStream& inputStream;
	OutStream& outputStream;

	enum Phase {
		None,
		Detection,
		Establish,
		Connected,
		Release,
		V14			// Bomberman Online, Power Smash, Pro Yakyu Team de asobou net!
	};
	Phase phase = None;
	u8 lastRx = 0;
	int odpCount = 0;
	V14Encoder v14Encoder;
	V14Decoder v14Decoder;
	// for V14 mode
	PipeIn v14PipeIn { v14Encoder, inputStream };
	PipeOut v14PipeOut { v14Decoder, outputStream };
	// for V42 mode
	HdlcEncoder hdlcEncoder;
	HdlcDecoder hdlcDecoder;
	u8 txSeqNum = 0;
	u8 rxSeqNum = 0;
	u8 txSeqAck = 0;
	unsigned txMaxSize = 128;
	int txWindow = 15;
	std::unordered_map<int, std::vector<u8>> sentIFrames;
	// V42 bis
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

	void handleFrame();
	void handleSabme(u8 address, u8 control);
	void handleDisc(u8 address);
	void handleIFrame(const std::vector<u8>& frame);
	void handleXid(std::vector<u8> frame);
	void handleReject(u8 control2);

	void sendFlag() {
		hdlcEncoder.sendFlag();
	}
	void sendFrame(const std::vector<u8>& data) {
		hdlcEncoder.sendFrame(data);
	}

	void sendIFrame();
	void ackIFrame(int seqNum);

	friend class V42Test;
};

//
// V.8 bis is used by older browsers (Planet Web 1.125, DreamKey 1.0/1.5 and the many games that include them)
// and Windows CE games.
// To enable 56k, a non-standard info field must be returned indicating K56flex/V90 support.
//
class V8bisProtocol : public InStream, public OutStream
{
public:
	V8bisProtocol(InStream& in, OutStream& out)
		: inputStream(in), outputStream(out)
	{}

	void emitTone(u8 tone);
	u8 detectTone();
	int read() override;
	int available() override;
	void write(u8 v) override;
	void reset();

	bool completed() {
		return available() == 0 && done;
	}

private:
	void sendCL();
	void sendAck(int n = 1);
	void sendNak(int n = 1);

	InStream& inputStream;
	OutStream& outputStream;
	HdlcEncoder encoder;
	HdlcDecoder decoder;
	int toneState = 0;
	u64 toneTime = 0;
	bool dataMode = false;
	bool done = false;
};

}	// namespace modem
