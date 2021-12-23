#include "ta_ctx.h"
#include "spg.h"
#include "cfg/option.h"
#include "Renderer_if.h"
#include "serialize.h"

extern u32 fskip;
extern u32 FrameCount;
static int RenderCount;

TA_context* ta_ctx;
tad_context ta_tad;

TA_context*  vd_ctx;
rend_context vd_rc;

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

TA_context* rqueue;
cResetEvent frame_finished;

bool QueueRender(TA_context* ctx)
{
	verify(ctx != 0);
	
	bool skipFrame = settings.disableRenderer;
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
		if (!settings.disableRenderer)
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

bool rend_framePending() {
	return rqueue != nullptr;
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

TA_context* tactx_Alloc()
{
	TA_context* rv = 0;

	mtx_pool.lock();
	if (!ctx_pool.empty())
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
	if (ta_ctx != nullptr)
		SetCurrentTARC(TACTX_NONE);

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

static void serializeContext(Serializer& ser, const TA_context *ctx)
{
	if (ser.dryrun())
	{
		// Maximum size: address, size, data, render pass count, render passes
		ser.skip(4 + 4 + TA_DATA_SIZE + 4 + ARRAY_SIZE(tad_context::render_passes) * 4);
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
	ser << tad.render_pass_count;
	for (u32 i = 0; i < tad.render_pass_count; i++)
	{
		u32 offset = (u32)(tad.render_passes[i] - tad.thd_root);
		ser << offset;
	}
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
	if (deser.version() >= Deserializer::V12 || (deser.version() >= Deserializer::V12_LIBRETRO && deser.version() < Deserializer::V5))
	{
		deser >> tad.render_pass_count;
		for (u32 i = 0; i < tad.render_pass_count; i++)
		{
			u32 offset;
			deser >> offset;
			tad.render_passes[i] = tad.thd_root + offset;
		}
	}
	else
	{
		tad.render_pass_count = 0;
	}
}

void SerializeTAContext(Serializer& ser)
{
	serializeContext(ser, ta_ctx);
	if (TA_CURRENT_CTX != CORE_CURRENT_CTX)
		serializeContext(ser, tactx_Find(TA_CURRENT_CTX, false));
	else
		serializeContext(ser, nullptr);

}
void DeserializeTAContext(Deserializer& deser)
{
	if (::ta_ctx != nullptr)
		SetCurrentTARC(TACTX_NONE);
	TA_context *ta_cur_ctx;
	deserializeContext(deser, &ta_cur_ctx);
	if (ta_cur_ctx != nullptr)
		SetCurrentTARC(ta_cur_ctx->Address);
	if (deser.version() >= Deserializer::V20)
		deserializeContext(deser, &ta_cur_ctx);
}
