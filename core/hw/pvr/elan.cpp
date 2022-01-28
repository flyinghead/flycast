/*
	Copyright 2022 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 * VideoLogic custom transformation & lighting (T&L) chip (codenamed: ELAN)
 * 32 MB RAM
 * Clock: 100 MHz
 * 16 light sources per polygon
 *   ambient, parallel, point or spot (Fog lights and alpha lights also exist)
 *   Perspective conversion
 *   Near, far and side clipping, offscreen and backface culling
 *   bump mapping, environmental mapping
 * dynamic & static model processing
 * model cache system
 *
 * Each PVR2 chip renders half the screen (rectangular, stripes, and checker board options)
 * so textures have to be duplicated in each vram
 *
 * Area 0:
 * 005f6800 - 005f7cff asic A regs
 * 005f8000 - 005f9fff CLXA regs
 * 025f6800 - 025f7cff asic B regs
 * 025f8000 - 025f9fff CLXB regs
 *
 * Area 1:
 * 05000000 - 06ffffff CLXA vram
 * 07000000 - 08ffffff CLXB vram
 *
 * Area 2:
 * 085f6800 - 085f7cff  write both asic regs
 * 085f8000 - 085f9fff  write both PVR regs
 * 08800000 - 088000ff? elan regs
 * 09000000 - ?         elan command buffer
 * 0A000000 - 0bfffffff elan RAM
 */
#include "elan.h"
#include "hw/mem/_vmem.h"
#include "pvr_mem.h"
#include "ta.h"
#include "ta_ctx.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/sh4/sh4_sched.h"
#include "emulator.h"
#include "serialize.h"
#include "elan_struct.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace elan {

static _vmem_handler elanRegHandler;
static _vmem_handler elanCmdHandler;
static _vmem_handler elanRamHandler;

u8 *RAM;

static u32 reg10;
static u32 reg74;
static u32 reg30 = 0x31;

static u32 elanCmd[32 / 4];

template<typename T>
T DYNACALL read_elanreg(u32 paddr)
{
	verify(sizeof(T) == 4);
	u32 addr = paddr & 0x01ffffff;
	switch (addr >> 16)
	{
	case 0x5F:
		if (addr >= 0x005F6800 && addr <= 0x005F7CFF)
		{
			// 5F6908: Tests for errors 4, 8, 10, 2 and 1 (render isp buf ovf, render hazard, ISP param ovf, ob list ptr ovf, ta ill param)
			// 5f6900: then int 4 and 40 (EoR TSP, EoT YUV)
			return (T)sb_ReadMem(paddr, sizeof(T));
		}
		else if (addr >= 0x005F8000 && addr <= 0x005F9FFF)
		{
			if (sizeof(T) != 4)
				// House of the Dead 2
				return 0;
			return (T)pvr_ReadReg(paddr);
		}
		else
		{
			INFO_LOG(MEMORY, "Read from area2 not implemented [Unassigned], addr=%x", addr);
			return 0;
		}

	default:
//		if ((addr & 0xFF) != 0x74)
			DEBUG_LOG(PVR, "ELAN read(%d) %08x [pc %08x]", (u32)sizeof(T), addr, p_sh4rcb->cntx.pc);
		switch (addr & 0xFF)
		{
		case 0: // magic number
			return (T)0xe1ad0000;
		case 4: // revision
			return 0x1;	// 1 or x10
							// 10 breaks vstriker?
		case 0xc:
			// command queue size
			// loops until < 2 (v1) or 3 (v10)
			return 1;
		case 0x10: // sh4 if control?
			// b0 broadcast on cs1
			// b1 elan channel 2
			// b2 enable pvr #2
			// rewritten by bios as reg10 & ~1
			return reg10;
		case 0x14: // SDRAM refresh (never read?)
			return (T)0x2029; //default 0x1429
		case 0x1c: // SDRAM CFG
			return (T)0x87320961;
		case 0x30: // Macro tiler config
			// 0 0 l l  l l l l  t t t t  0 0 r r  r r r r  b b b b  0 0 V H  0 0 0 T
			// lllll: left tile
			// tttt: top tile
			// rrrrrr: right tile
			// bbbb: bottom tile
			// V: tile vertically
			// H: tile horizontally
			// T: tiler enabled
			return reg30;
		case 0x74:
			// b0 dma completed
			// b1 cmd completed
			// b2-b3 geometry timeouts
			// b4-b6 errors?
			return reg74;
		case 0x78:	// IRQ MASK
			// 6 bits?
			return 0;
		default:
			return (T)0;
		}
	}
}

template<typename T>
void DYNACALL write_elanreg(u32 paddr, T data)
{
	verify(sizeof(T) == 4);
	u32 addr = paddr & 0x01ffffff;
	switch (addr >> 16)
	{
	case 0x5F:
		if (addr>= 0x005F6800 && addr <= 0x005F7CFF)
			sb_WriteMem(paddr, data, sizeof(T));
		else if (addr >= 0x005F8000 && addr <= 0x005F9FFF)
		{
			if (addr == 0x5F8040 && data == 0xFF00FF)
			{
				ERROR_LOG(PVR, "ELAN SCREWED pr %x pc %x", p_sh4rcb->cntx.pr, p_sh4rcb->cntx.pc);
				throw FlycastException("Boot aborted");
			}
			else if ((addr & 0x1fff) == SOFTRESET_addr && data == 0)
				reg74 &= 3;
			else if ((addr & 0x1fff) == STARTRENDER_addr)
				reg74 &= 3;

			//if ((paddr & 0x1c000000) == 0x08000000 && (addr & 0x1fff) == SOFTRESET_addr && data == 0)
			//	reg74 |= 2;
			pvr_WriteReg(paddr, data);
		}
		else
			INFO_LOG(COMMON, "Write to area2 not implemented [Unassigned], addr=%x,data=%x,size=%d", addr, data, (u32)sizeof(T));
		break;
	default:
//		if ((addr & 0xFF) != 0x74)
			DEBUG_LOG(PVR, "ELAN write(%d) %08x = %x", (u32)sizeof(T), addr, data);
		switch (addr & 0xFF)
		{
		case 0x0:
			// 0 multiple times (_kmtlifAbortDisplayListProcessing)
			break;
		// 0x4: _kmtlifAbortDisplayListProcessing: 0
		case 0x8: // write-only. reset ?
			// 1 then 0
			// bios: 5
			// _kmtlifAbortDisplayListProcessing: 5 then 0
			// _kmtlifHandleDMATimeout: 1, 0, 4, 0...
			if (data == 0)
				reg74 = 0;
			break;
		case 0xc:
			// 0
			break;
		case 0x10: // sh4 if control?
			reg10 = data;
			break;;
		case 0x14: // SDRAM refresh
			// x2029
			break;
		case 0x1c: // SDRAM CFG
			break;
		case 0x30:
			reg30 = data;
			break;
		case 0x74:	// IRQ STAT
			reg74 &= ~data;
			break;
		// _kmtlifSetupElanInts:
		// 78 = 3f
		// 7C = 0
		// 80 = 17
		// 84 = 2b
		// 88 = 0
		case 0xd0: // _kmtlifSetCullingRegister
			// 6
			break;;
		default:
			break;
		}
	}
}

template<typename T>
T DYNACALL read_elancmd(u32 addr)
{
	DEBUG_LOG(PVR, "ELAN cmd READ! (%d) %08x", (u32)sizeof(T), addr);
	return 0;
}

static GMP *curGmp;
static glm::mat4x4 curMatrix;
static float *taMVMatrix;
static float *taNormalMatrix;
static glm::mat4 projectionMatrix;
static float *taProjMatrix;
static LightModel *curLightModel;
static ElanBase *curLights[MAX_LIGHTS];
static float nearPlane = 0.001f;
static float farPlane = 100000.f;
static bool envMapping;
static bool cullingReversed;

struct State
{
	static constexpr u32 Null = 0xffffffff;

	int listType = -1;
	u32 gmp = Null;
	u32 instance = Null;
	u32 projMatrix = Null;
	u32 tileclip = 0;
	u32 lightModel = Null;
	u32 lights[MAX_LIGHTS] = {
			Null, Null, Null, Null, Null, Null, Null, Null,
			Null, Null, Null, Null, Null, Null, Null, Null
	};
	bool lightModelUpdated = false;
	float envMapUOffset = 0.f;
	float envMapVOffset = 0.f;

	void reset()
	{
		listType = -1;
		gmp = Null;
		instance = Null;
		projMatrix = Null;
		tileclip = 0;
		lightModel = Null;
		for (auto& light : lights)
			light = Null;
		update();
	}
	void setMatrix(InstanceMatrix *pinstance)
	{
		instance = elanRamAddress(pinstance);
		updateMatrix();
	}

	void updateMatrix()
	{
		if (instance == Null)
		{
			taMVMatrix = nullptr;
			taNormalMatrix = nullptr;
			envMapUOffset = 0.f;
			envMapVOffset = 0.f;
			return;
		}
		InstanceMatrix *mat = (InstanceMatrix *)&RAM[instance];
		DEBUG_LOG(PVR, "Matrix %f %f %f %f\n       %f %f %f %f\n       %f %f %f %f\nLight: %f %f %f\n       %f %f %f\n       %f %f %f",
				-mat->tm00, -mat->tm10, -mat->tm20, -mat->tm30,
				mat->tm01, mat->tm11, mat->tm21, mat->tm31,
				-mat->tm02, -mat->tm12, -mat->tm22, -mat->tm32,
				mat->lm00, mat->lm10, mat->lm20,
				mat->lm01, mat->lm11, mat->lm21,
				mat->lm02, mat->lm12, mat->lm22);

		curMatrix = glm::mat4x4{
			-mat->tm00, mat->tm01, -mat->tm02, 0.f,
			-mat->tm10, mat->tm11, -mat->tm12, 0.f,
			-mat->tm20, mat->tm21, -mat->tm22, 0.f,
			-mat->tm30, mat->tm31, -mat->tm32, 1.f
		};
		glm::mat4x4 normalMatrix = glm::mat4x4{
			mat->lm00, mat->lm01, mat->lm02, 0.f,
			mat->lm10, mat->lm11, mat->lm12, 0.f,
			mat->lm20, mat->lm21, mat->lm22, 0.f,
			-mat->tm30, mat->tm31, -mat->tm32, 1.f
		};
		nearPlane = mat->_near;
		farPlane = mat->_far;
		envMapUOffset = mat->envMapU;
		envMapVOffset = mat->envMapV;
		taMVMatrix = ta_add_matrix(glm::value_ptr(curMatrix));
		if (normalMatrix != curMatrix)
			taNormalMatrix = ta_add_matrix(glm::value_ptr(normalMatrix));
		else
			taNormalMatrix = taMVMatrix;
	}

	void setProjectionMatrix(void *p)
	{
		projMatrix = elanRamAddress(p);
		updateProjectionMatrix();
	}

	void updateProjectionMatrix()
	{
		if (projMatrix == Null)
		{
			taProjMatrix = nullptr;
			return;
		}
		ProjMatrix *pm = (ProjMatrix *)&RAM[projMatrix];
		DEBUG_LOG(PVR, "Proj matrix x: %f %f y: %f %f near %f far %f", pm->fx, pm->tx, pm->fy, pm->ty, nearPlane, farPlane);
		// fx = -m00 * w/2
		// tx = -m20 * w/2 + left + w/2
		// fy = -m11 * h/2
		// ty = -m21 * h/2 + top + h/2
		projectionMatrix = glm::mat4(
				-pm->fx,  0,       0,  0,
				0,        pm->fy,  0,  0,
				-pm->tx, -pm->ty, -1, -1,
				0,        0,       0,  0
		);
		taProjMatrix = ta_add_matrix(glm::value_ptr(projectionMatrix));
	}

	void setGMP(void *p)
	{
		gmp = elanRamAddress(p);
		updateGMP();
	}

	void updateGMP() {
		if (gmp == Null)
			curGmp = nullptr;
		else
		{
			curGmp = (GMP *)&RAM[gmp];
			DEBUG_LOG(PVR, "GMP paramSelect %x clip %d", curGmp->paramSelect.full, curGmp->pcw.userClip);
		}
	}

	void setLightModel(void *p)
	{
		lightModel = elanRamAddress(p);
		updateLightModel();
	}

	void updateLightModel()
	{
		lightModelUpdated = true;
		if (lightModel == Null)
			curLightModel = nullptr;
		else
		{
			curLightModel = (LightModel *)&RAM[lightModel];
			DEBUG_LOG(PVR, "Light model mask: diffuse %04x specular %04x, ambient base %08x offset %08x", curLightModel->diffuseMask0, curLightModel->specularMask0,
					curLightModel->ambientBase0, curLightModel->ambientOffset0);
		}
	}

	void setLight(int lightId, void *p)
	{
		lights[lightId] = elanRamAddress(p);
		updateLight(lightId);
	}

	void updateLight(int lightId)
	{
		lightModelUpdated = true;
		if (lights[lightId] == Null)
		{
			elan::curLights[lightId] = nullptr;
			return;
		}
		PointLight *plight = (PointLight *)&RAM[lights[lightId]];
		if (plight->pcw.parallelLight)
		{
			ParallelLight *light = (ParallelLight *)plight;
			DEBUG_LOG(PVR, "  Parallel light %d: col %d %d %d dir %d %d %d", light->lightId, light->red, light->green, light->blue,
					light->dirX, light->dirY, light->dirZ);
		}
		else
		{
			DEBUG_LOG(PVR, "  Point light %d: dattenmode %d col %d %d %d dir %d %d %d pos %f %f %f routing %d dist %f %f angle %f %f",
					plight->lightId, plight->dattenmode,
					plight->red, plight->green, plight->blue,
					plight->dirX, plight->dirY, plight->dirZ,
					plight->posX, plight->posY, plight->posZ,
					plight->routing, plight->attnMinDistance(), plight->attnMaxDistance(),
					plight->attnMinAngle(), plight->attnMaxAngle());
		}
		elan::curLights[lightId] = plight;
	}

	void setClipMode(PCW pcw)
	{
		tileclip = (tileclip & ~0xF0000000) | (pcw.userClip << 28);
	}

	void setClipTiles(u32 xmin, u32 ymin, u32 xmax, u32 ymax)
	{
		u32 t = tileclip & 0xF0000000;
		t |= xmin & 0x3f;         // 6 bits
		t |= (xmax & 0x3f) << 6;  // 6 bits
		t |= (ymin & 0x1f) << 12; // 5 bits
		t |= (ymax & 0x1f) << 17; // 5 bits
		tileclip = t;
	}

	void update()
	{
		updateMatrix();
		updateProjectionMatrix();
		updateGMP();
		updateLightModel();
		for (u32 i = 0; i < MAX_LIGHTS; i++)
			updateLight(i);
	}

	static u32 elanRamAddress(void *p)
	{
		if ((u8 *)p < RAM || (u8 *)p >= RAM + ELAN_RAM_SIZE)
			return Null;
		else
			return (u32)((u8 *)p - RAM);
	}

	void serialize(Serializer& ser)
	{
		ser << listType;
		ser << gmp;
		ser << instance;
		ser << projMatrix;
		ser << tileclip;
		ser << lightModel;
		ser << lights;
	}

	void deserialize(Deserializer& deser)
	{
		if (deser.version() < Deserializer::V24)
		{
			reset();
			return;
		}
		deser >> listType;
		deser >> gmp;
		deser >> instance;
		deser >> projMatrix;
		deser >> tileclip;
		deser >> lightModel;
		deser >> lights;
		update();
	}
};

static State state;

static void setCoords(Vertex& vtx, float x, float y, float z)
{
	vtx.x = x;
	vtx.y = y;
	vtx.z = z;
}

template <typename Ts>
static void setUV(const Ts& vs, Vertex& vd)
{
	if (envMapping)
	{
		vd.u = state.envMapUOffset;
		vd.v = state.envMapVOffset;
	}
	else
	{
		vd.u = vs.uv.u;
		vd.v = vs.uv.v;
	}
}

static void SetEnvMapUV(Vertex& vtx)
{
	if (envMapping)
	{
		vtx.u = state.envMapUOffset;
		vtx.v = state.envMapVOffset;
	}
}

static glm::vec4 unpackColor(u32 color)
{
	return glm::vec4((float)((color >> 16) & 0xff) / 255.f,
			(float)((color >> 8) & 0xff) / 255.f,
			(float)(color & 0xff) / 255.f,
			(float)(color >> 24) / 255.f);
}

static glm::vec4 unpackColor(u8 red, u8 green, u8 blue, u8 alpha = 0)
{
	return glm::vec4((float)red / 255.f, (float)green / 255.f, (float)blue / 255.f, (float)alpha / 255.f);
}

static u32 packColor(const glm::vec4& color)
{
	return (int)(std::max(0.f, std::min(1.f, color.a)) * 255.f) << 24
			| (int)(std::max(0.f, std::min(1.f, color.r)) * 255.f) << 16
			| (int)(std::max(0.f, std::min(1.f, color.g)) * 255.f) << 8
			| (int)(std::max(0.f, std::min(1.f, color.b)) * 255.f);
}

template<typename T>
glm::vec3 getNormal(const T& vtx)
{
	return { (int8_t)vtx.header.nx / 127.f, (int8_t)vtx.header.ny / 127.f, (int8_t)vtx.header.nz / 127.f };
}

template<>
glm::vec3 getNormal(const N2_VERTEX_VNU& vtx)
{
	return { vtx.normal.nx, vtx.normal.ny, vtx.normal.nz };
}

template<typename T>
void setNormal(Vertex& vd, const T& vs)
{
	glm::vec3 normal = getNormal(vs);
	vd.nx = normal.x;
	vd.ny = normal.y;
	vd.nz = normal.z;
}

template <typename T>
static void convertVertex(const T& vs, Vertex& vd);

template<>
void convertVertex(const N2_VERTEX& vs, Vertex& vd)
{
	setCoords(vd, vs.x, vs.y, vs.z);
	setNormal(vd, vs);
	SetEnvMapUV(vd);
	glm::vec4 baseCol0;
	glm::vec4 offsetCol0;
	glm::vec4 baseCol1;
	glm::vec4 offsetCol1;
	if (curGmp != nullptr)
	{
		baseCol0 = unpackColor(curGmp->diffuse0);
		offsetCol0 = unpackColor(curGmp->specular0);
		baseCol1 = unpackColor(curGmp->diffuse1);
		offsetCol1 = unpackColor(curGmp->specular1);
		if (state.listType == 2)
		{
			// FIXME
			baseCol0.a = 0;
			offsetCol0.a = 1;
			baseCol1.a = 0;
			offsetCol1.a = 1;
		}
	}
	else
	{
		baseCol0 = glm::vec4(0);
		offsetCol0 = glm::vec4(0);
		baseCol1 = glm::vec4(0);
		offsetCol1 = glm::vec4(0);
	}
	// non-textured vertices have no offset color
	*(u32 *)vd.col = packColor(baseCol0 + offsetCol0);
	*(u32 *)vd.col1 = packColor(baseCol1 + offsetCol1);
}

template<>
void convertVertex(const N2_VERTEX_VR& vs, Vertex& vd)
{
	setCoords(vd, vs.x, vs.y, vs.z);
	setNormal(vd, vs);
	SetEnvMapUV(vd);
	glm::vec4 baseCol0 = unpackColor(vs.rgb.argb0);
	glm::vec4 offsetCol0 = baseCol0;
	glm::vec4 baseCol1 = unpackColor(vs.rgb.argb1);
	glm::vec4 offsetCol1 = baseCol1;
	if (curGmp != nullptr)
	{
		// Not sure about offset but vf4 needs base addition
		baseCol0 += unpackColor(curGmp->diffuse0);
		offsetCol0 += unpackColor(curGmp->specular0);
		baseCol1 += unpackColor(curGmp->diffuse1);
		offsetCol1 += unpackColor(curGmp->specular1);
	}
	// non-textured vertices have no offset color
	*(u32 *)vd.col = packColor(baseCol0 + offsetCol0);
	*(u32 *)vd.col1 = packColor(baseCol1 + offsetCol1);
}

template<>
void convertVertex(const N2_VERTEX_VU& vs, Vertex& vd)
{
	setCoords(vd, vs.x, vs.y, vs.z);
	setNormal(vd, vs);
	setUV(vs, vd);
	glm::vec4 baseCol0;
	glm::vec4 offsetCol0;
	glm::vec4 baseCol1;
	glm::vec4 offsetCol1;
	if (curGmp != nullptr)
	{
		baseCol0 = unpackColor(curGmp->diffuse0);
		offsetCol0 = unpackColor(curGmp->specular0);
		baseCol1 = unpackColor(curGmp->diffuse1);
		offsetCol1 = unpackColor(curGmp->specular1);
	}
	else
	{
		baseCol0 = glm::vec4(0);
		offsetCol0 = glm::vec4(0);
		baseCol1 = glm::vec4(0);
		offsetCol1 = glm::vec4(0);
	}
	*(u32 *)vd.col = packColor(baseCol0);
	*(u32 *)vd.spc = packColor(offsetCol0);
	*(u32 *)vd.col1 = packColor(baseCol1);
	*(u32 *)vd.spc1 = packColor(offsetCol1);
}

template<>
void convertVertex(const N2_VERTEX_VUR& vs, Vertex& vd)
{
	setCoords(vd, vs.x, vs.y, vs.z);
	setNormal(vd, vs);
	setUV(vs, vd);
	glm::vec4 baseCol0 = unpackColor(vs.rgb.argb0);
	glm::vec4 offsetCol0 = baseCol0;
	glm::vec4 baseCol1 = unpackColor(vs.rgb.argb1);
	glm::vec4 offsetCol1 = baseCol1;
	if (curGmp != nullptr)
	{
		// Not sure about offset but vf4 needs base addition
		baseCol0 += unpackColor(curGmp->diffuse0);
		offsetCol0 += unpackColor(curGmp->specular0);
		baseCol1 += unpackColor(curGmp->diffuse1);
		offsetCol1 += unpackColor(curGmp->specular1);
	}
	*(u32 *)vd.col = packColor(baseCol0);
	*(u32 *)vd.spc = packColor(offsetCol0);
	*(u32 *)vd.col1 = packColor(baseCol1);
	*(u32 *)vd.spc1 = packColor(offsetCol1);
}

template<>
void convertVertex(const N2_VERTEX_VUB& vs, Vertex& vd)
{
	setCoords(vd, vs.x, vs.y, vs.z);
	setNormal(vd, vs);
	setUV(vs, vd);
	glm::vec4 baseCol0;
	glm::vec4 baseCol1;
	if (curGmp != nullptr)
	{
		baseCol0 = unpackColor(curGmp->diffuse0);
		baseCol1 = unpackColor(curGmp->diffuse1);
	}
	else
	{
		baseCol0 = glm::vec4(0);
		baseCol1 = glm::vec4(0);
	}
	*(u32 *)vd.col = packColor(baseCol0);
	*(u32 *)vd.col1 = packColor(baseCol1);
	// Stuff the bump map normals and parameters in the specular colors
	vd.spc[0] = vs.bump.tangent.x;
	vd.spc[1] = vs.bump.tangent.y;
	vd.spc[2] = vs.bump.tangent.z;
	vd.spc1[0] = vs.bump.bitangent.x;
	vd.spc1[1] = vs.bump.bitangent.y;
	vd.spc1[2] = vs.bump.bitangent.z;
	vd.spc[3] = vs.bump.scaleFactor.bumpDegree; // always 255?
	vd.spc1[3] = vs.bump.scaleFactor.fixedOffset; // always 0?
//	int nx = (int8_t)vs.header.nx;
//	int ny = (int8_t)vs.header.ny;
//	int nz = (int8_t)vs.header.nz;
//	printf("BumpMap vtx deg %d off %d normal %d %d %d tangent %d %d %d bitangent %d %d %d dot %d %d %d\n", vs.bump.scaleFactor.bumpDegree, vs.bump.scaleFactor.fixedOffset,
//			nx, ny, nz,
//			vs.bump.tangent.x, vs.bump.tangent.y, vs.bump.tangent.z, vs.bump.bitangent.x, vs.bump.bitangent.y, vs.bump.bitangent.z,
//			nx * vs.bump.tangent.x + ny * vs.bump.tangent.y + nz * vs.bump.tangent.z,
//			nx * vs.bump.bitangent.x + ny * vs.bump.bitangent.y + nz * vs.bump.bitangent.z,
//			vs.bump.tangent.x * vs.bump.bitangent.x + vs.bump.tangent.y * vs.bump.bitangent.y + vs.bump.tangent.z * vs.bump.bitangent.z
//			);
}

template <typename T>
static void boundingBox(const T* vertices, u32 count, glm::vec3& min, glm::vec3& max)
{
	min = { 1e38f, 1e38f, 1e38f };
	max = { -1e38f, -1e38f, -1e38f };
	for (u32 i = 0; i < count; i++)
	{
		glm::vec3 pos{ vertices[i].x, vertices[i].y, vertices[i].z };
		min = glm::min(min, pos);
		max = glm::max(max, pos);
	}
	glm::vec4 center((min + max) / 2.f, 1);
	glm::vec4 extents(max - glm::vec3(center), 0);
	// transform
	center = curMatrix * center;
	glm::vec3 extentX = curMatrix * glm::vec4(extents.x, 0, 0, 0);
	glm::vec3 extentY = curMatrix * glm::vec4(0, extents.y, 0, 0);
	glm::vec3 extentZ = curMatrix * glm::vec4(0, 0, extents.z, 0);
	// new AA extents
	glm::vec3 newExtent = glm::abs(extentX) + glm::abs(extentY) + glm::abs(extentZ);

	min = glm::vec3(center) - newExtent;
	max = glm::vec3(center) + newExtent;
}

template <typename T>
static bool isInFrustum(const T* vertices, u32 count)
{
	glm::vec3 min;
	glm::vec3 max;
	boundingBox(vertices, count, min, max);
	if (min.z > -nearPlane || max.z < -farPlane)
		return false;

	glm::vec4 pmin = projectionMatrix * glm::vec4(min, 1);
	glm::vec4 pmax = projectionMatrix * glm::vec4(max, 1);
	if (std::isnan(pmin.x) || std::isnan(pmin.y) || std::isnan(pmax.x) || std::isnan(pmax.y))
		return false;
//	// Check the farthest side
//	float w = std::max(pmin.w, pmax.w);
//	glm::vec2 smin = glm::min(glm::vec2(pmin) / w, glm::vec2(pmax) / w);
//	glm::vec2 smax = glm::max(glm::vec2(pmin) / w, glm::vec2(pmax) / w);
//
//	if (smax.x <= -214 || smin.x  >= 854	// FIXME viewport dimensions
//		|| smax.y < 0 || smin.y >= 480)
//		return false;

	return true;
}

template <typename T>
static void sendVertices(const ICHList *list, const T* vtx)
{
	Vertex taVtx;
	verify(list->vertexSize() > 0);

	Vertex fanCenterVtx{};
	Vertex fanLastVtx{};
	bool stripStart = true;
	int outStripIndex = 0;
	for (u32 i = 0; i < list->vtxCount; i++)
	{
		convertVertex(*vtx, taVtx);

		if (stripStart)
		{
			// Center vertex if triangle fan
			//verify(vtx->header.isFirstOrSecond()); This fails for some strips: strip=1 fan=0 (soul surfer)
			fanCenterVtx = taVtx;
			if (outStripIndex > 0)
			{
				// use degenerate triangles to link strips
				ta_add_vertex(fanLastVtx);
				ta_add_vertex(taVtx);
				outStripIndex += 2;
				if (outStripIndex & 1)
				{
					ta_add_vertex(taVtx);
					outStripIndex++;
				}
			}
			stripStart = false;
		}
		else if (vtx->header.isFan())
		{
			// use degenerate triangles to link strips
			ta_add_vertex(fanLastVtx);
			ta_add_vertex(fanCenterVtx);
			outStripIndex += 2;
			if (outStripIndex & 1)
			{
				ta_add_vertex(fanCenterVtx);
				outStripIndex++;
			}
			// Triangle fan
			ta_add_vertex(fanCenterVtx);
			ta_add_vertex(fanLastVtx);
			outStripIndex += 2;
		}
		ta_add_vertex(taVtx);
		outStripIndex++;
		fanLastVtx = taVtx;
		if (vtx->header.endOfStrip)
			stripStart = true;

		vtx++;
	}
}

template <typename T>
static void sendMVVertices(const ICHList *list, const T* vtx)
{
	verify(list->vertexSize() > 0);

	glm::vec3 vtx0{};
	glm::vec3 vtx1{};
	u32 stripStart = 0;

	for (u32 i = 0; i < list->vtxCount; i++)
	{
		glm::vec3 v(vtx->x, vtx->y, vtx->z);
//		printf("MV %f %f %f - strip %d fan %d eos %d _res %x\n", v.x, v.y, 1 / v.w, vtx->header.strip, vtx->header.fan, vtx->header.endOfStrip, vtx->header._res);
		u32 triIdx = i - stripStart;
		if (triIdx >= 2)
		{
			ModTriangle tri;

			if (triIdx & 1)
			{
				tri.x1 = vtx0.x;
				tri.y1 = vtx0.y;
				tri.z1 = vtx0.z;

				tri.x0 = vtx1.x;
				tri.y0 = vtx1.y;
				tri.z0 = vtx1.z;
			}
			else
			{
				tri.x0 = vtx0.x;
				tri.y0 = vtx0.y;
				tri.z0 = vtx0.z;

				tri.x1 = vtx1.x;
				tri.y1 = vtx1.y;
				tri.z1 = vtx1.z;
			}
			tri.x2 = v.x;
			tri.y2 = v.y;
			tri.z2 = v.z;

			ta_add_triangle(tri);
		}
		if (vtx->header.endOfStrip)
			stripStart = i + 1;
		vtx0 = vtx1;
		vtx1 = v;
		vtx++;
	}
}

static N2LightModel *taLightModel;
static bool usingAlphaLight;

static void sendLights()
{
	if (!state.lightModelUpdated)
		return;

	state.lightModelUpdated = false;
	usingAlphaLight = false;
	N2LightModel model;
	model.lightCount = 0;
	if (curLightModel == nullptr)
	{
		model.ambientMaterial = false;
		model.useBaseOver = false;
		model.ambientBase[0] = model.ambientBase[1] = model.ambientBase[2] = model.ambientBase[3] = 1.f;
		memset(model.ambientOffset, 0, sizeof(model.ambientOffset));
		return;
	}
	model.ambientMaterial = curLightModel->useAmbientBase0;
	// TODO model.ambientMaterialForSpec = curLightModel->useAmbientOffset0;
	model.useBaseOver = curLightModel->useBaseOver;
	memcpy(model.ambientBase, glm::value_ptr(unpackColor(curLightModel->ambientBase0)), sizeof(model.ambientBase));
	memcpy(model.ambientOffset, glm::value_ptr(unpackColor(curLightModel->ambientOffset0)), sizeof(model.ambientOffset));
	for (u32 i = 0; i < MAX_LIGHTS; i++)
	{
		bool diffuse = curLightModel->isDiffuse(i);
		bool specular = curLightModel->isSpecular(i);
		if (!diffuse && !specular)
			continue;
		if (curLights[i] == nullptr)
		{
			INFO_LOG(PVR, "Light %d is referenced but undefined", i);
			continue;
		}
		N2Light& light = model.lights[model.lightCount];
		light.diffuse = diffuse;
		light.specular = specular;
		light.parallel = curLights[i]->pcw.parallelLight;
		if (light.parallel != 0)
		{
			ParallelLight *plight = (ParallelLight *)curLights[i];
			memcpy(light.color, glm::value_ptr(unpackColor(plight->red, plight->green, plight->blue)), sizeof(light.color));
			light.routing = plight->routing;
			light.dmode = plight->dmode;
			light.smode = N2_LMETHOD_SINGLE_SIDED;
			memcpy(light.direction, glm::value_ptr(glm::normalize(glm::vec4(-(int8_t)plight->dirX, (int8_t)plight->dirY, -(int8_t)plight->dirZ, 0))),
					sizeof(light.direction));
		}
		else
		{
			PointLight *plight = (PointLight *)curLights[i];
			memcpy(light.color, glm::value_ptr(unpackColor(plight->red, plight->green, plight->blue)), sizeof(light.color));
			light.routing = plight->routing;
			light.dmode = plight->dmode;
			light.smode = plight->smode;
			memcpy(light.position, glm::value_ptr(glm::vec4(plight->posX, plight->posY, plight->posZ, 1)), sizeof(light.position));
			memcpy(light.direction, glm::value_ptr(glm::normalize(glm::vec4((int8_t)plight->dirX, (int8_t)plight->dirY, (int8_t)plight->dirZ, 0))),
					sizeof(light.direction));
			light.distAttnMode = plight->dattenmode;
			light.attnDistA = plight->distA();
			light.attnDistB = plight->distB();
			light.attnAngleA = plight->angleA();
			light.attnAngleB = plight->angleB();
		}
		usingAlphaLight = usingAlphaLight || light.routing == N2_LFUNC_ALPHADIFF_SUB;
		model.lightCount++;
	}
	taLightModel = ta_add_light(model);
}

static void setStateParams(PolyParam& pp)
{
	sendLights();
	pp.tileclip = state.tileclip;
	pp.mvMatrix = taMVMatrix;
	pp.normalMatrix = taNormalMatrix;
	pp.projMatrix = taProjMatrix;
	pp.lightModel = taLightModel;
	pp.envMapping = false;
	if (curGmp != nullptr)
	{
		pp.glossCoef0 = curGmp->gloss.getCoef0();
		pp.glossCoef1 = curGmp->gloss.getCoef1();
	}
	// FIXME hack ScrInstr condition fixes lens flares in vf4
	if (state.listType == 2 && usingAlphaLight && pp.tsp.SrcInstr == 1)
	{
		//printf("gmp pselect %x\n", curGmp->paramSelect.full); // ff ... not relevant
		pp.tsp.UseAlpha = 1; // TODO alpha light volumes need manual settings of which params?
		pp.tsp.ShadInstr = 3;
		pp.tsp.SrcInstr = 4;
		pp.tsp.DstInstr = 5;
	}
	// projFlip is for left-handed projection matrices (initd rear view mirror)
	bool projFlip = taProjMatrix != nullptr && std::signbit(taProjMatrix[0]) == std::signbit(taProjMatrix[5]);
	pp.isp.CullMode ^= (u32)cullingReversed ^ (u32)projFlip;
	if (pp.pcw.Volume == 0)
	{
		pp.tsp1.full = -1;
		pp.tcw1.full = -1;
	}
}

static void sendPolygon(ICHList *list)
{
	switch (list->flags)
	{
	case ICHList::VTX_TYPE_V:
		{
			N2_VERTEX *vtx = (N2_VERTEX *)((u8 *)list + sizeof(ICHList));
			if (!isInFrustum(vtx, list->vtxCount))
				break;
			if (state.listType & 1)
			{
				ModifierVolumeParam mvp{};
				mvp.isp.full = list->isp.full;
				mvp.isp.CullMode = 0; // FIXME required for closed volumes and not set properly
				if (mvp.isp.DepthMode >= 3)
					INFO_LOG(PVR, "MV mode %d", mvp.isp.DepthMode);
				mvp.isp.VolumeLast = list->pcw.volume;
				mvp.isp.DepthMode &= 3;
				mvp.mvMatrix = taMVMatrix;
				mvp.projMatrix = taProjMatrix;
				ta_add_poly(state.listType, mvp);

				//for (int i = 0; i < list->vtxCount; i++)
				//	printf("MV %f %f %f strip %d fan %d eos %d _res %x\n", vtx[i].x, vtx[i].y, vtx[i].z, vtx[i].header.strip, vtx[i].header.fan, vtx[i].header.endOfStrip, vtx[i].header._res);
				sendMVVertices(list, vtx);
			}
			else
			{
				PolyParam pp{};
				pp.pcw.Shadow = list->pcw.shadow;
				pp.pcw.Gouraud = list->pcw.gouraud;
				pp.pcw.Volume = list->pcw.volume;
				pp.isp = list->isp;
				pp.tsp = list->tsp0;
				pp.tsp1 = list->tsp1;
				setStateParams(pp);
				if (curGmp != nullptr && curGmp->paramSelect.e0)
				{
					// Environment mapping
					pp.pcw.Texture = 1;
					pp.pcw.Offset = 0;
					pp.tsp.UseAlpha = 1;
					pp.tsp.IgnoreTexA = 0;
					pp.envMapping = true;
					pp.tcw = list->tcw0;
					envMapping = true;
				}
				ta_add_poly(state.listType, pp);

				sendVertices(list, vtx);
				envMapping = false;
			}
		}
		break;

	case ICHList::VTX_TYPE_VU:
		{
			N2_VERTEX_VU *vtx = (N2_VERTEX_VU *)((u8 *)list + sizeof(ICHList));
			if (!isInFrustum(vtx, list->vtxCount))
				break;
			if (state.listType  & 1)
			{
				ModifierVolumeParam mvp{};
				mvp.isp.full = list->isp.full;
				mvp.isp.CullMode = 0; // FIXME required for closed volumes and not set properly
				if (mvp.isp.DepthMode >= 3)
					INFO_LOG(PVR, "MV mode %d", mvp.isp.DepthMode);
				mvp.isp.VolumeLast = list->pcw.volume;
				mvp.isp.DepthMode &= 3;
				mvp.mvMatrix = taMVMatrix;
				mvp.projMatrix = taProjMatrix;
				ta_add_poly(state.listType, mvp);

				//for (int i = 0; i < list->vtxCount; i++)
				//	printf("MV %f %f %f strip %d fan %d eos %d _res %x\n", vtx[i].x, vtx[i].y, vtx[i].z, vtx[i].header.strip, vtx[i].header.fan, vtx[i].header.endOfStrip, vtx[i].header._res);
				sendMVVertices(list, vtx);
			}
			else
			{
				PolyParam pp{};
				pp.pcw.Shadow = list->pcw.shadow;
				pp.pcw.Texture = 1;
				pp.pcw.Offset = list->pcw.offset;
				pp.pcw.Gouraud = list->pcw.gouraud;
				pp.pcw.Volume = list->pcw.volume;
				pp.isp = list->isp;
				pp.tsp = list->tsp0;
				pp.tcw = list->tcw0;
				pp.tsp1 = list->tsp1;
				pp.tcw1 = list->tcw1;
				setStateParams(pp);
				if (curGmp != nullptr && curGmp->paramSelect.e0)
				{
					// Environment mapping
					pp.pcw.Offset = 0;
					pp.tsp.UseAlpha = 1;
					pp.tsp.IgnoreTexA = 0;
					pp.envMapping = true;
					envMapping = true;
				}
				ta_add_poly(state.listType, pp);

				sendVertices(list, vtx);
				envMapping = false;
			}
		}
		break;

	case ICHList::VTX_TYPE_VUR:
		{
			verify(curGmp == nullptr || curGmp->paramSelect.e0 == 0);
			N2_VERTEX_VUR *vtx = (N2_VERTEX_VUR *)((u8 *)list + sizeof(ICHList));
			if (!isInFrustum(vtx, list->vtxCount))
				break;
			PolyParam pp{};
			pp.pcw.Shadow = list->pcw.shadow;
			pp.pcw.Texture = 1;
			pp.pcw.Offset = list->pcw.offset;
			pp.pcw.Gouraud = list->pcw.gouraud;
			pp.pcw.Volume = list->pcw.volume;
			pp.isp = list->isp;
			pp.tsp = list->tsp0;
			pp.tcw = list->tcw0;
			pp.tsp1 = list->tsp1;
			pp.tcw1 = list->tcw1;
			setStateParams(pp);
			ta_add_poly(state.listType, pp);

			sendVertices(list, vtx);
		}
		break;

	case ICHList::VTX_TYPE_VR:
		{
			N2_VERTEX_VR *vtx = (N2_VERTEX_VR *)((u8 *)list + sizeof(ICHList));
			if (!isInFrustum(vtx, list->vtxCount))
				break;
			PolyParam pp{};
			pp.pcw.Shadow = list->pcw.shadow;
			pp.pcw.Gouraud = list->pcw.gouraud;
			pp.pcw.Volume = list->pcw.volume;
			pp.isp = list->isp;
			pp.tsp = list->tsp0;
			pp.tsp1 = list->tsp1;
			setStateParams(pp);
			if (curGmp != nullptr && curGmp->paramSelect.e0)
			{
				// FIXME doesn't seem to work
				// Environment mapping
				pp.pcw.Texture = 1;
				pp.pcw.Offset = 0;
				pp.tsp.UseAlpha = 1;
				pp.tsp.IgnoreTexA = 0;
				pp.envMapping = true;
				pp.tcw = list->tcw0;
				envMapping = true;
			}
			ta_add_poly(state.listType, pp);

			sendVertices(list, vtx);
			envMapping = false;
		}
		break;

	case ICHList::VTX_TYPE_VUB:
		{
			// TODO
			//printf("BUMP MAP fmt %d filter %d src select %d dst %d\n", list->tcw0.PixelFmt, list->tsp0.FilterMode, list->tsp0.SrcSelect, list->tsp0.DstSelect);
			N2_VERTEX_VUB *vtx = (N2_VERTEX_VUB *)((u8 *)list + sizeof(ICHList));
			if (!isInFrustum(vtx, list->vtxCount))
				break;
			PolyParam pp{};
			pp.pcw.Shadow = list->pcw.shadow;
			pp.pcw.Texture = 1;
			pp.pcw.Offset = 1;
			pp.pcw.Gouraud = list->pcw.gouraud;
			pp.pcw.Volume = list->pcw.volume;
			pp.isp = list->isp;
			pp.tsp = list->tsp0;
			pp.tcw = list->tcw0;
			pp.tsp1 = list->tsp1;
			pp.tcw1 = list->tcw1;
			setStateParams(pp);
			ta_add_poly(state.listType, pp);

			sendVertices(list, vtx);
		}
		break;

	default:
		WARN_LOG(PVR, "Unhandled poly format %x", list->flags);
		die("Unsupported");
		break;
	}
}

static void executeCommand(u8 *data, int size)
{
	verify(size >= 0);
	verify(size < (int)ELAN_RAM_SIZE);
//	if (0x2b00 == (u32)(data - RAM))
//		for (int i = 0; i < size; i += 4)
//			DEBUG_LOG(PVR, "Elan Parse %08x: %08x", (u32)(&data[i] - RAM), *(u32 *)&data[i]);

	while (size >= 32)
	{
		const int oldSize = size;
		ElanBase *cmd = (ElanBase *)data;
		if (cmd->pcw.naomi2)
		{
			switch(cmd->pcw.n2Command)
			{
			case PCW::null:
				size -= 32;
				break;

			case PCW::projMatrix:
				state.setProjectionMatrix(data);
				size -= sizeof(ProjMatrix);
				break;

			case PCW::matrixOrLight:
				{
					InstanceMatrix *instance = (InstanceMatrix *)data;
					if (instance->isInstanceMatrix())
					{
						//DEBUG_LOG(PVR, "Model instance");
						state.setMatrix(instance);
						size -= sizeof(InstanceMatrix);
						break;
					}
					else if (instance->id1 & 0x10)
					{
						state.setLightModel(data);
					}
					else //if ((instance->id2 & 0x40000000) || (instance->id1 & 0xffffff00)) // FIXME what are these lights without id2|0x40000000? vf4
					{
						if (instance->pcw.parallelLight)
						{
							ParallelLight *light = (ParallelLight *)data;
							state.setLight(light->lightId, data);
						}
						else
						{
							PointLight *light = (PointLight *)data;
							state.setLight(light->lightId, data);
						}
					}
					//else
					//{
					//	WARN_LOG(PVR, "Other instance %08x %08x", instance->id1, instance->id2);
					//	for (int i = 0; i < 32; i += 4)
					//		INFO_LOG(PVR, "    %08x: %08x", (u32)(&data[i] - RAM), *(u32 *)&data[i]);
					//}
					size -= sizeof(LightModel);
				}
				break;

			case PCW::model:
				{
					Model *model = (Model *)data;
					cullingReversed = (model->id1 & 0x08000000) == 0;
					state.setClipMode(model->pcw);
					DEBUG_LOG(PVR, "Model offset %x size %x clip %d", model->offset, model->size, model->pcw.userClip);
					executeCommand(&RAM[model->offset & 0x1ffffff8], model->size);
					cullingReversed = false;
					size -= sizeof(Model);
				}
				break;

			case PCW::registerWait:
				{
					RegisterWait *wait = (RegisterWait *)data;
					if (wait->offset != (u32)-1 && wait->mask != 0)
					{
						DEBUG_LOG(PVR, "Register wait %x mask %x", wait->offset, wait->mask);
						// wait for interrupt
						HollyInterruptID inter;
						switch (wait->mask)
						{
						case 0x80:
							inter = holly_OPAQUE;
							break;
						case 0x100:
							inter = holly_OPAQUEMOD;
							break;
						case 0x200:
							inter = holly_TRANS;
							break;
						case 0x400:
							inter = holly_TRANSMOD;
							break;
						case 0x200000:
							inter = holly_PUNCHTHRU;
							break;
						default:
							WARN_LOG(PVR, "Unknown interrupt mask %x", wait->mask);
							die("unexpected");
							inter = holly_OPAQUE;
							break;
						}
						asic_RaiseInterruptBothCLX(inter);
						TA_ITP_CURRENT += 32;
						state.reset();
					}
					size -= sizeof(RegisterWait);
				}
				break;

			case PCW::link:
				{
					Link *link = (Link *)data;
					DEBUG_LOG(PVR, "Link to %x (%x)", link->offset & 0x1ffffff8, link->size);
					executeCommand(&RAM[link->offset & 0x1ffffff8], link->size);
					size -= sizeof(Link);
				}
				break;

			case PCW::gmp:
				state.setGMP(data);
				size -= sizeof(GMP);
				break;

			case PCW::ich:
				{
					ICHList *ich = (ICHList *)data;
					DEBUG_LOG(PVR, "ICH flags %x, %d verts", ich->flags, ich->vtxCount);
					sendPolygon(ich);
					size -= sizeof(ICHList) + ich->vertexSize() * ich->vtxCount;
				}
				break;

			default:
				DEBUG_LOG(PVR, "Unhandled Elan command %x", cmd->pcw.n2Command);
				size -= 32;
				break;
			}
		}
		else
		{
			u32 pcw = *(u32 *)data;
			if ((pcw & 0xd0ffff00) == 0x808c0000) // display list
			{
				DEBUG_LOG(PVR, "Display list type %d", (pcw >> 24) & 0xf);
				state.reset();
				state.listType  = (pcw >> 24) & 0xf;
				// TODO is this the right place for this?
				SQBuffer eol{};
				ta_vtx_data32(&eol);
				size -= 24 * 4;
			}
			else if ((pcw & 0xd0fcff00) == 0x80800000) // User clipping
			{
				state.setClipMode((PCW&)pcw);
				DEBUG_LOG(PVR, "User clip type %d", ((PCW&)pcw).userClip);
				size -= 0xE0;
			}
			else if ((pcw & 0xd0ffff00) == 0x80000000) // geometry follows or linked?
			{
				// FIXME this matches TA polys such as a2000009
				// no possible disambiguation since 80000000 is a valid OP poly pcw (poly type 0 / vtx 0)
				DEBUG_LOG(PVR, "Geometry type %d - %08x", (pcw >> 24) & 0xf, pcw);
				size -= 32;
				ta_add_ta_data((u32 *)(data + 32), size - 32);
				size = 32;
			}
			else if (pcw == 0x20000000)
			{
				// User clipping
				u32 *tiles = (u32 *)data + 4;
				DEBUG_LOG(PVR, "User clipping %d,%d - %d,%d", tiles[0] * 32, tiles[1] * 32,
						tiles[2] * 32, tiles[3] * 32);
				state.setClipTiles(tiles[0], tiles[1], tiles[2], tiles[3]);
				size -= 32;
			}
			else
			{
				if (pcw != 0)
					INFO_LOG(PVR, "Unhandled command %x", pcw);
				for (int i = 0; i < 32; i += 4)
					DEBUG_LOG(PVR, "    %08x: %08x", (u32)(&data[i] - RAM), *(u32 *)&data[i]);
				size -= 32;
			}
		}
		data += oldSize - size;
	}
}

template<typename T>
void DYNACALL write_elancmd(u32 addr, T data)
{
	verify(sizeof(T) == 4);
//	DEBUG_LOG(PVR, "ELAN cmd %08x = %x", addr, data);
	addr &= 0xff;
	verify(addr < 0x20);
	*(T *)&((u8 *)elanCmd)[addr] = data;

	if (addr == 0x1c)
	{
		executeCommand((u8 *)elanCmd, sizeof(elanCmd));
		reg74 |= 2;
		reg74 &= ~0x3c;
	}
}

template<typename T>
T DYNACALL read_elanram(u32 addr)
{
	return *(T *)&RAM[addr & (ELAN_RAM_SIZE - 1)];
}

template<typename T>
void DYNACALL write_elanram(u32 addr, T data)
{
	*(T *)&RAM[addr & (ELAN_RAM_SIZE - 1)] = data;
}

void init()
{
}

void reset(bool hard)
{
	if (hard)
	{
		memset(RAM, 0, ELAN_RAM_SIZE);
		state.reset();
	}
}

void term()
{
}

void vmem_init()
{
	elanRegHandler = _vmem_register_handler_Template(read_elanreg, write_elanreg);
	elanCmdHandler = _vmem_register_handler_Template(read_elancmd, write_elancmd);
	elanRamHandler = _vmem_register_handler_Template(read_elanram, write_elanram);
}

void vmem_map(u32 base)
{
	_vmem_map_handler(elanRegHandler, base | 8, base | 8);
	_vmem_map_handler(elanCmdHandler, base | 9, base | 9);
	_vmem_map_handler(elanRamHandler, base | 0xA, base | 0xB);
	_vmem_map_block(RAM, base | 0xA, base | 0xB, ELAN_RAM_SIZE - 1);
}

void serialize(Serializer& ser)
{
	if (!settings.platform.isNaomi2())
		return;
	ser << reg10;
	ser << reg74;
	ser << elanCmd;
	if (!ser.rollback())
		ser.serialize(RAM, ELAN_RAM_SIZE);
	state.serialize(ser);
}

void deserialize(Deserializer& deser)
{
	if (!settings.platform.isNaomi2())
		return;
	deser >> reg10;
	deser >> reg74;
	deser >> elanCmd;
	if (!deser.rollback())
		deser.deserialize(RAM, ELAN_RAM_SIZE);
	state.deserialize(deser);
}

}
