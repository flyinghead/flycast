#ifndef SH4_SCHED_H
#define SH4_SCHED_H

#include "types.h"

extern u64 sh4_sched_ffb;

/*
	tag, as passed on sh4_sched_register
	sch_cycles, the cycle duration that the callback requested (sh4_sched_request)
	jitter, the number of cycles that the callback was delayed, [0... 448]
*/
typedef int sh4_sched_callback(int tag, int sch_cycl, int jitter);

/*
	Registed a callback to the scheduler. The returned id 
	is used for sh4_sched_request and sh4_sched_elapsed calls
*/
int sh4_sched_register(int tag, sh4_sched_callback* ssc);

/*
	Return current cycle count, in 32 bits (wraps after 21 dreamcast seconds)
*/
static inline u32 sh4_sched_now()
{
	return sh4_sched_ffb - Sh4cntx.sh4_sched_next;
}

/*
	Return current cycle count, in 64 bits (effectively never wraps)
*/
static inline u64 sh4_sched_now64()
{
	return sh4_sched_ffb - Sh4cntx.sh4_sched_next;
}

/*
	Schedule a callback to be called sh4 *cycles* after the
	invocation of this function. *Cycles* range is (0, 200M].
	
	Passing a value of 0 disables the callback.
	If called multiple times, only the last call is in effect
*/
void sh4_sched_request(int id, int cycles);

/*
	Tick for *cycles*
*/
void sh4_sched_tick(int cycles);

void sh4_sched_ffts();

struct sched_list
{
	sh4_sched_callback* cb;
	int tag;
	int start;
	int end;
};

#endif //SH4_SCHED_H
