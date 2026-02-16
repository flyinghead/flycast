/*
	Lovely timers, its amazing how many times this module was bugged
*/

#include "types.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_mmr.h"
#include "serialize.h"

#define tmu_underflow 0x0100
#define tmu_UNIE      0x0020

TMURegisters tmu;
static u32 tmu_shift[3];
static u64 tmu_mask[3];
static u32 old_mode[3] = { 0xFFFF, 0xFFFF, 0xFFFF};

static constexpr InterruptID tmu_intID[3] = { sh4_TMU0_TUNI0, sh4_TMU1_TUNI1, sh4_TMU2_TUNI2 };
int tmu_sched[3];
static s64 tmu_ch_base[3];

inline static u64 now() {
	return sh4_sched_now64() + SH4_TIMESLICE - p_sh4rcb->cntx.cycle_counter;
}

static s64 read_TMU_TCNTch(u32 ch) {
	return tmu_ch_base[ch] - ((now() >> tmu_shift[ch]) & tmu_mask[ch]);
}

static void sched_chan_tick(int ch)
{
	//schedule next interrupt
	if (tmu_mask[ch])
	{
		const s64 cnt = read_TMU_TCNTch(ch);
		const u32 cycles = std::clamp<s64>(cnt << tmu_shift[ch], 0, SH4_MAIN_CLOCK);
		sh4_sched_request(tmu_sched[ch], cycles);
	}
	else {
		sh4_sched_request(tmu_sched[ch], -1);
	}
}

static void write_TMU_TCNTch(u32 ch, s64 data) {
	tmu_ch_base[ch] = data + ((now() >> tmu_shift[ch]) & tmu_mask[ch]);
	sched_chan_tick(ch);
}

static u32 reloadCounter(int ch, s64 curValue)
{
	// raise interrupt, timer counted down
	TMU_TCR(ch) |= tmu_underflow;
	InterruptPend(tmu_intID[ch], true);

	// schedule next trigger by writing the TCNT register
	u32 tcor = TMU_TCOR(ch);
	// Don't miss an underflow if tcor is less than -curValue
	curValue = std::max<s64>((s64)tcor + curValue, 0);
	write_TMU_TCNTch(ch, curValue);
	return curValue;
}

template<u32 ch>
static u32 read_TMU_TCNT(u32 addr)
{
	s64 v = read_TMU_TCNTch(ch);
	if (v < 0)
		return reloadCounter(ch, v);
	else
		return (u32)v;
}

template<u32 ch>
static void write_TMU_TCNT(u32 addr, u32 data) {
	write_TMU_TCNTch(ch, data);
}

static void turn_on_off_ch(u32 ch, bool on)
{
	s64 TCNT = read_TMU_TCNTch(ch);
	tmu_mask[ch] = on ? 0xFFFFFFFFFFFFFFFFllu : 0;
	write_TMU_TCNTch(ch, TCNT);
}

//Update internal counter registers
static void UpdateTMUCounts(u32 reg)
{
	InterruptPend(tmu_intID[reg], TMU_TCR(reg) & tmu_underflow);
	InterruptMask(tmu_intID[reg], TMU_TCR(reg) & tmu_UNIE);

	if (old_mode[reg] == (TMU_TCR(reg) & 7))
		return;
	old_mode[reg] = TMU_TCR(reg) & 7;

	s64 TCNT = read_TMU_TCNTch(reg);
	switch (TMU_TCR(reg) & 7)
	{
		case 0: //4
			tmu_shift[reg] = 2;
			break;

		case 1: //16
			tmu_shift[reg] = 4;
			break;

		case 2: //64
			tmu_shift[reg] = 6;
			break;

		case 3: //256
			tmu_shift[reg] = 8;
			break;

		case 4: //1024
			tmu_shift[reg] = 10;
			break;

		case 5: //reserved
			INFO_LOG(SH4, "TMU ch%d - TCR%d mode is reserved (5)", reg, reg);
			break;

		case 6: //RTC
			INFO_LOG(SH4, "TMU ch%d - TCR%d mode is RTC (6), can't be used on Dreamcast", reg, reg);
			break;

		case 7: //external
			INFO_LOG(SH4, "TMU ch%d - TCR%d mode is External (7), can't be used on Dreamcast", reg, reg);
			break;
	}
	tmu_shift[reg] += 2;
	write_TMU_TCNTch(reg, TCNT);
}

//Write to status registers
template<int ch>
static void TMU_TCR_write(u32 addr, u16 data)
{
	if constexpr (ch == 2)
		TMU_TCR(ch) = data & 0x03ff;
	else
		TMU_TCR(ch) = data & 0x013f;
	UpdateTMUCounts(ch);
}

//Chan 2 not used functions
static u32 TMU_TCPR2_read(u32 addr) {
	INFO_LOG(SH4, "Read from TMU_TCPR2 - this register should be not used on Dreamcast according to docs");
	return 0;
}

static void TMU_TCPR2_write(u32 addr, u32 data) {
	INFO_LOG(SH4, "Write to TMU_TCPR2 - this register should be not used on Dreamcast according to docs, data=%d", data);
}

static void write_TMU_TSTR(u32 addr, u8 data)
{
	TMU_TSTR = data & 7;

	for (int i = 0; i < 3; i++)
		turn_on_off_ch(i, data & (1 << i));
}

static int sched_tmu_cb(int ch, int sch_cycl, int jitter, void *arg)
{
	if (tmu_mask[ch])
	{
		s64 tcnt = (s64)read_TMU_TCNTch(ch);

		// 64 bit maths to differentiate big values from overflows
		if (tcnt < 0)
			reloadCounter(ch, tcnt);
		else
			// schedule next trigger by writing the TCNT register
			write_TMU_TCNTch(ch, tcnt);
	}
	return 0;	// already scheduled if needed
}

//Init/Res/Term
void TMURegisters::init()
{
	super::init();

	//TMU TOCR 0xFFD80000 0x1FD80000 8 0x00 0x00 Held Held Pclk
	setRW<TMU_TOCR_addr, u8, 1>();

	//TMU TSTR 0xFFD80004 0x1FD80004 8 0x00 0x00 Held 0x00 Pclk
	setWriteHandler<TMU_TSTR_addr>(write_TMU_TSTR);

	//TMU TCOR0 0xFFD80008 0x1FD80008 32 0xFFFFFFFF 0xFFFFFFFF Held Held Pclk
	setRW<TMU_TCOR0_addr>();

	//TMU TCNT0 0xFFD8000C 0x1FD8000C 32 0xFFFFFFFF 0xFFFFFFFF Held Held Pclk
	setHandlers<TMU_TCNT0_addr>(read_TMU_TCNT<0>, write_TMU_TCNT<0>);

	//TMU TCR0 0xFFD80010 0x1FD80010 16 0x0000 0x0000 Held Held Pclk
	setWriteHandler<TMU_TCR0_addr>(TMU_TCR_write<0>);

	//TMU TCOR1 0xFFD80014 0x1FD80014 32 0xFFFFFFFF 0xFFFFFFFF Held Held Pclk
	setRW<TMU_TCOR1_addr>();

	//TMU TCNT1 0xFFD80018 0x1FD80018 32 0xFFFFFFFF 0xFFFFFFFF Held Held Pclk
	setHandlers<TMU_TCNT1_addr>(read_TMU_TCNT<1>, write_TMU_TCNT<1>);

	//TMU TCR1 0xFFD8001C 0x1FD8001C 16 0x0000 0x0000 Held Held Pclk
	setWriteHandler<TMU_TCR1_addr>(TMU_TCR_write<1>);

	//TMU TCOR2 0xFFD80020 0x1FD80020 32 0xFFFFFFFF 0xFFFFFFFF Held Held Pclk
	setRW<TMU_TCOR2_addr>();

	//TMU TCNT2 0xFFD80024 0x1FD80024 32 0xFFFFFFFF 0xFFFFFFFF Held Held Pclk
	setHandlers<TMU_TCNT2_addr>(read_TMU_TCNT<2>, write_TMU_TCNT<2>);
	
	//TMU TCR2 0xFFD80028 0x1FD80028 16 0x0000 0x0000 Held Held Pclk
	setWriteHandler<TMU_TCR2_addr>(TMU_TCR_write<2>);

	//TMU TCPR2 0xFFD8002C 0x1FD8002C 32 Held Held Held Held Pclk
	setHandlers<TMU_TCPR2_addr>(TMU_TCPR2_read, TMU_TCPR2_write);

	for (std::size_t i = 0; i < std::size(tmu_sched); i++)
		tmu_sched[i] = sh4_sched_register(i, &sched_tmu_cb);

	reset();
}


void TMURegisters::reset()
{
	super::reset();

	memset(tmu_shift, 0, sizeof(tmu_shift));
	memset(tmu_mask, 0, sizeof(tmu_mask));
	memset(old_mode, 0xFF, sizeof(old_mode));
	memset(tmu_ch_base, 0, sizeof(tmu_ch_base));

	TMU_TCOR(0) = TMU_TCOR(1) = TMU_TCOR(2) = 0xffffffff;

	UpdateTMUCounts(0);
	UpdateTMUCounts(1);
	UpdateTMUCounts(2);

	write_TMU_TSTR(0, 0);

	for (int i = 0; i < 3; i++)
		write_TMU_TCNTch(i, 0xffffffff);
}

void TMURegisters::term()
{
	super::term();
	for (int& sched_id : tmu_sched)
	{
		sh4_sched_unregister(sched_id);
		sched_id = -1;
	}
}

void TMURegisters::serialize(Serializer& ser)
{
	ser << tmu_shift;
	ser << tmu_mask;
	ser << old_mode;
	ser << tmu_ch_base;
	for (int schedId : tmu_sched)
		sh4_sched_serialize(ser, schedId);
}

void TMURegisters::deserialize(Deserializer& deser)
{
	deser >> tmu_shift;
	deser.skip(sizeof(u32) * 3, Deserializer::V58); // u32 tmu_mask[3]
	deser >> tmu_mask;
	deser >> old_mode;
	deser.skip(sizeof(u32) * 3, Deserializer::V58); // u32 tmu_ch_base[3]
	deser >> tmu_ch_base;
	if (deser.version() >= Deserializer::V58) {
		for (int schedId : tmu_sched)
			sh4_sched_deserialize(deser, schedId);
	}
}
