/*
	Dreamcast serial port.
*/
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#else
#include <windows.h>
#include <io.h>
#endif
#include "types.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_interrupts.h"
#include "cfg/option.h"
#include "modules.h"
#include "hw/sh4/sh4_sched.h"
#include "serialize.h"

//#define DEBUG_SCIF

#ifdef DEBUG_SCIF
#define SCIF_LOG(...) INFO_LOG(SH4, __VA_ARGS__)
#else
#define SCIF_LOG(...)
#endif

SCIRegisters sci;
SCIFRegisters scif;

static void updateInterrupts()
{
    InterruptPend(sh4_SCIF_TXI, SCIF_SCFSR2.TDFE);
    InterruptMask(sh4_SCIF_TXI, SCIF_SCSCR2.TIE);

    InterruptPend(sh4_SCIF_RXI, SCIF_SCFSR2.RDF || SCIF_SCFSR2.DR);
    InterruptMask(sh4_SCIF_RXI, SCIF_SCSCR2.RIE);

    InterruptPend(sh4_SCIF_BRI, SCIF_SCFSR2.BRK);
    InterruptMask(sh4_SCIF_BRI, SCIF_SCSCR2.RIE || SCIF_SCSCR2.REIE);

    InterruptPend(sh4_SCIF_ERI, SCIF_SCFSR2.ER || SCIF_SCFSR2.FER || SCIF_SCFSR2.PER);
    InterruptMask(sh4_SCIF_ERI, SCIF_SCSCR2.RIE || SCIF_SCSCR2.REIE);
}

int SCIFSerialPort::schedCallback(int tag, int cycles, int lag, void *arg)
{
	SCIFSerialPort& scif = *(SCIFSerialPort *)arg;
	if (tag == 0)
	{
		bool reschedule = scif.txDone();
		scif.rxSched();
		if (reschedule || scif.pipe != nullptr)
			return scif.frameSize * scif.cyclesPerBit;
		else
			return 0;
	}
	else
	{
		scif.sendBreak();
		return 0;
	}
}

bool SCIFSerialPort::isTDFE() const {
	return (int)txFifo.size() <= 1 << (3 - SCIF_SCFCR2.TTRG);
}

bool SCIFSerialPort::isRDF() const {
	constexpr u32 trigLevels[] { 1, 4, 8, 14 };
	return rxFifo.size() >= trigLevels[SCIF_SCFCR2.RTRG];
}

bool SCIFSerialPort::txDone()
{
	if (!transmitting || SCIF_SCFCR2.TFRST == 1)
		return false;
	if (txFifo.empty())
	{
		setStatusBit(TEND);
		transmitting = false;
		return false; // don't reschedule
	}
	u8 v = txFifo.front();
	txFifo.pop_front();
	if (pipe != nullptr)
		pipe->write(v);
	if (isTDFE()) {
		setStatusBit(TDFE);
		updateInterrupts();
	}
	return true;
}

void SCIFSerialPort::rxSched()
{
	if (pipe == nullptr)
		return;

	if (pipe->available() > 0)
	{
		u8 v = pipe->read();
		if (SCIF_SCSCR2.RE == 0 || SCIF_SCFCR2.RFRST == 1)
			return;
		if (rxFifo.size() == 16)
		{
			// rx overrun
			SCIF_SCLSR2.ORER = 1;
			updateInterrupts();
			INFO_LOG(SH4, "scif: Receive overrun");
		}
		else
		{
			rxFifo.push_back(v);
			if (isRDF()) {
				setStatusBit(RDF);
				updateInterrupts();
			}
		}
	}
	// TODO fifo might have been emptied since last rx
	else if (!rxFifo.empty())
	{
		setStatusBit(DR);
		updateInterrupts();
	}
}

void SCIFSerialPort::updateBaudRate()
{
	// 1 start bit, 7 or 8 data bits, optional parity bit, 1 or 2 stop bits
	frameSize = 1 + 8 - SCIF_SCSMR2.CHR + SCIF_SCSMR2.PE + 1 + SCIF_SCSMR2.STOP;
	int bauds = SH4_MAIN_CLOCK / 4 / (SCIF_SCBRR2 + 1) / 32 / (1 << (SCIF_SCSMR2.CKS * 2));
	cyclesPerBit = SH4_MAIN_CLOCK / bauds;
	INFO_LOG(SH4, "SCIF: Frame size %d cycles/bit %d (%d bauds) pipe %p", frameSize, cyclesPerBit, bauds, pipe);
	sh4_sched_request(schedId, frameSize * cyclesPerBit);
}

// SCIF SCFTDR2 - Transmit FIFO Data Register
void SCIFSerialPort::SCFTDR2_write(u8 data)
{
	SCIF_LOG("serial out %02x %c fifo_sz %d", data, data == '\0' ? ' ' : data, (int)txFifo.size());
	if (SCIF_SCFCR2.TFRST == 1)
		return;
	if (SCIF_SCSMR2.CHR == 1)
		data &= 0x7f;
	if (txFifo.empty() && !transmitting && SCIF_SCSCR2.TE == 1)
	{
		if (pipe != nullptr)
			pipe->write(data);
		transmitting = true;
		// Need to reschedule so it doesn't happen too early (f355)
		sh4_sched_request(schedId, frameSize * cyclesPerBit);
		setStatusBit(TDFE); // immediately transfer SCFTDR2 into the shift register
		updateInterrupts();
	}
	else if (txFifo.size() < 16) {
		txFifo.push_back(data);
	}
}

// SCIF_SCFSR2 read - Serial Status Register
u16 SCIFSerialPort::readStatus()
{
//	SCIF_LOG("SCIF_SCFSR2.read %s%s%s%s%s%s%s%s",
//			SCIF_SCFSR2.ER ? "ER " : "",
//			SCIF_SCFSR2.TEND ? "TEND " : "",
//			SCIF_SCFSR2.TDFE ? "TDFE " : "",
//			SCIF_SCFSR2.BRK ? "BRK " : "",
//			SCIF_SCFSR2.FER ? "FER " : "",
//			SCIF_SCFSR2.PER ? "PER " : "",
//			SCIF_SCFSR2.RDF ? "RDF " : "",
//			SCIF_SCFSR2.DR ? "DR" : "");
	statusLastRead = SCIF_SCFSR2.full;
	return SCIF_SCFSR2.full;
}

void SCIFSerialPort::setStatusBit(StatusBit bit)
{
	statusLastRead &= ~bit;
	SCIF_SCFSR2.full |= bit;
}

// SCIF_SCFSR2 write - Serial Status Register
void SCIFSerialPort::writeStatus(u16 data)
{
	data = data | ~0x00f3 | ~statusLastRead;
	// RDF and TDFE cannot be reset until the trigger level is reached
	if (isRDF())
		data |= RDF;
	if (isTDFE())
		data |= TDFE;
	SCIF_LOG("SCIF_SCFSR2.reset %s%s%s%s%s%s%s%s",
			(data & ER)   ? "" : "ER ",
			(data & TEND) ? "" : "TEND ",
			(data & TDFE) ? "" : "TDFE ",
			(data & BRK)  ? "" : "BRK ",
			(data & FER)  ? "" : "FER ",
			(data & PER)  ? "" : "PER ",
			(data & RDF)  ? "" : "RDF ",
			(data & DR)   ? "" : "DR");

	SCIF_SCFSR2.full &= data;
	statusLastRead &= data;

	updateInterrupts();
}

//SCIF_SCFDR2 - FIFO Data Count Register
u16 SCIFSerialPort::SCFDR2_read()
{
	u16 rv = rxFifo.size() | (txFifo.size() << 8);
	SCIF_LOG("SCIF: fifo count rx %d tx %d", rv & 0xff, rv >> 8);

	return rv;
}

//SCIF_SCFRDR2 - Receive FIFO Data Register
u8 SCIFSerialPort::SCFRDR2_read()
{
	if (rxFifo.empty()) {
		INFO_LOG(SH4, "Empty rx fifo read");
		return 0;
	}
	u8 data = rxFifo.front();
	rxFifo.pop_front();
	SCIF_LOG("serial in %02x %c", data, data);
	return data;
}

SCIFSerialPort& SCIFSerialPort::Instance()
{
	static SCIFSerialPort instance;

	return instance;
}

//SCSCR2 - Serial Control Register
static u16 SCSCR2_read(u32 addr)
{
	return SCIF_SCSCR2.full;
}

void SCIFSerialPort::SCSCR2_write(u16 data)
{
	SCIF_SCSCR2.full = data & 0x00fa;
	if (SCIF_SCSCR2.TE == 0)
	{
		setStatusBit(TEND);
		// TE must be cleared to send a break
		setBreak(SCIF_SCSPTR2.SPB2IO == 1 && SCIF_SCSPTR2.SPB2DT == 0);
	}
	else {
		setBreak(false);
	}
	updateInterrupts();
	SCIF_LOG("SCIF_SCSCR2= %s%s%s%s%s",
			SCIF_SCSCR2.TIE ? "TIE " : "",
			SCIF_SCSCR2.RIE ? "RIE " : "",
			SCIF_SCSCR2.TE ? "TE " : "",
			SCIF_SCSCR2.RE ? "RE " : "",
			SCIF_SCSCR2.REIE ? "REIE" : "");
}

// SCSPTR2 - Serial Port Register
static u16 SCSPTR2_read(u32 addr)
{
	SCIF_LOG("SCIF_SCSPTR2.read %x", SCIF_SCSPTR2.full);
	return SCIF_SCSPTR2.full & ~0x10; // CTS active/low
}

void SCIFSerialPort::setBreak(bool on)
{
	if (on) {
		// tetris needs to send/receive breaks
		if (!sh4_sched_is_scheduled(brkSchedId))
			sh4_sched_request(brkSchedId, cyclesPerBit * frameSize);
	}
	else {
		if (sh4_sched_is_scheduled(brkSchedId))
			sh4_sched_request(brkSchedId, -1);
	}
}

void SCIFSerialPort::SCSPTR2_write(u16 data)
{
	SCIF_SCSPTR2.full = data & 0x00f3;
	if (SCIF_SCSPTR2.SPB2IO == 1)
		setBreak(SCIF_SCSPTR2.SPB2DT == 0 && SCIF_SCSCR2.TE == 0);
	else
		setBreak(false);

	SCIF_LOG("SCIF_SCSPTR2= %s%s%s%s%s%s",
			SCIF_SCSPTR2.RTSIO ? "RTSIO " : "",
			SCIF_SCSPTR2.RTSDT ? "RTSDT " : "",
			SCIF_SCSPTR2.CTSIO ? "CTSIO " : "",
			SCIF_SCSPTR2.CTSDT ? "CTSDT " : "",
			SCIF_SCSPTR2.SPB2IO ? "SPB2IO " : "",
			SCIF_SCSPTR2.SPB2DT ? "SPB2DT" : "");
}

// SCFCR2 - FIFO Control Register
u16 SCIFSerialPort::SCFCR2_read(u32 addr)
{
//	SCIF_LOG("SCIF_SCFCR2.read %x", SCIF_SCFCR2.full);
	return SCIF_SCFCR2.full;
}

void SCIFSerialPort::SCFCR2_write(u16 data)
{
	if (SCIF_SCFCR2.TFRST == 1 && !(data & 4))
	{
		// when TFRST 1 -> 0
		// seems to help tetris send data during sync
		setStatusBit(TEND);
		setStatusBit(TDFE);
		updateInterrupts();
	}
	SCIF_SCFCR2.full = data & 0x00ff;
	if (SCIF_SCFCR2.TFRST == 1)
	{
		txFifo.clear();
		if (pipe == nullptr)
			sh4_sched_request(schedId, -1);
		transmitting = false;
	}
	if (SCIF_SCFCR2.RFRST == 1)
		rxFifo.clear();
	SCIF_LOG("SCIF_SCFCR2= %s%s%sTTRG %d RTRG %d",
			SCIF_SCFCR2.RFRST ? "RFRST " : "",
			SCIF_SCFCR2.TFRST ? "TFRST " : "",
			SCIF_SCFCR2.MCE ? "MCE " : "",
			SCIF_SCFCR2.TTRG,
			SCIF_SCFCR2.RTRG);
}

// SCBRR2 - Bit Rate Register
void SCIFSerialPort::SCBRR2_write(u32 addr, u8 data)
{
	SCIF_SCBRR2 = data;
	Instance().updateBaudRate();
}

// SCSMR2 - Serial Mode Register
void SCIFSerialPort::SCSMR2_write(u32 addr, u16 data)
{
	SCIF_SCSMR2.full = data & 0x007b;
	Instance().updateBaudRate();
}

void SCIFSerialPort::receiveBreak()
{
	SCIF_LOG("Break received");
	setStatusBit(BRK);
	updateInterrupts();
}

void SCIFSerialPort::sendBreak()
{
	if (pipe != nullptr)
		pipe->sendBreak();
}


void SCIFSerialPort::init()
{
	if (schedId == -1)
		schedId = sh4_sched_register(0, schedCallback, this);
	if (brkSchedId == -1)
		brkSchedId = sh4_sched_register(1, schedCallback, this);
}

void SCIFSerialPort::term()
{
	if (schedId != -1) {
		sh4_sched_unregister(schedId);
		schedId = -1;
	}
	if (brkSchedId != -1) {
		sh4_sched_unregister(brkSchedId);
		brkSchedId = -1;
	}
}

void SCIFSerialPort::reset()
{
	sh4_sched_request(brkSchedId, -1);
	transmitting = false;
	statusLastRead = 0;
	txFifo.clear();
	rxFifo.clear();
	updateBaudRate();
}

void SCIFSerialPort::serialize(Serializer& ser)
{
	sh4_sched_serialize(ser, schedId);
	sh4_sched_serialize(ser, brkSchedId);
	ser << statusLastRead;
	ser << (int)txFifo.size();
	for (u8 b : txFifo)
		ser << b;
	ser << (int)rxFifo.size();
	for (u8 b : rxFifo)
		ser << b;
	ser << transmitting;
}

void SCIFSerialPort::deserialize(Deserializer& deser)
{
	txFifo.clear();
	rxFifo.clear();
	if (deser.version() >= Deserializer::V43)
	{
		sh4_sched_deserialize(deser, schedId);
		sh4_sched_deserialize(deser, brkSchedId);
		deser >> statusLastRead;
		int size;
		deser >> size;
		for (int i = 0; i < size; i++)
		{
			u8 b;
			deser >> b;
			txFifo.push_back(b);
		}
		deser >> size;
		for (int i = 0; i < size; i++)
		{
			u8 b;
			deser >> b;
			rxFifo.push_back(b);
		}
		deser >> transmitting;
	}
	else
	{
		statusLastRead = 0;
		transmitting = false;
	}
	updateBaudRate();
}

struct PTYPipe : public SerialPort::Pipe
{
	void write(u8 data) override
	{
		if (config::SerialConsole) {
			int rc = ::write(tty, &data, 1);
			(void)rc;
		}
	}

	int available() override {
		int count = 0;
#if defined(__unix__) || defined(__APPLE__)
		if (config::SerialConsole && tty != 1)
			ioctl(tty, FIONREAD, &count);
#endif
		return count;
	}

	u8 read() override
	{
		u8 data = 0;
		if (tty != 1) {
			int rc = ::read(tty, &data, 1);
			(void)rc;
		}
		return data;
	}

	void init()
	{
		if (config::SerialConsole && config::SerialPTY && tty == 1)
		{
#if defined(__unix__) || defined(__APPLE__)
			tty = open("/dev/ptmx", O_RDWR | O_NDELAY | O_NOCTTY | O_NONBLOCK);
			if (tty < 0)
			{
				ERROR_LOG(BOOT, "Cannot open /dev/ptmx: errno %d", errno);
				tty = 1;
			}
			else
			{
				grantpt(tty);
				unlockpt(tty);
				NOTICE_LOG(BOOT, "Pseudoterminal is at %s", ptsname(tty));
			}
#elif defined(_WIN32)
			if (AllocConsole())
			{
				SetConsoleTitle(TEXT("Flycast Serial Output"));

				// Pipe stdout
				HANDLE hStd = GetStdHandle(STD_OUTPUT_HANDLE);
				tty = _open_osfhandle((intptr_t)hStd, _O_TEXT);
				_dup2(tty, fileno(stdout));
				SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)_get_osfhandle(fileno(stdout)));
			}
			else
			{
				ERROR_LOG(BOOT, "Cannot AllocConsole(): errno %d", GetLastError());
			}
#endif
		}
		SCIFSerialPort::Instance().setPipe(this);
	}

	void term()
	{
		if (tty != 1)
		{
			::close(tty);
			tty = 1;
		}
		SCIFSerialPort::Instance().setPipe(nullptr);
	}

private:
	int tty = 1;
};

void setupPtyPipe()
{
	static PTYPipe ptyPipe;

	if (config::SerialConsole || config::SerialPTY)
	{
		if (SCIFSerialPort::Instance().getPipe() == nullptr)
			ptyPipe.init();
	}
	else
	{
		if (SCIFSerialPort::Instance().getPipe() == &ptyPipe)
			ptyPipe.term();
	}
}

template <typename T>
class SingletonForward {

};

template<typename Ret, typename Class, typename... Args>
struct SingletonForward<Ret(Class::*)(Args...)>
{
	static Ret(*forward(Ret(*function)(u32 addr, Args...)))(u32 addr, Args...) {
		return function;
	}
};

#define SINGLETON_FORWARD(accessor, function) \
	SingletonForward<decltype(&std::remove_reference<decltype(accessor)>::type::function)>::forward([](u32 addr, auto... args) { \
		return accessor.function(args...); \
	})

//Init term res
void SCIFRegisters::init()
{
	super::init();

	// Serial Communication Interface with FIFO

	//SCIF SCSMR2 0xFFE80000 0x1FE80000 16 0x0000 0x0000 Held Held Pclk
	setWriteHandler<SCIF_SCSMR2_addr, u16>(SCIFSerialPort::SCSMR2_write);

	//SCIF SCBRR2 0xFFE80004 0x1FE80004 8 0xFF 0xFF Held Held Pclk
	setWriteHandler<SCIF_SCBRR2_addr, u8>(SCIFSerialPort::SCBRR2_write);

	//SCIF SCSCR2 0xFFE80008 0x1FE80008 16 0x0000 0x0000 Held Held Pclk
	setHandlers<SCIF_SCSCR2_addr>(SCSCR2_read, SINGLETON_FORWARD(SCIFSerialPort::Instance(), SCSCR2_write));

	//SCIF SCFTDR2 0xFFE8000C 0x1FE8000C 8 Undefined Undefined Held Held Pclk
	setWriteOnly<SCIF_SCFTDR2_addr>(SINGLETON_FORWARD(SCIFSerialPort::Instance(), SCFTDR2_write));

	//SCIF SCFSR2 0xFFE80010 0x1FE80010 16 0x0060 0x0060 Held Held Pclk
	setHandlers<SCIF_SCFSR2_addr>(SINGLETON_FORWARD(SCIFSerialPort::Instance(), readStatus),
			SINGLETON_FORWARD(SCIFSerialPort::Instance(), writeStatus));

	//READ only
	//SCIF SCFRDR2 0xFFE80014 0x1FE80014 8 Undefined Undefined Held Held Pclk
	setReadOnly<SCIF_SCFRDR2_addr>(SINGLETON_FORWARD(SCIFSerialPort::Instance(), SCFRDR2_read));

	//SCIF SCFCR2 0xFFE80018 0x1FE80018 16 0x0000 0x0000 Held Held Pclk
	setHandlers<SCIF_SCFCR2_addr>(SCIFSerialPort::SCFCR2_read, SINGLETON_FORWARD(SCIFSerialPort::Instance(), SCFCR2_write));

	//Read only
	//SCIF SCFDR2 0xFFE8001C 0x1FE8001C 16 0x0000 0x0000 Held Held Pclk
	setReadOnly<SCIF_SCFDR2_addr>(SINGLETON_FORWARD(SCIFSerialPort::Instance(), SCFDR2_read));

	//SCIF SCSPTR2 0xFFE80020 0x1FE80020 16 0x0000 0x0000 Held Held Pclk
	setHandlers<SCIF_SCSPTR2_addr>(SCSPTR2_read, SINGLETON_FORWARD(SCIFSerialPort::Instance(), SCSPTR2_write));

	//SCIF SCLSR2 0xFFE80024 0x1FE80024 16 0x0000 0x0000 Held Held Pclk
	setRW<SCIF_SCLSR2_addr, u16, 1>();

	SCIFSerialPort::Instance().init();

	reset(true);
}

void SCIFRegisters::reset(bool hard)
{
	super::reset();

	/*
	SCIF SCSMR2 H'FFE8 0000 H'1FE8 0000 16 H'0000 H'0000 Held Held Pclk
	SCIF SCBRR2 H'FFE8 0004 H'1FE8 0004 8 H'FF H'FF Held Held Pclk
	SCIF SCSCR2 H'FFE8 0008 H'1FE8 0008 16 H'0000 H'0000 Held Held Pclk
	SCIF SCFTDR2 H'FFE8 000C H'1FE8 000C 8 Undefined Undefined Held Held Pclk
	SCIF SCFSR2 H'FFE8 0010 H'1FE8 0010 16 H'0060 H'0060 Held Held Pclk
	SCIF SCFRDR2 H'FFE8 0014 H'1FE8 0014 8 Undefined Undefined Held Held Pclk
	SCIF SCFCR2 H'FFE8 0018 H'1FE8 0018 16 H'0000 H'0000 Held Held Pclk
	SCIF SCFDR2 H'FFE8 001C H'1FE8 001C 16 H'0000 H'0000 Held Held Pclk
	SCIF SCSPTR2 H'FFE8 0020 H'1FE8 0020 16 H'0000*2 H'0000*2 Held Held Pclk
	SCIF SCLSR2 H'FFE8 0024 H'1FE8 0024 16 H'0000 H'0000 Held Held Pclk
	*/
	SCIF_SCBRR2 = 0xFF;
	SCIF_SCFSR2.full = 0x060;

	if (hard)
		SCIFSerialPort::Instance().setPipe(nullptr);
	SCIFSerialPort::Instance().reset();
}

void SCIFRegisters::term()
{
	SCIFSerialPort::Instance().term();

	super::term();
}

void SCIRegisters::init()
{
	super::init();

	// Serial Communication Interface
	setRW<SCI_SCSMR1_addr, u8>();
	setRW<SCI_SCBRR1_addr, u8>();
	setRW<SCI_SCSCR1_addr, u8>();
	setRW<SCI_SCTDR1_addr, u8>();
	setRW<SCI_SCSSR1_addr, u8, 0xf9>();
	setReadOnly<SCI_SCRDR1_addr, u8>();
	setRW<SCI_SCSPTR1_addr, u8, 0x8f>();

	reset();
}

void SCIRegisters::reset()
{
	super::reset();

	SCI_SCBRR1 = 0xff;
	SCI_SCTDR1 = 0xff;
	SCI_SCSSR1 = 0x84;
}
