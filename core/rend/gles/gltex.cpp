#include "glcache.h"
#include "gles.h"
#include "hw/pvr/pvr_mem.h"

#include <cstring>
#include <memory>
#include <string>

namespace
{
struct OpenGLGpuPreloadedTexture final : GpuPreloadedTexture
{
	explicit OpenGLGpuPreloadedTexture(u8 mipLevels) : GpuPreloadedTexture(mipLevels) {}
	~OpenGLGpuPreloadedTexture() override
	{
		if (texture != 0)
			glcache.DeleteTextures(1, &texture);
	}

	GLuint texture = 0;
};
}

#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGB8_ETC2
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#endif
#ifndef GL_COMPRESSED_RGBA8_ETC2_EAC
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_5x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR 0x93B1
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_5x5_KHR
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR 0x93B2
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_6x5_KHR
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR 0x93B3
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_6x6_KHR
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_8x5_KHR
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR 0x93B5
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_8x6_KHR
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR 0x93B6
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_8x8_KHR
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR 0x93B7
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_10x5_KHR
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR 0x93B8
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_10x6_KHR
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR 0x93B9
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_10x8_KHR
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR 0x93BA
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_10x10_KHR
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR 0x93BB
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_12x10_KHR
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR 0x93BC
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_12x12_KHR
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR 0x93BD
#endif
#ifndef GL_TEXTURE_BASE_LEVEL
#define GL_TEXTURE_BASE_LEVEL 0x813C
#endif
#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif

GlTextureCache TexCache;
void (TextureCacheData::*TextureCacheData::uploadToGpu)(int, int, const u8 *, bool, bool) = &TextureCacheData::UploadToGPUGl2;

static void getOpenGLTexParams(TextureType texType, u32& bytesPerPixel, GLuint& gltype, GLuint& comps, GLuint& internalFormat)
{
	comps = GL_RGBA;
	bytesPerPixel = 2;
	switch (texType)
	{
	case TextureType::_5551:
		gltype = GL_UNSIGNED_SHORT_5_5_5_1;
		internalFormat = GL_RGB5_A1;
		break;
	case TextureType::_565:
		gltype = GL_UNSIGNED_SHORT_5_6_5;
		comps = GL_RGB;
		internalFormat = GL_RGB565;
		break;
	case TextureType::_4444:
		gltype = GL_UNSIGNED_SHORT_4_4_4_4;
		internalFormat = GL_RGBA4;
		break;
	case TextureType::_8888:
		bytesPerPixel = 4;
		gltype = GL_UNSIGNED_BYTE;
		internalFormat = GL_RGBA8;
		break;
	case TextureType::_8:
		bytesPerPixel = 1;
		gltype = GL_UNSIGNED_BYTE;
		comps = gl.single_channel_format;
		internalFormat = GL_R8;
		break;
	default:
		die("Unsupported texture type");
		break;
	}
}

void TextureCacheData::UploadToGPUGl2(int width, int height, const u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
{
	if (texID == 0)
		texID = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texID);
	GLuint comps;
	GLuint gltype;
	GLuint internalFormat;
	u32 bytes_per_pixel;
	getOpenGLTexParams(tex_type, bytes_per_pixel, gltype, comps, internalFormat);

	if (mipmapsIncluded)
	{
		int mipmapLevels = 0;
		int dim = width;
		while (dim != 0) {
			mipmapLevels++;
			dim >>= 1;
		}
		for (int i = 0; i < mipmapLevels; i++) {
			glTexImage2D(GL_TEXTURE_2D, mipmapLevels - i - 1, comps, 1 << i, 1 << i, 0, comps, gltype, temp_tex_buffer);
			temp_tex_buffer += (1 << (2 * i)) * bytes_per_pixel;
		}
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0,comps, width, height, 0, comps, gltype, temp_tex_buffer);
		if (mipmapped)
			glGenerateMipmap(GL_TEXTURE_2D);
	}
}

void TextureCacheData::UploadToGPUGl4(int width, int height, const u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
{
#if !defined(GLES2) && (!defined(__APPLE__) || defined(TARGET_IPHONE))
	GLuint comps;
	GLuint gltype;
	GLuint internalFormat;
	u32 bytes_per_pixel;
	getOpenGLTexParams(tex_type, bytes_per_pixel, gltype, comps, internalFormat);

	int mipmapLevels = 1;
	if (mipmapped)
	{
		mipmapLevels = 0;
		int dim = width;
		while (dim != 0) {
			mipmapLevels++;
			dim >>= 1;
		}
	}
	if (texID == 0)
	{
		texID = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, texID);
		glTexStorage2D(GL_TEXTURE_2D, mipmapLevels, internalFormat, width, height);
	}
	else {
		glcache.BindTexture(GL_TEXTURE_2D, texID);
	}
	if (mipmapsIncluded)
	{
		for (int i = 0; i < mipmapLevels; i++) {
			glTexSubImage2D(GL_TEXTURE_2D, mipmapLevels - i - 1, 0, 0, 1 << i, 1 << i, comps, gltype, temp_tex_buffer);
			temp_tex_buffer += (1 << (2 * i)) * bytes_per_pixel;
		}
	}
	else
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, comps, gltype, temp_tex_buffer);
		if (mipmapped)
			glGenerateMipmap(GL_TEXTURE_2D);
	}
#endif
}

void TextureCacheData::UploadToGPU(int width, int height, const u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
{
	if (usingGpuPreloadedTexture)
	{
		texID = 0;
		gpuPreloadedTexture.reset();
		usingGpuPreloadedTexture = false;
		customTextureObject = false;
	}
	if (customTextureObject && texID != 0)
	{
		glcache.DeleteTextures(1, &texID);
		texID = 0;
		customTextureObject = false;
	}
	((*this).*uploadToGpu)(width, height, temp_tex_buffer, mipmapped, mipmapsIncluded);
	glCheck();
}

namespace
{
GLenum customGlFormat(NativeTextureFormat format)
{
	switch (format)
	{
	case NativeTextureFormat::Bc7Unorm: return GL_COMPRESSED_RGBA_BPTC_UNORM;
	case NativeTextureFormat::Bc7Srgb: return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
	case NativeTextureFormat::Bc1Unorm: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
	case NativeTextureFormat::Bc3Unorm: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	case NativeTextureFormat::Etc2Rgb8Unorm: return GL_COMPRESSED_RGB8_ETC2;
	case NativeTextureFormat::Etc2Rgba8Unorm: return GL_COMPRESSED_RGBA8_ETC2_EAC;
	case NativeTextureFormat::Astc4x4Unorm: return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
	case NativeTextureFormat::Astc5x4Unorm: return GL_COMPRESSED_RGBA_ASTC_5x4_KHR;
	case NativeTextureFormat::Astc5x5Unorm: return GL_COMPRESSED_RGBA_ASTC_5x5_KHR;
	case NativeTextureFormat::Astc6x5Unorm: return GL_COMPRESSED_RGBA_ASTC_6x5_KHR;
	case NativeTextureFormat::Astc6x6Unorm: return GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
	case NativeTextureFormat::Astc8x5Unorm: return GL_COMPRESSED_RGBA_ASTC_8x5_KHR;
	case NativeTextureFormat::Astc8x6Unorm: return GL_COMPRESSED_RGBA_ASTC_8x6_KHR;
	case NativeTextureFormat::Astc10x5Unorm: return GL_COMPRESSED_RGBA_ASTC_10x5_KHR;
	case NativeTextureFormat::Astc10x6Unorm: return GL_COMPRESSED_RGBA_ASTC_10x6_KHR;
	case NativeTextureFormat::Astc8x8Unorm: return GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
	case NativeTextureFormat::Astc10x8Unorm: return GL_COMPRESSED_RGBA_ASTC_10x8_KHR;
	case NativeTextureFormat::Astc10x10Unorm: return GL_COMPRESSED_RGBA_ASTC_10x10_KHR;
	case NativeTextureFormat::Astc12x10Unorm: return GL_COMPRESSED_RGBA_ASTC_12x10_KHR;
	case NativeTextureFormat::Astc12x12Unorm: return GL_COMPRESSED_RGBA_ASTC_12x12_KHR;
	default: return 0;
	}
}

}

bool TextureCacheData::UploadCustomTexture(const PreparedCustomTexture& customTexture, bool mipmapped)
{
	if (usingGpuPreloadedTexture)
	{
		texID = 0;
		gpuPreloadedTexture.reset();
		usingGpuPreloadedTexture = false;
		customTextureObject = false;
	}
	std::string validationError;
	if (!validatePreparedCustomTexture(customTexture, validationError))
		return false;
	const BlockGeometry geometry = getBlockGeometry(customTexture.nativeFormat);
	const GLenum compressedFormat = customGlFormat(customTexture.nativeFormat);
	if (geometry.compressed && compressedFormat == 0)
		return false;
	const bool generateMipmaps = mipmapped && customTexture.generateMipmaps;
	const u32 mipmapLevels = generateMipmaps
			? mipmapLevelCount(customTexture.width, customTexture.height)
			: static_cast<u32>(customTexture.levels.size());

	while (glGetError() != GL_NO_ERROR) {}
	const GLuint newTexture = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, newTexture);
	if (!gl.is_gles || gl.gl_major >= 3)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
				static_cast<GLint>(mipmapLevels - 1));
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	const bool immutableStorage = allocateImmutableTextureStorage(
			static_cast<GLsizei>(customTexture.levels.size()),
			geometry.compressed ? compressedFormat : GL_RGBA8,
			customTexture.width, customTexture.height);
	for (size_t levelIndex = 0; levelIndex < customTexture.levels.size(); ++levelIndex)
	{
		const PreparedMipLevel& level = customTexture.levels[levelIndex];
		const void *data = customTexture.bytes.data() + level.byteOffset;
		if (geometry.compressed)
		{
			if (immutableStorage)
				uploadCompressedTextureSubImage2D(static_cast<GLint>(levelIndex),
						level.width, level.height, compressedFormat,
						static_cast<GLsizei>(level.byteSize), data);
			else
				glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(levelIndex), compressedFormat,
						level.width, level.height, 0, static_cast<GLsizei>(level.byteSize), data);
		}
		else
		{
			if (immutableStorage)
				glTexSubImage2D(GL_TEXTURE_2D, static_cast<GLint>(levelIndex), 0, 0,
						level.width, level.height, GL_RGBA, GL_UNSIGNED_BYTE, data);
			else
				glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(levelIndex),
						gl.is_gles && gl.gl_major < 3 ? GL_RGBA : GL_RGBA8,
						level.width, level.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
		if (glGetError() != GL_NO_ERROR)
		{
			glcache.DeleteTextures(1, &newTexture);
			if (texID != 0)
				glcache.BindTexture(GL_TEXTURE_2D, texID);
			return false;
		}
	}
	if (generateMipmaps)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
		if (glGetError() != GL_NO_ERROR)
		{
			glcache.DeleteTextures(1, &newTexture);
			if (texID != 0)
				glcache.BindTexture(GL_TEXTURE_2D, texID);
			return false;
		}
	}
	texID = newTexture;
	customTextureObject = true;
	return true;
}

GpuPreloadedTexturePtr TextureCacheData::CreateGpuPreloadedTexture(
		const PreparedCustomTexture& customTexture)
{
	if (!gl.textureStorageSupported)
		return nullptr;
	auto texture = std::make_shared<OpenGLGpuPreloadedTexture>(
			static_cast<u8>(customTexture.generateMipmaps
					? mipmapLevelCount(customTexture.width, customTexture.height)
					: customTexture.levels.size()));
	TextureCacheData uploadedTexture({}, {}, 0);
	if (!uploadedTexture.UploadCustomTexture(customTexture, true))
		return nullptr;
	texture->texture = uploadedTexture.texID;
	uploadedTexture.texID = 0;
	uploadedTexture.customTextureObject = false;
	return texture;
}

bool TextureCacheData::UseGpuPreloadedTexture(const GpuPreloadedTexturePtr& texture)
{
	auto openGlTexture = std::dynamic_pointer_cast<OpenGLGpuPreloadedTexture>(texture);
	if (!openGlTexture)
		return false;
	texID = openGlTexture->texture;
	customTextureObject = false;
	return true;
}

CustomTextureCapabilities TextureCacheData::GetCustomTextureCapabilities()
{
	GLint maxTextureSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	CustomTextureCapabilities capabilities = CustomTextureCapabilities::rgbaOnly(
			gl.is_gles ? CustomTextureBackend::OpenGLES : CustomTextureBackend::OpenGL,
			maxTextureSize > 0 ? static_cast<u32>(maxTextureSize) : 16384);
	const bool bptc = !gl.is_gles && ((gl.gl_major > 4 || (gl.gl_major == 4 && gl.gl_minor >= 2))
			|| hasGlExtension("GL_ARB_texture_compression_bptc")
			|| hasGlExtension("GL_EXT_texture_compression_bptc"));
	const bool astc = hasGlExtension("GL_KHR_texture_compression_astc_ldr")
			|| hasGlExtension("GL_OES_texture_compression_astc");
	const bool etc2 = (gl.is_gles && gl.gl_major >= 3)
			|| (!gl.is_gles && (gl.gl_major > 4 || (gl.gl_major == 4 && gl.gl_minor >= 3)))
			|| hasGlExtension("GL_ARB_ES3_compatibility");
	const bool s3tc = hasGlExtension("GL_EXT_texture_compression_s3tc");
	const bool bc1 = s3tc || hasGlExtension("GL_EXT_texture_compression_dxt1");
	const bool bc3 = s3tc
			|| hasGlExtension("GL_EXT_texture_compression_dxt5")
			|| hasGlExtension("GL_ANGLE_texture_compression_dxt5");
	capabilities.setSupported(NativeTextureFormat::Bc7Unorm, bptc);
	capabilities.setSupported(NativeTextureFormat::Bc7Srgb, bptc);
	capabilities.setSupported(NativeTextureFormat::Bc1Unorm, bc1);
	capabilities.setSupported(NativeTextureFormat::Bc3Unorm, bc3);
	capabilities.setSupported(NativeTextureFormat::Etc2Rgb8Unorm, etc2);
	capabilities.setSupported(NativeTextureFormat::Etc2Rgba8Unorm, etc2);
	for (NativeTextureFormat format : { NativeTextureFormat::Astc4x4Unorm,
			NativeTextureFormat::Astc5x4Unorm, NativeTextureFormat::Astc5x5Unorm,
			NativeTextureFormat::Astc6x5Unorm, NativeTextureFormat::Astc6x6Unorm,
			NativeTextureFormat::Astc8x5Unorm, NativeTextureFormat::Astc8x6Unorm,
			NativeTextureFormat::Astc10x5Unorm, NativeTextureFormat::Astc10x6Unorm,
			NativeTextureFormat::Astc8x8Unorm, NativeTextureFormat::Astc10x8Unorm,
			NativeTextureFormat::Astc10x10Unorm, NativeTextureFormat::Astc12x10Unorm,
			NativeTextureFormat::Astc12x12Unorm })
		capabilities.setSupported(format, astc);
	return capabilities;
}
	
void TextureCacheData::setUploadToGPUFlavor()
{
#if !defined(GLES2) && (!defined(__APPLE__) || defined(TARGET_IPHONE))
	// OpenGL 4.2 or GLES 3.0 min
	if (gl.gl_major > 4 || (gl.gl_major == 4 && gl.gl_minor >= 2)
			|| (gl.is_gles && gl.gl_major >= 3))
		uploadToGpu = &TextureCacheData::UploadToGPUGl4;
#endif
}

bool TextureCacheData::Delete()
{
	const bool gpuPreloaded = usingGpuPreloadedTexture;
	if (!BaseTextureCacheData::Delete())
		return false;

	if (gpuPreloaded)
	{
		texID = 0;
	}
	else if (texID != 0) {
		glcache.DeleteTextures(1, &texID);
		texID = 0;
	}
	customTextureObject = false;

	return true;
}

GLuint BindRTT(bool withDepthBuffer)
{
	GLenum channels, format;
	switch(gl.rendContext->fb_W_CTRL.fb_packmode)
	{
	case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
		channels = GL_RGBA;
		format = GL_UNSIGNED_BYTE;
		break;

	case 1: //0x1   565 RGB 16 bit
		channels = GL_RGB;
		format = GL_UNSIGNED_SHORT_5_6_5;
		break;

	case 2: //0x2   4444 ARGB 16 bit
		channels = GL_RGBA;
		format = GL_UNSIGNED_BYTE;
		break;

	case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
		channels = GL_RGBA;
		format = GL_UNSIGNED_BYTE;
		break;

	case 4: //0x4   888 RGB 24 bit packed
	case 5: //0x5   0888 KRGB 32 bit    K is the value of fk_kval.
	case 6: //0x6   8888 ARGB 32 bit
		WARN_LOG(RENDERER, "Unsupported render to texture format: %d", gl.rendContext->fb_W_CTRL.fb_packmode);
		return 0;

	case 7: //7     invalid
		WARN_LOG(RENDERER, "Invalid framebuffer format: 7");
		return 0;
	}
	u32 fbw = gl.rendContext->framebufferWidth;
	u32 fbh = gl.rendContext->framebufferHeight;
	u32 texAddress = gl.rendContext->fb_W_SOF1 & VRAM_MASK;
	DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d x %d @ %06x", gl.rendContext->fb_W_CTRL.fb_packmode, gl.rendContext->fb_W_LINESTRIDE * 8,
			fbw, fbh, texAddress);

	gl.rtt.framebuffer.reset();

	// Create a texture for rendering to
	GLuint texture = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, channels, fbw, fbh, 0, channels, format, 0);

	gl.rtt.framebuffer = std::make_unique<GlFramebuffer>((int)fbw, (int)fbh, withDepthBuffer, texture);

	glViewport(0, 0, fbw, fbh);

	return gl.rtt.framebuffer->getFramebuffer();
}

void ReadRTTBuffer()
{
	u32 w = gl.rendContext->framebufferWidth;
	u32 h = gl.rendContext->framebufferHeight;

	const u8 fb_packmode = gl.rendContext->fb_W_CTRL.fb_packmode;
	const u32 tex_addr = gl.rendContext->fb_W_SOF1 & VRAM_MASK;

	if (config::RenderToTextureBuffer)
	{
#ifdef TARGET_VIDEOCORE
		// Remove all vram locks before calling glReadPixels
		// (deadlock on rpi)
		u32 size = w * h * 2;
		u32 page_tex_addr = tex_addr & ~PAGE_MASK;
		u32 page_size = size + tex_addr - page_tex_addr;
		page_size = ((page_size - 1) / PAGE_SIZE + 1) * PAGE_SIZE;
		for (u32 page = page_tex_addr; page < page_tex_addr + page_size; page += PAGE_SIZE)
			VramLockedWriteOffset(page);
#endif

		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		u16 *dst = (u16 *)&vram[tex_addr];

		u32 linestride = gl.rendContext->fb_W_LINESTRIDE * 8;
		if (linestride == 0)
			linestride = w * 2;

		GLint color_fmt, color_type;
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &color_fmt);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &color_type);

		if (fb_packmode == 1
				&& linestride == w * 2
				&& color_fmt == GL_RGB
				&& color_type == GL_UNSIGNED_SHORT_5_6_5
				&& gl.rendContext->fbClip.origin.x == 0
				&& gl.rendContext->fbClip.origin.y == 0
				&& gl.rendContext->fbClip.size.x >= (int)w
				&& gl.rendContext->fbClip.size.y >= (int)h)
		{
			glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, dst);
		}
		else
		{
			PixelBuffer<u32> tmp_buf;
			tmp_buf.init(w, h);

			u8 *p = (u8 *)tmp_buf.data();
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);

			WriteTextureToVRam(w, h, p, dst, gl.rendContext->fb_W_CTRL, linestride, gl.rendContext->fbClip);
		}
		glCheck();
	}
	else
	{
		//memset(&vram[tex_addr], 0, size);
		int wpo2, hpo2;
		getPvrFramebufferSize(*gl.rendContext, wpo2, hpo2);
		if (wpo2 <= 1024 && hpo2 <= 1024)
		{
			TextureCacheData *texture_data = TexCache.getRTTexture(tex_addr, fb_packmode, wpo2, hpo2);
			glcache.DeleteTextures(1, &texture_data->texID);
			texture_data->texID = gl.rtt.framebuffer->detachTexture();
			texture_data->dirty = 0;
			texture_data->unprotectVRam();
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);
}

BaseTextureCacheData *OpenGLRenderer::GetTexture(TSP tsp, TCW tcw, int area)
{
	//lookup texture
	TextureCacheData* tf = TexCache.getTextureCacheData(tsp, tcw, area);

	//update if needed
	if (tf->NeedsUpdate())
	{
		const GLuint oldTexture = tf->texID;
		const bool oldTextureWasGpuPreloaded = tf->usingGpuPreloadedTexture;
		if (!tf->Update())
			tf = nullptr;
		else if (tf->is_custom_replaced && !oldTextureWasGpuPreloaded
				&& oldTexture != 0 && oldTexture != tf->texID)
			TexCache.DeleteLater(oldTexture);
	}
	else if (tf->IsCustomTextureAvailable())
	{
		const GLuint oldTexture = tf->texID;
		const bool oldTextureWasGpuPreloaded = tf->usingGpuPreloadedTexture;
		if (tf->CheckCustomTexture() && !oldTextureWasGpuPreloaded
				&& oldTexture != 0 && oldTexture != tf->texID)
			TexCache.DeleteLater(oldTexture);
	}

	return tf;
}

void glReadFramebuffer(const FramebufferInfo& info)
{
	PixelBuffer<u32> pb;
	ReadFramebuffer(info, pb, gl.dcfb.width, gl.dcfb.height);
	
	if (gl.dcfb.tex == 0)
		gl.dcfb.tex = glcache.GenTexture();
	
	glcache.BindTexture(GL_TEXTURE_2D, gl.dcfb.tex);
	
	//set texture repeat mode
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gl.dcfb.width, gl.dcfb.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pb.data());
}

GLuint init_output_framebuffer(int width, int height)
{
	if (gl.ofbo.framebuffer != nullptr
			&& (width != gl.ofbo.framebuffer->getWidth() || height != gl.ofbo.framebuffer->getHeight()))
	{
		gl.ofbo.framebuffer.reset();
	}

	if (gl.ofbo.framebuffer == nullptr)
	{
		// Create a texture for rendering to
		GLuint texture = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, texture);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		gl.ofbo.framebuffer = std::make_unique<GlFramebuffer>(width, height, true, texture);

		glcache.Disable(GL_SCISSOR_TEST);
		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
		gl.ofbo.framebuffer->bind();

	glViewport(0, 0, width, height);
	glCheck();

	return gl.ofbo.framebuffer->getFramebuffer();
}

GlFramebuffer::GlFramebuffer(int width, int height, bool withDepth, GLuint texture)
	: width(width), height(height), texture(texture)
{
	if (this->texture == 0)
	{
		if (gl.gl_major < 3)
		{
			// Create a texture for rendering to
			this->texture = glcache.GenTexture();
			glcache.BindTexture(GL_TEXTURE_2D, this->texture);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		else
		{
			// Use a renderbuffer and glBlitFramebuffer
			glGenRenderbuffers(1, &colorBuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
		}
	}
	makeFramebuffer(withDepth);
}

void GlFramebuffer::makeFramebuffer(bool withDepth)
{
	// Create the framebuffer
	glGenFramebuffers(1, &framebuffer);
	bind();

	if (withDepth)
	{
		// Generate and bind a render buffer which will become a depth buffer
		glGenRenderbuffers(1, &depthBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);

		// Currently it is unknown to GL that we want our new render buffer to be a depth buffer.
		// glRenderbufferStorage will fix this and will allocate a depth buffer
		if (gl.is_gles)
		{
#if defined(GL_DEPTH24_STENCIL8)
	        if (gl.gl_major >= 3)
	            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	        else
#endif
#if defined(GL_DEPTH24_STENCIL8_OES)
	        if (gl.GL_OES_packed_depth_stencil_supported)
	            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
	        else
#endif
#if defined(GL_DEPTH_COMPONENT24_OES)
	        if (gl.GL_OES_depth24_supported)
	            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, width, height);
	        else
#endif
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
		}
#ifdef GL_DEPTH24_STENCIL8
		else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
#endif

		// Attach the depth buffer we just created to our FBO.
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

		if (!gl.is_gles || gl.gl_major >= 3 || gl.GL_OES_packed_depth_stencil_supported)
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
	}

	// Attach the texture/renderbuffer to the FBO
	if (texture != 0)
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	else
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer);
}

GlFramebuffer::GlFramebuffer(int width, int height, bool withDepth, bool withTexture)
	: width(width), height(height), texture(0)
{
	if (gl.gl_major < 3 || withTexture)
	{
		// Create a texture for rendering to
		texture = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, texture);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		// Use a renderbuffer and glBlitFramebuffer
		glGenRenderbuffers(1, &colorBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
	}

	makeFramebuffer(withDepth);
}

GlFramebuffer::~GlFramebuffer()
{
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteRenderbuffers(1, &depthBuffer);
	glcache.DeleteTextures(1, &texture);
	glDeleteRenderbuffers(1, &colorBuffer);
}

bool testBlitFramebuffer()
{
#ifdef GLES2
	return false;
#else
	GLint ofbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&ofbo);

	GLuint texture = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texture);

	u32 data[32 * 32];
	// Lower half is red
	for (int i = 0; i < 16 * 32; i++)
		data[i] = 0xFF0000FF;
	// Upper half is green
	for (int i = 16 * 32; i < 32 * 32; i++)
		data[i] = 0xFF00FF00;   // green
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	GlFramebuffer src(32, 32, false, texture);

	GlFramebuffer dest(32, 64, false, true);

	src.bind(GL_READ_FRAMEBUFFER);
	GLenum error = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
	if (error != GL_FRAMEBUFFER_COMPLETE) {
		WARN_LOG(RENDERER, "testBlitFramebuffer: Source framebuffer error %x", error);
		return false;
	}
	dest.bind(GL_DRAW_FRAMEBUFFER);
	error = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if (error != GL_FRAMEBUFFER_COMPLETE) {
		WARN_LOG(RENDERER, "testBlitFramebuffer: Destination framebuffer error %x", error);
		return false;
	}

	glcache.Disable(GL_SCISSOR_TEST);
	glcache.ClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	// Apple A8X chokes on negative coordinates
	// Many mobile GPUs don't support dstY0 > dstY1 and the resulting image is flipped vertically
	glBlitFramebuffer(0, -1, 32, 31, 0, 64, 32, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	u32 outdata[32 * 64];
	dest.bind(GL_READ_FRAMEBUFFER);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, 32, 64, GL_RGBA, GL_UNSIGNED_BYTE, outdata);
	glBindFramebuffer(GL_FRAMEBUFFER, ofbo);
	error = glGetError();
	if (error != GL_NO_ERROR) {
		WARN_LOG(RENDERER, "testBlitFramebuffer: OpenGL error %x", error);
		return false;
	}
	// Now lower half should be green (except last line due to srcY0 == -1)
	if (outdata[32 * 2] != 0xFF00FF00) {    // green
		WARN_LOG(RENDERER, "testBlitFramebuffer: Expected 0xFF00FF00 but was %08x", outdata[0]);
		return false;
	}
	// And upper half should be red
	if (outdata[31 * 64 - 1] != 0xFF0000FF) {       // red
		WARN_LOG(RENDERER, "testBlitFramebuffer: Expected 0xFF0000FF but was %08x", outdata[32 * 64 - 1]);
		return false;
	}
	return true;
#endif
}
