#pragma once
#include "hw/hwreg.h"
#include <deque>

extern u32 UBC[9];
extern u32 BSC[19];
extern u32 CPG[5];
extern u32 RTC[16];
extern u32 INTC[5];
extern u32 TMU[12];
extern u32 SCI[8];
extern u32 SCIF[10];

class UBCRegisters : public RegisterBank<UBC, 9>
{
	using super = RegisterBank<UBC, 9>;

public:
	void init();
};
extern UBCRegisters ubc;

class BSCRegisters : public RegisterBank<BSC, 19>
{
	using super = RegisterBank<BSC, 19>;

public:
	void init();
	void reset();
};
extern BSCRegisters bsc;

class CPGRegisters : public RegisterBank<CPG, 5>
{
	using super = RegisterBank<CPG, 5>;

public:
	void init();
};
extern CPGRegisters cpg;

class RTCRegisters : public RegisterBank<RTC, 16>
{
	using super = RegisterBank<RTC, 16>;

public:
	void init();
	void reset();
};
extern RTCRegisters rtc;

class INTCRegisters : public RegisterBank<INTC, 5>
{
	using super = RegisterBank<INTC, 5>;

public:
	void init();
	void reset();
	void term();
};
extern INTCRegisters intc;

class TMURegisters : public RegisterBank<TMU, 12>
{
	using super = RegisterBank<TMU, 12>;

public:
	void init();
	void reset();
	void term();
	void serialize(Serializer& ser);
	void deserialize(Deserializer& deser);
};
extern TMURegisters tmu;

class SCIRegisters : public RegisterBank<SCI, 8>
{
	using super = RegisterBank<SCI, 8>;

public:
	void init();
	void reset();
};
extern SCIRegisters sci;

class SCIFRegisters : public RegisterBank<SCIF, 10>
{
	using super = RegisterBank<SCIF, 10>;

public:
	void init();
	void reset(bool hard);
	void term();
};
extern SCIFRegisters scif;

class SCIFSerialPort : public SerialPort
{
public:
	void setPipe(Pipe *pipe) override {
		this->pipe = pipe;
	}
	Pipe *getPipe() const {
		return pipe;
	}
	void updateStatus() override {}
	void receiveBreak() override;
	void init();
	void term();
	void reset();
	void serialize(Serializer& ser);
	void deserialize(Deserializer& deser);

	u8 SCFRDR2_read();
	void SCFTDR2_write(u8 data);
	u16 readStatus();
	void writeStatus(u16 data);
	u16 SCFDR2_read();
	static u16 SCFCR2_read(u32 addr);
	void SCFCR2_write(u16 data);
	void SCSPTR2_write(u16 data);
	static void SCBRR2_write(u32 addr, u8 data);
	static void SCSMR2_write(u32 addr, u16 data);
	void SCSCR2_write(u16 data);

	static SCIFSerialPort& Instance();

private:
	enum StatusBit {
		DR = 0x01,
		RDF = 0x02,
		PER = 0x04,
		FER = 0x08,
		BRK = 0x10,
		TDFE = 0x20,
		TEND = 0x40,
		ER = 0x80,
	};

	void setStatusBit(StatusBit bit);
	bool isTDFE() const;
	bool isRDF() const;
	void updateBaudRate();
	void setBreak(bool on);
	void sendBreak();
	bool txDone();
	void rxSched();
	static int schedCallback(int tag, int cycles, int lag, void *arg);

	Pipe *pipe = nullptr;
	int schedId = -1;
	int brkSchedId = -1;
	int frameSize = 10; // default 8 data bits, 1 stop bit, no parity
	int cyclesPerBit = SH4_MAIN_CLOCK / 6103;
	u16 statusLastRead = 0;
	std::deque<u8> txFifo;
	std::deque<u8> rxFifo;
	bool transmitting = false;
};

void setupPtyPipe();
