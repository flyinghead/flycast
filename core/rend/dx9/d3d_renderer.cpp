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
#include "ui/gui.h"
#include "rend/sorter.h"

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
	maxAnisotropy = caps.MaxAnisotropy;

	device = theDXContext.getDevice();
	devCache.setDevice(device);

	bool success = ensureVertexBufferSize(vertexBuffer, vertexBufferSize, 4_MB);
	success &= ensureIndexBufferSize(indexBuffer, indexBufferSize, 120 * 1024 * 4);

	success &= SUCCEEDED(device->CreateVertexDeclaration(MainVtxElement, &mainVtxDecl.get()));
	success &= SUCCEEDED(device->CreateVertexDeclaration(ModVolVtxElement, &modVolVtxDecl.get()));

	shaders.init(device);
	success &= (bool)shaders.getVertexShader(true);
	success &= SUCCEEDED(device->CreateTexture(32, 32, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &paletteTexture.get(), 0));
	success &= SUCCEEDED(device->CreateTexture(128, 2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8, D3DPOOL_DEFAULT, &fogTexture.get(), 0));
	fog_needs_update = true;

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
	fbScaledTexture.reset();
	fbScaledSurface.reset();
	fogTexture.reset();
	paletteTexture.reset();
	modVolVtxDecl.reset();
	mainVtxDecl.reset();
	modvolBuffer.reset();
	modvolBufferSize = 0;
	indexBuffer.reset();
	indexBufferSize = 0;
	vertexBuffer.reset();
	vertexBufferSize = 0;
	framebufferSurface.reset();
	framebufferTexture.reset();
	frameRendered = false;
	frameRenderedOnce = false;
}

void D3DRenderer::postReset()
{
	devCache.reset();
	u32 w = width;	// FIXME
	u32 h = height;
	width = 0;
	height = 0;
	resize(w, h);
	bool rc = ensureVertexBufferSize(vertexBuffer, vertexBufferSize, 4_MB);
	verify(rc);
	rc = ensureIndexBufferSize(indexBuffer, indexBufferSize, 120 * 1024 * 4);
	verify(rc);
	rc = SUCCEEDED(device->CreateVertexDeclaration(MainVtxElement, &mainVtxDecl.get()));
	verify(rc);
	rc = SUCCEEDED(device->CreateVertexDeclaration(ModVolVtxElement, &modVolVtxDecl.get()));
	verify(rc);
	rc = SUCCEEDED(device->CreateTexture(32, 32, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &paletteTexture.get(), 0));
	verify(rc);
	rc = SUCCEEDED(device->CreateTexture(128, 2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8, D3DPOOL_DEFAULT, &fogTexture.get(), 0));
	verify(rc);
	fog_needs_update = true;
	palette_updated = true;
}

void D3DRenderer::Term()
{
	preReset();
	devCache.reset();
	shaders.term();
	device.reset();
}

BaseTextureCacheData *D3DRenderer::GetTexture(TSP tsp, TCW tcw)
{
	if (!theDXContext.isReady())
		return nullptr;
	//lookup texture
	D3DTexture* tf = texCache.getTextureCacheData(tsp, tcw);

	//update if needed
	if (tf->NeedsUpdate())
	{
		if (!tf->Update())
			tf = nullptr;
	}
	else if (tf->IsCustomTextureAvailable())
	{
		texCache.DeleteLater(tf->texture);
		tf->texture.reset();
		tf->loadCustomTexture();
	}
	return tf;
}

void D3DRenderer::RenderFramebuffer(const FramebufferInfo& info)
{
	if (!theDXContext.isReady()) {
		// force a Present
		frameRendered = true;
		return;
	}

	backbuffer.reset();
	device->GetRenderTarget(0, &backbuffer.get());

	PixelBuffer<u32> pb;
	int width;
	int height;

	if (info.fb_r_ctrl.fb_enable == 0 || info.vo_control.blank_video == 1)
	{
		// Video output disabled
		width = height = 1;
		pb.init(width, height, false);
		u8 *p = (u8 *)pb.data(0, 0);
		p[0] = info.vo_border_col._blue;
		p[1] = info.vo_border_col._green;
		p[2] = info.vo_border_col._red;
		p[3] = 255;
	}
	else
	{
		ReadFramebuffer<BGRAPacker>(info, pb, width, height);
	}

	if (dcfbTexture)
	{
		D3DSURFACE_DESC desc;
		dcfbTexture->GetLevelDesc(0, &desc);
		if ((int)desc.Width != width || (int)desc.Height != height)
			dcfbTexture.reset();
	}
	if (!dcfbTexture)
	{
		device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dcfbTexture.get(), 0);
		dcfbSurface.reset();
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

	resize(width, height);
	devCache.reset();
	devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->ColorFill(framebufferSurface, 0, D3DCOLOR_ARGB(255, info.vo_border_col._red, info.vo_border_col._green, info.vo_border_col._blue));
	device->StretchRect(dcfbSurface, nullptr, framebufferSurface, nullptr, D3DTEXF_LINEAR);

	aspectRatio = getDCFramebufferAspectRatio();
	displayFramebuffer();
	DrawOSD(false);
	frameRendered = true;
	frameRenderedOnce = true;
	theDXContext.setFrameRendered();
}

void D3DRenderer::Process(TA_context* ctx)
{
	if (!theDXContext.isReady()) {
		// force a Present
		frameRendered = true;
		return;
	}
	if (settings.platform.isNaomi2())
		throw FlycastException("DirectX 9 doesn't support Naomi 2 games. Select a different graphics API");

	if (KillTex)
		texCache.Clear();
	texCache.Cleanup();

	ta_parse(ctx, false);
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

	bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min.full != 0 || pvrrc.fog_clamp_max.full != 0xffffffff);
	int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;

	int clip_rect[4] = {};
	TileClipping clipmode = GetTileClip(gp->tileclip, matrices.GetViewportMatrix(), clip_rect);
	D3DTexture *texture = (D3DTexture *)gp->texture;
	int gpuPalette = texture == nullptr || !texture->gpuPalette ? 0
			: gp->tsp.FilterMode + 1;
	if (gpuPalette != 0)
	{
		if (config::TextureFiltering == 1)
			gpuPalette = 1; // force nearest
		else if (config::TextureFiltering == 2)
			gpuPalette = 2; // force linear
	}

	devCache.SetPixelShader(shaders.getShader(
			gp->pcw.Texture,
			gp->tsp.UseAlpha,
			gp->tsp.IgnoreTexA || gp->tcw.PixelFmt == Pixel565,
			gp->tsp.ShadInstr,
			gp->pcw.Offset,
			fog_ctrl,
			gp->tcw.PixelFmt == PixelBumpMap,
			color_clamp,
			trilinear_alpha != 1.f,
			gpuPalette,
			gp->pcw.Gouraud,
			clipmode == TileClipping::Inside,
			dithering));

	if (trilinear_alpha != 1.f)
	{
		float f[4] { trilinear_alpha, 0, 0, 0 };
		device->SetPixelShaderConstantF(5, f, 1);
	}
	if (gpuPalette != 0)
	{
		float textureSize[4] { (float)texture->width, (float)texture->height };
		device->SetPixelShaderConstantF(9, textureSize, 1);
		float paletteIndex[4];
		if (gp->tcw.PixelFmt == PixelPal4)
			paletteIndex[0] = (float)(gp->tcw.PalSelect << 4);
		else
			paletteIndex[0] = (float)((gp->tcw.PalSelect >> 4) << 8);
		device->SetPixelShaderConstantF(0, paletteIndex, 1);
	}
	devCache.SetVertexShader(shaders.getVertexShader(gp->pcw.Gouraud));
	devCache.SetRenderState(D3DRS_SHADEMODE, gp->pcw.Gouraud == 1 ? D3DSHADE_GOURAUD : D3DSHADE_FLAT);

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
		if (clipmode == TileClipping::Inside)
		{
			float f[] = { (float)clip_rect[0], (float)clip_rect[1], (float)(clip_rect[0] + clip_rect[2]), (float)(clip_rect[1] + clip_rect[3]) };
			device->SetPixelShaderConstantF(4, f, 1);
		}
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
		bool linearFiltering;
		if (gpuPalette != 0)
			linearFiltering = false;
		else if (config::TextureFiltering == 0)
			linearFiltering = gp->tsp.FilterMode != 0;
		else if (config::TextureFiltering == 1)
			linearFiltering = false;
		else
			linearFiltering = true;

		if (!linearFiltering)
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
			if (Type == ListType_Punch_Through) {
				devCache.SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
				devCache.SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1);
			}
			else {
				devCache.SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);		// LINEAR for Trilinear filtering
				devCache.SetSamplerState(0, D3DSAMP_MAXANISOTROPY, std::min(maxAnisotropy, (int)config::AnisotropicFiltering));
			}
		}
		float bias = -1.f;
		devCache.SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, *(DWORD *)&bias);
	}

	// Apparently punch-through polys support blending, or at least some combinations
	devCache.SetRenderState(D3DRS_SRCBLEND, SrcBlendGL[gp->tsp.SrcInstr]);
	devCache.SetRenderState(D3DRS_DESTBLEND, DstBlendGL[gp->tsp.DstInstr]);

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

	if (SortingEnabled /* && !config::PerStripSorting */)
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
void D3DRenderer::drawList(const std::vector<PolyParam>& gply, int first, int count)
{
	if (count == 0)
		return;
	const PolyParam *params = &gply[first];

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

void D3DRenderer::drawSorted(int first, int count, bool multipass)
{
	if (count == 0)
		return;
	int end = first + count;
	for (int p = first; p < end; p++)
	{
		const PolyParam *params = &pvrrc.global_param_tr[pvrrc.sortedTriangles[p].polyIndex];
		setGPState<ListType_Translucent, true>(params);
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, pvrrc.sortedTriangles[p].count, pvrrc.sortedTriangles[p].first, pvrrc.sortedTriangles[p].count / 3);
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

		for (int p = first; p < end; p++)
		{
			const PolyParam *params = &pvrrc.global_param_tr[pvrrc.sortedTriangles[p].polyIndex];
			if (!params->isp.ZWriteDis)
			{
				// FIXME no clipping in modvol shader
				//SetTileClip(gp->tileclip,true);

				devCache.SetRenderState(D3DRS_CULLMODE, CullMode[params->isp.CullMode]);
				device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, pvrrc.sortedTriangles[p].count, pvrrc.sortedTriangles[p].first, pvrrc.sortedTriangles[p].count / 3);
			}
		}
		devCache.SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
	}
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
	if (count == 0 || pvrrc.modtrig.empty() || !config::ModifierVolumes)
		return;

	device->SetVertexDeclaration(modVolVtxDecl);
	device->SetStreamSource(0, modvolBuffer, 0, 3 * sizeof(float));

	devCache.SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER);
	devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	devCache.SetRenderState(D3DRS_STENCILENABLE, TRUE);
	devCache.SetRenderState(D3DRS_ZWRITEENABLE, D3DZB_FALSE);
	devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, scissorEnable);
	if (scissorEnable)
		device->SetScissorRect(&scissorRect);

	devCache.SetPixelShader(shaders.getModVolShader());

	ModifierVolumeParam* params = &pvrrc.global_param_mvo[first];

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

void D3DRenderer::drawStrips()
{
	RenderPass previous_pass {};
    for (int render_pass = 0; render_pass < (int)pvrrc.render_passes.size(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes[render_pass];
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
		devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

		drawList<ListType_Opaque, false>(pvrrc.global_param_op, previous_pass.op_count, op_count);

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
				drawSorted(previous_pass.sorted_tr_count, current_pass.sorted_tr_count - previous_pass.sorted_tr_count,
						render_pass < (int)pvrrc.render_passes.size() - 1);
			else
				drawList<ListType_Translucent, true>(pvrrc.global_param_tr, previous_pass.tr_count, tr_count);
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
	bool wide_screen_on = !pvrrc.isRTT && config::Widescreen && !matrices.IsClipped()
			&& !config::Rotate90 && !config::EmulateFramebuffer;
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

				D3DCOLOR borderColor = D3DCOLOR_ARGB(255, VO_BORDER_COL._red, VO_BORDER_COL._green, VO_BORDER_COL._blue);
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
			min_x = (float)pvrrc.getFramebufferMinX();
			min_y = (float)pvrrc.getFramebufferMinY();
			fWidth = (float)pvrrc.getFramebufferWidth() - min_x;
			fHeight = (float)pvrrc.getFramebufferHeight() - min_y;
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

void D3DRenderer::prepareRttRenderTarget(u32 texAddress, int& vpWidth, int& vpHeight)
{
	u32 fbw = pvrrc.getFramebufferWidth();
	u32 fbh = pvrrc.getFramebufferHeight();
	DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d x %d @ %06x",
			pvrrc.fb_W_CTRL.fb_packmode, pvrrc.fb_W_LINESTRIDE * 8, fbw, fbh, texAddress);
	u32 fbw2;
	u32 fbh2;
	getRenderToTextureDimensions(fbw, fbh, fbw2, fbh2);

	rttTexture.reset();
	device->CreateTexture(fbw2, fbh2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &rttTexture.get(), NULL);

	rttSurface.reset();
	rttTexture->GetSurfaceLevel(0, &rttSurface.get());
	device->SetRenderTarget(0, rttSurface);

	if (fbw2 > width || fbh2 > height || !depthSurface)
	{
		if (depthSurface)
		{
			D3DSURFACE_DESC desc;
			depthSurface->GetDesc(&desc);
			if (fbw2 > desc.Width || fbh2 > desc.Height)
				depthSurface.reset();
		}
		if (!depthSurface)
		{
			HRESULT rc = SUCCEEDED(device->CreateDepthStencilSurface(std::max(fbw2, width), std::max(fbh2, height), D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &depthSurface.get(), nullptr));
			verify(rc);
		}
	}

	D3DVIEWPORT9 viewport;
	viewport.X = viewport.Y = 0;
	viewport.Width = fbw;
	viewport.Height = fbh;
	viewport.MinZ = 0;
	viewport.MaxZ = 1;
	device->SetViewport(&viewport);
	vpWidth = fbw;
	vpHeight = fbh;
}

void D3DRenderer::readRttRenderTarget(u32 texAddress)
{
	u32 w = pvrrc.getFramebufferWidth();
	u32 h = pvrrc.getFramebufferHeight();
	if (config::RenderToTextureBuffer)
	{
		D3DSURFACE_DESC rttDesc;
		rttSurface->GetDesc(&rttDesc);
		ComPtr<IDirect3DSurface9> offscreenSurface;
		bool rc = SUCCEEDED(device->CreateOffscreenPlainSurface(rttDesc.Width, rttDesc.Height, rttDesc.Format, D3DPOOL_SYSTEMMEM, &offscreenSurface.get(), nullptr));
		verify(rc);
		rc = SUCCEEDED(device->GetRenderTargetData(rttSurface, offscreenSurface));
		verify(rc);

		PixelBuffer<u32> tmp_buf;
		tmp_buf.init(w, h);

		u8 *p = (u8 *)tmp_buf.data();
		D3DLOCKED_RECT rect;
		RECT lockRect { 0, 0, (long)w, (long)h };
		rc = SUCCEEDED(offscreenSurface->LockRect(&rect, &lockRect, D3DLOCK_READONLY));
		verify(rc);
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
		rc = SUCCEEDED(offscreenSurface->UnlockRect());
		verify(rc);

		u16 *dst = (u16 *)&vram[texAddress];
		WriteTextureToVRam<2, 1, 0, 3>(w, h, (u8 *)tmp_buf.data(), dst, pvrrc.fb_W_CTRL, pvrrc.fb_W_LINESTRIDE * 8);
	}
	else
	{
		//memset(&vram[gl.rtt.texAddress], 0, size);
		if (w <= 1024 && h <= 1024)
		{
			D3DTexture* texture = texCache.getRTTexture(texAddress, pvrrc.fb_W_CTRL.fb_packmode, w, h);
			texture->texture = rttTexture;
			texture->dirty = 0;
			texture->unprotectVRam();
		}
	}
}

bool D3DRenderer::Render()
{
	if (!theDXContext.isReady())
		return false;

	bool is_rtt = pvrrc.isRTT;

	backbuffer.reset();
	bool rc = SUCCEEDED(device->GetRenderTarget(0, &backbuffer.get()));
	verify(rc);
	u32 texAddress = pvrrc.fb_W_SOF1 & VRAM_MASK;
	int vpWidth, vpHeight;
	if (is_rtt)
	{
		prepareRttRenderTarget(texAddress, vpWidth, vpHeight);
	}
	else
	{
		resize(pvrrc.framebufferWidth, pvrrc.framebufferHeight);
		if (pvrrc.clearFramebuffer)
			device->ColorFill(framebufferSurface, 0, D3DCOLOR_ARGB(255, VO_BORDER_COL._red, VO_BORDER_COL._green, VO_BORDER_COL._blue));
		rc = SUCCEEDED(device->SetRenderTarget(0, framebufferSurface));
		verify(rc);
		D3DVIEWPORT9 viewport;
		viewport.X = viewport.Y = 0;
		viewport.Width = width;
		viewport.Height = height;
		viewport.MinZ = 0;
		viewport.MaxZ = 1;
		rc = SUCCEEDED(device->SetViewport(&viewport));
		verify(rc);
		vpWidth = width;
		vpHeight = height;
	}
	rc = SUCCEEDED(device->SetDepthStencilSurface(depthSurface));
	verify(rc);
	matrices.CalcMatrices(&pvrrc, width, height);
	// infamous DX9 half-pixel viewport shift
	// https://docs.microsoft.com/en-us/windows/win32/direct3d9/directly-mapping-texels-to-pixels
	glm::mat4 normalMat = glm::translate(glm::vec3(1.f / vpWidth, 1.f / vpHeight, 0)) * matrices.GetNormalMatrix();
	rc = SUCCEEDED(device->SetVertexShaderConstantF(0, &normalMat[0][0], 4));
	verify(rc);

	devCache.reset();
	devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->Clear(0, NULL, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0, 0.0f, 0);

	setFirstProvokingVertex(pvrrc);
	// Set clip planes at (-1,0) (1,0) (0,-1) and (0,1).
	// Helps avoiding interpolation errors on large triangles.
	devCache.SetRenderState(D3DRS_CLIPPLANEENABLE, 15);
	float v[4] {};
	v[3] = 1.f;
	// left
	v[0] = 1.f;
	device->SetClipPlane(0, v);
	// right
	v[0] = -1.f;
	device->SetClipPlane(1, v);
	// top
	v[0] = 0.f;
	v[1] = 1.f;
	device->SetClipPlane(2, v);
	// bottom
	v[1] = -1.f;
	device->SetClipPlane(3, v);

	size_t size = pvrrc.verts.size() * sizeof(decltype(*pvrrc.verts.data()));
	rc = ensureVertexBufferSize(vertexBuffer, vertexBufferSize, size);
	verify(rc);
	void *ptr;
	rc = SUCCEEDED(vertexBuffer->Lock(0, size, &ptr, D3DLOCK_DISCARD));
	verify(rc);
	memcpy(ptr, pvrrc.verts.data(), size);
	vertexBuffer->Unlock();
	size = pvrrc.idx.size() * sizeof(decltype(*pvrrc.idx.data()));
	rc = ensureIndexBufferSize(indexBuffer, indexBufferSize, size);
	verify(rc);
	rc = SUCCEEDED(indexBuffer->Lock(0, size, &ptr, D3DLOCK_DISCARD));
	verify(rc);
	memcpy(ptr, pvrrc.idx.data(), size);
	indexBuffer->Unlock();

	if (config::ModifierVolumes && !pvrrc.modtrig.empty())
	{
		size = pvrrc.modtrig.size() * sizeof(decltype(pvrrc.modtrig[0]));
		rc = ensureVertexBufferSize(modvolBuffer, modvolBufferSize, size);
		verify(rc);
		rc = SUCCEEDED(modvolBuffer->Lock(0, size, &ptr, D3DLOCK_DISCARD));
		verify(rc);
		memcpy(ptr, &pvrrc.modtrig[0], size);
		modvolBuffer->Unlock();
	}

	updateFogTexture();
	updatePaletteTexture();

	devCache.SetVertexShader(shaders.getVertexShader(true));

	// VERT and RAM fog color constants
	float ps_FOG_COL_VERT[4];
	float ps_FOG_COL_RAM[4];
	FOG_COL_VERT.getRGBColor(ps_FOG_COL_VERT);
	FOG_COL_RAM.getRGBColor(ps_FOG_COL_RAM);
	device->SetPixelShaderConstantF(1, ps_FOG_COL_VERT, 1);
	device->SetPixelShaderConstantF(2, ps_FOG_COL_RAM, 1);

	// Fog density and scale constants
	float fog_den_float = FOG_DENSITY.get() * config::ExtraDepthScale;
	float fogDensityAndScale[4]= { fog_den_float, 1.f - FPU_SHAD_SCALE.scale_factor / 256.f, 0, 1 };
	device->SetPixelShaderConstantF(3, fogDensityAndScale, 1);

	// Color clamping
	float color_clamp[4];
	pvrrc.fog_clamp_min.getRGBAColor(color_clamp);
	device->SetPixelShaderConstantF(6, color_clamp, 1);
	pvrrc.fog_clamp_max.getRGBAColor(color_clamp);
	device->SetPixelShaderConstantF(7, color_clamp, 1);

	// Dithering
	dithering = config::EmulateFramebuffer && pvrrc.fb_W_CTRL.fb_dither && pvrrc.fb_W_CTRL.fb_packmode <= 3;
	if (dithering)
	{
		float ditherColorMax[4];
		switch (pvrrc.fb_W_CTRL.fb_packmode)
		{
		case 0: // 0555 KRGB 16 bit
		case 3: // 1555 ARGB 16 bit
			ditherColorMax[0] = ditherColorMax[1] = ditherColorMax[2] = 31.f;
			ditherColorMax[3] = 255.f;
			break;
		case 1: // 565 RGB 16 bit
			ditherColorMax[0] = ditherColorMax[2] = 31.f;
			ditherColorMax[1] = 63.f;
			ditherColorMax[3] = 255.f;
			break;
		case 2: // 4444 ARGB 16 bit
			ditherColorMax[0] = ditherColorMax[1]
				= ditherColorMax[2] = ditherColorMax[3] = 15.f;
			break;
		default:
			break;
		}
		device->SetPixelShaderConstantF(8, ditherColorMax, 1);
	}

	devCache.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

	device->SetVertexDeclaration(mainVtxDecl);
	device->SetStreamSource(0, vertexBuffer, 0, sizeof(Vertex));
	device->SetIndices(indexBuffer);

	devCache.SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	devCache.SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	devCache.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	devCache.SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	devCache.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	devCache.SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	devCache.SetRenderState(D3DRS_CLIPPING, FALSE);

	setBaseScissor();

	if (!SUCCEEDED(device->BeginScene()))
	{
		WARN_LOG(RENDERER, "Render: BeginScene failed!");
		return false;
	}
	drawStrips();
	device->EndScene();
	devCache.SetRenderState(D3DRS_CLIPPLANEENABLE, 0);

	rc = SUCCEEDED(device->SetRenderTarget(0, backbuffer));
	verify(rc);

	if (is_rtt)
	{
		readRttRenderTarget(texAddress);
	}
	else if (config::EmulateFramebuffer)
	{
		writeFramebufferToVRAM();
	}
	else
	{
		aspectRatio = getOutputFramebufferAspectRatio();
		displayFramebuffer();
		DrawOSD(false);
		frameRendered = true;
		frameRenderedOnce = true;
		theDXContext.setFrameRendered();
	}

	return !is_rtt;
}

void D3DRenderer::resize(int w, int h)
{
	if (width == (u32)w && height == (u32)h)
		return;
	if (!config::EmulateFramebuffer)
		// TODO use different surfaces in full fb emulation to avoid resizing twice per frame
		NOTICE_LOG(RENDERER, "D3DRenderer::resize: %d x %d -> %d x %d", width, height, w, h);
	width = w;
	height = h;
	framebufferTexture.reset();
	framebufferSurface.reset();
	HRESULT hr = device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &framebufferTexture.get(), NULL);
	if (FAILED(hr)) {
		ERROR_LOG(RENDERER, "Framebuffer texture (%d x %d) creation failed: %x", w, h, hr);
		die("Framebuffer texture creation failed");
	}
	bool rc = SUCCEEDED(framebufferTexture->GetSurfaceLevel(0, &framebufferSurface.get()));
	verify(rc);
	depthSurface.reset();
	rc = SUCCEEDED(device->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &depthSurface.get(), nullptr));
	verify(rc);
	frameRendered = false;
	frameRenderedOnce = false;
}

void D3DRenderer::displayFramebuffer()
{
	devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->ColorFill(backbuffer, 0, D3DCOLOR_ARGB(255, VO_BORDER_COL._red, VO_BORDER_COL._green, VO_BORDER_COL._blue));
	float screenAR = (float)settings.display.width / settings.display.height;
	int dx = 0;
	int dy = 0;
	if (aspectRatio > screenAR)
		dy = (int)roundf(settings.display.height * (1 - screenAR / aspectRatio) / 2.f);
	else
		dx = (int)roundf(settings.display.width * (1 - aspectRatio / screenAR) / 2.f);

	float shiftX, shiftY;
	getVideoShift(shiftX, shiftY);
	if (!config::Rotate90 && shiftX == 0 && shiftY == 0)
	{
		RECT rs { 0, 0, (long)width, (long)height };
		RECT rd { dx, dy, settings.display.width - dx, settings.display.height - dy };
		device->StretchRect(framebufferSurface, &rs, backbuffer, &rd,
				config::TextureFiltering == 1 ? D3DTEXF_POINT : D3DTEXF_LINEAR);	// This can fail if window is minimized
	}
	else
	{
		device->SetPixelShader(NULL);
		device->SetVertexShader(NULL);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, config::TextureFiltering == 1 ? D3DTEXF_POINT : D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, config::TextureFiltering == 1 ? D3DTEXF_POINT : D3DTEXF_LINEAR);

		glm::mat4 identity = glm::identity<glm::mat4>();
		glm::mat4 projection = glm::translate(glm::vec3(-1.f / settings.display.width, 1.f / settings.display.height, 0));
		if (config::Rotate90)
			projection *= glm::rotate((float)M_PI_2, glm::vec3(0, 0, 1));

		device->SetTransform(D3DTS_WORLD, (const D3DMATRIX *)&identity[0][0]);
		device->SetTransform(D3DTS_VIEW, (const D3DMATRIX *)&identity[0][0]);
		device->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX *)&projection[0][0]);

		device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
		D3DVIEWPORT9 viewport;
		viewport.X = dx;
		viewport.Y = dy;
		viewport.Width = settings.display.width - dx * 2;
		viewport.Height = settings.display.height - dy * 2;
		viewport.MinZ = 0;
		viewport.MaxZ = 1;
		bool rc = SUCCEEDED(device->SetViewport(&viewport));
		verify(rc);
		float coords[] {
			-1,  1, 0.5f,  0, 0,
			-1, -1, 0.5f,  0, 1,
			 1,  1, 0.5f,  1, 0,
			 1, -1, 0.5f,  1, 1,
		};
		coords[0] = coords[5] = -1.f + shiftX * 2.f / width;
		coords[10] = coords[15] = coords[0] + 2;
		coords[1] = coords[11] = 1.f - shiftY * 2.f / height;
		coords[6] = coords[16] = coords[1] - 2;

		device->SetTexture(0, framebufferTexture);
		device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, coords, sizeof(float) * 5);
	}
}

bool D3DRenderer::RenderLastFrame()
{
	if (!frameRenderedOnce || !theDXContext.isReady())
		return false;
	backbuffer.reset();
	bool rc = SUCCEEDED(device->GetRenderTarget(0, &backbuffer.get()));
	verify(rc);
	devCache.reset();
	displayFramebuffer();

	return true;
}

void D3DRenderer::updatePaletteTexture()
{
	if (!palette_updated)
		return;
	palette_updated = false;

	D3DLOCKED_RECT rect;
	bool rc = SUCCEEDED(paletteTexture->LockRect(0, &rect, nullptr, 0));
	verify(rc);
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
	bool rc = SUCCEEDED(fogTexture->LockRect(0, &rect, nullptr, 0));
	verify(rc);
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

void D3DRenderer::writeFramebufferToVRAM()
{
	u32 width = (pvrrc.ta_GLOB_TILE_CLIP.tile_x_num + 1) * 32;
	u32 height = (pvrrc.ta_GLOB_TILE_CLIP.tile_y_num + 1) * 32;

	float xscale = pvrrc.scaler_ctl.hscale == 1 ? 0.5f : 1.f;
	float yscale = 1024.f / pvrrc.scaler_ctl.vscalefactor;
	if (std::abs(yscale - 1.f) < 0.01)
		yscale = 1.f;

	ComPtr<IDirect3DSurface9> fbSurface = framebufferSurface;
	FB_X_CLIP_type xClip = pvrrc.fb_X_CLIP;
	FB_Y_CLIP_type yClip = pvrrc.fb_Y_CLIP;

	if (xscale != 1.f || yscale != 1.f)
	{
		u32 scaledW = width * xscale;
		u32 scaledH = height * yscale;

		if (fbScaledTexture)
		{
			D3DSURFACE_DESC desc;
			fbScaledTexture->GetLevelDesc(0, &desc);
			if (desc.Width != scaledW || desc.Height != scaledH)
			{
				fbScaledTexture.reset();
				fbScaledSurface.reset();
			}
		}
		if (!fbScaledTexture)
		{
			device->CreateTexture(scaledW, scaledH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &fbScaledTexture.get(), NULL);
			fbScaledTexture->GetSurfaceLevel(0, &fbScaledSurface.get());
		}
		device->StretchRect(framebufferSurface, nullptr, fbScaledSurface, nullptr, D3DTEXF_LINEAR);

		width = scaledW;
		height = scaledH;
		fbSurface = fbScaledSurface;
		// FB_Y_CLIP is applied before vscalefactor if > 1, so it must be scaled here
		if (yscale > 1) {
			yClip.min = std::round(yClip.min * yscale);
			yClip.max = std::round(yClip.max * yscale);
		}
	}
	u32 texAddress = pvrrc.fb_W_SOF1 & VRAM_MASK; // TODO SCALER_CTL.interlace, SCALER_CTL.fieldselect
	u32 linestride = pvrrc.fb_W_LINESTRIDE * 8;

	ComPtr<IDirect3DSurface9> offscreenSurface;
	bool rc = SUCCEEDED(device->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &offscreenSurface.get(), nullptr));
	verify(rc);
	rc = SUCCEEDED(device->GetRenderTargetData(fbSurface, offscreenSurface));
	verify(rc);

	PixelBuffer<u32> tmp_buf;
	tmp_buf.init(width, height);

	u8 *p = (u8 *)tmp_buf.data();
	D3DLOCKED_RECT rect;
	RECT lockRect { 0, 0, (long)width, (long)height };
	rc = SUCCEEDED(offscreenSurface->LockRect(&rect, &lockRect, D3DLOCK_READONLY));
	verify(rc);
	if ((u32)rect.Pitch == width * sizeof(u32))
		memcpy(p, rect.pBits, width * height * sizeof(u32));
	else
	{
		u8 *src = (u8 *)rect.pBits;
		for (u32 y = 0; y < height; y++)
		{
			memcpy(p, src, width * sizeof(u32));
			src += rect.Pitch;
			p += width * sizeof(u32);
		}
	}
	rc = SUCCEEDED(offscreenSurface->UnlockRect());
	verify(rc);

	xClip.min = std::min(xClip.min, width - 1);
	xClip.max = std::min(xClip.max, width - 1);
	yClip.min = std::min(yClip.min, height - 1);
	yClip.max = std::min(yClip.max, height - 1);
	WriteFramebuffer<2, 1, 0, 3>(width, height, (u8 *)tmp_buf.data(), texAddress, pvrrc.fb_W_CTRL, linestride, xClip, yClip);
}

bool D3DRenderer::GetLastFrame(std::vector<u8>& data, int& width, int& height)
{
	if (!frameRenderedOnce || !theDXContext.isReady())
		return false;

	if (width != 0) {
		height = width / aspectRatio;
	}
	else if (height != 0) {
		width = aspectRatio * height;
	}
	else
	{
		width = this->width;
		height = this->height;
		if (config::Rotate90)
			std::swap(width, height);
		// We need square pixels for PNG
		int w = aspectRatio * height;
		if (width > w)
			height = width / aspectRatio;
		else
			width = w;
	}

	backbuffer.reset();
	device->GetRenderTarget(0, &backbuffer.get());

	// Target texture and surface
	ComPtr<IDirect3DTexture9> target;
	device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &target.get(), NULL);
	ComPtr<IDirect3DSurface9> surface;
	target->GetSurfaceLevel(0, &surface.get());
	device->SetRenderTarget(0, surface);
	// Draw
	devCache.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetPixelShader(NULL);
	device->SetVertexShader(NULL);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	glm::mat4 identity = glm::identity<glm::mat4>();
	glm::mat4 projection = glm::translate(glm::vec3(-1.f / width, 1.f / height, 0));
	if (config::Rotate90)
		projection *= glm::rotate((float)M_PI_2, glm::vec3(0, 0, 1));

	device->SetTransform(D3DTS_WORLD, (const D3DMATRIX *)&identity[0][0]);
	device->SetTransform(D3DTS_VIEW, (const D3DMATRIX *)&identity[0][0]);
	device->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX *)&projection[0][0]);

	device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
	D3DVIEWPORT9 viewport{};
	viewport.Width = width;
	viewport.Height = height;
	viewport.MaxZ = 1;
	bool rc = SUCCEEDED(device->SetViewport(&viewport));
	verify(rc);
	float coords[] {
		-1,  1, 0.5f,  0, 0,
		-1, -1, 0.5f,  0, 1,
		 1,  1, 0.5f,  1, 0,
		 1, -1, 0.5f,  1, 1,
	};
	device->SetTexture(0, framebufferTexture);
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, coords, sizeof(float) * 5);

	// Copy back
	ComPtr<IDirect3DSurface9> offscreenSurface;
	rc = SUCCEEDED(device->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &offscreenSurface.get(), nullptr));
	verify(rc);
	rc = SUCCEEDED(device->GetRenderTargetData(surface, offscreenSurface));
	verify(rc);

	D3DLOCKED_RECT rect;
	RECT lockRect { 0, 0, (long)width, (long)height };
	rc = SUCCEEDED(offscreenSurface->LockRect(&rect, &lockRect, D3DLOCK_READONLY));
	verify(rc);
	data.clear();
	data.reserve(width * height * 3);
	for (int y = 0; y < height; y++)
	{
		const u8 *src = (const u8 *)rect.pBits + y * rect.Pitch;
		for (int x = 0; x < width; x++, src += 4)
		{
			data.push_back(src[2]);
			data.push_back(src[1]);
			data.push_back(src[0]);
		}
	}
	rc = SUCCEEDED(offscreenSurface->UnlockRect());
	device->SetRenderTarget(0, backbuffer);

	return true;
}

Renderer* rend_DirectX9()
{
	return new D3DRenderer();
}
