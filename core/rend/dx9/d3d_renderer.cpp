/*
	Copyright 2021 flyinghead

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
#include "d3d_renderer.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/tileclip.h"
#include "rend/gui.h"

#define verifyWin(x) verify(SUCCEEDED(x))

const u32 DstBlendGL[]
{
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_SRCCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA,
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA
};

const u32 SrcBlendGL[]
{
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_DESTCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA,
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA
};

const u32 CullMode[]
{
	D3DCULL_NONE,	//0	No culling	no culling
	D3DCULL_NONE,	//1	Cull if Small	Cull if	( |det| < fpu_cull_val )

	D3DCULL_CCW,	//2	Cull if Negative	Cull if 	( |det| < 0 ) or
					//( |det| < fpu_cull_val )
	D3DCULL_CW,		//3	Cull if Positive	Cull if 	( |det| > 0 ) or
					//( |det| < fpu_cull_val )
};

const u32 Zfunction[]
{
	D3DCMP_NEVER,				//0	Never
	D3DCMP_LESS,				//1	Less
	D3DCMP_EQUAL,				//2	Equal
	D3DCMP_LESSEQUAL,			//3	Less Or Equal
	D3DCMP_GREATER,				//4	Greater
	D3DCMP_NOTEQUAL,			//5	Not Equal
	D3DCMP_GREATEREQUAL,		//6	Greater Or Equal
	D3DCMP_ALWAYS,				//7	Always
};

const D3DVERTEXELEMENT9 MainVtxElement[]
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },	//Base color
	{ 0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },	//Specular color
	{ 0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },	//u,v
	D3DDECL_END()
};
const D3DVERTEXELEMENT9 ModVolVtxElement[]
{
	{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	D3DDECL_END()
};

bool D3DRenderer::ensureVertexBufferSize(ComPtr<IDirect3DVertexBuffer9>& buffer, u32& currentSize, u32 minSize)
{
	if (minSize <= currentSize && buffer)
		return true;
	if (currentSize == 0)
		currentSize = minSize;
	else
		while (currentSize < minSize)
			currentSize *= 2;
	buffer.reset();
	return SUCCEEDED(device->CreateVertexBuffer(currentSize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &buffer.get(), 0));
}

bool D3DRenderer::ensureIndexBufferSize(ComPtr<IDirect3DIndexBuffer9>& buffer, u32& currentSize, u32 minSize)
{
	if (minSize <= currentSize && buffer)
		return true;
	if (currentSize == 0)
		currentSize = minSize;
	else
		while (currentSize < minSize)
			currentSize *= 2;
	buffer.reset();
	return SUCCEEDED(device->CreateIndexBuffer(currentSize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &buffer.get(), 0));
}

bool D3DRenderer::Init()
{
	ComPtr<IDirect3D9> d3d9 = theDXContext.getD3D();
	D3DCAPS9 caps;
	d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
	if (caps.VertexShaderVersion < D3DVS_VERSION(1, 0))
	{
		WARN_LOG(RENDERER, "Vertex shader version %x", caps.VertexShaderVersion);
		return false;
	}
	if (caps.PixelShaderVersion < D3DPS_VERSION(2, 0))
	{
		WARN_LOG(RENDERER, "Pixel shader version %x", caps.PixelShaderVersion);
		return false;
	}

	device = theDXContext.getDevice();
	devCache.setDevice(device);

	bool success = ensureVertexBufferSize(vertexBuffer, vertexBufferSize, 4 * 1024 * 1024);
	success &= ensureIndexBufferSize(indexBuffer, indexBufferSize, 120 * 1024 * 4);

	success &= SUCCEEDED(device->CreateVertexDeclaration(MainVtxElement, &mainVtxDecl.get()));
	success &= SUCCEEDED(device->CreateVertexDeclaration(ModVolVtxElement, &modVolVtxDecl.get()));

	shaders.init(device);
	success &= (bool)shaders.getVertexShader(true);
	success &= SUCCEEDED(device->CreateTexture(32, 32, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &paletteTexture.get(), 0));
	success &= SUCCEEDED(device->CreateTexture(128, 2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8, D3DPOOL_DEFAULT, &fogTexture.get(), 0));
	fog_needs_update = true;
	palette_updated = true;

	if (!success)
	{
		WARN_LOG(RENDERER, "DirectX9 renderer initialization failed");
		Term();
	}
	frameRendered = false;
	frameRenderedOnce = false;

	return success;
}

void D3DRenderer::preReset()
{
	texCache.Clear();
	backbuffer.reset();
	depthSurface.reset();
	rttSurface.reset();
	rttTexture.reset();
	dcfbSurface.reset();
	dcfbTexture.reset();
	fogTexture.reset();
	paletteTexture.reset();
	modVolVtxDecl.reset();
	mainVtxDecl.reset();
	modvolBuffer.reset();
	modvolBufferSize = 0;
	sortedTriIndexBuffer.reset();
	sortedTriIndexBufferSize = 0;
	indexBuffer.reset();
	indexBufferSize = 0;
	vertexBuffer.reset();
	vertexBufferSize = 0;
	framebufferSurface.reset();
	framebufferTexture.reset();
	resetting = true;
	frameRendered = false;
	frameRenderedOnce = false;
}

void D3DRenderer::postReset()
{
	resetting = false;
	devCache.reset();
	u32 w = width;	// FIXME
	u32 h = height;
	width = 0;
	height = 0;
	Resize(w, h);
	verify(ensureVertexBufferSize(vertexBuffer, vertexBufferSize, 4 * 1024 * 1024));
	verify(ensureIndexBufferSize(indexBuffer, indexBufferSize, 120 * 1024 * 4));
	verifyWin(device->CreateVertexDeclaration(MainVtxElement, &mainVtxDecl.get()));
	verifyWin(device->CreateVertexDeclaration(ModVolVtxElement, &modVolVtxDecl.get()));
	verifyWin(device->CreateTexture(32, 32, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &paletteTexture.get(), 0));
	verifyWin(device->CreateTexture(128, 2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8, D3DPOOL_DEFAULT, &fogTexture.get(), 0));
	fog_needs_update = true;
	palette_updated = true;
}

void D3DRenderer::Term()
{
	preReset();
	resetting = false;
	devCache.reset();
	shaders.term();
	device.reset();
}

BaseTextureCacheData *D3DRenderer::GetTexture(TSP tsp, TCW tcw)
{
	if (resetting)
		return nullptr;
	//lookup texture
	D3DTexture* tf = texCache.getTextureCacheData(tsp, tcw);

	if (tf->texture == nullptr)
		tf->Create();

	//update if needed
	if (tf->NeedsUpdate())
		tf->Update();
	else
	{
		if (tf->IsCustomTextureAvailable())
		{
			texCache.DeleteLater(tf->texture);
			tf->texture.reset();
			tf->loadCustomTexture();
		}
	}
	return tf;
}

void D3DRenderer::readDCFramebuffer()
{
	if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
		return;

	PixelBuffer<u32> pb;
	int width;
	int height;
	ReadFramebuffer<BGRAPacker>(pb, width, height);

	if (!dcfbTexture)
	{
		// FIXME dimension can change
		device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dcfbTexture.get(), 0);
		dcfbTexture->GetSurfaceLevel(0, &dcfbSurface.get());
	}

	D3DLOCKED_RECT rect;
	dcfbTexture->LockRect(0, &rect, nullptr, 0);
	if ((u32)rect.Pitch ==  width * sizeof(u32))
		memcpy(rect.pBits, pb.data(), width * height * sizeof(u32));
	else
	{
		u8 *dst = (u8 *)rect.pBits;
		for (int y = 0; y < height; y++)
			memcpy(dst + y * rect.Pitch, pb.data() + y * width, width * sizeof(u32));
	}
	dcfbTexture->UnlockRect(0);
}

void D3DRenderer::renderDCFramebuffer()
{
	device->ColorFill(framebufferSurface, 0, D3DCOLOR_ARGB(255, VO_BORDER_COL.Red, VO_BORDER_COL.Green, VO_BORDER_COL.Blue));
	u32 bar = (width - height * 640 / 480) / 2;
	RECT rd{ (LONG)bar, 0, (LONG)(width - bar), (LONG)height };
	device->StretchRect(dcfbSurface, nullptr, framebufferSurface, &rd, D3DTEXF_LINEAR);
}

bool D3DRenderer::Process(TA_context* ctx)
{
	if (resetting)
		return false;

	if (KillTex)
		texCache.Clear();
	texCache.Cleanup();

	if (ctx->rend.isRenderFramebuffer)
	{
		readDCFramebuffer();
	}
	else
	{
		if (!ta_parse_vdrc(ctx, true))
			return false;
	}

	return true;
}

inline void D3DRenderer::setTexMode(D3DSAMPLERSTATETYPE state, u32 clamp, u32 mirror)
{
	if (clamp)
		devCache.SetSamplerState(0, state, D3DTADDRESS_CLAMP);
	else
	{
		if (mirror)
			devCache.SetSamplerState(0, state, D3DTADDRESS_MIRROR);
		else
			devCache.SetSamplerState(0, state, D3DTADDRESS_WRAP);
	}
}

template <u32 Type, bool SortingEnabled>
void D3DRenderer::setGPState(const PolyParam *gp)
{
	float trilinear_alpha;
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1 && Type != ListType_Punch_Through && gp->tcw.MipMapped == 1)
	{
		trilinear_alpha = 0.25f * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			trilinear_alpha = 1.f - trilinear_alpha;
	}
	else
		trilinear_alpha = 1.f;

	bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min != 0 || pvrrc.fog_clamp_max != 0xffffffff);
	int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;

	int clip_rect[4] = {};
	TileClipping clipmode = GetTileClip(gp->tileclip, matrices.GetViewportMatrix(), clip_rect);
	D3DTexture *texture = (D3DTexture *)gp->texture;
	bool gpuPalette = texture != nullptr ? texture->gpuPalette : false;

	devCache.SetPixelShader(shaders.getShader(
			gp->pcw.Texture,
			gp->tsp.UseAlpha,
			gp->tsp.IgnoreTexA,
			gp->tsp.ShadInstr,
			gp->pcw.Offset,
			fog_ctrl,
			gp->tcw.PixelFmt == PixelBumpMap,
			color_clamp,
			trilinear_alpha != 1.f,
			gpuPalette,
			gp->pcw.Gouraud));

	if (trilinear_alpha != 1.f)
	{
		float f[4] { trilinear_alpha, 0, 0, 0 };
		device->SetPixelShaderConstantF(5, f, 1);
	}
	if (gpuPalette)
	{
		float paletteIndex[4];
		if (gp->tcw.PixelFmt == PixelPal4)
			paletteIndex[0] = (float)(gp->tcw.PalSelect << 4);
		else
			paletteIndex[0] = (float)((gp->tcw.PalSelect >> 4) << 8);
		device->SetPixelShaderConstantF(0, paletteIndex, 1);
	}
	devCache.SetVertexShader(shaders.getVertexShader(gp->pcw.Gouraud));
	devCache.SetRenderState(D3DRS_SHADEMODE, gp->pcw.Gouraud == 1 ? D3DSHADE_GOURAUD : D3DSHADE_FLAT);

	/* TODO
	if (clipmode == TileClipping::Inside)
	{
		float f[] = { clip_rect[0], clip_rect[1], clip_rect[0] + clip_rect[2], clip_rect[1] + clip_rect[3] };
		device->SetPixelShaderConstantF(n, f, 1);
	}
	else */
	if (clipmode == TileClipping::Outside)
	{
		devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
		RECT rect { clip_rect[0], clip_rect[1], clip_rect[0] + clip_rect[2], clip_rect[1] + clip_rect[3] };
		// TODO cache
		device->SetScissorRect(&rect);
	}
	else
	{
		devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, scissorEnable);
		if (scissorEnable)
			device->SetScissorRect(&scissorRect);
	}

	const u32 stencil = (gp->pcw.Shadow != 0) ? 0x80 : 0;
	if (config::ModifierVolumes)
		devCache.SetRenderState(D3DRS_STENCILREF, stencil);

	if (texture != nullptr)
	{
		devCache.SetTexture(0, texture->texture);
		setTexMode(D3DSAMP_ADDRESSU, gp->tsp.ClampU, gp->tsp.FlipU);
		setTexMode(D3DSAMP_ADDRESSV, gp->tsp.ClampV, gp->tsp.FlipV);

		//set texture filter mode
		if (gp->tsp.FilterMode == 0 || gpuPalette)
		{
			//disable filtering, mipmaps
			devCache.SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			devCache.SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			devCache.SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
		}
		else
		{
			//bilinear filtering
			devCache.SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			devCache.SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			devCache.SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);		// LINEAR for Trilinear filtering
		}
	}

	// Apparently punch-through polys support blending, or at least some combinations
	if (Type == ListType_Translucent || Type == ListType_Punch_Through)
	{
		devCache.SetRenderState(D3DRS_SRCBLEND, SrcBlendGL[gp->tsp.SrcInstr]);
		devCache.SetRenderState(D3DRS_DESTBLEND, DstBlendGL[gp->tsp.DstInstr]);
	}

	devCache.SetRenderState(D3DRS_CULLMODE, CullMode[gp->isp.CullMode]);

	//set Z mode, only if required
	if (Type == ListType_Punch_Through || (Type == ListType_Translucent && SortingEnabled))
	{
		devCache.SetRenderState(D3DRS_ZFUNC, Zfunction[6]); // GEQ
	}
	else
	{
		devCache.SetRenderState(D3DRS_ZFUNC, Zfunction[gp->isp.DepthMode]);
	}

	if (SortingEnabled && !config::PerStripSorting)
		devCache.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	else
	{
		// Z Write Disable seems to be ignored for punch-through.
		// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
		if (Type == ListType_Punch_Through)
			devCache.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		else
			devCache.SetRenderState(D3DRS_ZWRITEENABLE, !gp->isp.ZWriteDis);
	}
}

template <u32 Type, bool SortingEnabled>
void D3DRenderer::drawList(const List<PolyParam>& gply, int first, int count)
{
	PolyParam* params = &gply.head()[first];

	while (count-- > 0)
	{
		if (params->count > 2)
		{
			if ((Type == ListType_Opaque || (Type == ListType_Translucent && !SortingEnabled)) && params->isp.DepthMode == 0)
			{
				// depthFunc = never
				params++;
				continue;
			}
			setGPState<Type, SortingEnabled>(params);
			device->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, 0, params->count, params->first, params->count - 2);
		}

		params++;
	}
}

void D3DRenderer::sortTriangles(int first, int count)
{
	std::vector<u32> vidx_sort;
	GenSorted(first, count, pidx_sort, vidx_sort);

	//Upload to GPU if needed
	if (pidx_sort.empty())
		return;

	const u32 bufSize = vidx_sort.size() * sizeof(u32);
	// Upload sorted index buffer
	ensureIndexBufferSize(sortedTriIndexBuffer, sortedTriIndexBufferSize, bufSize);
	void *ptr;
	sortedTriIndexBuffer->Lock(0, bufSize, &ptr, D3DLOCK_DISCARD);
	memcpy(ptr, &vidx_sort[0], bufSize);
	sortedTriIndexBuffer->Unlock();
	device->SetIndices(sortedTriIndexBuffer);
}

void D3DRenderer::drawSorted(bool multipass)
{
	if (pidx_sort.empty())
		return;

	u32 count = pidx_sort.size();

	for (u32 p = 0; p < count; p++)
	{
		const PolyParam* params = pidx_sort[p].ppid;
		if (pidx_sort[p].count > 2)
		{
			setGPState<ListType_Translucent, true>(params);
			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, pidx_sort[p].count, pidx_sort[p].first, pidx_sort[p].count / 3);
		}
	}
	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		devCache.SetRenderState(D3DRS_COLORWRITEENABLE, 0);
		devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

		// We use the modifier volumes shader because it's fast. We don't need textures, etc.
		devCache.SetPixelShader(shaders.getModVolShader());

		devCache.SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL);
		devCache.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, scissorEnable);
		if (scissorEnable)
			device->SetScissorRect(&scissorRect);

		for (u32 p = 0; p < count; p++)
		{
			const PolyParam* params = pidx_sort[p].ppid;
			if (pidx_sort[p].count > 2 && !params->isp.ZWriteDis) {
				// FIXME no clipping in modvol shader
				//SetTileClip(gp->tileclip,true);

				devCache.SetRenderState(D3DRS_CULLMODE, CullMode[params->isp.CullMode]);
				device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, pidx_sort[p].count, pidx_sort[p].first, pidx_sort[p].count / 3);
			}
		}
		devCache.SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
	}
	device->SetIndices(indexBuffer);
}

//All pixels are in area 0 by default.
//If inside an 'in' volume, they are in area 1
//if inside an 'out' volume, they are in area 0
/*
	Stencil bits:
		bit 7: mv affected (must be preserved)
		bit 1: current volume state
		but 0: summary result (starts off as 0)

	Lower 2 bits:

	IN volume (logical OR):
	00 -> 00
	01 -> 01
	10 -> 01
	11 -> 01

	Out volume (logical AND):
	00 -> 00
	01 -> 00
	10 -> 00
	11 -> 01
*/
void D3DRenderer::setMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc)
{
	if (mv_mode == Xor)
	{
		// set states
		devCache.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		// write only bit 1
		devCache.SetRenderState(D3DRS_STENCILWRITEMASK, 2);
		// no stencil testing
		devCache.SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		devCache.SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INVERT); // flip bit 1
		devCache.SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);	// else keep it

		// Cull mode needs to be set
		devCache.SetRenderState(D3DRS_CULLMODE, CullMode[ispc.CullMode]);

	}
	else if (mv_mode == Or)
	{
		// set states
		devCache.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		// write only bit 1
		devCache.SetRenderState(D3DRS_STENCILWRITEMASK, 2);
		// no stencil testing
		devCache.SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
		// Or'ing of all triangles
		devCache.SetRenderState(D3DRS_STENCILREF, 2);
		devCache.SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);	// set bit 1
		devCache.SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);		// else keep it

		// Cull mode needs to be set
		devCache.SetRenderState(D3DRS_CULLMODE, CullMode[ispc.CullMode]);
	}
	else
	{
		// Inclusion or Exclusion volume

		// no depth test
		devCache.SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		// write bits 1:0
		devCache.SetRenderState(D3DRS_STENCILWRITEMASK, 3);
		// read bits 1:0
		devCache.SetRenderState(D3DRS_STENCILMASK, 3);

		if (mv_mode == Inclusion)
		{
			// Inclusion volume
			//res : old : final
			//0   : 0      : 00
			//0   : 1      : 01
			//1   : 0      : 01
			//1   : 1      : 01

			// if (1<=st) st=1; else st=0;
			devCache.SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL);
			devCache.SetRenderState(D3DRS_STENCILREF, 1);
			devCache.SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);	// set bit 0, clear bit 1
			devCache.SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO);
		}
		else
		{
			// Exclusion volume
			/*
				I've only seen a single game use it, so i guess it doesn't matter ? (Zombie revenge)
				(actually, i think there was also another, racing game)
			*/
			// The initial value for exclusion volumes is 1 so we need to invert the result before and'ing.
			//res : old : final
			//0   : 0   : 00
			//0   : 1   : 01
			//1   : 0   : 00
			//1   : 1   : 00

			// if (1 == st) st = 1; else st = 0;
			devCache.SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL);
			devCache.SetRenderState(D3DRS_STENCILREF, 1);
			devCache.SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
			devCache.SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO);
		}
	}
}

void D3DRenderer::drawModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.used() == 0 || !config::ModifierVolumes)
		return;

	device->SetVertexDeclaration(modVolVtxDecl);
	device->SetStreamSource(0, modvolBuffer, 0, 3 * sizeof(float));

	devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	devCache.SetRenderState(D3DRS_STENCILENABLE, TRUE);
	devCache.SetRenderState(D3DRS_ZWRITEENABLE, D3DZB_FALSE);
	devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, scissorEnable);
	if (scissorEnable)
		device->SetScissorRect(&scissorRect);

	devCache.SetPixelShader(shaders.getModVolShader());

	ModifierVolumeParam* params = &pvrrc.global_param_mvo.head()[first];

	devCache.SetRenderState(D3DRS_COLORWRITEENABLE, 0);

	int mod_base = -1;

	for (int cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

		if (!param.isp.VolumeLast && mv_mode > 0)
			setMVS_Mode(Or, param.isp);		// OR'ing (open volume or quad)
		else
			setMVS_Mode(Xor, param.isp);	// XOR'ing (closed volume)

		device->DrawPrimitive(D3DPT_TRIANGLELIST, param.first * 3, param.count);

		if (mv_mode == 1 || mv_mode == 2)
		{
			// Sum the area
			setMVS_Mode(mv_mode == 1 ? Inclusion : Exclusion, param.isp);
			device->DrawPrimitive(D3DPT_TRIANGLELIST, mod_base * 3, param.first + param.count - mod_base);
			mod_base = -1;
		}
	}
	//disable culling
	devCache.SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	//enable color writes
	devCache.SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

	//black out any stencil with '1'
	devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	devCache.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	devCache.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	//only pixels that are Modvol enabled, and in area 1
	devCache.SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
	devCache.SetRenderState(D3DRS_STENCILREF, 0x81);
	devCache.SetRenderState(D3DRS_STENCILMASK, 0x81);

	//clear the stencil result bits
	devCache.SetRenderState(D3DRS_STENCILWRITEMASK, 3);
	devCache.SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_ZERO);
	devCache.SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO);

	//don't do depth testing
	devCache.SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

	device->SetVertexDeclaration(mainVtxDecl);
	device->SetStreamSource(0, vertexBuffer, 0, sizeof(Vertex));
	device->SetIndices(indexBuffer);

	device->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, 0, 4, 0, 2);

	//restore states
	devCache.SetRenderState(D3DRS_STENCILENABLE, FALSE);
	devCache.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
}

// Direct3D uses the color values of the first vertex for flat shaded triangle strips.
// On Dreamcast the last vertex is the provoking one so we must copy it onto the first.
// TODO refactor with Vk
void D3DRenderer::setProvokingVertices()
{
	auto setProvokingVertex = [](const List<PolyParam>& list) {
        u32 *idx_base = pvrrc.idx.head();
        Vertex *vtx_base = pvrrc.verts.head();
		const PolyParam *pp_end = list.LastPtr(0);
		for (const PolyParam *pp = list.head(); pp != pp_end; pp++)
		{
			if (!pp->pcw.Gouraud && pp->count > 2)
			{
				for (u32 i = 0; i < pp->count - 2; i++)
				{
					Vertex *vertex = &vtx_base[idx_base[pp->first + i]];
					Vertex *lastVertex = &vtx_base[idx_base[pp->first + i + 2]];
					memcpy(vertex->col, lastVertex->col, 4);
					memcpy(vertex->spc, lastVertex->spc, 4);
					//memcpy(vertex->col1, lastVertex->col1, 4);
					//memcpy(vertex->spc1, lastVertex->spc1, 4);
				}
			}
		}
	};
	setProvokingVertex(pvrrc.global_param_op);
	setProvokingVertex(pvrrc.global_param_pt);
	setProvokingVertex(pvrrc.global_param_tr);
}

void D3DRenderer::drawStrips()
{
	RenderPass previous_pass {};
    for (int render_pass = 0; render_pass < pvrrc.render_passes.used(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];
        u32 op_count = current_pass.op_count - previous_pass.op_count;
        u32 pt_count = current_pass.pt_count - previous_pass.pt_count;
        u32 tr_count = current_pass.tr_count - previous_pass.tr_count;
        u32 mvo_count = current_pass.mvo_count - previous_pass.mvo_count;
        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d", render_pass + 1,
        		op_count, pt_count, tr_count, mvo_count);

		if (config::ModifierVolumes)
		{
			devCache.SetRenderState(D3DRS_STENCILENABLE, TRUE);
			devCache.SetRenderState(D3DRS_STENCILWRITEMASK, 0xFF);
			devCache.SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
			devCache.SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
			devCache.SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		}
		else
		{
			devCache.SetRenderState(D3DRS_STENCILENABLE, FALSE);
		}
		devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

		drawList<ListType_Opaque, false>(pvrrc.global_param_op, previous_pass.op_count, op_count);

		devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		devCache.SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
		devCache.SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		devCache.SetRenderState(D3DRS_ALPHAREF, PT_ALPHA_REF & 0xFF);

		drawList<ListType_Punch_Through, false>(pvrrc.global_param_pt, previous_pass.pt_count, pt_count);

		devCache.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

		drawModVols(previous_pass.mvo_count, mvo_count);

		devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		devCache.SetRenderState(D3DRS_STENCILENABLE, FALSE);

		if (current_pass.autosort)
		{
			if (!config::PerStripSorting)
			{
				sortTriangles(previous_pass.tr_count, tr_count);
				drawSorted(render_pass < pvrrc.render_passes.used() - 1);
			}
			else
			{
				SortPParams(previous_pass.tr_count, tr_count);
				drawList<ListType_Translucent, true>(pvrrc.global_param_tr, previous_pass.tr_count, tr_count);
			}
		}
		else
		{
			drawList<ListType_Translucent, false>(pvrrc.global_param_tr, previous_pass.tr_count, tr_count);
		}
		previous_pass = current_pass;
    }
}

void D3DRenderer::setBaseScissor()
{
	bool wide_screen_on = !pvrrc.isRTT && config::Widescreen && !matrices.IsClipped() && !config::Rotate90;
	if (!wide_screen_on)
	{
		float fWidth;
		float fHeight;
		float min_x;
		float min_y;
		if (!pvrrc.isRTT)
		{
			glm::vec4 clip_min(pvrrc.fb_X_CLIP.min, pvrrc.fb_Y_CLIP.min, 0, 1);
			glm::vec4 clip_dim(pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1,
							   pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1, 0, 0);
			clip_min = matrices.GetScissorMatrix() * clip_min;
			clip_dim = matrices.GetScissorMatrix() * clip_dim;

			min_x = clip_min[0];
			min_y = clip_min[1];
			fWidth = clip_dim[0];
			fHeight = clip_dim[1];
			if (fWidth < 0)
			{
				min_x += fWidth;
				fWidth = -fWidth;
			}
			if (fHeight < 0)
			{
				min_y += fHeight;
				fHeight = -fHeight;
			}
			if (matrices.GetSidebarWidth() > 0)
			{
				float scaled_offs_x = matrices.GetSidebarWidth();

				D3DCOLOR borderColor = D3DCOLOR_ARGB(255, VO_BORDER_COL.Red, VO_BORDER_COL.Green, VO_BORDER_COL.Blue);
				devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
				D3DRECT rects[] {
						{ 0, 0, lroundf(scaled_offs_x), (long)height },
						{ (long)(width - scaled_offs_x), 0, (long)(width + 1), (long)height },
				};
				device->Clear(2, rects, D3DCLEAR_TARGET, borderColor, 0.f, 0);
			}
		}
		else
		{
			fWidth = (float)(pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1);
			fHeight = (float)(pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1);
			min_x = (float)pvrrc.fb_X_CLIP.min;
			min_y = (float)pvrrc.fb_Y_CLIP.min;
			if (config::RenderResolution > 480 && !config::RenderToTextureBuffer)
			{
				min_x *= config::RenderResolution / 480.f;
				min_y *= config::RenderResolution / 480.f;
				fWidth *= config::RenderResolution / 480.f;
				fHeight *= config::RenderResolution / 480.f;
			}
		}
		scissorEnable = true;
		scissorRect.left = lroundf(min_x);
		scissorRect.top = lroundf(min_y);
		scissorRect.right = scissorRect.left + lroundf(fWidth);
		scissorRect.bottom = scissorRect.top + lroundf(fHeight);
		device->SetScissorRect(&scissorRect);
		devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	}
	else
	{
		devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		scissorEnable = false;
	}
}

void D3DRenderer::prepareRttRenderTarget(u32 texAddress)
{
	u32 fbw = pvrrc.fb_X_CLIP.max + 1;
	u32 fbh = pvrrc.fb_Y_CLIP.max + 1;
	DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d x %d @ %06x",
			FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8, fbw, fbh, texAddress);
	// Find the smallest power of two texture that fits the viewport
	u32 fbh2 = 2;
	while (fbh2 < fbh)
		fbh2 *= 2;
	u32 fbw2 = 2;
	while (fbw2 < fbw)
		fbw2 *= 2;
	if (!config::RenderToTextureBuffer)
	{
		fbw *= config::RenderResolution / 480.f;
		fbh *= config::RenderResolution / 480.f;
		fbw2 *= config::RenderResolution / 480.f;
		fbh2 *= config::RenderResolution / 480.f;
	}
	rttTexture.reset();
	device->CreateTexture(fbw2, fbh2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &rttTexture.get(), NULL);

	rttSurface.reset();
	rttTexture->GetSurfaceLevel(0, &rttSurface.get());
	device->SetRenderTarget(0, rttSurface);

	D3DVIEWPORT9 viewport;
	viewport.X = viewport.Y = 0;
	viewport.Width = fbw;
	viewport.Height = fbh;
	viewport.MinZ = 0;
	viewport.MaxZ = 1;
	device->SetViewport(&viewport);
}

void D3DRenderer::readRttRenderTarget(u32 texAddress)
{
	u32 w = pvrrc.fb_X_CLIP.max + 1;
	u32 h = pvrrc.fb_Y_CLIP.max + 1;
	const u8 fb_packmode = FB_W_CTRL.fb_packmode;
	if (config::RenderToTextureBuffer)
	{
		D3DSURFACE_DESC rttDesc;
		rttSurface->GetDesc(&rttDesc);
		ComPtr<IDirect3DSurface9> offscreenSurface;
		verifyWin(device->CreateOffscreenPlainSurface(rttDesc.Width, rttDesc.Height, rttDesc.Format, D3DPOOL_SYSTEMMEM, &offscreenSurface.get(), nullptr));
		verifyWin(device->GetRenderTargetData(rttSurface, offscreenSurface));

		PixelBuffer<u32> tmp_buf;
		tmp_buf.init(w, h);

		u8 *p = (u8 *)tmp_buf.data();
		D3DLOCKED_RECT rect;
		RECT lockRect { 0, 0, (long)w, (long)h };
		verifyWin(offscreenSurface->LockRect(&rect, &lockRect, D3DLOCK_READONLY));
		if ((u32)rect.Pitch == w * sizeof(u32))
			memcpy(p, rect.pBits, w * h * sizeof(u32));
		else
		{
			u8 *src = (u8 *)rect.pBits;
			for (u32 y = 0; y < h; y++)
			{
				memcpy(p, src, w * sizeof(u32));
				src += rect.Pitch;
				p += w * sizeof(u32);
			}
		}
		verifyWin(offscreenSurface->UnlockRect());

		u16 *dst = (u16 *)&vram[texAddress];
		WriteTextureToVRam<2, 1, 0, 3>(w, h, p, dst);
	}
	else
	{
		//memset(&vram[gl.rtt.texAddress], 0, size);
		if (w <= 1024 && h <= 1024)
		{
			// TexAddr : (address), Reserved : 0, StrideSel : 0, ScanOrder : 1
			TCW tcw = { { texAddress >> 3, 0, 0, 1 } };
			switch (fb_packmode) {
			case 0:
			case 3:
				tcw.PixelFmt = Pixel1555;
				break;
			case 1:
				tcw.PixelFmt = Pixel565;
				break;
			case 2:
				tcw.PixelFmt = Pixel4444;
				break;
			}
			TSP tsp = { 0 };
			for (tsp.TexU = 0; tsp.TexU <= 7 && (8u << tsp.TexU) < w; tsp.TexU++)
				;

			for (tsp.TexV = 0; tsp.TexV <= 7 && (8u << tsp.TexV) < h; tsp.TexV++)
				;

			D3DTexture* texture = texCache.getTextureCacheData(tsp, tcw);
			if (!texture->texture)
				texture->Create();

			texture->texture = rttTexture;
			texture->dirty = 0;
			libCore_vramlock_Lock(texture->sa_tex, texture->sa + texture->size - 1, texture);
		}
	}
}

bool D3DRenderer::Render()
{
	if (resetting)
		return false;
	bool is_rtt = pvrrc.isRTT;

	backbuffer.reset();
	verifyWin(device->GetRenderTarget(0, &backbuffer.get()));
	u32 texAddress = FB_W_SOF1 & VRAM_MASK;
	if (is_rtt)
	{
		prepareRttRenderTarget(texAddress);
	}
	else
	{
		verifyWin(device->SetRenderTarget(0, framebufferSurface));
		D3DVIEWPORT9 viewport;
		viewport.X = viewport.Y = 0;
		viewport.Width = width;
		viewport.Height = height;
		viewport.MinZ = 0;
		viewport.MaxZ = 1;
		verifyWin(device->SetViewport(&viewport));
	}
	verifyWin(device->SetDepthStencilSurface(depthSurface));
	matrices.CalcMatrices(&pvrrc, width, height);
	// infamous DX9 half-pixel viewport shift
	// https://docs.microsoft.com/en-us/windows/win32/direct3d9/directly-mapping-texels-to-pixels
	glm::mat4 normalMat = glm::translate(glm::vec3(-1.f / width, 1.f / height, 0)) * matrices.GetNormalMatrix();
	verifyWin(device->SetVertexShaderConstantF(0, &normalMat[0][0], 4));

	devCache.reset();
	devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->Clear(0, NULL, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0, 0.0f, 0);

	if (!pvrrc.isRenderFramebuffer)
	{
		setProvokingVertices();

		verify(ensureVertexBufferSize(vertexBuffer, vertexBufferSize, pvrrc.verts.bytes()));
		void *ptr;
		verifyWin(vertexBuffer->Lock(0, pvrrc.verts.bytes(), &ptr, D3DLOCK_DISCARD));
		memcpy(ptr, pvrrc.verts.head(), pvrrc.verts.bytes());
		vertexBuffer->Unlock();
		verify(ensureIndexBufferSize(indexBuffer, indexBufferSize, pvrrc.idx.bytes()));
		verifyWin(indexBuffer->Lock(0, pvrrc.idx.bytes(), &ptr, D3DLOCK_DISCARD));
		memcpy(ptr, pvrrc.idx.head(), pvrrc.idx.bytes());
		indexBuffer->Unlock();

		if (config::ModifierVolumes && pvrrc.modtrig.used())
		{
			verify(ensureVertexBufferSize(modvolBuffer, modvolBufferSize, pvrrc.modtrig.bytes()));
			verifyWin(modvolBuffer->Lock(0, pvrrc.modtrig.bytes(), &ptr, D3DLOCK_DISCARD));
			memcpy(ptr, pvrrc.modtrig.head(), pvrrc.modtrig.bytes());
			modvolBuffer->Unlock();
		}

		updateFogTexture();
		updatePaletteTexture();

		devCache.SetVertexShader(shaders.getVertexShader(true));

		// VERT and RAM fog color constants
		u8* fog_colvert_bgra = (u8*)&FOG_COL_VERT;
		u8* fog_colram_bgra = (u8*)&FOG_COL_RAM;
		float ps_FOG_COL_VERT[4] = { fog_colvert_bgra[2] / 255.0f, fog_colvert_bgra[1] / 255.0f, fog_colvert_bgra[0] / 255.0f, 1 };
		float ps_FOG_COL_RAM[4] = { fog_colram_bgra[2] / 255.0f, fog_colram_bgra[1] / 255.0f, fog_colram_bgra[0] / 255.0f, 1 };
		device->SetPixelShaderConstantF(1, ps_FOG_COL_VERT, 1);
		device->SetPixelShaderConstantF(2, ps_FOG_COL_RAM, 1);

		// Fog density and scale constants
		u8* fog_density = (u8*)&FOG_DENSITY;
		float fog_den_mant = fog_density[1] / 128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
		s32 fog_den_exp = (s8)fog_density[0];
		float fog_den_float = fog_den_mant * powf(2.0f, (float)fog_den_exp) * config::ExtraDepthScale;
		float fogDensityAndScale[4]= { fog_den_float, 1.f - FPU_SHAD_SCALE.scale_factor / 256.f, 0, 1 };
		device->SetPixelShaderConstantF(3, fogDensityAndScale, 1);

		// Color clamping
		float fog_clamp_min[] {
			((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f,
			((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f,
			((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f,
			((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f
		};
		device->SetPixelShaderConstantF(6, fog_clamp_min, 1);
		float fog_clamp_max[] {
			((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f,
			((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f,
			((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f,
			((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f
		};
		device->SetPixelShaderConstantF(7, fog_clamp_max, 1);

		devCache.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

		device->SetVertexDeclaration(mainVtxDecl);
		device->SetStreamSource(0, vertexBuffer, 0, sizeof(Vertex));
		device->SetIndices(indexBuffer);

		devCache.SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		devCache.SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

		devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		devCache.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		devCache.SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		devCache.SetRenderState(D3DRS_CLIPPLANEENABLE, 0);

		setBaseScissor();

		if (!SUCCEEDED(device->BeginScene()))
		{
			WARN_LOG(RENDERER, "Render: BeginScene failed!");
			return false;
		}
		drawStrips();
		device->EndScene();
	}
	else
	{
		renderDCFramebuffer();
	}

	verifyWin(device->SetRenderTarget(0, backbuffer));

	if (is_rtt)
	{
		readRttRenderTarget(texAddress);
	}
	else
	{
		renderFramebuffer();
		DrawOSD(false);
		frameRendered = true;
		frameRenderedOnce = true;
		theDXContext.setFrameRendered();
	}

	return !is_rtt;
}

void D3DRenderer::Resize(int w, int h)
{
	if (width == (u32)w && height == (u32)h)
		return;
	width = w;
	height = h;
	framebufferTexture.reset();
	framebufferSurface.reset();
	verifyWin(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &framebufferTexture.get(), NULL));
	verifyWin(framebufferTexture->GetSurfaceLevel(0, &framebufferSurface.get()));
	depthSurface.reset();
	verifyWin(device->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &depthSurface.get(), nullptr));
	frameRendered = false;
	frameRenderedOnce = false;
}

void D3DRenderer::renderFramebuffer()
{
	devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->ColorFill(backbuffer, 0, D3DCOLOR_ARGB(255, VO_BORDER_COL.Red, VO_BORDER_COL.Green, VO_BORDER_COL.Blue));
	int fx = 0;
	int sx = 0;
	float screenAR = (float)settings.display.width / settings.display.height;
	int fbwidth = width;
	int fbheight = height;
	if (config::Rotate90)
		std::swap(fbwidth, fbheight);
	float renderAR = (float)fbwidth / fbheight;
	if (renderAR > screenAR)
		fx = (int)roundf((fbwidth - screenAR * fbheight) / 2.f);
	else
		sx = (int)roundf((settings.display.width - renderAR * settings.display.height) / 2.f);

	if (!config::Rotate90)
	{
		RECT rs { 0, 0, (long)width, (long)height };
		RECT rd { 0, 0, settings.display.width, settings.display.height };
		if (sx != 0)
		{
			rd.left = sx;
			rd.right = settings.display.width - sx;
		}
		else
		{
			rs.left = fx;
			rs.right = width - fx;
		}
		device->StretchRect(framebufferSurface, &rs, backbuffer, &rd, D3DTEXF_LINEAR);	// This can fail if window is minimized
	}
	else
	{
		device->SetPixelShader(NULL);
		device->SetVertexShader(NULL);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

		glm::mat4 identity = glm::identity<glm::mat4>();
		glm::mat4 projection = glm::translate(glm::vec3(-1.f / settings.display.width, 1.f / settings.display.height, 0))
			* glm::rotate((float)M_PI_2, glm::vec3(0, 0, 1));

		device->SetTransform(D3DTS_WORLD, (const D3DMATRIX *)&identity[0][0]);
		device->SetTransform(D3DTS_VIEW, (const D3DMATRIX *)&identity[0][0]);
		device->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX *)&projection[0][0]);

		device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
		D3DVIEWPORT9 viewport;
		viewport.X = sx;
		viewport.Y = fx * settings.display.width / height;
		viewport.Width = settings.display.width - sx * 2;
		viewport.Height = settings.display.height - 2 * fx * settings.display.width / height;
		viewport.MinZ = 0;
		viewport.MaxZ = 1;
		verifyWin(device->SetViewport(&viewport));
		float coords[] {
			-1,  1, 0.5f,  0, 0,
			-1, -1, 0.5f,  0, 1,
			 1,  1, 0.5f,  1, 0,
			 1, -1, 0.5f,  1, 1,
		};
		device->SetTexture(0, framebufferTexture);
		device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, coords, sizeof(float) * 5);
	}
}

bool D3DRenderer::RenderLastFrame()
{
	if (!frameRenderedOnce)
		return false;
	backbuffer.reset();
	verifyWin(device->GetRenderTarget(0, &backbuffer.get()));
	renderFramebuffer();

	return true;
}

void D3DRenderer::updatePaletteTexture()
{
	if (!palette_updated)
		return;
	palette_updated = false;

	D3DLOCKED_RECT rect;
	verifyWin(paletteTexture->LockRect(0, &rect, nullptr, 0));
	if (rect.Pitch == 32 * sizeof(u32))
		memcpy(rect.pBits, palette32_ram, 32 * 32 * sizeof(u32));
	else
	{
		u8 *dst = (u8 *)rect.pBits;
		for (int y = 0; y < 32; y++)
			memcpy(dst + y * rect.Pitch, palette32_ram + y * 32, 32 * sizeof(u32));
	}

	paletteTexture->UnlockRect(0);
	device->SetTexture(1, paletteTexture);
	device->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	device->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
}

void D3DRenderer::updateFogTexture()
{
	if (!fog_needs_update || !config::Fog)
		return;
	fog_needs_update = false;
	u8 temp_tex_buffer[256];
	MakeFogTexture(temp_tex_buffer);

	D3DLOCKED_RECT rect;
	verifyWin(fogTexture->LockRect(0, &rect, nullptr, 0));
	if (rect.Pitch == 128)
		memcpy(rect.pBits, temp_tex_buffer, 128 * 2 * 1);
	else
	{
		u8 *dst = (u8 *)rect.pBits;
		for (int y = 0; y < 2; y++)
			memcpy(dst + y * rect.Pitch, temp_tex_buffer + y * 128, 128);
	}
	fogTexture->UnlockRect(0);
	device->SetTexture(2, fogTexture);
	device->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
}

void D3DRenderer::DrawOSD(bool clear_screen)
{
	theDXContext.setOverlay(!clear_screen);
	gui_display_osd();
	theDXContext.setOverlay(false);
}

Renderer* rend_DirectX9()
{
	return new D3DRenderer();
}
