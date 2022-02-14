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

const D3D11_INPUT_ELEMENT_DESC MainLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, spc), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(Vertex, u),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
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

	shaders = &theDX11Context.getShaders();
	samplers = &theDX11Context.getSamplers();
	bool success = (bool)shaders->getVertexShader(true);
	ComPtr<ID3DBlob> blob = shaders->getVertexShaderBlob();
	success = success && SUCCEEDED(device->CreateInputLayout(MainLayout, ARRAY_SIZE(MainLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &mainInputLayout.get()));
	blob = shaders->getMVVertexShaderBlob();
	success = success && SUCCEEDED(device->CreateInputLayout(ModVolLayout, ARRAY_SIZE(ModVolLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &modVolInputLayout.get()));

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

	quad = std::unique_ptr<Quad>(new Quad());
	quad->init(device, deviceContext, shaders);

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
	vtxConstants.reset();
	pxlConstants.reset();
	fbTex.reset();
	fbTextureView.reset();
	fbRenderTarget.reset();
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

void DX11Renderer::Resize(int w, int h)
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
			// FIXME textureView
			tf->loadCustomTexture();
		}
	}
	return tf;
}

bool DX11Renderer::Process(TA_context* ctx)
{
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

//
// Efficient Triangle and Quadrilateral Clipping within Shaders. M. McGuire
// Journal of Graphics GPU and Game Tools � November 2011
//
static glm::vec3 intersect(const glm::vec3& A, float Adist , const glm::vec3& B, float Bdist)
{
	return (A * std::abs(Bdist) + B * std::abs(Adist)) / (std::abs(Adist) + std::abs(Bdist));
}

// Clip the triangle 'trig' with respect to the plane defined by the given point and normal vector.
static int sutherlandHodgmanClip(const glm::vec2& point, const glm::vec2& normal, ModTriangle& trig, ModTriangle& newTrig)
{
	constexpr float clipEpsilon = 0.f; //0.00001;
	constexpr float clipEpsilon2 = 0.f; //0.01;

	glm::vec3 v0(trig.x0, trig.y0, trig.z0);
	glm::vec3 v1(trig.x1, trig.y1, trig.z1);
	glm::vec3 v2(trig.x2, trig.y2, trig.z2);

	glm::vec3 dist = glm::vec3(
			glm::dot(glm::vec2(v0) - point, normal),
			glm::dot(glm::vec2(v1) - point, normal),
			glm::dot(glm::vec2(v2) - point, normal));
	if (!glm::any(glm::greaterThanEqual(dist , glm::vec3(clipEpsilon2))))
		// all clipped
		return 0;
	if (glm::all(glm::greaterThanEqual(dist , glm::vec3(-clipEpsilon))))
		// none clipped
		return 3;

	// There are either 1 or 2 vertices above the clipping plane.
	glm::bvec3 above = glm::greaterThanEqual(dist, glm::vec3(0.f));
	bool nextIsAbove;
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

static void clipModVols(List<ModifierVolumeParam>& params, std::vector<ModTriangle>& triangles)
{
	for (ModifierVolumeParam& param : params)
	{
		std::vector<ModTriangle> trigs(&pvrrc.modtrig.head()[param.first], &pvrrc.modtrig.head()[param.first + param.count]);
		std::vector<ModTriangle> nextTrigs;
		nextTrigs.reserve(trigs.size());
		for (int axis = 0; axis < 4; axis++)
		{
			glm::vec2 point;
			glm::vec2 normal;
			switch (axis)
			{
			case 0: // left
				point = glm::vec2(-6400.f, 0.f);
				normal = glm::vec2(1.f, 0.f);
				break;
			case 1: // top
				point = glm::vec2(0.f, -4800.f);
				normal = glm::vec2(0.f, 1.f);
				break;
			case 2: // right
				point = glm::vec2(7040.f, 0.f);
				normal = glm::vec2(-1.f, 0.f);
				break;
			case 3: // bottom
				point = glm::vec2(-0.f, 5280.f);
				normal = glm::vec2(0.f, -1.f);
				break;
			}

			for (ModTriangle& trig : trigs)
			{
				ModTriangle newTrig;
				int size = sutherlandHodgmanClip(point, normal, trig, newTrig);
				if (size > 0)
				{
					nextTrigs.push_back(trig);
					if (size == 4)
						nextTrigs.push_back(newTrig);
				}
			}
			std::swap(trigs, nextTrigs);
			nextTrigs.clear();
		}
		param.first = (u32)triangles.size();
		param.count = (u32)trigs.size();
		triangles.insert(triangles.end(), trigs.begin(), trigs.end());
	}
}

void DX11Renderer::configVertexShader()
{
	matrices.CalcMatrices(&pvrrc, width, height);
	setBaseScissor();

	if (pvrrc.isRTT)
	{
		prepareRttRenderTarget(FB_W_SOF1 & VRAM_MASK);
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

	verify(ensureBufferSize(vertexBuffer, D3D11_BIND_VERTEX_BUFFER, vertexBufferSize, pvrrc.verts.bytes()));
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, pvrrc.verts.head(), pvrrc.verts.bytes());
	deviceContext->Unmap(vertexBuffer, 0);

	verify(ensureBufferSize(indexBuffer, D3D11_BIND_INDEX_BUFFER, indexBufferSize, pvrrc.idx.bytes()));
	deviceContext->Map(indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, pvrrc.idx.head(), pvrrc.idx.bytes());
	deviceContext->Unmap(indexBuffer, 0);

	if (config::ModifierVolumes && pvrrc.modtrig.used())
	{
		const ModTriangle *data = nullptr;
		u32 size = 0;
#if 1
		// clip triangles
		std::vector<ModTriangle> modVolTriangles;
		modVolTriangles.reserve(pvrrc.modtrig.used());
		clipModVols(pvrrc.global_param_mvo, modVolTriangles);
		clipModVols(pvrrc.global_param_mvo_tr, modVolTriangles);
		if (!modVolTriangles.empty())
		{
			size = (u32)(modVolTriangles.size() * sizeof(ModTriangle));
			data = modVolTriangles.data();
		}
#else
		size = pvrrc.modtrig.bytes();
		data = pvrrc.modtrig.head();
#endif
		if (size > 0)
		{
			verify(ensureBufferSize(modvolBuffer, D3D11_BIND_VERTEX_BUFFER, modvolBufferSize, size));
			deviceContext->Map(modvolBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
			memcpy(mappedSubres.pData, data, size);
			deviceContext->Unmap(modvolBuffer, 0);
		}
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
	deviceContext->PSSetConstantBuffers(0, ARRAY_SIZE(buffers), buffers);
}

bool DX11Renderer::Render()
{
	u32 texAddress = FB_W_SOF1 & VRAM_MASK;

	// make sure to unbind the framebuffer view before setting it as render target
	ID3D11ShaderResourceView *nullView = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullView);
	bool is_rtt = pvrrc.isRTT;
	if (!is_rtt)
	{
		deviceContext->OMSetRenderTargets(1, &fbRenderTarget.get(), depthTexView);
		deviceContext->ClearDepthStencilView(depthTexView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
	}
	configVertexShader();

	deviceContext->IASetInputLayout(mainInputLayout);

	if (!pvrrc.isRenderFramebuffer)
	{
		uploadGeometryBuffers();

		updateFogTexture();
		updatePaletteTexture();

		setupPixelShaderConstants();

		drawStrips();
	}
	else
	{
		renderDCFramebuffer();
	}

	if (is_rtt)
	{
		readRttRenderTarget(texAddress);
	}
	else
	{
#ifndef LIBRETRO
		deviceContext->OMSetRenderTargets(1, &theDX11Context.getRenderTarget().get(), nullptr);
		renderFramebuffer();
		DrawOSD(false);
		theDX11Context.setFrameRendered();
#else
		theDX11Context.drawOverlay(width, height);
		ID3D11RenderTargetView *nullView = nullptr;
		deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
		deviceContext->PSSetShaderResources(0, 1, &fbTextureView.get());
#endif
		frameRendered = true;
		frameRenderedOnce = true;
	}

	return !is_rtt;
}

void DX11Renderer::renderDCFramebuffer()
{
	float colors[4];
	VO_BORDER_COL.getRGBColor(colors);
	colors[3] = 1.f;
	deviceContext->ClearRenderTargetView(fbRenderTarget, colors);
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);

	float bar = (width - height * 640.f / 480.f) / 2.f;
	quad->draw(dcfbTextureView, samplers->getSampler(true), nullptr, bar / width * 2.f - 1.f, -1.f, (width - bar * 2.f) / width * 2.f, 2.f);
}

void DX11Renderer::renderFramebuffer()
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
	int outwidth = settings.display.width;
	int outheight = settings.display.height;
	if (config::Rotate90)
		std::swap(outwidth, outheight);
	float renderAR = (float)width / height;
	float screenAR = (float)outwidth / outheight;
	int dy = 0;
	int dx = 0;
	if (renderAR > screenAR)
		dy = (int)roundf((outheight - outwidth / renderAR) / 2.f);
	else
		dx = (int)roundf((outwidth - outheight * renderAR) / 2.f);

	float x = 0, y = 0, w = (float)outwidth, h = (float)outheight;
	if (dx != 0)
	{
		x = (float)dx;
		w = (float)(outwidth - 2 * dx);
	}
	else
	{
		y = (float)dy;
		h = (float)(outheight - 2 * dy);
	}
	// Normalize
	x = x * 2.f / outwidth - 1.f;
	w *= 2.f / outwidth;
	y = y * 2.f / outheight - 1.f;
	h *= 2.f / outheight;
	deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	quad->draw(fbTextureView, samplers->getSampler(true), nullptr, x, y, w, h, config::Rotate90);
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

	ComPtr<ID3D11VertexShader> vertexShader = shaders->getVertexShader(gp->pcw.Gouraud);
	deviceContext->VSSetShader(vertexShader, nullptr, 0);
	ComPtr<ID3D11PixelShader> pixelShader = shaders->getShader(
			gp->pcw.Texture,
			gp->tsp.UseAlpha,
			gp->tsp.IgnoreTexA,
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
        auto sampler = samplers->getSampler(gp->tsp.FilterMode != 0 && !gpuPalette, gp->tsp.ClampU, gp->tsp.ClampV, gp->tsp.FlipU, gp->tsp.FlipV);
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
	if (SortingEnabled && !config::PerStripSorting)
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
}

template <u32 Type, bool SortingEnabled>
void DX11Renderer::drawList(const List<PolyParam>& gply, int first, int count)
{
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

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
			setRenderState<Type, SortingEnabled>(params);
			deviceContext->DrawIndexed(params->count, params->first, 0);
		}

		params++;
	}
}

void DX11Renderer::sortTriangles(int first, int count)
{
	std::vector<u32> vidx_sort;
	GenSorted(first, count, pidx_sort, vidx_sort);

	//Upload to GPU if needed
	if (pidx_sort.empty())
		return;

	const size_t bufSize = vidx_sort.size() * sizeof(u32);
	// Upload sorted index buffer
	ensureBufferSize(sortedTriIndexBuffer, D3D11_BIND_INDEX_BUFFER, sortedTriIndexBufferSize, (u32)bufSize);
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	deviceContext->Map(sortedTriIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	memcpy(mappedSubres.pData, &vidx_sort[0], bufSize);
	deviceContext->Unmap(sortedTriIndexBuffer, 0);
	deviceContext->IASetIndexBuffer(sortedTriIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

void DX11Renderer::drawSorted(bool multipass)
{
	if (pidx_sort.empty())
		return;

	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	size_t count = pidx_sort.size();

	for (u32 p = 0; p < count; p++)
	{
		const PolyParam* params = pidx_sort[p].ppid;
		if (pidx_sort[p].count > 2)
		{
			setRenderState<ListType_Translucent, true>(params);
			deviceContext->DrawIndexed(pidx_sort[p].count, pidx_sort[p].first, 0);
		}
	}
	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);

		ComPtr<ID3D11VertexShader> vertexShader = shaders->getVertexShader(true);
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

		for (u32 p = 0; p < count; p++)
		{
			const PolyParam* params = pidx_sort[p].ppid;
			if (pidx_sort[p].count > 2 && !params->isp.ZWriteDis)
			{
				setCullMode(params->isp.CullMode);
				deviceContext->DrawIndexed(pidx_sort[p].count, pidx_sort[p].first, 0);
			}
		}
	}
	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

void DX11Renderer::drawModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.used() == 0 || !config::ModifierVolumes)
		return;

	deviceContext->IASetInputLayout(modVolInputLayout);
    unsigned int stride = 3 * sizeof(float);
    unsigned int offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &modvolBuffer.get(), &stride, &offset);
	deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);

	deviceContext->VSSetShader(shaders->getMVVertexShader(), nullptr, 0);
	deviceContext->PSSetShader(shaders->getModVolShader(), nullptr, 0);

	deviceContext->RSSetScissorRects(1, &scissorRect);
	setCullMode(0);

	ModifierVolumeParam* params = &pvrrc.global_param_mvo.head()[first];

	int mod_base = -1;

	for (int cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

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

	deviceContext->DrawIndexed(4, 0, 0);
}

void DX11Renderer::drawStrips()
{
	RenderPass previous_pass {};
    for (int render_pass = 0; render_pass < pvrrc.render_passes.used(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];
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

bool DX11Renderer::RenderLastFrame()
{
	if (!frameRenderedOnce)
		return false;
	renderFramebuffer();
	return false;
}

void DX11Renderer::readDCFramebuffer()
{
	if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
		return;

	PixelBuffer<u32> pb;
	int width;
	int height;
	ReadFramebuffer<BGRAPacker>(pb, width, height);

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
}

void DX11Renderer::setBaseScissor()
{
	bool wide_screen_on = !pvrrc.isRTT && config::Widescreen && !matrices.IsClipped() && !config::Rotate90;
	if (!wide_screen_on && !pvrrc.isRenderFramebuffer)
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
		fbw = (u32)(fbw * config::RenderResolution / 480.f);
		fbh = (u32)(fbh * config::RenderResolution / 480.f);
		fbw2 = (u32)(fbw2 * config::RenderResolution / 480.f);
		fbh2 = (u32)(fbh2 * config::RenderResolution / 480.f);
	}
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
}

void DX11Renderer::readRttRenderTarget(u32 texAddress)
{
	u32 w = pvrrc.fb_X_CLIP.max + 1;
	u32 h = pvrrc.fb_Y_CLIP.max + 1;
	const u8 fb_packmode = FB_W_CTRL.fb_packmode;
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
		WriteTextureToVRam<2, 1, 0, 3>(w, h, (u8 *)tmp_buf.data(), dst);
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

			DX11Texture* texture = texCache.getTextureCacheData(tsp, tcw);
			if (!texture->texture)
				texture->Create();

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
			libCore_vramlock_Lock(texture->sa_tex, texture->sa + texture->size - 1, texture);
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

Renderer *rend_DirectX11()
{
	return new DX11Renderer();
}
