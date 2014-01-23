#include "Renderer_if.h"
#include "ta.h"
#include "hw/pvr/pvr_mem.h"


u32 VertexCount=0;
u32 FrameCount=0;

Renderer* rend;
cResetEvent rs(false,true);

int max_idx,max_mvo,max_op,max_pt,max_tr,max_vtx,max_modt, ovrn;

TA_context* _pvrrc;
void SetREP(TA_context* cntx);



bool rend_single_frame()
{
	//wait render start only if no frame pending
	_pvrrc = DequeueRender();

	while (!_pvrrc)
	{
		rs.Wait();
		_pvrrc = DequeueRender();
	}

	bool do_swp=false;
	
	do_swp=rend->Render();
	

	if (do_swp)
	{
		//OSD_DRAW();
	}

	//clear up & free data ..
	tactx_Recycle(_pvrrc);
	_pvrrc=0;

	return do_swp;
}

void* rend_thread(void* p)
{
	#if SET_AFNT
	cpu_set_t mask;

	/* CPU_ZERO initializes all the bits in the mask to zero. */

	CPU_ZERO( &mask );



	/* CPU_SET sets only the bit corresponding to cpu. */

	CPU_SET( 1, &mask );



	/* sched_setaffinity returns 0 in success */

	if( sched_setaffinity( 0, sizeof(mask), &mask ) == -1 )

	{

		printf("WARNING: Could not set CPU Affinity, continuing...\n");

	}
	#endif



	if (!rend->Init())
		die("rend->init() failed\n");

	rend->Resize(640,480);

	for(;;)
	{
		if (rend_single_frame())
			rend->Present();	
	}
}

cThread rthd(rend_thread,0);



void rend_start_render()
{
	bool is_rtt=(FB_W_SOF1& 0x1000000)!=0;
	TA_context* ctx = tactx_Pop(CORE_CURRENT_CTX);

	SetREP(ctx);

	if (ctx)
	{
		if (!ctx->rend.Overrun)
		{
			//printf("REP: %.2f ms\n",render_end_pending_cycles/200000.0);
			FillBGP(ctx);
			
			ctx->rend.isRTT=is_rtt;
			ctx->rend.isAutoSort = UsingAutoSort();

			ctx->rend.fb_X_CLIP=FB_X_CLIP;
			ctx->rend.fb_Y_CLIP=FB_Y_CLIP;
			
			max_idx=max(max_idx,ctx->rend.idx.used());
			max_vtx=max(max_vtx,ctx->rend.verts.used());
			max_op=max(max_op,ctx->rend.global_param_op.used());
			max_pt=max(max_pt,ctx->rend.global_param_pt.used());
			max_tr=max(max_tr,ctx->rend.global_param_tr.used());
			
			max_mvo=max(max_mvo,ctx->rend.global_param_mvo.used());
			max_modt=max(max_modt,ctx->rend.modtrig.used());

#if HOST_OS==OS_WINDOWS && 0
			printf("max: idx: %d, vtx: %d, op: %d, pt: %d, tr: %d, mvo: %d, modt: %d, ov: %d\n", max_idx, max_vtx, max_op, max_pt, max_tr, max_mvo, max_modt, ovrn);
#endif
			QueueRender(ctx);
			rs.Set();
		}
		else
		{
			ovrn++;
			printf("WARNING: Rendering context is overrun (%d), aborting frame\n",ovrn);
			tactx_Recycle(ctx);
		}
	}
}


void rend_end_render()
{
#if 0 //also disabled the printf, it takes quite some time ...
	#if HOST_OS!=OS_WINDOWS && !defined(_ANDROID)
		if (!re.state) printf("Render > Extended time slice ...\n");
	#endif
	rend_end_wait();
#endif
}

/*
void rend_end_wait()
{
	#if HOST_OS!=OS_WINDOWS && !defined(_ANDROID)
	//	if (!re.state) printf("Render End: Waiting ...\n");
	#endif
	re.Wait();
	pvrrc.InUse=false;
}
*/

bool rend_init()
{

#if NO_REND
	rend = rend_norend();
#else

#if HOST_OS == OS_WINDOWS
	rend = settings.pvr.rend == 0 ? rend_GLES2() : rend_D3D11() ;
#else
	rend = rend_GLES2();
#endif

#endif

#if !defined(_ANDROID)
	rthd.Start();
#endif

#if SET_AFNT
	cpu_set_t mask;



	/* CPU_ZERO initializes all the bits in the mask to zero. */

	CPU_ZERO( &mask );



	/* CPU_SET sets only the bit corresponding to cpu. */

	CPU_SET( 0, &mask );



	/* sched_setaffinity returns 0 in success */

	if( sched_setaffinity( 0, sizeof(mask), &mask ) == -1 )

	{

		printf("WARNING: Could not set CPU Affinity, continuing...\n");

	}
#endif

	return true;
}

void rend_term()
{
}

void rend_vblank()
{
	os_DoEvents();
}
