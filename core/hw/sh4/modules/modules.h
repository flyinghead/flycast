#pragma once
#include "hw/hwreg.h"

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

struct SerialPipe
{
	// Serial TX
	virtual void write(u8 data) { }
	// RX buffer Size
	virtual int available() { return 0; }
	// Serial RX
	virtual u8 read() { return 0; }

	virtual ~SerialPipe() = default;
};
void serial_setPipe(SerialPipe *pipe);
void serial_updateStatusRegister();
