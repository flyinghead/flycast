#include "glcache.h"
#include "gles.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/TexCache.h"

#include <memory>

GlTextureCache TexCache;

void TextureCacheData::UploadToGPU(int width, int height, const u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
{
	//upload to OpenGL !
	glcache.BindTexture(GL_TEXTURE_2D, texID);
	GLuint comps = tex_type == TextureType::_8 ? gl.single_channel_format : GL_RGBA;
	GLuint gltype;
	u32 bytes_per_pixel = 2;
	switch (tex_type)
	{
	case TextureType::_5551:
		gltype = GL_UNSIGNED_SHORT_5_5_5_1;
		break;
	case TextureType::_565:
		gltype = GL_UNSIGNED_SHORT_5_6_5;
		comps = GL_RGB;
		break;
	case TextureType::_4444:
		gltype = GL_UNSIGNED_SHORT_4_4_4_4;
		break;
	case TextureType::_8888:
		bytes_per_pixel = 4;
		gltype = GL_UNSIGNED_BYTE;
		break;
	case TextureType::_8:
		bytes_per_pixel = 1;
		gltype = GL_UNSIGNED_BYTE;
		break;
	default:
		die("Unsupported texture type");
		gltype = 0;
		break;
	}
	if (mipmapsIncluded)
	{
		int mipmapLevels = 0;
		int dim = width;
		while (dim != 0)
		{
			mipmapLevels++;
			dim >>= 1;
		}
#if !defined(GLES2) && (!defined(__APPLE__) || defined(TARGET_IPHONE))
		// OpenGL 4.2 or GLES 3.0 min
		if (gl.gl_major > 4 || (gl.gl_major == 4 && gl.gl_minor >= 2)
				|| (gl.is_gles && gl.gl_major >= 3))
		{
			GLuint internalFormat;
			switch (tex_type)
			{
			case TextureType::_5551:
				internalFormat = GL_RGB5_A1;
				break;
			case TextureType::_565:
				internalFormat = GL_RGB565;
				break;
			case TextureType::_4444:
				internalFormat = GL_RGBA4;
				break;
			case TextureType::_8888:
				internalFormat = GL_RGBA8;
				break;
			case TextureType::_8:
				internalFormat = comps;
				break;
			default:
				die("Unsupported texture format");
				internalFormat = 0;
				break;
			}
			if (Updates == 1)
			{
				glTexStorage2D(GL_TEXTURE_2D, mipmapLevels, internalFormat, width, height);
				glCheck();
			}
			for (int i = 0; i < mipmapLevels; i++)
			{
				glTexSubImage2D(GL_TEXTURE_2D, mipmapLevels - i - 1, 0, 0, 1 << i, 1 << i, comps, gltype, temp_tex_buffer);
				temp_tex_buffer += (1 << (2 * i)) * bytes_per_pixel;
			}
		}
		else
#endif
		{
			for (int i = 0; i < mipmapLevels; i++)
			{
				glTexImage2D(GL_TEXTURE_2D, mipmapLevels - i - 1, comps, 1 << i, 1 << i, 0, comps, gltype, temp_tex_buffer);
				temp_tex_buffer += (1 << (2 * i)) * bytes_per_pixel;
			}
		}
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0,comps, width, height, 0, comps, gltype, temp_tex_buffer);
		if (mipmapped)
			glGenerateMipmap(GL_TEXTURE_2D);
	}
	glCheck();
}
	
bool TextureCacheData::Delete()
{
	if (!BaseTextureCacheData::Delete())
		return false;

	if (texID)
		glcache.DeleteTextures(1, &texID);

	return true;
}

GLuint BindRTT(bool withDepthBuffer)
{
	GLenum channels, format;
	switch(pvrrc.fb_W_CTRL.fb_packmode)
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
		WARN_LOG(RENDERER, "Unsupported render to texture format: %d", pvrrc.fb_W_CTRL.fb_packmode);
		return 0;

	case 7: //7     invalid
		WARN_LOG(RENDERER, "Invalid framebuffer format: 7");
		return 0;
	}
	u32 fbw = pvrrc.getFramebufferWidth();
	u32 fbh = pvrrc.getFramebufferHeight();
	u32 texAddress = pvrrc.fb_W_SOF1 & VRAM_MASK;
	DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d x %d @ %06x", pvrrc.fb_W_CTRL.fb_packmode, pvrrc.fb_W_LINESTRIDE * 8,
			fbw, fbh, texAddress);

	gl.rtt.framebuffer.reset();

	u32 fbw2;
	u32 fbh2;
	getRenderToTextureDimensions(fbw, fbh, fbw2, fbh2);

	// Create a texture for rendering to
	GLuint texture = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, channels, fbw2, fbh2, 0, channels, format, 0);

	gl.rtt.framebuffer = std::make_unique<GlFramebuffer>((int)fbw2, (int)fbh2, withDepthBuffer, texture);

	glViewport(0, 0, fbw, fbh);

	return gl.rtt.framebuffer->getFramebuffer();
}

void ReadRTTBuffer()
{
	u32 w = pvrrc.getFramebufferWidth();
	u32 h = pvrrc.getFramebufferHeight();

	const u8 fb_packmode = pvrrc.fb_W_CTRL.fb_packmode;
	const u32 tex_addr = pvrrc.fb_W_SOF1 & VRAM_MASK;

	if (config::RenderToTextureBuffer)
	{
#ifdef TARGET_VIDEOCORE
		// Remove all vram locks before calling glReadPixels
		// (deadlock on rpi)
		u32 size = w * h * 2;
		u32 page_tex_addr = tex_addr & PAGE_MASK;
		u32 page_size = size + tex_addr - page_tex_addr;
		page_size = ((page_size - 1) / PAGE_SIZE + 1) * PAGE_SIZE;
		for (u32 page = page_tex_addr; page < page_tex_addr + page_size; page += PAGE_SIZE)
			VramLockedWriteOffset(page);
#endif

		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		u16 *dst = (u16 *)&vram[tex_addr];

		u32 linestride = pvrrc.fb_W_LINESTRIDE * 8;
		if (linestride == 0)
			linestride = w * 2;

		GLint color_fmt, color_type;
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &color_fmt);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &color_type);

		if (fb_packmode == 1 && linestride == w * 2 && color_fmt == GL_RGB && color_type == GL_UNSIGNED_SHORT_5_6_5)
		{
			glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, dst);
		}
		else
		{
			PixelBuffer<u32> tmp_buf;
			tmp_buf.init(w, h);

			u8 *p = (u8 *)tmp_buf.data();
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);

			WriteTextureToVRam(w, h, p, dst, pvrrc.fb_W_CTRL, linestride);
		}
		glCheck();
	}
	else
	{
		//memset(&vram[tex_addr], 0, size);
		if (w <= 1024 && h <= 1024)
		{
			TextureCacheData *texture_data = TexCache.getRTTexture(tex_addr, fb_packmode, w, h);
			glcache.DeleteTextures(1, &texture_data->texID);
			texture_data->texID = gl.rtt.framebuffer->detachTexture();
			texture_data->dirty = 0;
			texture_data->unprotectVRam();
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);
}

BaseTextureCacheData *OpenGLRenderer::GetTexture(TSP tsp, TCW tcw)
{
	//lookup texture
	TextureCacheData* tf = TexCache.getTextureCacheData(tsp, tcw);

	//update if needed
	if (tf->NeedsUpdate())
	{
		if (!tf->Update())
			tf = nullptr;
	}
	else if (tf->IsCustomTextureAvailable())
	{
		TexCache.DeleteLater(tf->texID);
		tf->texID = glcache.GenTexture();
		tf->CheckCustomTexture();
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
			&& (width != gl.ofbo.framebuffer->getWidth() || height != gl.ofbo.framebuffer->getHeight()
				// if the rotate90 setting has changed
				|| (gl.gl_major >= 3 && (gl.ofbo.framebuffer->getTexture() == 0) == config::Rotate90)))
	{
		gl.ofbo.framebuffer.reset();
	}

	if (gl.ofbo.framebuffer == nullptr)
	{
		GLuint texture = 0;
		if (config::Rotate90)
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
#ifdef GL_RGBA8
			glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
#endif
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
#ifdef GL_RGBA8
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
#endif
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
