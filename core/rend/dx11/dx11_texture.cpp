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
#include "dx11_texture.h"
#include "dx11context.h"
#include <versionhelpers.h>

void DX11Texture::UploadToGPU(int width, int height, const u8* temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
{
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	u32 bpp = 2;
	switch (tex_type)
	{
	case TextureType::_5551:
		desc.Format = DXGI_FORMAT_B5G5R5A1_UNORM;
		break;
	case TextureType::_4444:
		desc.Format = DXGI_FORMAT_B4G4R4A4_UNORM;
		break;
	case TextureType::_565:
		desc.Format = DXGI_FORMAT_B5G6R5_UNORM;
		break;
	case TextureType::_8888:
		bpp = 4;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case TextureType::_8:
		bpp = 1;
		desc.Format = DXGI_FORMAT_A8_UNORM;
		break;
	default:
		return;
	}
	int mipmapLevels = 1;
	if (mipmapsIncluded)
	{
		mipmapLevels = 0;
		int dim = width;
		while (dim != 0)
		{
			mipmapLevels++;
			dim >>= 1;
		}
	}
	desc.MipLevels = mipmapLevels;

	if (texture != nullptr)
	{
		// Recreate the texture if its dimensions or format have changed
		D3D11_TEXTURE2D_DESC curDesc;
		texture->GetDesc(&curDesc);
		if (desc.Width != curDesc.Width || desc.Height != curDesc.Height || desc.Format != curDesc.Format || desc.MipLevels != curDesc.MipLevels)
		{
			textureView.reset();
			texture.reset();
		}
	}
	if (texture == nullptr)
	{
		if (mipmapped && !mipmapsIncluded)
		{
			desc.MipLevels = 0;
			desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			mipmapLevels = 1;
		}
		if (SUCCEEDED(DX11Context::Instance()->getDevice()->CreateTexture2D(&desc, nullptr, &texture.get())))
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
			viewDesc.Format = desc.Format;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipLevels = desc.MipLevels == 0 ? -1 : desc.MipLevels;
			DX11Context::Instance()->getDevice()->CreateShaderResourceView(texture, &viewDesc, &textureView.get());
		}
		else
		{
			ERROR_LOG(RENDERER, "Texture creation failed type %d dim %dx%d mipmap %d (included %d)", tex_type, width, height, mipmapped, mipmapsIncluded);
		}
		verify(texture != nullptr);
		verify(textureView != nullptr);
	}

	for (int i = 0; i < mipmapLevels; i++)
	{
		u32 w = mipmapLevels == 1 ? width : 1 << i;
		u32 h = mipmapLevels == 1 ? height : 1 << i;
		DX11Context::Instance()->getDeviceContext()->UpdateSubresource(texture, mipmapLevels - i - 1, nullptr, temp_tex_buffer, w * bpp, w * bpp * h);
		temp_tex_buffer += (1 << (2 * i)) * bpp;
	}
	if (mipmapped && !mipmapsIncluded)
		DX11Context::Instance()->getDeviceContext()->GenerateMips(textureView);
}

#ifndef TARGET_UWP
bool DX11Texture::Force32BitTexture(TextureType type) const
{
	if (!DX11Context::Instance()->textureFormatSupported(type))
		return true;
	if (IsWindows8OrGreater())
		return false;
	// DXGI_FORMAT_B5G5R5A1_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM and DXGI_FORMAT_B5G6R5_UNORM
	// are not supported on Windows 7
	return type == TextureType::_565 || type == TextureType::_5551 || type == TextureType::_4444;
}
#endif

bool DX11Texture::Delete()
{
	if (!BaseTextureCacheData::Delete())
		return false;

	textureView.reset();
	texture.reset();
	return true;
}

bool DX11Texture::UploadCustomTexture(const PreparedCustomTexture& customTexture, bool mipmapped)
{
	std::string validationError;
	if (!validatePreparedCustomTexture(customTexture, validationError))
		return false;
	DXGI_FORMAT format;
	switch (customTexture.nativeFormat)
	{
	case NativeTextureFormat::Rgba8Unorm: format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
	case NativeTextureFormat::Bc7Unorm: format = DXGI_FORMAT_BC7_UNORM; break;
	case NativeTextureFormat::Bc7Srgb: format = DXGI_FORMAT_BC7_UNORM_SRGB; break;
	case NativeTextureFormat::Bc1Unorm: format = DXGI_FORMAT_BC1_UNORM; break;
	case NativeTextureFormat::Bc3Unorm: format = DXGI_FORMAT_BC3_UNORM; break;
	default: return false;
	}
	const bool generateMipmaps = mipmapped && customTexture.generateMipmaps;

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = customTexture.width;
	desc.Height = customTexture.height;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	ComPtr<ID3D11Texture2D> newTexture;
	ID3D11Device *device = DX11Context::Instance()->getDevice();
	if (generateMipmaps)
	{
		desc.MipLevels = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		if (FAILED(device->CreateTexture2D(&desc, nullptr, &newTexture.get())))
			return false;
	}
	else
	{
		desc.MipLevels = static_cast<UINT>(customTexture.levels.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		std::vector<D3D11_SUBRESOURCE_DATA> levels(customTexture.levels.size());
		for (size_t i = 0; i < customTexture.levels.size(); ++i)
		{
			const PreparedMipLevel& level = customTexture.levels[i];
			if (level.rowPitchBytes > UINT_MAX || level.byteSize > UINT_MAX)
				return false;
			levels[i].pSysMem = customTexture.bytes.data() + level.byteOffset;
			levels[i].SysMemPitch = level.rowPitchBytes;
			levels[i].SysMemSlicePitch = static_cast<UINT>(level.byteSize);
		}
		if (FAILED(device->CreateTexture2D(&desc, levels.data(), &newTexture.get())))
			return false;
	}
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
	viewDesc.Format = format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels = generateMipmaps ? UINT(-1) : desc.MipLevels;
	ComPtr<ID3D11ShaderResourceView> newView;
	if (FAILED(device->CreateShaderResourceView(newTexture, &viewDesc, &newView.get())))
		return false;
	if (generateMipmaps)
	{
		const PreparedMipLevel& level = customTexture.levels.front();
		ID3D11DeviceContext *context = DX11Context::Instance()->getDeviceContext();
		context->UpdateSubresource(newTexture.get(), 0, nullptr, customTexture.bytes.data(),
				level.rowPitchBytes, static_cast<UINT>(level.byteSize));
		context->GenerateMips(newView.get());
	}
	texture = std::move(newTexture);
	textureView = std::move(newView);
	return true;
}

CustomTextureCapabilities DX11Texture::GetCustomTextureCapabilities()
{
	CustomTextureCapabilities capabilities = CustomTextureCapabilities::rgbaOnly(
			CustomTextureBackend::Direct3D11, D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION);
	ID3D11Device *device = DX11Context::Instance()->getDevice();
	const auto query = [device](DXGI_FORMAT format) {
		UINT support = 0;
		return SUCCEEDED(device->CheckFormatSupport(format, &support))
				&& (support & (D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE))
					== (D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE);
	};
	capabilities.setSupported(NativeTextureFormat::Bc7Unorm, query(DXGI_FORMAT_BC7_UNORM));
	capabilities.setSupported(NativeTextureFormat::Bc7Srgb, query(DXGI_FORMAT_BC7_UNORM_SRGB));
	capabilities.setSupported(NativeTextureFormat::Bc1Unorm, query(DXGI_FORMAT_BC1_UNORM));
	capabilities.setSupported(NativeTextureFormat::Bc3Unorm, query(DXGI_FORMAT_BC3_UNORM));
	return capabilities;
}

HRESULT Samplers::createSampler(const D3D11_SAMPLER_DESC *desc, ID3D11SamplerState **sampler)
{
	return DX11Context::Instance()->getDevice()->CreateSamplerState(desc, sampler);
}
