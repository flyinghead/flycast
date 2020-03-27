/*
	 This file is part of reicast.

	 reicast is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 2 of the License, or
	 (at your option) any later version.

	 reicast is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

	 You should have received a copy of the GNU General Public License
	 along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include "sorter.h"

struct IndexTrig
{
	u32 id[3];
	u16 pid;
	f32 z;
};

#if 0
static float min3(float v0, float v1, float v2)
{
	return min(min(v0,v1),v2);
}

static float max3(float v0, float v1, float v2)
{
	return max(max(v0,v1),v2);
}
#endif

static float minZ(const Vertex *v, const u32 *mod)
{
	return min(min(v[mod[0]].z,v[mod[1]].z),v[mod[2]].z);
}

static bool operator<(const IndexTrig& left, const IndexTrig& right)
{
	return left.z<right.z;
}

static bool operator<(const PolyParam& left, const PolyParam& right)
{
/* put any condition you want to sort on here */
	return left.zvZ<right.zvZ;
	//return left.zMin<right.zMax;
}

void SortPParams(int first, int count)
{
	if (pvrrc.verts.used() == 0 || count <= 1)
		return;

	Vertex* vtx_base=pvrrc.verts.head();
	u32* idx_base = pvrrc.idx.head();

	PolyParam* pp = &pvrrc.global_param_tr.head()[first];
	PolyParam* pp_end = pp + count;

	while(pp!=pp_end)
	{
		if (pp->count<2)
		{
			pp->zvZ=0;
		}
		else
		{
			u32* idx = idx_base + pp->first;

			Vertex* vtx=vtx_base+idx[0];
			Vertex* vtx_end=vtx_base + idx[pp->count-1]+1;

			u32 zv=0xFFFFFFFF;
			while(vtx!=vtx_end)
			{
				zv=min(zv,(u32&)vtx->z);
				vtx++;
			}

			pp->zvZ=(f32&)zv;
		}
		pp++;
	}

	std::stable_sort(pvrrc.global_param_tr.head() + first, pvrrc.global_param_tr.head() + first + count);
}

const static Vertex *vtx_sort_base;

#if 0
/*

	Per triangle sorting experiments

*/

//approximate the triangle area
float area_x2(Vertex* v)
{
	return 2/3*fabs( (v[0].x-v[2].x)*(v[1].y-v[0].y) - (v[0].x-v[1].x)*(v[2].y-v[0].y)) ;
}

//approximate the distance ^2
float distance_apprx(Vertex* a, Vertex* b)
{
	float xd=a->x-b->x;
	float yd=a->y-b->y;

	return xd*xd+yd*yd;
}

//was good idea, but not really working ..
bool Intersect(Vertex* a, Vertex* b)
{
	float a1=area_x2(a);
	float a2=area_x2(b);

	float d = distance_apprx(a,b);

	return (a1+a1)>d;
}

//root for quick-union
u16 rid(vector<u16>& v, u16 id)
{
	while(id!=v[id]) id=v[id];
	return id;
}

struct TrigBounds
{
	float xs,xe;
	float ys,ye;
	float zs,ze;
};

//find 3d bounding box for triangle
TrigBounds bound(Vertex* v)
{
	TrigBounds rv = {	min(min(v[0].x,v[1].x),v[2].x), max(max(v[0].x,v[1].x),v[2].x),
						min(min(v[0].y,v[1].y),v[2].y), max(max(v[0].y,v[1].y),v[2].y),
						min(min(v[0].z,v[1].z),v[2].z), max(max(v[0].z,v[1].z),v[2].z),
					};

	return rv;
}

//bounding box 2d intersection
bool Intersect(TrigBounds& a, TrigBounds& b)
{
	return  ( !(a.xe<b.xs || a.xs>b.xe) && !(a.ye<b.ys || a.ys>b.ye) /*&& !(a.ze<b.zs || a.zs>b.ze)*/ );
}


bool operator<(const IndexTrig &left, const IndexTrig &right)
{
	/*
	TrigBounds l=bound(vtx_sort_base+left.id);
	TrigBounds r=bound(vtx_sort_base+right.id);

	if (!Intersect(l,r))
	{
		return true;
	}
	else
	{
		return (l.zs + l.ze) < (r.zs + r.ze);
	}*/

	return minZ(&vtx_sort_base[left.id])<minZ(&vtx_sort_base[right.id]);
}

//Not really working cuz of broken intersect
bool Intersect(const IndexTrig &left, const IndexTrig &right)
{
	TrigBounds l=bound(vtx_sort_base+left.id);
	TrigBounds r=bound(vtx_sort_base+right.id);

	return Intersect(l,r);
}

#endif

//are two poly params the same?
static bool PP_EQ(const PolyParam *pp0, const PolyParam *pp1)
{
	return (pp0->pcw.full & PCW_DRAW_MASK) == (pp1->pcw.full & PCW_DRAW_MASK) && pp0->isp.full == pp1->isp.full
			&& pp0->tcw.full == pp1->tcw.full && pp0->tsp.full == pp1->tsp.full && pp0->tileclip == pp1->tileclip;
}

static void fill_id(u32 *d, const Vertex *v0, const Vertex *v1, const Vertex *v2,  const Vertex *vb)
{	
	d[0] = (u32)(v0 - vb);
	d[1] = (u32)(v1 - vb);
	d[2] = (u32)(v2 - vb);
}

void GenSorted(int first, int count, vector<SortTrigDrawParam>& pidx_sort, vector<u32>& vidx_sort)
{
	u32 tess_gen=0;

	pidx_sort.clear();

	if (pvrrc.verts.used() == 0 || count <= 1)
		return;

	const Vertex *vtx_base = pvrrc.verts.head();
	const u32 *idx_base = pvrrc.idx.head();

	const PolyParam *pp_base = &pvrrc.global_param_tr.head()[first];
	const PolyParam *pp = pp_base;
	const PolyParam *pp_end = pp + count;

	vtx_sort_base=vtx_base;

	static u32 vtx_cnt;

	int vtx_count = pvrrc.verts.used() - idx_base[pp->first];
	if (vtx_count>vtx_cnt)
		vtx_cnt=vtx_count;

#if PRINT_SORT_STATS
	printf("TVTX: %d || %d\n",vtx_cnt,vtx_count);
#endif

	if (vtx_count<=0)
		return;

	//make lists of all triangles, with their pid and vid
	static vector<IndexTrig> lst;

	lst.resize(vtx_count*4);


	int pfsti=0;

	while(pp!=pp_end)
	{
		u32 ppid = (u32)(pp - pp_base);

		if (pp->count>2)
		{
			const u32 *idx = idx_base + pp->first;
			u32 flip = 0;

			for (int i = 0; i < pp->count - 2; i++)
			{
				const Vertex *v0, *v1;
				if (flip)
				{
					v0 = vtx_base + idx[i + 1];
					v1 = vtx_base + idx[i];
				}
				else
				{
					v0 = vtx_base + idx[i];
					v1 = vtx_base + idx[i + 1];
				}
				const Vertex *v2 = vtx_base + idx[i + 2];
#if 0
				const Vertex *v3, *v4, *v5;
				if (settings.pvr.subdivide_transp)
				{
					u32 tess_x=(max3(v0->x,v1->x,v2->x)-min3(v0->x,v1->x,v2->x))/32;
					u32 tess_y=(max3(v0->y,v1->y,v2->y)-min3(v0->y,v1->y,v2->y))/32;

					if (tess_x==1) tess_x=0;
					if (tess_y==1) tess_y=0;

					//bool tess=(maxZ(v0,v1,v2)/minZ(v0,v1,v2))>=1.2;

					if (tess_x + tess_y)
					{
						v3=pvrrc.verts.Append(3);
						v4=v3+1;
						v5=v4+1;

						//xyz
						for (int i=0;i<3;i++)
						{
							((float*)&v3->x)[i]=((float*)&v0->x)[i]*0.5f+((float*)&v2->x)[i]*0.5f;
							((float*)&v4->x)[i]=((float*)&v0->x)[i]*0.5f+((float*)&v1->x)[i]*0.5f;
							((float*)&v5->x)[i]=((float*)&v1->x)[i]*0.5f+((float*)&v2->x)[i]*0.5f;
						}

						//*TODO* Make it perspective correct

						//uv
						for (int i=0;i<2;i++)
						{
							((float*)&v3->u)[i]=((float*)&v0->u)[i]*0.5f+((float*)&v2->u)[i]*0.5f;
							((float*)&v4->u)[i]=((float*)&v0->u)[i]*0.5f+((float*)&v1->u)[i]*0.5f;
							((float*)&v5->u)[i]=((float*)&v1->u)[i]*0.5f+((float*)&v2->u)[i]*0.5f;
						}

						//color
						for (int i=0;i<4;i++)
						{
							v3->col[i]=v0->col[i]/2+v2->col[i]/2;
							v4->col[i]=v0->col[i]/2+v1->col[i]/2;
							v5->col[i]=v1->col[i]/2+v2->col[i]/2;
						}

						fill_id(lst[pfsti].id,v0,v3,v4,vtx_base);
						lst[pfsti].pid= ppid ;
						lst[pfsti].z = minZ(vtx_base,lst[pfsti].id);
						pfsti++;

						fill_id(lst[pfsti].id,v2,v3,v5,vtx_base);
						lst[pfsti].pid= ppid ;
						lst[pfsti].z = minZ(vtx_base,lst[pfsti].id);
						pfsti++;

						fill_id(lst[pfsti].id,v3,v4,v5,vtx_base);
						lst[pfsti].pid= ppid ;
						lst[pfsti].z = minZ(vtx_base,lst[pfsti].id);
						pfsti++;

						fill_id(lst[pfsti].id,v5,v4,v1,vtx_base);
						lst[pfsti].pid= ppid ;
						lst[pfsti].z = minZ(vtx_base,lst[pfsti].id);
						pfsti++;

						tess_gen+=3;
					}
					else
					{
						fill_id(lst[pfsti].id,v0,v1,v2,vtx_base);
						lst[pfsti].pid= ppid ;
						lst[pfsti].z = minZ(vtx_base,lst[pfsti].id);
						pfsti++;
					}
				}
				else
#endif
				{
					fill_id(lst[pfsti].id,v0,v1,v2,vtx_base);
					lst[pfsti].pid= ppid ;
					lst[pfsti].z = minZ(vtx_base,lst[pfsti].id);
					pfsti++;
				}

				flip ^= 1;
			}
		}
		pp++;
	}

	u32 aused=pfsti;

	lst.resize(aused);

	//sort them
#if 1
	std::stable_sort(lst.begin(),lst.end());

	//Merge pids/draw cmds if two different pids are actually equal
	if (true)
	{
		for (u32 k=1;k<aused;k++)
		{
			if (lst[k].pid!=lst[k-1].pid)
			{
				if (PP_EQ(&pp_base[lst[k].pid],&pp_base[lst[k-1].pid]))
				{
					lst[k].pid=lst[k-1].pid;
				}
			}
		}
	}
#endif


#if 0
	//tries to optimise draw calls by reordering non-intersecting polygons
	//uber slow and not very effective
	{
		int opid=lst[0].pid;

		for (int k=1;k<aused;k++)
		{
			if (lst[k].pid!=opid)
			{
				if (opid>lst[k].pid)
				{
					//MOVE UP
					for (int j=k;j>0 && lst[j].pid!=lst[j-1].pid && !Intersect(lst[j],lst[j-1]);j--)
					{
						swap(lst[j],lst[j-1]);
					}
				}
				else
				{
					//move down
					for (int j=k+1;j<aused && lst[j].pid!=lst[j-1].pid && !Intersect(lst[j],lst[j-1]);j++)
					{
						swap(lst[j],lst[j-1]);
					}
				}
			}

			opid=lst[k].pid;
		}
	}
#endif

	//re-assemble them into drawing commands
	vidx_sort.resize(aused*3);

	int idx=-1;

	for (u32 i=0; i<aused; i++)
	{
		int pid=lst[i].pid;
		u32* midx = lst[i].id;

		vidx_sort[i*3 + 0]=midx[0];
		vidx_sort[i*3 + 1]=midx[1];
		vidx_sort[i*3 + 2]=midx[2];

		if (idx!=pid /* && !PP_EQ(&pp_base[pid],&pp_base[idx]) */ )
		{
			SortTrigDrawParam stdp = { pp_base + pid, i * 3, 0 };

			if (idx!=-1)
			{
				SortTrigDrawParam* last=&pidx_sort[pidx_sort.size()-1];
				last->count=stdp.first-last->first;
			}

			pidx_sort.push_back(stdp);
			idx=pid;
		}
	}

	SortTrigDrawParam* stdp=&pidx_sort[pidx_sort.size()-1];
	stdp->count=aused*3-stdp->first;

#if PRINT_SORT_STATS
	printf("Reassembled into %d from %d\n",pidx_sort.size(),pp_end-pp_base);
#endif

	if (tess_gen) DEBUG_LOG(RENDERER, "Generated %.2fK Triangles !", tess_gen / 1000.0);
}
