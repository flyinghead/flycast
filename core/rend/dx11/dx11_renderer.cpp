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
#include "dx11_renderer.h"
#include "dx11context.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/gui.h"
#include "rend/tileclip.h"
#include "rend/sorter.h"

#include <memory>

const D3D11_INPUT_ELEMENT_DESC MainLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, spc), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(Vertex, u),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, nx),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
const D3D11_INPUT_ELEMENT_DESC ModVolLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(ModTriangle, x0), D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

bool DX11Renderer::Init()
{
	NOTICE_LOG(RENDERER, "DX11 renderer initializing");
	device = theDX11Context.getDevice();
	deviceContext = theDX11Context.getDeviceContext();
	if (!device || !deviceContext)
	{
		WARN_LOG(RENDERER, "Null device or device context. Aborting");
		return false;
	}

	shaders = &theDX11Context.getShaders();
	samplers = &theDX11Context.getSamplers();
	bool success = (bool)shaders->getVertexShader(true, true);
	ComPtr<ID3DBlob> blob = shaders->getVertexShaderBlob();
	success = success && SUCCEEDED(device->CreateInputLayout(MainLayout, std::size(MainLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &mainInputLayout.get()));
	blob = shaders->getMVVertexShaderBlob();
	success = success && SUCCEEDED(device->CreateInputLayout(ModVolLayout, std::size(ModVolLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &modVolInputLayout.get()));

	// Constants buffers
	{
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(VertexConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &vtxConstants.get()));

		desc.ByteWidth = sizeof(PixelConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &pxlConstants.get()));

		desc.ByteWidth = sizeof(PixelPolyConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		success = success && SUCCEEDED(device->CreateBuffer(&desc, nullptr, &pxlPolyConstants.get()));
	}

	// Rasterizer state
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.FrontCounterClockwise = true;
		desc.ScissorEnable = true;
		desc.DepthClipEnable = false;
		device->CreateRasterizerState(&desc, &rasterCullNone.get());
		desc.CullMode = D3D11_CULL_FRONT;
		device->CreateRasterizerState(&desc, &rasterCullFront.get());
		desc.CullMode = D3D11_CULL_BACK;
		device->CreateRasterizerState(&desc, &rasterCullBack.get());
	}
	// Palette texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 32;
		desc.Height = 32;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &paletteTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(paletteTexture, &viewDesc, &paletteTextureView.get());
	}
	// Fog texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 128;
		desc.Height = 2;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &fogTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(fogTexture, &viewDesc, &fogTextureView.get());
	}
	// White texture
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 8;
		desc.Height = 8;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;
		device->CreateTexture2D(&desc, nullptr, &whiteTexture.get());

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(whiteTexture, &viewDesc, &whiteTextureView.get());

		u32 texData[8 * 8];
		memset(texData, 0xff, sizeof(texData));
		deviceContext->UpdateSubresource(whiteTexture, 0, nullptr, texData, 8 * sizeof(u32), 8 * sizeof(u32) * 8);
	}

	quad = std::make_unique<Quad>();
	quad->init(device, deviceContext, shaders);
	n2Helper.init(device, deviceContext);

	fog_needs_update = true;
	forcePaletteUpdate();

	if (!success)
	{
		WARN_LOG(RENDERER, "DirectX 11 renderer initialization failed");
		Term();
	}
	frameRendered = false;

	return success;
}

void DX11Renderer::Term()
{
	NOTICE_LOG(RENDERER, "DX11 renderer terminating");
	n2Helper.term();
	vtxConstants.reset();
	pxlConstants.reset();
	fbTex.reset();
	fbTextureView.reset();
	fbRenderTarget.reset();
	fbScaledRenderTarget.reset();
	fbScaledTextureView.reset();
	fbScaledTexture.reset();
	quad.reset();
	deviceContext.reset();
	device.reset();
}

void DX11Renderer::createDepthTexAndView(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11DepthStencilView>& view, int width, int height, DXGI_FORMAT format, UINT bindFlags)
{
	view.reset();
	texture.reset();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | bindFlags;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture.get());
	if (FAILED(hr))
		WARN_LOG(RENDERER, "Depth/stencil creation failed");

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc{};
	viewDesc.Format = format == DXGI_FORMAT_R32G8X24_TYPELESS ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_D24_UNORM_S8_UINT;
	viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	hr = device->CreateDepthStencilView(texture, &viewDesc, &view.get());
	if (FAILED(hr))
		WARN_LOG(RENDERER, "Depth/stencil view creation failed");
}

void DX11Renderer::createTexAndRenderTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& renderTarget, int width, int height)
{
	texture.reset();
	renderTarget.reset();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.MipLevels = 1;

	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Framebuffer texture creation failed");
		return;
	}

	hr = device->CreateRenderTargetView(texture, nullptr, &renderTarget.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Framebuffer render target creation failed");
		return;
	}
	FLOAT black[4] = { 0.f, 0.f, 0.f, 0.f };
	deviceContext->ClearRenderTargetView(renderTarget, black);
}

void DX11Renderer::resize(int w, int h)
{
	if (width == (u32)w && height == (u32)h)
		return;
	width = w;
	height = h;

	// Create framebuffer texture
	{
		fbTextureView.reset();
		createTexAndRenderTarget(fbTex, fbRenderTarget, width, height);

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(fbTex, &viewDesc, &fbTextureView.get());
	}

	// Create depth stencil texture
	createDepthTexAndView(depthTex, depthTexView, width, height);

	frameRendered = false;
	frameRenderedOnce = false;
}

bool DX11Renderer::ensureBufferSize(ComPtr<ID3D11Buffer>& buffer, D3D11_BIND_FLAG bind, u32& currentSize, u32 minSize)
{
	if (minSize <= currentSize && buffer)
		return true;
	if (currentSize == 0)
		currentSize = minSize;
	else
		while (currentSize < minSize)
			currentSize *= 2;
	buffer.reset();
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = currentSize;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = bind;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &buffer.get()));
}

BaseTextureCacheData *DX11Renderer::GetTexture(TSP tsp, TCW tcw)
{
	//lookup texture
	DX11Texture* tf = texCache.getTextureCacheData(tsp, tcw);

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
		// FIXME textureView
		tf->loadCustomTexture();
	}
	return tf;
}

void DX11Renderer::Process(TA_context* ctx)
{
	if (KillTex)
		texCache.Clear();
	texCache.Cleanup();

	ta_parse(ctx, true);
}

void DX11Renderer::configVertexShader()
{
	matrices.CalcMatrices(&pvrrc, width, height);
	setBaseScissor();

	if (pvrrc.isRTT)
	{
		prepareRttRenderTarget(pvrrc.fb_W_SOF1 & VRAM_MASK);
	}
	else
	{
		D3D11_VIEWPORT vp{};
		vp.Width = (FLOAT)width;
		vp.Height = (FLOAT)height;
		vp.MinDepth = 0.f;
		vp.MaxDepth = 1.f;
		deviceContext->RSSetViewports(1, &vp);
	}
	VertexConstants constant{};
	memcpy(&constant.transMatrix, &matrices.GetNormalMatrix(), sizeof(constant.transMatrix));
	constant.leftPlane[0] = 1;
	constant.leftPlane[3] = 1;
	constant.rightPlane[0] = -1;
	constant.rightPlane[3] = 1;
	constant.topPlane[1] = 1;
	constant.topPlane[3] = 1;
	constant.bottomPlane[1] = -1;
	constant.bottomPlane[3] = 1;
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(vtxConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, &constant, sizeof(constant));
	deviceContext->Unmap(vtxConstants, 0);
	deviceContext->VSSetConstantBuffers(0, 1, &vtxConstants.get());
	deviceContext->GSSetShader(nullptr, nullptr, 0);
	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShader(nullptr, nullptr, 0);
}

void DX11Renderer::uploadGeometryBuffers()
{
	setFirstProvokingVertex(pvrrc);

	size_t size = pvrrc.verts.size() * sizeof(decltype(*pvrrc.verts.data()));
	bool rc = ensureBufferSize(vertexBuffer, D3D11_BIND_VERTEX_BUFFER, vertexBufferSize, size);
	verify(rc);
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, pvrrc.verts.data(), size);
	deviceContext->Unmap(vertexBuffer, 0);

	size = pvrrc.idx.size() * sizeof(decltype(*pvrrc.idx.data()));
	rc = ensureBufferSize(indexBuffer, D3D11_BIND_INDEX_BUFFER, indexBufferSize, size);
	verify(rc);
	deviceContext->Map(indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, pvrrc.idx.data(), size);
	deviceContext->Unmap(indexBuffer, 0);

	if (config::ModifierVolumes && !pvrrc.modtrig.empty())
	{
		const ModTriangle *data = &pvrrc.modtrig[0];
		size = pvrrc.modtrig.size() * sizeof(decltype(pvrrc.modtrig[0]));
		rc = ensureBufferSize(modvolBuffer, D3D11_BIND_VERTEX_BUFFER, modvolBufferSize, size);
		verify(rc);
		deviceContext->Map(modvolBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, data, size);
		deviceContext->Unmap(modvolBuffer, 0);
	}
    unsigned int stride = sizeof(Vertex);
    unsigned int offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

void DX11Renderer::setupPixelShaderConstants()
{
	PixelConstants pixelConstants;
	// VERT and RAM fog color constants
	FOG_COL_VERT.getRGBColor(pixelConstants.fog_col_vert);
	FOG_COL_RAM.getRGBColor(pixelConstants.fog_col_ram);

	// Fog density
	pixelConstants.fogDensity = FOG_DENSITY.get() * config::ExtraDepthScale;
	// Shadow scale
	pixelConstants.shadowScale = FPU_SHAD_SCALE.scale_factor / 256.f;

	// Color clamping
	pvrrc.fog_clamp_min.getRGBAColor(pixelConstants.colorClampMin);
	pvrrc.fog_clamp_max.getRGBAColor(pixelConstants.colorClampMax);

	// Punch-through alpha ref
	pixelConstants.alphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(pxlConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, &pixelConstants, sizeof(pixelConstants));
	deviceContext->Unmap(pxlConstants, 0);
	ID3D11Buffer *buffers[] { pxlConstants, pxlPolyConstants };
	deviceContext->PSSetConstantBuffers(0, std::size(buffers), buffers);
}

bool DX11Renderer::Render()
{
	// make sure to unbind the framebuffer view before setting it as render target
	ID3D11ShaderResourceView *nullView = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullView);
	bool is_rtt = pvrrc.isRTT;
	if (!is_rtt)
	{
		resize(pvrrc.framebufferWidth, pvrrc.framebufferHeight);
		deviceContext->OMSetRenderTargets(1, &fbRenderTarget.get(), depthTexView);
		deviceContext->ClearDepthStencilView(depthTexView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
	}
	configVertexShader();

	deviceContext->IASetInputLayout(mainInputLayout);

	n2Helper.resetCache();
	uploadGeometryBuffers();

	updateFogTexture();
	updatePaletteTexture();

	setupPixelShaderConstants();

	drawStrips();

	if (is_rtt)
	{
		readRttRenderTarget(pvrrc.fb_W_SOF1 & VRAM_MASK);
	}
	else if (config::EmulateFramebuffer)
	{
		writeFramebufferToVRAM();
	}
	else
	{
		aspectRatio = getOutputFramebufferAspectRatio();
#ifndef LIBRETRO
		deviceContext->OMSetRenderTargets(1, &theDX11Context.getRenderTarget().get(), nullptr);
		displayFramebuffer();
		DrawOSD(false);
		theDX11Context.setFrameRendered();
#else
		ID3D11RenderTargetView *nullView = nullptr;
		deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
		theDX11Context.presentFrame(fbTextureView, width, height);
#endif
		frameRendered = true;
		frameRenderedOnce = true;
	}

	return !is_rtt;
}

void DX11Renderer::displayFramebuffer()
{
#ifndef LIBRETRO
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)settings.display.width;
	vp.Height = (FLOAT)settings.display.height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);

	const D3D11_RECT r = { 0, 0, settings.display.width, settings.display.height };
	deviceContext->RSSetScissorRects(1, &r);
	float colors[4];
	VO_BORDER_COL.getRGBColor(colors);
	colors[3] = 1.f;
	deviceContext->ClearRenderTargetView(theDX11Context.getRenderTarget(), colors);

	float shiftX, shiftY;
	getVideoShift(shiftX, shiftY);
	shiftX *=  2.f / width;
	shiftY *=  -2.f / height;

	int outwidth = settings.display.width;
	int outheight = settings.display.height;
	float renderAR = aspectRatio;
	if (config::Rotate90) {
		std::swap(outwidth, outheight);
		std::swap(shiftX, shiftY);
		renderAR = 1 / renderAR;
	}
	float screenAR = (float)outwidth / outheight;
	int dy = 0;
	int dx = 0;
	if (renderAR > screenAR)
		dy = (int)roundf(outheight * (1 - screenAR / renderAR) / 2.f);
	else
		dx = (int)roundf(outwidth * (1 - renderAR / screenAR) / 2.f);

	float x = (float)dx;
	float y = (float)dy;
	float w = (float)(outwidth - 2 * dx);
	float h = (float)(outheight - 2 * dy);

	// Normalize
	x = x * 2.f / outwidth - 1.f;
	w *= 2.f / outwidth;
	y = y * 2.f / outheight - 1.f;
	h *= 2.f / outheight;
	// Shift
	x += shiftX;
	y += shiftY;
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	quad->draw(fbTextureView, samplers->getSampler(config::TextureFiltering != 1), nullptr, x, y, w, h, config::Rotate90);
#endif
}

void DX11Renderer::setCullMode(int mode)
{
	ComPtr<ID3D11RasterizerState> rasterizer;
	switch (mode)
	{
	case 0:
	case 1:
	default:
		rasterizer = rasterCullNone;
		break;
	case 2:
		rasterizer = rasterCullFront;
		break;
	case 3:
		rasterizer = rasterCullBack;
		break;
	}
	deviceContext->RSSetState(rasterizer);
}

template <u32 Type, bool SortingEnabled>
void DX11Renderer::setRenderState(const PolyParam *gp)
{
	PixelPolyConstants constants;
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1 && Type != ListType_Punch_Through && gp->tcw.MipMapped == 1)
	{
		constants.trilinearAlpha = 0.25f * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			constants.trilinearAlpha = 1.f - constants.trilinearAlpha;
	}
	else
		constants.trilinearAlpha = 1.f;

	bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min.full != 0 || pvrrc.fog_clamp_max.full != 0xffffffff);
	int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;

	int clip_rect[4] = {};
	TileClipping clipmode = GetTileClip(gp->tileclip, matrices.GetViewportMatrix(), clip_rect);
	DX11Texture *texture = (DX11Texture *)gp->texture;
	bool gpuPalette = texture != nullptr ? texture->gpuPalette : false;

	ComPtr<ID3D11VertexShader> vertexShader = shaders->getVertexShader(gp->pcw.Gouraud, gp->isNaomi2());
	deviceContext->VSSetShader(vertexShader, nullptr, 0);
	ComPtr<ID3D11PixelShader> pixelShader = shaders->getShader(
			gp->pcw.Texture,
			gp->tsp.UseAlpha,
			gp->tsp.IgnoreTexA || gp->tcw.PixelFmt == Pixel565,
			gp->tsp.ShadInstr,
			gp->pcw.Offset,
			fog_ctrl,
			gp->tcw.PixelFmt == PixelBumpMap,
			color_clamp,
			constants.trilinearAlpha != 1.f,
			gpuPalette,
			gp->pcw.Gouraud,
			Type == ListType_Punch_Through,
			clipmode == TileClipping::Inside,
			gp->pcw.Texture && gp->tsp.FilterMode == 0 && !gp->tsp.ClampU && !gp->tsp.ClampV && !gp->tsp.FlipU && !gp->tsp.FlipV);
	deviceContext->PSSetShader(pixelShader, nullptr, 0);

	if (gpuPalette)
	{
		if (gp->tcw.PixelFmt == PixelPal4)
			constants.paletteIndex = (float)(gp->tcw.PalSelect << 4);
		else
			constants.paletteIndex = (float)((gp->tcw.PalSelect >> 4) << 8);
	}

	if (clipmode == TileClipping::Outside)
	{
		RECT rect { clip_rect[0], clip_rect[1], clip_rect[0] + clip_rect[2], clip_rect[1] + clip_rect[3] };
		deviceContext->RSSetScissorRects(1, &rect);
	}
	else
	{
		deviceContext->RSSetScissorRects(1, &scissorRect);
		if (clipmode == TileClipping::Inside)
		{
			constants.clipTest[0] = (float)clip_rect[0];
			constants.clipTest[1] = (float)clip_rect[1];
			constants.clipTest[2] = (float)(clip_rect[0] + clip_rect[2]);
			constants.clipTest[3] = (float)(clip_rect[1] + clip_rect[3]);
		}
	}
	if (constants.trilinearAlpha != 1.f || gpuPalette || clipmode == TileClipping::Inside)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		deviceContext->Map(pxlPolyConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, &constants, sizeof(constants));
		deviceContext->Unmap(pxlPolyConstants, 0);
	}

	if (texture != nullptr)
	{
        deviceContext->PSSetShaderResources(0, 1, &texture->textureView.get());
		bool linearFiltering;
		if (config::TextureFiltering == 0)
			linearFiltering = gp->tsp.FilterMode != 0 && !gpuPalette;
		else if (config::TextureFiltering == 1)
			linearFiltering = false;
		else
			linearFiltering = true;
        auto sampler = samplers->getSampler(linearFiltering, gp->tsp.ClampU, gp->tsp.ClampV, gp->tsp.FlipU, gp->tsp.FlipV);
        deviceContext->PSSetSamplers(0, 1, &sampler.get());
	}

	// Apparently punch-through polys support blending, or at least some combinations
	if (Type == ListType_Translucent || Type == ListType_Punch_Through)
		deviceContext->OMSetBlendState(blendStates.getState(true, gp->tsp.SrcInstr, gp->tsp.DstInstr), nullptr, 0xffffffff);
	else
		deviceContext->OMSetBlendState(blendStates.getState(false, gp->tsp.SrcInstr, gp->tsp.DstInstr), nullptr, 0xffffffff);

	setCullMode(gp->isp.CullMode);

	//set Z mode, only if required
	int zfunc;
	if (Type == ListType_Punch_Through || (Type == ListType_Translucent && SortingEnabled))
		zfunc = 6; // GEQ
	else
		zfunc = gp->isp.DepthMode;

	bool zwriteEnable;
	if (SortingEnabled /* && !config::PerStripSorting */)
		zwriteEnable = false;
	else
	{
		// Z Write Disable seems to be ignored for punch-through.
		// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
		if (Type == ListType_Punch_Through)
			zwriteEnable = true;
		else
			zwriteEnable = !gp->isp.ZWriteDis;
	}
	const u32 stencil = (gp->pcw.Shadow != 0) ? 0x80 : 0;
	deviceContext->OMSetDepthStencilState(depthStencilStates.getState(true, zwriteEnable, zfunc, config::ModifierVolumes), stencil);

	if (gp->isNaomi2())
		n2Helper.setConstants(*gp, 0, pvrrc); // poly number only used in OIT
}

template <u32 Type, bool SortingEnabled>
void DX11Renderer::drawList(const std::vector<PolyParam>& gply, int first, int count)
{
	if (count == 0)
		return;
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	const PolyParam* params = &gply[first];

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
			setRenderState<Type, SortingEnabled>(params);
			deviceContext->DrawIndexed(params->count, params->first, 0);
		}

		params++;
	}
}

void DX11Renderer::drawSorted(int first, int count, bool multipass)
{
	if (count == 0)
		return;
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	int end = first + count;
	for (int p = first; p < end; p++)
	{
		const PolyParam* params = &pvrrc.global_param_tr[pvrrc.sortedTriangles[p].polyIndex];
		setRenderState<ListType_Translucent, true>(params);
		deviceContext->DrawIndexed(pvrrc.sortedTriangles[p].count, pvrrc.sortedTriangles[p].first, 0);
	}
	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);

		ComPtr<ID3D11VertexShader> vertexShader = shaders->getVertexShader(true, settings.platform.isNaomi2());
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		ComPtr<ID3D11PixelShader> pixelShader = shaders->getShader(
				false,
				false,
				false,
				0,
				false,
				2,
				false,
				false,
				false,
				false,
				true,
				false,
				false,
				false);
		deviceContext->PSSetShader(pixelShader, nullptr, 0);

		// Enable depth test, enable depth write, >=, disable stencil
		deviceContext->OMSetDepthStencilState(depthStencilStates.getState(true, true, 6, false), 0);
		deviceContext->RSSetScissorRects(1, &scissorRect);

		for (int p = first; p < end; p++)
		{
			const PolyParam* params = &pvrrc.global_param_tr[pvrrc.sortedTriangles[p].polyIndex];
			if (!params->isp.ZWriteDis)
			{
				setCullMode(params->isp.CullMode);
				deviceContext->DrawIndexed(pvrrc.sortedTriangles[p].count, pvrrc.sortedTriangles[p].first, 0);
			}
		}
	}
}

void DX11Renderer::drawModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.empty() || !config::ModifierVolumes)
		return;

	deviceContext->IASetInputLayout(modVolInputLayout);
    unsigned int stride = 3 * sizeof(float);
    unsigned int offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &modvolBuffer.get(), &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);

	deviceContext->PSSetShader(shaders->getModVolShader(), nullptr, 0);

	deviceContext->RSSetScissorRects(1, &scissorRect);
	setCullMode(0);

	const ModifierVolumeParam *params = &pvrrc.global_param_mvo[first];

	int mod_base = -1;
	int curMVMat = -1;
	int curProjMat = -1;

	for (int cmv = 0; cmv < count; cmv++)
	{
		const ModifierVolumeParam& param = params[cmv];

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

		if (param.isNaomi2() && (param.mvMatrix != curMVMat || param.projMatrix != curProjMat))
		{
			curMVMat = param.mvMatrix;
			curProjMat = param.projMatrix;
			n2Helper.setConstants(pvrrc.matrices[param.mvMatrix].mat, pvrrc.matrices[param.projMatrix].mat);
		}
		deviceContext->VSSetShader(shaders->getMVVertexShader(param.isNaomi2()), nullptr, 0);
		if (!param.isp.VolumeLast && mv_mode > 0)
			// OR'ing (open volume or quad)
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Or), 2);
		else
			// XOR'ing (closed volume)
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Xor), 0);

		if (param.count > 0)
		{
			setCullMode(param.isp.CullMode);
			deviceContext->Draw(param.count * 3, param.first * 3);
		}

		if (mv_mode == 1 || mv_mode == 2)
		{
			// Sum the area
			deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(mv_mode == 1 ? DepthStencilStates::Inclusion : DepthStencilStates::Exclusion), 1);
			deviceContext->Draw((param.first + param.count - mod_base) * 3, mod_base * 3);
			mod_base = -1;
		}
	}
	//disable culling
	setCullMode(0);
	//enable color writes
	deviceContext->OMSetBlendState(blendStates.getState(true, 4, 5), nullptr, 0xffffffff);

	//black out any stencil with '1'
	//only pixels that are Modvol enabled, and in area 1
	deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Final), 0x81);

	deviceContext->IASetInputLayout(mainInputLayout);
    stride = sizeof(Vertex);
    offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Use the background poly as a quad
	deviceContext->VSSetShader(shaders->getMVVertexShader(false), nullptr, 0);
	deviceContext->DrawIndexed(4, 0, 0);
}

void DX11Renderer::drawStrips()
{
	RenderPass previous_pass {};
    for (int render_pass = 0; render_pass < (int)pvrrc.render_passes.size(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes[render_pass];
        u32 op_count = current_pass.op_count - previous_pass.op_count;
        u32 pt_count = current_pass.pt_count - previous_pass.pt_count;
        u32 tr_count = current_pass.tr_count - previous_pass.tr_count;
        u32 mvo_count = current_pass.mvo_count - previous_pass.mvo_count;
        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d autosort %d", render_pass + 1,
        		op_count, pt_count, tr_count, mvo_count, current_pass.autosort);

		drawList<ListType_Opaque, false>(pvrrc.global_param_op, previous_pass.op_count, op_count);

		drawList<ListType_Punch_Through, false>(pvrrc.global_param_pt, previous_pass.pt_count, pt_count);

		drawModVols(previous_pass.mvo_count, mvo_count);

		if (current_pass.autosort)
		{
			if (!config::PerStripSorting)
				drawSorted(previous_pass.sorted_tr_count, current_pass.sorted_tr_count - previous_pass.sorted_tr_count, render_pass < (int)pvrrc.render_passes.size() - 1);
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

bool DX11Renderer::RenderLastFrame()
{
	if (!frameRenderedOnce)
		return false;
	displayFramebuffer();
	return true;
}

void DX11Renderer::RenderFramebuffer(const FramebufferInfo& info)
{
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
		D3D11_TEXTURE2D_DESC desc;
		dcfbTexture->GetDesc(&desc);
		if ((int)desc.Width != width || (int)desc.Height != height)
		{
			dcfbTexture.reset();
			dcfbTextureView.reset();
		}
	}
	if (!dcfbTexture)
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;

		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &dcfbTexture.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "DC Framebuffer texture creation failed");
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		hr = device->CreateShaderResourceView(dcfbTexture, &viewDesc, &dcfbTextureView.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "DC Framebuffer texture view creation failed");
	}
	deviceContext->UpdateSubresource(dcfbTexture, 0, nullptr, pb.data(), width * sizeof(u32), width * sizeof(u32) * height);

#ifndef LIBRETRO
	ID3D11ShaderResourceView *nullResView = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullResView);
	resize(width, height);
	deviceContext->OMSetRenderTargets(1, &fbRenderTarget.get(), nullptr);
	float colors[4];
	info.vo_border_col.getRGBColor(colors);
	colors[3] = 1.f;
	deviceContext->ClearRenderTargetView(fbRenderTarget, colors);
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)this->width;
	vp.Height = (FLOAT)this->height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	const D3D11_RECT r = { 0, 0, (LONG)this->width, (LONG)this->height };
	deviceContext->RSSetScissorRects(1, &r);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	deviceContext->GSSetShader(nullptr, nullptr, 0);
	deviceContext->HSSetShader(nullptr, nullptr, 0);
	deviceContext->DSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShader(nullptr, nullptr, 0);

	quad->draw(dcfbTextureView, samplers->getSampler(true));

	aspectRatio = getDCFramebufferAspectRatio();

	deviceContext->OMSetRenderTargets(1, &theDX11Context.getRenderTarget().get(), nullptr);
	displayFramebuffer();
	DrawOSD(false);
	theDX11Context.setFrameRendered();
#else
	ID3D11RenderTargetView *nullView = nullptr;
	deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
	theDX11Context.presentFrame(dcfbTextureView, width, height);
#endif
	frameRendered = true;
	frameRenderedOnce = true;
}

void DX11Renderer::setBaseScissor()
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

				float borderColor[4];
				VO_BORDER_COL.getRGBColor(borderColor);
				borderColor[3] = 1.f;
				D3D11_VIEWPORT vp{};
				vp.MaxDepth = 1.f;
				vp.Width = scaled_offs_x;
				vp.Height = (float)height;
				deviceContext->RSSetViewports(1, &vp);
				quad->draw(whiteTextureView, samplers->getSampler(false), borderColor);

				vp.TopLeftX = width - scaled_offs_x;
				vp.Width = scaled_offs_x + 1;
				deviceContext->RSSetViewports(1, &vp);
				quad->draw(whiteTextureView, samplers->getSampler(false), borderColor);
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
	}
	else
	{
		scissorEnable = false;
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = width;
		scissorRect.bottom = height;
	}
	deviceContext->RSSetScissorRects(1, &scissorRect);
}

void DX11Renderer::prepareRttRenderTarget(u32 texAddress)
{
	u32 fbw = pvrrc.getFramebufferWidth();
	u32 fbh = pvrrc.getFramebufferHeight();
	DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d x %d @ %06x",
			pvrrc.fb_W_CTRL.fb_packmode, pvrrc.fb_W_LINESTRIDE * 8, fbw, fbh, texAddress);
	u32 fbw2;
	u32 fbh2;
	getRenderToTextureDimensions(fbw, fbh, fbw2, fbh2);

	createTexAndRenderTarget(rttTexture, rttRenderTarget, fbw2, fbh2);
	createDepthTexAndView(rttDepthTex, rttDepthTexView, fbw2, fbh2);
	deviceContext->ClearDepthStencilView(rttDepthTexView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
	deviceContext->OMSetRenderTargets(1, &rttRenderTarget.get(), rttDepthTexView);

	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)fbw;
	vp.Height = (FLOAT)fbh;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	setRTTSize(fbw2, fbh2);
}

void DX11Renderer::readRttRenderTarget(u32 texAddress)
{
	u32 w = pvrrc.getFramebufferWidth();
	u32 h = pvrrc.getFramebufferHeight();
	if (config::RenderToTextureBuffer)
	{
		D3D11_TEXTURE2D_DESC desc;
		rttTexture->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		ComPtr<ID3D11Texture2D> stagingTex;
		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &stagingTex.get());
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "Staging RTT texture creation failed");
			return;
		}
		deviceContext->CopyResource(stagingTex, rttTexture);

		PixelBuffer<u32> tmp_buf;
		tmp_buf.init(w, h);
		u8 *p = (u8 *)tmp_buf.data();

		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		hr = deviceContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mappedSubres);
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "Failed to map staging RTT texture");
			return;
		}
		if (w * sizeof(u32) == mappedSubres.RowPitch)
			memcpy(p, mappedSubres.pData, w * h * sizeof(u32));
		else
		{
			u8 *src = (u8 *)mappedSubres.pData;
			for (u32 y = 0; y < h; y++)
			{
				memcpy(p, src, w * sizeof(u32));
				p += w * sizeof(u32);
				src += mappedSubres.RowPitch;
			}
		}
		deviceContext->Unmap(stagingTex, 0);

		u16 *dst = (u16 *)&vram[texAddress];
		WriteTextureToVRam<2, 1, 0, 3>(w, h, (u8 *)tmp_buf.data(), dst, pvrrc.fb_W_CTRL, pvrrc.fb_W_LINESTRIDE * 8);
	}
	else
	{
		//memset(&vram[gl.rtt.texAddress], 0, size);
		if (w <= 1024 && h <= 1024)
		{
			DX11Texture* texture = texCache.getRTTexture(texAddress, pvrrc.fb_W_CTRL.fb_packmode, w, h);

			texture->texture = rttTexture;
			rttTexture.reset();
			rttRenderTarget.reset();
			texture->textureView.reset();
			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(texture->texture, &viewDesc, &texture->textureView.get());

			texture->dirty = 0;
			texture->unprotectVRam();
		}
	}
}

void DX11Renderer::updatePaletteTexture()
{
	if (!palette_updated)
		return;
	palette_updated = false;

	deviceContext->UpdateSubresource(paletteTexture, 0, nullptr, palette32_ram, 32 * sizeof(u32), 32 * sizeof(u32) * 32);

    deviceContext->PSSetShaderResources(1, 1, &paletteTextureView.get());
    deviceContext->PSSetSamplers(1, 1, &samplers->getSampler(false).get());
}

void DX11Renderer::updateFogTexture()
{
	if (!fog_needs_update || !config::Fog)
		return;
	fog_needs_update = false;
	u8 temp_tex_buffer[256];
	MakeFogTexture(temp_tex_buffer);

	deviceContext->UpdateSubresource(fogTexture, 0, nullptr, temp_tex_buffer, 128, 128 * 2);

    deviceContext->PSSetShaderResources(2, 1, &fogTextureView.get());
    deviceContext->PSSetSamplers(2, 1, &samplers->getSampler(true).get());
}

void DX11Renderer::DrawOSD(bool clear_screen)
{
#ifndef LIBRETRO
	theDX11Context.setOverlay(!clear_screen);
	gui_display_osd();
	theDX11Context.setOverlay(false);
#endif
}

void DX11Renderer::writeFramebufferToVRAM()
{
	u32 width = (pvrrc.ta_GLOB_TILE_CLIP.tile_x_num + 1) * 32;
	u32 height = (pvrrc.ta_GLOB_TILE_CLIP.tile_y_num + 1) * 32;

	float xscale = pvrrc.scaler_ctl.hscale == 1 ? 0.5f : 1.f;
	float yscale = 1024.f / pvrrc.scaler_ctl.vscalefactor;
	if (std::abs(yscale - 1.f) < 0.01)
		yscale = 1.f;

	ComPtr<ID3D11Texture2D> fbTexture = fbTex;
	FB_X_CLIP_type xClip = pvrrc.fb_X_CLIP;
	FB_Y_CLIP_type yClip = pvrrc.fb_Y_CLIP;

	if (xscale != 1.f || yscale != 1.f)
	{
		u32 scaledW = width * xscale;
		u32 scaledH = height * yscale;

		if (fbScaledTexture)
		{
			D3D11_TEXTURE2D_DESC desc;
			fbScaledTexture->GetDesc(&desc);
			if (desc.Width != scaledW || desc.Height != scaledH)
			{
				fbScaledTexture.reset();
				fbScaledTextureView.reset();
				fbScaledRenderTarget.reset();
			}
		}
		if (!fbScaledTexture)
		{
			createTexAndRenderTarget(fbScaledTexture, fbScaledRenderTarget, scaledW, scaledH);

			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(fbScaledTexture, &viewDesc, &fbScaledTextureView.get());
		}
		D3D11_VIEWPORT vp{};
		vp.Width = (FLOAT)width;
		vp.Height = (FLOAT)height;
		vp.MinDepth = 0.f;
		vp.MaxDepth = 1.f;
		deviceContext->RSSetViewports(1, &vp);
		deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
		quad->draw(fbTextureView, samplers->getSampler(true));

		width = scaledW;
		height = scaledH;
		fbTexture = fbScaledTexture;
		// FB_Y_CLIP is applied before vscalefactor if > 1, so it must be scaled here
		if (yscale > 1) {
			yClip.min = std::round(yClip.min * yscale);
			yClip.max = std::round(yClip.max * yscale);
		}
	}
	u32 texAddress = pvrrc.fb_W_SOF1 & VRAM_MASK; // TODO SCALER_CTL.interlace, SCALER_CTL.fieldselect
	u32 linestride = pvrrc.fb_W_LINESTRIDE * 8;

	D3D11_TEXTURE2D_DESC desc;
	fbTexture->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	ComPtr<ID3D11Texture2D> stagingTex;
	HRESULT hr = device->CreateTexture2D(&desc, nullptr, &stagingTex.get());
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Staging RTT texture creation failed");
		return;
	}
	deviceContext->CopyResource(stagingTex, fbTexture);

	PixelBuffer<u32> tmp_buf;
	tmp_buf.init(width, height);
	u8 *p = (u8 *)tmp_buf.data();

	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	hr = deviceContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mappedSubres);
	if (FAILED(hr))
	{
		WARN_LOG(RENDERER, "Failed to map staging RTT texture");
		return;
	}
	if (width * sizeof(u32) == mappedSubres.RowPitch)
		memcpy(p, mappedSubres.pData, width * height * sizeof(u32));
	else
	{
		u8 *src = (u8 *)mappedSubres.pData;
		for (u32 y = 0; y < height; y++)
		{
			memcpy(p, src, width * sizeof(u32));
			p += width * sizeof(u32);
			src += mappedSubres.RowPitch;
		}
	}
	deviceContext->Unmap(stagingTex, 0);

	xClip.min = std::min(xClip.min, width - 1);
	xClip.max = std::min(xClip.max, width - 1);
	yClip.min = std::min(yClip.min, height - 1);
	yClip.max = std::min(yClip.max, height - 1);
	WriteFramebuffer<2, 1, 0, 3>(width, height, (u8 *)tmp_buf.data(), texAddress, pvrrc.fb_W_CTRL, linestride, xClip, yClip);
}

Renderer *rend_DirectX11()
{
	return new DX11Renderer();
}
