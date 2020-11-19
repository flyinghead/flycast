#include "ta_ctx.h"
#include "hw/sh4/sh4_sched.h"
#include "oslib/oslib.h"

extern u32 fskip;
extern u32 FrameCount;

TA_context* ta_ctx;
tad_context ta_tad;

TA_context*  vd_ctx;
rend_context vd_rc;

// helper for 32 byte aligned memory allocation
void* OS_aligned_malloc(size_t align, size_t size)
{
#ifdef __MINGW32__
	return __mingw_aligned_malloc(size, align);
#elif defined(_WIN32)
	return _aligned_malloc(size, align);
#else
	void *result;
	if (posix_memalign(&result, align, size))
		return NULL;
	else
		return result;
#endif
}

// helper for 32 byte aligned memory de-allocation
void OS_aligned_free(void *ptr)
{
#ifdef __MINGW32__
	__mingw_aligned_free(ptr);
#elif defined(_WIN32)
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}


void SetCurrentTARC(u32 addr)
{
	if (addr != TACTX_NONE)
	{
		if (ta_ctx)
			SetCurrentTARC(TACTX_NONE);

		verify(ta_ctx == 0);
		//set new context
		ta_ctx = tactx_Find(addr,true);

		//copy cached params
		ta_tad = ta_ctx->tad;
	}
	else
	{
		//Flush cache to context
		verify(ta_ctx != 0);
		ta_ctx->tad=ta_tad;
		
		//clear context
		ta_ctx=0;
		ta_tad.Reset(0);
	}
}

static std::mutex mtx_rqueue;
TA_context* rqueue;
cResetEvent frame_finished;

static double last_frame1, last_frame2;
static u64 last_cycles1, last_cycles2;

bool QueueRender(TA_context* ctx)
{
	verify(ctx != 0);
	
 	//Try to limit speed to a "sane" level
 	//Speed is also limited via audio, but audio
 	//is sometimes not accurate enough (android, vista+)
	u32 cycle_span = (u32)(sh4_sched_now64() - last_cycles2);
	last_cycles2 = last_cycles1;
	last_cycles1 = sh4_sched_now64();
 	double time_span = os_GetSeconds() - last_frame2;
 	last_frame2 = last_frame1;
 	last_frame1 = os_GetSeconds();

 	bool too_fast = (cycle_span / time_span) > SH4_MAIN_CLOCK;

 	// Vulkan: rtt frames seem to be discarded often
	if (rqueue && (too_fast || ctx->rend.isRTT) && settings.pvr.SynchronousRender)
		//wait for a frame if
		//  we have another one queue'd and
		//  sh4 run at > 120% over the last two frames
		//  and SynchronousRender is enabled
		frame_finished.Wait();

	if (rqueue)
	{
		tactx_Recycle(ctx);
		fskip++;
		return false;
	}

	frame_finished.Reset();
	mtx_rqueue.lock();
	TA_context* old = rqueue;
	rqueue=ctx;
	mtx_rqueue.unlock();

	verify(!old);

	return true;
}

TA_context* DequeueRender()
{
	mtx_rqueue.lock();
	TA_context* rv = rqueue;
	mtx_rqueue.unlock();

	if (rv)
		FrameCount++;

	return rv;
}

bool rend_framePending() {
	mtx_rqueue.lock();
	TA_context* rv = rqueue;
	mtx_rqueue.unlock();

	return rv != 0;
}

void FinishRender(TA_context* ctx)
{
	if (ctx != NULL)
	{
		verify(rqueue == ctx);
		mtx_rqueue.lock();
		rqueue = NULL;
		mtx_rqueue.unlock();

		tactx_Recycle(ctx);
	}
	frame_finished.Set();
}

static std::mutex mtx_pool;

static std::vector<TA_context*> ctx_pool;
static std::vector<TA_context*> ctx_list;

TA_context* tactx_Alloc()
{
	TA_context* rv = 0;

	mtx_pool.lock();
	if (ctx_pool.size())
	{
		rv = ctx_pool[ctx_pool.size()-1];
		ctx_pool.pop_back();
	}
	mtx_pool.unlock();
	
	if (!rv)
	{
		rv = new TA_context();
		rv->Alloc();
	}

	return rv;
}

void tactx_Recycle(TA_context* poped_ctx)
{
	mtx_pool.lock();
	{
		if (ctx_pool.size()>2)
		{
			poped_ctx->Free();
			delete poped_ctx;
		}
		else
		{
			poped_ctx->Reset();
			ctx_pool.push_back(poped_ctx);
		}
	}
	mtx_pool.unlock();
}

TA_context* tactx_Find(u32 addr, bool allocnew)
{
	for (size_t i=0; i<ctx_list.size(); i++)
	{
		if (ctx_list[i]->Address==addr)
			return ctx_list[i];
	}

	if (allocnew)
	{
		TA_context* rv = tactx_Alloc();
		rv->Address=addr;
		ctx_list.push_back(rv);

		return rv;
	}
	return 0;
}

TA_context* tactx_Pop(u32 addr)
{
	for (size_t i=0; i<ctx_list.size(); i++)
	{
		if (ctx_list[i]->Address==addr)
		{
			TA_context* rv = ctx_list[i];
			
			if (ta_ctx == rv)
				SetCurrentTARC(TACTX_NONE);

			ctx_list.erase(ctx_list.begin() + i);

			return rv;
		}
	}
	return 0;
}

void tactx_Term()
{
	for (size_t i = 0; i < ctx_list.size(); i++)
	{
		ctx_list[i]->Free();
		delete ctx_list[i];
	}
	ctx_list.clear();
	mtx_pool.lock();
	{
		for (size_t i = 0; i < ctx_pool.size(); i++)
		{
			ctx_pool[i]->Free();
			delete ctx_pool[i];
		}
	}
	ctx_pool.clear();
	mtx_pool.unlock();
}

const u32 NULL_CONTEXT = ~0u;

void SerializeTAContext(void **data, unsigned int *total_size)
{
	if (ta_ctx == nullptr)
	{
		REICAST_S(NULL_CONTEXT);
		return;
	}
	REICAST_S(ta_ctx->Address);
	const u32 taSize = ta_tad.thd_data - ta_tad.thd_root;
	REICAST_S(taSize);
	REICAST_SA(ta_tad.thd_root, taSize);
}

void UnserializeTAContext(void **data, unsigned int *total_size)
{
	u32 address;
	REICAST_US(address);
	if (address == NULL_CONTEXT)
		return;
	SetCurrentTARC(address);
	u32 size;
	REICAST_US(size);
	REICAST_USA(ta_tad.thd_root, size);
	ta_tad.thd_data = ta_tad.thd_root + size;
}
