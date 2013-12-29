
#include "types.h"
#include "sh4_interrupts.h"
#include "sh4_core.h"
#include "sh4_sched.h"
#include "oslib/oslib.h"


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
u32 sh4_sched_intr;

struct sched_list
{
	sh4_sched_callback* cb;
	int tag;
	int start;
	int end;
};

vector<sched_list> list;

int sh4_sched_next_id=-1;

u32 sh4_sched_remaining(int id)
{
	if (list[id].end!=-1)
	{
		return list[id].end-sh4_sched_now();
	}
	else
	{
		return -1;
	}
}

void sh4_sched_ffts()
{
	u32 diff=-1;
	int slot=-1;

	for (size_t i=0;i<list.size();i++)
	{
		if (sh4_sched_remaining(i)<diff)
		{
			slot=i;
			diff=sh4_sched_remaining(i);
		}
	}

	sh4_sched_ffb-=Sh4cntx.sh4_sched_next;

	sh4_sched_next_id=slot;
	if (slot!=-1)
	{
		Sh4cntx.sh4_sched_next=diff;
	}
	else
	{
		Sh4cntx.sh4_sched_next=SH4_MAIN_CLOCK;
	}

	sh4_sched_ffb+=Sh4cntx.sh4_sched_next;
}

int sh4_sched_register(int tag, sh4_sched_callback* ssc)
{
	sched_list t={ssc,tag,-1,-1};

	list.push_back(t);

	return list.size()-1;
}

u32 sh4_sched_now()
{
	return sh4_sched_ffb-Sh4cntx.sh4_sched_next;
}

u64 sh4_sched_now64()
{
	return sh4_sched_ffb-Sh4cntx.sh4_sched_next;
}
void sh4_sched_request(int id, int cycles)
{
	list[id].start=sh4_sched_now();
	list[id].end=list[id].start+cycles;

	sh4_sched_ffts();
}

int sh4_sched_elapsed(int id)
{
	if (list[id].end!=-1)
	{
		int rv=sh4_sched_now()-list[id].start;
		list[id].start=sh4_sched_now();
		return rv;
	}
	else
		return -1;
}

void handle_cb(int id)
{
	int remain=list[id].end-list[id].start;
	int elapsd=sh4_sched_elapsed(id);
	int jitter=elapsd-remain;

	list[id].end=-1;
	int re_sch=list[id].cb(list[id].tag,remain,jitter);

	if (re_sch>0)	sh4_sched_request(id,re_sch-jitter);
}

void sh4_sched_tick(int cycles)
{
	/*
	Sh4cntx.sh4_sched_time+=cycles;
	Sh4cntx.sh4_sched_next-=cycles;
	*/

	if (Sh4cntx.sh4_sched_next<0)
	{
		u32 fztime=sh4_sched_now()-cycles;
		sh4_sched_intr++;
		if (sh4_sched_next_id!=-1)
		{
			for (int i=0;i<list.size();i++)
			{
				if ((list[i].end-fztime)<=(u32)cycles)
				{
					handle_cb(i);
				}
			}
		}
		sh4_sched_ffts();
	}
}