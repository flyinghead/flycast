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
#include "sorter.h"
#include "hw/pvr/Renderer_if.h"
#include <algorithm>

struct IndexTrig
{
	u32 id[3];
	u16 pid;
	f32 z;
};

static float minZ(const Vertex *v, const u32 *mod)
{
	return std::min(std::min(v[mod[0]].z, v[mod[1]].z), v[mod[2]].z);
}

static bool operator<(const IndexTrig& left, const IndexTrig& right)
{
	return left.z < right.z;
}

static float getProjectedZ(const Vertex *v, const float *mat)
{
	// -1 / z
	return -1 / (mat[2] * v->x + mat[1 * 4 + 2] * v->y + mat[2 * 4 + 2] * v->z + mat[3 * 4 + 2]);
}

const static Vertex *vtx_sort_base;

static void fill_id(u32 *d, const Vertex *v0, const Vertex *v1, const Vertex *v2,  const Vertex *vb)
{	
	d[0] = (u32)(v0 - vb);
	d[1] = (u32)(v1 - vb);
	d[2] = (u32)(v2 - vb);
}

void GenSorted(int first, int count, std::vector<SortTrigDrawParam>& pidx_sort, std::vector<u32>& vidx_sort)
{
	u32 tess_gen=0;

	pidx_sort.clear();

	if (pvrrc.verts.used() == 0 || count == 0)
		return;

	const Vertex * const vtx_base = pvrrc.verts.head();
	const u32 * const idx_base = pvrrc.idx.head();

	const PolyParam * const pp_base = &pvrrc.global_param_tr.head()[first];
	const PolyParam *pp = pp_base;
	const PolyParam * const pp_end = pp + count;
	while (pp->count == 0 && pp < pp_end)
		pp++;
	if (pp == pp_end)
		return;

	vtx_sort_base=vtx_base;

	static u32 vtx_cnt;

	int vtx_count = pvrrc.verts.used() - idx_base[pp->first];
	if ((u32)vtx_count > vtx_cnt)
		vtx_cnt = vtx_count;

#if PRINT_SORT_STATS
	printf("TVTX: %d || %d\n",vtx_cnt,vtx_count);
#endif

	if (vtx_count<=0)
		return;

	//make lists of all triangles, with their pid and vid
	static std::vector<IndexTrig> lst;

	lst.resize(vtx_count*4);


	int pfsti=0;

	while (pp != pp_end)
	{
		u32 ppid = (u32)(pp - pp_base);

		if (pp->count > 2)
		{
			const u32 *idx = idx_base + pp->first;
			u32 flip = 0;
			float z0 = 0, z1 = 0;

			if (pp->isNaomi2())
			{
				z0 = getProjectedZ(vtx_base + idx[0], pp->mvMatrix);
				z1 = getProjectedZ(vtx_base + idx[1], pp->mvMatrix);
			}
			for (u32 i = 0; i < pp->count - 2; i++)
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
				fill_id(lst[pfsti].id, v0, v1, v2, vtx_base);
				lst[pfsti].pid = ppid;
				if (pp->isNaomi2())
				{
					float z2 = getProjectedZ(v2, pp->mvMatrix);
					lst[pfsti].z = std::min(z0, std::min(z1, z2));
					z0 = z1;
					z1 = z2;
				}
				else
				{
					lst[pfsti].z = minZ(vtx_base, lst[pfsti].id);
				}
				pfsti++;

				flip ^= 1;
			}
		}
		pp++;
	}

	u32 aused=pfsti;

	lst.resize(aused);

	//sort them
	std::stable_sort(lst.begin(),lst.end());

	//Merge pids/draw cmds if two different pids are actually equal
	for (u32 k = 1; k < aused; k++)
		if (lst[k].pid != lst[k - 1].pid)
		{
			const PolyParam& curPoly = pp_base[lst[k].pid];
			const PolyParam& prevPoly = pp_base[lst[k - 1].pid];
			if (curPoly.equivalentIgnoreCullingDirection(prevPoly)
					&& (curPoly.isp.CullMode < 2 || curPoly.isp.CullMode == prevPoly.isp.CullMode))
				lst[k].pid = lst[k - 1].pid;
		}

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
				SortTrigDrawParam& last = pidx_sort.back();
				last.count = stdp.first - last.first;
			}

			pidx_sort.push_back(stdp);
			idx=pid;
		}
	}

	if (!pidx_sort.empty())
	{
		SortTrigDrawParam& last = pidx_sort.back();
		last.count = aused * 3 - last.first;
	}

#if PRINT_SORT_STATS
	printf("Reassembled into %d from %d\n",pidx_sort.size(),pp_end-pp_base);
#endif

	if (tess_gen) DEBUG_LOG(RENDERER, "Generated %.2fK Triangles !", tess_gen / 1000.0);
}

// Vulkan and DirectX use the color values of the first vertex for flat shaded triangle strips.
// On Dreamcast the last vertex is the provoking one so we must copy it onto the first.
void setFirstProvokingVertex(rend_context& rendContext)
{
	auto setProvokingVertex = [&rendContext](const List<PolyParam>& list) {
        u32 *idx_base = rendContext.idx.head();
        Vertex *vtx_base = rendContext.verts.head();
		for (const PolyParam& pp : list)
		{
			if (pp.pcw.Gouraud)
				continue;
			for (u32 i = 0; i + 2 < pp.count; i++)
			{
				Vertex& vertex = vtx_base[idx_base[pp.first + i]];
				Vertex& lastVertex = vtx_base[idx_base[pp.first + i + 2]];
				memcpy(vertex.col, lastVertex.col, sizeof(vertex.col));
				memcpy(vertex.spc, lastVertex.spc, sizeof(vertex.spc));
				memcpy(vertex.col1, lastVertex.col1, sizeof(vertex.col1));
				memcpy(vertex.spc1, lastVertex.spc1, sizeof(vertex.spc1));
			}
		}
	};
	setProvokingVertex(rendContext.global_param_op);
	setProvokingVertex(rendContext.global_param_pt);
	setProvokingVertex(rendContext.global_param_tr);
}
