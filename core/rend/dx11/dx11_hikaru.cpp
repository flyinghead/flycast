/*
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
#include "dx11_hikaru.h"
#include "dx11context.h"
#include <algorithm>

using namespace hikaru;

// Shading record, same layout as hikaru::Shading
#define HIKARU_SHADING_CBUFFER R"(
struct Light
{
	float4 color;
	float4 ambient;
	float4 vec;
	float4 att;
	float4 spot;
};

cbuffer shading : register(b2)
{
	float4 base0;
	float4 base1;
	float4 ambient;
	float4 matSpecular;
	float4 fogColor;
	float4 fogParams;	// fog (z = 1): x density, y offset; depth cue (z = 2): x scale, y start
	int4 modes;			// lit, texture mode, light count, alternate mode
	float4 alphaTest;	// min, max, mode, double-sided
	float4 clipZ;		// near plane, near clip enabled, LOD bias, depth bias scale
	Light lights[4];
};
)"

// Vertices stay in object space. Each vertex picks its modelview (mode bit 0: the second one) and
// may leave out its rotation (bit 1) or its translation (bit 2), as Demul and Odin do.
// They are lit per pixel in eye space (looking down -Z). The position is output in clip space:
// the GPU clips at the near plane (as the Hikaru does) and interpolates with perspective correction.
static const char * const VertexShader = HIKARU_SHADING_CBUFFER R"(
struct VertexIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
	float alpha : TEXCOORD1;
	uint mode : TEXCOORD2;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float4 uv : TEXCOORD0;		// xy: texture coordinates, w: eye depth
	float4 col : COLOR0;		// a: vertex alpha
	float4 eye : TEXCOORD1;
	float4 normal : TEXCOORD2;
	float clip : SV_ClipDistance0;
};

cbuffer drawConstants : register(b1)
{
	float4x4 modelview0;
	float4x4 modelview1;
	float4x4 projection;	// eye space to screen pixels (640x480, y down) times w
	float4 drawParams;		// x: 0x501 pixel pattern
};

VertexOut main(in VertexIn vin)
{
	VertexOut vo;
	float4x4 mv = (vin.mode & 1) != 0 ? modelview1 : modelview0;
	float3 eye = vin.pos;
	float3 normal = vin.normal;
	if ((vin.mode & 2) == 0)
	{
		eye = mul((float3x3)mv, eye);
		normal = mul((float3x3)mv, normal);
	}
	if ((vin.mode & 4) == 0)
		eye += mul(mv, float4(0.f, 0.f, 0.f, 1.f)).xyz;
	float4 pos = mul(projection, float4(eye, 1.f));
	vo.pos = float4(pos.x / 320.f - pos.w, pos.w - pos.y / 240.f, 0.f, pos.w);
	vo.clip = clipZ.y != 0.f ? pos.w - clipZ.x : 1.f;
	vo.uv = float4(vin.uv, 0.f, pos.w);
	vo.col = float4(1.f, 1.f, 1.f, vin.alpha);
	vo.eye = float4(eye, 0.f);
	vo.normal = float4(normal, 0.f);
	return vo;
}
)";

static const char * const PixelShader = HIKARU_SHADING_CBUFFER R"(
Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

cbuffer drawConstants : register(b1)
{
	float4x4 modelview0;
	float4x4 modelview1;
	float4x4 projection;
	float4 drawParams;		// x: 0x501 pixel pattern
};

struct Pixel
{
	float4 pos : SV_POSITION;
	float4 uv : TEXCOORD0;
	float4 col : COLOR0;
	float4 eye : TEXCOORD1;
	float4 normal : TEXCOORD2;
	float clip : SV_ClipDistance0;
	bool frontFace : SV_IsFrontFace;
};

struct PSO
{
	float4 col : SV_TARGET;
	float z : SV_DEPTH;
};

void addLight(Light l, float3 eye, float3 N, inout float3 diffuse, inout float3 specular)
{
	float3 L;
	float atten = 1.f;
	if (l.vec.w == 1.f)
	{
		float3 delta = l.vec.xyz - eye;
		float d = length(delta);
		L = delta / max(d, 1e-6f);
		if (l.att.z == 0.f)
		{
			float f = l.att.w == 0.f ? d : l.att.w == 1.f ? d * d : l.att.w == 2.f ? 1.f / d : 1.f / (d * d);
			atten = saturate(l.att.x * (l.att.y + f));
		}
	}
	else
		L = normalize(l.vec.xyz);
	if (any(l.spot.xyz != 0.f))
		atten *= saturate(dot(L, l.spot.xyz) - l.spot.w);	// spot light: fade = saturate(L . spot.xyz - spot.w), spot.xyz = axis toward the light * scale
	float nl = max(dot(N, L), 0.f);
	diffuse += atten * (nl * l.color.rgb + l.ambient.rgb);
	if (nl > 0.f && matSpecular.w > 0.f)
	{
		float3 R = reflect(-L, N);
		float s = max(dot(R, -normalize(eye)), 0.f);
		specular += atten * pow(s, matSpecular.w) * l.color.rgb;
	}
}

PSO main(in Pixel inpix)
{
	// 0x501: only the pixels of the 4x4 pattern are drawn
	uint2 cell = uint2(inpix.pos.xy) & 3;
	if ((((uint)drawParams.x >> (cell.x + cell.y * 4)) & 1) == 0)
		discard;
	float eyeDepth = inpix.uv.w;
	float alpha = inpix.col.a;
	float4 res;
	res.a = alpha;
#if TEXTURED == 1
	float4 tex = texture0.SampleBias(sampler0, inpix.uv.xy, clipZ.z);
	if (modes.w != 0)
	{
		res.rgb = lerp(base0.rgb, tex.rgb, alpha);
		res.a = tex.a;
	}
	else if (modes.y == 0)
	{
		res.rgb = tex.rgb;
		res.a *= tex.a;
	}
	else if (modes.y == 1)
		res.rgb = lerp(base0.rgb, tex.rgb, tex.a);
	else if (modes.y == 2)
	{
		res.rgb = base1.rgb * tex.rgb;
		res.a *= tex.a;
	}
	else if (modes.y == 5)
	{
		res.rgb = lerp(base0.rgb, base1.rgb, tex.rgb);
		res.a *= tex.a;
	}
	else
	{
		res.rgb = base0.rgb;
		res.a *= tex.a;
	}
#else
	if (modes.w != 0)
	{
		res.rgb = lerp(base0.rgb, base1.rgb, alpha);
		res.a = 1.f;
	}
	else
		res.rgb = base0.rgb;
#endif
	float3 eye = inpix.eye.xyz;
	if (modes.x != 0)
	{
		float3 N = normalize(inpix.normal.xyz);
		if (alphaTest.w != 0.f && inpix.frontFace)	// the back faces of the Hikaru are the rasterizer's front faces
			N = -N;	// double-sided: the back face takes the reversed normal
		float3 diffuse = float3(0.f, 0.f, 0.f);
		float3 specular = float3(0.f, 0.f, 0.f);
		for (int i = 0; i < modes.z; i++)
			addLight(lights[i], eye, N, diffuse, specular);
		res.rgb *= ambient.rgb + diffuse;
		res.rgb += matSpecular.rgb * specular;
	}
	res = saturate(res);
	if (fogParams.z == 1.f)
	{
		// visibility: 0 = fully fogged
		float fa = saturate(fogParams.x * (eye.z + fogParams.y));
		res.rgb = lerp(fogColor.rgb, res.rgb, fa);
	}

	if (alphaTest.z == 1.f)
	{
		// alpha-tested polygons: keep the range, then opaque
		if (res.a < alphaTest.x || res.a > alphaTest.y)
			discard;
		res.a = 1.f;
	}
	else if (alphaTest.z == 2.f && res.a < 1.f / 255.f)
		discard;

	if (fogParams.z == 2.f)
	{
		// depth cue: the polygon fades into what is behind it with its eye depth (Odin)
		float cue = saturate((-eye.z - fogParams.y) * fogParams.x);
		if (cue >= 1.f)
			discard;
		res.a = 1.f - cue;
	}

	PSO pso;
	// logarithmic depth of 1 / eye depth: greater is nearer. The depth bias scales the eye depth (clipZ.w).
	pso.z = log2(1.0f + 100000.0f / max(eyeDepth * clipZ.w, 1e-6f)) / 34.0f;
	pso.col = res;
	return pso;
}
)";

// Full-screen passes: the frame blender and the final picture
static const char * const ScreenVertexShader = R"(
struct VertexOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VertexOut main(uint id : SV_VertexID)
{
	VertexOut vo;
	vo.uv = float2(id & 1, id >> 1);
	vo.pos = float4(vo.uv.x * 2.f - 1.f, 1.f - vo.uv.y * 2.f, 0.f, 1.f);
	return vo;
}
)";

#define HIKARU_SCREEN_CBUFFER R"(
Texture2D slot0 : register(t0);
Texture2D slot1 : register(t1);
Texture2D slot2 : register(t2);
SamplerState pointClamp : register(s0);
SamplerState pointWrap : register(s1);

cbuffer screenConstants : register(b0)
{
	float4 blendFactor;		// x: frame blend factor
	float4 scroll[3];		// xy: layer scroll of each slot, in texture coordinates
	int4 slotKind;			// per slot: 0 none, 1 3D picture, 2 layer
	float4 keyed3D;			// x: 1 when the 3D picture is keyed on its alpha
};
)"

// Frame blender: the source with the blend factor as alpha, blended over the target 3D buffer
static const char * const BlendPixelShader = HIKARU_SCREEN_CBUFFER R"(
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
	return float4(slot0.Sample(pointClamp, uv).rgb, blendFactor.x);
}
)";

// Final picture: the topmost slot whose pixel is not transparent wins, the bottom slot is opaque
static const char * const CompositePixelShader = HIKARU_SCREEN_CBUFFER R"(
bool sampleSlot(Texture2D tex, int kind, float2 uv, float2 offset, out float4 c)
{
	if (kind == 1)
	{
		c = tex.Sample(pointClamp, uv);
		return keyed3D.x != 0.f ? c.a != 0.f : any(c.rgb != 0.f);
	}
	c = tex.Sample(pointWrap, uv + offset);
	return kind == 2 && c.a != 0.f;
}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
	float4 c;
	if (slotKind.z != 0 && sampleSlot(slot2, slotKind.z, uv, scroll[2].xy, c))
		return float4(c.rgb, 1.f);
	if (slotKind.y != 0 && sampleSlot(slot1, slotKind.y, uv, scroll[1].xy, c))
		return float4(c.rgb, 1.f);
	if (slotKind.x != 0)
	{
		sampleSlot(slot0, slotKind.x, uv, scroll[0].xy, c);
		return float4(c.rgb, 1.f);
	}
	return float4(0.f, 0.f, 0.f, 1.f);
}
)";

struct ScreenConstants
{
	float blendFactor[4];
	float scroll[3][4];
	int slotKind[4];
	float keyed3D[4];
};

struct DrawConstants
{
	float modelview0[16];
	float modelview1[16];
	float projection[16];
	float params[4];	// 0x501 pixel pattern
};

bool HikaruRenderer::init(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext> deviceContext)
{
	this->device = device;
	this->deviceContext = deviceContext;
	bool success = true;

	pD3DCompile compile = DX11Context::Instance()->getCompiler();
	auto compileBlob = [&](const char *source, const char *profile, const D3D_SHADER_MACRO *macros) {
		ComPtr<ID3DBlob> blob, errors;
		if (FAILED(compile(source, strlen(source), nullptr, macros, nullptr, "main", profile, 0, 0, &blob.get(), &errors.get())))
		{
			ERROR_LOG(RENDERER, "Hikaru shader compilation failed: %s", errors ? (const char *)errors->GetBufferPointer() : "?");
			success = false;
		}
		return blob;
	};
	ComPtr<ID3DBlob> blob = compileBlob(VertexShader, "vs_4_0", nullptr);
	if (blob)
	{
		device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vertexShader.get());
		static const D3D11_INPUT_ELEMENT_DESC layout[] {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DrawVertex, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DrawVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(DrawVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, offsetof(DrawVertex, alpha), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 2, DXGI_FORMAT_R32_UINT, 0, offsetof(DrawVertex, mode), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(device->CreateInputLayout(layout, std::size(layout), blob->GetBufferPointer(), blob->GetBufferSize(), &inputLayout.get())))
			success = false;
	}
	for (int textured = 0; textured < 2; textured++)
	{
		const D3D_SHADER_MACRO macros[] {
			{ "TEXTURED", textured ? "1" : "0" },
			{ nullptr, nullptr }
		};
		blob = compileBlob(PixelShader, "ps_4_0", macros);
		if (blob)
			device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &pixelShaders[textured].get());
	}
	blob = compileBlob(ScreenVertexShader, "vs_4_0", nullptr);
	if (blob)
		device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &screenVertexShader.get());
	blob = compileBlob(BlendPixelShader, "ps_4_0", nullptr);
	if (blob)
		device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &blendPixelShader.get());
	blob = compileBlob(CompositePixelShader, "ps_4_0", nullptr);
	if (blob)
		device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &compositePixelShader.get());

	auto createConstants = [&](ComPtr<ID3D11Buffer>& buffer, size_t size) {
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = (UINT)((size + 15) & ~15);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(device->CreateBuffer(&desc, nullptr, &buffer.get())))
			success = false;
	};
	createConstants(drawConstants, sizeof(DrawConstants));
	createConstants(shadingConstants, sizeof(Shading));
	createConstants(screenConstants, sizeof(ScreenConstants));

	// Rasterizer: same face convention as flycast's other DX11 paths
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.FrontCounterClockwise = true;
		desc.ScissorEnable = true;
		desc.DepthClipEnable = false;
		static const D3D11_CULL_MODE modes[3] { D3D11_CULL_NONE, D3D11_CULL_FRONT, D3D11_CULL_BACK };
		for (int i = 0; i < 3; i++)
		{
			desc.CullMode = modes[i];
			device->CreateRasterizerState(&desc, &rasterizers[i].get());
		}
	}
	// Depth: greater or equal (the depth grows nearer the eye), cleared to 0
	for (int test = 0; test < 2; test++)
		for (int write = 0; write < 2; write++)
		{
			D3D11_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = TRUE;
			desc.DepthWriteMask = write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = test ? D3D11_COMPARISON_GREATER_EQUAL : D3D11_COMPARISON_ALWAYS;
			desc.StencilEnable = FALSE;
			device->CreateDepthStencilState(&desc, &depthStates[test][write].get());
		}
	// Blending: opaque, then the material blend modes (0x081 sub 5)
	{
		static const D3D11_BLEND src[5] { D3D11_BLEND_ONE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE };
		static const D3D11_BLEND dst[5] { D3D11_BLEND_ZERO, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA };
		for (int i = 0; i < 5; i++)
		{
			D3D11_BLEND_DESC desc{};
			D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];
			rt.BlendEnable = i != 0;
			rt.SrcBlend = src[i];
			rt.DestBlend = dst[i];
			rt.BlendOp = D3D11_BLEND_OP_ADD;
			// the 3D buffer keeps the alpha of the last fragment (Demul 2025): 0x1BC bit 0 keys the 3D picture on it
			rt.SrcBlendAlpha = D3D11_BLEND_ONE;
			rt.DestBlendAlpha = D3D11_BLEND_ZERO;
			rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			device->CreateBlendState(&desc, &blendStates[i].get());
		}
	}
	for (int wrap = 0; wrap < 2; wrap++)
	{
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = desc.AddressV = desc.AddressW = wrap ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MaxLOD = 0.f;
		device->CreateSamplerState(&desc, &screenSamplers[wrap].get());
	}
	if (!success)
		WARN_LOG(RENDERER, "Hikaru renderer initialization failed");
	return success;
}

void HikaruRenderer::term()
{
	textures.clear();
	vertexShader.reset();
	for (auto& ps : pixelShaders)
		ps.reset();
	inputLayout.reset();
	screenVertexShader.reset();
	blendPixelShader.reset();
	compositePixelShader.reset();
	for (auto& b : buffers)
		b = Buffer{};
	bufferWidth = bufferHeight = 0;
	drawConstants.reset();
	shadingConstants.reset();
	screenConstants.reset();
	vertexBuffer.reset();
	indexBuffer.reset();
	vertexBufferSize = indexBufferSize = 0;
	for (auto& r : rasterizers)
		r.reset();
	for (auto& t : depthStates)
		for (auto& s : t)
			s.reset();
	for (auto& b : blendStates)
		b.reset();
	for (auto& s : samplers)
		s.reset();
	for (auto& smp : screenSamplers)
		smp.reset();
	deviceContext.reset();
	device.reset();
}

// Upload a texture store entry if it changed. Called with the store locked.
bool HikaruRenderer::updateTexture(int id)
{
	TextureStore& store = textureStore();
	if (id < 0 || id >= (int)store.entries.size())
		return false;
	TextureEntry& e = store.entries[id];
	if (e.version == 0 || e.texels.empty())
		return false;
	if (id >= (int)textures.size())
		textures.resize(id + 1);
	Texture& t = textures[id];
	if (t.version == e.version)
		return true;

	if (!t.texture || t.width != e.width || t.height != e.height || t.levels != e.levels)
	{
		t.view.reset();
		t.texture.reset();
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = e.width;
		desc.Height = e.height;
		desc.MipLevels = e.levels;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		std::vector<D3D11_SUBRESOURCE_DATA> data(e.levels);
		const u32 *p = e.texels.data();
		for (u32 l = 0; l < e.levels; l++)
		{
			u32 w = std::max(e.width >> l, 1u);
			u32 h = std::max(e.height >> l, 1u);
			data[l].pSysMem = p;
			data[l].SysMemPitch = w * 4;
			p += w * h;
		}
		if (FAILED(device->CreateTexture2D(&desc, data.data(), &t.texture.get())))
		{
			WARN_LOG(RENDERER, "Hikaru texture creation failed (%dx%d)", e.width, e.height);
			t = Texture{};
			return false;
		}
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = e.levels;
		device->CreateShaderResourceView(t.texture, &viewDesc, &t.view.get());
		t.width = e.width;
		t.height = e.height;
		t.levels = e.levels;
	}
	else if (e.levels == 1 && e.dirtyY1 > e.dirtyY0)
	{
		// only the rows that changed (2D layers)
		D3D11_BOX box { 0, e.dirtyY0, 0, e.width, e.dirtyY1, 1 };
		deviceContext->UpdateSubresource(t.texture, 0, &box, &e.texels[e.dirtyY0 * e.width], e.width * 4, 0);
	}
	else
	{
		const u32 *p = e.texels.data();
		for (u32 l = 0; l < e.levels; l++)
		{
			u32 w = std::max(e.width >> l, 1u);
			u32 h = std::max(e.height >> l, 1u);
			deviceContext->UpdateSubresource(t.texture, l, nullptr, p, w * 4, 0);
			p += w * h;
		}
	}
	t.version = e.version;
	e.dirtyY0 = e.dirtyY1 = 0;
	return true;
}

ID3D11ShaderResourceView *HikaruRenderer::textureView(int id) const
{
	if (id < 0 || id >= (int)textures.size() || textures[id].version == 0)
		return nullptr;
	return textures[id].view;
}

void HikaruRenderer::uploadTextures(const DrawList& dl)
{
	TextureStore& store = textureStore();
	std::lock_guard<std::mutex> lock(store.mutex);
	// Textures freed by the emulation are released here too
	for (size_t id = 0; id < textures.size(); id++)
		if (id >= store.entries.size() || store.entries[id].version == 0)
			textures[id] = Texture{};
	int last = -1;
	for (const Draw& d : dl.draws)
		if (d.texture >= 0 && d.texture != last)
		{
			updateTexture(d.texture);
			last = d.texture;
		}
	updateTexture(dl.blend.texture);
	for (const CompositeSlot& slot : dl.composite.slots)
		updateTexture(slot.texture);
}

void HikaruRenderer::uploadGeometry(const DrawList& dl)
{
	auto upload = [&](ComPtr<ID3D11Buffer>& buffer, u32& bufferSize, UINT bind, const void *data, u32 size) {
		if (size == 0)
			return;
		if (size > bufferSize)
		{
			buffer.reset();
			u32 newSize = std::max(size, bufferSize * 2);
			D3D11_BUFFER_DESC desc{};
			desc.ByteWidth = newSize;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = bind;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			if (FAILED(device->CreateBuffer(&desc, nullptr, &buffer.get())))
			{
				bufferSize = 0;
				return;
			}
			bufferSize = newSize;
		}
		D3D11_MAPPED_SUBRESOURCE mapped;
		deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, data, size);
		deviceContext->Unmap(buffer, 0);
	};
	upload(vertexBuffer, vertexBufferSize, D3D11_BIND_VERTEX_BUFFER, dl.vertices.data(), (u32)(dl.vertices.size() * sizeof(DrawVertex)));
	upload(indexBuffer, indexBufferSize, D3D11_BIND_INDEX_BUFFER, dl.indices.data(), (u32)(dl.indices.size() * sizeof(u32)));
}

ID3D11SamplerState *HikaruRenderer::getSampler(bool linear, bool mipmapped, Address u, Address v)
{
	int idx = (((int)linear * 2 + (int)mipmapped) * 3 + (int)u) * 3 + (int)v;
	ComPtr<ID3D11SamplerState>& sampler = samplers[idx];
	if (!sampler)
	{
		static const D3D11_TEXTURE_ADDRESS_MODE address[3] { D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR };
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = address[(int)u];
		desc.AddressV = address[(int)v];
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = 0.f;
		// nearest and bilinear filtering sample the base level only (Demul)
		desc.MaxLOD = mipmapped ? D3D11_FLOAT32_MAX : 0.f;
		if (FAILED(device->CreateSamplerState(&desc, &sampler.get())))
			WARN_LOG(RENDERER, "Hikaru sampler creation failed");
	}
	return sampler;
}

void HikaruRenderer::ensureBuffers(int width, int height)
{
	if (bufferWidth == width && bufferHeight == height)
		return;
	for (Buffer& b : buffers)
	{
		b = Buffer{};
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(device->CreateTexture2D(&desc, nullptr, &b.texture.get())))
		{
			WARN_LOG(RENDERER, "Hikaru 3D buffer creation failed");
			continue;
		}
		device->CreateRenderTargetView(b.texture, nullptr, &b.renderTarget.get());
		device->CreateShaderResourceView(b.texture, nullptr, &b.view.get());
		const float black[4] {};
		deviceContext->ClearRenderTargetView(b.renderTarget, black);
	}
	bufferWidth = width;
	bufferHeight = height;
}

void HikaruRenderer::drawScreen(ID3D11PixelShader *shader, ID3D11ShaderResourceView *const views[3], const DrawList& dl)
{
	ScreenConstants c {};
	c.blendFactor[0] = dl.blend.factor;
	for (int i = 0; i < 3; i++)
	{
		const CompositeSlot& slot = dl.composite.slots[i];
		c.slotKind[i] = slot.is3D ? 1 : views[i] != nullptr ? 2 : 0;
		c.scroll[i][0] = (float)slot.scrollX / SCREEN_WIDTH;
		c.scroll[i][1] = (float)slot.scrollY / SCREEN_HEIGHT;
	}
	c.keyed3D[0] = dl.composite.keyed3D ? 1.f : 0.f;
	setConstBuffer(screenConstants, c);

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	deviceContext->IASetInputLayout(nullptr);
	deviceContext->VSSetShader(screenVertexShader, nullptr, 0);
	deviceContext->PSSetShader(shader, nullptr, 0);
	ID3D11SamplerState *smp[2] { screenSamplers[0], screenSamplers[1] };
	deviceContext->PSSetSamplers(0, 2, smp);
	deviceContext->PSSetConstantBuffers(0, 1, &screenConstants.get());
	deviceContext->PSSetShaderResources(0, 3, views);
	deviceContext->RSSetState(rasterizers[(int)Cull::None]);
	deviceContext->OMSetDepthStencilState(depthStates[0][0], 0);
	D3D11_RECT full { 0, 0, (LONG)bufferWidth, (LONG)bufferHeight };
	deviceContext->RSSetScissorRects(1, &full);
	deviceContext->Draw(4, 0);
	ID3D11ShaderResourceView *nullViews[3] {};
	deviceContext->PSSetShaderResources(0, 3, nullViews);
}

void HikaruRenderer::draw(const DrawList& dl, ID3D11RenderTargetView *renderTarget, ID3D11DepthStencilView *depth,
		int width, int height)
{
	scaleX = (float)width / SCREEN_WIDTH;
	scaleY = (float)height / SCREEN_HEIGHT;

	uploadTextures(dl);
	uploadGeometry(dl);
	ensureBuffers(width, height);
	Buffer& target = buffers[dl.target % HIKARU_3D_BUFFERS];
	if (!target.renderTarget)
		return;

	// The 3D picture goes into the target buffer, cleared with the clear color
	deviceContext->OMSetRenderTargets(1, &target.renderTarget.get(), depth);
	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	deviceContext->RSSetViewports(1, &vp);
	deviceContext->ClearRenderTargetView(target.renderTarget, dl.clearColor);
	deviceContext->ClearDepthStencilView(depth, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);

	if (!dl.passes.empty() && vertexBuffer && indexBuffer)
	{
		UINT stride = sizeof(DrawVertex);
		UINT offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
		deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->IASetInputLayout(inputLayout);
		deviceContext->VSSetShader(vertexShader, nullptr, 0);
		ID3D11Buffer *vsBuffers[] { drawConstants, shadingConstants };
		deviceContext->VSSetConstantBuffers(1, 2, vsBuffers);
		deviceContext->PSSetConstantBuffers(1, 2, vsBuffers);

		int lastShading = -1;
		int lastMatrices[3] { -1, -1, -1 };
		int lastPattern = -1;
		for (const Pass& pass : dl.passes)
		{
			// higher viewport priorities draw over lower ones
			deviceContext->ClearDepthStencilView(depth, D3D11_CLEAR_DEPTH, 0.f, 0);
			for (u32 i = pass.firstDraw; i < pass.firstDraw + pass.drawCount; i++)
			{
				const Draw& d = dl.draws[i];
				if (d.indexCount == 0 || d.clip[2] <= d.clip[0] || d.clip[3] <= d.clip[1])
					continue;
				ID3D11ShaderResourceView *view = textureView(d.texture);
				if (d.texture >= 0 && view == nullptr)
					continue;
				if (d.modelview[0] != lastMatrices[0] || d.modelview[1] != lastMatrices[1] || d.projection != lastMatrices[2]
						|| d.pattern != lastPattern)
				{
					DrawConstants c {};
					memcpy(c.modelview0, dl.matrices[d.modelview[0]].data(), sizeof(c.modelview0));
					memcpy(c.modelview1, dl.matrices[d.modelview[1]].data(), sizeof(c.modelview1));
					memcpy(c.projection, dl.matrices[d.projection].data(), sizeof(c.projection));
					c.params[0] = (float)d.pattern;
					setConstBuffer(drawConstants, c);
					lastPattern = d.pattern;
					lastMatrices[0] = d.modelview[0];
					lastMatrices[1] = d.modelview[1];
					lastMatrices[2] = d.projection;
				}
				if (d.shading != lastShading)
				{
					setConstBuffer(shadingConstants, dl.shadings[d.shading]);
					lastShading = d.shading;
				}
				deviceContext->PSSetShader(pixelShaders[view != nullptr], nullptr, 0);
				if (view != nullptr)
				{
					deviceContext->PSSetShaderResources(0, 1, &view);
					ID3D11SamplerState *sampler = getSampler(d.linear, d.mipmapped, d.addressU, d.addressV);
					deviceContext->PSSetSamplers(0, 1, &sampler);
				}
				deviceContext->RSSetState(rasterizers[(int)d.cull]);
				deviceContext->OMSetDepthStencilState(depthStates[d.depthTest][d.depthWrite], 0);
				deviceContext->OMSetBlendState(blendStates[(int)d.blend], nullptr, 0xffffffff);
				D3D11_RECT rect {
					(LONG)(d.clip[0] * scaleX + 0.5f), (LONG)(d.clip[1] * scaleY + 0.5f),
					(LONG)(d.clip[2] * scaleX + 0.5f), (LONG)(d.clip[3] * scaleY + 0.5f)
				};
				deviceContext->RSSetScissorRects(1, &rect);
				deviceContext->DrawIndexed(d.indexCount, d.firstIndex, 0);
			}
		}
	}

	// Frame blender: target = factor * source + (1 - factor) * target, target alpha = factor.
	// A source that is the target itself reads as black.
	if (dl.blend.enabled)
	{
		ID3D11ShaderResourceView *views[3] {};
		if (dl.blend.source < 2)
		{
			if (dl.blend.source != dl.target)
				views[0] = buffers[dl.blend.source].view;
		}
		else
			views[0] = textureView(dl.blend.texture);
		deviceContext->OMSetRenderTargets(1, &target.renderTarget.get(), nullptr);
		deviceContext->OMSetBlendState(blendStates[(int)Blend::AlphaOver], nullptr, 0xffffffff);
		drawScreen(blendPixelShader, views, dl);
	}

	// Final picture: the shown 3D buffer and the 2D layers, in the 0x1B8 order
	deviceContext->OMSetRenderTargets(1, &renderTarget, nullptr);
	deviceContext->OMSetBlendState(blendStates[(int)Blend::Opaque], nullptr, 0xffffffff);
	ID3D11ShaderResourceView *views[3];
	for (int i = 0; i < 3; i++)
	{
		const CompositeSlot& slot = dl.composite.slots[i];
		views[i] = slot.is3D ? buffers[dl.displayed % HIKARU_3D_BUFFERS].view.get() : textureView(slot.texture);
	}
	drawScreen(compositePixelShader, views, dl);
}
