#include "ta_ctx.h"
#include "spg.h"
#include "cfg/option.h"
#include "Renderer_if.h"
#include "serialize.h"
#include "stdclass.h"

#include <mutex>
#include <vector>

extern u32 fskip;
static int RenderCount;

TA_context* ta_ctx;
tad_context ta_tad;

static void tactx_Recycle(TA_context* ctx);
static TA_context *tactx_Find(u32 addr, bool allocnew = false);

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

static TA_context* rqueue;
static cResetEvent frame_finished;

bool QueueRender(TA_context* ctx)
{
	verify(ctx != 0);
	
	bool skipFrame = !rend_is_enabled();
	if (!skipFrame)
	{
		RenderCount++;
		if (RenderCount % (config::SkipFrame + 1) != 0)
			skipFrame = true;
		else if (config::ThreadedRendering && rqueue != nullptr
				&& (config::AutoSkipFrame == 0 || (config::AutoSkipFrame == 1 && SH4FastEnough)))
			// The previous render hasn't completed yet so we wait.
			// If autoskipframe is enabled (normal level), we only do so if the CPU is running
			// fast enough over the last frames
			frame_finished.Wait();
	}

	if (skipFrame || rqueue)
	{
		tactx_Recycle(ctx);
		if (rend_is_enabled())
			fskip++;
		return false;
	}
	// disable net rollbacks until the render thread has processed the frame
	rend_disable_rollback();
	frame_finished.Reset();
	verify(rqueue == nullptr);
	rqueue = ctx;


	return true;
}

TA_context* DequeueRender()
{
	if (rqueue != nullptr)
		FrameCount++;

	return rqueue;
}

void FinishRender(TA_context* ctx)
{
	if (ctx != nullptr)
	{
		verify(rqueue == ctx);
		rqueue = nullptr;
		tactx_Recycle(ctx);
	}
	frame_finished.Set();
}

static std::mutex mtx_pool;

static std::vector<TA_context*> ctx_pool;
static std::vector<TA_context*> ctx_list;

TA_context *tactx_Alloc()
{
	TA_context *ctx = nullptr;

	mtx_pool.lock();
	if (!ctx_pool.empty())
	{
		ctx = ctx_pool.back();
		ctx_pool.pop_back();
	}
	mtx_pool.unlock();

	if (ctx == nullptr)
	{
		ctx = new TA_context();
		ctx->Alloc();
	}
	return ctx;
}

static void tactx_Recycle(TA_context* ctx)
{
	if (ctx->nextContext != nullptr)
		tactx_Recycle(ctx->nextContext);
	mtx_pool.lock();
	if (ctx_pool.size() > 3)
	{
		delete ctx;
	}
	else
	{
		ctx->Reset();
		ctx_pool.push_back(ctx);
	}
	mtx_pool.unlock();
}

static TA_context *tactx_Find(u32 addr, bool allocnew)
{
	TA_context *oldCtx = nullptr;
	for (TA_context *ctx : ctx_list)
	{
		if (ctx->Address == addr)
		{
			ctx->lastFrameUsed = FrameCount;
			return ctx;
		}
		if (FrameCount - ctx->lastFrameUsed > 60)
			oldCtx = ctx;
	}

	if (allocnew)
	{
		TA_context *ctx;
		if (oldCtx != nullptr)
		{
			ctx = oldCtx;
			ctx->Reset();
		}
		else
		{
			ctx = tactx_Alloc();
			ctx_list.push_back(ctx);
		}
		ctx->Address = addr;
		ctx->lastFrameUsed = FrameCount;

		return ctx;
	}
	return nullptr;
}

TA_context *tactx_Pop(u32 addr)
{
	for (size_t i = 0; i < ctx_list.size(); i++)
	{
		if (ctx_list[i]->Address == addr)
		{
			TA_context *ctx = ctx_list[i];
			
			if (::ta_ctx == ctx)
				SetCurrentTARC(TACTX_NONE);

			ctx_list.erase(ctx_list.begin() + i);

			return ctx;
		}
	}
	return nullptr;
}

void tactx_Term()
{
	if (ta_ctx != nullptr)
		SetCurrentTARC(TACTX_NONE);

	for (TA_context *ctx : ctx_list)
		delete ctx;
	ctx_list.clear();

	mtx_pool.lock();
	for (TA_context *ctx : ctx_pool)
		delete ctx;
	ctx_pool.clear();
	mtx_pool.unlock();
}

const u32 NULL_CONTEXT = ~0u;

static void serializeContext(Serializer& ser, const TA_context *ctx)
{
	if (ser.dryrun())
	{
		// Maximum size: address, size, data
		ser.skip(4 + 4 + TA_DATA_SIZE);
		return;
	}
	if (ctx == nullptr)
	{
		ser << NULL_CONTEXT;
		return;
	}
	ser << ctx->Address;
	const tad_context& tad = ctx == ::ta_ctx ? ta_tad : ctx->tad;
	const u32 taSize = tad.thd_data - tad.thd_root;
	ser << taSize;
	ser.serialize(tad.thd_root, taSize);
}

static void deserializeContext(Deserializer& deser, TA_context **pctx)
{
	u32 address;
	deser >> address;
	if (address == NULL_CONTEXT)
	{
		*pctx = nullptr;
		return;
	}
	*pctx = tactx_Find(address, true);
	u32 size;
	deser >> size;
	tad_context& tad = (*pctx)->tad;
	deser.deserialize(tad.thd_root, size);
	tad.thd_data = tad.thd_root + size;
	if ((deser.version() >= Deserializer::V12 && deser.version() < Deserializer::V26)
			|| (deser.version() >= Deserializer::V12_LIBRETRO && deser.version() <= Deserializer::VLAST_LIBRETRO))
	{
		u32 render_pass_count;
		deser >> render_pass_count;
		deser.skip(sizeof(u32) * render_pass_count);
	}
}

void SerializeTAContext(Serializer& ser)
{
	ser << (u32)ctx_list.size();
	int curCtx = -1;
	for (const auto& ctx : ctx_list)
	{
		if (ctx == ::ta_ctx)
			curCtx = (int)(&ctx - &ctx_list[0]);
		serializeContext(ser, ctx);
	}
	ser << curCtx;
}

void DeserializeTAContext(Deserializer& deser)
{
	if (::ta_ctx != nullptr)
		SetCurrentTARC(TACTX_NONE);
	if (deser.version() >= Deserializer::V25)
	{
		u32 listSize;
		deser >> listSize;
		for (const auto& ctx : ctx_list)
			tactx_Recycle(ctx);
		ctx_list.clear();
		for (u32 i = 0; i < listSize; i++)
		{
			TA_context *ctx;
			deserializeContext(deser, &ctx);
		}
		int curCtx;
		deser >> curCtx;
		if (curCtx >= 0 && curCtx < (int)ctx_list.size())
			SetCurrentTARC(ctx_list[curCtx]->Address);
	}
	else
	{
		TA_context *ta_cur_ctx;
		deserializeContext(deser, &ta_cur_ctx);
		if (ta_cur_ctx != nullptr)
			SetCurrentTARC(ta_cur_ctx->Address);
		if (deser.version() >= Deserializer::V20)
			deserializeContext(deser, &ta_cur_ctx);
	}
}
