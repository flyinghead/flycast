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

namespace elan {

static _vmem_handler elanRegHandler;
static _vmem_handler elanCmdHandler;
static _vmem_handler elanRamHandler;

static u8 *elanRAM;
constexpr u32 ELAN_RAM_SIZE = 32 * 1024 * 1024;

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
static glm::mat4x4 lightMatrix;
static glm::mat4 projectionMatrix;
static LightModel *curLightModel;
static ElanBase *curLights[MAX_LIGHTS];
static float near = 0.001f;
static float far = 100000.f;

struct State
{
	static constexpr u32 Null = 0xffffffff;

	int listType = -1;
	u32 gmp = Null;
	u32 matrix = Null;
	u32 projMatrix = Null;
	int userClip = 0;
	u32 lightModel = Null;
	u32 lights[MAX_LIGHTS] = {
			Null, Null, Null, Null, Null, Null, Null, Null,
			Null, Null, Null, Null, Null, Null, Null, Null
	};

	void reset()
	{
		listType = -1;
		gmp = Null;
		matrix = Null;
		projMatrix = Null;
		userClip = 0;
		lightModel = Null;
		for (auto& light : lights)
			light = Null;
		update();
	}
	void setMatrix(void *p)
	{
		matrix = elanRamAddress(p);
		updateMatrix();
	}

	void updateMatrix()
	{
		if (matrix == Null)
			return;
		Matrix *mat = (Matrix *)&elanRAM[matrix];
		DEBUG_LOG(PVR, "Matrix %f %f %f %f\n       %f %f %f %f\n       %f %f %f %f\nLight: %f %f %f\n       %f %f %f",
				-mat->tm00, -mat->tm01, -mat->tm02, -mat->mat03,
				mat->tm10, mat->tm11, mat->tm12, mat->mat13,
				mat->tm20, mat->tm21, mat->tm22, -mat->mat23,
				mat->lm00, mat->lm01, mat->lm02,
				mat->lm10, mat->lm11, mat->lm12);
//					DEBUG_LOG(PVR, "Matrix proj4 %f %f %f %f %f",
//							mat->proj4, mat->proj5, mat->mproj6, mat->proj7, mat->proj8);
		curMatrix = glm::mat4x4{
			-mat->tm00, mat->tm10, mat->tm20, 0,
			-mat->tm01, mat->tm11, mat->tm21, 0,
			-mat->tm02, mat->tm12, mat->tm22, 0,
			-mat->mat03, mat->mat13, -mat->mat23, 1
		};
		lightMatrix = glm::mat4x4{
			-mat->lm00, mat->lm10, mat->tm20, 0,
			-mat->lm01, mat->lm11, mat->tm21, 0,
			-mat->lm02, mat->lm12, mat->tm22, 0,
			-mat->mat03, mat->mat13, -mat->mat23, 1
		};
		near = mat->proj4;
		far = mat->proj5;
	}

	void setProjectionMatrix(void *p)
	{
		projMatrix = elanRamAddress(p);
		updateProjectionMatrix();
	}

	void updateProjectionMatrix()
	{
		if (projMatrix == Null)
			return;
		ProjMatrix *pm = (ProjMatrix *)&elanRAM[projMatrix];
		DEBUG_LOG(PVR, "Proj matrix x: %f %f y: %f %f", pm->fx, pm->tx, pm->fy, pm->ty);
		projectionMatrix = glm::mat4(
				-pm->fx,  0,       0,  0,
				 0,       pm->fy,  0,  0,
				-pm->tx, -pm->ty, -1, -1,
				 0,       0,       0,  0
		);
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
			curGmp = (GMP *)&elanRAM[gmp];
			DEBUG_LOG(PVR, "GMP paramSelect %x clip %d", curGmp->paramSelect.full, curGmp->pcw.userClip);
		}
	}

	void setLightModel(void *p)
	{
		lightModel = elanRamAddress(p);
		updateLightModel();
	}

	void updateLightModel() {
		if (lightModel == Null)
			curLightModel = nullptr;
		else
		{
			curLightModel = (LightModel *)&elanRAM[lightModel];
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
		if (lights[lightId] == Null)
		{
			elan::curLights[lightId] = nullptr;
			return;
		}
		Instance *instance = (Instance *)&elanRAM[lights[lightId]];
		if (instance->pcw.parallelLight)
		{
			ParallelLight *light = (ParallelLight *)instance;
			DEBUG_LOG(PVR, "  Parallel light %d: col %d %d %d dir %d %d %d", light->lightId, light->red, light->green, light->blue,
					light->dirX, light->dirY, light->dirZ);
		}
		else
		{
			PointLight *light = (PointLight *)instance;
			DEBUG_LOG(PVR, "  Point light %d: dattenmode %d col %d %d %d dir %d %d %d pos %f %f %f routing %d dist %f %f angle %f %f",
					light->lightId, light->dattenmode,
					light->red, light->green, light->blue,
					light->dirX, light->dirY, light->dirZ,
					light->posX, light->posY, light->posZ,
					light->routing, light->attnMinDistance(), light->attnMaxDistance(),
					light->attnMinAngle(), light->attnMaxAngle());
		}
		elan::curLights[lightId] = instance;
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
		if ((u8 *)p < elanRAM || (u8 *)p >= elanRAM + ELAN_RAM_SIZE)
			return Null;
		else
			return (u32)((u8 *)p - elanRAM);
	}
};

static State state;

template <typename T>
static void setCoords(T& vtx, float x, float y, float z)
{
	glm::vec4 v(x, y, z, 1);
	v = projectionMatrix * curMatrix * v;
	v.x /= v.w;
	v.y /= v.w;
	vtx.xyz[0] = v.x;
	vtx.xyz[1] = v.y;
	vtx.xyz[2] = 1 / v.w;
}

template <typename Ts, typename Td>
static void setUV(const Ts& vs, Td& vd)
{
	vd.u = vs.uv.u;
	vd.v = vs.uv.v;
}

glm::vec4 unpackColor(u32 color)
{
	return glm::vec4((float)((color >> 16) & 0xff) / 255.f,
			(float)((color >> 8) & 0xff) / 255.f,
			(float)(color & 0xff) / 255.f,
			(float)(color >> 24) / 255.f);
}

glm::vec4 unpackColor(u8 red, u8 green, u8 blue, u8 alpha = 0)
{
	return glm::vec4((float)red / 255.f, (float)green / 255.f, (float)blue / 255.f, (float)alpha / 255.f);
}

u32 packColor(const glm::vec4& color)
{
	return (int)(std::max(0.f, std::min(1.f, color.a)) * 255.f) << 24
			| (int)(std::max(0.f, std::min(1.f, color.r)) * 255.f) << 16
			| (int)(std::max(0.f, std::min(1.f, color.g)) * 255.f) << 8
			| (int)(std::max(0.f, std::min(1.f, color.b)) * 255.f);
}

static void computeColors(glm::vec4& baseCol, glm::vec4& offsetCol, const glm::vec4& pos, const glm::vec4& normal)
{
	if (curLightModel == nullptr)
		return;
	glm::vec4 ambient{};
	glm::vec4 diffuse{};
	glm::vec4 specular{};
	float diffuseAlphaDiff = 0;
	float specularAlphaDiff = 0;

	for (u32 lightId = 0; lightId < MAX_LIGHTS; lightId++)
	{
		const ElanBase *base = curLights[lightId];
		if (base == nullptr)
			continue;
		if (!curLightModel->isDiffuse(lightId) && !curLightModel->isSpecular(lightId))
			continue;

		glm::vec4 lightDir; // direction to the light
		int routing;
		int dmode;
		int smode = N2_LMETHOD_SINGLE_SIDED;
		glm::vec4 lightColor;
		if (base->pcw.parallelLight)
		{
			ParallelLight *light = (ParallelLight *)base;
			lightDir = glm::normalize(glm::vec4((int8_t)light->dirX, -(int8_t)light->dirY, -(int8_t)light->dirZ, 0));
			lightColor = unpackColor(light->red, light->green, light->blue);
			routing = light->routing;
			dmode = light->dmode;
		}
		else
		{
			PointLight *light = (PointLight *)base;
			glm::vec4 lightPos(light->posX, light->posY, light->posZ, 1);
			lightDir = glm::normalize(lightPos - pos);
			lightColor = unpackColor(light->red, light->green, light->blue);
			routing = light->routing;

			if (light->isAttnDist())
			{
				float distance = glm::length(lightPos - pos);
				lightColor *= light->attnDist(distance);
			}
			if (light->isAttnAngle())
			{
				glm::vec4 spotDir = glm::normalize(glm::vec4((int8_t)light->dirX, (int8_t)light->dirY, (int8_t)light->dirZ, 0));
				float cosAngle = glm::max(0.f, glm::dot(-lightDir, spotDir));
				lightColor *= light->attnAngle(cosAngle);
			}
			dmode = light->dmode;
			smode = light->smode;
		}
		verify(dmode == N2_LMETHOD_SINGLE_SIDED || dmode == N2_LMETHOD_SPECIAL_EFFECT || dmode == N2_LMETHOD_DOUBLE_SIDED);
		verify(smode == N2_LMETHOD_SINGLE_SIDED || smode == N2_LMETHOD_SPECIAL_EFFECT || smode == N2_LMETHOD_DOUBLE_SIDED);
		if (!(routing == N2_LFUNC_BASEDIFF_BASESPEC_ADD || routing == N2_LFUNC_BASEDIFF_OFFSSPEC_ADD
				|| routing == N2_LFUNC_OFFSDIFF_BASESPEC_ADD || routing == N2_LFUNC_OFFSDIFF_OFFSSPEC_ADD
				|| routing == N2_LFUNC_ALPHADIFF_SUB))
			WARN_LOG(PVR, "routing = %d dmode %d smode %d lightCol %f %f %f %f", routing ,dmode, smode, lightColor.r, lightColor.g, lightColor.b, lightColor.a);
		if (curLightModel->isDiffuse(lightId) && curGmp->paramSelect.d0)
		{
			float factor;
			switch (dmode)
			{
			case N2_LMETHOD_SINGLE_SIDED:
				factor = glm::max(glm::dot(normal, lightDir), 0.f);
				break;
			case N2_LMETHOD_DOUBLE_SIDED:
				factor = glm::abs(glm::dot(normal, lightDir));
				break;
			case N2_LMETHOD_SPECIAL_EFFECT:
			default:
				factor = 1;
				break;
			}
			if (routing == N2_LFUNC_ALPHADIFF_SUB)
				// FIXME probably need to substract from baseCol.a/OffsetCol.a instead? still not working...
				//diffuse.a = std::max(0.f, diffuse.a - lightColor.r * factor);
				diffuseAlphaDiff -= lightColor.r * factor;
			else if (routing == N2_LFUNC_BASEDIFF_BASESPEC_ADD || routing == N2_LFUNC_BASEDIFF_OFFSSPEC_ADD)
				diffuse += lightColor * factor;
			if (routing == N2_LFUNC_OFFSDIFF_BASESPEC_ADD || routing == N2_LFUNC_OFFSDIFF_OFFSSPEC_ADD)
				specular += lightColor * factor;
		}
		if (curLightModel->isSpecular(lightId) && curGmp->paramSelect.s0)
		{
			glm::vec4 reflectDir = glm::reflect(-lightDir, normal);
			float factor;
			switch (smode)
			{
			case N2_LMETHOD_SINGLE_SIDED:
				factor = glm::pow(glm::max(glm::dot(glm::normalize(-pos), reflectDir), 0.f), curGmp->gloss.getCoef0());
				break;
			case N2_LMETHOD_DOUBLE_SIDED:
				factor = glm::pow(glm::abs(glm::dot(glm::normalize(-pos), reflectDir)), curGmp->gloss.getCoef0());
				break;
			case N2_LMETHOD_SPECIAL_EFFECT:
			default:
				factor = 1;
				break;
			}
			if (routing == N2_LFUNC_ALPHADIFF_SUB)
				//specular.a = std::max(0.f, specular.a - lightColor.r * factor);
				specularAlphaDiff -= lightColor.r * factor;
			else if (routing == N2_LFUNC_OFFSDIFF_OFFSSPEC_ADD || routing == N2_LFUNC_BASEDIFF_OFFSSPEC_ADD)
				specular += lightColor * factor;
			if (routing == N2_LFUNC_BASEDIFF_BASESPEC_ADD || routing == N2_LFUNC_OFFSDIFF_BASESPEC_ADD)
				diffuse += lightColor * factor;
		}
	}
	if (curGmp->paramSelect.a0) // ambient0 TODO check
	{
		if (curLightModel->useAmbientBase0)
			diffuse += unpackColor(curLightModel->ambientBase0);
		if (curLightModel->useAmbientOffset0)
			specular += unpackColor(curLightModel->ambientOffset0);
	}
	baseCol *= diffuse;
	offsetCol *= specular;
	if (curGmp->paramSelect.a0)
	{
		if (!curLightModel->useAmbientBase0)
			baseCol += unpackColor(curLightModel->ambientBase0);
		if (!curLightModel->useAmbientOffset0)
			offsetCol += unpackColor(curLightModel->ambientOffset0);
	}
	baseCol.a = std::max(0.f, baseCol.a + diffuseAlphaDiff);
	offsetCol.a = std::max(0.f, offsetCol.a + specularAlphaDiff);
	if (curLightModel->useBaseOver)
	{
		glm::vec4 overflow = glm::max(glm::vec4(0), baseCol - glm::vec4(1));
		offsetCol += overflow;
	}
}

template<typename T>
glm::vec4 getNormal(const T& vtx)
{
	return glm::normalize(lightMatrix * glm::vec4((int8_t)vtx.header.nx / 127.f, (int8_t)vtx.header.ny / 127.f, (int8_t)vtx.header.nz / 127.f, 0));
}

template<>
glm::vec4 getNormal(const N2_VERTEX_VNU& vtx)
{
	return glm::normalize(lightMatrix * glm::vec4(vtx.normal.nx, vtx.normal.ny, vtx.normal.nz, 0));
}

template <typename T>
static void convertVertex(const T& vs, TA_VertexParam& vd);

template<>
void convertVertex(const Vertex& vs, TA_VertexParam& vd)
{
	setCoords(vd.vtx0, vs.x, vs.y, vs.z);
	glm::vec4 baseCol;
	glm::vec4 offsetCol;
	if (curGmp != nullptr)
	{
		baseCol = unpackColor(curGmp->diffuse0);
		offsetCol = unpackColor(curGmp->specular0);
		computeColors(baseCol, offsetCol, curMatrix * glm::vec4(vs.x, vs.y, vs.z, 1), getNormal(vs));
	}
	else
	{
		baseCol = glm::vec4(0);
		offsetCol = glm::vec4(0);
	}
	vd.vtx0.BaseCol = packColor(baseCol + offsetCol);
}

template<>
void convertVertex(const N2_VERTEX_VR& vs, TA_VertexParam& vd)
{
	setCoords(vd.vtx0, vs.x, vs.y, vs.z);
	glm::vec4 baseCol = unpackColor(vs.rgb.argb0);
	glm::vec4 offsetCol = baseCol;
	if (curGmp != nullptr)
	{
		// Not sure about offset but vf4 needs base addition
		baseCol += unpackColor(curGmp->diffuse0);
		offsetCol += unpackColor(curGmp->specular0);
		computeColors(baseCol, offsetCol, curMatrix * glm::vec4(vs.x, vs.y, vs.z, 1), getNormal(vs));
	}
	vd.vtx0.BaseCol = packColor(baseCol + offsetCol);
}

template<>
void convertVertex(const N2_VERTEX_VU& vs, TA_VertexParam& vd)
{
	setCoords(vd.vtx3, vs.x, vs.y, vs.z);
	setUV(vs, vd.vtx3);
	glm::vec4 baseCol;
	glm::vec4 offsetCol;
	if (curGmp != nullptr)
	{
		baseCol = unpackColor(curGmp->diffuse0);
		offsetCol = unpackColor(curGmp->specular0);
		computeColors(baseCol, offsetCol, curMatrix * glm::vec4(vs.x, vs.y, vs.z, 1), getNormal(vs));
	}
	else
	{
		baseCol = glm::vec4(0);
		offsetCol = glm::vec4(0);
	}
	vd.vtx3.BaseCol = packColor(baseCol);
	vd.vtx3.OffsCol = packColor(offsetCol);
}

template<>
void convertVertex(const N2_VERTEX_VUR& vs, TA_VertexParam& vd)
{
	setCoords(vd.vtx3, vs.x, vs.y, vs.z);
	setUV(vs, vd.vtx3);
	glm::vec4 baseCol = unpackColor(vs.rgb.argb0);
	glm::vec4 offsetCol = baseCol;
	if (curGmp != nullptr)
	{
		// Not sure about offset but vf4 needs base addition
		baseCol += unpackColor(curGmp->diffuse0);
		offsetCol += unpackColor(curGmp->specular0);
		computeColors(baseCol, offsetCol, curMatrix * glm::vec4(vs.x, vs.y, vs.z, 1), getNormal(vs));
	}
	vd.vtx3.BaseCol = packColor(baseCol);
	vd.vtx3.OffsCol = packColor(offsetCol);
}

template<>
void convertVertex(const N2_VERTEX_VUB& vs, TA_VertexParam& vd)
{
	// TODO
	setCoords(vd.vtx3, vs.x, vs.y, vs.z);
	setUV(vs, vd.vtx3);
	glm::vec4 baseCol;
	glm::vec4 offsetCol;
	if (curGmp != nullptr)
	{
		baseCol = unpackColor(curGmp->diffuse0);
		offsetCol = unpackColor(curGmp->specular0);
		computeColors(baseCol, offsetCol, curMatrix * glm::vec4(vs.x, vs.y, vs.z, 1), getNormal(vs));
	}
	else
	{
		baseCol = glm::vec4(0);
		offsetCol = glm::vec4(0);
	}
	vd.vtx3.BaseCol = packColor(baseCol);
	vd.vtx3.OffsCol = packColor(offsetCol);
}

template <typename T>
static void sendVertices(const ICHList *list, const T* vtx)
{
	alignas(32) TA_VertexParam taVtx;
	taVtx.pcw.ParaType = 7;
	verify(list->vertexSize() > 0);

	alignas(32) TA_VertexParam fanCenterVtx{};
	alignas(32) TA_VertexParam fanLastVtx{};
	for (u32 i = 0; i < list->vtxCount; i++)
	{
		taVtx.pcw.EndOfStrip = vtx->header.endOfStrip;

		convertVertex(*vtx, taVtx);

		if (fanCenterVtx.pcw.ParaType == 0)
		{
			// Center vertex if triangle fan
			//verify(vtx->header.isFirstOrSecond()); This fails for some strips: strip=1 fan=0 (soul surfer)
			memcpy(&fanCenterVtx, &taVtx, sizeof(SQBuffer));
		}
		else if (vtx->header.isThird())
		{
			// End of strip if triangle fan
			if (i + 1 < list->vtxCount && vtx[1].header.isFan())
				taVtx.pcw.EndOfStrip = 1;
		}
		else if (vtx->header.isFan())
		{
			// Triangle fan
			ta_vtx_data32((SQBuffer *)&fanCenterVtx);
			ta_vtx_data32((SQBuffer *)&fanLastVtx);
			taVtx.pcw.EndOfStrip = 1;
		}
		ta_vtx_data32((SQBuffer *)&taVtx);
		memcpy(&fanLastVtx, &taVtx, sizeof(SQBuffer));
		fanLastVtx.pcw.EndOfStrip = 0;
		if (vtx->header.endOfStrip)
			fanCenterVtx.pcw.ParaType = 0;

		vtx++;
	}
}

template <typename T>
static void sendMVVertices(const ICHList *list, const T* vtx)
{
	SQBuffer sqb[2]{};
	TA_VertexParam& taVtx = *(TA_VertexParam *)&sqb[0];
	taVtx.mvolA.pcw.ParaType = 7;
	taVtx.mvolA.pcw.EndOfStrip = 1;
	verify(list->vertexSize() > 0);

	glm::vec4 vtx0{};
	glm::vec4 vtx1{};
	u32 stripStart = 0;

	for (u32 i = 0; i < list->vtxCount; i++)
	{
		glm::vec4 v(vtx->x, vtx->y, vtx->z, 1);
		v = projectionMatrix * curMatrix * v;
		v.x /= v.w;
		v.y /= v.w;
//		printf("MV %f %f %f - strip %d fan %d eos %d _res %x\n", v.x, v.y, 1 / v.w, vtx->header.strip, vtx->header.fan, vtx->header.endOfStrip, vtx->header._res);
		u32 triIdx = i - stripStart;
		if (triIdx >= 2)
		{
			if (triIdx & 1)
			{
				taVtx.mvolA.x1 = vtx0.x;
				taVtx.mvolA.y1 = vtx0.y;
				taVtx.mvolA.z1 = 1 / vtx0.w;

				taVtx.mvolA.x0 = vtx1.x;
				taVtx.mvolA.y0 = vtx1.y;
				taVtx.mvolA.z0 = 1 / vtx1.w;
			}
			else
			{
				taVtx.mvolA.x0 = vtx0.x;
				taVtx.mvolA.y0 = vtx0.y;
				taVtx.mvolA.z0 = 1 / vtx0.w;

				taVtx.mvolA.x1 = vtx1.x;
				taVtx.mvolA.y1 = vtx1.y;
				taVtx.mvolA.z1 = 1 / vtx1.w;
			}
			taVtx.mvolA.x2 = v.x;
			taVtx.mvolB.y2 = v.y;
			taVtx.mvolB.z2 = 1 / v.w;

			ta_vtx_data32(&sqb[0]);
			ta_vtx_data32(&sqb[1]);
		}
		if (vtx->header.endOfStrip)
			stripStart = i + 1;
		vtx0 = vtx1;
		vtx1 = v;
		vtx++;
	}
}

static void sendPolygon(ICHList *list)
{
	switch (list->flags)
	{
	case ICHList::VTX_TYPE_V:
		{
			Vertex *vtx = (Vertex *)((u8 *)list + sizeof(ICHList));
			if (state.listType & 1)
			{
				TA_ModVolParam pp{};
				pp.pcw.ParaType = 4;
				pp.pcw.ListType = state.listType ;
				pp.pcw.User_Clip = state.userClip;
				pp.pcw.Volume = list->pcw.volume;
				pp.isp = list->isp;
				pp.isp.CullMode = 0; // FIXME required for closed volumes and not set properly
				pp.isp.DepthMode &= 3;
				ta_vtx_data32((const SQBuffer *)&pp);

				//for (int i = 0; i < list->vtxCount; i++)
				//	printf("MV %f %f %f strip %d fan %d eos %d _res %x\n", vtx[i].x, vtx[i].y, vtx[i].z, vtx[i].header.strip, vtx[i].header.fan, vtx[i].header.endOfStrip, vtx[i].header._res);
				sendMVVertices(list, vtx);
			}
			else
			{
				// poly 0, vtx 0
				TA_PolyParam0 pp{};
				pp.pcw.ParaType = 4;
				pp.pcw.ListType = state.listType ;
				pp.pcw.User_Clip = state.userClip;
				pp.pcw.Shadow = list->pcw.shadow;
				pp.pcw.Gouraud = list->pcw.gouraud;
				pp.isp = list->isp;
				pp.tsp = list->tsp0;
				ta_vtx_data32((const SQBuffer *)&pp);

				sendVertices(list, vtx);
			}
		}
		break;

	case ICHList::VTX_TYPE_VU:
		{
			N2_VERTEX_VU *vtx = (N2_VERTEX_VU *)((u8 *)list + sizeof(ICHList));
			if (state.listType  & 1)
			{
				TA_ModVolParam pp{};
				pp.pcw.ParaType = 4;
				pp.pcw.ListType = state.listType ;
				pp.pcw.User_Clip = state.userClip;
				pp.pcw.Volume = list->pcw.volume;
				pp.isp = list->isp;
				pp.isp.CullMode = 0; // FIXME required for closed volumes and not set properly
				pp.isp.DepthMode &= 3;
				ta_vtx_data32((const SQBuffer *)&pp);

				//for (int i = 0; i < list->vtxCount; i++)
				//	printf("MV %f %f %f strip %d fan %d eos %d _res %x\n", vtx[i].x, vtx[i].y, vtx[i].z, vtx[i].header.strip, vtx[i].header.fan, vtx[i].header.endOfStrip, vtx[i].header._res);
				sendMVVertices(list, vtx);
			}
			else
			{
				TA_PolyParam0 pp{};
				pp.pcw.ParaType = 4;
				pp.pcw.ListType = state.listType ;
				pp.pcw.User_Clip = state.userClip;
				pp.pcw.Shadow = list->pcw.shadow;
				pp.pcw.Texture = 1;
				pp.pcw.Offset = list->pcw.offset;
				pp.pcw.Gouraud = list->pcw.gouraud;
				pp.isp = list->isp;
				pp.tsp = list->tsp0;
				pp.tcw = list->tcw0;
				if (state.listType == 2)
					pp.tsp.UseAlpha = 1; // FIXME alpha light volumes need manual settings of params?
//				pp.tsp.ShadInstr = 3; // FIXME
//				if (state.listType  == 2) // FIXME
//				{
//					pp.tsp.SrcInstr = 4;
//					pp.tsp.DstInstr = 5;
//				}
				ta_vtx_data32((const SQBuffer *)&pp);

				sendVertices(list, vtx);
			}
		}
		break;

	case ICHList::VTX_TYPE_VUR:
		{
			TA_PolyParam0 pp{};
			pp.pcw.ParaType = 4;
			pp.pcw.ListType = state.listType ;
			pp.pcw.User_Clip = state.userClip;
			pp.pcw.Shadow = list->pcw.shadow;
			pp.pcw.Texture = 1;
			pp.pcw.Offset = list->pcw.offset;
			pp.pcw.Gouraud = list->pcw.gouraud;
			pp.isp = list->isp;
			pp.tsp = list->tsp0;
			pp.tcw = list->tcw0;
			if (state.listType == 2)
				pp.tsp.UseAlpha = 1; // FIXME alpha light volumes need manual settings of params?
//			pp.tsp.ShadInstr = 3; // FIXME
//			if (state.listType  == 2) // FIXME
//			{
//				pp.tsp.SrcInstr = 4;
//				pp.tsp.DstInstr = 5;
//			}
			ta_vtx_data32((const SQBuffer *)&pp);

			N2_VERTEX_VUR *vtx = (N2_VERTEX_VUR *)((u8 *)list + sizeof(ICHList));
			sendVertices(list, vtx);
		}
		break;

	case ICHList::VTX_TYPE_VR:
		{
			// poly 0, vtx 0
			TA_PolyParam0 pp{};
			pp.pcw.ParaType = 4;
			pp.pcw.ListType = state.listType ;
			pp.pcw.User_Clip = state.userClip;
			pp.pcw.Shadow = list->pcw.shadow;
			pp.pcw.Gouraud = list->pcw.gouraud;
			pp.isp = list->isp;
			pp.tsp = list->tsp0;
			if (state.listType == 2)
				pp.tsp.UseAlpha = 1; // FIXME alpha light volumes need manual settings of params?
			ta_vtx_data32((const SQBuffer *)&pp);

			N2_VERTEX_VR *vtx = (N2_VERTEX_VR *)((u8 *)list + sizeof(ICHList));
			sendVertices(list, vtx);
		}
		break;

	case ICHList::VTX_TYPE_VUB:
		{
			// TODO
			TA_PolyParam0 pp{};
			pp.pcw.ParaType = 4;
			pp.pcw.ListType = state.listType ;
			pp.pcw.User_Clip = state.userClip;
			pp.pcw.Shadow = list->pcw.shadow;
			pp.pcw.Texture = 1;
			pp.pcw.Offset = 1;
			pp.pcw.Gouraud = list->pcw.gouraud;
			pp.isp = list->isp;
			pp.tsp = list->tsp0;
			pp.tcw = list->tcw0;
			//ta_vtx_data32((const SQBuffer *)&pp);

			//N2_VERTEX_VUB *vtx = (N2_VERTEX_VUB *)((u8 *)list + sizeof(ICHList));
			//sendVertices(list, vtx);
			INFO_LOG(PVR, "Unhandled poly format VTX_TYPE_VUB");
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
//	if (0x2b00 == (u32)(data - elanRAM))
//		for (int i = 0; i < size; i += 4)
//			DEBUG_LOG(PVR, "Elan Parse %08x: %08x", (u32)(&data[i] - elanRAM), *(u32 *)&data[i]);

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

			case PCW::matrix:
				state.setMatrix(data);
				size -= sizeof(Matrix);
				break;

			case PCW::projMatrix:
				state.setProjectionMatrix(data);
				size -= sizeof(ProjMatrix);
				break;

			case PCW::instance:
				{
					Instance *instance = (Instance *)data;
					if (instance->isModelInstance())
					{
						DEBUG_LOG(PVR, "Model instance offset %x size %x", instance->offset & 0x1ffffff8, instance->size);
//FIXME instance? model? executeCommand(&elanRAM[instance->offset & 0x1ffffff8], instance->size);
					}
					else if (instance->id1 & 0x10)
					{
						state.setLightModel(data);
					}
					else if ((instance->id2 & 0x40000000) || (instance->id1 & 0xffffff00)) // FIXME what are these lights without id2|0x40000000? vf4
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
					else
					{
						INFO_LOG(PVR, "Other instance %08x %08x", instance->id1, instance->id2);
						for (int i = 0; i < 32; i += 4)
							INFO_LOG(PVR, "    %08x: %08x", (u32)(&data[i] - elanRAM), *(u32 *)&data[i]);
					}
					size -= sizeof(Instance);
				}
				break;

			case PCW::model:
				{
					// FIXME instance and model are switched? this is used for nl_set_light_instance()
					// or static vs. dynamic?
					Model *model = (Model *)data;
					// TODO fails rt66 start verify(model->id1 == 0x18000000 || model->id1 == 0x10000000);
					state.userClip = model->pcw.userClip;
					DEBUG_LOG(PVR, "Model offset %x size %x clip %d", model->offset, model->size, model->pcw.userClip);
					executeCommand(&elanRAM[model->offset & 0x1ffffff8], model->size);
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
//bad						reg74 |= 0x3c;
						asic_RaiseInterrupt(inter);
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
					executeCommand(&elanRAM[link->offset & 0x1ffffff8], link->size);
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
				state.listType  = (pcw >> 24) & 0xf;
				// TODO is this the right place for this?
				SQBuffer eol{};
				ta_vtx_data32(&eol);
				size -= 24 * 4;
			}
			else if ((pcw & 0xd0fcff00) == 0x80800000) // User clipping
			{
				state.userClip = ((PCW&)pcw).userClip;
				DEBUG_LOG(PVR, "User clip type %d", state.userClip);
				size -= 0xE0;
			}
			else if ((pcw & 0xd0ffff00) == 0x80000000) // geometry follows or linked?
			{
				// FIXME this matches TA polys such as a2000009
				// no possible disambiguation since 80000000 is a valid OP poly pcw (poly type 0 / vtx 0)
				DEBUG_LOG(PVR, "Geometry type %d - %08x", (pcw >> 24) & 0xf, pcw);
				size -= 32;
				SQBuffer *sqb = (SQBuffer *)data + 1;
				while (size > 32)
				{
					DEBUG_LOG(PVR, "vtx data %p", sqb);
					ta_vtx_data32(sqb);
					sqb++;
					size -= 32;
				}
			}
			else if (pcw == 0x20000000)
			{
				// User clipping
				DEBUG_LOG(PVR, "User clipping %d,%d - %d,%d", ((u32 *)data)[4] * 32, ((u32 *)data)[5] * 32,
						((u32 *)data)[6] * 32, ((u32 *)data)[7] * 32);
				ta_vtx_data32((SQBuffer *)data);
				size -= 32;
			}
			else
			{
				if (pcw != 0)
					INFO_LOG(PVR, "Unhandled command %x", pcw);
				for (int i = 0; i < 32; i += 4)
					DEBUG_LOG(PVR, "    %08x: %08x", (u32)(&data[i] - elanRAM), *(u32 *)&data[i]);
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
	return *(T *)&elanRAM[addr & (ELAN_RAM_SIZE - 1)];
}

template<typename T>
void DYNACALL write_elanram(u32 addr, T data)
{
	*(T *)&elanRAM[addr & (ELAN_RAM_SIZE - 1)] = data;
}

void init()
{
	elanRAM = (u8 *)allocAligned(PAGE_SIZE, ELAN_RAM_SIZE);
}

void reset(bool hard)
{
	if (hard)
	{
		memset(elanRAM, 0, ELAN_RAM_SIZE);
		state.reset();
	}
}

void term()
{
	freeAligned(elanRAM);
	elanRAM = nullptr;
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
	_vmem_map_block(elanRAM, base | 0xA, base | 0xB, ELAN_RAM_SIZE - 1);
}

void serialize(Serializer& ser)
{
	if (settings.platform.system != DC_PLATFORM_NAOMI2)
		return;
	ser << reg10;
	ser << reg74;
	ser << elanCmd;
	if (!ser.rollback())
		ser.serialize(elanRAM, ELAN_RAM_SIZE);
}

void deserialize(Deserializer& deser)
{
	if (settings.platform.system != DC_PLATFORM_NAOMI2)
		return;
	deser >> reg10;
	deser >> reg74;
	deser >> elanCmd;
	if (!deser.rollback())
		deser.deserialize(elanRAM, ELAN_RAM_SIZE);
	state.reset();
}

}
