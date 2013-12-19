#include "types.h"

typedef int sh4_sched_callback(int tag, int sch_cycl, int jitter);

int sh4_sched_register(int tag, sh4_sched_callback* ssc);
u32 sh4_sched_now();
u64 sh4_sched_now64();

void sh4_sched_request(int id, int cycles);
int sh4_sched_elapsed(int id);

void sh4_sched_tick(int cycles);

extern u32 sh4_sched_intr;