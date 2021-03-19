#include "glcache.h"
#include "gles.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/TexCache.h"

#include <cstdio>
#include <cstdlib>

GlTextureCache TexCache;

void TextureCacheData::UploadToGPU(int width, int height, u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
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
#if !defined(GLES2) && !defined(__APPLE__)
		// Open GL 4.2 or GLES 3.0 min
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

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt)
{
	if (gl.rtt.fbo) glDeleteFramebuffers(1,&gl.rtt.fbo);
	if (gl.rtt.tex) glcache.DeleteTextures(1,&gl.rtt.tex);
	if (gl.rtt.depthb) glDeleteRenderbuffers(1,&gl.rtt.depthb);

	gl.rtt.TexAddr=addy>>3;

	// Find the smallest power of two texture that fits the viewport
	u32 fbh2 = 2;
	while (fbh2 < fbh)
		fbh2 *= 2;
	u32 fbw2 = 2;
	while (fbw2 < fbw)
		fbw2 *= 2;

	if (config::RenderToTextureUpscale > 1 && !config::RenderToTextureBuffer)
	{
		fbw *= config::RenderToTextureUpscale;
		fbh *= config::RenderToTextureUpscale;
		fbw2 *= config::RenderToTextureUpscale;
		fbh2 *= config::RenderToTextureUpscale;
	}
	// Get the currently bound frame buffer object. On most platforms this just gives 0.
	//glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_i32OriginalFbo);

	// Generate and bind a render buffer which will become a depth buffer shared between our two FBOs
	glGenRenderbuffers(1, &gl.rtt.depthb);
	glBindRenderbuffer(GL_RENDERBUFFER, gl.rtt.depthb);

	/*
		Currently it is unknown to GL that we want our new render buffer to be a depth buffer.
		glRenderbufferStorage will fix this and in this case will allocate a depth buffer
		m_i32TexSize by m_i32TexSize.
	*/

	if (gl.is_gles)
	{
#if defined(GL_DEPTH24_STENCIL8_OES) && defined(GL_DEPTH_COMPONENT24_OES)
		if (gl.GL_OES_packed_depth_stencil_supported)
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, fbw2, fbh2);
		else if (gl.GL_OES_depth24_supported)
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, fbw2, fbh2);
		else
#endif
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, fbw2, fbh2);
	}
	else
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fbw2, fbh2);

	// Create a texture for rendering to
	gl.rtt.tex = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, gl.rtt.tex);

	glTexImage2D(GL_TEXTURE_2D, 0, channels, fbw2, fbh2, 0, channels, fmt, 0);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// Create the object that will allow us to render to the aforementioned texture
	glGenFramebuffers(1, &gl.rtt.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gl.rtt.fbo);

	// Attach the texture to the FBO
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl.rtt.tex, 0);

	// Attach the depth buffer we created earlier to our FBO.
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl.rtt.depthb);

	if (!gl.is_gles || gl.GL_OES_packed_depth_stencil_supported)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gl.rtt.depthb);

	// Check that our FBO creation was successful
	GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

	glViewport(0, 0, fbw, fbh);		// TODO CLIP_X/Y min?
}

void ReadRTTBuffer() {
	u32 w = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
	u32 h = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;

	u32 stride = FB_W_LINESTRIDE.stride * 8;
	if (stride == 0)
		stride = w * 2;
	else if (w * 2 > stride) {
		// Happens for Virtua Tennis
		w = stride / 2;
	}
	u32 size = w * h * 2;

	const u8 fb_packmode = FB_W_CTRL.fb_packmode;

	if (config::RenderToTextureBuffer)
	{
		u32 tex_addr = gl.rtt.TexAddr << 3;

		// Remove all vram locks before calling glReadPixels
		// (deadlock on rpi)
		u32 page_tex_addr = tex_addr & PAGE_MASK;
		u32 page_size = size + tex_addr - page_tex_addr;
		page_size = ((page_size - 1) / PAGE_SIZE + 1) * PAGE_SIZE;
		for (u32 page = page_tex_addr; page < page_tex_addr + page_size; page += PAGE_SIZE)
			VramLockedWriteOffset(page);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		u16 *dst = (u16 *)&vram[tex_addr];

		GLint color_fmt, color_type;
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &color_fmt);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &color_type);

		if (fb_packmode == 1 && stride == w * 2 && color_fmt == GL_RGB && color_type == GL_UNSIGNED_SHORT_5_6_5)
		{
			// Can be read directly into vram
			glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, dst);
		}
		else
		{
			PixelBuffer<u32> tmp_buf;
			tmp_buf.init(w, h);

			u8 *p = (u8 *)tmp_buf.data();
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);

			WriteTextureToVRam(w, h, p, dst);
		}
	}
	else
	{
		//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);
	}

    //dumpRtTexture(fb_rtt.TexAddr, w, h);

    if (w > 1024 || h > 1024 || config::RenderToTextureBuffer) {
    	glcache.DeleteTextures(1, &gl.rtt.tex);
    }
    else
    {
    	// TexAddr : fb_rtt.TexAddr, Reserved : 0, StrideSel : 0, ScanOrder : 1
    	TCW tcw = { { gl.rtt.TexAddr, 0, 0, 1 } };
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
    	for (tsp.TexU = 0; tsp.TexU <= 7 && (8u << tsp.TexU) < w; tsp.TexU++);
    	for (tsp.TexV = 0; tsp.TexV <= 7 && (8u << tsp.TexV) < h; tsp.TexV++);

    	TextureCacheData *texture_data = TexCache.getTextureCacheData(tsp, tcw);
    	if (texture_data->texID != 0)
    		glcache.DeleteTextures(1, &texture_data->texID);
    	else
    		texture_data->Create();
    	texture_data->texID = gl.rtt.tex;
    	texture_data->dirty = 0;
    	libCore_vramlock_Lock(texture_data->sa_tex, texture_data->sa + texture_data->size - 1, texture_data);
    }
    gl.rtt.tex = 0;

	if (gl.rtt.fbo) { glDeleteFramebuffers(1,&gl.rtt.fbo); gl.rtt.fbo = 0; }
	if (gl.rtt.depthb) { glDeleteRenderbuffers(1,&gl.rtt.depthb); gl.rtt.depthb = 0; }

}

static int TexCacheLookups;
static int TexCacheHits;
//static float LastTexCacheStats;

u64 gl_GetTexture(TSP tsp, TCW tcw)
{
	TexCacheLookups++;

	//lookup texture
	TextureCacheData* tf = TexCache.getTextureCacheData(tsp, tcw);

	if (tf->texID == 0)
	{
		tf->Create();
		tf->texID = glcache.GenTexture();
	}

	//update if needed
	if (tf->NeedsUpdate())
		tf->Update();
	else
	{
		if (tf->IsCustomTextureAvailable())
		{
			TexCache.DeleteLater(tf->texID);
			tf->texID = glcache.GenTexture();
			tf->CheckCustomTexture();
		}
		TexCacheHits++;
	}

//	if (os_GetSeconds() - LastTexCacheStats >= 2.0)
//	{
//		LastTexCacheStats = os_GetSeconds();
//		printf("Texture cache efficiency: %.2f%% cache size %ld\n", (float)TexCacheHits / TexCacheLookups * 100, TexCache.size());
//		TexCacheLookups = 0;
//		TexCacheHits = 0;
//	}

	//return gl texture
	return tf->texID;
}

GLuint fbTextureId;

void RenderFramebuffer()
{
	if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
		return;

	PixelBuffer<u32> pb;
	int width;
	int height;
	ReadFramebuffer(pb, width, height);
	
	if (fbTextureId == 0)
		fbTextureId = glcache.GenTexture();
	
	glcache.BindTexture(GL_TEXTURE_2D, fbTextureId);
	
	//set texture repeat mode
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pb.data());
}

GLuint init_output_framebuffer(int width, int height)
{
	if (width != gl.ofbo.width || height != gl.ofbo.height
		|| (gl.ofbo.tex == 0) == config::Rotate90)
	{
		free_output_framebuffer();
		gl.ofbo.width = width;
		gl.ofbo.height = height;
	}
	if (gl.ofbo.fbo == 0)
	{
		// Create the depth+stencil renderbuffer
		glGenRenderbuffers(1, &gl.ofbo.depthb);
		glBindRenderbuffer(GL_RENDERBUFFER, gl.ofbo.depthb);

		if (gl.is_gles)
		{
#if defined(GL_DEPTH24_STENCIL8_OES) && defined(GL_DEPTH_COMPONENT24_OES)
			if (gl.GL_OES_packed_depth_stencil_supported)
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
			else if (gl.GL_OES_depth24_supported)
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, width, height);
			else
#endif
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
		}
		else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

		if (gl.gl_major < 3 || config::Rotate90)
		{
			// Create a texture for rendering to
			gl.ofbo.tex = glcache.GenTexture();
			glcache.BindTexture(GL_TEXTURE_2D, gl.ofbo.tex);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		else
		{
			// Use a renderbuffer and glBlitFramebuffer
			glGenRenderbuffers(1, &gl.ofbo.colorb);
			glBindRenderbuffer(GL_RENDERBUFFER, gl.ofbo.colorb);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
		}

		// Create the framebuffer
		glGenFramebuffers(1, &gl.ofbo.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.fbo);

		// Attach the depth buffer to our FBO.
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl.ofbo.depthb);

		if (!gl.is_gles || gl.GL_OES_packed_depth_stencil_supported)
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gl.ofbo.depthb);

		// Attach the texture/renderbuffer to the FBO
		if (gl.ofbo.tex != 0)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl.ofbo.tex, 0);
		else
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl.ofbo.colorb);

		// Check that our FBO creation was successful
		GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

		glcache.Disable(GL_SCISSOR_TEST);
		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
		glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.fbo);

	glViewport(0, 0, width, height);
	glCheck();

	return gl.ofbo.fbo;
}

void free_output_framebuffer()
{
	if (gl.ofbo.fbo != 0)
	{
		glDeleteFramebuffers(1, &gl.ofbo.fbo);
		gl.ofbo.fbo = 0;
		glDeleteRenderbuffers(1, &gl.ofbo.depthb);
		gl.ofbo.depthb = 0;
		if (gl.ofbo.tex != 0)
		{
			glcache.DeleteTextures(1, &gl.ofbo.tex);
			gl.ofbo.tex = 0;
		}
		if (gl.ofbo.colorb != 0)
		{
			glDeleteRenderbuffers(1, &gl.ofbo.colorb);
			gl.ofbo.colorb = 0;
		}
	}
}
