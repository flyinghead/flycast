#include "types.h"
#include "sh4_interrupts.h"
#include "sh4_core.h"
#include "sh4_sched.h"

//sh4 scheduler

/*

	register handler
	request callback at time

	single fire events only

	sh4_sched_register(id)
	sh4_sched_request(id, in_cycles)

	sh4_sched_now()

*/

u64 sh4_sched_ffb;
std::vector<sched_list> sch_list;
int sh4_sched_next_id = -1;

static u32 sh4_sched_now();

static u32 sh4_sched_remaining(const sched_list& sched, u32 reference)
{
	if (sched.end != -1)
		return sched.end - reference;
	else
		return -1;
}

void sh4_sched_ffts()
{
	u32 diff = -1;
	int slot = -1;

	u32 now = sh4_sched_now();
	for (const sched_list& sched : sch_list)
	{
		u32 remaining = sh4_sched_remaining(sched, now);
		if (remaining < diff)
		{
			slot = &sched - &sch_list[0];
			diff = remaining;
		}
	}

	sh4_sched_ffb -= Sh4cntx.sh4_sched_next;

	sh4_sched_next_id = slot;
	if (slot != -1)
		Sh4cntx.sh4_sched_next = diff;
	else
		Sh4cntx.sh4_sched_next = SH4_MAIN_CLOCK;

	sh4_sched_ffb += Sh4cntx.sh4_sched_next;
}

int sh4_sched_register(int tag, sh4_sched_callback* ssc)
{
	sched_list t{ ssc, tag, -1, -1};
	for (sched_list& sched : sch_list)
		if (sched.cb == nullptr)
		{
			sched = t;
			return &sched - &sch_list[0];
		}

	sch_list.push_back(t);

	return sch_list.size() - 1;
}

void sh4_sched_unregister(int id)
{
	if (id == -1)
		return;
	verify(id < (int)sch_list.size());
	if (id == (int)sch_list.size() - 1)
		sch_list.resize(sch_list.size() - 1);
	else
	{
		sch_list[id].cb = nullptr;
		sch_list[id].end = -1;
	}
	sh4_sched_ffts();
}

/*
	Return current cycle count, in 32 bits (wraps after 21 dreamcast seconds)
*/
static u32 sh4_sched_now()
{
	return sh4_sched_ffb - Sh4cntx.sh4_sched_next;
}

/*
	Return current cycle count, in 64 bits (effectively never wraps)
*/
u64 sh4_sched_now64()
{
	return sh4_sched_ffb - Sh4cntx.sh4_sched_next;
}

void sh4_sched_request(int id, int cycles)
{
	verify(cycles == -1 || (cycles >= 0 && cycles <= SH4_MAIN_CLOCK));

	sched_list& sched = sch_list[id];
	sched.start = sh4_sched_now();

	if (cycles == -1)
	{
		sched.end = -1;
	}
	else
	{
		sched.end = sched.start + cycles;
		if (sched.end == -1)
			sched.end++;
	}

	sh4_sched_ffts();
}

/* Returns how much time has passed for this callback */
static int sh4_sched_elapsed(sched_list& sched)
{
	if (sched.end != -1)
	{
		int rv = sh4_sched_now() - sched.start;
		sched.start = sh4_sched_now();
		return rv;
	}
	else
		return -1;
}

static void handle_cb(sched_list& sched)
{
	int remain = sched.end - sched.start;
	int elapsd = sh4_sched_elapsed(sched);
	int jitter = elapsd - remain;

	sched.end = -1;
	int re_sch = sched.cb(sched.tag, remain, jitter);

	if (re_sch > 0)
		sh4_sched_request(&sched - &sch_list[0], std::max(0, re_sch - jitter));
}

void sh4_sched_tick(int cycles)
{
	if (Sh4cntx.sh4_sched_next >= 0)
		return;

	u32 fztime = sh4_sched_now() - cycles;
	if (sh4_sched_next_id != -1)
	{
		for (sched_list& sched : sch_list)
		{
			int remaining = sh4_sched_remaining(sched, fztime);
			verify(remaining >= 0 || remaining == -1);
			if (remaining >= 0 && remaining <= (int)cycles)
				handle_cb(sched);
		}
	}
	sh4_sched_ffts();
}

void sh4_sched_reset(bool hard)
{
	if (hard)
	{
		sh4_sched_ffb = 0;
		sh4_sched_next_id = -1;
		for (sched_list& sched : sch_list)
			sched.start = sched.end = -1;
		Sh4cntx.sh4_sched_next = 0;
	}
}
