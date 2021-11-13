#pragma once
#include "types.h"
#include "ta_structs.h"
#include "pvr_regs.h"
#include "helper_classes.h"
#include "stdclass.h"
#include "oslib/oslib.h"

#include <mutex>

class BaseTextureCacheData;

//Vertex storage types
struct Vertex
{
	float x,y,z;

	u8 col[4];
	u8 spc[4];

	float u,v;

	// Two volumes format
	u8 col1[4];
	u8 spc1[4];

	float u1,v1;
};

struct PolyParam
{
	u32 first;		//entry index , holds vertex/pos data
	u32 count;

	BaseTextureCacheData *texture;
#if !defined(HOST_64BIT_CPU)
	u32 _pad0;
#endif

	TSP tsp;
	TCW tcw;
	PCW pcw;
	ISP_TSP isp;
	float zvZ;
	u32 tileclip;
	//float zMin,zMax;
	TSP tsp1;
	TCW tcw1;
	BaseTextureCacheData *texture1;
#if !defined(HOST_64BIT_CPU)
	u32 _pad1;
#endif
};

struct ModifierVolumeParam
{
	u32 first;
	u32 count;
	ISP_Modvol isp;
};

struct ModTriangle
{
	f32 x0,y0,z0,x1,y1,z1,x2,y2,z2;
};

struct  tad_context
{
	u8* thd_data;
	u8* thd_root;
	u8* thd_old_data;
	u8 *render_passes[10];
	u32 render_pass_count;

	void Clear()
	{
		thd_old_data = thd_data = thd_root;
		render_pass_count = 0;
	}

	void ClearPartial()
	{
		thd_old_data = thd_data;
		thd_data = thd_root;
	}

	void Continue()
	{
		render_passes[render_pass_count] = End();
		if (render_pass_count < sizeof(render_passes) / sizeof(u8*) - 1)
			render_pass_count++;
	}
	
	u8* End()
	{
		return thd_data == thd_root ? thd_old_data : thd_data;
	}

	void Reset(u8* ptr)
	{
		thd_data = thd_root = thd_old_data = ptr;
		render_pass_count = 0;
	}

};

struct RenderPass {
	bool autosort;
	bool z_clear;
	u32 op_count;
	u32 mvo_count;
	u32 pt_count;
	u32 tr_count;
	u32 mvo_tr_count;
};

struct rend_context
{
	u8* proc_start;
	u8* proc_end;

	f32 fZ_min;
	f32 fZ_max;

	bool Overrun;
	bool isRTT;
	bool isRenderFramebuffer;
	
	FB_X_CLIP_type    fb_X_CLIP;
	FB_Y_CLIP_type    fb_Y_CLIP;
	
	u32 fog_clamp_min;
	u32 fog_clamp_max;

	List<Vertex>      verts;
	List<u32>         idx;
	List<ModTriangle> modtrig;
	List<ModifierVolumeParam>  global_param_mvo;
	List<ModifierVolumeParam>  global_param_mvo_tr;

	List<PolyParam>   global_param_op;
	List<PolyParam>   global_param_pt;
	List<PolyParam>   global_param_tr;
	List<RenderPass>  render_passes;

	void Clear()
	{
		verts.Clear();
		idx.Clear();
		global_param_op.Clear();
		global_param_pt.Clear();
		global_param_tr.Clear();
		modtrig.Clear();
		global_param_mvo.Clear();
		global_param_mvo_tr.Clear();
		render_passes.Clear();

		Overrun=false;
		fZ_min= 1000000.0f;
		fZ_max= 1.0f;
		isRenderFramebuffer = false;
	}
};

#define TA_DATA_SIZE (8 * 1024 * 1024)

//vertex lists
struct TA_context
{
	u32 Address;
	u32 LastUsed;

	std::mutex thd_inuse;
	std::mutex rend_inuse;

	tad_context tad;
	rend_context rend;

	
	/*
		Dreamcast games use up to 20k vtx, 30k idx, 1k (in total) parameters.
		at 30 fps, thats 600kvtx (900 stripped)
		at 20 fps thats 1.2M vtx (~ 1.8M stripped)

		allocations allow much more than that !

		some stats:
			recv:   idx: 33528, vtx: 23451, op: 128, pt: 4, tr: 133, mvo: 14, modt: 342
			sc:     idx: 26150, vtx: 17417, op: 162, pt: 12, tr: 244, mvo: 6, modt: 2044
			doa2le: idx: 47178, vtx: 34046, op: 868, pt: 0, tr: 354, mvo: 92, modt: 976 (overruns)
			ika:    idx: 46748, vtx: 33818, op: 984, pt: 9, tr: 234, mvo: 10, modt: 16, ov: 0  
			ct:     idx: 30920, vtx: 21468, op: 752, pt: 0, tr: 360, mvo: 101, modt: 732, ov: 0
			sa2:    idx: 36094, vtx: 24520, op: 1330, pt: 10, tr: 177, mvo: 39, modt: 360, ov: 0
	*/

	void MarkRend(u32 render_pass)
	{
		verify(render_pass <= tad.render_pass_count);

		rend.proc_start = render_pass == 0 ? tad.thd_root : tad.render_passes[render_pass - 1];
		rend.proc_end = render_pass == tad.render_pass_count ? tad.End() : tad.render_passes[render_pass];
	}

	void Alloc()
	{
		tad.Reset((u8*)allocAligned(32, TA_DATA_SIZE));

		rend.verts.InitBytes(4 * 1024 * 1024, &rend.Overrun, "verts");	//up to 4 mb of vtx data/frame = ~ 96k vtx/frame
		rend.idx.Init(120 * 1024, &rend.Overrun, "idx");				//up to 120K indexes ( idx have stripification overhead )
		rend.global_param_op.Init(16384, &rend.Overrun, "global_param_op");
		rend.global_param_pt.Init(5120, &rend.Overrun, "global_param_pt");
		rend.global_param_mvo.Init(4096, &rend.Overrun, "global_param_mvo");
		rend.global_param_tr.Init(10240, &rend.Overrun, "global_param_tr");
		rend.global_param_mvo_tr.Init(4096, &rend.Overrun, "global_param_mvo_tr");

		rend.modtrig.Init(16384, &rend.Overrun, "modtrig");
		
		rend.render_passes.Init(sizeof(RenderPass) * 10, &rend.Overrun, "render_passes");	// 10 render passes

		Reset();
	}

	void Reset()
	{
		verify(tad.End() - tad.thd_root <= TA_DATA_SIZE);
		tad.Clear();
		rend_inuse.lock();
		rend.Clear();
		rend.proc_end = rend.proc_start = tad.thd_root;
		rend_inuse.unlock();
	}

	void Free()
	{
		verify(tad.End() - tad.thd_root <= TA_DATA_SIZE);
		freeAligned(tad.thd_root);
		rend.verts.Free();
		rend.idx.Free();
		rend.global_param_op.Free();
		rend.global_param_pt.Free();
		rend.global_param_tr.Free();
		rend.modtrig.Free();
		rend.global_param_mvo.Free();
		rend.global_param_mvo_tr.Free();
		rend.render_passes.Free();
	}
};


extern TA_context* ta_ctx;
extern tad_context ta_tad;

extern TA_context*  vd_ctx;
extern rend_context vd_rc;

TA_context* tactx_Find(u32 addr, bool allocnew=false);
TA_context* tactx_Pop(u32 addr);

TA_context* tactx_Alloc();
void tactx_Recycle(TA_context* poped_ctx);
void tactx_Term();
/*
	Ta Context

	Rend Context
*/

#define TACTX_NONE (0xFFFFFFFF)

void SetCurrentTARC(u32 addr);
bool QueueRender(TA_context* ctx);
TA_context* DequeueRender();
void FinishRender(TA_context* ctx);

//must be moved to proper header
void FillBGP(TA_context* ctx);
bool UsingAutoSort(int pass_number);
bool rend_framePending();
void SerializeTAContext(Serializer& ser);
void DeserializeTAContext(Deserializer& deser);
