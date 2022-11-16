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
#include "hw/sh4/sh4_mem.h"
#include "emulator.h"
#include "serialize.h"
#include "elan_struct.h"
#include "network/ggpo.h"
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

static u32 DYNACALL read_elanreg(u32 paddr)
{
	u32 addr = paddr & 0x01ffffff;
	switch (addr >> 16)
	{
	case 0x5F:
		if (addr >= 0x005F6800 && addr <= 0x005F7CFF)
			return sb_ReadMem(paddr, sizeof(u32));
		if (addr >= 0x005F8000 && addr <= 0x005F9FFF)
			return pvr_ReadReg(paddr);

		INFO_LOG(PVR, "Read from area2 not implemented [Unassigned], addr=%x", addr);
		return 0;

	case 0x80:
		//if ((addr & 0xFF) != 0x74)
		DEBUG_LOG(PVR, "ELAN read %08x [pc %08x]", addr, p_sh4rcb->cntx.pc);
		switch (addr & 0xFF)
		{
		case 0: // magic number
			return 0xe1ad0000;
		case 4: // revision
			return 0x10;	// 1 or x10
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
		case 0x14: // SDRAM refresh
			return 0x2029; //default 0x1429
		case 0x1c: // SDRAM CFG
			return 0x87320961;
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
			return 0;
		}

	default:
		INFO_LOG(PVR, "Read from area2 not implemented [Unassigned], addr=%x", addr);
		return 0;
	}
}

static void DYNACALL write_elanreg(u32 paddr, u32 data)
{
	u32 addr = paddr & 0x01ffffff;
	switch (addr >> 16)
	{
	case 0x5F:
		if (addr>= 0x005F6800 && addr <= 0x005F7CFF)
			sb_WriteMem(paddr, data, sizeof(u32));
		else if (addr >= 0x005F8000 && addr <= 0x005F9FFF)
			pvr_WriteReg(paddr, data);
		else
			INFO_LOG(PVR, "Write to area2 not implemented [Unassigned], addr=%x, data=%x", addr, data);
		break;

	case 0x80:
		//if ((addr & 0xFF) != 0x74)
		DEBUG_LOG(PVR, "ELAN write %08x = %x", addr, data);
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
		break;

	default:
		INFO_LOG(PVR, "Write to area2 not implemented [Unassigned], addr=%x, data=%x", addr, data);
		break;
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

static u32 packColorBGRA(const glm::vec4& color)
{
	return (int)(std::min(1.f, color.a) * 255.f) << 24
			| (int)(std::min(1.f, color.r) * 255.f) << 16
			| (int)(std::min(1.f, color.g) * 255.f) << 8
			| (int)(std::min(1.f, color.b) * 255.f);
}

static u32 packColorRGBA(const glm::vec4& color)
{
	return (int)(std::min(1.f, color.r) * 255.f)
			| (int)(std::min(1.f, color.g) * 255.f) << 8
			| (int)(std::min(1.f, color.b) * 255.f) << 16
			| (int)(std::min(1.f, color.a) * 255.f) << 24;
}

static u32 (*packColor)(const glm::vec4& color) = packColorRGBA;

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
static bool openModifierVolume;
static bool shadowedVolume;
static TSP modelTSP;
static glm::vec4 gmpDiffuseColor0;
static glm::vec4 gmpSpecularColor0;
static glm::vec4 gmpDiffuseColor1;
static glm::vec4 gmpSpecularColor1;

struct State
{
	static constexpr u32 Null = 0xffffffff;

	u32 gmp = Null;
	u32 instance = Null;
	u32 projMatrix = Null;
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
		gmp = Null;
		instance = Null;
		projMatrix = Null;
		lightModel = Null;
		for (auto& light : lights)
			light = Null;
		update();
		if (isDirectX(config::RendererType))
			packColor = packColorBGRA;
		else
			packColor = packColorRGBA;
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

	void updateGMP()
	{
		if (gmp == Null)
		{
			curGmp = nullptr;
			gmpDiffuseColor0 = glm::vec4(0);
			gmpSpecularColor0 = glm::vec4(0);
			gmpDiffuseColor1 = glm::vec4(0);
			gmpSpecularColor1 = glm::vec4(0);
		}
		else
		{
			curGmp = (GMP *)&RAM[gmp];
			DEBUG_LOG(PVR, "GMP paramSelect %x", curGmp->paramSelect.full);
			if (curGmp->paramSelect.d0)
				gmpDiffuseColor0 = unpackColor(curGmp->diffuse0);
			else
				gmpDiffuseColor0 = glm::vec4(0);
			if (curGmp->paramSelect.s0)
				gmpSpecularColor0 = unpackColor(curGmp->specular0);
			else
				gmpSpecularColor0 = glm::vec4(0);
			if (curGmp->paramSelect.d1)
				gmpDiffuseColor1 = unpackColor(curGmp->diffuse1);
			else
				gmpDiffuseColor1 = glm::vec4(0);
			if (curGmp->paramSelect.s1)
				gmpSpecularColor1 = unpackColor(curGmp->specular1);
			else
				gmpSpecularColor1 = glm::vec4(0);
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
			DEBUG_LOG(PVR, "  Parallel light %d: [%x] routing %d dmode %d col %d %d %d dir %f %f %f", light->lightId, plight->pcw.full,
					light->routing, light->dmode,
					light->red, light->green, light->blue,
					light->getDirX(), light->getDirY(), light->getDirZ());
		}
		else
		{
			DEBUG_LOG(PVR, "  Point light %d: [%x] routing %d dmode %d smode %d col %d %d %d dir %f %f %f pos %f %f %f dist %f %f angle %f %f",
					plight->lightId, plight->pcw.full, plight->routing, plight->dmode, plight->smode,
					plight->red, plight->green, plight->blue,
					plight->getDirX(), plight->getDirY(), plight->getDirZ(),
					plight->posX, plight->posY, plight->posZ,
					plight->distA(), plight->distB(),
					plight->angleA(), plight->angleB());
		}
		elan::curLights[lightId] = plight;
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
		ser << ta_get_list_type();
		ser << gmp;
		ser << instance;
		ser << projMatrix;
		ser << ta_get_tileclip();
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
		ta_parse_reset();
		u32 listType;
		deser >> listType;
		ta_set_list_type(listType);
		deser >> gmp;
		deser >> instance;
		deser >> projMatrix;
		u32 tileclip;
		deser >> tileclip;
		ta_set_tileclip(tileclip);
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
		vd.u1 = state.envMapUOffset;
		vd.v1 = state.envMapVOffset;
	}
	else
	{
		vd.u = vs.uv.u;
		vd.v = vs.uv.v;
		vd.u1 = vs.uv.u;
		vd.v1 = vs.uv.v;
	}
}

static void SetEnvMapUV(Vertex& vtx)
{
	if (envMapping)
	{
		vtx.u = state.envMapUOffset;
		vtx.v = state.envMapVOffset;
		vtx.u1 = state.envMapUOffset;
		vtx.v1 = state.envMapVOffset;
	}
}

template<typename T>
static glm::vec3 getNormal(const T& vtx)
{
	return { (int8_t)vtx.header.nx / 127.f, (int8_t)vtx.header.ny / 127.f, (int8_t)vtx.header.nz / 127.f };
}

template<typename T>
static void setNormal(Vertex& vd, const T& vs)
{
	glm::vec3 normal = getNormal(vs);
	vd.nx = normal.x;
	vd.ny = normal.y;
	vd.nz = normal.z;
}

static void setModelColors(glm::vec4& baseCol0, glm::vec4& offsetCol0, glm::vec4& baseCol1, glm::vec4& offsetCol1)
{
	if (curGmp == nullptr)
		return;
	if (curGmp->paramSelect.d0)
		baseCol0 = gmpDiffuseColor0;
	if (curGmp->paramSelect.s0)
		offsetCol0 = gmpSpecularColor0;
	if (curGmp->paramSelect.d1)
		baseCol1 = gmpDiffuseColor1;
	if (curGmp->paramSelect.s1)
		offsetCol1 = gmpSpecularColor1;
}

template <typename T>
static void convertVertex(const T& vs, Vertex& vd);

template<>
void convertVertex(const N2_VERTEX& vs, Vertex& vd)
{
	setCoords(vd, vs.x, vs.y, vs.z);
	setNormal(vd, vs);
	SetEnvMapUV(vd);
	glm::vec4 baseCol0(1);
	glm::vec4 offsetCol0(0);
	glm::vec4 baseCol1(1);
	glm::vec4 offsetCol1(0);
	setModelColors(baseCol0, offsetCol0, baseCol1, offsetCol1);

	*(u32 *)vd.col = packColor(baseCol0);
	*(u32 *)vd.spc = packColor(offsetCol0);
	*(u32 *)vd.col1 = packColor(baseCol1);
	*(u32 *)vd.spc1 = packColor(offsetCol1);
}

template<>
void convertVertex(const N2_VERTEX_VR& vs, Vertex& vd)
{
	setCoords(vd, vs.x, vs.y, vs.z);
	setNormal(vd, vs);
	SetEnvMapUV(vd);
	glm::vec4 baseCol0 = unpackColor(vs.rgb.argb0);
	glm::vec4 offsetCol0(0);
	glm::vec4 baseCol1 = unpackColor(vs.rgb.argb1);
	glm::vec4 offsetCol1(0);
	setModelColors(baseCol0, offsetCol0, baseCol1, offsetCol1);
	*(u32 *)vd.col = packColor(baseCol0);
	*(u32 *)vd.spc = packColor(offsetCol0);
	*(u32 *)vd.col1 = packColor(baseCol1);
	*(u32 *)vd.spc1 = packColor(offsetCol1);
}

template<>
void convertVertex(const N2_VERTEX_VU& vs, Vertex& vd)
{
	setCoords(vd, vs.x, vs.y, vs.z);
	setNormal(vd, vs);
	setUV(vs, vd);
	glm::vec4 baseCol0(1);
	glm::vec4 offsetCol0(0);
	glm::vec4 baseCol1(1);
	glm::vec4 offsetCol1(0);
	setModelColors(baseCol0, offsetCol0, baseCol1, offsetCol1);
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
	glm::vec4 offsetCol0(0);
	glm::vec4 baseCol1 = unpackColor(vs.rgb.argb1);
	glm::vec4 offsetCol1(0);
	setModelColors(baseCol0, offsetCol0, baseCol1, offsetCol1);
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
	glm::vec4 baseCol0(1);
	glm::vec4 offsetCol0(0);
	glm::vec4 baseCol1(1);
	glm::vec4 offsetCol1(0);
	setModelColors(baseCol0, offsetCol0, baseCol1, offsetCol1);
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
static bool isBetweenNearAndFar(const T* vertices, u32 count, bool& needNearClipping)
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

	needNearClipping = max.z > -nearPlane;

	return true;
}

class TriangleStripClipper
{
public:
	TriangleStripClipper(bool enabled) : enabled(enabled) {}

	void add(const Vertex& vtx)
	{
		if (enabled)
		{
			float z = vtx.x * curMatrix[0][2] + vtx.y * curMatrix[1][2] + vtx.z * curMatrix[2][2] + curMatrix[3][2];
			float dist = -z - nearPlane;
			clip(vtx, dist);
			count++;
		}
		else
		{
			ta_add_vertex(vtx);
		}
	}

private:
	void sendVertex(const Vertex& r)
	{
		if (dupeNext)
			ta_add_vertex(r);
		dupeNext = false;
		ta_add_vertex(r);
	}

	// Three-Dimensional Homogeneous Clipping of Triangle Strips
	// Patrick-Gilles Maillot. Graphics Gems II - 1991
	void clip(const Vertex& r, float rDist)
	{
		clipCode >>= 1;
		clipCode |= (int)(rDist < 0) << 2;
		if (count == 1)
		{
			switch (clipCode >> 1) {
			case 0: // Q and R inside
				sendVertex(q);
				sendVertex(r);
				break;
			case 1: // Q outside, R inside
				sendVertex(interpolate(q, qDist, r, rDist));
				sendVertex(r);
				break;
			case 2: // Q inside, R outside
				sendVertex(q);
				sendVertex(interpolate(q, qDist, r, rDist));
				break;
			case 3: // Q and R outside
				break;
			}
		}
		else if (count >= 2)
		{
			switch (clipCode)
			{
			case 0: // all inside
				sendVertex(r);
				break;
			case 1: // P outside, Q and R inside
				sendVertex(interpolate(r, rDist, p, pDist));
				sendVertex(q);
				sendVertex(r);
				break;
			case 2: // P inside, Q outside and R inside
				sendVertex(r);
				sendVertex(interpolate(q, qDist, r, rDist));
				sendVertex(r);
				break;
			case 3: // P and Q outside, R inside
				{
					Vertex tmp = interpolate(r, rDist, p, pDist);
					sendVertex(tmp);
					sendVertex(tmp);
					sendVertex(tmp); // One more to preserve strip swap order
					sendVertex(interpolate(q, qDist, r, rDist));
					sendVertex(r);
				}
				break;
			case 4: // P and Q inside, R outside
				sendVertex(interpolate(r, rDist, p, pDist));
				sendVertex(q);
				sendVertex(interpolate(q, qDist, r, rDist));
				break;
			case 5: // P outside, Q inside, R outside
				sendVertex(interpolate(q, qDist, r, rDist));
				break;
			case 6: // P inside, Q and R outside
				{
					Vertex tmp = interpolate(r, rDist, p, pDist);
					sendVertex(tmp);
					sendVertex(tmp);
					sendVertex(tmp); // One more to preserve strip swap order
				}
				break;
			case 7: // P, Q and R outside
				dupeNext = !dupeNext;
				break;
			}
		}
		p = q;
		pDist = qDist;
		q = r;
		qDist = rDist;
	}

	Vertex interpolate(const Vertex& v1, float f1, const Vertex& v2, float f2)
	{
		Vertex v;
		float a2 = std::abs(f1) / (std::abs(f1) + std::abs(f2));
		float a1 = 1 - a2;
		v.x = v1.x * a1 + v2.x * a2;
		v.y = v1.y * a1 + v2.y * a2;
		v.z = v1.z * a1 + v2.z * a2;

		v.u = v1.u * a1 + v2.u * a2;
		v.v = v1.v * a1 + v2.v * a2;
		v.u1 = v1.u1 * a1 + v2.u1 * a2;
		v.v1 = v1.v1 * a1 + v2.v1 * a2;

		for (size_t i = 0; i < ARRAY_SIZE(v1.col); i++)
		{
			v.col[i] = (u8)std::round(v1.col[i] * a1 + v2.col[i] * a2);
			v.spc[i] = (u8)std::round(v1.spc[i] * a1 + v2.spc[i] * a2);
			v.col1[i] = (u8)std::round(v1.col1[i] * a1 + v2.col1[i] * a2);
			v.spc1[i] = (u8)std::round(v1.spc1[i] * a1 + v2.spc1[i] * a2);
		}
		v.nx = v1.nx * a1 + v2.nx * a2;
		v.ny = v1.ny * a1 + v2.ny * a2;
		v.nz = v1.nz * a1 + v2.nz * a2;

		return v;
	}

	bool enabled;
	int count = 0;
	int clipCode = 0;
	Vertex p;
	float pDist = 0;
	Vertex q;
	float qDist = 0;
	bool dupeNext = false;
};

template <typename T>
static void sendVertices(const ICHList *list, const T* vtx, bool needClipping)
{
	Vertex taVtx;
	verify(list->vertexSize() > 0);

	Vertex fanCenterVtx{};
	Vertex fanLastVtx{};
	bool stripStart = true;
	int outStripIndex = 0;
	TriangleStripClipper clipper(needClipping);

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
				clipper.add(fanLastVtx);
				clipper.add(taVtx);
				outStripIndex += 2;
				if (outStripIndex & 1)
				{
					clipper.add(taVtx);
					outStripIndex++;
				}
			}
			stripStart = false;
		}
		else if (vtx->header.isFan())
		{
			// use degenerate triangles to link strips
			clipper.add(fanLastVtx);
			clipper.add(fanCenterVtx);
			outStripIndex += 2;
			if (outStripIndex & 1)
			{
				clipper.add(fanCenterVtx);
				outStripIndex++;
			}
			// Triangle fan
			clipper.add(fanCenterVtx);
			clipper.add(fanLastVtx);
			outStripIndex += 2;
		}
		clipper.add(taVtx);
		outStripIndex++;
		fanLastVtx = taVtx;
		if (vtx->header.endOfStrip)
			stripStart = true;

		vtx++;
	}
}

class ModifierVolumeClipper
{
public:
	ModifierVolumeClipper(bool enabled) : enabled(enabled) {}

	void add(ModTriangle& tri)
	{
		if (enabled)
		{
			glm::vec3 dist{
				tri.x0 * curMatrix[0][2] + tri.y0 * curMatrix[1][2] + tri.z0 * curMatrix[2][2] + curMatrix[3][2],
				tri.x1 * curMatrix[0][2] + tri.y1 * curMatrix[1][2] + tri.z1 * curMatrix[2][2] + curMatrix[3][2],
				tri.x2 * curMatrix[0][2] + tri.y2 * curMatrix[1][2] + tri.z2 * curMatrix[2][2] + curMatrix[3][2]
			};
			dist = -dist - nearPlane;
			ModTriangle newTri;
			int n = sutherlandHodgmanClip(dist, tri, newTri);
			switch (n)
			{
			case 0:
				// fully clipped
				break;
			case 3:
				ta_add_triangle(tri);
				break;
			case 4:
				ta_add_triangle(tri);
				ta_add_triangle(newTri);
				break;
			}
		}
		else
		{
			ta_add_triangle(tri);
		}
	}

private:
	//
	// Efficient Triangle and Quadrilateral Clipping within Shaders. M. McGuire
	// Journal of Graphics GPU and Game Tools - November 2011
	//
	glm::vec3 intersect(const glm::vec3& A, float Adist , const glm::vec3& B, float Bdist)
	{
		return (A * std::abs(Bdist) + B * std::abs(Adist)) / (std::abs(Adist) + std::abs(Bdist));
	}

	// Clip the triangle 'trig' with respect to the provided distances to the clipping plane.
	int sutherlandHodgmanClip(glm::vec3& dist, ModTriangle& trig, ModTriangle& newTrig)
	{
		constexpr float clipEpsilon = 0.f; //0.00001;
		constexpr float clipEpsilon2 = 0.f; //0.01;

		if (!glm::any(glm::greaterThanEqual(dist , glm::vec3(clipEpsilon2))))
			// all clipped
			return 0;
		if (glm::all(glm::greaterThanEqual(dist , glm::vec3(-clipEpsilon))))
			// none clipped
			return 3;

		// There are either 1 or 2 vertices above the clipping plane.
		glm::bvec3 above = glm::greaterThanEqual(dist, glm::vec3(0.f));
		bool nextIsAbove;
		glm::vec3 v0(trig.x0, trig.y0, trig.z0);
		glm::vec3 v1(trig.x1, trig.y1, trig.z1);
		glm::vec3 v2(trig.x2, trig.y2, trig.z2);
		glm::vec3 v3;
		// Find the CCW-most vertex above the plane.
		if (above[1] && !above[0])
		{
			// Cycle once CCW. Use v3 as a temp
			nextIsAbove = above[2];
			v3 = v0;
			v0 = v1;
			v1 = v2;
			v2 = v3;
			dist = glm::vec3(dist.y, dist.z, dist.x);
		}
		else if (above[2] && !above[1])
		{
			// Cycle once CW. Use v3 as a temp.
			nextIsAbove = above[0];
			v3 = v2;
			v2 = v1;
			v1 = v0;
			v0 = v3;
			dist = glm::vec3(dist.z, dist.x, dist.y);
		}
		else
			nextIsAbove = above[1];
		trig.x0 = v0.x;
		trig.y0 = v0.y;
		trig.z0 = v0.z;
		// We always need to clip v2-v0.
		v3 = intersect(v0, dist[0], v2, dist[2]);
		if (nextIsAbove)
		{
			v2 = intersect(v1, dist[1], v2, dist[2]);
			trig.x1 = v1.x;
			trig.y1 = v1.y;
			trig.z1 = v1.z;
			trig.x2 = v2.x;
			trig.y2 = v2.y;
			trig.z2 = v2.z;
			newTrig.x0 = v0.x;
			newTrig.y0 = v0.y;
			newTrig.z0 = v0.z;
			newTrig.x1 = v2.x;
			newTrig.y1 = v2.y;
			newTrig.z1 = v2.z;
			newTrig.x2 = v3.x;
			newTrig.y2 = v3.y;
			newTrig.z2 = v3.z;

			return 4;
		}
		else
		{
			v1 = intersect(v0, dist[0], v1, dist[1]);
			trig.x1 = v1.x;
			trig.y1 = v1.y;
			trig.z1 = v1.z;
			trig.x2 = v3.x;
			trig.y2 = v3.y;
			trig.z2 = v3.z;

			return 3;
		}
	}

	bool enabled;
};

template <typename T>
static void sendMVPolygon(ICHList *list, const T *vtx, bool needClipping)
{
	ModifierVolumeParam mvp{};
	mvp.isp.full = list->isp.full;
	if (!openModifierVolume)
		mvp.isp.CullMode = 0;
	mvp.isp.VolumeLast = list->pcw.volume;
	mvp.isp.DepthMode &= 3;
	mvp.mvMatrix = taMVMatrix;
	mvp.projMatrix = taProjMatrix;
	ta_add_poly(list->pcw.listType, mvp);

	ModifierVolumeClipper clipper(needClipping);
	glm::vec3 vtx0{};
	glm::vec3 vtx1{};
	u32 stripStart = 0;

	for (u32 i = 0; i < list->vtxCount; i++)
	{
		glm::vec3 v(vtx->x, vtx->y, vtx->z);
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

			clipper.add(tri);
		}
		if (vtx->header.endOfStrip)
			stripStart = i + 1;
		vtx0 = vtx1;
		vtx1 = v;
		vtx++;
	}
}

static N2LightModel *taLightModel;

static void sendLights()
{
	if (!state.lightModelUpdated)
		return;

	state.lightModelUpdated = false;
	N2LightModel model;
	model.lightCount = 0;
	if (curLightModel == nullptr)
	{
		model.useBaseOver = 0;
		for (int i = 0; i < 2; i++)
		{
			model.ambientMaterialBase[i] = 0;
			model.ambientMaterialOffset[i] = 0;
			model.ambientBase[i][0] = model.ambientBase[i][1] = model.ambientBase[i][2] = model.ambientBase[i][3] = 1.f;
		}
		memset(model.ambientOffset, 0, sizeof(model.ambientOffset));
		taLightModel = nullptr;
		return;
	}
	model.ambientMaterialBase[0] = curLightModel->useAmbientBase0;
	model.ambientMaterialBase[1] = curLightModel->useAmbientBase1;
	model.ambientMaterialOffset[0] = curLightModel->useAmbientOffset0;
	model.ambientMaterialOffset[1] = curLightModel->useAmbientOffset1;
	model.useBaseOver = curLightModel->useBaseOver;
	model.bumpId1 = -1;
	model.bumpId2 = -1;
	memcpy(model.ambientBase[0], glm::value_ptr(unpackColor(curLightModel->ambientBase0)), sizeof(model.ambientBase[0]));
	memcpy(model.ambientBase[1], glm::value_ptr(unpackColor(curLightModel->ambientBase1)), sizeof(model.ambientBase[1]));
	memcpy(model.ambientOffset[0], glm::value_ptr(unpackColor(curLightModel->ambientOffset0)), sizeof(model.ambientOffset[0]));
	memcpy(model.ambientOffset[1], glm::value_ptr(unpackColor(curLightModel->ambientOffset1)), sizeof(model.ambientOffset[1]));

	for (u32 i = 0; i < MAX_LIGHTS; i++)
	{
		N2Light& light = model.lights[model.lightCount];
		for (int vol = 0; vol < 2; vol++)
		{
			light.diffuse[vol] = curLightModel->isDiffuse(i, vol);
			light.specular[vol] = curLightModel->isSpecular(i, vol);
		}
		if (!light.diffuse[0] && !light.specular[0]
				&& !light.diffuse[1] && !light.specular[1])
			continue;
		if (curLights[i] == nullptr)
		{
			INFO_LOG(PVR, "Light %d is referenced but undefined", i);
			continue;
		}
		if (i == curLightModel->bumpId1)
			model.bumpId1 = model.lightCount;
		if (i == curLightModel->bumpId2)
			model.bumpId2 = model.lightCount;
		light.parallel = curLights[i]->pcw.parallelLight;
		if (light.parallel)
		{
			ParallelLight *plight = (ParallelLight *)curLights[i];
			memcpy(light.color, glm::value_ptr(unpackColor(plight->red, plight->green, plight->blue)), sizeof(light.color));
			light.routing = plight->routing;
			light.dmode = plight->dmode;
			light.smode = N2_LMETHOD_SINGLE_SIDED;
			memcpy(light.direction, glm::value_ptr(-glm::vec4(plight->getDirX(), plight->getDirY(), plight->getDirZ(), 0)),
					sizeof(light.direction));
		}
		else
		{
			PointLight *plight = (PointLight *)curLights[i];
			memcpy(light.color, glm::value_ptr(unpackColor(plight->red, plight->green, plight->blue)), sizeof(light.color));
			light.routing = plight->routing;
			light.dmode = plight->dmode;
			light.smode = plight->smode;
			if (plight->posX == 0 && plight->posY == 0 && plight->posZ == 0
					&& plight->_distA == 0 && plight->_distB == 0
					&& plight->_angleA == 0 && plight->_angleB == 0)
			{
				// Lights not using distance or angle attenuation are converted into parallel lights on the CPU side?
				DEBUG_LOG(PVR, "Point -> parallel light[%d] dir %d %d %d", i, -(int8_t)plight->dirX, -(int8_t)plight->dirY, -(int8_t)plight->dirZ);
				light.parallel = true;
				memcpy(light.direction, glm::value_ptr(-glm::vec4(plight->getDirX(), plight->getDirY(), plight->getDirZ(), 0)),
						sizeof(light.direction));
			}
			else
			{
				memcpy(light.direction, glm::value_ptr(-glm::vec4(plight->getDirX(), plight->getDirY(), plight->getDirZ(), 0)),
						sizeof(light.direction));
				memcpy(light.position, glm::value_ptr(glm::vec4(plight->posX, plight->posY, plight->posZ, 1)), sizeof(light.position));
				light.distAttnMode = plight->dattenmode;
				light.attnDistA = plight->distA();
				light.attnDistB = plight->distB();
				light.attnAngleA = plight->angleA();
				light.attnAngleB = plight->angleB();
			}
		}
		model.lightCount++;
	}
	taLightModel = ta_add_light(model);
}

static void setStateParams(PolyParam& pp, const ICHList *list)
{
	sendLights();
	pp.mvMatrix = taMVMatrix;
	pp.normalMatrix = taNormalMatrix;
	pp.projMatrix = taProjMatrix;
	pp.lightModel = taLightModel;
	pp.envMapping[0] = false;
	pp.envMapping[1] = false;
	if (curGmp != nullptr)
	{
		pp.glossCoef[0] = curGmp->gloss.getCoef0();
		pp.glossCoef[1] = curGmp->gloss.getCoef1();
		pp.constantColor[0] = curGmp->paramSelect.b0;
		pp.constantColor[1] = curGmp->paramSelect.b1;

		// Environment mapping
		if (curGmp->paramSelect.e0)
		{
			pp.pcw.Texture = 1;
			pp.pcw.Offset = 0;
			pp.tsp.UseAlpha = 1;
			pp.tsp.IgnoreTexA = 0;
			pp.envMapping[0] = true;
			pp.tcw = list->tcw0;
			envMapping = true;
		}
		if (curGmp->paramSelect.e1)
		{
			pp.pcw.Texture = 1;
			pp.pcw.Offset = 0;
			pp.tsp1.UseAlpha = 1;
			pp.tsp1.IgnoreTexA = 0;
			pp.envMapping[1] = true;
			pp.tcw1 = list->tcw1;
			envMapping = true;
		}
	}
	pp.tsp.full ^= modelTSP.full;
	pp.tsp1.full ^= modelTSP.full;

	// projFlip is for left-handed projection matrices (initd rear view mirror)
	bool projFlip = taProjMatrix != nullptr && std::signbit(taProjMatrix[0]) == std::signbit(taProjMatrix[5]);
	pp.isp.CullMode ^= (u32)cullingReversed ^ (u32)projFlip;
	pp.pcw.Shadow ^= shadowedVolume;
	if (pp.pcw.Shadow == 0 || pp.pcw.Volume == 0)
	{
		pp.tsp1.full = -1;
		pp.tcw1.full = -1;
		pp.glossCoef[1] = 0;
		pp.constantColor[1] = false;
	}
//	else if (pp.pcw.Volume == 1)
//		printf("2-Volume poly listType %d vtxtype %x gmp params %x diff tcw %08x tsp %08x\n", ta_get_list_type(), list->flags, curGmp->paramSelect.full,
//				pp.tcw.full ^ pp.tcw1.full, pp.tsp.full ^ pp.tsp1.full);
}

static void sendPolygon(ICHList *list)
{
	bool needClipping;

	switch (list->flags)
	{
	case ICHList::VTX_TYPE_V:
		{
			N2_VERTEX *vtx = (N2_VERTEX *)((u8 *)list + sizeof(ICHList));
			if (!isBetweenNearAndFar(vtx, list->vtxCount, needClipping))
				break;
			int listType = ta_get_list_type();
			if (listType == -1)
				listType = list->pcw.listType;
			if (listType & 1)
				sendMVPolygon(list, vtx, needClipping);
			else
			{
				PolyParam pp{};
				pp.pcw.Shadow = list->pcw.shadow;
				pp.pcw.Texture = 0;
				pp.pcw.Offset = list->pcw.offset;
				pp.pcw.Gouraud = list->pcw.gouraud;
				pp.pcw.Volume = list->pcw.volume;
				pp.isp = list->isp;
				pp.tsp = list->tsp0;
				pp.tsp1 = list->tsp1;
				setStateParams(pp, list);
				ta_add_poly(pp);

				sendVertices(list, vtx, needClipping);
			}
		}
		break;

	case ICHList::VTX_TYPE_VU:
		{
			N2_VERTEX_VU *vtx = (N2_VERTEX_VU *)((u8 *)list + sizeof(ICHList));
			if (!isBetweenNearAndFar(vtx, list->vtxCount, needClipping))
				break;
			int listType = ta_get_list_type();
			if (listType == -1)
				listType = list->pcw.listType;
			if (listType  & 1)
				sendMVPolygon(list, vtx, needClipping);
			else
			{
				PolyParam pp{};
				pp.pcw.Shadow = list->pcw.shadow;
				pp.pcw.Texture = list->pcw.texture;
				pp.pcw.Offset = list->pcw.offset;
				pp.pcw.Gouraud = list->pcw.gouraud;
				pp.pcw.Volume = list->pcw.volume;
				pp.isp = list->isp;
				pp.tsp = list->tsp0;
				pp.tcw = list->tcw0;
				pp.tsp1 = list->tsp1;
				pp.tcw1 = list->tcw1;
				setStateParams(pp, list);
				ta_add_poly(pp);

				sendVertices(list, vtx, needClipping);
			}
		}
		break;

	case ICHList::VTX_TYPE_VUR:
		{
			N2_VERTEX_VUR *vtx = (N2_VERTEX_VUR *)((u8 *)list + sizeof(ICHList));
			if (!isBetweenNearAndFar(vtx, list->vtxCount, needClipping))
				break;
			PolyParam pp{};
			pp.pcw.Shadow = list->pcw.shadow;
			pp.pcw.Texture = list->pcw.texture;
			pp.pcw.Offset = list->pcw.offset;
			pp.pcw.Gouraud = list->pcw.gouraud;
			pp.pcw.Volume = list->pcw.volume;
			pp.isp = list->isp;
			pp.tsp = list->tsp0;
			pp.tcw = list->tcw0;
			pp.tsp1 = list->tsp1;
			pp.tcw1 = list->tcw1;
			setStateParams(pp, list);
			ta_add_poly(pp);

			sendVertices(list, vtx, needClipping);
		}
		break;

	case ICHList::VTX_TYPE_VR:
		{
			N2_VERTEX_VR *vtx = (N2_VERTEX_VR *)((u8 *)list + sizeof(ICHList));
			if (!isBetweenNearAndFar(vtx, list->vtxCount, needClipping))
				break;
			PolyParam pp{};
			pp.pcw.Shadow = list->pcw.shadow;
			pp.pcw.Texture = 0;
			pp.pcw.Offset = list->pcw.offset;
			pp.pcw.Gouraud = list->pcw.gouraud;
			pp.pcw.Volume = list->pcw.volume;
			pp.isp = list->isp;
			pp.tsp = list->tsp0;
			pp.tsp1 = list->tsp1;
			setStateParams(pp, list);
			ta_add_poly(pp);

			sendVertices(list, vtx, needClipping);
		}
		break;

	case ICHList::VTX_TYPE_VUB:
		{
			// TODO
			//printf("BUMP MAP fmt %d filter %d src select %d dst %d\n", list->tcw0.PixelFmt, list->tsp0.FilterMode, list->tsp0.SrcSelect, list->tsp0.DstSelect);
			N2_VERTEX_VUB *vtx = (N2_VERTEX_VUB *)((u8 *)list + sizeof(ICHList));
			if (!isBetweenNearAndFar(vtx, list->vtxCount, needClipping))
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
			setStateParams(pp, list);
			ta_add_poly(pp);

			sendVertices(list, vtx, needClipping);
		}
		break;

	default:
		WARN_LOG(PVR, "Unhandled poly format %x", list->flags);
		// initdv2 crash (area conquered screen after the 4th race)
		//die("Unsupported");
		break;
	}
	envMapping = false;
}

template<bool Active = true>
static void executeCommand(u8 *data, int size)
{
//	verify(size >= 0);
//	verify(size < (int)ELAN_RAM_SIZE);
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
				if (Active)
					state.setProjectionMatrix(data);
				size -= sizeof(ProjMatrix);
				break;

			case PCW::matrixOrLight:
				{
					InstanceMatrix *instance = (InstanceMatrix *)data;
					if (instance->isInstanceMatrix())
					{
						//DEBUG_LOG(PVR, "Model instance");
						if (Active)
							state.setMatrix(instance);
						size -= sizeof(InstanceMatrix);
						break;
					}
					if (Active)
					{
						if (instance->id1 & 0x10)
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
					}
					size -= sizeof(LightModel);
				}
				break;

			case PCW::model:
				{
					Model *model = (Model *)data;
					if (Active)
					{
						cullingReversed = model->param.cwCulling == 0;
						ta_set_tileclip((model->pcw.userClip << 28) | (ta_get_tileclip() & 0x0fffffff));
						openModifierVolume = model->param.openVolume;
						shadowedVolume = model->pcw.shadow;
						modelTSP = model->tsp;
						DEBUG_LOG(PVR, "Model offset %x size %x pcw %08x tsp %08x", model->offset, model->size, model->pcw.full, model->tsp.full);
					}
					executeCommand<Active>(&RAM[model->offset & 0x1ffffff8], model->size);
					cullingReversed = false;
					openModifierVolume = false;
					shadowedVolume = false;
					modelTSP.full = 0;
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
							// initdv2j: happens at end of race, garbage data after end of model due to wrong size?
							//die("unexpected");
							inter = (HollyInterruptID)-1;
							break;
						}
						if (inter != (HollyInterruptID)-1)
						{
							asic_RaiseInterruptBothCLX(inter);
							TA_ITP_CURRENT += 32;
							if (Active)
								state.reset();
						}
					}
					size -= sizeof(RegisterWait);
				}
				break;

			case PCW::link:
				{
					Link *link = (Link *)data;
					if (link->offset & 0x80000000)
					{
						// elan v10 only
						if (link->size > VRAM_SIZE)
						{
							WARN_LOG(PVR, "Texture DMA from %x to %x (%x invalid)", DMAC_SAR(2), link->vramAddress & 0x1ffffff8, link->size);
							size = 0;
							break;
						}
						DEBUG_LOG(PVR, "Texture DMA from %x to %x (%x)", DMAC_SAR(2), link->vramAddress & 0x1ffffff8, link->size);
						memcpy(&vram[link->vramAddress & VRAM_MASK], &mem_b[DMAC_SAR(2) & RAM_MASK], link->size);
						reg74 |= 1;
					}
					else if (link->offset & 0x20000000)
					{
						// elan v10 only
						if (link->size > VRAM_SIZE)
						{
							WARN_LOG(PVR, "Texture DMA from eram %x -> %x (%x invalid)", link->offset & ELAN_RAM_MASK, link->vramAddress & VRAM_MASK, link->size);
							size = 0;
							break;
						}
						DEBUG_LOG(PVR, "Texture DMA from eram %x -> %x (%x)", link->offset & ELAN_RAM_MASK, link->vramAddress & VRAM_MASK, link->size);
						memcpy(&vram[link->vramAddress & VRAM_MASK], &RAM[link->offset & ELAN_RAM_MASK], link->size);
						reg74 |= 1;
					}
					else
					{
						DEBUG_LOG(PVR, "Link to %8x (%x)", link->offset, link->size);
						executeCommand<Active>(&RAM[link->offset & ELAN_RAM_MASK], link->size);
					}
					size -= sizeof(Link);
				}
				break;

			case PCW::gmp:
				if (Active)
					state.setGMP(data);
				size -= sizeof(GMP);
				break;

			case PCW::ich:
				{
					ICHList *ich = (ICHList *)data;
					if (Active)
					{
						DEBUG_LOG(PVR, "ICH flags %x, %d verts", ich->flags, ich->vtxCount);
						sendPolygon(ich);
					}
					size -= sizeof(ICHList) + ich->vertexSize() * ich->vtxCount;
				}
				break;

			default:
				WARN_LOG(PVR, "Unhandled Elan command %x", cmd->pcw.n2Command);
				size -= 32;
				break;
			}
		}
		else
		{
			if (Active)
			{
				u32 pcw = *(u32 *)data;
				DEBUG_LOG(PVR, "Geometry type %d - %08x", (pcw >> 24) & 0xf, pcw);
				try {
					size -= ta_add_ta_data((u32 *)data, size);
				} catch (const TAParserException& e) {
					size = 0;
				}
			}
			else
			{
				u32 vertexSize = 32;
				int listType = ta_get_list_type();
				int i = 0;
				while (i < size)
				{
					PCW pcw = *(PCW *)&data[i];
					if (pcw.naomi2 == 1)
						break;
					switch (pcw.paraType)
					{
					case ParamType_End_Of_List:
						listType = -1;
						i += 32;
						break;
					case ParamType_Object_List_Set:
					case ParamType_User_Tile_Clip:
						i += 32;
						break;
					case ParamType_Polygon_or_Modifier_Volume:
						{
							static const u32 * const PolyTypeLut = TaTypeLut::instance().table;

							if (listType == -1)
								listType = pcw.listType;
							if (listType & 1)
							{
								// modifier volumes
								vertexSize = 64;
								i += 32;
							}
							else
							{
								u32 polyId = PolyTypeLut[pcw.objectControl];
								u32 polySize = polyId >> 30;
								u32 vertexType = (u8)polyId;
								if (vertexType == 5 || vertexType == 6 || (vertexType >= 11 && vertexType <= 14))
									vertexSize = 64;
								else
									vertexSize = 32;
								i += polySize == SZ64 ? 64 : 32;
							}
						}
						break;
					case ParamType_Sprite:
						if (listType == -1)
							listType = pcw.listType;
						vertexSize = 64;
						i += 32;
						break;
					case ParamType_Vertex_Parameter:
						i += vertexSize;
						break;
					default:
						WARN_LOG(PVR, "Invalid param type %d", pcw.paraType);
						i = size;
						break;
					}
				}
				size -= i;
			}
		}
		data += oldSize - size;
	}
}

static void DYNACALL write_elancmd(u32 addr, u32 data)
{
//	DEBUG_LOG(PVR, "ELAN cmd %08x = %x", addr, data);
	addr = (addr & (sizeof(elanCmd) - 1)) / sizeof(u32);
	elanCmd[addr] = data;

	if (addr == 7)
	{
		if (!ggpo::rollbacking())
			executeCommand<true>((u8 *)elanCmd, sizeof(elanCmd));
		else
			executeCommand<false>((u8 *)elanCmd, sizeof(elanCmd));
		if (!(reg74 & 1))
			reg74 |= 2;
	}
}

template<typename T>
static T DYNACALL read_elanram(u32 addr)
{
	return *(T *)&RAM[addr & ELAN_RAM_MASK];
}

template<typename T>
static void DYNACALL write_elanram(u32 addr, T data)
{
	*(T *)&RAM[addr & ELAN_RAM_MASK] = data;
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
	elanRegHandler = _vmem_register_handler(nullptr, nullptr, read_elanreg, nullptr, nullptr, write_elanreg);
	elanCmdHandler = _vmem_register_handler(nullptr, nullptr, nullptr, nullptr, nullptr, write_elancmd);
	elanRamHandler = _vmem_register_handler_Template(read_elanram, write_elanram);
}

void vmem_map(u32 base)
{
	_vmem_map_handler(elanRegHandler, base | 8, base | 8);
	_vmem_map_handler(elanCmdHandler, base | 9, base | 9);
	_vmem_map_handler(elanRamHandler, base | 0xA, base | 0xB);
	_vmem_map_block(RAM, base | 0xA, base | 0xB, ELAN_RAM_MASK);
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
