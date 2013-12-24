#pragma once
#include "ta.h"
#include "pvr_regs.h"

//Vertex storage types
struct Vertex
{
	float x,y,z;

	u8 col[4];
	u8 spc[4];

	float u,v;
};

struct PolyParam
{
	u32 first;		//entry index , holds vertex/pos data
	u32 count;

	//lets see what more :)

	TSP tsp;
	TCW tcw;
	PCW pcw;
	ISP_TSP isp;
	float zvZ;
	u32 tileclip;
	//float zMin,zMax;
};

struct ModParam
{
	u32 first;		//entry index , holds vertex/pos data
	u32 count;
};

struct ModTriangle
{
	f32 x0,y0,z0,x1,y1,z1,x2,y2,z2;
};

void decode_pvr_vertex(u32 base,u32 ptr,Vertex* cv);

struct  tad_context
{
	u8* thd_data;
	u8* thd_root;

	void Clear()
	{
		thd_data=thd_root;
	}
};

struct rend_context
{
	u8* proc_start;
	u8* proc_end;

	f32 fZ_min;
	f32 fZ_max;

	bool Overrun;
	bool isRTT;
	bool isAutoSort;

	double early;

	FB_X_CLIP_type    fb_X_CLIP;
	FB_Y_CLIP_type    fb_Y_CLIP;

	List<Vertex>      verts;
	List<u16>         idx;
	List<ModTriangle> modtrig;
	List<ISP_Modvol>  global_param_mvo;

	List<PolyParam>   global_param_op;
	List<PolyParam>   global_param_pt;
	List<PolyParam>   global_param_tr;

	void Clear()
	{
		verts.Clear();
		idx.Clear();
		global_param_op.Clear();
		global_param_pt.Clear();
		global_param_tr.Clear();
		modtrig.Clear();
		global_param_mvo.Clear();

		Overrun=false;
		fZ_min= 1000000.0f;
		fZ_max= 1.0f;
	}
};

//vertex lists
struct TA_context
{
	u32 Address;
	u32 LastUsed;

	cMutex thd_inuse;
	cMutex rend_inuse;

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

	void MarkRend()
	{
		rend.proc_start = rend.proc_end;
		rend.proc_end = tad.thd_data;
	}
	void Alloc()
	{
		tad.thd_root=tad.thd_data=(u8*)malloc(2*1024*1024);

		rend.verts.InitBytes(1024*1024,&rend.Overrun); //up to 1 mb of vtx data/frame = ~ 38k vtx/frame
		rend.idx.Init(60*1024,&rend.Overrun);			//up to 60K indexes ( idx have stripification overhead )
		rend.global_param_op.Init(4096,&rend.Overrun);
		rend.global_param_pt.Init(4096,&rend.Overrun);
		rend.global_param_mvo.Init(4096,&rend.Overrun);
		rend.global_param_tr.Init(4096,&rend.Overrun);

		rend.modtrig.Init(4096,&rend.Overrun);
		
		Reset();
	}

	void Reset()
	{
		tad.Clear();
		rend_inuse.Lock();
		rend.Clear();
		rend.proc_end = rend.proc_start = tad.thd_data;
		rend_inuse.Unlock();
	}

	void Free()
	{
		free(tad.thd_root);
		rend.verts.Free();
		rend.idx.Free();
		rend.global_param_op.Free();
		rend.global_param_pt.Free();
		rend.global_param_tr.Free();
		rend.modtrig.Free();
		rend.global_param_mvo.Free();
	}
};


extern TA_context* ta_ctx;
extern tad_context ta_tad;

extern TA_context*  vd_ctx;
extern rend_context vd_rc;

TA_context* tactx_Find(u32 addr, bool allocnew=false);
TA_context* tactx_Pop(u32 addr);

void tactx_Recycle(TA_context* poped_ctx);

/*
	Ta Context

	Rend Context
*/

#define TACTX_NONE (0xFFFFFFFF)

void SetCurrentTARC(u32 addr);
void QueueRender(TA_context* ctx);
TA_context* DequeueRender();
bool TryDecodeTARC();
void VDecEnd();

//must be moved to proper header
void FillBGP(TA_context* ctx);
bool UsingAutoSort();